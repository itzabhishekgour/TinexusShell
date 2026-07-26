# Tinexus Shell — Project Vision

> **Document:** 01_VISION.md  
> **Version:** 1.0.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** ADR-000 (Architecture Review)

---

## Table of Contents

1. [The Problem We Are Solving](#1-the-problem-we-are-solving)
2. [Project Philosophy](#2-project-philosophy)
3. [Mission Statement](#3-mission-statement)
4. [Goals](#4-goals)
5. [Non-Goals](#5-non-goals)
6. [Future Vision](#6-future-vision)
7. [Target Audience](#7-target-audience)
8. [Design Principles](#8-design-principles)
9. [Competitive Analysis](#9-competitive-analysis)
10. [Why Tinexus Shell Exists](#10-why-Tinexus Shell-exists)

---

## 1. The Problem We Are Solving

Modern desktop environments have accumulated decades of design decisions built for a computing paradigm that no longer reflects how knowledge workers and developers actually use their computers. Today's Linux desktops — despite their technical merit — inherit assumptions from the 1990s.

**The fundamental problem is noise.**

A default GNOME installation presents the user with: a top bar, a dock, an Activities overview, a notification tray, a system tray, desktop icons (on some distributions), workspace indicators, and a clock — all before the user has opened a single application.

Every visual element on a desktop is a **demand on user attention**. When a user sits down to write code, design a product, or analyze data, every pixel that is not their work is a distraction. The cumulative effect of these distractions across an eight-hour workday is measurably harmful to deep work.

The desktop computing world has been moving toward a command palette paradigm. Evidence:

- **VSCode** (2015): Ctrl+Shift+P command palette becomes the primary interaction model
- **Notion** (2016): Slash commands as primary content creation interface
- **Linear** (2019): Command+K as primary navigation
- **Raycast** (2020): Replaces macOS Spotlight with a developer-first command palette
- **Arc Browser** (2022): Command bar replaces traditional browser chrome
- **Cursor** (2023): AI command bar as primary coding interface

The trend is clear. The future of desktop interaction is **intent-driven**, not **icon-driven**.

Tinexus Shell is the first Linux Desktop Environment designed from the ground up around this philosophy.

---

## 2. Project Philosophy

### 2.1 Core Philosophy

> **"Everything starts with simplicity."**

This is not a statement about features. It is a statement about **cognitive load**.

Every design decision in Tinexus Shell must answer one question: *Does this reduce or increase the user's cognitive load?*

If it increases cognitive load without a proportional gain in power or capability, it does not belong in Tinexus Shell by default.

### 2.2 The Three Principles

**Principle I: The Desktop is a Canvas, Not a Toolbar**

A blank canvas is not empty — it is full of potential. When a user opens their computer, they should see their wallpaper and nothing else. The desktop should feel like opening a clean notebook: ready for whatever the user chooses to create. Every application, every action, every file should be summoned intentionally — not displayed by default.

**Principle II: Intent Over Discovery**

Traditional desktops optimize for discovery: browse the file manager, scan the application grid, look at the dock. Discovery is for beginners. Power users and professionals know what they want. Tinexus Shell optimizes for intent: press Ctrl+K, type what you want, press Enter.

Discovery is still possible — the launcher can surface recently used applications, suggested files, and categories — but it is not the primary model. Tinexus Shell respects the user's time.

**Principle III: Performance is a Feature**

A 100ms delay is perceived by humans. A 300ms delay breaks flow. A 1-second delay causes task-switching. Tinexus Shell treats every millisecond as a UX decision. Animations must be smooth. The launcher must open instantly. Window transitions must not stutter. This is not a goal — it is a requirement. Any component that cannot meet its performance contract will be redesigned, not shipped.

---

## 3. Mission Statement

> Tinexus Shell is an open-source Linux Desktop Environment designed to eliminate visual noise, maximize user focus, and deliver the fastest, most elegant command-palette-first desktop experience on any operating system. It runs on the Linux kernel with Wayland and is built by engineers who believe the desktop can still be reinvented.

---

## 4. Goals

### 4.1 Technical Goals

| Goal ID | Goal | Measurable Target |
|---|---|---|
| G-T01 | Native Wayland support | Zero X11 code in core, XWayland optional |
| G-T02 | 60fps minimum animation | ≤16.67ms frame time at 60Hz |
| G-T03 | Launcher open time | ≤100ms from keypress to visible launcher |
| G-T04 | Search result latency | ≤50ms for first result set to appear |
| G-T05 | Idle RAM consumption | ≤150MB for compositor + all core daemons |
| G-T06 | Session boot time | ≤3 seconds from login to usable desktop |
| G-T07 | GPU-accelerated rendering | All animations via GPU pipeline, no CPU compositing |
| G-T08 | Modular architecture | All components replaceable without rebuilding core |
| G-T09 | C++20 codebase | Strict C++20, no older standard in new code |
| G-T10 | Zero crash compositor | Core compositor may not crash on app failure |

### 4.2 Product Goals

| Goal ID | Goal | Description |
|---|---|---|
| G-P01 | Distraction-free desktop | Empty desktop by default. No visible UI until summoned. |
| G-P02 | Unified launcher | All actions accessible from a single Ctrl+K interface |
| G-P03 | Plugin extensibility | Third-party plugins can add launcher commands safely |
| G-P04 | Theme engine | Full color/font/motion customization via theme tokens |
| G-P05 | Accessibility | WCAG 2.1 AA compliant by v1.0 |
| G-P06 | Multi-monitor support | Full HiDPI and mixed-DPI multi-monitor |
| G-P07 | Developer-first | Best-in-class developer workflow (terminal, IDE launch, git) |
| G-P08 | Privacy by default | No telemetry, no cloud, all data local |
| G-P09 | Open source | 100% open source, Apache 2.0 / GPL-2+ dual licensed |

### 4.3 Community Goals

- Build a contributor-friendly codebase from Day 1
- Maintain documentation that equals or exceeds Linux Foundation standards
- Publish stable, versioned APIs that plugin developers can rely on
- Establish a security disclosure process before first public release

---

## 5. Non-Goals

Understanding what Tinexus Shell will **not** do is as important as what it will do. These are hard boundaries that will be enforced in code review and design reviews.

| Non-Goal | Rationale |
|---|---|
| **Modifying the Linux Kernel** | Tinexus Shell runs on the kernel. It does not replace or modify it. |
| **Forking GNOME** | GNOME has a 20-year accumulation of design debt. We build, not fork. |
| **Forking KDE Plasma** | Same rationale. Shared genetics mean shared problems. |
| **Being a window manager only** | Tinexus Shell is a full Desktop Environment. A WM is a component. |
| **Supporting X11 natively** | Wayland-only. XWayland provides legacy app support. |
| **Cloud-dependent features** | All v1.0 functionality must work offline. Cloud is a future plugin. |
| **Mobile/tablet support in v1** | Desktop first. Touch support is a v2.0 research item. |
| **Replacing systemd** | Tinexus Shell integrates with systemd. Session management uses logind. |
| **Distributing a full OS** | Tinexus Shell is a Desktop Environment. It runs on existing distros. |
| **Creating a display server** | Wayland is the display protocol. We build a compositor, not a server. |
| **Supporting NVIDIA proprietary drivers** | NVIDIA proprietary Wayland support is a user responsibility. |

---

## 6. Future Vision

This section describes where Tinexus Shell is heading beyond Version 1.0. These are not commitments — they are design constraints. Every v1.0 decision must be made with these future directions in mind.

### 6.1 Version 2.0 — Intelligence Layer

The Tinexus Shell AI Layer will add natural language command processing to the launcher. It will be entirely local (no cloud), using a small, quantized language model (≤2GB RAM) for:

- Natural language app launching: *"open my design files from last week"*
- System automation: *"set up a Python development environment"*
- Contextual suggestions based on time of day and workflow
- Voice input (optional, via Whisper.cpp)

This will be implemented as a **search provider plugin**, not as a modification to the launcher core. The launcher's pluggable search provider architecture (designed in v1.0) will be the foundation.

### 6.2 Version 3.0 — Tinexus Shell as a Distribution

Long-term, the project may produce an opinionated Linux distribution with:
- Custom installer (not Calamares, custom-built)
- Curated application defaults
- Optimized kernel parameters
- Rolling release model
- First-class hardware support list

### 6.3 The Ultimate Vision: A New Computing Paradigm

In five to ten years, Tinexus Shell envisions a desktop where:

- Every action is accessible in under two keystrokes
- The desktop learns the user's workflow without surveillance
- Applications communicate through well-defined APIs
- The user's data is fully portable and never locked in
- The compositor is as reliable as the kernel

This is not a roadmap. It is a north star. Every engineering decision should ask: *does this move us toward or away from this vision?*

---

## 7. Target Audience

### 7.1 Primary Audience: Developer Power Users

**Profile:** Software engineers, system administrators, data scientists, technical architects.

**Characteristics:**
- Spend 6–10 hours per day at their computer
- Use keyboard shortcuts extensively
- Switch between multiple applications and contexts frequently
- Value performance over visual decoration
- Use a terminal daily
- Understand and accept that learning a new paradigm requires upfront investment

**Why Tinexus Shell:** Ctrl+K launch model eliminates mouse-based app switching. Minimal desktop reduces context switching cost. High performance means no waiting.

### 7.2 Secondary Audience: Design and Creative Professionals

**Profile:** UI/UX designers, video editors, graphic designers, architects.

**Characteristics:**
- Need distraction-free workspace for deep creative work
- Appreciate visual polish and animation quality
- Use multiple large monitors
- Benefit from clean desktop as mental canvas

**Why Tinexus Shell:** Clean desktop philosophy directly supports creative flow states. Beautiful launcher is consistent with design-oriented aesthetic preferences.

### 7.3 Tertiary Audience: Linux Enthusiasts and Minimalists

**Profile:** Users who run i3wm, Sway, DWM, or other tiling window managers but want a more complete DE experience.

**Characteristics:**
- Already comfortable with keyboard-driven interfaces
- Want more features than a bare WM without the weight of GNOME/KDE
- Interested in the open-source development process

**Why Tinexus Shell:** Bridges the gap between bare WM and full DE. Modular architecture means they can replace components they don't like.

### 7.4 Out of Scope Audiences (v1.0)

- General consumers migrating from Windows
- Enterprise IT deployers (target: v2.0)
- Users requiring accessibility-first design (target: v1.5)

---

## 8. Design Principles

### 8.1 The Six Principles of Tinexus Shell Design

**P1 — Default to Empty**  
Every screen, every panel, every surface should be empty until the user needs it. Information should be summoned, not displayed. The user's wallpaper is their desktop. Nothing else belongs there uninvited.

**P2 — Speed is Respect**  
Asking a user to wait is disrespecting their time. Every interaction must have a perceived response within 100ms. Animations must communicate state, not waste time. Loading should be invisible wherever possible.

**P3 — Keyboard First, Pointer Never Excluded**  
The primary interaction model is keyboard. Every action reachable by mouse must also be reachable by keyboard. The reverse is not required — some power actions are keyboard-only by design.

**P4 — Progressive Disclosure**  
Show minimal information first. Reveal more on request. The launcher shows 5 results by default. It shows 50 when the user scrolls. Settings shows basic options first. Advanced options require one more click.

**P5 — Consistent Motion Language**  
All animations in Tinexus Shell share the same easing curves, durations, and choreography. Motion is never random. Every animation follows the Tinexus Shell Motion Design System (see 05_UI_UX_GUIDELINES.md). Animation communicates state change — it is never decorative.

**P6 — Composable Defaults**  
Every default can be changed. Every component can be configured. Nothing is forced. But defaults must be opinionated and correct for the primary audience. The default configuration should require zero configuration to be productive.

---

## 9. Competitive Analysis

### 9.1 Desktop Environment Landscape

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DESKTOP ENVIRONMENT POSITIONING MAP                      │
│                                                                             │
│  HEAVY ──────────────────────────────────────────────────── LIGHT          │
│                                                                             │
│  [GNOME Shell]    [KDE Plasma]    [XFCE]    [Openbox]    [i3/Sway]        │
│       ↑                ↑              ↑           ↑           ↑            │
│   ~400MB RAM      ~250MB RAM    ~150MB RAM   ~80MB RAM   ~50MB RAM        │
│                                                                             │
│  TRADITIONAL ────────────────────────────────────────── INNOVATIVE         │
│                                                                             │
│  [GNOME Shell]                                           [Tinexus Shell]       │
│  [KDE Plasma]                 [XFCE]                     [Hyprland]        │
│                         [Budgie]                                            │
└─────────────────────────────────────────────────────────────────────────────┘

Tinexus Shell targets the LIGHT + INNOVATIVE quadrant:
  - Lighter than GNOME/KDE (target: ≤150MB idle)
  - More complete than bare i3/Sway
  - More innovative than XFCE/Budgie
```

### 9.2 Launcher Competitive Analysis

| Launcher | Platform | Open | Search | Extensions | Performance |
|---|---|---|---|---|---|
| **macOS Spotlight** | macOS | ❌ | App+File+Web | ❌ | Fast |
| **Raycast** | macOS | ❌ | Everything | ✅ (Paid) | Very Fast |
| **Alfred** | macOS | ❌ | Everything | ✅ (Paid) | Fast |
| **GNOME Search** | Linux | ✅ | Limited | Via extensions | Slow |
| **KRunner** | Linux | ✅ | Good | Via plugins | Good |
| **Rofi** | Linux | ✅ | Apps+Windows | Via scripts | Fast |
| **Wofi** | Linux | ✅ | Apps only | Limited | Fast |
| **Tinexus Shell Launcher** | Linux | ✅ | **Everything** | **Yes (sandboxed)** | **Very Fast** |

**Tinexus Shell Launcher Unique Advantages:**
- Only launcher with **sandboxed plugin isolation** (plugins cannot crash the launcher)
- Only launcher designed as a **first-class shell component** (not a standalone app)
- Only launcher with a **designed-in AI search provider slot** (future NLP)
- Only launcher with **native Wayland layer-shell integration** (appears above fullscreen apps)

### 9.3 Why Tinexus Shell is Different

| Dimension | GNOME | KDE | Hyprland | **Tinexus Shell** |
|---|---|---|---|---|
| **Primary paradigm** | Icon+Dock | Icon+Dock | WM only | **Command Palette** |
| **Desktop philosophy** | Content-first | Configurable | Minimal | **Intent-first** |
| **Plugin safety** | Unsafe (JS) | Unsafe (C++) | N/A | **Process-isolated** |
| **Documentation standard** | Good | Excellent | Fair | **Linux Foundation** |
| **AI integration** | None | None | None | **Roadmapped (v2)** |
| **Target user** | General | Power user | Enthusiast | **Developer/Creative** |

---

## 10. Why Tinexus Shell Exists

The developers of Tinexus Shell spent years using every major Linux desktop environment. We found something consistently true: the desktop always gets in the way.

We tried GNOME — beautiful, but slow and opinionated in ways we disagreed with.  
We tried KDE — powerful, but overwhelming and visually noisy.  
We tried i3 and Sway — fast and distraction-free, but bare. Too much manual work.  
We tried Hyprland — excellent compositor, but fundamentally a window manager.

None of these projects are bad. They are excellent projects maintained by talented people. But none of them were built around the philosophy we believe in: **the desktop should get out of the way.**

We are building Tinexus Shell because we believe:

1. The keyboard is faster than the mouse for 90% of power user actions
2. An empty desktop is more productive than a full one
3. A command palette is the most efficient application launcher ever designed
4. Linux deserves a desktop environment built with 2020s design principles
5. Open source can produce desktop software that rivals commercial products

This project exists for the developers who close every notification, hide every dock, and dream of a desktop that respects their focus.

We are building it.

---

*Document End: 01_VISION.md*  
*Next: 02_REQUIREMENTS.md*
