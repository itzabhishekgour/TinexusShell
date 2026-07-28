# ADR 0002: Backend-Agnostic Renderer Abstraction

- **Status**: Accepted
- **Date**: 2026-07-28
- **Applies to**: `libtxui::render` (`Renderer`, `Backend`, `PixmanBackend`, `VulkanBackend`)

## Context
A modern Linux desktop UI framework must run efficiently on high-end Vulkan-capable GPUs as well as fallback software environments (such as headless servers, VMs without 3D acceleration, or early boot stages).

## Decision
We decouple the abstract `Renderer` interface from its physical rasterization backends:
- All concrete rendering logic is encapsulated in classes deriving from `txui::Backend`.
- `txui::PixmanBackend`: Uses software CPU rasterization via the Pixman library (Phase 4.2 initial implementation).
- `txui::VulkanBackend`: Uses hardware GPU Vulkan shaders and texture atlases (Future hardware target).
- The `Widget` tree and `Painter` never include backend headers directly.

## Rationale
1. **Zero UI Rewrite**: Upgrading from software rendering to Vulkan hardware shaders requires zero changes to widget or layout code.
2. **Deterministic Fallback**: If Vulkan device creation fails at runtime, `libtxui` automatically falls back to `PixmanBackend`.

## Consequences
- Requires virtual dispatch or interface bridges at the backend execution layer.
- Brush patterns and images must be represented in a common CPU/GPU accessible surface format (`txui::Image` with backend-specific handles).
