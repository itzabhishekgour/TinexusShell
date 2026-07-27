# Tinexus Platform

> **The distraction-free, command-palette-first Linux Desktop Platform.**  
> Built on Wayland. Built in C++20. Built for focus.

```
"Everything starts with simplicity."
```

---

## 🚦 Component Implementation Status Matrix

Tinexus Platform tracks component maturity transparently across three tiers:

| Subsystem Component | Implementation Tier | Verified Technical Highlights |
|---|---|---|
| **Terminal Emulator (`tinexus-terminal`)** | 🟢 **REAL SYSTEM APIs** | POSIX `posix_openpt()`, `grantpt()`, `unlockpt()`, `ptsname()`, `fork()`, `execvp()`, `dup2()`, `ioctl(TIOCSWINSZ)` PTY master/slave engine. |
| **System Monitor (`tinexus-monitor`)** | 🟢 **REAL SYSTEM APIs** | Direct Linux `/proc/stat` reader, per-core delta usage parser, `/proc/meminfo`, `/proc/diskstats`, `/proc/net/dev`, `/proc/[pid]/stat`. |
| **Platform Supervisor (`tinexus-serviced`)** | 🟢 **REAL SYSTEM APIs** | POSIX `fork()`, `execvp()`, `kill()`, `waitpid()`, supervision watchdog, Unix sockets (`/tmp/tinexus-serviced.sock`). |
| **IPC Broker (`tinexus-ipcd`) & SDK** | 🟢 **REAL SYSTEM APIs** | Real Unix domain socket broker, IPC packet framing, payload serialization, `libtinexus-sdk.so` client library. |
| **Settings Daemon (`tinexus-settings`)** | 🟢 **REAL SYSTEM APIs** | TOML config parser, schema validator, atomic file sync (`.tmp` ➔ `fsync` ➔ `rename`). |
| **Wayland Compositor (`tinexus-comp`)** | 🟡 **PARTIAL / FRAMEWORK** | Window rules engine, workspace manager, surface manager, frame scheduler; event loop currently runs sleep loop. |
| **Display Manager (`tinexus-displayd`)** | 🟡 **PARTIAL / FRAMEWORK** | VT allocation/switching architecture, seat0 acquisition, login supervisor, systemd `READY=1`/`STOPPING=1` socket signals. |
| **Package Manager (`tinexus-pkg`)** | 🟡 **PARTIAL / FRAMEWORK** | `.tinexus` manifest parser, SHA256 checksums, Ed25519 signatures, topological DAG solver, package DB, staging state machine. |
| **PAM Login (`tinexus-login`)** | 🟡 **PARTIAL / FRAMEWORK** | Memory zeroing (`explicit_bzero`), POSIX privilege drop sequence (`initgroups()` ➔ `setgid()` ➔ `setuid()`), baseline auth rules. |
| **Graphical Installer (`tinexus-installer`)** | 🔴 **DRY-RUN / MOCKED** | 10-stage wizard state machine, `/dev/disk/by-id/` discovery, live media safety protection; disk formatting (`mkfs.ext4`) mocked for host safety. |
| **ISO Builder (`tinexus-iso`)** | 🔴 **DRY-RUN / MOCKED** | RootFS stager tree, initramfs generator, squashfs builder, GRUB EFI config; binary calls (`xorriso`, `mksquashfs`) mocked in dry-run mode. |
| **Live USB Engine (`tinexus-liveusb`)** | 🔴 **DRY-RUN / MOCKED** | Removable USB detector, pre-flight ISO verifier, read-back SHA256 verifier; raw block writes (`/dev/sdX`) mocked in dry-run mode. |
| **Release Pipeline (`tinexus-release`)** | 🔴 **DRY-RUN / MOCKED** | SHA256 generator, GPG signature engine, release notes generator, `release.json` manifest, GitHub artifact packager. |

---

## 🎯 Stabilization Roadmap

```
v0.1.0-alpha ✅ Architecture Freeze & Component Framework (23/23 Test Suites Passed)
v0.2.0       ⏳ Real Wayland Session & C-API Display Event Loop (`tinexus-comp`)  ◄ CURRENT FOCUS
v0.3.0       ⏳ Real Display Manager Boot Sequence (`tinexus-displayd` ➔ `tinexus-login` ➔ `tinexus-session`)
v0.4.0       ⏳ Real Package Manager Subprocess Engine (`tinexus-pkg`)
v0.5.0       ⏳ Real Hardware Installer, ISO Builder & Live USB Writes
v1.0.0       ⏳ General Availability (GA) Production Release
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

## License

Tinexus Platform is dual-licensed:
- Core platform components & daemons: **GPL-2.0-or-later**
- SDKs, protocols, and shared libraries: **Apache-2.0**

See [LICENSE](LICENSE) for details.
