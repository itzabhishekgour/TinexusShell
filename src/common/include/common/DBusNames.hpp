// ============================================================================
// DBusNames.hpp — Canonical D-Bus Service, Path, and Interface Constants
// Ref: AGENTS.md §5 (Strict Reverse-Domain Rule: io.tinexus.shell.<Component>)
// ============================================================================
#pragma once

#if defined(QT_CORE_LIB) || __has_include(<QtCore/QString>)
#include <QtCore/QString>
#define TINEXUS_HAS_QT 1
#endif

// Preprocessor macro definitions for compile-time string literal requirements
// (such as Qt Q_CLASSINFO("D-Bus Interface", ...) and XML string concatenation).
#define TINEXUS_DBUS_SERVICE_COMPOSITOR    "io.tinexus.shell.Compositor"
#define TINEXUS_DBUS_SERVICE_SUPERVISOR    "io.tinexus.shell.Supervisor"
#define TINEXUS_DBUS_SERVICE_IPC           "io.tinexus.shell.IPC"
#define TINEXUS_DBUS_SERVICE_SEARCH        "io.tinexus.shell.Search"
#define TINEXUS_DBUS_SERVICE_SESSION       "io.tinexus.shell.Session"
#define TINEXUS_DBUS_SERVICE_LAUNCHER      "io.tinexus.shell.Launcher"
#define TINEXUS_DBUS_SERVICE_SETTINGS      "io.tinexus.shell.Settings"
#define TINEXUS_DBUS_SERVICE_NOTIFICATIONS "io.tinexus.shell.Notifications"
#define TINEXUS_DBUS_SERVICE_CLIPBOARD     "io.tinexus.shell.Clipboard"
#define TINEXUS_DBUS_SERVICE_WALLPAPER     "io.tinexus.shell.Wallpaper"
#define TINEXUS_DBUS_SERVICE_DOCK          "io.tinexus.shell.Dock"

#define TINEXUS_DBUS_PATH_COMPOSITOR    "/io/tinexus/shell/Compositor"
#define TINEXUS_DBUS_PATH_SUPERVISOR    "/io/tinexus/shell/Supervisor"
#define TINEXUS_DBUS_PATH_IPC           "/io/tinexus/shell/IPC"
#define TINEXUS_DBUS_PATH_SEARCH        "/io/tinexus/shell/Search"
#define TINEXUS_DBUS_PATH_SESSION       "/io/tinexus/shell/Session"
#define TINEXUS_DBUS_PATH_LAUNCHER      "/io/tinexus/shell/Launcher"
#define TINEXUS_DBUS_PATH_SETTINGS      "/io/tinexus/shell/Settings"
#define TINEXUS_DBUS_PATH_NOTIFICATIONS "/io/tinexus/shell/Notifications"
#define TINEXUS_DBUS_PATH_CLIPBOARD     "/io/tinexus/shell/Clipboard"
#define TINEXUS_DBUS_PATH_WALLPAPER     "/io/tinexus/shell/Wallpaper"
#define TINEXUS_DBUS_PATH_DOCK          "/io/tinexus/shell/Dock"

#define TINEXUS_DBUS_INTERFACE_COMPOSITOR    "io.tinexus.shell.Compositor"
#define TINEXUS_DBUS_INTERFACE_SUPERVISOR    "io.tinexus.shell.Supervisor"
#define TINEXUS_DBUS_INTERFACE_IPC           "io.tinexus.shell.IPC"
#define TINEXUS_DBUS_INTERFACE_SEARCH        "io.tinexus.shell.Search"
#define TINEXUS_DBUS_INTERFACE_SESSION       "io.tinexus.shell.Session"
#define TINEXUS_DBUS_INTERFACE_LAUNCHER      "io.tinexus.shell.Launcher"
#define TINEXUS_DBUS_INTERFACE_SETTINGS      "io.tinexus.shell.Settings"
#define TINEXUS_DBUS_INTERFACE_NOTIFICATIONS "io.tinexus.shell.Notifications"
#define TINEXUS_DBUS_INTERFACE_CLIPBOARD     "io.tinexus.shell.Clipboard"
#define TINEXUS_DBUS_INTERFACE_WALLPAPER     "io.tinexus.shell.Wallpaper"
#define TINEXUS_DBUS_INTERFACE_DOCK          "io.tinexus.shell.Dock"

