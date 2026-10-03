# Tinexus Platform: Window Management & Animation Subsystem Implementation Plan (v2.0)

**Document Status**: REVISED ARCHITECTURE SPECIFICATION — PENDING EXPLICIT APPROVAL  
**Target Branch**: `feature/window-management-animation-v1`  
**Hard Invariants**: 30/30 Architectural Rules Satisfied  
**Author**: Principal Systems Engineer  

---

## 1. Overview & Architectural Directives

This document specifies the complete architectural overhaul of the Tinexus window management, geometry, snapping, and animation subsystems. It resolves the disconnection between `WlrootsBackend` and window state management, establishes an output-aware work-area abstraction, implements a Windows-like edge/corner snapping engine, strengthens the window state model, integrates complete `xdg_toplevel` state parsing in TXUI, creates a privileged scene-graph `AnimationManager` with deterministic lifetime safety, and purges all machine-specific assumptions across the monorepo.

### Non-Negotiable Invariants:
1. **Zero Hardcoded Constants**: No hardcoded resolutions (`1920x1080`, `1280x800`), refresh rates (`60`, `120`, `120.02`, `144`), DRM card nodes (`card0`, `renderD128`), connector names (`eDP-1`, `HDMI-A-1`), fixed panel sizes, or fixed dock positions.
2. **Decoupled Compositor Scene Animations**: Compositor visual animations transform `wlr_scene_node` directly (position, opacity, clip, scale) without triggering client buffer reallocation or configure loops.
3. **Zero-Damage Idle CPU Guarantee**: Frames are scheduled ONLY when compositor animations are active or when client surfaces send damage. When static, CPU must immediately return to **0.0%**. No permanent frame timers.
4. **Wall-Clock dt Physics**: Animation timing is strictly driven by elapsed monotonic wall-clock delta (`dt`). Frame-count animations are prohibited. Visual duration is identical across 60 Hz, 120 Hz, 144 Hz, and variable refresh rate displays.
5. **Standard Wayland Client Compatibility**: External GTK4, Qt6, Electron, and Firefox clients must animate, snap, maximize, and restore seamlessly via standard `xdg-shell` without Tinexus-specific protocols or headers.
6. **Unified Usable Work-Area**: Maximize, fullscreen, snapping, initial cascading, and minimize targets all consume a single, dynamically derived `OutputWorkArea` abstraction.

---

## 2. Single Output Work-Area Abstraction (`OutputWorkArea`)

All window geometry operations must consume a single source of truth for display space. We introduce `OutputWorkArea` to eliminate duplicate geometry calculations across `WlrootsBackend`, `WindowManager`, and layout systems.

### 2.1 Class Structure & Definition (`src/comp/output/include/comp/output/output_work_area.hpp`)
```cpp
namespace tinexus::comp {

struct LayerExclusiveZones {
    int32_t top{0};
    int32_t bottom{0};
    int32_t left{0};
    int32_t right{0};
};

struct OutputWorkArea {
    struct wlr_output* output{nullptr};
    std::string name;
    
    // Global layout box (in compositor global coordinates via wlr_output_layout)
    struct wlr_box global_box{0, 0, 0, 0};
    
    // Effective display resolution (accounting for scale)
    int32_t effective_width{0};
    int32_t effective_height{0};
    float scale{1.0f};
    double refresh_hz{60.0}; // Derived from mode refresh mHz / 1000.0
    
    // Cumulative layer-shell exclusive margins
    LayerExclusiveZones exclusive;
    
    // Usable Work Area (global compositor coordinates)
    // usable_box = global_box inset by layer-shell exclusive margins
    struct wlr_box usable_box{0, 0, 0, 0};

    // Helper queries
    [[nodiscard]] bool contains_point(double gx, double gy) const noexcept {
        return gx >= global_box.x && gx < (global_box.x + global_box.width) &&
               gy >= global_box.y && gy < (global_box.y + global_box.height);
    }
    
    [[nodiscard]] bool contains_usable_point(double gx, double gy) const noexcept {
        return gx >= usable_box.x && gx < (usable_box.x + usable_box.width) &&
               gy >= usable_box.y && gy < (usable_box.y + usable_box.height);
    }
};

} // namespace tinexus::comp
```

