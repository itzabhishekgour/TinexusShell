# Document 22: libtxui — Tinexus Native C++20 UI Framework Specification

> **STATUS**: `FROZEN (v1.0)`  
> **APPLIES TO**: `libtxui` (`src/txui/`, `include/txui/`)  
> **AUTHORITY**: Master Architecture Specification for Phase 4+ (Desktop Experience Engineering)

---

## 1. Executive Summary & Core Philosophy

`libtxui` (**Tinexus UI**) is a standalone, high-performance, declarative-retained C++20 GUI framework engineered as the official native display toolkit for the Tinexus Desktop OS.

### 1.1 Core Tenets
- **"No Qt/GTK Dependency in Core Applications"**: Tinexus defines its own visual identity, event model, and rendering pipeline.
- **Backend-Agnostic Command Rendering**: Widgets record drawing operations into a `CommandBuffer`. The underlying `Renderer` executes commands via interchangeable backends (`PixmanBackend` for CPU software rendering, `VulkanBackend` for GPU hardware acceleration) without widget-layer awareness.
- **Strict Layered Dependency Hierarchy**: Modules depend strictly downward. Upward or circular dependencies are forbidden by architectural contract.
- **Accessibility (`a11y`) First**: Semantic nodes and screen-reader hooks are integrated into the root `Object` and `Widget` hierarchy from Day 1.

### 1.2 Non-Goals
- **Non-Goal**: We do not provide X11 or Win32 legacy desktop compatibility layers in core `libtxui`.
- **Non-Goal**: We do not implement immediate-mode GUI drawing (`ImGui` style) where state is lost per frame; `libtxui` uses retained widget trees with command-buffered rendering.

---

## 2. Strict Layered Architecture & Dependency Governance

`libtxui` enforces a **strict downward dependency hierarchy**. Any source file importing a header from a layer above its own will fail architectural linting and code review.

```
┌────────────────────────────────────────────────────────┐
│                      Application                       │  (Tinexus Shell, Files, Settings)
├────────────────────────────────────────────────────────┤
│                       widgets/                         │  (Container, Row, Column, ScrollArea, Window)
├────────────────────────────────────────────────────────┤
│                       layout/                          │  (Constraints, Measure, Layout, Box Model)
├────────────────────────────────────────────────────────┤
│                        theme/                          │  (HSL Tokens, Typography, Radius, Elevation)
├────────────────────────────────────────────────────────┤
│                      animation/                        │  (Spring Physics, Tweens, Frame Scheduler)
├────────────────────────────────────────────────────────┤
│                        input/                          │  (Hit Testing, Event Bubbling, Focus Tree)
├────────────────────────────────────────────────────────┤
│                       effects/                         │  (Blur API, Shadow Shaders, Glassmorphism)
├────────────────────────────────────────────────────────┤
│                        render/                         │  (Canvas, Painter, CommandBuffer, Backend)
├────────────────────────────────────────────────────────┤
│                       graphics/                        │  (Color, Gradient, Brush, Border, Path)
├────────────────────────────────────────────────────────┤
│                        math/                           │  (Point, Size, Rect, Matrix4, Vector2/3)
├────────────────────────────────────────────────────────┤
│                        core/                           │  (Object, Ref, Types, Assert, Logger, Time)
└────────────────────────────────────────────────────────┘
```

### 2.1 Dependency Rules Matrix
- **✅ ALLOWED**: `Widget` (`widgets/`) including `Painter.hpp` (`render/`) or `Brush.hpp` (`graphics/`).
- **❌ FORBIDDEN**: `Painter.hpp` (`render/`) referencing `Widget.hpp` (`widgets/`).
- **❌ FORBIDDEN**: `Brush.hpp` (`graphics/`) referencing `Canvas.hpp` (`render/`).

---

## 3. Public vs. Private API Boundary

To ensure ABI stability and prevent internal implementation leakage:

