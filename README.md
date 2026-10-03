# Tinexus Platform

> **The distraction-free, command-palette-first Linux Desktop Platform.**  
> Built on Wayland. Powered by TxUI (Pure C++20). Built for focus.

```
"Everything starts with simplicity."
```

---

## 🌟 Overview

**Tinexus Platform** is a modern, lightweight, Wayland-native desktop platform engineered from first principles in C++20. Departing from legacy monolithic desktop environments, Tinexus delivers a modular microservices architecture, a native Wayland compositor, and a custom UI framework (**TxUI**) designed for deterministic frame rendering and minimal resource footprint.

---

## 🚦 Component Implementation Status Matrix

Tinexus Platform tracks component maturity transparently across three tiers:

| Subsystem Component | Implementation Tier | Verified Technical Highlights |
|---|---|---|
| **TxUI Framework (`libtxui`)** | 🟢 **PRODUCTION READY** | Pure C++20 retained-mode UI toolkit with immediate-mode command recording. 100% exact `FontMetrics` text layout, flex layouts, and macOS-grade widgets. |
| **Desktop Shell (`tinexus-shell`)** | 🟢 **REAL SYSTEM APIs** | Top bar, clock/calendar widget, notification panel, brand logo menu, and `Ctrl+K` visual command launcher. |
| **File Manager (`tinexus-files`)** | 🟢 **REAL SYSTEM APIs** | Miller column browser, icon grid, multi-column list, gallery strip, inspector panel, spacebar Quick Look modal, and context menus with tagging. |
| **Command Palette (`tinexus-launcher`)** | 🟢 **REAL SYSTEM APIs** | Fast `Ctrl+K` launcher palette, fuzzy frecency-ranked search, app categories, and keyboard navigation. |
| **Application Dock (`tinexus-dock`)** | 🟢 **REAL SYSTEM APIs** | Parabolic wave magnification dock, running app indicators, active package launch zoom, and popover previews. |
| **System Settings (`tinexus-settings-ui`)**| 🟢 **REAL SYSTEM APIs** | Categorized system control center, live display resolution manager, and Wi-Fi management modal. |
| **Notifications (`tinexus-notifications`)**| 🟢 **REAL SYSTEM APIs** | High-performance notification stack with auto-dismiss timers, action buttons, and D-Bus integration. |
| **About Tinexus (`tinexus-about`)** | 🟢 **REAL SYSTEM APIs** | Hardware profiler, CPU/RAM/GPU architecture inspector, and active display visualizer. |
| **Lock Screen (`tinexus-lock`)** | 🟢 **REAL SYSTEM APIs** | PAM-authenticated security boundary with animated shake, avatar, and time display. |
| **Terminal Emulator (`foot` / native)** | 🟢 **HYBRID INTEGRATION** | Bundled high-performance Wayland terminal (`foot`) + native C++20 PTY engine (`tinexus-terminal`). |
| **System Monitor (`tinexus-monitor`)** | 🟢 **REAL SYSTEM APIs** | Direct Linux `/proc/stat` reader, per-core delta usage parser, `/proc/meminfo`, `/proc/diskstats`, `/proc/net/dev`. |
| **Platform Supervisor (`tinexus-serviced`)**| 🟢 **REAL SYSTEM APIs** | POSIX `fork()`, `execvp()`, `kill()`, `waitpid()`, supervision watchdog, Unix sockets (`/tmp/tinexus-serviced.sock`). |
| **IPC Broker (`tinexus-ipcd`) & SDK** | 🟢 **REAL SYSTEM APIs** | Unix domain socket broker, binary packet framing, payload serialization, `libtinexus-sdk.so` client library. |
| **Settings Daemon (`tinexus-settings`)** | 🟢 **REAL SYSTEM APIs** | TOML config parser, schema validator, atomic file sync (`.tmp` ➔ `fsync` ➔ `rename`). |
| **Wayland Compositor (`tinexus-comp`)** | 🟡 **PARTIAL / FRAMEWORK** | Window rules engine, workspace manager, surface manager, frame scheduler, blocking `wl_display_run()` C-API loop. |
| **Display Manager (`tinexus-displayd`)** | 🟡 **PARTIAL / FRAMEWORK** | VT allocation/switching architecture, seat0 acquisition, login supervisor, systemd `READY=1`/`STOPPING=1` socket signals. |
| **Package Manager (`tinexus-pkg`)** | 🟡 **PARTIAL / FRAMEWORK** | `.tinexus` manifest parser, SHA256 checksums, Ed25519 signatures, topological DAG solver, package DB. |
| **Production ISO Builder** | 🟢 **PRODUCTION READY** | Full hybrid BIOS + UEFI bootable ISO generator with Linux 6.x kernel, Intel/MediaTek firmware, and rootfs compression (`~90 MB`). |

> **⚠️ Third-Party App Execution Note:**  
> Flatpak and XWayland are **NOT** implemented — do not add these claims until infrastructure is built and verified. Current application execution for external binaries relies on standalone Wayland binaries or the `tx-appimage` runner.

---

## 🎨 TxUI Design System & Architecture

All desktop consumers have been refactored into modular child widgets powered by **TxUI**:

- **Three-Phase Layout Pipeline**: Strictly ordered `measure()`, `layout()`, `paint()` passes.
- **Deterministic Text Extents**: 100% exact text measurement via `txui::FontMetrics::measure()`, eliminating all hardcoded font approximations.
- **Zero Raw Pointers & Zero `const_cast`**: Complete encapsulation of hit-testing, layout, and component state.
- **Zero External Dependencies**: Zero Qt runtime dependencies in desktop consumers; rendered directly through Pixman / Wayland surfaces.

```
Tinexus Platform Architecture
┌──────────────────────────────────────────────────────────────────┐
│                   TxUI Desktop Shell & Apps                      │
│ [tinexus-shell] [tinexus-files] [tinexus-launcher] [tinexus-dock]│
│ [tinexus-settings] [tinexus-notif] [tinexus-about] [tinexus-lock]│
└─────────────────────────────────┬────────────────────────────────┘
                                  │ Unix Sockets / Shared Memory
┌─────────────────────────────────▼────────────────────────────────┐
│               Core Daemons & IPC Architecture                    │
│ [tinexus-serviced (PID 1)]  ◄──►  [tinexus-ipcd (Broker)]       │
│ [tinexus-searchd]           ◄──►  [tinexus-settings]             │
└─────────────────────────────────┬────────────────────────────────┘
                                  │ Wayland Protocol
┌─────────────────────────────────▼────────────────────────────────┐
│             tinexus-comp (wlroots + Vulkan Compositor)           │
├──────────────────────────────────────────────────────────────────┤
│                       Linux Kernel & DRM/KMS                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 💿 Live Bootable ISO

Tinexus includes a production-grade Hybrid ISO builder that packages the entire operating system, Linux 6.x kernel, hardware firmware, and all TxUI applications into a compact live image:

- **Image Size**: **~90 MB**
- **Boot Support**: Dual **UEFI (x86_64)** + **Legacy BIOS (El Torito / Hybrid MBR)**
- **Hardware Drivers**: Intel i915 iGPU firmware, MediaTek Wi-Fi 6 (MT7921/MT7922), NVMe, USB HID
- **Build Script**: `tools/build_release_iso.sh`

```bash
# Build the production ISO
sudo bash tools/build_release_iso.sh

# Test via QEMU
qemu-system-x86_64 -enable-kvm -m 2G -cdrom build/Tinexus-x86_64.iso -boot d -vga virtio
```

---

## 📚 Documentation Index (Architecture Freeze v1.1)

| # | Document | Description | Status |
|---|---|---|---|
| 00 | [Architecture Review](docs/00_ARCHITECTURE_REVIEW.md) | Principal Architect's risk analysis, ADRs, & tech stack decisions | ✅ FROZEN |
| 01 | [Vision](docs/01_VISION.md) | Philosophy, goals, mission, competitive analysis, platform scope | ✅ FROZEN |
| 02 | [Requirements](docs/02_REQUIREMENTS.md) | Functional, non-functional, security, performance, & use cases | ✅ FROZEN |
| 03 | [System Architecture](docs/03_SYSTEM_ARCHITECTURE.md) | Platform layered architecture, `serviced`/`ipcd`, Vulkan, Workspace Manager | ✅ FROZEN |
| 04 | [Folder Structure](docs/04_FOLDER_STRUCTURE.md) | Monorepo layout specification, ownership matrix, naming conventions | ✅ FROZEN |
| 05 | [UI/UX Guidelines](docs/05_UI_UX_GUIDELINES.md) | Design token system (Motion, Blur, Spacing, Typography, Radius) | ✅ FROZEN |
| 06 | [Component Design](docs/06_COMPONENT_DESIGN.md) | Component specs (`searchd`, `serviced`, `ipcd`, UI boundary) | ✅ FROZEN |
| 07 | [Security](docs/07_SECURITY.md) | Threat model, sandbox, privilege separation, Capability Tokens | ✅ FROZEN |
| 08 | [Performance](docs/08_PERFORMANCE.md) | GPU scheduler, texture atlas, frame pacing, partial rendering | ✅ FROZEN |
| 09 | [Build System](docs/09_BUILD_SYSTEM.md) | CMake monorepo layout, CI/CD, presets, packaging specifications | ✅ FROZEN |
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
| 22 | [LibTxUI Specification](docs/22_LIBTXUI_SPECIFICATION.md) | Pure C++20 UI framework specification, command buffer, ADRs | ✅ FROZEN |

---

## 🛠️ Building From Source

### Prerequisites (Ubuntu/Debian)
```bash
sudo apt update && sudo apt install -y \
    build-essential cmake ninja-build git \
    libpixman-1-dev libwayland-dev wayland-protocols \
    libxkbcommon-dev libpam0g-dev libdbus-1-dev libvulkan-dev \
    xorriso squashfs-tools grub-efi-amd64-bin grub-pc-bin dosfstools mtools
```

### Build Platform Binaries
```bash
# Configure build with CMake Presets
cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja

# Compile all targets
cmake --build build -j$(nproc)
```

---

## 📜 License

Tinexus Platform uses a **per-component license split** (the license depends on the directory, not on a choice by the user):
- Core platform components, daemons & applications: **GPL-2.0-or-later** ([LICENSE-GPL-2.0](LICENSE-GPL-2.0))
- SDK, shared libraries, `libtxui` and Tinexus-authored protocols: **Apache-2.0** ([LICENSE-Apache-2.0](LICENSE-Apache-2.0))
- Third-party protocol XMLs and fonts keep their own licenses.

See [LICENSE](LICENSE) for the exact directory-to-license mapping.
