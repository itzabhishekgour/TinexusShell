# txui::accessibility — Semantic Tree & Assistive Technology Interfaces

> **Module**: `txui::accessibility`  
> **Responsibility**: Semantic roles, accessible labels, keyboard focus traversal, and screen reader / magnifier integration hooks.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`, `txui::input`, `txui::animation`, `txui::theme`, `txui::widgets`, `txui::effects`.

## Forbidden Dependencies
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `IAccessibleNode`: Virtual interface exposing semantic role, label, hint, bounding rect, and focus state.
- `SemanticRole`: Enumeration (`Button`, `Label`, `Container`, `Window`, `List`).
- `AccessibilityTree`: Semantic hierarchy broadcaster for assistive tools.
