# txui::graphics — Visual Primitives, Brushes & Styling Nodes

> **Module**: `txui::graphics`  
> **Responsibility**: Colors, gradients, drawing brushes, border styles, shadow definitions, and vector paths.

## Allowed Dependencies
- `txui::core`, `txui::math`.
- Standard C++20 Library (`<vector>`, `<variant>`, `<optional>`).

## Forbidden Dependencies
- `txui::render`, `txui::layout`, `txui::widgets`, `txui::theme`, `txui::animation`, `txui::input`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `Color`: HSL / RGBA color representation.
- `Gradient`: Linear and radial gradient stops.
- `Brush`: Variant encapsulating `SolidColor`, `Gradient`, `ImagePattern`, or `ShaderPattern`.
- `Pen`: Stroke width, cap style, join style, and brush.
- `Border` / `CornerRadius`: Border stroke and rounded corner radii.
- `Shadow`: Drop-shadow offset, color, and blur radius.
- `Path`: Vector Bezier curve and line path geometry.
