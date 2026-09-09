# Tinexus Platform: Comprehensive Window Management & Animation Subsystem Audit

**Document Status**: COMPLETED AUDIT  
**Date**: September 9, 2026  
**Target Branch**: `feature/window-management-animation-v1`  
**Baseline HEAD**: `ef9d0de`  
**Scope**: Window Lifecycle, State Machine, Resize/Maximize Layout, Compositor Animation Architecture, Portability, and Hardware Invariants.

---

## 1. Executive Summary

A rigorous, code-level audit was conducted across all subsystems of the Tinexus desktop platform: `src/comp/` (Compositor & wlroots integration), `src/txui/` (UI framework & Wayland client runtime), `src/dock/`, `src/launcher/`, `src/shell/`, `src/monitor/`, `src/about/`, `src/settings-ui/`, and `src/common/`.

### Key Discoveries:
1. **The "Two Parallel Universes" Architectural Disconnect**:
   - **Universe A (Production Execution)**: `src/comp/backend/wlroots_backend.cpp` directly manages Wayland toplevels via `ToplevelWrapper`, `wlr_xdg_toplevel`, and `wlr_scene_tree`. It handles mapping, focusing, resizing, and maximizing.
   - **Universe B (Mock / Test Island)**: `src/comp/window/window_manager.cpp`, `scene_graph.cpp`, and `SurfaceManager` maintain separate classes (`WindowNode`, `SurfaceRecord`) and mock animations (`MinimizeAnimation`, `RestoreAnimation`).
   - **The Break**: `WlrootsBackend` **never registers** real Wayland toplevel surfaces with `WindowManager`. Consequently, when `handle_toplevel_request_minimize` calls `TinexusServer::trigger_minimize()`, `WindowManager::instance().find_window(surface_id)` returns `nullptr`. **Minimize animations and Dock state notifications are completely inactive in production.**
2. **Activity Monitor Responsive UI Breakdown**:
   - The Activity Monitor window UI looks broken upon maximize/resize due to three compounded root causes:
     - **Hardcoded Column Coordinates**: In `MonitorWidget.cpp:583-590`, all 7 process table columns are hardcoded pixel offsets relative to `table_card.x() + 16/216/280/416/496/596/700`. On a 1920px display, the table spans 1872px, leaving a massive ~1100px blank void on the right.
     - **Hardcoded Chart Heights & Metric Y-Offsets**: In `MonitorWidget.cpp:398`, `card_y` is hardcoded at `area.y() + 160.0 * 2.0 + 32.0`. On a maximized window (~1048px tall), the bottom half of the window is completely empty.
     - **Unconditional Client-Side Frame Rounding & Shadow**: `txui::ChromeWidget.cpp:58-75` unconditionally paints an 11px rounded rectangle and drop shadow even when the window is maximized flush against the screen boundaries (`0, 32, 1920, 1048`), causing visible rounded corners and black border cutouts against the screen edge.
3. **Hardcoded Screen Dimensions in Compositor Maximization**:
   - In `src/comp/backend/wlroots_backend.cpp:630` and `711`, maximize and fullscreen fallback to hardcoded `{0, 0, 1280, 800}` and unconditionally query `m_outputs.front()`, ignoring multi-output geometry and secondary monitor positions.
4. **Safety-Critical Timer Leaks & Use-After-Free**:
   - `handle_toplevel_destroy` in `wlroots_backend.cpp:881` does **not** cancel `wrapper->fade_timer` or clear `m_grabbed_toplevel`. If a client crashes while fading out or while being interactively dragged, the timer callback or cursor motion dereferences freed memory, causing a compositor segfault.

---

## 2. Current Architecture & Ownership Map