```
Tinexus/
├── include/txui/            # PUBLIC API — Consumer headers only
│   ├── core/
│   ├── math/
│   ├── graphics/
│   ├── render/
│   ├── layout/
│   ├── widgets/
│   ├── theme/
│   ├── animation/
│   └── accessibility/
│
└── src/txui/                # PRIVATE IMPLEMENTATION — Internal .cpp & helper headers
    ├── core/
    ├── math/
    ├── graphics/
    ├── render/
    │   ├── pixman/          # Software CPU Backend
    │   └── vulkan/          # Hardware GPU Backend
    └── ...
```

- Applications (`shell`, `files`) link against `txui` via CMake: `target_link_libraries(<app> PRIVATE txui)`.
- Applications must ONLY `#include <txui/modulename/header.hpp>`.
- Including headers from `src/txui/` directly is illegal.

---

## 4. Rendering Core Pipeline (`libtxui 0.1`)

The rendering engine separates command recording from rasterization.

```
       [Widget Tree]
             │
             │ widget->paint(PaintEvent&)
             ▼
     [txui::Painter]  ──(Records Drawing Primitives)──┐
             │                                        │
             │ painter.fillRect(rect, brush)          ▼
             │                               [txui::CommandBuffer]
             │                                        │
             ▼                                        │
   [txui::Renderer]  ◄──(Consumes Commands)───────────┘
             │
             ├───────────────────────┬───────────────────────┐
             ▼                       ▼                       ▼
   [txui::PixmanBackend]   [txui::VulkanBackend]    [txui::TestBackend]
   (CPU Software Render)   (GPU Hardware Render)     (Headless Verification)
```

### 4.1 Key Architectural Decisions
1. **Painter Never Uses Raw Color Directives**:
   - All fills and strokes use `txui::Brush` (`SolidColor`, `Gradient`, `ImagePattern`).
   - This decouples color values from shader pipeline states and enables zero-cost theme transitions.
2. **Command Buffering**:
   - `Painter` produces immutable drawing commands (`DrawRectCommand`, `DrawPathCommand`, `SetClipCommand`).
   - Commands are batched, culled, and sorted before execution by the active `Backend`.
3. **Widgets Do Not Inherit Painter**:
   - Following modern browser and Flutter engines, widgets implement a virtual `paint(PaintEvent& event)` callback that receives a temporary context containing the `Painter`.

---

## 5. Accessibility (`a11y`) First-Class Architecture

To ensure Tinexus is fully accessible without legacy bolt-on hacks:
- Every `txui::Object` in the visual hierarchy exposes an `IAccessibleNode` interface.
- Core attributes required on all semantic nodes:
  - `SemanticRole`: `Button`, `Label`, `Container`, `Window`, `List`, `Image`.
  - `Label / Hint`: Screen-reader descriptive strings.
  - `FocusState`: Explicit keyboard and accessibility focus flags.
  - `BoundingBox`: Global screen coordinates for screen magnifier and voice-over tracking.

---

## 6. Official `libtxui` Release Roadmap & Milestone Contract

```
libtxui 0.1  ──►  Rendering Core (Core, Math, Graphics, Render + PixmanBackend + Blur Stub) [ZERO CONTROLS]
libtxui 0.2  ──►  Layout Engine (Constraints → Measure() → Layout() Box Model & Flex/Grid Layouts)
libtxui 0.3  ──►  Event System (Hit Testing, Focus Tree, Pointer/Keyboard Event Bubbling)
libtxui 0.4  ──►  Animation Engine (Apple-style Natural Spring Physics, Tweening, Timeline Scheduler)
libtxui 0.5  ──►  Theme Engine (HSL Design Tokens, Typography, Spacing, Token-Driven Dark/Light Mode)
libtxui 0.6  ──►  Core Controls (Button, Label, Icon, Image, Checkbox, Slider, Switch, ListView, GridView)
libtxui 1.0  ──►  Production Stable C++20 UI Framework (Ready for Tinexus Shell, Finder-style Files, Dock)
```

