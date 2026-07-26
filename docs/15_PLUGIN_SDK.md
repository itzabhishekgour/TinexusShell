# Tinexus Platform — Plugin SDK & Sandbox Architecture

> **Document:** 15_PLUGIN_SDK.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 03_SYSTEM_ARCHITECTURE.md, 07_SECURITY.md

---

## Table of Contents

1. [Plugin Architecture Overview](#1-plugin-architecture-overview)
2. [Process-Isolated Sandbox Model](#2-process-isolated-sandbox-model)
3. [Android-Style Capability Tokens](#3-android-style-capability-tokens)
4. [Plugin API Protocol (JSON-RPC over Unix Socket)](#4-plugin-api-protocol-json-rpc-over-unix-socket)
5. [Plugin Manifest Format (`plugin.toml`)](#5-plugin-manifest-format-plugintoml)
6. [Plugin SDK Header (`tinexus_plugin_sdk.h`)](#6-plugin-sdk-header-tinexus_plugin_sdkh)
7. [First-Party Plugin Reference Implementations](#7-first-party-plugin-reference-implementations)

---

## 1. Plugin Architecture Overview

Plugins extend `tinexus-searchd` with third-party search providers, automations, and tools (e.g., Spotify control, Git status, VS Code project launching, Docker container management).

**Core Safety Principle:** Plugins NEVER run inside the compositor or launcher process. They run as isolated, low-privilege child processes managed by the plugin subsystem.

---

## 2. Process-Isolated Sandbox Model

```
tinexus-searchd
       │ (Spawns isolated child process via clone3 + namespaces)
       ▼
Plugin Execution Sandbox
├── Seccomp-BPF Filter (Disallows execve, ptrace, socket(AF_INET))
├── Mount Namespace (Read-only access to plugin's own dir)
├── User Namespace (Runs as unprivileged nobody/sandbox user)
└── Unix Socket Pair (Connected only to tinexus-searchd)
```

---

## 3. Android-Style Capability Tokens

Plugins declare required capabilities in their manifest. The user is prompted during plugin installation to grant or deny each permission:

| Capability Token | Granted Access |
|---|---|
| `CAP_FILESYSTEM_READ` | Read access to user-selected directory paths |
| `CAP_CLIPBOARD_READ` | Access to active clipboard contents |
| `CAP_NETWORK_OUTBOUND` | Outbound HTTPS requests (e.g., GitHub, Jira) |
| `CAP_NOTIFICATIONS` | Power to trigger desktop notifications |
| `CAP_SYSTEM_ACTIONS` | Power to trigger system actions (Lock, Suspend) |
| `CAP_AUDIO_CONTROL` | Power to inspect and alter media playback |

If a plugin attempts an action without holding the Capability Token, the sandbox immediately terminates the plugin process with `SIGSYS`.

---

## 4. Plugin API Protocol (JSON-RPC over Unix Socket)

### 4.1 Search Request (Host → Plugin)

```json
{
  "jsonrpc": "2.0",
  "method": "search",
  "params": {
    "query": "spotify play",
    "max_results": 5
  },
  "id": 42
}
```

### 4.2 Search Response (Plugin → Host)

```json
{
  "jsonrpc": "2.0",
  "result": {
    "items": [
      {
        "id": "spotify:action:play",
        "title": "Play / Pause",
        "subtitle": "Spotify Media Control",
        "icon": "media-playback-start",
        "score": 0.95
      }
    ]
  },
  "id": 42
}
```

---

## 5. Plugin Manifest Format (`plugin.toml`)

```toml
[plugin]
id = "io.tinexus.plugins.spotify"
name = "Spotify Control"
version = "1.0.0"
author = "Tinexus Platform Team"
description = "Control playback and search tracks via Spotify API"
executable = "bin/spotify_plugin"
min_platform_version = "1.0.0"

[capabilities]
network = true
audio = true
clipboard = false
filesystem = false
```

---

## 6. Plugin SDK Header (`tinexus_plugin_sdk.h`)

Plugins can be written in C, C++, Rust, or Python using the C-compatible SDK wrapper:

```c
#ifndef TINEXUS_PLUGIN_SDK_H
#define TINEXUS_PLUGIN_SDK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* id;
    const char* title;
    const char* subtitle;
    const char* icon;
    float score;
} tinexus_plugin_item_t;

typedef void (*tinexus_search_callback_fn)(
    const char* query, 
    int32_t request_id
);

typedef void (*tinexus_activate_callback_fn)(
    const char* item_id
);

void tinexus_plugin_init(void);
void tinexus_plugin_push_results(
    int32_t request_id, 
    const tinexus_plugin_item_t* items, 
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif // TINEXUS_PLUGIN_SDK_H
```

---

## 7. First-Party Plugin Reference Implementations

Located in `sdk/examples/`:
- `spotify_plugin` (C++)
- `docker_plugin` (C++)
- `git_projects_plugin` (Rust)
- `vscode_recent_plugin` (C++)

---

*Document End: 15_PLUGIN_SDK.md*
