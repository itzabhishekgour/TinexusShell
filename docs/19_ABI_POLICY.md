# 19. ABI & IPC Compatibility Policy (v1.0 Frozen)

## Status
FROZEN (Architecture Freeze v1.1 / ABI v1.0)

## Overview
This document specifies the ABI (Application Binary Interface) and IPC binary compatibility rules for Tinexus Platform v1.x.

## Core Rules for v1.x
1. **Never Break Binary Compatibility**: Public headers (`sdk/include/tinexus/`) and IPC binary structs (`MessageHeader`, `ActionRequest`, `SearchQuery`, `SearchResultItem`) MUST remain binary compatible throughout all v1.x minor releases (`v1.0.0` through `v1.99.99`).
2. **Append-Only Extension Policy**: Struct fields and enum values may ONLY be appended at the end. Reordering or deleting existing fields/enums is strictly forbidden.
3. **Pimpl Pointer Enforcement**: All public SDK facade classes (`tinexus::Client`) MUST hide internal implementation details behind opaque implementation pointers (`std::unique_ptr<Impl>`).
4. **IPC Binary Header Lock**:
   - `magic` = `0x544E5853` (`TNXS`)
   - `version` = `0x0100` (v1.0)
   - Header length = 20 bytes packed (`#pragma pack(push, 1)`).

## Version Negotiation Handshake
Clients connecting over Unix Domain Sockets (`/run/user/$UID/tinexus/ipc.sock`) perform version handshake upon connection. Mismatched major version requests (`version >> 8 != 1`) MUST be rejected with `Error { code: 409, message: "Protocol Version Mismatch" }`.