### 6.1 Strict Phase 4.1 Deliverables Checklist
By the completion of **Phase 4.1 (`libtxui 0.1`)**, the codebase MUST contain:
- [x] Complete directory structure (`include/txui/` and `src/txui/`).
- [x] Functional CMake library target `txui` with C++20 standard compliance.
- [x] `core/`: `Object`, `Ref`, `NonCopyable`, `Types`, `Assert`, `Logger`, `Time`, `UUID`.
- [x] `math/`: `Point`, `Size`, `Rect`, `Insets`, `Matrix4`, `Transform`, `Vector2`, `Vector3`, `ColorSpace`.
- [x] `graphics/`: `Color`, `Gradient`, `Brush`, `Pen`, `Border`, `Shadow`, `CornerRadius`, `Image`, `Path`.
- [x] `render/`: `Canvas`, `Painter`, `RenderTarget`, `CommandBuffer`, `Renderer`, `Backend`, `PixmanBackend`.
- [x] `effects/`: `BlurEffect` API stub (`Radius`, `Sigma`, `Quality`).
- [x] **ZERO UI BUTTONS OR CONTROLS**: No control classes permitted in `0.1`.

---

## 7. Architecture Decision Records (ADR) Governance

All major engineering decisions in `libtxui` MUST be recorded as immutable markdown files under `docs/adr/`.

### 7.1 Required Initial ADR Index
- `docs/adr/0001-command-buffer-rendering.md` — Rationale for CommandBuffer vs. Immediate Mode drawing.
- `docs/adr/0002-render-backend-abstraction.md` — Decoupling Painter from Pixman/Vulkan rasterizers.
- `docs/adr/0003-widget-tree-ownership.md` — Ref-counted intrusive pointer model (`Ref<T>`) for UI nodes.
- `docs/adr/0004-layout-engine-box-model.md` — Three-phase layout (`Constraints -> Measure -> Layout`).
- `docs/adr/0005-theme-engine-tokens.md` — Dynamic HSL design tokens over static hardcoded styles.

---

## 8. Demo, Testing, Benchmark & Stability Governance

### 8.1 The Standalone Demo Rule
> **"Phase complete tabhi hogi jab uska standalone demo chale."**
- Simply compiling the library is insufficient to close a phase. Every release milestone MUST include an independently executable demo program in `examples/` that verifies the pipeline.

```
examples/
├── 01_canvas/      # Verifies Canvas → Painter → CommandBuffer → Backend → Target
├── 02_shapes/      # Verifies Rect, RoundedRect, Circle, Border primitives
├── 03_gradient/    # Verifies linear and radial gradient brush rendering
├── 04_painter/     # Verifies transformations, clipping, and state stacking
├── 05_layout/      # Verifies Three-Phase Box Model constraint solver
└── 06_widgets/     # Verifies Widget tree, focus, and event bubbling
```

### 8.2 Testing & Benchmark Suite
- **Automated Unit Tests (`tests/txui/`)**: Every module (`core`, `math`, `graphics`, `render`, `layout`) must provide unit tests executed via `ctest`.
- **Performance Benchmarks (`benchmarks/`)**:
  - `paint_1000_rectangles`
  - `layout_10000_widgets`
  - `text_shaping`
  - `blur`
  - Any performance regression detected in benchmarks blocks merging.

### 8.3 Namespace & ABI Policy
- **Public API Namespace**: `namespace txui { ... }`
- **Private Implementation Namespace**: `namespace txui::internal { ... }`
- **Application Restriction**: Applications MUST NEVER reference `txui::internal`.
- **CRTP & Value Types**: Public API minimizes runtime `virtual` overhead; value types, CRTP, and composition are preferred.
- **ABI Stability**: During `0.x` milestones, binary ABI is not frozen and breaking changes are permitted. Upon reaching `1.0`, strict semantic versioning and ABI stability rules take effect.

### 8.4 Ultimate Exit Criteria for `libtxui 0.1` (Rendering Core)
- [ ] Builds independently as a static/shared CMake target (`txui`).
- [ ] Runs independently via `txui-demo-01` (`examples/01_canvas`).
- [ ] Has automated unit tests in `tests/txui/`.
- [ ] Has benchmark scaffolding in `benchmarks/`.
- [ ] Can be linked cleanly by `tinexus-shell` without symbol conflicts.
- [ ] Zero dependency on Qt, GTK, or X11.

