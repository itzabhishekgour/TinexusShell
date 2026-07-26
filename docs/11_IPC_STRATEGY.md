# Tinexus Platform — IPC Strategy & Broker Specification

> **Document:** 11_IPC_STRATEGY.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 06_COMPONENT_DESIGN.md

---

## Table of Contents

1. [IPC Philosophy](#1-ipc-philosophy)
2. [IPC Mechanism Selection Matrix](#2-ipc-mechanism-selection-matrix)
3. [`tinexus-ipcd` Broker Daemon](#3-tinexus-ipcd-broker-daemon)
4. [D-Bus Bus Specification](#4-d-bus-bus-specification)
5. [Unix Domain Socket Protocol](#5-unix-domain-socket-protocol)
6. [Shared Memory Protocol (POSIX `shm_open`)](#6-shared-memory-protocol-posix-shm_open)
7. [In-Process Lock-Free Event Queues](#7-in-process-lock-free-event-queues)
8. [Security & Peer Credential Verification](#8-security--peer-credential-verification)

---

## 1. IPC Philosophy

Inter-Process Communication in Tinexus Platform follows two fundamental rules:

1. **Protocol Decoupling via Broker:** Individual platform components do not bake low-level socket or D-Bus transport logic into their core domain logic. They communicate via `tinexus-ipcd` (IPC Broker) or standardized client adapters.
2. **Right Transport for the Right Job:** High-frequency binary data uses Unix Domain Sockets; bulk pixel/texture transfers use Shared Memory; service discovery and system events use D-Bus.

---

## 2. IPC Mechanism Selection Matrix

| Scenario | Transport | Latency Target | Throughput | Message Format |
|---|---|---|---|---|
| **Service Discovery & Public APIs** | D-Bus (Session) | < 5ms | Low | D-Bus Wire Protocol (GVariant) |
| **Config Change Events** | D-Bus Signals | < 2ms | Low | D-Bus Signals |
| **Power & Session Lifecycle** | D-Bus (System/Session) | < 5ms | Low | D-Bus Methods |
| **Search Query & Result Streaming** | Unix Domain Socket | < 0.5ms | High (>100 msg/s) | Binary TLV (Type-Length-Value) |
| **Compositor → Launcher Frame Sync** | Unix Domain Socket | < 0.1ms | 60–120 msg/s | Fixed 32-byte struct |
| **Icon & Thumbnail Transfers** | Shared Memory (`shm_open`) | < 0.05ms | Bulk (MBs) | Raw Pixel Buffers |
| **Plugin Communication** | Unix Socket (Sandboxed) | < 1.0ms | Moderate | Length-prefixed JSON |

---

## 3. `tinexus-ipcd` Broker Daemon

### 3.1 Responsibility

`tinexus-ipcd` is the central IPC router for Tinexus Platform. It:
- Manages high-frequency Unix socket connections between local daemons.
- Acts as a message broker and subscription manager.
- Isolates components so that changing an internal IPC transport mechanism requires modifying ONLY `tinexus-ipcd`.

### 3.2 Process Architecture

```
                       tinexus-ipcd (Broker Daemon)
                       ├── Epoll Event Loop
                       ├── Peer Credential Validator (SO_PEERCRED)
                       ├── Shared Memory Allocator Tracker
                       └── Message Router Matrix
                                  │
         ┌────────────────────────┼────────────────────────┐
         │ (Unix Socket)          │ (Unix Socket)          │ (Shared Memory Handle)
         ▼                        ▼                        ▼
  tinexus-searchd         tinexus-launcher         tinexus-wallpaper
```

---

## 4. D-Bus Bus Specification

### 4.1 Namespace Standard

All D-Bus services MUST strictly use the reverse-domain hierarchy without spaces:

```
io.tinexus.shell.<ComponentName>
```

### 4.2 Standard Interfaces

- `io.tinexus.shell.Supervisor` — Managed by `tinexus-serviced`
- `io.tinexus.shell.IPC` — Managed by `tinexus-ipcd`
- `io.tinexus.shell.Compositor` — Managed by `tinexus-comp`
- `io.tinexus.shell.Search` — Managed by `tinexus-searchd`
- `io.tinexus.shell.Notifications` — Managed by `tinexus-notif`
- `io.tinexus.shell.Clipboard` — Managed by `tinexus-clip`
- `io.tinexus.shell.Settings` — Managed by `tinexus-settings`
- `io.tinexus.shell.Wallpaper` — Managed by `tinexus-wallpaper`

---

## 5. Unix Domain Socket Protocol

### 5.1 Socket Location

Sockets live strictly in runtime directory:
`/run/user/{uid}/tinexus/`

- `/run/user/{uid}/tinexus/ipc.sock` (Broker Endpoint)
- `/run/user/{uid}/tinexus/search.sock` (Search Stream Endpoint)

### 5.2 Header Format (16 Bytes, Little-Endian)

```cpp
struct Header {
    uint32_t magic;         // 0x544E5853 ("TNXS")
    uint16_t version;       // 0x0100 (v1.0)
    uint16_t msg_type;      // e.g., 0x0001 = SearchQuery, 0x0002 = SearchResult
    uint32_t sequence_id;   // Monotonic request ID
    uint32_t payload_len;   // Payload length in bytes
};
```

---

## 6. Shared Memory Protocol (POSIX `shm_open`)

Used for transferring large render buffers, icons, and thumbnails between daemons without copying through IPC pipes.

1. Producer creates SHM object: `/tinexus-shm-{uuid}` via `shm_open()`.
2. Producer calls `ftruncate()` and `mmap()`.
3. Producer sends file descriptor across Unix Socket via `sendmsg()` with `SCM_RIGHTS`.
4. Consumer maps FD via `mmap()`, reads buffer zero-copy, and closes FD.
5. Producer unlinks SHM name via `shm_unlink()`.

---

## 7. In-Process Lock-Free Event Queues

Within single multi-threaded binaries (such as `tinexus-searchd` and `tinexus-comp`), inter-thread communication uses a single-producer single-consumer (SPSC) ring buffer queue (`boost::lockfree::spsc_queue` or custom fixed-size atomic ring buffer).

Zero mutex acquisition on hot execution paths.

---

## 8. Security & Peer Credential Verification

Every socket connection accepted by `tinexus-ipcd` or any daemon MUST inspect socket credentials:

```cpp
struct ucred creds;
socklen_t len = sizeof(creds);
if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &creds, &len) == 0) {
    if (creds.uid != getuid()) {
        // REJECT: Cross-user connection attempt
        close(client_fd);
    }
}
```

---

*Document End: 11_IPC_STRATEGY.md*