### 2.2 Dynamic Derivation Algorithm
1. **Global Position**: Query `wlr_output_layout_get_box(layout, output, &global_box)`.
2. **Effective Resolution**: Query `wlr_output_effective_resolution(output, &effective_width, &effective_height)`.
3. **Dynamic Refresh Rate**: Query `output->current_mode ? output->current_mode->refresh : output->refresh`. Convert `mHz` to `Hz`.
4. **Layer-Shell Inset Pass**:
   - Initialize `usable_box = global_box`.
   - Iterate over all active `wlr_layer_surface_v1` instances assigned to this output in standard z-order (Background → Bottom → Top → Overlay).
   - Compute exclusive zones:
     - Top anchors with exclusive zone: `usable_box.y += zone; usable_box.height -= zone; exclusive.top += zone;`
     - Bottom anchors: `usable_box.height -= zone; exclusive.bottom += zone;`
     - Left anchors: `usable_box.x += zone; usable_box.width -= zone; exclusive.left += zone;`
     - Right anchors: `usable_box.width -= zone; exclusive.right += zone;`
5. **Consumption**: Maximize, fullscreen, snapping, cascade positioning, and animations query `OutputWorkArea::get(output)` exclusively.

---

## 3. Strengthened Window State Model

`WindowState` must not be overloaded with geometry partition concepts. Geometry tracking and protocol state are strictly decoupled.

### 3.1 State Enumerations & Geometry Container
```cpp
namespace tinexus::comp {

enum class WindowState : uint8_t {
    Normal,     // Floating window
    Maximized,  // Fills output usable_box
    Minimized,  // Unmapped/hidden from scene, dock indicator active
    Fullscreen, // Fills output global_box (covers exclusive zones)
    Closing     // Transitioning to destruction
};

enum class SnapMode : uint8_t {
    None,
    LeftHalf,
    RightHalf,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct WindowGeometryModel {
    struct wlr_box normal_geom{0, 0, 800, 600}; // Pre-snap / pre-maximize floating geometry
    struct wlr_box snap_geom{0, 0, 0, 0};        // Active snap geometry
    struct wlr_box current_geom{0, 0, 800, 600}; // Current configured geometry
    struct wlr_output* assigned_output{nullptr}; // Output currently owning this window
};

} // namespace tinexus::comp
```

### 3.2 State Transition Matrix & Restoration Invariants
| Current State | Event | Target State | SnapMode | Geometry Applied | Saved Geometry Update |
|---|---|---|---|---|---|
| `Normal` | Snap to Edge | `Normal` | `LeftHalf` (etc.) | `snap_geom = calculate_snap(...)` | `normal_geom` preserved untouched |
| `Normal (Snapped)` | Drag away from edge | `Normal` | `None` | Restored to `normal_geom` | `snap_geom` cleared |
| `Normal (Snapped)` | User Maximizes | `Maximized` | Preserved (`LeftHalf`) | `usable_box` | `snap_geom` preserved |
| `Maximized` | User Un-maximizes | `Normal` | `LeftHalf` | Restored to `snap_geom` | — |
| `Normal (Floating)` | User Maximizes | `Maximized` | `None` | `usable_box` | `normal_geom` preserved |
| `Maximized` | User Un-maximizes | `Normal` | `None` | Restored to `normal_geom` | — |
| Any | Minimize | `Minimized` | Preserved | Hidden from scene | Active geometry preserved |
| `Minimized` | Restore | Prior state | Prior snap | Restored to active geometry | — |

**Core Invariant**: `Normal → Snap → Normal` restores the EXACT pre-snap floating position and dimensions. `Normal → Snap → Maximize → Restore` restores the snap geometry, and dragging away restores the normal floating geometry.

---

## 4. Windows-Like Edge & Corner Snapping Subsystem

### 4.1 Trigger Zones & Thresholds
During interactive window movement (`CursorMode::Move`), cursor coordinates `(cx, cy)` are evaluated against the `OutputWorkArea` containing the cursor:
- `EDGE_THRESHOLD_PX = 16`
- `CORNER_THRESHOLD_PX = 48`

```text
┌────────────────────────────────────────────────────────┐
│ [TopLeft]          [Top Edge: Maximize]    [TopRight]  │
├─────────┬────────────────────────────────────┬─────────┤
│         │                                    │         │
│ [Left   │                                    │ [Right  │
│  Half]  │             (Floating)             │  Half]  │
│         │                                    │         │
├─────────┴────────────────────────────────────┴─────────┤
│ [BottomLeft]                              [BottomRight]│
└────────────────────────────────────────────────────────┘
```

