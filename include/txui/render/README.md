# txui::render — Command-Buffer Rendering & Backend Rasterizers

> **Module**: `txui::render`  
> **Responsibility**: Canvas drawing operations, Command Buffer recording, Renderer orchestration, and Backend abstractions (Pixman/Vulkan).

## Allowed Dependencies
- `txui::core`, `txui::math`, `txui::graphics`.
- Standard C++20 Library (`<memory>`, `<vector>`, `<span>`).

## Forbidden Dependencies
- `txui::layout`, `txui::widgets`, `txui::theme`, `txui::animation`, `txui::input`, `txui::effects`, `txui::accessibility`.
- Any external UI framework (Qt, GTK, X11).

## Core Classes
- `CommandBuffer`: Immutable sequence of recorded drawing commands (`DrawRectCommand`, `DrawPathCommand`, etc.).
- `Painter`: High-level command recorder API (`fillRect`, `drawPath`, `setClip`).
- `Canvas`: Surface drawing target wrapper.
- `Renderer`: Orchestrator consuming a `CommandBuffer` and dispatching to a `Backend`.
- `Backend`: Abstract interface for physical rasterizers (`PixmanBackend` / `VulkanBackend`).
