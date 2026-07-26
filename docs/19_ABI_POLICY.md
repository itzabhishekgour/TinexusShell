# Tinexus Platform — Application Binary Interface (ABI) Policy

> **Document:** 19_ABI_POLICY.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 09_BUILD_SYSTEM.md, 15_PLUGIN_SDK.md

---

## Table of Contents

1. [ABI Stability Philosophy](#1-abi-stability-philosophy)
2. [C vs C++ ABI Boundaries](#2-c-vs-c-abi-boundaries)
3. [Pointer-to-Implementation (Pimpl) Idiom](#3-pointer-to-implementation-pimpl-idiom)
4. [Symbol Visibility Control](#4-symbol-visibility-control)
5. [Inline Namespaces for Versioning](#5-inline-namespaces-for-versioning)
6. [Shared Library Versioning (`libtinexus-common.so`)](#6-shared-library-versioning-libtinexus-commonso)
7. [Breaking Change Prevention Rules](#7-breaking-change-prevention-rules)

---

## 1. ABI Stability Philosophy

Binary compatibility ensures that plugins, third-party extensions, and shared libraries compiled against Tinexus Platform SDK v1.0 continue to work seamlessly across all v1.x minor and patch releases without re-compilation.

---

## 2. C vs C++ ABI Boundaries

- **Internal Component Code:** Uses modern C++20 features (concepts, templates, ranges). No ABI guarantees between internal component binaries across releases.
- **Exported SDK & Plugin Interfaces (`sdk/include/`):** Must expose C-compatible headers (`extern "C"`) or strictly Pimpl-wrapped C++ interfaces to avoid C++ name-mangling and compiler-version fragility.

---

## 3. Pointer-to-Implementation (Pimpl) Idiom

Every exported public C++ class in `sdk/` uses the Pimpl idiom to hide private data members and internal implementation layout:

```cpp
// sdk/include/tinexus/search_result.hpp
namespace tinexus::v1 {

class TINEXUS_SDK_API SearchResult {
public:
    SearchResult();
    ~SearchResult();
    
    SearchResult(SearchResult&&) noexcept;
    SearchResult& operator=(SearchResult&&) noexcept;

    void setTitle(std::string_view title);
    [[nodiscard]] std::string_view title() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace tinexus::v1
```

Adding internal fields to `Impl` does NOT alter the class size or vtable layout of `SearchResult`, preserving ABI stability.

---

## 4. Symbol Visibility Control

Compiler flags default to hidden visibility:
`-fvisibility=hidden -fvisibility-inlines-hidden`

Only explicitly macro-annotated symbols are exported into shared object dynamic symbol tables:

```cpp
#if defined(TINEXUS_BUILD_SDK)
  #define TINEXUS_SDK_API __attribute__((visibility("default")))
#else
  #define TINEXUS_SDK_API
#endif
```

---

## 5. Inline Namespaces for Versioning

Exported headers use inline C++ namespaces to allow ABI evolution:

```cpp
namespace tinexus {
    inline namespace v1 {
        class SearchResult;
    }
}
```

When v2 introduces breaking layout changes, `v2` becomes inline while `v1` remains accessible for backwards compatibility.

---

## 6. Shared Library Versioning (`libtinexus-common.so`)

- **SONAME:** `libtinexus-common.so.1`
- **Real Name:** `libtinexus-common.so.1.0.0`
- Linker flags: `-Wl,-soname,libtinexus-common.so.1`

---

## 7. Breaking Change Prevention Rules

The following actions are STRICTLY FORBIDDEN in any minor release (`v1.x`):
- ❌ Removing a virtual function or altering virtual function order in an exported class.
- ❌ Changing the size or alignment of any exported struct/class.
- ❌ Removing or reordering fields in exported structs.
- ❌ Changing function parameter types in exported APIs.

ABI compliance is verified in CI using `abidiff` (libabigail).

---

*Document End: 19_ABI_POLICY.md*
