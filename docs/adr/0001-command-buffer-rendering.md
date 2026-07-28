# ADR 0001: Command Buffer vs. Immediate Mode Rendering

- **Status**: Accepted
- **Date**: 2026-07-28
- **Applies to**: `libtxui::render` (`CommandBuffer`, `Painter`, `Renderer`)

## Context
Desktop GUI toolkits must choose between immediate mode rendering (where drawing commands directly execute against a raster target per call) and retained/command-buffer rendering (where drawing commands are recorded into an intermediate buffer and executed later by a rasterizer backend).

## Decision
We adopt **Command-Buffer Rendering** for `libtxui 0.1` and above.
- The `txui::Painter` class does not directly mutate pixels.
- All drawing operations (`fillRect`, `drawPath`, `setClip`) emit immutable command objects into a `txui::CommandBuffer`.
- A backend-agnostic `txui::Renderer` consumes the `CommandBuffer` and dispatches it to the active `Backend` (`PixmanBackend` or `VulkanBackend`).

## Rationale
1. **GPU Batching & Culling**: An intermediate command buffer allows culling off-screen commands and merging adjacent draw calls before submitting them to Vulkan shaders.
2. **Backend Decoupling**: Widgets and layouts remain 100% agnostic to software CPU rasterization vs. hardware GPU pipelines.
3. **Thread Safety & Frame Pipelining**: Recording drawing commands on the UI thread while rasterizing the previous frame's command buffer on a background GPU thread becomes possible.

## Consequences
- Requires allocating temporary command nodes per frame (mitigated via arena/block allocators).
- Simplifies multi-backend testing via a headless `TestBackend` that inspects command lists without rendering pixels.
