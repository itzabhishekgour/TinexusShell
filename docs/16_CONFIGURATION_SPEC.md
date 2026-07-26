# Tinexus Platform — Configuration Specification

> **Document:** 16_CONFIGURATION_SPEC.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 06_COMPONENT_DESIGN.md

---

## Table of Contents

1. [Configuration System Principles](#1-configuration-system-principles)
2. [Directory Hierarchy & System Defaults](#2-directory-hierarchy--system-defaults)
3. [`compositor.toml` Schema](#3-compositortoml-schema)
4. [`launcher.toml` Schema](#4-launchertoml-schema)
5. [`search.toml` Schema](#5-searchtoml-schema)
6. [`theme.toml` Schema](#6-themetoml-schema)
7. [`notifications.toml` Schema](#7-notificationstoml-schema)
8. [`shortcuts.toml` Schema](#8-shortcutstoml-schema)
9. [Atomic Write Algorithm & Hot-Reload Protocol](#9-atomic-write-algorithm--hot-reload-protocol)

---

## 1. Configuration System Principles

Tinexus Platform uses **TOML** for all configuration.
- **Typed & Human-Readable:** Strict schemas enforced by `tinexus-settings`.
- **Atomic Operations:** File edits write to `.tmp`, flush to disk (`fsync`), then atomically rename.
- **Zero-Downtime Hot Reload:** All daemons listen for `io.tinexus.shell.Settings` signals and update instantly.

---

## 2. Directory Hierarchy & System Defaults

```
System Defaults (Read-Only):
/etc/tinexus/defaults/
├── compositor.toml
├── launcher.toml
├── search.toml
├── theme.toml
├── notifications.toml
└── shortcuts.toml

User Configuration (Writable):
~/.config/tinexus/
├── compositor.toml
├── launcher.toml
├── search.toml
├── theme.toml
├── notifications.toml
└── shortcuts.toml
```

---

## 3. `compositor.toml` Schema

```toml
[display]
output = "DP-1"
mode = "3840x2160@144Hz"
scale = 1.5
transform = "normal"
vrr = true

[workspaces]
count = 4
animation = "slide"

[input]
kb_layout = "us"
repeat_rate = 25
repeat_delay = 300
accel_profile = "flat"
tap_to_click = true
```

---

## 4. `launcher.toml` Schema

```toml
[appearance]
width = 640
max_visible_items = 6
backdrop_blur_radius = 32
translucency = 0.85
position = "center"

[behavior]
debounce_ms = 30
close_on_activate = true
show_recent_on_empty = true
clear_query_on_close = true
```

---

## 5. `search.toml` Schema

```toml
[ranking]
base_weight_app = 1000
base_weight_action = 900
base_weight_file = 500
recent_launch_bonus = 400
pinned_bonus = 300
usage_frequency_factor = 1.5
recency_decay_hours = 168.0

[providers]
apps_enabled = true
files_enabled = true
calculator_enabled = true
clipboard_enabled = true
recent_enabled = true
```

---

## 6. `theme.toml` Schema

```toml
[meta]
name = "Tinexus Dark"
version = "1.0"
base_theme = "dark"

[color.background]
desktop = "#0A0A0E"
surface = "#13131A"
surface_alt = "#1A1A24"
overlay = "#0A0A0E99"
input = "#1E1E2A"

[color.text]
primary = "#F0F0F8"
secondary = "#9090A8"
disabled = "#505060"

[color.accent]
primary = "#6B8CEF"

[radius]
sm = 6
md = 8
lg = 12
xl = 16
```

---

## 7. `notifications.toml` Schema

```toml
[dnd]
enabled = false
schedule_enabled = true
schedule_start = "22:00"
schedule_end = "08:00"

[appearance]
anchor = "top_right"
timeout_ms = 4000
max_visible = 3

[[per_app_rules]]
app_name = "Signal"
allowed_in_dnd = true
```

---

## 8. `shortcuts.toml` Schema

```toml
[global]
launcher_open = "Ctrl+K"
terminal_open = "Super+Return"
screen_lock = "Super+L"
workspace_1 = "Super+1"
workspace_2 = "Super+2"
workspace_3 = "Super+3"
workspace_4 = "Super+4"
```

---

## 9. Atomic Write Algorithm & Hot-Reload Protocol

```cpp
void ConfigStore::saveAtomic(const std::string& path, const std::string& data) {
    std::string tmp_path = path + ".tmp." + std::to_string(getpid());
    int fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write(fd, data.data(), data.size());
    fsync(fd);
    close(fd);
    rename(tmp_path.c_str(), path.c_str());
    
    // Broadcast D-Bus signal via io.tinexus.shell.Settings
    emitSettingChangedSignal(path);
}
```

---

*Document End: 16_CONFIGURATION_SPEC.md*
