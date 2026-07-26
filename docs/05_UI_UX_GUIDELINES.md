# Tinexus Shell — UI/UX Design Guidelines

> **Document:** 05_UI_UX_GUIDELINES.md  
> **Version:** 1.0.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 01_VISION.md, 03_SYSTEM_ARCHITECTURE.md

---

## Table of Contents

1. [Design System Philosophy](#1-design-system-philosophy)
2. [Color System](#2-color-system)
3. [Typography](#3-typography)
4. [Spacing and Layout](#4-spacing-and-layout)
5. [Motion Design System](#5-motion-design-system)
6. [Visual Effects](#6-visual-effects)
7. [Corner Radius System](#7-corner-radius-system)
8. [Elevation and Shadow](#8-elevation-and-shadow)
9. [Iconography](#9-iconography)
10. [Keyboard Navigation](#10-keyboard-navigation)
11. [Accessibility Guidelines](#11-accessibility-guidelines)
12. [Launcher Behavior Specification](#12-launcher-behavior-specification)
13. [Wallpaper Behavior](#13-wallpaper-behavior)
14. [Power Menu Specification](#14-power-menu-specification)
15. [Notification Design](#15-notification-design)
16. [Theme Engine](#16-theme-engine)
17. [Responsive Design](#17-responsive-design)
18. [Component State Matrix](#18-component-state-matrix)

---

## 1. Design System Philosophy

### 1.1 The Tinexus Shell Aesthetic

Tinexus Shell's visual identity is built on four pillars:

**Translucency** — Surfaces exist in layers. Background content is always subtly visible through foreground surfaces, creating spatial depth without opacity.

**Precision** — Every pixel is intentional. Spacing, sizing, and alignment follow a strict 4px base grid. No arbitrary values.

**Motion as Communication** — Animations do not exist for decoration. Every transition communicates a state change: appearing, disappearing, loading, completing.

**Restraint** — Less is more. The UI has fewer elements than users expect from a desktop shell. What is visible is there for a reason.

### 1.2 The Design Vocabulary

| Term | Meaning |
|---|---|
| **Surface** | Any visible panel, container, or card |
| **Layer** | The z-order layer a surface exists on |
| **Token** | A named design value (color, size, duration) |
| **Easing** | The acceleration curve of an animation |
| **Affordance** | A visual cue that an element is interactive |

---

## 2. Color System

### 2.1 Color Philosophy

Tinexus Shell uses a **dark-first** design. The primary theme is dark. Light theme is a first-class option, not an afterthought.

Color is not decorative — it is functional:
- **Primary** — The user's chosen accent (default: `#6B8CEF` — a calm indigo-blue)
- **Surface** — Background of panels and containers
- **On-surface** — Text and icons on surfaces
- **Error** — Error states only
- **Critical** — Critical notifications only (red)

### 2.2 Base Color Tokens (Dark Theme)

```toml
# assets/themes/dark.toml

[color]
# Backgrounds
background.desktop      = "#0A0A0E"   # Near-black, very slight blue tint
background.surface      = "#13131A"   # Surface panels
background.surface_alt  = "#1A1A24"   # Elevated surfaces (hover)
background.overlay      = "#0A0A0E99" # Overlay scrim (60% opacity)
background.input        = "#1E1E2A"   # Input fields

# Foreground
text.primary            = "#F0F0F8"   # Primary text
text.secondary          = "#9090A8"   # Subdued text (descriptions)
text.disabled           = "#505060"   # Disabled state
text.inverse            = "#0A0A0E"   # Text on light surfaces

# Accent (user-configurable)
accent.primary          = "#6B8CEF"   # Default indigo-blue
accent.hover            = "#8AAAF5"   # Lighter on hover
accent.pressed          = "#5070D4"   # Darker on press
accent.subtle           = "#6B8CEF1A" # 10% opacity accent (backgrounds)
accent.glow             = "#6B8CEF33" # 20% opacity accent (glow effects)

# Status
status.error            = "#EF6B6B"   # Error state
status.warning          = "#EFC86B"   # Warning state
status.success          = "#6BEFAC"   # Success state
status.critical         = "#EF4444"   # Critical notification

# Borders
border.default          = "#FFFFFF0F" # Very subtle border (6% white)
border.focus            = "#6B8CEF80" # Focus ring (50% accent)
border.hover            = "#FFFFFF1A" # Hover border (10% white)

# Glass effect base
glass.background        = "#13131ACC" # 80% opacity surface
glass.blur_strength     = "40px"
```

### 2.3 Base Color Tokens (Light Theme)

```toml
# assets/themes/light.toml

[color]
background.desktop      = "#F0F0F5"
background.surface      = "#FFFFFF"
background.surface_alt  = "#F5F5FA"
background.overlay      = "#00000040"
background.input        = "#F8F8FC"

text.primary            = "#0A0A1A"
text.secondary          = "#50506A"
text.disabled           = "#A0A0B8"

accent.primary          = "#4A6EE0"
accent.hover            = "#3A5ED0"
accent.pressed          = "#2A4EC0"
accent.subtle           = "#4A6EE01A"

border.default          = "#0000000F"
border.focus            = "#4A6EE080"

glass.background        = "#FFFFFFCC"
glass.blur_strength     = "30px"
```

### 2.4 Semantic Color Usage Rules

| Semantic Token | Do | Don't |
|---|---|---|
| `accent.primary` | CTA buttons, selected state, focus ring | General backgrounds |
| `status.error` | Error messages, failed state | General red elements |
| `text.secondary` | Descriptions, timestamps, hints | Primary content |
| `border.default` | Subtle separators | Prominent borders |
| `glass.background` | Launcher, notifications, lock screen | Regular app windows |

### 2.5 Color Contrast Requirements

| Pairing | Minimum Ratio | Target Ratio |
|---|---|---|
| `text.primary` on `background.surface` | 4.5:1 | 7:1 |
| `text.secondary` on `background.surface` | 3:1 | 4.5:1 |
| `accent.primary` on `background.surface` | 3:1 | 4.5:1 |
| Interactive element on any background | 3:1 | 4.5:1 |

---

## 3. Typography

### 3.1 Type Hierarchy

| Name | Font | Weight | Size | Line Height | Usage |
|---|---|---|---|---|---|
| `title-xl` | Inter | 700 (Bold) | 28px | 36px | Lock screen clock |
| `title-lg` | Inter | 600 (SemiBold) | 22px | 30px | Section headers |
| `title-md` | Inter | 600 (SemiBold) | 17px | 24px | Notification title |
| `title-sm` | Inter | 500 (Medium) | 14px | 20px | Result item title |
| `body-lg` | Inter | 400 (Regular) | 16px | 24px | Settings descriptions |
| `body-md` | Inter | 400 (Regular) | 14px | 20px | Result description |
| `body-sm` | Inter | 400 (Regular) | 12px | 16px | Timestamps, hints |
| `label-lg` | Inter | 500 (Medium) | 14px | 20px | Button labels |
| `label-sm` | Inter | 500 (Medium) | 11px | 14px | Tag labels, badges |
| `mono-md` | JetBrains Mono | 400 | 14px | 20px | Calculator result, file paths |
| `mono-sm` | JetBrains Mono | 400 | 12px | 16px | Command hints |

### 3.2 Font Loading Strategy

1. **Primary:** System Inter font (if available via fontconfig)
2. **Fallback 1:** Bundled Inter (if Tinexus Shell ships it)
3. **Fallback 2:** System `sans-serif` (Noto Sans, DejaVu Sans, etc.)
4. **Monospace:** JetBrains Mono (bundled) → system `monospace`

### 3.3 Text Rendering Settings

```
font-antialiasing: subpixel (LCD) for horizontal displays
font-hinting: slight (best for Inter at UI sizes)
minimum-font-size: 8pt
maximum-font-size: 36pt (user configurable)
```

### 3.4 Typography Rules

- **Never** use more than 2 font weights in a single surface
- **Never** use text smaller than `body-sm` (12px) for content
- **Always** align text to the 4px grid (top of cap-height, not baseline)
- **Numbers** in the launcher use tabular figures (tnum) for stable layout

---

## 4. Spacing and Layout

### 4.1 The 4px Base Grid

All spacing values in Tinexus Shell are multiples of **4px**.

| Token | Value | Usage |
|---|---|---|
| `space-1` | 4px | Minimum gap, icon margin |
| `space-2` | 8px | Compact padding, list item gap |
| `space-3` | 12px | Standard inner padding |
| `space-4` | 16px | Standard gap |
| `space-5` | 20px | Section padding |
| `space-6` | 24px | Large gap, card padding |
| `space-8` | 32px | Extra large gap |
| `space-10` | 40px | Section margins |
| `space-12` | 48px | Large section separation |
| `space-16` | 64px | Major layout divisions |

### 4.2 Launcher Dimensions

```
Launcher Window:
  Width:        640px (fixed)
  Max-height:   480px (content-driven, max cap)
  Position:     Center of active monitor, 30% from top
  
Search Bar:
  Height:       52px
  Padding:      space-4 (16px) horizontal, space-3 (12px) vertical
  Icon size:    20px
  Gap:          space-2 (8px)

Result Item:
  Height:       48px (compact), 56px (with description)
  Padding:      space-4 (16px) horizontal
  Icon size:    24px
  Gap between icon and text: space-3 (12px)

Section Header:
  Height:       32px
  Padding:      space-4 (16px) horizontal, space-2 (8px) vertical
  
Calculator Result:
  Height:       40px
  Padding:      space-4 (16px) horizontal
```

### 4.3 Notification Dimensions

```
Notification Card:
  Width:        360px
  Min-height:   64px
  Max-height:   160px
  Padding:      space-4 (16px)
  Margin from corner: space-4 (16px)
  Gap between stacked notifications: space-2 (8px)
  
Icon:           32px × 32px
Title:          title-md
Body:           body-md (max 3 lines)
```

---

## 5. Motion Design System

### 5.1 Animation Philosophy

> "Animation should be a shadow of a physical action, not a performance."

Every animation must answer: **What state change am I communicating?**

- **Appearing:** Elements scale from 95% → 100% + fade in (communicates materialization)
- **Disappearing:** Elements fade out (communicates dematerialization; no scale out)
- **Navigation:** Elements slide in the direction of navigation (communicates spatial relationship)
- **Selection:** Elements shift slightly toward selected state (communicates press)
- **Error:** Elements shake horizontally (communicates rejection)

### 5.2 Duration Scale

| Token | Duration | Usage |
|---|---|---|
| `duration-instant` | 0ms | State changes that require no animation (user-requested instant actions) |
| `duration-fast` | 80ms | Micro-interactions (hover, press feedback) |
| `duration-normal` | 150ms | Most UI transitions (focus change, selection) |
| `duration-medium` | 250ms | Panel appearances, result list updates |
| `duration-slow` | 400ms | Major view transitions, launcher open/close |
| `duration-xslow` | 600ms | Wallpaper transitions |

### 5.3 Easing Curves

Tinexus Shell uses cubic bezier curves, not linear or quadratic:

| Token | Bezier | Usage | Feel |
|---|---|---|---|
| `ease-standard` | `cubic-bezier(0.4, 0.0, 0.2, 1.0)` | General transitions | Smooth, balanced |
| `ease-decelerate` | `cubic-bezier(0.0, 0.0, 0.2, 1.0)` | Elements entering the screen | Decelerates into position |
| `ease-accelerate` | `cubic-bezier(0.4, 0.0, 1.0, 1.0)` | Elements exiting the screen | Accelerates out |
| `ease-spring` | `cubic-bezier(0.34, 1.56, 0.64, 1.0)` | Confirmations, selections | Slight overshoot, elastic |
| `ease-linear` | `linear` | Progress bars, loading only | No personality |

### 5.4 Launcher Open Animation

```
Timeline:
  0ms    Ctrl+K received
  5ms    Layer-shell surface visible (zero opacity)
  10ms   Backdrop blur begins computing
  10ms   Opacity: 0 → 1 [duration: 150ms, ease-decelerate]
  10ms   Scale: 0.97 → 1.0 [duration: 150ms, ease-decelerate]
  10ms   Blur: 0 → 40px [duration: 150ms, ease-standard]
  160ms  Launcher fully visible
  160ms  Search field focused
  165ms  Cursor blink begins
  
Result list stagger (triggered after first search):
  0ms    First result: opacity 0→1, translateY: 4px→0 [80ms, ease-decelerate]
  20ms   Second result: same animation
  40ms   Third result: same
  (each item offset 20ms, max 5 items staggered)
```

### 5.5 Launcher Close Animation

```
Timeline:
  0ms    Escape / outside click
  0ms    Opacity: 1 → 0 [duration: 100ms, ease-accelerate]
  0ms    Scale: 1.0 → 0.97 [duration: 100ms, ease-accelerate]
  100ms  Surface hidden (not destroyed — reused on next open)
```

### 5.6 Workspace Switch Animation

```
Direction: Left (→ next workspace)
  0ms    Outgoing workspace: translateX: 0 → -100% [200ms, ease-standard]
  0ms    Incoming workspace: translateX: 100% → 0 [200ms, ease-standard]
  200ms  Switch complete

Direction: Right (← previous workspace)
  (mirror of above)
```

### 5.7 Window Appear / Minimize Animation

```
Window appear:
  Scale: 0.9 → 1.0 [150ms, ease-decelerate]
  Opacity: 0 → 1 [100ms, ease-decelerate]

Window minimize:
  Scale: 1.0 → 0.8 [150ms, ease-accelerate]
  Opacity: 1 → 0 [100ms, ease-accelerate]
  
Window close:
  Scale: 1.0 → 0.95 [100ms, ease-accelerate]
  Opacity: 1 → 0 [80ms, ease-accelerate]
```

### 5.8 Reduced Motion Mode

When the user enables **Motion Reduction** in Settings:
- All durations reduced to 0ms (instant transitions)
- No scale animations
- Opacity transitions remain (at 50% duration)
- No spatial slide animations

---

## 6. Visual Effects

### 6.1 Backdrop Blur (Glassmorphism)

Tinexus Shell uses **backdrop blur** for the launcher, notifications, and lock screen. This creates the "frosted glass" effect.

**Implementation:** Qt's `MultiEffect` with `blurEnabled: true` or via Vulkan compute shader in the compositor.

| Surface | Blur Radius | Saturation | Brightness |
|---|---|---|---|
| Launcher | 40px | 120% | 100% |
| Notification | 30px | 110% | 100% |
| Lock Screen | 60px | 80% | 70% |
| Context Menu | 20px | 115% | 100% |

**Fallback:** If GPU cannot support backdrop blur (ancient hardware), use `background.surface` solid color without blur. Never render a transparent/empty surface.

### 6.2 Blur Performance Contract

- Blur must complete within the frame budget (16.67ms at 60Hz)
- On hardware that cannot achieve blur within frame budget: **automatically disable blur** and show solid surface
- User setting: `visual.backdrop_blur: true/false`

### 6.3 Gradient Usage

Gradients are used sparingly:

| Location | Gradient | Purpose |
|---|---|---|
| Launcher accent line | `linear-gradient(90deg, accent.primary 0%, accent.hover 100%)` | Visual accent |
| Result item hover | `linear-gradient(90deg, accent.subtle, transparent)` | Selection state |
| Notification urgency indicator | Solid (no gradient) | Cleaner appearance |

**Rule:** No more than one gradient per surface. Background surfaces are flat colors only.

---

## 7. Corner Radius System

| Token | Radius | Usage |
|---|---|---|
| `radius-xs` | 4px | Small tags, badges, chips |
| `radius-sm` | 6px | Input fields, compact elements |
| `radius-md` | 8px | Buttons, result items, cards |
| `radius-lg` | 12px | Notification cards |
| `radius-xl` | 16px | Launcher window, panels |
| `radius-2xl` | 24px | Lock screen widgets |
| `radius-full` | 999px | Avatar circles, toggle switches |

**Corner Radius Consistency Rule:** The inner element radius = outer element radius − padding. This maintains visual consistency when nesting rounded elements.

Example: Launcher (`radius-xl = 16px`) contains result items (`radius-md = 8px`). With 4px of padding, this is correct: 16 − (8 / 2) ≈ 12, near 8px.

---

## 8. Elevation and Shadow

Tinexus Shell uses **subtle shadows** to communicate layer elevation without heavy drop shadows.

| Elevation | Usage | Shadow Value |
|---|---|---|
| 0 | Desktop, wallpaper | None |
| 1 | In-app elements | `0 1px 3px rgba(0,0,0,0.3)` |
| 2 | Notification cards | `0 4px 16px rgba(0,0,0,0.4), 0 1px 4px rgba(0,0,0,0.2)` |
| 3 | Launcher window | `0 8px 32px rgba(0,0,0,0.5), 0 2px 8px rgba(0,0,0,0.3)` |
| 4 | Lock screen overlay | `0 16px 64px rgba(0,0,0,0.6)` |

**Shadow Color Rule:** In dark theme, shadows are pure black at varying opacities. In light theme, shadows use a tinted color matching the surface.

---

## 9. Iconography

### 9.1 Icon System

Tinexus Shell uses **system icon themes** (hicolor, Papirus, or user-selected). For its own UI icons:

- **Format:** SVG (scalable, color-adaptive)
- **Sizes:** 16×16, 24×24, 32×32, 48×48 (rasterized for performance)
- **Style:** Rounded line icons, 1.5px stroke weight at 24×24
- **Color:** Inherits `text.secondary` by default; `accent.primary` when active

### 9.2 App Icon Rendering

App icons in the launcher are rendered at:
- Result item: 24×24px
- Pinned apps (if dock enabled): 48×48px

Icons are loaded from the system icon theme using Qt's `QIcon::fromTheme()`. Fallback chain:
1. App's declared icon name in .desktop file
2. App's declared icon path in .desktop file (absolute path)
3. Generic `application-x-executable` icon
4. First letter of app name on colored background (Google-style avatar)

---

## 10. Keyboard Navigation

### 10.1 Global Keyboard Shortcuts

| Shortcut | Action | Configurable |
|---|---|---|
| `Ctrl+K` | Open launcher | ✅ |
| `Escape` | Close launcher / dismiss | ❌ (system reserved) |
| `Alt+Tab` | Window switcher | ✅ |
| `Alt+Shift+Tab` | Window switcher (reverse) | ✅ |
| `Super+[1-9]` | Switch to workspace N | ✅ |
| `Super+Shift+[1-9]` | Move window to workspace N | ✅ |
| `Super+Left/Right` | Snap window to half | ✅ |
| `Super+Up` | Maximize window | ✅ |
| `Super+Down` | Restore window | ✅ |
| `Super+L` | Lock screen | ✅ |
| `Super+Q` | Close active window | ✅ |

### 10.2 Launcher Keyboard Navigation

| Key | Action |
|---|---|
| `Arrow Up / Down` | Navigate results |
| `Enter` | Execute selected result |
| `Tab` | Cycle between search categories |
| `Shift+Tab` | Cycle categories reverse |
| `Escape` | Close launcher |
| `Ctrl+C` | Copy selected result text |
| `Alt+Enter` | Secondary action on result (e.g., Open file location) |
| `Ctrl+1...9` | Execute Nth result directly |

### 10.3 Focus Trap

When the launcher is open:
- Tab key cycles within launcher only
- Focus cannot leave the launcher via keyboard
- When launcher closes, focus returns to the previously focused window

### 10.4 Keyboard Navigation Visual Feedback

- **Focus ring:** `border.focus` (2px solid, `accent.primary` at 50% opacity, 2px offset)
- **Selected item:** `background.surface_alt` with left accent bar (3px, `accent.primary`)
- **Hover state:** `background.surface_alt` (no accent bar — hover is not selection)

---

## 11. Accessibility Guidelines

### 11.1 WCAG 2.1 AA Compliance Targets

| Criterion | Requirement | Tinexus Shell Implementation |
|---|---|---|
| 1.4.3 Contrast (Minimum) | 4.5:1 for text | All text pairs validated |
| 1.4.4 Resize Text | Scale to 200% | Font size settings + scale |
| 1.4.11 Non-text Contrast | 3:1 for UI components | Enforced in design tokens |
| 2.1.1 Keyboard | Full keyboard access | All actions keyboard-reachable |
| 2.4.3 Focus Order | Logical focus order | Enforced in QML tab order |
| 2.4.7 Focus Visible | Visible focus indicator | Focus ring on all interactive elements |
| 3.2.2 On Input | No unexpected context change | Keyboard nav does not execute actions |
| 4.1.2 Name, Role, Value | All elements named | AT-SPI2 accessible names |

### 11.2 AT-SPI2 Integration

All launcher result items must expose via AT-SPI2:
- **Role:** `ROLE_LIST_ITEM`
- **Name:** App name or action label
- **Description:** App description or action hint
- **State:** Selected / Not selected
- **Action:** "Activate" → launches the result

### 11.3 Screen Reader Announcements

| Event | Announcement |
|---|---|
| Launcher opens | "Tinexus Shell Launcher opened. Search field ready." |
| Result navigated | "{Result name}: {Result description}" |
| Result executed | "Launching {App name}" |
| No results | "No results found for '{query}'" |
| Launcher closes | "Launcher closed" |

---

## 12. Launcher Behavior Specification

### 12.1 Search Behavior

```
Query: ""  (empty)
Display: Recent apps (5 items) + System Actions (collapsed)

Query: "f" (single character)
Display: Apps starting with "F" (prefix match priority)

Query: "fire" (4+ characters)
Display: Fuzzy match across all providers, ranked by score + frequency

Query: "2+2" (expression)
Display: Calculator result "= 4" at top, then apps named "2+2" (usually none)

Query: "/home" (starts with /)
Display: File path navigation (v1.1)

Query: "lock" or "sleep" or "shutdown" (system keywords)
Display: System actions at top
```

### 12.2 Result Categories and Display Order

```
1. Calculator result (if query is arithmetic) — always first
2. Exact match apps
3. Prefix match apps
4. Fuzzy match apps
5. System actions (if query matches)
6. Clipboard entries (only when Tab selects Clipboard section)
7. Recent files (only when Tab selects Files section)
8. Plugin results (only when Tab selects Plugin section)
```

### 12.3 Empty State Design

When launcher has no query and no recent apps:
- Show Tinexus Shell logo (small, centered)
- Show placeholder text: "Type to search apps, files, and actions..."
- Show keyboard hint: `Ctrl+K` to open, `Esc` to close

### 12.4 Launcher Position Rules

- **Default:** Horizontally centered, 30% from top of active monitor
- **Multi-monitor:** Opens on the monitor containing the pointer
- **Fullscreen app:** Opens above fullscreen (layer-shell OVERLAY layer)

### 12.5 Launcher State After Close

The launcher process is **always running** (not launched on demand). When closed:
- QML surface is hidden (not destroyed)
- Search state is cleared
- Focus is returned to previous app
- App index remains loaded in memory (no re-indexing on reopen)

This ensures the open animation is never delayed by initialization.

---

## 13. Wallpaper Behavior

### 13.1 Wallpaper Rendering Modes

| Mode | Behavior | Use Case |
|---|---|---|
| `fill` | Scale to fill, center, crop | Default for all aspect ratios |
| `fit` | Scale to fit, letterbox with blur background | Unusual aspect ratios |
| `stretch` | Stretch to fill (no crop) | Rare, user preference |
| `center` | No scaling, centered, background color around | Small or pixel art images |
| `tile` | Tile to fill | Pattern images |

Default mode: `fill`

### 13.2 Per-Monitor Wallpaper

Each connected monitor can have an independent wallpaper:
```toml
[wallpaper]
default = "~/.config/Tinexus Shell/wallpapers/default.jpg"

[[wallpaper.per_monitor]]
output = "DP-1"
path = "~/.config/Tinexus Shell/wallpapers/monitor1.jpg"

[[wallpaper.per_monitor]]
output = "HDMI-A-1"
path = "~/.config/Tinexus Shell/wallpapers/monitor2.jpg"
```

### 13.3 Wallpaper on Launcher Open

When the launcher opens, the wallpaper remains fully visible. The launcher's frosted glass effect provides the visual separation — the wallpaper is not dimmed or hidden.

### 13.4 Wallpaper on Lock Screen

When the lock screen activates, the wallpaper is visible behind a heavily blurred and darkened overlay. The original wallpaper is not replaced — only a visual effect is applied on top.

---

## 14. Power Menu Specification

The power menu is not a separate UI — it is a **section within the launcher**.

### 14.1 Power Actions in Launcher

| Search Query | Action | Icon | Keyboard Shortcut |
|---|---|---|---|
| "shutdown" / "power off" | System shutdown | `system-shutdown` | Ctrl+Shift+Q (configurable) |
| "restart" / "reboot" | System restart | `system-reboot` | — |
| "sleep" / "suspend" | Suspend to RAM | `system-suspend` | — |
| "hibernate" | Suspend to disk | `system-hibernate` | — |
| "lock" / "lock screen" | Lock session | `system-lock-screen` | Super+L |
| "log out" / "logout" | End session | `system-log-out` | — |

### 14.2 Destructive Action Confirmation

For **Shutdown**, **Restart**, and **Log Out** only:

```
Action: Shutdown selected
┌─────────────────────────────────┐
│ ⏻  Shut Down?                  │
│                                 │
│ Your computer will power off.   │
│                                 │
│  [Cancel]  [Shut Down in 5s]   │
└─────────────────────────────────┘
Countdown: 5 → 4 → 3 → 2 → 1 → Execute
User can: Click Cancel, press Escape, or wait
```

**Sleep** and **Lock** do NOT require confirmation (they are reversible).

---

## 15. Notification Design

### 15.1 Notification Anatomy

```
┌──────────────────────────────────────────┐
│ [APP ICON] App Name          [CLOSE ×]  │
│            Notification Title           │
│            Body text up to 3 lines...  │
│                              [ACTION]  │
└──────────────────────────────────────────┘
```

| Element | Spec |
|---|---|
| App icon | 32×32px, rounded `radius-md` |
| App name | `label-sm`, `text.secondary` |
| Title | `title-md`, `text.primary` |
| Body | `body-md`, `text.secondary`, max 3 lines |
| Close button | 16×16px, appears on hover |
| Action button | `label-sm`, `accent.primary` text, no background |

### 15.2 Notification Stack Behavior

- Max 3 notifications visible at once
- Additional notifications queue behind visible ones
- When a notification is dismissed, the next slides in from the top
- Slide-in animation: `translateY: -40px → 0`, `opacity: 0 → 1`, 200ms ease-decelerate

### 15.3 Urgency Visual Language

| Urgency | Left Border Color | Title Color | Auto-dismiss |
|---|---|---|---|
| Low | None | `text.secondary` | 3s |
| Normal | `accent.primary` (2px left border) | `text.primary` | 5s |
| Critical | `status.critical` (3px left border) | `status.critical` | Never |

---

## 16. Theme Engine

### 16.1 Token-Based Theming

Tinexus Shell uses a **design token system**. The theme file defines all visual constants. No hardcoded colors exist in QML files.

All QML components access theme values through a global `Theme` object:

```qml
// In QML:
Rectangle {
    color: Theme.color.background.surface
    radius: Theme.radius.xl
}

Text {
    color: Theme.color.text.primary
    font.pixelSize: Theme.typography.titleMd.size
    font.family: Theme.typography.fontFamily
}
```

### 16.2 Theme File Structure

```toml
# A complete theme file defines all tokens

[meta]
name = "Tinexus Shell Dark"
version = "1.0"
author = "Tinexus Shell Team"
base_theme = "dark"   # "dark" | "light" | "high-contrast"

[color]
# ... all color tokens as defined in Section 2.2 ...

[typography]
font_family = "Inter"
mono_font_family = "JetBrains Mono"
# sizes defined by semantic tokens, not raw values

[motion]
duration_fast = 80
duration_normal = 150
duration_medium = 250
duration_slow = 400
duration_xslow = 600
reduced_motion = false   # Override: all durations → 0

[effects]
backdrop_blur = true
backdrop_blur_radius = 40
shadows = true

[spacing]
base = 4   # All spacing = N × base
```

### 16.3 Theme Application

Themes apply instantly when changed:
1. Settings UI writes new token values to `theme.toml`
2. Settings daemon notifies all subscribers via D-Bus `SettingChanged` signal
3. Each component receives the signal and reloads the `Theme` object
4. QML bindings automatically re-evaluate, updating the UI
5. No restart required

### 16.4 Built-in Themes

| Theme | Base | Description |
|---|---|---|
| `Tinexus Shell Dark` | Dark | Default. Dark blue-tinted surfaces, indigo accent |
| `Tinexus Shell Light` | Light | Clean white surfaces, blue accent |
| `Tinexus Shell High Contrast` | Dark | WCAG AAA contrast ratios, no transparency effects |
| `Tinexus Shell Midnight` | Dark | Pure black (#000000) surfaces, no blue tint |

---

## 17. Responsive Design

### 17.1 Multi-Monitor Considerations

| Scenario | Behavior |
|---|---|
| Single monitor | Normal layout |
| Two monitors (same DPI) | Extended desktop, launcher on focused monitor |
| Two monitors (different DPI) | Each monitor uses its own DPI scale |
| Ultra-wide single monitor | Launcher width capped at 640px, centered |
| Portrait monitor | Launcher adapts width to min(640, monitor_width - 64px) |
| Monitor disconnected | Open windows move to remaining monitors |

### 17.2 HiDPI Scaling

- All sizes are defined in logical pixels
- Qt handles DPR scaling automatically
- Icon sizes use the correct resolution from the icon theme
- Blur radii scale with DPR (40px logical = 80px physical at 2× DPR)

### 17.3 Small Screen Adaptation

For monitors smaller than 1024×768:
- Launcher width: min(640, screen_width × 0.9)
- Notification width: min(360, screen_width - 32px)

---

## 18. Component State Matrix

All interactive components must implement these states visually:

| State | Trigger | Visual Change |
|---|---|---|
| Default | Normal state | Base appearance |
| Hover | Pointer over element | `background.surface_alt` |
| Focus | Keyboard focus | Focus ring (`border.focus`) |
| Pressed | Active press | Darker background + slight scale (0.98) |
| Selected | Selected in list | Accent left bar + `background.surface_alt` |
| Disabled | Non-interactive | 40% opacity, no hover effect |
| Loading | Async operation | Animated shimmer / pulse |
| Error | Invalid state | `status.error` border |

### 18.1 Result Item State Example

```
Default:    [ICON] App Name                    Description
            Background: transparent

Hover:      [ICON] App Name                    Description
            Background: background.surface_alt
            
Selected:   ┃ [ICON] App Name                  Description
            Background: accent.subtle
            Left border: 3px accent.primary
            
Pressed:    ┃ [ICON] App Name                  Description
            Background: accent.subtle (darker)
            Scale: 0.99
```

---

*Document End: 05_UI_UX_GUIDELINES.md*  
*Next: 06_COMPONENT_DESIGN.md*
