# txui::animation — Natural Spring Physics & Fluid Motion Engine

> **Module**: `txui::animation`  
> **Responsibility**: Apple-style spring physics solvers, tweens, keyframes, and frame schedulers.

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`, `txui::render`, `txui::layout`, `txui::input`.

## Forbidden Dependencies
- `txui::theme`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `SpringSolver`: Natural spring-damper motion equations.
- `Tween<T>`: Interpolation value wrappers.
- `Timeline`: Multi-stage animation sequence coordinator.
- `FrameScheduler`: Vsync-synchronized frame dispatch loop.
