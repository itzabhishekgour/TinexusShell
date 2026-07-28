# src/txui — Tinexus UI Framework Private Implementation

> **Directory**: `src/txui`  
> **Responsibility**: Private `.cpp` source files and internal helper headers for `libtxui`.

## Architectural Rules
1. **Private Namespace**: All internal implementation details must reside in `namespace txui::internal { ... }`.
2. **No Application Import**: Applications (`shell`, `files`, etc.) MUST NEVER `#include` headers from `src/txui/` directly. All public contracts live in `include/txui/`.
3. **Backend Implementations**:
   - `render/pixman/`: Software CPU rasterization backend via Pixman (`PixmanBackend`).
   - `render/vulkan/`: Hardware GPU Vulkan backend stub (`VulkanBackend`).
4. **Testing**: Every implementation module must have corresponding unit tests in `tests/txui/` executed via `ctest`.