### 4.2 Mathematical Partitioning of `usable_box`
Given active output `usable_box` `{ux, uy, uw, uh}`:
- **LeftHalf**:
  - `x = ux`, `y = uy`, `w = uw / 2`, `h = uh`
- **RightHalf**:
  - `x = ux + (uw / 2)`, `y = uy`, `w = uw - (uw / 2)`, `h = uh`
- **TopLeft**:
  - `x = ux`, `y = uy`, `w = uw / 2`, `h = uh / 2`
- **TopRight**:
  - `x = ux + (uw / 2)`, `y = uy`, `w = uw - (uw / 2)`, `h = uh / 2`
- **BottomLeft**:
  - `x = ux`, `y = uy + (uh / 2)`, `w = uw / 2`, `h = uh - (uh / 2)`
- **BottomRight**:
  - `x = ux + (uw / 2)`, `y = uy + (uh / 2)`, `w = uw - (uw / 2)`, `h = uh - (uh / 2)`
- **Top Edge (Maximize)**:
  - `x = ux`, `y = uy`, `w = uw`, `h = uh` (Transitions `WindowState` to `Maximized`)

*Note*: Integer division with `uw - (uw / 2)` guarantees zero pixel gap or single-pixel overflow on odd display resolutions (e.g. 1366x768).

### 4.3 Snapping Workflow & Visual Feedback
1. **Drag Tracking**: As the window is dragged, if `(cx, cy)` enters a snap trigger zone, a lightweight compositor snap preview node (`wlr_scene_rect`) renders a translucent blue accent outline over the prospective snap bounds.
2. **Release Execution**: On mouse button release (`WL_POINTER_BUTTON_STATE_RELEASED`):
   - If in a snap zone:
     - Record `normal_geom` if `snap_mode == None`.
     - Assign `snap_mode`.
     - Animate or configure toplevel to target snap bounds.
     - Send `wlr_xdg_toplevel_set_size` and schedule configure.
   - Hide preview node.
3. **Un-snapping on Drag**: When a snapped window is grabbed by its title bar:
   - On cursor motion exceeding 8px:
     - `snap_mode = SnapMode::None`.
     - Restore window dimensions to `normal_geom.width, normal_geom.height`.
     - Re-center or align grab point proportionally along the restored title bar:
       `new_x = cx - (normal_geom.width * (grab_offset_x / snap_geom.width))`.
     - Continue standard move tracking.

---

## 5. Compositor-Level AnimationManager & Physics Clock

### 5.1 Architecture & Decoupling
Dock's client-side animation system (`txui::SpringState`) is an unprivileged software rendering calculation. Window transitions require a privileged compositor `AnimationManager`.

`AnimationManager` transforms `wlr_scene_node` directly:
- **Center Zoom on Map**: Node scale `0.85 → 1.0`, opacity `0.0 → 1.0`.
- **Minimize to Dock**: Node scales down and translates toward the registered Dock icon coordinate `(dock_x, dock_y)`. Upon completion, node is disabled (`wlr_scene_node_set_enabled(&node, false)`).
- **Restore from Minimized**: Node enabled, expands and translates from `(dock_x, dock_y)` to `current_geom`.
- **Close / Fade Out**: Node opacity `1.0 → 0.0` over 120ms, followed by immediate destruction.
- **Maximize / Snap Transition**: Scene node translates and clips smoothly while client receives standard `xdg-shell` configure event.

### 5.2 Dynamic Monotonic dt Physics Clock
- Animations are evaluated using wall-clock elapsed time delta:
  ```cpp
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  double dt = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
  ```
- Second-order Critically Damped Spring / Semi-Implicit Euler integration:
  ```cpp
  // F = -k * (x - target) - c * v
  double force = -stiffness * (current_val - target_val) - damping * velocity;
  velocity += force * dt;
  current_val += velocity * dt;
  ```
- **Zero Frame-Count Reliance**: Behavior is identical at 60 Hz, 120.02 Hz, 144 Hz, or irregular VSYNC timings.

