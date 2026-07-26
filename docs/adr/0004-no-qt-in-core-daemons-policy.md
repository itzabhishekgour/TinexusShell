# ADR 0004: Strict Pure C++20 Policy for Core Daemons

## Status
ACCEPTED (Architecture Freeze v1.1)

## Context
UI frameworks like Qt/QML introduce heavy memory overhead and complex event loop dependencies when linked into background daemons.

## Decision
Qt6/QML is allowed ONLY in UI binaries (`tinexus-launcher`, `tinexus-lock`). All core platform daemons (`serviced`, `ipcd`, `indexerd`, `searchd`, `comp`, `settings`, `notifications`, `clipboard`) MUST be written in Pure C++20 using standard POSIX sockets and epoll/poll event loops.

## Consequences
- Ultra-low background RSS memory footprint (< 12MB RSS per core daemon).
- Fast cold boot startup (< 500ms total stack boot).