namespace tinexus::common::dbus {

// Canonical Reverse-Domain Bus Names (io.tinexus.shell.*)
namespace service {
    inline constexpr const char* Compositor    = TINEXUS_DBUS_SERVICE_COMPOSITOR;
    inline constexpr const char* Supervisor    = TINEXUS_DBUS_SERVICE_SUPERVISOR;
    inline constexpr const char* IPC           = TINEXUS_DBUS_SERVICE_IPC;
    inline constexpr const char* Search        = TINEXUS_DBUS_SERVICE_SEARCH;
    inline constexpr const char* Session       = TINEXUS_DBUS_SERVICE_SESSION;
    inline constexpr const char* Launcher      = TINEXUS_DBUS_SERVICE_LAUNCHER;
    inline constexpr const char* Settings      = TINEXUS_DBUS_SERVICE_SETTINGS;
    inline constexpr const char* Notifications = TINEXUS_DBUS_SERVICE_NOTIFICATIONS;
    inline constexpr const char* Clipboard     = TINEXUS_DBUS_SERVICE_CLIPBOARD;
    inline constexpr const char* Wallpaper     = TINEXUS_DBUS_SERVICE_WALLPAPER;
    inline constexpr const char* Dock          = TINEXUS_DBUS_SERVICE_DOCK;
} // namespace service

namespace path {
    inline constexpr const char* Compositor    = TINEXUS_DBUS_PATH_COMPOSITOR;
    inline constexpr const char* Supervisor    = TINEXUS_DBUS_PATH_SUPERVISOR;
    inline constexpr const char* IPC           = TINEXUS_DBUS_PATH_IPC;
    inline constexpr const char* Search        = TINEXUS_DBUS_PATH_SEARCH;
    inline constexpr const char* Session       = TINEXUS_DBUS_PATH_SESSION;
    inline constexpr const char* Launcher      = TINEXUS_DBUS_PATH_LAUNCHER;
    inline constexpr const char* Settings      = TINEXUS_DBUS_PATH_SETTINGS;
    inline constexpr const char* Notifications = TINEXUS_DBUS_PATH_NOTIFICATIONS;
    inline constexpr const char* Clipboard     = TINEXUS_DBUS_PATH_CLIPBOARD;
    inline constexpr const char* Wallpaper     = TINEXUS_DBUS_PATH_WALLPAPER;
    inline constexpr const char* Dock          = TINEXUS_DBUS_PATH_DOCK;
} // namespace path

namespace interface {
    inline constexpr const char* Compositor    = TINEXUS_DBUS_INTERFACE_COMPOSITOR;
    inline constexpr const char* Supervisor    = TINEXUS_DBUS_INTERFACE_SUPERVISOR;
    inline constexpr const char* IPC           = TINEXUS_DBUS_INTERFACE_IPC;
    inline constexpr const char* Search        = TINEXUS_DBUS_INTERFACE_SEARCH;
    inline constexpr const char* Session       = TINEXUS_DBUS_INTERFACE_SESSION;
    inline constexpr const char* Launcher      = TINEXUS_DBUS_INTERFACE_LAUNCHER;
    inline constexpr const char* Settings      = TINEXUS_DBUS_INTERFACE_SETTINGS;
    inline constexpr const char* Notifications = TINEXUS_DBUS_INTERFACE_NOTIFICATIONS;
    inline constexpr const char* Clipboard     = TINEXUS_DBUS_INTERFACE_CLIPBOARD;
    inline constexpr const char* Wallpaper     = TINEXUS_DBUS_INTERFACE_WALLPAPER;
    inline constexpr const char* Dock          = TINEXUS_DBUS_INTERFACE_DOCK;
} // namespace interface

// Legacy Aliases (pre-v1.1 names preserved for backward compatibility registration)
namespace legacy {
    inline constexpr const char* Settings       = "io.tinexus.Settings";
    inline constexpr const char* Wallpaper      = "io.tinexus.Wallpaper";
    inline constexpr const char* Dock           = "io.tinexus.Dock";
    inline constexpr const char* Compositor     = "io.tinexus.Compositor";

