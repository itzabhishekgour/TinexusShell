# txui::widgets — Declarative UI Nodes & Controls

> **Module**: `txui::widgets`  
> **Responsibility**: Widget base class, layout containers, and core interactive controls.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`, `txui::input`, `txui::animation`, `txui::theme`.

## Forbidden Dependencies
- `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `Widget`: Base declarative UI node with virtual `paint`, `measure`, and `layout` callbacks.
- `Container`: Single-child padding and decoration container.
- `Row` / `Column` / `Stack`: Flexbox and z-ordered multi-child containers.
- `ScrollArea`: Clipping viewport with smooth scroll physics.
- `Window`: Top-level surface wrapper.
- **NOTE**: Interactive controls (`Button`, `Label`, `Slider`) are scheduled for Phase 4.7 (`0.6`).
