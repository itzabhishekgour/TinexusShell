# txui::core — Tinexus UI Core Utilities & Ownership

> **Module**: `txui::core`  
> **Responsibility**: Foundational C++20 base classes, intrusive reference counting, assertions, timers, and GUIDs.

## Allowed Dependencies
- Standard C++20 Library (`<atomic>`, `<memory>`, `<string_view>`, `<chrono>`, `<cstdint>`, `<type_traits>`).

## Forbidden Dependencies
- **ALL other `txui` modules** (`math`, `graphics`, `render`, `layout`, `widgets`, `theme`, `animation`, `input`, `effects`, `accessibility`).
- Any external UI framework (Qt, GTK, X11).
- Any application-level headers (`shell`, `files`, `serviced`).

## Core Classes
- `Object`: Root base class for all ref-counted `libtxui` instances.
- `Ref<T>`: Intrusive smart pointer with atomic reference increment/decrement.
- `NonCopyable`: CRTP/mixin class preventing copy construction and assignment.
- `Types`: Core fixed-width scalar and coordinate type aliases.
- `Logger`: Formatted logging macros for diagnostic output.
- `Time` / `UUID`: Frame timing and unique identifier generation.