| Responsibility | Owning Component & File | Implementation Status & Notes |
|---|---|---|
| **XDG Surface & Toplevel Creation** | `src/comp/backend/wlroots_backend.cpp:766` (`handle_new_xdg_toplevel`) | **REAL (wlroots)**: Creates `ToplevelWrapper`, wraps in `wlr_scene_xdg_surface_create(m_scene_tree_normal)`. |
| **Window State Tracking** | `src/comp/backend/wlroots_backend.cpp:407-422` (`ToplevelWrapper`) | **PARTIAL**: Stores `is_maximized`, `is_fullscreen`, `saved_x/y/w/h`. Lacks `is_minimized`, normal state enum, or output ID. |
| **Abstract Window Manager** | `src/comp/window/window_manager.cpp` | **DISCONNECTED**: Manages `WindowNode` instances in an isolated map. Never populated by `WlrootsBackend`. |
| **Surface Lifecycle Manager** | `src/comp/surface/surface_manager.cpp` | **DISCONNECTED**: State machine exists for unit tests; not updated by live Wayland toplevels. |
| **Layer Shell (Dock / Aura)** | `src/comp/backend/wlroots_backend.cpp:425` (`handle_new_layer_surface`) | **REAL (wlroots)**: Anchored to scene trees (`m_scene_tree_top`, etc.). Fully functional. |
| **Window Maximization** | `src/comp/backend/wlroots_backend.cpp:615` (`toplevel_set_maximized`) | **FLAWED**: Hardcoded to `m_outputs.front()` and `1280x800` fallback; hardcoded 32px top bar offset. |
| **Window Minimization** | `src/comp/backend/wlroots_backend.cpp:868` (`handle_toplevel_request_minimize`) | **NON-FUNCTIONAL**: Delegates to `server.cpp:489` which fails to find window in `WindowManager`. |
| **Window Close Animation** | `src/comp/backend/wlroots_backend.cpp:657` (`handle_fade_out`) | **PARTIAL / HAZARDOUS**: 15ms timer decrements buffer opacity. Timer is leaked on premature destroy. |
| **Client Window Framework** | `src/txui/window/Window.cpp` | **REAL**: Handles Wayland connection, `xdg_surface`, `xdg_toplevel`, `Window::present()`, `m_needs_repaint`. |
| **Client Window Chrome** | `src/txui/widgets/ChromeWidget.cpp` | **REAL**: Wraps content with TitleBar (traffic lights) and drop shadow; lacks maximized-state styling. |
| **Interactive Move / Resize** | `src/comp/backend/wlroots_backend.cpp:736, 746` | **REAL**: Interactively adjusts position / size on cursor motion. |
| **Output / Frame Scheduling** | `src/comp/output/output.cpp:150` (`TinexusOutput::frame`) | **REAL**: Gated on `wlr_scene_output_needs_frame()`. Clean 0% idle CPU preserved. |

---

## 3. Current Window State Machine Audit

### Detailed Evaluation Against the 16 Required Audit Points:

1. **Does a window have Normal state?**
   - **Implicit Only**: There is no `WindowState` enum in `ToplevelWrapper`. Normal is represented merely as `!is_maximized && !is_fullscreen`.
2. **Does it have Maximized state?**
   - **Yes, boolean flag**: `wrapper->is_maximized` in `ToplevelWrapper`.
3. **Does it have Minimized state?**
   - **No**: `ToplevelWrapper` has NO `is_minimized` member. `WindowNode` has `bool minimized{false}`, but `WindowNode` is never instantiated for real toplevels.
4. **Does it have Fullscreen state?**
   - **Yes, boolean flag**: `wrapper->is_fullscreen` in `ToplevelWrapper`.
5. **Is previous geometry stored?**
   - **Partially**: `wrapper->saved_x`, `saved_y`, `saved_width`, `saved_height` exist in `ToplevelWrapper`.
6. **Is previous geometry restored correctly?**
   - **Partially**: When un-maximizing (`toplevel_set_maximized(wrapper, false)`), it restores `saved_x/y` and sends configure with `saved_width/height`. However, if the user moves or resizes the window while maximized, saved geometry is corrupted or not updated.
7. **Is maximization compositor-side or client-side?**
   - **Hybrid Wayland Protocol**: Compositor initiates by updating scene node position to `(0, 32)` and calls `wlr_xdg_toplevel_set_size()`. Client is expected to receive `configure` and re-render its buffer to match.
8. **How are configure events generated?**
   - Via `wlr_xdg_surface_schedule_configure(wrapper->toplevel->base)`.
9. **How are configure events acknowledged?**
   - Client calls `xdg_surface_ack_configure(xdg_surface, serial)` inside `src/txui/window/Window.cpp:27`.
10. **What happens when the client provides a new buffer?**
    - The client calls `wl_surface_commit()`. `wlroots` updates the `wlr_scene_buffer` in `m_scene_tree_normal`. Wlroots damages the output area, triggering `TinexusOutput::frame()`.
