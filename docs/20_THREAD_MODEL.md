# Tinexus Platform — Per-Process Thread Model & Scheduling Specification

> **Document:** 20_THREAD_MODEL.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 08_PERFORMANCE.md, 13_MEMORY_STRATEGY.md

---

## Table of Contents

1. [Thread Architecture Philosophy](#1-thread-architecture-philosophy)
2. [Per-Daemon Thread Inventories](#2-per-daemon-thread-inventories)
3. [Scheduling Policies & CPU Priorities (`SCHED_FIFO` vs `SCHED_OTHER`)](#3-scheduling-policies--cpu-priorities-sched_fifo-vs-sched_other)
4. [CPU Core Affinity & Mask Rules](#4-cpu-core-affinity--mask-rules)
5. [Wake Strategies & Inter-Thread Signaling](#5-wake-strategies--inter-thread-signaling)
6. [Locking Strategy & Mutex Avoidance](#6-locking-strategy--mutex-avoidance)

---

## 1. Thread Architecture Philosophy

Thread contention and uncoordinated context switching ruin real-time desktop responsiveness. Tinexus Platform uses a **deterministic multi-threading model**:
- Every thread has a single, well-defined responsibility.
- Hot paths (rendering, IPC routing, search scoring) use lock-free queues and event loops.
- Real-time threads use `SCHED_FIFO` to prevent vblank drops.

---

## 2. Per-Daemon Thread Inventories

### 2.1 `tinexus-comp` (Wayland Compositor)
Total Threads: 4

| Thread Name | Role | Scheduling | Priority | Wake Strategy |
|---|---|---|---|---|
| `comp:render` | Vblank render loop, GPU command submission | `SCHED_FIFO` | RT Priority 1 | Vblank DRM Interrupt / epoll |
| `comp:input` | libinput event reading & gesture parsing | `SCHED_FIFO` | RT Priority 2 | Evdev epoll event |
| `comp:ipc` | Socket listener for `ipcd` & Wayland clients | `SCHED_OTHER` | Nice -5 | epoll |
| `comp:worker` | Async texture decoding & background tasks | `SCHED_OTHER` | Nice 0 | Condition Variable / Task Queue |

### 2.2 `tinexus-searchd` (Search Engine)
Total Threads: 3

| Thread Name | Role | Scheduling | Priority | Wake Strategy |
|---|---|---|---|---|
| `search:main` | Sockets listener & query parsing | `SCHED_OTHER` | Nice -2 | Socket epoll |
| `search:worker` | Parallel provider search & trigram evaluation | `SCHED_OTHER` | Nice 0 | Lock-free SPSC Queue |
| `search:rank` | Result ranking engine & score decay | `SCHED_OTHER` | Nice -1 | Search worker completion signal |

### 2.3 `tinexus-launcher` (Qt6/QML UI)
Total Threads: 3

| Thread Name | Role | Scheduling | Priority | Wake Strategy |
|---|---|---|---|---|
| `launch:ui` | Qt Main Event Loop & QML rendering | `SCHED_OTHER` | Nice -4 | Qt Event Loop |
| `launch:ipc` | Sockets client & stream receiver | `SCHED_OTHER` | Nice -2 | epoll |
| `launch:async` | Thumbnail loader thread pool | `SCHED_OTHER` | Nice 5 | QThreadPool Task Queue |

---

## 3. Scheduling Policies & CPU Priorities

Real-Time Scheduling requires `CAP_SYS_NICE` capability granted seamlessly by `systemd` or `logind`.

```ini
# systemd service config for tinexus-comp
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=1
Nice=-5
```

---

## 4. CPU Core Affinity & Mask Rules

On asymmetric architectures (e.g., Intel Alder Lake Performance/Efficient cores, ARM big.LITTLE):
- `comp:render` and `comp:input` are pinned to **Performance Cores** via `pthread_setaffinity_np()`.
- Background indexers (`tinexus-indexer`) are pinned to **Efficient Cores**.

---

## 5. Wake Strategies & Inter-Thread Signaling

1. **Zero Busy-Waiting:** No thread is permitted to spin-wait (`while(true) {}`).
2. **epoll / eventfd:** Thread wakeups triggered via `eventfd` writes or `epoll_wait()`.
3. **Condition Variables:** Used only for long-sleep background worker pools.

---

## 6. Locking Strategy & Mutex Avoidance

```
                          ┌───────────────────────────┐
                          │   Lock-Free SPSC Queue    │
                          │ (Ring Buffer + AtomicPtr) │
                          └─────────────┬─────────────┘
                                        │
           ┌────────────────────────────┴────────────────────────────┐
           ▼                                                         ▼
     Producer Thread                                           Consumer Thread
(No Mutex Lock, Atomic Inc)                              (No Mutex Lock, Atomic Dec)
```

- Mutexes prohibited on `comp:render` and `search:worker` execution loops.
- Atomic flag sync (`std::atomic<bool>`) used for lifecycle status updates.

---

*Document End: 20_THREAD_MODEL.md*