### 5.3 Frame Scheduling & Zero-Damage Idle
- In `TinexusOutput::frame()`:
  ```cpp
  bool has_anims = AnimationManager::instance().has_active_animations();
  if (!has_anims && !wlr_scene_output_needs_frame(scene_output)) {
      return; // Sleep! Zero CPU usage!
  }
  if (has_anims) {
      AnimationManager::instance().tick(dt);
  }
  // Render frame
  wlr_scene_output_commit(scene_output, nullptr);
  wlr_scene_output_send_frame_done(scene_output, &now);
  
  // Re-schedule ONLY if animation is still running
  if (AnimationManager::instance().has_active_animations()) {
      wlr_output_schedule_frame(m_output);
  }
  ```
- Once all springs settle (`std::abs(velocity) < EPSILON && std::abs(val - target) < EPSILON`), animations are marked inactive, no frame is scheduled, and the compositor sleeps at **0.0% CPU**.

---

## 6. Full Inventory of Machine-Specific Assumptions & Removal Strategy

A comprehensive monorepo audit revealed hardcoded constants that must be purged and replaced with dynamic queries:

| File | Line | Current Hardcoded Assumption | Mandatory Dynamic Replacement |
|---|---|---|---|
| `src/comp/backend/wlroots_backend.cpp` | 630, 711 | `struct wlr_box output_box = {0, 0, 1280, 800};` | `OutputWorkArea::get(assigned_output).usable_box` |
| `src/comp/backend/wlroots_backend.cpp` | 638, 640 | `target_height = output_box.height - 32;` (Hardcoded 32px top bar) | Use `OutputWorkArea::exclusive.top` dynamically |
| `src/comp/surface/include/comp/surface/exclusive_zone_calculator.hpp` | 39-47 | `width{1920}; height{1080};` | Initialize to `0, 0` and require explicit bounds |
| `src/comp/output/include/comp/output/output_layout.hpp` | 16-17 | `width{1920}; height{1080}; refresh_hz{60};` | Derive from `wlr_output_mode` |
| `src/comp/output/include/comp/output/output_manager.hpp` | 12-13 | `int width{1920}; int height{1080};` | Remove defaults, populate from DRM/wlroots mode |
| `src/comp/output/output.cpp` | 109, 122 | `m_output->name ? m_output->name : "eDP-1"` | Use `m_output->name ? m_output->name : "unknown"` |
| `src/comp/server/server.cpp` | 481-482 | `win->dock_icon_x = 1920 / 2; win->dock_icon_y = 1080 - 36;` | Query active output width and dock layer surface position |
| `src/txui/window/Window.cpp` | 179, 485 | `(1920 - static_cast<int32_t>(width)) / 2;` | Use actual configured output width from configure event |
| `src/common/DisplayUtils.cpp` | 79-80 | `connector_name = "eDP-1"; resolution = "1920x1080";` | Query sysfs DRM modes dynamically; return error if absent |
| `src/settings/include/settings/config_store.hpp` | 23 | `resolution{"1920x1080@144Hz"};` | Query active compositor output mode |
| `src/comp/backend/include/comp/backend/drm_backend.hpp` | 38 | `device_path = "/dev/dri/card0"` | Discover primary GPU node via `GraphicsProbe` |
| `src/comp/surface/surface_manager.cpp` | 13 | `SurfaceRecord{..., "HDMI-A-1", ...}` | Use active `output->name` |

---

## 7. Multi-Monitor Topology & Output-Aware Routing

### 7.1 Dynamic Output Resolution
Selection of the active output must never rely on `m_outputs.front()`. Instead:
1. **Interactive Operations (Drag/Snap/Click)**:
   ```cpp
   struct wlr_output* target_output = wlr_output_layout_output_at(m_output_layout, m_cursor->x, m_cursor->y);
   ```
2. **Window State Changes (Maximize/Restore/Configure)**:
   ```cpp
   // Calculate window center point in global compositor space
   int cx = wrapper->geometry.x + wrapper->geometry.width / 2;
   int cy = wrapper->geometry.y + wrapper->geometry.height / 2;
   struct wlr_output* target_output = wlr_output_layout_output_at(m_output_layout, cx, cy);
   if (!target_output && !m_outputs.empty()) {
       target_output = wlr_output_layout_get_center_output(m_output_layout);
   }
   ```
3. **Output Migration Tracking**:
   When a window is dragged across output boundaries, `wrapper->geom_model.assigned_output` updates to the new output. Maximize or snap commands immediately consume the work area of the new output.