11. **What happens if the client refuses / resizes differently?**
    - If a client commits a buffer smaller or larger than requested, the compositor's scene node renders the buffer at its actual pixel dimensions starting from `(node.x, node.y)`. Because no compositor viewport clipping or letterboxing is applied, client buffers overflow or underflow the allocated screen tile.
12. **What happens during rapid resize?**
    - In `src/comp/backend/wlroots_backend.cpp:1145`, cursor motion during interactive resize repeatedly calls `wlr_xdg_toplevel_set_size()`. Each call queues a configure event. TXUI clients process these in `Window::on_configure()`, reallocating Cairo/shm surfaces. If client buffer generation lags behind mouse movement, visual tearing/stretching occurs temporarily until the client catches up.
13. **What happens during maximize → restore?**
    - Compositor restores `saved_x/y` and sends configure with `saved_w/h`. If the window was previously at `(50, 100)` with `800x600`, it jumps back immediately.
14. **What happens during minimize → restore?**
    - **Broken in current build**: `handle_toplevel_request_minimize` calls `TinexusServer::trigger_minimize`, which fails. The window does not hide, and no restore path is triggered.
15. **What happens when the window is closed during an animation?**
    - **Fatal Bug**: If `close_active_window()` initiates `fade_timer` and the client process crashes or destroys its surface before opacity reaches 0, `handle_toplevel_destroy` deletes `ToplevelWrapper`. The timer is **not disarmed**, causing a crash on the next 15ms timer tick.
16. **What happens if the client crashes during an animation?**
    - Wlroots destroys the `wlr_surface`. `handle_toplevel_destroy` fires. The scene tree is destroyed by wlroots, but internal grab and timer pointers become dangling.

---

## 4. Current Resize / Maximize UI Audit (Activity Monitor Analysis)

### Trace: User Maximize → Client Display
```text
User clicks green traffic light in Activity Monitor (ChromeWidget)
      ↓
Window::set_maximized(true) called
      ↓
Client sends xdg_toplevel.set_maximized to compositor
      ↓
Compositor receives handle_toplevel_request_maximize
      ↓
WlrootsBackend::toplevel_set_maximized(wrapper, true)
  - wlr_scene_node_set_position(&wrapper->scene_tree->node, 0, 32)
  - wlr_xdg_toplevel_set_size(wrapper->toplevel, target_width, target_height)
  - wlr_xdg_surface_schedule_configure(...)
      ↓
Client receives handle_xdg_toplevel_configure(width, height)
      ↓
Window::on_configure(width, height)
  - m_width = width, m_height = height
  - m_needs_repaint = true
  - WaylandRenderTarget::resize(width, height)
  - m_root_widget->mark_needs_measure()
  - m_root_widget->mark_needs_layout()
      ↓
Window::present() executes layout & paint
      ↓
ChromeWidget::layout() runs with new bounds (e.g. 1920 x 1048)
      ↓
MonitorWidget::layout_override() runs
      ↓
[DEFECT 1]: Performance charts remain fixed height (160px).
[DEFECT 2]: Performance Power & GPU cards remain pinned at y=352px. Massive 600px void at bottom.
[DEFECT 3]: Processes tab table columns col_name..col_state remain fixed offsets from left. Massive 1100px void at right.
[DEFECT 4]: ChromeWidget paints 11px rounded rect & drop shadow around maximized window.
```

---

## 5. TXUI Lifecycle & Layout Audit

1. **Dimension Storage**: Stored as `uint32 m_width, m_height` in `Window.hpp`. Correctly updated during `on_configure()`.
2. **Resize Event Delivery**: `Window::on_configure()` pushes `EventType::WindowResize` to `m_events`.
3. **Configure Event Handling**:
   - `handle_xdg_surface_configure` acks the serial.
   - `handle_xdg_toplevel_configure` passes `(width, height)` to `on_configure()`.
   - **Crucial Defect**: `handle_xdg_toplevel_configure` completely ignores `struct wl_array* states`. TXUI clients **never learn** whether they are maximized, activated, or fullscreened from the compositor!
4. **Layout Recalculation**:
   - `Window::present()` checks `m_root_widget->needs_measure()` and `needs_layout()`.
   - Passes `Constraints::tight(m_width, m_height)` and `Rect(0, 0, m_width, m_height)`.
5. **Widget Size Propagation**:
   - `FlexLayout` correctly distributes width to children. However, leaf widgets (`MonitorWidget`, etc.) utilize hardcoded inner rect coordinates rather than dynamic flex fractions or responsive column sizing.