### 8.5 Tinexus Engineering Rule #1 — Zero Placeholder Policy
> **"No placeholders. No dummy implementations. No fake success paths."**
1. **End-to-End Working Only**: Any feature exposed in public API headers must be fully functioning end-to-end. If it cannot be completed, it must not be declared.
2. **No Dummy Returns**: Stub implementations returning `true` or empty function bodies (`// TODO`) are strictly illegal.
3. **PixmanBackend Integrity**: Must execute real Pixman rasterization (`pixman_image_create_bits`, `pixman_fill`, `pixman_composite`), clipping, transforms, and alpha blending.
4. **5-Point Prerequisite for Public API Exposure**:
   A class or method may ONLY enter public headers if it has:
   - ✅ Complete implementation
   - ✅ Working example executable
   - ✅ Automated unit test verifying pixel/data integrity
   - ✅ Performance benchmark
   - ✅ Complete documentation

### 8.6 Tinexus Engineering Rule #2 — Vertical Slice Implementation Policy
> **"Implement Vertically, Not Horizontally"**
1. **Vertical Slicing**: Features are built one end-to-end working slice at a time. Exposing empty or partially implemented horizontal headers is strictly forbidden.
2. **Phase 4.2 Sub-Milestones**:
   - `4.2.1` — **Solid Rectangle** (✓ Completed): Immutable `Color` → `SolidBrush` → `DrawRect` Command (`std::variant`) → `CommandBuffer` → `Painter` → `Canvas` → `PixmanBackend` → `output.png` generated & pixel-verified.
   - `4.2.2` — **Rounded Rectangle**: Anti-aliased corner clipping & pixel correctness.
   - `4.2.3` — **WaylandRenderTarget**: Real Wayland shared memory buffer target (`wl_display`, `wl_compositor`, `wl_shm`, `wl_surface`, `wl_buffer`).
   - `4.2.4` — **Live Window on Tinexus**: First visible milestone running `txui-window-demo` inside `tinexus-comp` on QEMU/ISO.
   - `4.3` — **Widget Tree & Layout**
   - `4.4` — **Event System**

### 8.7 Tinexus Engineering Rule #3 — Production API Exclusivity Policy
> **"No Demo Code vs. Production Code Split"**
1. **No Dummy/Demo UI Classes**: Creating `DemoWindow`, `ExampleCanvas`, or `TestWidget` is strictly illegal. Examples and test applications MUST be consumers of production APIs (`WaylandWindow`, `RenderTarget`).
2. **Abstract RenderTarget Architecture**:
   - `RenderTarget` abstract base interface decouples rendering from storage.
   - `CanvasRenderTarget` serves unit tests and offscreen verification.
   - `WaylandRenderTarget` serves real desktop Wayland shared memory (`wl_shm`).
3. **Zero Duplicate Rendering**: All Tinexus desktop applications (`tinexus-launcher`, `tinexus-panel`, `tinexus-lock`, `tinexus-files`) share the exact same `libtxui` rendering pipeline.

### 8.8 Tinexus Engineering Rule #4 — Risk-Reduction Engineering Gates
> **"Every vertical slice must remove risk, not just add features."**
1. **Gate 0: Rendering Core Validation (Prerequisite for Phase 4.2.2)**:
   - **Command Buffer Immutability**: `Painter` only records commands; rasterization never occurs inside `Painter`. A single `CommandBuffer` can be replayed across multiple targets (`renderer.execute(cb, target1); renderer.execute(cb, target2);`).
   - **Pixel Format Freeze**: **Premultiplied ARGB8888** (`ARGB8888 premultiplied alpha`) is permanently frozen for Pixman, Cairo, Skia, Wayland SHM, and GPU blending.
   - **Coordinate Precision**: Geometry uses `double` (`float64`); Rasterizer uses integer coordinates (`int32`).
   - **Stateless Renderer**: `Renderer` and `Backend` retain zero drawing state across executions.
   - **Backend Independence & Replay Test**: Automated test (`unit_test_txui_gate0`) verifies identical FNV-1a pixel hashes across replayed targets.

---

*libtxui Specification — Tinexus Platform*  
*Architecture Freeze v1.0 Complete*
