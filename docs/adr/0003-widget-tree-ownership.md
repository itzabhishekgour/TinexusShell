# ADR 0003: Intrusive Ref-Counted Ownership Model for UI Nodes

- **Status**: Accepted
- **Date**: 2026-07-28
- **Applies to**: `libtxui::core` (`Object`, `Ref<T>`) and `libtxui::widgets` (`Widget`, `Container`)

## Context
GUI trees involve complex ownership graphs: parent containers own their child widgets, but event dispatchers, focus managers, and animations also hold short-lived references to active widgets. Using raw pointers leads to use-after-free bugs, while `std::shared_ptr` introduces unnecessary control-block heap allocations per widget.

## Decision
All visual objects in `libtxui` must derive from `txui::Object`, which embeds an atomic reference counter.
- Reference ownership is managed via `txui::Ref<T>` (an intrusive smart pointer).
- Parent containers hold `Ref<Widget>` references to their children.
- Widgets maintain weak/uncounted parent pointers to avoid reference cycles.

## Rationale
1. **Zero External Control Blocks**: Intrusive ref-counting eliminates the secondary memory allocation of `std::shared_ptr`.
2. **Safe Event & Animation Callbacks**: Animations and timers can hold a `Ref<Widget>` without risk of crash if the widget is removed from its parent tree mid-animation.

## Consequences
- Objects derived from `txui::Object` cannot be allocated on the stack; they must be heap-allocated via `txui::make_ref<T>(...)`.
