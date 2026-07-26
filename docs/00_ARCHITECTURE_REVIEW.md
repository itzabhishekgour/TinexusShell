# Tinexus Shell — Principal Architect's Pre-Documentation Review

> **Document Status:** APPROVED FOR DOCUMENTATION FREEZE  
> **Author:** Lead Software Architect, Tinexus Shell Core Team  
> **Revision:** 1.0.0  
> **Date:** 2026-07-25  
> **Classification:** Internal — Architecture Decision Record (ADR-000)

---

## Executive Summary

This document is the **first deliverable** of the Tinexus Shell project. It serves as a rigorous architectural critique, risk identification report, and engineering recommendation document. It is a prerequisite to all subsequent documentation. No code shall be written until all documents listed in the Documentation Plan (Section 6) are frozen, reviewed, and marked complete.

The vision for Tinexus Shell is architecturally sound in its high-level philosophy. However, several implementation-level decisions carry significant risk. This document identifies those risks, challenges assumptions, and provides superior engineering alternatives grounded in industry practice and lessons learned from comparable projects (Sway, Hyprland, GNOME Shell, KDE Plasma, and wlroots itself).

---

## Table of Contents

1. [Vision Assessment](#1-vision-assessment)
2. [Architectural Risk Register](#2-architectural-risk-register)
3. [Technology Stack Review](#3-technology-stack-review)
4. [Design Philosophy Critique](#4-design-philosophy-critique)
5. [Competitive Intelligence](#5-competitive-intelligence)
6. [Documentation Plan](#6-documentation-plan)
7. [Engineering Recommendations](#7-engineering-recommendations)
8. [Decision Log](#8-decision-log)

---

## 1. Vision Assessment

### 1.1 What Tinexus Shell Gets Right

| Principle | Assessment | Rationale |
|---|---|---|
| Wayland-native | ✅ Correct | X11 is legacy. All serious DEs are migrating. Starting Wayland-first is correct. |
| No taskbar / dock | ✅ Bold, defensible | Aligned with modern launcher-centric UX (Raycast, Alfred). Valid paradigm shift. |
| Ctrl+K as primary interaction | ✅ Excellent | Command palette is the future. VSCode, Raycast, Linear, Notion all confirm this. |
| Modular architecture | ✅ Essential | Mandatory for long-term maintainability. Non-negotiable. |
| Modern C++20 | ✅ Appropriate | Gives performance guarantees. Concepts, ranges, coroutines enable cleaner async code. |
| Qt6 for UI | ✅ Sensible | Best C++ UI toolkit available. Strong Wayland support via QtWayland. |
| Documentation before code | ✅ Exemplary | Rare discipline. This is what separates production systems from hobby projects. |

### 1.2 Statements That Need Clarification

These are not wrong, but they are **underspecified** and need formal definition before documentation freeze.

**"Butter Smooth Animations"**  
- This must be defined in measurable terms: target FPS (60/90/120?), frame budget (ms), acceptable jank threshold (% dropped frames).

**"Minimal RAM"**  
- Define "minimal." GNOME Shell at idle uses ~400MB. KDE Plasma uses ~250MB. What is Tinexus Shell's target? This drives compositor architecture choices.

**"GPU Accelerated Rendering"**  
- Qt6 renders via RHI (Vulkan, OpenGL, Metal, D3D12). Which backend is primary? What is the fallback for older hardware? LLVMpipe software rendering must be supported for VMs.

**"Ctrl+K as ONLY launcher"**  
- Edge case: What happens when a user's accessibility profile prevents keyboard use entirely? Is there a pointer-accessible equivalent? This must be answered before UI/UX guidelines are frozen.

---

## 2. Architectural Risk Register

> Each risk is rated: **Probability (P)**, **Impact (I)**, **Severity (S = P × I)** on a 1–5 scale.

### RISK-001: Compositor Complexity Underestimated

| Field | Value |
|---|---|
| **Risk** | Building a Wayland compositor from scratch is one of the hardest problems in systems programming |
| **Probability** | 5/5 |
| **Impact** | 5/5 |
| **Severity** | CRITICAL |
| **Detail** | Compositing involves frame scheduling, damage tracking, buffer management (DMA-BUF, GBM, DRM/KMS), input handling (libinput), XWayland, multi-monitor, HiDPI, VRR, HDR, and protocol negotiation (xdg-shell, layer-shell, etc.). Each of these is a project in itself. |
| **Current Plan** | "Use wlroots if necessary" — this hedge is dangerous. **wlroots is not optional. It is mandatory for Version 1.** |
| **Recommendation** | **Formally adopt wlroots as the compositor foundation.** It handles DRM/KMS, GBM, libinput, DMA-BUF, and ~30 Wayland protocols. Sway, Hyprland, and Wayfire all use it. Build the **Tinexus Shell Compositor (tinexus-comp)** as a wlroots-based compositor. This is not a compromise — it is what the best Wayland compositors do. |

### RISK-002: Qt6 Wayland Layer-Shell Integration

| Field | Value |
|---|---|
| **Risk** | The launcher must appear on top of all windows, including fullscreen applications. This requires `zwlr_layer_shell_v1` which Qt6 does not natively expose cleanly. |
| **Probability** | 4/5 |
| **Impact** | 4/5 |
| **Severity** | HIGH |
| **Detail** | `QWindow`-based surfaces cannot directly set layer-shell properties. This requires either: (a) a custom Qt platform plugin, (b) using Qt's native interface to manually set Wayland surface roles, or (c) using `wlr-layer-shell-unstable-v1` via a thin C binding. |
| **Recommendation** | Use `QWindow::nativeInterface<QNativeInterface::QWaylandWindow>()` combined with a custom Wayland shell integration written in C (compiled as a thin Qt plugin). This is exactly how `qtwayland-layer-shell` works. Document this as a known integration point. |

### RISK-003: IPC Architecture Bottleneck

| Field | Value |
|---|---|
| **Risk** | No IPC mechanism has been specified. Without a defined IPC layer, components become tightly coupled. |
| **Probability** | 4/5 |
| **Impact** | 5/5 |
| **Severity** | CRITICAL |
| **Detail** | The launcher needs to talk to the compositor, session manager, settings daemon, notification daemon, clipboard manager, and app indexer. Without a formal IPC design, each component will invent its own communication pattern — creating a maintenance nightmare. |
| **Recommendation** | Adopt **D-Bus** as the primary IPC mechanism. It is the Linux desktop standard (used by systemd, PulseAudio, NetworkManager, and every major DE). Design formal D-Bus interfaces for each Tinexus Shell service. Supplement with **Unix domain sockets** for high-frequency, low-latency local communication (e.g., compositor ↔ launcher frame sync). |

### RISK-004: No Fallback for Accessibility

| Field | Value |
|---|---|
| **Risk** | A keyboard-only launcher violates accessibility standards for motor-impaired users. |
| **Probability** | 3/5 |
| **Impact** | 4/5 |
| **Severity** | HIGH |
| **Detail** | WCAG 2.1 requires that all functionality be operable without a keyboard. Additionally, switch access, eye-tracking, and pointer-only users cannot use Ctrl+K. |
| **Recommendation** | Design a pointer-accessible "launcher trigger zone" (bottom-center hot corner or floating trigger button) that opens the launcher. It should be optional and configurable. Document this in the accessibility requirements before UI/UX is frozen. |

### RISK-005: App Indexer — Privacy and Performance Conflict

| Field | Value |
|---|---|
| **Risk** | A background indexer that scans files poses privacy risks and CPU/IO overhead. |
| **Probability** | 3/5 |
| **Impact** | 3/5 |
| **Severity** | MEDIUM |
| **Detail** | File indexing in the background can: expose file names in compositor crash dumps, use excessive IO on spinning disks, and conflict with user privacy expectations. |
| **Recommendation** | Index only `.desktop` files (app entries) at startup. File/folder search should use lazy indexing triggered at launcher open time, scoped only to user-specified directories. Implement a privacy whitelist in settings. |

### RISK-006: Plugin System Security Surface

| Field | Value |
|---|---|
| **Risk** | A plugin system is a massive security attack surface if not sandboxed from Day 1. |
| **Probability** | 2/5 |
| **Impact** | 5/5 |
| **Severity** | HIGH |
| **Detail** | Loading arbitrary code (plugins) into the compositor process or launcher process creates privilege escalation risk. GNOME Shell's JavaScript plugins have historically been a source of instability and security issues. |
| **Recommendation** | **Plugins must NEVER run in the compositor process.** Design a **separate plugin host process** with a formal API (JSON over Unix socket or D-Bus). Plugins are isolated processes with limited filesystem access (no `/proc`, no `/sys`, no arbitrary shell execution). This is mandatory from the first plugin spec. |

### RISK-007: Version 2.0 AI Layer — Scope Creep Risk

| Field | Value |
|---|---|
| **Risk** | "Future AI" mentioned in requirements is architecturally vague and could cause premature over-engineering. |
| **Probability** | 4/5 |
| **Impact** | 3/5 |
| **Severity** | MEDIUM |
| **Detail** | "AI" is mentioned as a future launcher category without defining what AI means here: local LLM, cloud API, semantic search, NLP command parsing? Each option has radically different architecture, privacy, and latency implications. |
| **Recommendation** | Define AI as: **natural language command parsing for the launcher** in v2.0, using a local model (llama.cpp / whisper.cpp) with no cloud dependency. Document this explicitly so the launcher's search architecture can be designed to accommodate NLP tokenization as a future search backend without a rewrite. |

---

## 3. Technology Stack Review

### 3.1 Approved Stack

```
Layer               Technology          Verdict     Notes
─────────────────────────────────────────────────────────────────────
Kernel Interface    Linux Kernel        ✅ Use as-is  Never modify
Display Protocol    Wayland             ✅ Correct    Native
Compositor Base     wlroots             ✅ MANDATORY  Not optional
Window Manager      Custom (wlroots)    ✅ Correct    Tiling optional
UI Toolkit          Qt6 (QML + C++)     ✅ Best fit   Use QtQuick for animations
Language            C++20               ✅ Correct    Enforce clang-format
Build System        CMake 3.28+         ✅ Correct    Use presets
IPC                 D-Bus + Unix Sock   ✅ Recommend  See RISK-003
Config Format       TOML                ✅ Recommend  Human-readable, typed
Shader Language     GLSL / SPIR-V       ✅ Correct    Via Qt RHI
Animation Engine    Qt Quick            ✅ Correct    Scene graph GPU accelerated
Search              Custom FTS engine   ⚠️ Evaluate  Consider Xapian for v1
File Indexing       inotify + lazy scan ✅ Correct    Never daemon-heavy
Session Manager     Custom (D-Bus)      ✅ Correct    systemd-logind integration
Notifications       Custom D-Bus impl   ✅ Correct    Freedesktop spec compliant
Testing             GoogleTest + Catch2 ✅ Recommend  Unit + Integration
CI                  GitHub Actions      ✅ Recommend  Matrix: GCC + Clang
Packaging           Flatpak + .deb/rpm  ✅ Recommend  Flatpak for distribution
```

### 3.2 Technology Decisions That Need Formal ADRs

The following decisions must each produce an **Architecture Decision Record** before the build system document is frozen:

| Decision | Options | Recommended |
|---|---|---|
| Qt rendering backend | OpenGL / Vulkan / Software | Vulkan primary, OpenGL fallback |
| Config file format | JSON / TOML / YAML | TOML (human-friendly, typed) |
| Search algorithm | FTS / Trigram / Fuzzy | Custom trigram with edit-distance scoring |
| IPC transport | D-Bus only / D-Bus + sockets | Hybrid (D-Bus for discovery, sockets for data) |
| Compositor process model | Single process / Separated | Single compositor + plugin host isolation |
| Theme format | CSS / QSS / Custom tokens | Custom JSON token system + QML theming |

---

## 4. Design Philosophy Critique

### 4.1 "Everything starts with simplicity"

**Assessment: Correct philosophy, incomplete specification.**

Simplicity in UI design does not mean absence of complexity in engineering. The simplest user interfaces (iPhone, Raycast) have some of the most complex engineering behind them. The documentation must distinguish:

- **User-perceived simplicity** (empty desktop, single launcher)
- **Implementation complexity** (compositor, GPU pipeline, search engine)

These are not in conflict, but conflating them in design discussions leads to poor prioritization.

### 4.2 No taskbar / dock / desktop icons

**Assessment: Defensible for power users, potential adoption barrier.**

The risk: New Linux users coming from Windows/macOS will find no visual affordance for running applications. The Ctrl+K paradigm requires the user to already know what they want. Discovery of new applications becomes harder.

**Recommendation:** The design should be flexible enough to show:
- An **optional** minimal dock (3–5 pinned apps) that can be enabled in settings
- This should be implemented as an optional **layer-shell surface** that is **off by default**

This does not compromise the philosophy — it extends it. The system default is "empty desktop." Power users keep it empty. Less experienced users can enable a minimal dock. Document this as a **tiered UX configuration** model.

### 4.3 The Ctrl+K Launcher

**Assessment: Excellent as primary, must be supplemented.**

The launcher should support:
1. **Fuzzy application search** (like Raycast)
2. **Inline calculations** (2 + 2 = 4 instantly displayed)
3. **File path navigation** (~ to home, /path to root)
4. **System actions** (Shutdown, Sleep, Lock, Restart) as first-class commands
5. **Clipboard history** with fuzzy search
6. **Recent files** from app-reported history (via a standard protocol)
7. **Extension commands** from plugins (future)
8. **AI commands** prefixed with `>` or `?` (future)

The launcher's search architecture must treat these as **pluggable search providers**, not hardcoded categories. This is the single most important architectural decision for the launcher.

---

## 5. Competitive Intelligence

### 5.1 Comparable Projects and Lessons Learned

| Project | Language | Lesson for Tinexus Shell |
|---|---|---|
| **Sway** | C + wlroots | wlroots is mature and production-ready. Their IPC (sway-ipc over unix socket) is a good reference. |
| **Hyprland** | C++20 + wlroots | Proves C++20 + wlroots works beautifully. Their animations system (bezier curves) is reference quality. |
| **Wayfire** | C++ + wlroots | Plugin system via .so loading — avoid this model (security risk). Use process isolation instead. |
| **GNOME Shell** | C + Mutter + JS | JS plugin system caused instability. Avoid dynamic scripting in compositor. |
| **KDE Plasma** | C++ + Qt6 | Best Qt6 integration reference. Their KWin compositor is highly optimized. Study KWin's buffer management. |
| **Raycast** | Swift + macOS | Best UX reference for launchers. Study their extension model and result ranking. |
| **Niri** | Rust + Smithay | Scrollable window layout, Rust proves memory safety in compositors is achievable. Consider if team Rust expertise exists. |

### 5.2 Differentiation Strategy

Tinexus Shell must be differentiated on:

1. **UX philosophy** — The "everything from Ctrl+K" paradigm
2. **Performance** — Sub-100ms launcher open, 60fps guaranteed animations
3. **Modularity** — Every component is a separate D-Bus service
4. **Documentation quality** — Linux Foundation standard from Day 1
5. **Developer experience** — Clear APIs, documented protocols, great onboarding

---

## 6. Documentation Plan

### 6.1 Document Registry

| ID | Document | Owner | Status | Priority |
|---|---|---|---|---|
| 00 | Architecture Review (this document) | Lead Architect | ✅ COMPLETE | P0 |
| 01 | VISION.md | Lead Architect | 🔄 In Progress | P0 |
| 02 | REQUIREMENTS.md | Systems Engineer | 🔄 In Progress | P0 |
| 03 | SYSTEM_ARCHITECTURE.md | Lead Architect | 🔄 In Progress | P0 |
| 04 | FOLDER_STRUCTURE.md | Lead Engineer | 🔄 In Progress | P1 |
| 05 | UI_UX_GUIDELINES.md | Design Lead | 🔄 In Progress | P1 |
| 06 | COMPONENT_DESIGN.md | Component Leads | 🔄 In Progress | P1 |
| 07 | SECURITY.md | Security Architect | 🔄 In Progress | P0 |
| 08 | PERFORMANCE.md | Performance Engineer | 🔄 In Progress | P1 |
| 09 | BUILD_SYSTEM.md | Build Engineer | 🔄 In Progress | P1 |
| 10 | ROADMAP.md | Project Manager | 🔄 In Progress | P2 |

### 6.2 Documentation Freeze Criteria

A document is considered **FROZEN** when:

- [ ] All sections are complete (no `TODO` or `TBD` markers)
- [ ] All Mermaid diagrams render without error
- [ ] All cross-references to other documents are valid
- [ ] At least one architectural peer review has been completed
- [ ] The document has been committed to the `docs/` directory with a version tag

### 6.3 Documentation Dependency Order

```
ADR-000 (This document)
    ├── 01_VISION.md           (no dependencies)
    ├── 02_REQUIREMENTS.md     (depends on: 01)
    ├── 07_SECURITY.md         (depends on: 01, 02)
    ├── 03_SYSTEM_ARCHITECTURE.md  (depends on: 01, 02, 07)
    │       ├── 04_FOLDER_STRUCTURE.md  (depends on: 03)
    │       ├── 06_COMPONENT_DESIGN.md  (depends on: 03)
    │       └── 05_UI_UX_GUIDELINES.md  (depends on: 03)
    ├── 08_PERFORMANCE.md      (depends on: 02, 03)
    ├── 09_BUILD_SYSTEM.md     (depends on: 03, 04)
    └── 10_ROADMAP.md          (depends on: all above)
```

---

## 7. Engineering Recommendations

### REC-001: Adopt a Layered Service Model

Every Tinexus Shell daemon must be:
- A standalone process with a clean D-Bus interface
- Restartable without restarting the compositor
- Configurable via TOML in `~/.config/Tinexus Shell/`
- Testable in isolation (without a running compositor)

### REC-002: Define Protocol Stability Tiers

Explicitly mark all internal APIs:
- **STABLE** — Frozen after v1.0. Breaking changes require major version bump.
- **UNSTABLE** — May change between minor versions. Documented as such.
- **PRIVATE** — Internal implementation detail. Not part of any public API.

### REC-003: Adopt Semantic Versioning from Day 1

`MAJOR.MINOR.PATCH` with:
- MAJOR: Breaking API change
- MINOR: New feature, backward compatible
- PATCH: Bug fix

### REC-004: Establish a Code Quality Gate

Before code is written, define:
- Clang-format style (based on LLVM style with modifications)
- Clang-tidy checks (minimum: cppcoreguidelines, modernize, performance)
- Address Sanitizer in all Debug builds
- UBSan in all Debug builds
- Valgrind integration in CI

### REC-005: Design for Testability from Day 0

Every component must have:
- A mock/stub interface for testing without hardware
- Unit tests in `tests/unit/`
- Integration tests in `tests/integration/`
- A headless compositor mode for CI testing (using wlroots' headless backend)

### REC-006: Establish a Formal ADR Process

Every major technical decision must produce an Architecture Decision Record:
- File: `docs/adr/ADR-NNN-title.md`
- Fields: Status, Context, Decision, Consequences, Alternatives Considered
- Immutable once accepted (new ADRs supersede old ones, never edit accepted ADRs)

### REC-007: Memory Safety Strategy

C++ memory management decisions:
- Prefer `std::unique_ptr` / `std::shared_ptr` over raw pointers in all new code
- Use RAII wrappers for all C library handles (wlroots, libinput, etc.)
- No `new` / `delete` without a corresponding smart pointer wrapper
- Address Sanitizer enabled in Debug mode
- Consider a Rust FFI boundary for the most security-critical components (input handling) in v2.0

---

## 8. Decision Log

> This log records architectural decisions made during the pre-documentation phase.

| ADR | Decision | Rationale | Status |
|---|---|---|---|
| ADR-001 | Use wlroots as compositor foundation | Reduces compositor complexity from years to months | ACCEPTED |
| ADR-002 | D-Bus as primary IPC | Industry standard, tooling, introspection support | ACCEPTED |
| ADR-003 | Qt6 + QML for all UI | Best C++ GUI toolkit, GPU-accelerated scene graph | ACCEPTED |
| ADR-004 | TOML for all config files | Typed, human-readable, better than INI | ACCEPTED |
| ADR-005 | Plugin host process isolation | Security non-negotiable, no .so loading into compositor | ACCEPTED |
| ADR-006 | Launcher uses pluggable search providers | Future AI/NLP compatibility, extensibility | ACCEPTED |
| ADR-007 | Optional minimal dock (off by default) | Accessibility and UX adoption — preserves philosophy | ACCEPTED |
| ADR-008 | Semantic versioning from v0.1 | Engineering discipline, clear API contracts | ACCEPTED |
| ADR-009 | CMake Presets for build configuration | Reproducible builds, CI/CD compatibility | ACCEPTED |
| ADR-010 | AI = local NLP only (no cloud) | Privacy-first, offline functionality | ACCEPTED |

---

*End of Architecture Review — ADR-000*  
*Next document: 01_VISION.md*
