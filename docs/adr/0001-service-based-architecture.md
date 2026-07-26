# ADR 0001: Microservice-Based Desktop Platform Architecture

## Status
ACCEPTED (Architecture Freeze v1.1)

## Context
Traditional Linux desktop environments (GNOME, KDE) often suffer from monolithic process bloat or tight coupling between rendering, search, session management, and configuration.

## Decision
Tinexus Platform adopts a strict microservice architecture where core platform responsibilities are split into isolated binary daemons:
- `tinexus-serviced` (Platform Supervisor Authority)
- `tinexus-ipcd` (Central IPC Broker)
- `tinexus-indexerd` (App & File Indexing Daemon)
- `tinexus-searchd` (Search Engine & Ranking Pipeline)
- `tinexus-comp` (Wayland Vulkan Compositor)
- `tinexus-launcher` (Command Palette UI Surface)

## Consequences
- **Positive**: Isolated process crash boundaries (if `searchd` crashes, `serviced` restarts it in < 50ms without crashing the compositor or launcher).
- **Positive**: Strict memory caps (< 50MB RSS total core footprint).
- **Negative**: Requires versioned binary IPC messaging overhead across processes.
