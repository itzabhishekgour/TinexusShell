# ADR 0004: Three-Phase Declarative Layout Pipeline

- **Status**: Accepted
- **Date**: 2026-07-28
- **Applies to**: `libtxui::layout` (`Constraints`, `LayoutEngine`) and `libtxui::widgets` (`Widget`)

## Context
Desktop toolkits often suffer from layout thrashing when widgets resize themselves during painting or when layout equations contain cyclic dependencies.

## Decision
We adopt a **Three-Phase Unidirectional Layout Pipeline** inspired by Flutter and browser layout engines:
1. **Constraints Pass (`Constraints`)**: Parent container passes min/max width and height constraints down to child widgets.
2. **Measure Pass (`measure(const Constraints&)`)**: Child computes its desired size within the given constraints and returns a `Size` to the parent.
3. **Layout Pass (`layout(const Rect&)`)**: Parent assigns a definitive screen bounding `Rect` to each child.
4. **Paint Pass (`paint(PaintEvent&)`)**: Widgets paint exclusively within their assigned layout `Rect`.

## Rationale
1. **O(N) Linear Layout Complexity**: A strict top-down constraint / bottom-up size negotiation guarantees layout resolution in a single pass without iterative equation solvers.
2. **Deterministic Resizing**: Widgets cannot mutate their layout dimensions during the paint phase.

## Consequences
- Custom layout containers must implement both `measure` and `layout` overrides.
