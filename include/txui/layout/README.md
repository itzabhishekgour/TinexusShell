# txui::layout — Three-Phase Unidirectional Constraint Solver

> **Module**: `txui::layout`  
> **Responsibility**: Constraint propagation, sizing calculation, and layout rectangle assignment.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`.

## Forbidden Dependencies
- `txui::theme`, `txui::animation`, `txui::input`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `Constraints`: Min/max width and height bounding box passed from parent to child.
- `LayoutEngine`: Solver executing `measure(Constraints)` and `layout(Rect)`.
- `FlexLayout` / `GridLayout`: Declarative flexbox and grid positioning algorithms.