    inline constexpr const char* SettingsPath   = "/io/tinexus/Settings";
    inline constexpr const char* WallpaperPath  = "/io/tinexus/Wallpaper";
    inline constexpr const char* DockPath       = "/io/tinexus/Dock";
    inline constexpr const char* CompositorPath = "/io/tinexus/Compositor";
} // namespace legacy

// Desktop Entry & Application Identifiers
namespace app_id {
    inline constexpr const char* Prefix         = "io.tinexus.";
    inline constexpr const char* ShellPrefix    = "io.tinexus.shell.";
    inline constexpr const char* TopBar         = "io.tinexus.shell.TopBar";
    inline constexpr const char* Launcher       = "io.tinexus.shell.Launcher";
    inline constexpr const char* Dock           = "io.tinexus.shell.Dock";
    inline constexpr const char* Settings       = "io.tinexus.shell.Settings";
    inline constexpr const char* Shell          = "io.tinexus.shell";
    inline constexpr const char* Monitor        = "io.tinexus.monitor";
    inline constexpr const char* Terminal       = "io.tinexus.terminal";
} // namespace app_id

#if defined(TINEXUS_HAS_QT)
namespace qservice {
    inline QString Compositor()    { return QStringLiteral(TINEXUS_DBUS_SERVICE_COMPOSITOR); }
    inline QString Supervisor()    { return QStringLiteral(TINEXUS_DBUS_SERVICE_SUPERVISOR); }
    inline QString IPC()           { return QStringLiteral(TINEXUS_DBUS_SERVICE_IPC); }
    inline QString Search()        { return QStringLiteral(TINEXUS_DBUS_SERVICE_SEARCH); }
    inline QString Session()       { return QStringLiteral(TINEXUS_DBUS_SERVICE_SESSION); }
    inline QString Launcher()      { return QStringLiteral(TINEXUS_DBUS_SERVICE_LAUNCHER); }
    inline QString Settings()      { return QStringLiteral(TINEXUS_DBUS_SERVICE_SETTINGS); }
    inline QString Notifications() { return QStringLiteral(TINEXUS_DBUS_SERVICE_NOTIFICATIONS); }
    inline QString Clipboard()     { return QStringLiteral(TINEXUS_DBUS_SERVICE_CLIPBOARD); }
    inline QString Wallpaper()     { return QStringLiteral(TINEXUS_DBUS_SERVICE_WALLPAPER); }
    inline QString Dock()          { return QStringLiteral(TINEXUS_DBUS_SERVICE_DOCK); }

    namespace legacy {
        inline QString Settings()   { return QStringLiteral("io.tinexus.Settings"); }
        inline QString Wallpaper()  { return QStringLiteral("io.tinexus.Wallpaper"); }
        inline QString Dock()       { return QStringLiteral("io.tinexus.Dock"); }
        inline QString Compositor() { return QStringLiteral("io.tinexus.Compositor"); }
    } // namespace legacy
} // namespace qservice

namespace qpath {
    inline QString Compositor()    { return QStringLiteral(TINEXUS_DBUS_PATH_COMPOSITOR); }
    inline QString Supervisor()    { return QStringLiteral(TINEXUS_DBUS_PATH_SUPERVISOR); }
    inline QString IPC()           { return QStringLiteral(TINEXUS_DBUS_PATH_IPC); }
    inline QString Search()        { return QStringLiteral(TINEXUS_DBUS_PATH_SEARCH); }
    inline QString Session()       { return QStringLiteral(TINEXUS_DBUS_PATH_SESSION); }
    inline QString Launcher()      { return QStringLiteral(TINEXUS_DBUS_PATH_LAUNCHER); }
    inline QString Settings()      { return QStringLiteral(TINEXUS_DBUS_PATH_SETTINGS); }
    inline QString Notifications() { return QStringLiteral(TINEXUS_DBUS_PATH_NOTIFICATIONS); }
    inline QString Clipboard()     { return QStringLiteral(TINEXUS_DBUS_PATH_CLIPBOARD); }
    inline QString Wallpaper()     { return QStringLiteral(TINEXUS_DBUS_PATH_WALLPAPER); }
    inline QString Dock()          { return QStringLiteral(TINEXUS_DBUS_PATH_DOCK); }