6. **DPI / Scaling Assumptions**:
   - Surface buffer dimensions in `WaylandRenderTarget` assume 1:1 scale. No hi-DPI buffer scaling (`wl_surface_set_buffer_scale`) is currently negotiated.
7. **Repaint & Dirty State**:
   - `m_needs_repaint` dirty tracking functions properly and preserves 0% idle CPU.
   - `m_frame_ready` throttle prevents frame queuing storms.

---

## 6. Compositor & wlroots Audit

1. **Window Internal Representation**:
   - Exclusively `ToplevelWrapper` in `src/comp/backend/wlroots_backend.cpp:394-422`.
   - Scene graph node is `struct wlr_scene_tree* scene_tree`, created under `m_scene_tree_normal`.
2. **Coordinate Spaces & Conversions**:
   - `wrapper->scene_tree->node.x/y`: **Scene coordinates** (global compositor space).
   - `wrapper->toplevel->base->current.geometry`: **Surface-local coordinates**.
   - Input hit-testing in `FocusManager::pick_surface()` converts screen cursor `(x, y)` to surface-local coordinates `(sx, sy)` via `wlr_scene_node_at()`.
3. **Multi-Output Mapping**:
   - Scene tree is attached to `m_scene->tree`.
   - `wlr_scene_output_create` attaches physical outputs to the scene.
   - When moving windows across monitors, coordinates in `wlr_scene_node` are global across the `wlr_output_layout`.
   - **Defect**: `toplevel_set_maximized` hardcodes `(0, 32)` and queries `m_outputs.front()`, assuming a single monitor at `(0,0)`.

---

## 7. Animation Infrastructure Audit

1. **Compositor Animation Infrastructure (`src/comp/animation/`)**:
   - `IAnimation`, `BaseAnimation` (time-based with curves: `EaseDecelerate`, `EaseAccelerate`, `EaseSpring`).
   - `SpringAnimation` and `SpringState` (physics-based Euler integration: `stiffness`, `damping`).
   - **Finding**: High quality mathematical foundation already exists in `src/comp/animation/animation.hpp`.
2. **Dock Animation Infrastructure (`src/dock/`)**:
   - `DockWidget` uses `txui::SpringState` for icon hover scale (1.0 → 1.25) and click bounce.
   - **Reasoning against reuse**: Dock animation runs inside an unprivileged client process manipulating pixel offsets inside its own 2D buffer. The compositor requires a dedicated, privileged `AnimationManager` that manipulates `wlr_scene_node` properties (position, scale, opacity, clip) across all Wayland clients.

---

## 8. Launch Origin Audit

1. **Current Launch Pathways**:
   - **Ctrl+K**: `server.cpp:167` forks `tinexus-launcher`. `tinexus-launcher` forks app.
   - **Dock**: `DockWidget.cpp:100` forks app.
   - **Shell TopBar**: `DesktopShellWidget.cpp:115` forks app.
2. **Compositor Awareness**:
   - When `wlr_xdg_shell->events.new_toplevel` triggers in the compositor, it has **zero context** on who launched it.
   - It cannot distinguish whether a window came from the Dock, Ctrl+K, terminal CLI, or autostart.
3. **Recommended Architecture**:
   - Standardize launch tokens or IPC expectation registration via `RuntimePaths::get_comp_fifo_path()`:
     - When Dock launches `app_id`, it sends: `expect_launch <app_id> dock <icon_x> <icon_y> <icon_w> <icon_h>`
     - When Ctrl+K launches `app_id`, it sends: `expect_launch <app_id> launcher <center_x> <center_y>`
     - When the toplevel maps within 2000ms with matching `app_id`, the compositor applies the corresponding entrance transition. If no token exists, fallback to standard center-zoom.

---

## 9. Minimize / Restore Audit

1. **Current Meaning of Minimize**:
   - Currently a **dead code path**.
   - Intended design in `MinimizeAnimation.cpp`: shrink scale to `0.1`, fade opacity to `0.0`, interpolate position towards `(dock_icon_x, dock_icon_y)`, and disable node upon completion.
2. **Dock Inter-Process Communication**:
   - Protocol already defines `DOCK_NOTIFY_MINIMIZED`, `DOCK_NOTIFY_RESTORED`, `DOCK_NOTIFY_FOCUS_CHANGED`, and `DOCK_QUERY_ICON_POSITION`.
   - The compositor never dispatches these messages because `ToplevelWrapper` is never connected to `WindowManager`.