### 7.2 Multi-Monitor Verification Topologies
The test suite and implementation must support and validate:
- Primary laptop display + external monitor to the right (`x = 1920, y = 0`).
- External monitor positioned above (`x = 0, y = -1080`).
- Mixed resolutions (e.g. 1920x1080 primary @ 120 Hz, 2560x1440 secondary @ 60 Hz).
- Dynamic hotplug: Adding or removing an output shifts windows gracefully onto the remaining outputs without segfaults.

---

## 8. TXUI Comprehensive Wayland State Synchronization

TXUI clients must not parse only `MAXIMIZED`. They must handle the complete suite of `xdg_toplevel` configure states.

### 8.1 State Handling in `src/txui/window/Window.cpp`
```cpp
void handle_xdg_toplevel_configure(void* data, struct xdg_toplevel* /*toplevel*/, 
                                   int32_t width, int32_t height, struct wl_array* states) {
    auto* win = static_cast<Window*>(data);
    
    WaylandWindowState ws;
    uint32_t* state;
    wl_array_for_each(state, states) {
        switch (*state) {
            case XDG_TOPLEVEL_STATE_MAXIMIZED:    ws.is_maximized = true; break;
            case XDG_TOPLEVEL_STATE_FULLSCREEN:   ws.is_fullscreen = true; break;
            case XDG_TOPLEVEL_STATE_RESIZING:     ws.is_resizing = true; break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:    ws.is_activated = true; break;
            case XDG_TOPLEVEL_STATE_TILED_LEFT:   ws.is_tiled_left = true; break;
            case XDG_TOPLEVEL_STATE_TILED_RIGHT:  ws.is_tiled_right = true; break;
            case XDG_TOPLEVEL_STATE_TILED_TOP:    ws.is_tiled_top = true; break;
            case XDG_TOPLEVEL_STATE_TILED_BOTTOM: ws.is_tiled_bottom = true; break;
            default: break;
        }
    }
    
    win->update_wayland_states(ws);
    
    if (width > 0 && height > 0) {
        win->on_configure(static_cast<uint32>(width), static_cast<uint32>(height));
    }
}
```

### 8.2 Widget Responsive Adaptations
- **`ChromeWidget`**:
  - `is_maximized`: Border radius `0.0`, drop shadow disabled.
  - `is_tiled_*`: Adjacent snapped edges render with `0.0` radius and no shadow, while un-tiled edges retain soft styling.
  - `is_activated`: High-contrast active titlebar accent; deactivated windows dim subtly.
  - `is_fullscreen`: Titlebar and frame chrome completely hidden.
- **`MonitorWidget`**:
  - Process table columns dynamically scaled via flex weights:
    `col_w = table_width * weight[i]`.
  - Chart heights and card vertical positioning dynamically expand to consume `area.height()` without fixed voids.

---

## 9. Animation Lifetime Safety & Destruction Sequences

To prevent use-after-free crashes when a client terminates mid-animation or during interactive grab:

### 9.1 Deterministic Teardown Sequence (`handle_toplevel_destroy`)
1. **Disarm Timers**: Disarm and free all wl_event_source timers linked to the wrapper.
2. **Release Grabs**: If `m_grabbed_toplevel == wrapper`, reset `m_grabbed_toplevel = nullptr; m_cursor_mode = CursorMode::Passthrough;`.
3. **Cancel Active Animations**:
   ```cpp
   AnimationManager::instance().cancel_animations_for(wrapper);
   ```
   This immediately removes any pending or active animation objects referencing `wrapper` or `wrapper->scene_tree`, rendering dangling pointer dereferences impossible.
4. **Remove from Compositor Registries**:
   - Erase from `m_toplevels`.
   - Clear from `FocusManager` keyboard/pointer focus pointers.
5. **Destroy Scene Node & Wrapper**:
   - `wlr_scene_node_destroy(&wrapper->scene_tree->node);`
   - `delete wrapper;`

---

## 10. External Wayland Client Compatibility

The window management and animation architecture strictly maintains compatibility with standard Linux Wayland applications:
- **Protocol Adherence**: Compositor communicates with windows exclusively via standard `xdg-shell` (v1-v6). Snapping and maximize communicate standard `set_size` and `set_maximized` configure events.
- **Client Agnostic**: GTK4, Qt6, Electron, and native Wayland apps require zero modification. They configure their buffers according to standard Wayland event contracts.
- **Visual Decoupling**: Compositor-level animations operate on the Wayland subsurface / scene-tree container. When a GTK app takes 50ms to allocate and commit a new buffer upon maximize, the compositor scene animation smoothly interpolates the viewport geometry without visual artifacts.