    namespace legacy {
        inline QString Settings()   { return QStringLiteral("/io/tinexus/Settings"); }
        inline QString Wallpaper()  { return QStringLiteral("/io/tinexus/Wallpaper"); }
        inline QString Dock()       { return QStringLiteral("/io/tinexus/Dock"); }
        inline QString Compositor() { return QStringLiteral("/io/tinexus/Compositor"); }
    } // namespace legacy
} // namespace qpath

namespace qinterface {
    inline QString Compositor()    { return QStringLiteral(TINEXUS_DBUS_INTERFACE_COMPOSITOR); }
    inline QString Supervisor()    { return QStringLiteral(TINEXUS_DBUS_INTERFACE_SUPERVISOR); }
    inline QString IPC()           { return QStringLiteral(TINEXUS_DBUS_INTERFACE_IPC); }
    inline QString Search()        { return QStringLiteral(TINEXUS_DBUS_INTERFACE_SEARCH); }
    inline QString Session()       { return QStringLiteral(TINEXUS_DBUS_INTERFACE_SESSION); }
    inline QString Launcher()      { return QStringLiteral(TINEXUS_DBUS_INTERFACE_LAUNCHER); }
    inline QString Settings()      { return QStringLiteral(TINEXUS_DBUS_INTERFACE_SETTINGS); }
    inline QString Notifications() { return QStringLiteral(TINEXUS_DBUS_INTERFACE_NOTIFICATIONS); }
    inline QString Clipboard()     { return QStringLiteral(TINEXUS_DBUS_INTERFACE_CLIPBOARD); }
    inline QString Wallpaper()     { return QStringLiteral(TINEXUS_DBUS_INTERFACE_WALLPAPER); }
    inline QString Dock()          { return QStringLiteral(TINEXUS_DBUS_INTERFACE_DOCK); }

    namespace legacy {
        inline QString Settings()   { return QStringLiteral("io.tinexus.Settings"); }
        inline QString Wallpaper()  { return QStringLiteral("io.tinexus.Wallpaper"); }
        inline QString Dock()       { return QStringLiteral("io.tinexus.Dock"); }
        inline QString Compositor() { return QStringLiteral("io.tinexus.Compositor"); }
    } // namespace legacy
} // namespace qinterface

namespace qapp_id {
    inline QString Prefix()      { return QStringLiteral("io.tinexus."); }
    inline QString ShellPrefix() { return QStringLiteral("io.tinexus.shell."); }
    inline QString TopBar()      { return QStringLiteral("io.tinexus.shell.TopBar"); }
    inline QString Launcher()    { return QStringLiteral("io.tinexus.shell.Launcher"); }
    inline QString Dock()        { return QStringLiteral("io.tinexus.shell.Dock"); }
    inline QString Settings()    { return QStringLiteral("io.tinexus.shell.Settings"); }
    inline QString Shell()       { return QStringLiteral("io.tinexus.shell"); }
    inline QString Monitor()     { return QStringLiteral("io.tinexus.monitor"); }
    inline QString Terminal()    { return QStringLiteral("io.tinexus.terminal"); }
} // namespace qapp_id
#endif

} // namespace tinexus::common::dbus