---

## 10. Multi-Hardware & Portability Audit

1. **Display Refresh Invariants**:
   - Dynamic refresh discovery successfully implemented in Phase 4 (`120.02 Hz` dynamic detection).
   - Compositor animation system must use `dt` (elapsed time in seconds) rather than a fixed frame count (e.g. `60 ticks`), ensuring identical perceptual duration whether running at 60 Hz, 120.02 Hz, or 144 Hz.
2. **Resolution Invariants**:
   - Zero hardcoding of `1920x1080` or `1280x800` permitted.
   - Window geometry and maximize targets must query the active `wlr_output` effective resolution.
3. **Renderer Invariants**:
   - Must run identically on GLES2 hardware rendering (Intel i915 / Iris Gallium) and Pixman software fallback.
   - Scene node transformations (`wlr_scene_node_set_position`) work identically across both backends.

---

## 11. Multi-Monitor Readiness

- **Output Enumeration**: `m_outputs` stores all active displays. `wlr_output_layout_add_auto` positions them.
- **Window to Output Mapping**: **NOT SUPPORTED CURRENTLY**. Maximization queries `m_outputs.front()`.
- **Target Architecture**: Map window to output using `wlr_output_layout_output_at(layout, cursor_x, cursor_y)` or window center point. Maximization bounds must equal `output_box - usable_layer_margins` for that specific output.

---

## 12. Frame / Damage / Performance Audit

1. **Zero-Damage Frame Gating**:
   - `TinexusOutput::frame()` commits only when `WindowManager::has_active_animations() || wlr_scene_output_needs_frame()`.
   - **Crucial Rule**: While an animation is running, `AnimationManager::has_active_animations()` returns `true`, triggering `wlr_output_schedule_frame()`. As soon as all animations settle, it returns `false`, immediately dropping compositor CPU to **0.0%**.
2. **No Client Ping-Pong Loop**:
   - Window animations (scale, opacity, slide) transform the compositor scene node. They **must not** send configure events to the client on every frame! Configure is sent only when final geometry is established (e.g. at the completion of maximize/restore).

---

## 13. Failure & Edge-Case Audit

1. **Client Crash During Close / Animation**:
   - **Finding**: Dangling `fade_timer` and `m_grabbed_toplevel`.
   - **Fix Required**: In `handle_toplevel_destroy`, verify `if (wrapper->fade_timer) wl_event_source_remove(wrapper->fade_timer);` and `if (m_grabbed_toplevel == wrapper) m_grabbed_toplevel = nullptr;`.
2. **Rapid Repeated Maximize Clicks**:
   - If user clicks maximize while maximize animation is running, reverse the target animation smoothly to restore without re-querying or corrupting `saved_x/y`.
3. **Window Destruction Mid-Flight**:
   - `AnimationManager` must hold weak references or hook toplevel `destroy` signal to purge active animation handles instantly.

---

## 14. External Wayland Client Compatibility

1. **Non-TXUI Applications (GTK, Qt, Firefox, Foot, Electron)**:
   - Must not require custom IPC or headers.
   - Must obey standard `xdg-shell` protocol:
     - `xdg_toplevel.set_maximized` / `unset_maximized`
     - `xdg_toplevel.set_fullscreen` / `unset_fullscreen`
     - `xdg_toplevel.configure` with state flags
   - Compositor scene node animations work identically on standard Wayland buffers without client awareness.

---

## 15. Comprehensive Findings & Severity Register

