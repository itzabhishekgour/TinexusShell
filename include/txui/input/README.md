# txui::input — Event Routing, Hit-Testing & Focus Management

> **Module**: `txui::input`  
> **Responsibility**: Pointer motion hit-testing, event bubbling, keyboard focus tree, and shortcut accelerators.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`.

## Forbidden Dependencies
- `txui::theme`, `txui::animation`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `Event` / `PointerEvent` / `KeyEvent`: Input event structures.
- `HitTester`: Scene node picking algorithm (`wlr_cursor` coordinate mapping).
- `FocusManager`: Active keyboard and pointer focus tracking.
