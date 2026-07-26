# Tinexus Platform — Wayland Protocol Specifications

> **Document:** 14_WAYLAND_PROTOCOLS.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 06_COMPONENT_DESIGN.md

---

## Table of Contents

1. [Wayland Protocol Overview](#1-wayland-protocol-overview)
2. [Standard Protocol Adoption](#2-standard-protocol-adoption)
3. [`tinexus-global-shortcut-v1` Custom Protocol](#3-tinexus-global-shortcut-v1-custom-protocol)
4. [`tinexus-shell-helper-v1` Custom Protocol](#4-tinexus-shell-helper-v1-custom-protocol)
5. [Protocol Header Code Generation (wayland-scanner)](#5-protocol-header-code-generation-wayland-scanner)

---

## 1. Wayland Protocol Overview

Tinexus Compositor (`tinexus-comp`) implements both freedesktop.org standard Wayland protocols and custom extension protocols designed specifically for Tinexus Shell components.

---

## 2. Standard Protocol Adoption

| Protocol Name | Purpose | Used By |
|---|---|---|
| `wl_compositor` / `wl_subcompositor` | Core rendering surfaces | All Wayland clients |
| `xdg_shell` | Standard window management | GUI Applications |
| `zwlr_layer_shell_v1` | Shell surface placement (Overlay, Background) | Launcher, Wallpaper, Notifications |
| `ext_session_lock_v1` | Secure lock screen protocol | `tinexus-lock` |
| `zwp_idle_inhibit_v1` | Screen sleep inhibition | Media players |
| `zwlr_screencopy_v1` | Screen content capture | Compositor backdrop blur snapshot |

---

## 3. `tinexus-global-shortcut-v1` Custom Protocol

### 3.1 Description

Allows shell clients (specifically `tinexus-launcher`) to register global shortcuts (e.g., `Ctrl+K`) with `tinexus-comp` without needing X11-style raw keylogging.

### 3.2 Protocol XML

```xml
<?xml version="1.0" encoding="UTF-8"?>
<protocol name="tinexus_global_shortcut_v1">
  <copyright>
    Copyright 2026 Tinexus Platform Team.
    Licensed under Apache-2.0.
  </copyright>

  <interface name="tinexus_global_shortcut_manager_v1" version="1">
    <request name="register_shortcut">
      <arg name="id" type="new_id" interface="tinexus_global_shortcut_v1"/>
      <arg name="key_combo" type="string" summary="e.g. 'ctrl+k'"/>
      <arg name="description" type="string"/>
    </request>
  </interface>

  <interface name="tinexus_global_shortcut_v1" version="1">
    <event name="activated">
      <arg name="tv_sec_hi" type="uint"/>
      <arg name="tv_sec_lo" type="uint"/>
      <arg name="tv_nsec" type="uint"/>
    </event>
    <request name="destroy" type="destructor"/>
  </interface>
</protocol>
```

---

## 4. `tinexus-shell-helper-v1` Custom Protocol

### 4.1 Description

Exposes shell status metadata (active workspace ID, total workspaces, window focus changes, backdrop blur updates) directly from `tinexus-comp` to `tinexus-launcher`.

### 4.2 Protocol XML

```xml
<?xml version="1.0" encoding="UTF-8"?>
<protocol name="tinexus_shell_helper_v1">
  <interface name="tinexus_shell_helper_v1" version="1">
    <event name="workspace_changed">
      <arg name="active_workspace" type="uint"/>
      <arg name="total_workspaces" type="uint"/>
    </event>
    <event name="active_window_changed">
      <arg name="app_id" type="string"/>
      <arg name="title" type="string"/>
    </event>
    <request name="request_backdrop_snapshot"/>
  </interface>
</protocol>
```

---

## 5. Protocol Header Code Generation (wayland-scanner)

All custom XML files live in `protocols/`. `CMakeLists.txt` automatically invokes `wayland-scanner` during build:

```cmake
# protocols/CMakeLists.txt
ecm_add_wayland_server_protocol(COMPOSITOR_PROTOCOLS
    PROTOCOL tinexus-global-shortcut-v1.xml
    BASENAME tinexus-global-shortcut-v1
)
```

---

*Document End: 14_WAYLAND_PROTOCOLS.md*
