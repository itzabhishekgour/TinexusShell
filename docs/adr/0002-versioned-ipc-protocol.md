# ADR 0002: Versioned Binary IPC Protocol Framing

## Status
ACCEPTED (Architecture Freeze v1.1)

## Context
Cross-process communication requires a fast, zero-copy, future-proof wire transport format.

## Decision
All IPC messages across Unix domain sockets `/run/user/$UID/tinexus/ipc.sock` MUST begin with a 20-byte tightly packed binary `MessageHeader`:
- `magic`: `0x544E5853` (`TNXS`)
- `version`: `0x0100` (Version 1.0)
- `msg_type`: Typed 16-bit message ID (`SHORTCUT_ACTIVATED`, `ACTION_REQUEST`, `SEARCH_QUERY`, `SEARCH_RESULT`)
- `sequence_id`: Monotonic request counter for asynchronous response matching
- `payload_len`: Binary payload byte size

## Consequences
- Enables sub-millisecond roundtrip IPC latency (< 350 µs).
- Guarantees backward and forward binary protocol compatibility.
