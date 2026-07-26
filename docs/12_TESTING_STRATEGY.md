# Tinexus Platform — Testing & Quality Assurance Strategy

> **Document:** 12_TESTING_STRATEGY.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 02_REQUIREMENTS.md, 09_BUILD_SYSTEM.md

---

## Table of Contents

1. [Testing Philosophy](#1-testing-philosophy)
2. [The 8-Level Testing Pyramid](#2-the-8-level-testing-pyramid)
3. [Unit Testing (Google Test)](#3-unit-testing-google-test)
4. [Integration Testing (Headless Wayland Environment)](#4-integration-testing-headless-wayland-environment)
5. [UI Testing (Qt Test / QML Testing)](#5-ui-testing-qt-test--qml-testing)
6. [Performance & Benchmark Testing (Google Benchmark)](#6-performance--benchmark-testing-google-benchmark)
7. [Stress & Longevity Testing](#7-stress--longevity-testing)
8. [Fuzz Testing (libFuzzer / LLVM)](#8-fuzz-testing-libfuzzer--llvm)
9. [Memory Leak & Sanitizer Analysis (ASan/UBSan/Valgrind)](#9-memory-leak--sanitizer-analysis-asanubsanvalgrind)
10. [Wayland Protocol Conformance Testing](#10-wayland-protocol-conformance-testing)
11. [CI Automation & Quality Gates](#11-ci-automation--quality-gates)

---

## 1. Testing Philosophy

Untested code is broken code. Tinexus Platform enforces automated testing at every layer of the architecture. Code cannot be merged into `main` without 100% passing automated tests and minimum 80% code coverage.

---

## 2. The 8-Level Testing Pyramid

```
                       / \
                      /   \  Level 8: Wayland Protocol Conformance
                     /     \  Level 7: Fuzz Testing (Parsers/IPC)
                    /       \  Level 6: Stress & Longevity (72h)
                   /         \  Level 5: Memory Leak & Sanitizers
                  /           \  Level 4: Performance Benchmarks
                 /             \  Level 3: UI & QML Component Tests
                /               \  Level 2: Headless Integration Tests
               /_________________\  Level 1: Pure C++ Unit Tests
```

---

## 3. Unit Testing (Google Test)

- Targets: All non-UI domain logic, ranking algorithms, trigram indexer, TOML parser adapters, IPC frame encoders.
- Directory: `tests/unit/`
- Execution time target: Entire unit test suite runs in < 5 seconds.

---

## 4. Integration Testing (Headless Wayland Environment)

- Compositor tests run using wlroots `headless` backend (`WLR_BACKENDS=headless`).
- No physical GPU or monitor required.
- Tests window mapping, layer-shell placement, shortcut triggering, D-Bus method dispatch.
- Directory: `tests/integration/`

---

## 5. UI Testing (Qt Test / QML Testing)

- Uses `QtTest` framework to verify QML component property bindings, keyboard focus traversal, animation completion, and design token application.
- Directory: `tests/ui/`

---

## 6. Performance & Benchmark Testing (Google Benchmark)

Microbenchmarks measure critical execution paths:
- Search ranking pipeline latency (< 5ms target)
- Trigram index lookup speed
- IPC header serialization/deserialization
- Directory: `tests/benchmark/`

Regressions > 10% automatically fail CI.

---

## 7. Stress & Longevity Testing

- Automated 72-hour continuous test on reference hardware (T2).
- Simulates continuous window creation/destruction, launcher open/close toggles (10,000 cycles), and continuous search query streaming.
- Monitored for memory growth (leak check) and CPU creep.

---

## 8. Fuzz Testing (libFuzzer / LLVM)

Fuzz targets continuously fed random malformed input:
- `.desktop` file parser
- TOML configuration parser
- IPC header and payload decoder
- Custom Wayland protocol message decoder

Fuzzers live in `tests/fuzz/`.

---

## 9. Memory Leak & Sanitizer Analysis (ASan/UBSan/Valgrind)

- **Debug Builds:** AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`) enabled by default.
- **Valgrind Massif:** Measures heap allocation over 24-hour test runs to guarantee zero memory fragmentation leaks.

---

## 10. Wayland Protocol Conformance Testing

Custom Wayland client harness verifies:
- `zwlr_layer_shell_v1` compliance
- `ext-session-lock-v1` locking guarantees
- `tinexus-global-shortcut-v1` activation behavior

---

## 11. CI Automation & Quality Gates

```yaml
# Quality Gate Thresholds
Min Unit Test Coverage:        80%
Max Permissible Compiler Warn:  0 (-Werror enforced)
Max Permissible ASan Errors:   0
Max Search Latency P95:        50ms
```

---

*Document End: 12_TESTING_STRATEGY.md*
