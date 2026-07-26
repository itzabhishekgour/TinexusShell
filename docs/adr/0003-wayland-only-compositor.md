# ADR 0003: Wayland-Native Vulkan Compositor

## Status
ACCEPTED (Architecture Freeze v1.1)

## Context
X11 is deprecated and lacks security isolation between client windows.

## Decision
`tinexus-comp` is built strictly as a Wayland-native compositor on top of `wlroots` and `Vulkan` rendering pipelines.
It exposes `/run/user/$UID/wayland-0` for native Wayland clients (`xdg-shell`, `wlr-layer-shell-v1`).

## Consequences
- Zero legacy X11 code in core compositor binaries.
- High refresh rate frame pacing (60Hz / 120Hz / 144Hz) with clean damage tracking.
