# txui::theme — Design Tokens & HSL Dark/Light Mode Engine

> **Module**: `txui::theme`  
> **Responsibility**: Centralized HSL color tokens, typography scales, spacing tokens, and runtime theme switching.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`, `txui::input`, `txui::animation`.

## Forbidden Dependencies
- `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `ColorToken` / `TypographyToken`: Strongly typed semantic styling identifiers.
- `Theme`: Runtime design token dictionary (`DarkTheme`, `LightTheme`, `HighContrastTheme`).
- `ThemeManager`: Active theme broadcaster and switcher.
