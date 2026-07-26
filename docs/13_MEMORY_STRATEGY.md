# Tinexus Platform — Memory Allocation & Ownership Strategy

> **Document:** 13_MEMORY_STRATEGY.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 08_PERFORMANCE.md

---

## Table of Contents

1. [Memory Architecture Overview](#1-memory-architecture-overview)
2. [C++20 Memory Ownership Rules](#2-c20-memory-ownership-rules)
3. [Small Object Pool Allocator](#3-small-object-pool-allocator)
4. [Search Cycle Arena Allocator](#4-search-cycle-arena-allocator)
5. [Compositor Frame Allocator](#5-compositor-frame-allocator)
6. [GPU Resource Pool & Texture Atlas](#6-gpu-resource-pool--texture-atlas)
7. [String Interning Pool](#7-string-interning-pool)
8. [RAII Wrappers for C Libraries](#8-raii-wrappers-for-c-libraries)

---

## 1. Memory Architecture Overview

High-performance desktop systems fail when generic memory allocation (`malloc`/`free` or global `operator new`) is spammed on hot paths. Tinexus Platform uses specialized memory allocation strategies tailored to life-cycle characteristics.

---

## 2. C++20 Memory Ownership Rules

### 2.1 Ownership Types

| Type | Ownership Model | When to Use |
|---|---|---|
| `std::unique_ptr<T>` | Exclusive Single Owner | Default choice for all heap allocations. Transfers via `std::move`. |
| `std::shared_ptr<T>` | Shared Co-Ownership | ONLY for reference-counted cache objects or multi-threaded event buffers. Must document why. |
| `std::weak_ptr<T>` | Non-Owning Observer | Breaks cyclic dependencies in shared graph structures. |
| `T*` / `T&` | Non-Owning Observer | Function parameters and non-owning views. Never call `delete` on raw pointers. |

### 2.2 Move-Only Semantics

All major domain objects (`SearchResult`, `NotificationMessage`, `IPCMessage`, `WaylandSurfaceState`) are **move-only types** (`copy-constructor = delete`, `move-constructor = default`). Eliminates accidental heap copies.

---

## 3. Small Object Pool Allocator

- **Target:** High-frequency, fixed-size objects (`SearchResult` nodes, `Notification` entries, IPC headers).
- **Implementation:** Lock-free fixed-size block pool (`tinexus::memory::ObjectPool<T, BlockSize>`).
- **Benefit:** Zero kernel heap allocation calls on critical paths. $O(1)$ allocation and deallocation.

---

## 4. Search Cycle Arena Allocator

- **Target:** Intermediate search strings, tokenization vectors, scoring calculation structs in `tinexus-searchd`.
- **Strategy:** Linear bump-pointer allocation out of a 256KB pre-allocated block per search query.
- **Reset:** When search query finishes, offset resets to 0 ($O(1)$ bulk deallocation).

```cpp
class SearchArena {
    alignas(64) std::array<uint8_t, 256 * 1024> m_buffer;
    size_t m_offset{0};
public:
    template<typename T, typename... Args>
    T* allocate(Args&&... args);
    void reset() noexcept { m_offset = 0; }
};
```

---

## 5. Compositor Frame Allocator

- **Target:** Per-frame damage rectangles, render pass data, scene-graph dirty rect vectors in `tinexus-comp`.
- **Lifetime:** Exactly 1 frame (16.67ms at 60Hz).
- **Behavior:** Double-buffered frame arena. Frame $N$ uses Buffer A; Frame $N+1$ uses Buffer B while Buffer A resets.

---

## 6. GPU Resource Pool & Texture Atlas

- **Target:** Wallpaper textures, app icons, window thumbnails, QML layer buffers.
- **Atlas Layout:** 2048×2048 RGBA8 texture atlas for app icons.
- **VRAM Cap:** Maximum 260MB total VRAM allocation across all daemons.
- **Eviction:** LRU (Least-Recently-Used) texture eviction policy managed by `tinexus-wallpaper` and `tinexus-launcher`.

---

## 7. String Interning Pool

- **Target:** Application IDs (`"org.mozilla.firefox"`), icon names (`"web-browser"`), category labels.
- **Behavior:** Global thread-safe string pool returning `std::string_view` mapped to canonical deduplicated string storage. Saves megabytes of RAM across search indexes.

---

## 8. RAII Wrappers for C Libraries

All C types (`wlroots` structs, `libinput` handles, `PAM` handles) MUST use custom deleter `std::unique_ptr` aliases:

```cpp
using WlrSurfacePtr = std::unique_ptr<wlr_surface, void(*)(wlr_surface*)>;
using PamHandlePtr  = std::unique_ptr<pam_handle_t, void(*)(pam_handle_t*)>;
```

---

*Document End: 13_MEMORY_STRATEGY.md*
