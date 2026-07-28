# txui::effects — Visual Shaders & Blur API

> **Module**: `txui::effects`  
> **Responsibility**: Real-time backdrop blur specifications, glassmorphism parameters, and shader effects.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`, `txui::input`, `txui::animation`, `txui::theme`, `txui::widgets`.

## Forbidden Dependencies
- `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `BlurEffect`: API stub defining `Radius`, `Sigma`, and `Quality`.
- `GlassEffect`: Backdrop blur and tint parameters for macOS/iOS style frosted glass.
