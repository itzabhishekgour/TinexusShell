# ADR 0004: Three-Phase Declarative Layout Pipeline

- **Status**: Accepted
- **Date**: 2026-07-29
- **Applies to**: `libtxui::layout` (`Constraints`, `LayoutEngine`) and `libtxui::widgets` (`Widget`)

## Context
Desktop toolkits often suffer from layout thrashing when widgets resize themselves during painting or when layout equations contain cyclic dependencies. Furthermore, mixing input routing with layout math creates untestable, coupled spaghetti code.

## Decision
We adopt a strict **Three-Phase Unidirectional Layout Pipeline** inspired by Flutter and browser layout engines:

1. **Measure Pass (`measure(const Constraints&)`)**:
   - Parent passes min/max constraints down to children.
   - Child computes its desired size.
   - Computations MUST be purely mathematical.
   - Outputs exclusively to `Size m_desired_size`.
2. **Layout Pass (`layout(const Rect&)`)**:
   - Parent assigns a definitive screen bounding `Rect` to each child based on the child's `m_desired_size` and the parent's alignment rules.
   - Outputs exclusively to `Rect m_frame`.
3. **Paint Pass (`paint(Painter&) const`)**:
   - Widgets paint exclusively within their assigned layout `m_frame`.
   - The widget must be immutable with respect to rendering state. Rendering is pull-based. `Widget` does NOT own `Painter`, `Brush`, `Canvas`, or `RenderTarget` internally.

## Strict Rules & Acceptance Criteria
1. **No Input Awareness**: The layout engine (libtxui 0.2) must NOT know about mouse, keyboard, focus, dragging, hover, or gestures. Event routing and hit testing are exclusively reserved for Phase 4.4 (libtxui 0.3).
2. **Constraints Determinism**: The `Constraints` solver is mathematically deterministic. Includes `is_tight()` and `is_bounded()` helpers.
3. **No Geometry Mutation in Measure**: `measure()` never mutates widget frame geometry.
4. **No State Mutation in Paint**: `paint()` never changes layout state or sizes. It is `const`.
5. **Acyclic Tree Ownership**: Parents own children via `std::vector<Ref<Widget>>`. Children reference parents via raw `Widget* m_parent` to prevent reference cycles.
6. **Dirty Flags**: Every widget has `m_needs_measure`, `m_needs_layout`, and `m_needs_paint` flags. Layout computations only process dirty subtrees.
7. **Window Integration**: Resizing triggers layout recomputation via `Window` checking dirty flags, NOT by eagerly calling `measure/layout/paint` continuously.

## Consequences
- Custom layout containers must respect `Constraints` strictly.
- Missing `mark_needs_layout()` calls will result in stale UI rendering.
- Test suites can verify layout trees entirely headlessly by inspecting `m_frame` and `m_desired_size`.