| ID | Component | File & Line | Finding & Architectural Impact | Severity |
|---|---|---|---|---|
| **F-01** | Compositor | `wlroots_backend.cpp:875` | `handle_toplevel_request_minimize` calls `TinexusServer::trigger_minimize`, which fails because `WindowManager` contains no windows. Window minimization is completely non-functional. | **P0** |
| **F-02** | Compositor | `wlroots_backend.cpp:630, 711` | Maximize and Fullscreen fall back to hardcoded `1280x800` and `m_outputs.front()`. Broken on secondary displays and multi-monitor setups. | **P0** |
| **F-03** | Compositor | `wlroots_backend.cpp:881` | `handle_toplevel_destroy` leaks `fade_timer` and fails to clear `m_grabbed_toplevel`. Client crash during animation causes compositor segfault (Use-After-Free). | **P0** |
| **F-04** | TXUI Window | `txui/window/Window.cpp:35` | `handle_xdg_toplevel_configure` completely ignores `struct wl_array* states`. Clients never learn if they are maximized or active from the compositor. | **P1** |
| **F-05** | UI Layout | `monitor/MonitorWidget.cpp:583` | Process table columns are fixed pixel offsets up to `x+700`. Maximize leaves 1100px blank void on right. | **P1** |
| **F-06** | UI Layout | `monitor/MonitorWidget.cpp:398` | Performance tab charts (160px) and cards (y=352px) are hardcoded. Maximize leaves 600px blank void at bottom. | **P1** |
| **F-07** | UI Chrome | `txui/widgets/ChromeWidget.cpp:60` | Client-side 11px corner radius and drop shadow painted unconditionally even when window is maximized. | **P1** |
| **F-08** | Compositor | `server.cpp:481` | Hardcoded `1920/2` and `1080-36` fallback dock icon coordinates in `trigger_minimize`. | **P2** |
| **F-09** | Compositor | `wlroots_backend.cpp:779` | Window placement uses static cascade offsets `(50, 100) + i*30` with zero awareness of launch origin. | **P2** |
| **F-10** | Dock IPC | `dock/main.cpp:97` | Dock icon state machine has full logic for `DOCK_NOTIFY_MINIMIZED/RESTORED`, but compositor never emits notifications. | **P2** |

---

## 16. Existing vs. Missing Test Coverage

### Existing Tests:
- `tests/unit/test_comp.cpp`: Tests mock `WindowManager` and `WorkspaceManager` in isolation.
- `tests/unit/test_txui_gate1_resize.cpp`: Headless 500-cycle configure test on `Window`.
- `tests/integration/test_wayland_server.cpp`: Tests `SurfaceManager` mock state machine.

### Missing Test Coverage:
1. Compositor toplevel maximize/restore geometry unit test against real `wlr_output`.
2. Multi-monitor output targeting test for window maximization.
3. Compositor animation clock & physics step test (`dt`-based time stepping).
4. Destroy listener safety test during active fade/minimize transitions.
5. Responsive table column calculation unit test for `MonitorWidget`.
6. XDG toplevel state array parsing test in TXUI.

---

## 17. Recommended Architecture

```text
┌────────────────────────────────────────────────────────────────────────┐
│                        TINEXUS COMPOSITOR                             │
│                                                                        │
│   ┌────────────────────────┐         ┌──────────────────────────────┐  │
│   │ WlrootsBackend         │         │ AnimationManager (Privileged)│  │
│   │ - wlr_xdg_shell        │◄───────►│ - Spring physics (dt-driven) │  │
│   │ - wlr_scene_tree       │         │ - Easing curves              │  │
│   │ - ToplevelController   │         │ - Scene node transforms:     │  │
│   │   (Unified State)      │         │   * Position, Scale, Opacity │  │
│   └──────────┬─────────────┘         └──────────────┬───────────────┘  │
│              │                                      │                  │
│              ▼                                      ▼                  │
│   ┌────────────────────────┐         ┌──────────────────────────────┐  │
│   │ Output-Aware Bounds    │         │ TinexusOutput Frame Loop     │  │
│   │ - Active monitor probe │         │ - has_active_animations()    │  │
│   │ - TopBar/Dock margins  │         │ - Zero-damage idle sleep     │  │
│   └────────────────────────┘         └──────────────────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Wayland xdg-shell + Configure
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        TXUI CLIENT LAYER                               │
│                                                                        │
│   ┌────────────────────────┐         ┌──────────────────────────────┐  │
│   │ Window Runtime         │         │ Responsive Widget Tree       │  │
│   │ - Parse state array    │◄───────►│ - ChromeWidget (0 radius on  │  │
│   │   (Maximized, Active)  │         │   maximize; no shadow)       │  │
│   │ - Ack configure        │         │ - MonitorWidget: Dynamic     │  │
│   │ - Damage presentation  │         │   flex columns & card layout │  │
│   └────────────────────────┘         └──────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 18. Stop Condition Verification

- Working branch: `feature/window-management-animation-v1`
- Working tree: Clean
- Zero production code modified during this audit.
- Full findings and implementation plan presented for explicit user review.