---

## 11. Phased Implementation Roadmap

```text
Phase 1: OutputWorkArea Abstraction & Machine-Specific Constant Purge
   │
   ▼
Phase 2: Strengthened State Machine & Snapping Engine
   │
   ▼
Phase 3: Animation Lifetime Safety & Compositor-Level AnimationManager
   │
   ▼
Phase 4: TXUI Complete Wayland State Integration & Responsive Polish
   │
   ▼
Phase 5: Multi-Monitor Geometry Routing & Output Hotplug
   │
   ▼
Phase 6: Launch Origins & Dock IPC Integration
   │
   ▼
Phase 7: Automated Test Suites, Regression Gate, and Physical Hardware Validation
```

### Phase 1: OutputWorkArea Abstraction & Constant Purge
- Create `src/comp/output/include/comp/output/output_work_area.hpp` and `src/comp/output/output_work_area.cpp`.
- Integrate progressive layer-shell exclusive zone calculation into `OutputWorkArea`.
- Purge hardcoded `1920x1080`, `1280x800`, `card0`, and `eDP-1` defaults from `WlrootsBackend`, `ExclusiveZoneCalculator`, `OutputLayout`, and `DisplayUtils`.

### Phase 2: Strengthened State Machine & Snapping Engine
- Implement `WindowState`, `SnapMode`, and `WindowGeometryModel` in `WlrootsBackend`.
- Implement edge/corner hit testing in `process_cursor_motion` and snap execution on mouse button release.
- Implement snap preview outline rendering via `wlr_scene_rect`.
- Ensure pre-snap `normal_geom` is preserved and restored without pixel drift.

### Phase 3: Animation Lifetime Safety & Compositor-Level AnimationManager
- Create `src/comp/animation/include/comp/animation/animation_manager.hpp` and `.cpp`.
- Implement dt-driven spring interpolation (`map`, `minimize`, `restore`, `close`, `snap`).
- Connect `AnimationManager::has_active_animations()` to `TinexusOutput::frame()` to guarantee zero-damage sleep (0.0% CPU).
- Implement deterministic `cancel_animations_for(wrapper)` in `handle_toplevel_destroy`.

### Phase 4: TXUI Complete Wayland State Integration & Responsive Polish
- Update `handle_xdg_toplevel_configure` in `src/txui/window/Window.cpp` to parse all `xdg_toplevel_state` flags.
- Update `ChromeWidget` to disable corner radius and shadows when maximized or on snapped tiled edges.
- Update `MonitorWidget` to calculate column widths and chart heights dynamically.

### Phase 5: Multi-Monitor Geometry Routing & Output Hotplug
- Replace all instances of `m_outputs.front()` with `wlr_output_layout_output_at`.
- Ensure windows retain their assigned output when maximized or snapped.

### Phase 6: Launch Origins & Dock IPC Integration
- Add `expect_launch <app_id> <origin> <x> <y> <w> <h>` to FIFO server.
- Connect launch origin tokens to center-zoom vs dock-icon expansion animations.
- Send `DOCK_NOTIFY_MINIMIZED` and `DOCK_NOTIFY_RESTORED` IPC messages to Dock upon state changes.

### Phase 7: Automated Test Suites & Physical Validation
- Implement unit tests covering state transitions, snapping partitions, responsive layouts, destruction during animation, and dt stepping.
- Build live ISO and validate on physical target hardware (ASUS TUF Gaming F15).

---

## 12. Detailed Inventory of Files to Modify & New Files

### New Files:
1. `src/comp/output/include/comp/output/output_work_area.hpp`: OutputWorkArea definition.
2. `src/comp/output/output_work_area.cpp`: Usable work area and exclusive zone derivation logic.
3. `src/comp/animation/include/comp/animation/animation_manager.hpp`: Scene-graph animation engine interface.
4. `src/comp/animation/animation_manager.cpp`: Spring physics, dt stepping, and scene node transformations.
5. `tests/unit/test_window_state_machine.cpp`: State machine and snapping unit tests.
6. `tests/unit/test_output_work_area.cpp`: Multi-monitor and exclusive zone calculation tests.
7. `tests/unit/test_destruction_during_animation.cpp`: Lifetime safety and crash prevention tests.

