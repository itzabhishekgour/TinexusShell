# Tinexus Platform

> **The distraction-free, command-palette-first Linux Desktop Platform.**  
> Built on Wayland. Built in C++20. Built for focus.

```
"Everything starts with simplicity."
```

---

## Documentation Index (Architecture Freeze v1.1)

| # | Document | Description | Status |
|---|---|---|---|
| 00 | [Architecture Review](docs/00_ARCHITECTURE_REVIEW.md) | Principal Architect's risk analysis, ADRs, & tech stack decisions | ✅ FROZEN |
| 01 | [Vision](docs/01_VISION.md) | Philosophy, goals, mission, competitive analysis, platform scope | ✅ FROZEN |
| 02 | [Requirements](docs/02_REQUIREMENTS.md) | Functional, non-functional, security, performance, & use cases | ✅ FROZEN |
| 03 | [System Architecture](docs/03_SYSTEM_ARCHITECTURE.md) | Platform layered architecture, `serviced`/`ipcd`, Vulkan, Workspace Manager | ✅ FROZEN |
| 04 | [Folder Structure](docs/04_FOLDER_STRUCTURE.md) | Monorepo layout specification, ownership matrix, naming conventions | ✅ FROZEN |
| 05 | [UI/UX Guidelines](docs/05_UI_UX_GUIDELINES.md) | Expanded token system (Motion, Blur, Spacing, Typography, Radius) | ✅ FROZEN |
| 06 | [Component Design](docs/06_COMPONENT_DESIGN.md) | Component specs (`searchd`, `serviced`, `ipcd`, UI boundary) | ✅ FROZEN |
| 07 | [Security](docs/07_SECURITY.md) | Threat model, sandbox, privilege separation, Capability Tokens | ✅ FROZEN |
| 08 | [Performance](docs/08_PERFORMANCE.md) | GPU scheduler, texture atlas, frame pacing, partial rendering | ✅ FROZEN |
| 09 | [Build System](docs/09_BUILD_SYSTEM.md) | CMake monorepo layout, CI/CD, Qt RHI, packaging specifications | ✅ FROZEN |
| 10 | [Roadmap](docs/10_ROADMAP.md) | Release milestones (v0.1 → v3.0) & natural implementation sequence | ✅ FROZEN |
| 11 | [IPC Strategy](docs/11_IPC_STRATEGY.md) | D-Bus vs Sockets vs Shared Memory decision matrix & `ipcd` broker | ✅ FROZEN |
| 12 | [Testing Strategy](docs/12_TESTING_STRATEGY.md) | 8-level testing pyramid (Unit, Integration, UI, Perf, Stress, Fuzz, Leak, Protocol) | ✅ FROZEN |
| 13 | [Memory Strategy](docs/13_MEMORY_STRATEGY.md) | Allocator patterns (Small Object, Arena, Frame, GPU) & ownership rules | ✅ FROZEN |
| 14 | [Wayland Protocols](docs/14_WAYLAND_PROTOCOLS.md) | Custom protocol XML specifications (`tinexus-global-shortcut-v1`, etc.) | ✅ FROZEN |
| 15 | [Plugin SDK](docs/15_PLUGIN_SDK.md) | Plugin SDK specification, sandbox isolation, Capability Tokens | ✅ FROZEN |
| 16 | [Configuration Spec](docs/16_CONFIGURATION_SPEC.md) | Complete TOML schemas for all platform configuration files | ✅ FROZEN |
| 17 | [Crash Recovery](docs/17_CRASH_RECOVERY.md) | Supervisor supervision tree, coredump collection, `tinexus-diag` CLI | ✅ FROZEN |
| 18 | [Governance](docs/18_GOVERNANCE.md) | Monorepo branching model, PR rules, release checklists, CLA policy | ✅ FROZEN |
| 19 | [ABI Policy](docs/19_ABI_POLICY.md) | C++ ABI stability rules, Pimpl idiom, symbol visibility, C wrappers | ✅ FROZEN |
| 20 | [Thread Model](docs/20_THREAD_MODEL.md) | Per-process thread inventory, priorities, scheduling, lock-free queues | ✅ FROZEN |
| 21 | [Search Ranking](docs/21_SEARCH_RANKING.md) | Ranking engine scoring heuristics, recency/frequency decay, trigram+Levenshtein | ✅ FROZEN |

