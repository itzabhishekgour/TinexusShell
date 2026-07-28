# txui::math — 2D/3D Geometry, Linear Algebra & Color Spaces

> **Module**: `txui::math`  
> **Responsibility**: Geometric primitives, affine transformations, matrix algebra, and color space conversions.

## Allowed Dependencies
- `txui::core`
- Standard C++20 Library (`<cmath>`, `<array>`, `<algorithm>`).

## Forbidden Dependencies
- `txui::graphics`, `txui::render`, `txui::layout`, `txui::widgets`, `txui::theme`, `txui::animation`, `txui::input`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `Point`: 2D/3D coordinate pair (`x, y`, `x, y, z`).
- `Size`: Dimensions pair (`width, height`).
- `Rect`: Axis-aligned bounding rectangle (`Point + Size`).
- `Insets`: Edge offsets (`top, right, bottom, left`).
- `Matrix4` / `Transform`: 4x4 projective transformation matrix.
- `Vector2` / `Vector3`: Linear algebra vector primitives.
- `ColorSpace`: sRGB / Display-P3 / Linear RGB coordinate conversions.