### Files to Modify:
1. [`src/comp/backend/wlroots_backend.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/comp/backend/wlroots_backend.cpp):
   - Replace ad-hoc flags with `WindowState`, `SnapMode`, and `WindowGeometryModel`.
   - Implement snapping hit-detection and execution in cursor handlers.
   - Replace hardcoded `1280x800` and `32px` with `OutputWorkArea`.
   - Implement safe teardown in `handle_toplevel_destroy`.
2. [`src/comp/output/output.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/comp/output/output.cpp):
   - Query `AnimationManager::has_active_animations()` in `TinexusOutput::frame()`.
3. [`src/txui/window/Window.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/txui/window/Window.cpp):
   - Parse all `xdg_toplevel` configure states.
   - Eliminate hardcoded `1920` fallback output width.
4. [`src/txui/widgets/ChromeWidget.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/txui/widgets/ChromeWidget.cpp):
   - Adjust corner radius and drop shadow for maximized and tiled states.
5. [`src/monitor/MonitorWidget.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/monitor/MonitorWidget.cpp):
   - Replace fixed column offsets and chart heights with responsive flex calculations.
6. [`src/comp/server/server.cpp`](file:///e:/Tinu's%20Technology/Tinexus%20Manager/src/comp/server/server.cpp):
   - Remove hardcoded `1920 / 2` and `1080 - 36` dock coordinates; query `OutputWorkArea`.

---

## 13. Test Deliverables & Engineering Verification Gates

1. **`test_window_state_machine`**:
   - Verify `Normal → Snap(LeftHalf) → Normal` restores exact pre-snap floating geometry.
   - Verify `Normal → Snap(LeftHalf) → Maximize → Restore` restores `LeftHalf` snap geometry.
   - Verify un-snapping via drag restores floating dimensions and calculates smooth titlebar grab offset.
2. **`test_output_work_area`**:
   - Verify usable area calculation with multiple layer surfaces (Top Bar 32px, Dock 64px).
   - Verify side-by-side and stacked multi-monitor layouts produce correct global coordinates.
   - Verify odd-width resolution partitioning (e.g. 1366 / 2) has zero pixel overlap and zero gaps.
3. **`test_destruction_during_animation`**:
   - Simulate client destruction while `map`, `fade_out`, `minimize`, and `snap` animations are running.
   - Assert `AnimationManager` contains zero active handles and zero memory leaks.
4. **`test_animation_physics_dt`**:
   - Assert spring settling time is identical within 1% error margin across simulated 60 Hz (16.6ms), 120.02 Hz (8.33ms), and 144 Hz (6.94ms) frame steps.
5. **Physical Hardware Validation Gate**:
   - 30-minute idle stability test on ASUS TUF Gaming F15: verify compositor remains at **<1.5% CPU** (targeting 0.0% when static).
   - Activity Monitor responsive layout verification: zero blank voids when maximized or snapped.

---

## 14. Portability & Architectural Risk Assessment

| Risk Area | Likelihood | Impact | Architectural Mitigation |
|---|---|---|---|
| **Multi-Monitor Coordinate Drift** | Low | High | All calculations strictly consume global coordinates via `wlr_output_layout_get_box()`. |
| **Idle CPU / Frame Loop Regression** | Low | High | `AnimationManager` never registers recurring event loop timers; frame scheduling is strictly gated by `has_active_animations()` in `TinexusOutput::frame()`. |
| **External Client Protocol Desync** | Low | Medium | Standard `xdg-shell` configure serials and configure bounds are strictly followed; visual animations interpolate the scene node independently. |
| **Odd-Pixel Seam / Gap Artifacts** | Low | Low | Snapping partition math uses `w = total_w - (total_w / 2)` for complementary halves/quarters. |

---

## 15. Rollback Strategy

Each phase maintains an isolated git commit checkpoint:
- If `AnimationManager` introduces instability on Pixman fallback, scene node transforms can be bypassed with instantaneous geometry updates via `git revert`.
- If TXUI flex layout exhibits calculation regressions on small windows, `MonitorWidget` can fall back to min-constrained layout clamps without affecting compositor stability.

---

## 16. Hard Approval Gate (STOP Condition)

> **MANDATORY STOP**: This revised implementation plan is complete and awaits explicit user approval.  
> **NO PRODUCTION CODE OR REPOSITORIES HAVE BEEN MODIFIED.**  
> Implementation will begin only upon explicit authorization.