---

## What is Tinexus Platform?

Tinexus is a Wayland-native Linux Desktop Platform designed around one radical idea:

**The desktop should get out of the way.**

No taskbar. No dock. No desktop icons. No widgets.  
When you open your computer, you see your wallpaper. Nothing else.

Everything is accessible through **Ctrl+K** — a single, beautiful launcher powered by `tinexus-searchd` and inspired by Raycast, VSCode's command palette, and macOS Spotlight.

```
Ctrl+K → type anything → press Enter
```

That's Tinexus.

---

## Core Platform Decisions

| Decision | Choice | Reason |
|---|---|---|
| Platform Architecture | Microservices via `serviced` & `ipcd` | Crash isolation, modular replaceability |
| D-Bus Namespace | `io.tinexus.shell.*` | Reverse-domain, future-proof hierarchy |
| Display Protocol | Wayland (Vulkan-native) | Modern rendering pipeline |
| Compositor Foundation | wlroots (C++20 wrapper) | Solid DRM/KMS and libinput stack |
| UI Framework | Qt6 / QML (Qt RHI) | GPU-accelerated UI layer only |
| Non-UI Core | Pure C++20 | Zero Qt dependency in compositor, session, searchd, ipcd, serviced |
| IPC Broker | `tinexus-ipcd` | Protocol decoupling & abstraction |
| Supervisor | `tinexus-serviced` | Supervision tree & health monitoring |
| Search Engine | `tinexus-searchd` | Independent scoring, ranking, & provider bus |
| Config Format | TOML | Typed, human-readable, atomic |
| Repo Layout | Monorepo | Clean cross-component development & release cycle |

---

## Architecture Overview

```
                          ┌─────────────────────┐
                          │  systemd-logind     │
                          └──────────┬──────────┘
                                     │
                          ┌──────────▼──────────┐
                          │  tinexus-serviced   │ (Platform Supervisor)
                          └──────────┬──────────┘
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
  ┌────────▼────────┐       ┌────────▼────────┐       ┌────────▼────────┐
  │  tinexus-comp   │       │   tinexus-ipcd  │       │ tinexus-searchd │
  │  (Compositor)   │       │  (IPC Router)   │       │ (Search Engine) │
  └────────┬────────┘       └────────┬────────┘       └────────┬────────┘
           │                         │                         │
 ┌─────────┴─────────┐       ┌───────┴─────────┐       ┌───────┴─────────┐
 │ tinexus-wallpaper │       │ tinexus-notif   │       │ tinexus-clip    │
 │ tinexus-lock      │       │ tinexus-settings│       │ tinexus-indexer │
 └─────────┬─────────┘       └─────────────────┘       └─────────────────┘
           │
 ┌─────────▼─────────┐
 │ tinexus-launcher  │ (Qt6/QML UI over IPC)
 └───────────────────┘
```

---

## Monorepo Layout

```
Tinexus/
├── compositor/     # Wayland compositor (wlroots + Vulkan)
├── launcher/       # Command palette UI (Qt6/QML)
├── searchd/        # Search engine & ranking daemon
├── serviced/       # Platform service supervisor
├── ipcd/           # Central IPC router & broker
├── clipboard/      # Clipboard history daemon
├── wallpaper/      # Layer-shell wallpaper renderer
├── settings/       # Config daemon & schema engine
├── notifications/  # Notification daemon
├── lockscreen/     # PAM lock screen surface
├── sdk/            # Plugin SDK & capability headers
├── protocols/      # Custom Wayland XML protocols
├── shared/         # Common C++ libraries & memory pools
├── docs/           # Engineering documentation (00–21)
├── tests/          # Test suites (unit, integration, perf, fuzz)
└── tools/          # Diagnostics CLI (tinexus-diag) & build scripts
```

---

## License

Tinexus Platform is dual-licensed:
- Core platform components & daemons: **GPL-2.0-or-later**
- SDKs, protocols, and shared libraries: **Apache-2.0**

See [LICENSE](LICENSE) for details.

---

## Status

> **📋 ARCHITECTURE FREEZE v1.1 COMPLETE**  
> All 22 documentation files (README + docs 00–21) are frozen.  
> Phase 2: Implementation begins with the monorepo skeleton & `shared/` library.

---

*"We are building it."*
