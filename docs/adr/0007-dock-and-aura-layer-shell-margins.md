# ADR 0007: Dock and Aura Layer Shell Margin Handling

## Status
Accepted

## Context
During the UI polish phase for the Tinexus Shell, visual clipping issues were identified with the `tinexus-dock` widget. The dock appeared entirely flat at the bottom, stuck to the edge of the screen, and its bottom rounded corners and drop shadows were clipped off.

Simultaneously, the `tinexus-shell` (Aura/Island widget) was experiencing text alignment issues where the 'T' logo and the text were not vertically centered, and an unwanted pill background was rendering when inactive.

Upon investigation, it was discovered that the root cause of the dock's clipping was a hardcoded margin reset in `Window::resize` (located in `src/txui/window/Window.cpp`). The logic was explicitly forcing a Top margin of `12px` and a Bottom margin of `0px` on **all** layer surfaces whenever a resize event occurred. Because the dock is anchored to the bottom (via `LayerAnchor::Bottom`), zeroing its bottom margin caused it to sit flush with the screen edge, and applying a top margin effectively pushed it downwards by 12 pixels, causing the Wayland compositor to crop the bottom 12 pixels of the dock's rendering buffer.

## Decision
We decided to fix the margin handling logic directly in the `Window::resize` method by applying conditional margins based on the widget's title (`m_title`).

1. **Aura (`tinexus-shell`)**:
   - Preserved the explicit `12px` Top margin.
   - Preserved the `0px` Bottom margin.
   
2. **Dock (`tinexus-dock`)**:
   - Restored and preserved a `12px` Bottom margin.
   - Preserved a `0px` Top margin.

For the Aura alignment, the text baseline calculations were manually adjusted to center the logo and text vertically, and the unwanted background rect was removed for the inactive state.

## Consequences

### Positive
- The `tinexus-dock` now correctly floats above the bottom screen edge, fully revealing its rounded corners and drop shadows without any clipping.
- The `tinexus-shell` (Aura) retains its correct top-center positioning.
- Text and logos within the Aura widget are now perfectly aligned.

### Negative
- `Window::resize` now contains hardcoded widget titles (`"shell"`, `"dock"`) to determine margin behavior. This is a slight architectural coupling between the UI library (`txui`) and specific applications. 
- **Future Tech Debt**: In the future, this hardcoded title check should be refactored into a configurable property on the `Window` class (e.g., `Window::set_margins(top, right, bottom, left)`) so that `txui` remains completely agnostic of the applications consuming it.
