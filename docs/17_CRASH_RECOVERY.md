# Tinexus Platform — Crash Recovery & Diagnostics Specification

> **Document:** 17_CRASH_RECOVERY.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 06_COMPONENT_DESIGN.md

---

## Table of Contents

1. [Supervision & Recovery Philosophy](#1-supervision--recovery-philosophy)
2. [`tinexus-serviced` Supervision Tree](#2-tinexus-serviced-supervision-tree)
3. [Daemon Crash Restart Policy](#3-daemon-crash-restart-policy)
4. [Compositor Crash & Client Reconnection](#4-compositor-crash--client-reconnection)
5. [Coredump Collection (systemd-coredump)](#5-coredump-collection-systemd-coredump)
6. [`tinexus-diag` CLI Diagnostic Tool](#6-tinexus-diag-cli-diagnostic-tool)
7. [Log Aggregation Specification](#7-log-aggregation-specification)

---

## 1. Supervision & Recovery Philosophy

Zero single-point-of-failure runtime crashes. If a background daemon crashes, the platform supervisor (`tinexus-serviced`) restores it automatically within milliseconds without terminating the user session or losing application state.

---

## 2. `tinexus-serviced` Supervision Tree

```
                       systemd (User Instance)
                                  │
                       ┌──────────▼──────────┐
                       │  tinexus-serviced   │ (Platform Supervisor)
                       └──────────┬──────────┘
                                  │
   ┌──────────────┬───────────────┼───────────────┬──────────────┐
   │              │               │               │              │
   ▼              ▼               ▼               ▼              ▼
tinexus-ipcd  tinexus-comp  tinexus-searchd  tinexus-notif  tinexus-clip ...
```

---

## 3. Daemon Crash Restart Policy

When a child daemon exits non-zero or terminates via signal (`SIGSEGV`, `SIGABRT`):

1. `tinexus-serviced` catches `SIGCHLD`.
2. Inspects restart counter for daemon:
   - Attempt 1: Immediate restart (0ms delay).
   - Attempt 2: Restart after 2 seconds.
   - Attempt 3: Restart after 8 seconds.
   - Attempt 4: Mark daemon `FAILED`, issue critical system notification.
3. Reset counter if daemon runs stable for 60 seconds.

---

## 4. Compositor Crash & Client Reconnection

If `tinexus-comp` crashes:
- `tinexus-serviced` restarts `tinexus-comp` immediately.
- Wayland applications supporting `wp_single_pixel_buffer_v1` and socket reconnect re-attach to the new Wayland socket automatically.
- `tinexus-launcher`, `tinexus-wallpaper`, and `tinexus-notif` re-map layer-shell surfaces within < 500ms.

---

## 5. Coredump Collection (systemd-coredump)

- Crash dumps are caught via `systemd-coredump`.
- Stack traces automatically symbolicated using local debug symbols.
- Saved to `/var/lib/systemd/coredump/` or user runtime dir.

---

## 6. `tinexus-diag` CLI Diagnostic Tool

Command-line tool included in `tools/tinexus-diag`:

```bash
# Check platform health status
tinexus-diag status

# Collect bug report package (logs, config, backtraces)
tinexus-diag --collect-report --output bug_report.tar.gz

# Monitor D-Bus and socket latency in real-time
tinexus-diag monitor-ipc
```

---

## 7. Log Aggregation Specification

All components emit structured JSON logs to systemd journal:

```json
{
  "PRIORITY": 3,
  "SYSLOG_IDENTIFIER": "tinexus-searchd",
  "MESSAGE": "Search provider query timeout",
  "COMPONENT": "AppProvider",
  "QUERY": "fire",
  "ELAPSED_MS": 201.4
}
```

---

*Document End: 17_CRASH_RECOVERY.md*
