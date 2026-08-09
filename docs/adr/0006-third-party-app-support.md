# ADR-006: Third-Party App Support Strategy for Tinexus

**Status:** Proposed — awaiting decision
**Date:** 2026-08-09
**Deciders:** Tinexus core team

---

## Context

Tinexus is currently a from-scratch, ~40MB minimal Wayland OS with a custom
C++20 UI toolkit (`libtxui`), a custom compositor (`tinexus-comp`), and a
custom PID-1 supervisor (`tinexus-serviced`). It has no glibc-compatible
app ecosystem beyond what we build ourselves.

To let users run real-world apps (VS Code, Chrome, Firefox, GIMP, etc.) we
need a way to bring in binaries that were built against a much larger
dependency surface (glibc, GTK/Qt, NSS, fontconfig, D-Bus, PulseAudio/
PipeWire, and so on) than Tinexus ships today.

Three candidate strategies were evaluated:

1. **Rebase onto Ubuntu Core** (`debootstrap`) — put the full Ubuntu
   userland underneath Tinexus's compositor and UI shell.
2. **Integrate Flatpak** — keep the Tinexus base minimal, and let each app
   carry its own dependency runtime in a sandbox.
3. **Bundle apps as AppImages** — keep the Tinexus base minimal, and let
   each app carry its own dependency payload as a single mountable file,
   with no sandbox.

A fourth idea — running a `systemd-nspawn`/LXC container with the Tinexus
Wayland socket bind-mounted directly into it (a rough analogue of ChromeOS
Crostini) — was raised and **rejected during this review**. As written, it
bind-mounts the compositor's socket directly into the container with no
protocol filtering, which hands any app inside the container full control
of the compositor (input injection, cross-window snooping, ability to
crash/DoS the whole session). ChromeOS's actual implementation avoids this
via `sommelier`, a dedicated Wayland protocol proxy — building an
equivalent proxy is at least as much work as the Flatpak portal layer
described below, so this path has no effort advantage over Option B and is
not carried forward as a candidate here.

This ADR compares the three remaining options so we can commit to one path
(or a staged combination) before starting the App Installer work.

---

## Option A — Debootstrap / Ubuntu Core base

Tinexus's rootfs becomes a `debootstrap`-built Ubuntu minimal base;
`tinexus-comp`, `tinexus-serviced`, and `libtxui` are installed as the
default (and possibly only) desktop session on top of it, replacing GNOME/
KDE the way Ubuntu's own alternate spins do.

### Pros
- **App compatibility is close to 100% immediately.** Any `.deb`, any
  AppImage, any statically-built binary — `apt install` just works because
  the full library surface (glibc, GTK, Qt, NSS, PAM, systemd if kept)
  is already present.
- **No new IPC subsystem needed for portals.** Apps built against GTK/Qt
  already know how to talk to a standard Wayland compositor; file pickers,
  clipboard, and notifications are solved problems in those toolkits as
  long as `tinexus-comp` implements the standard Wayland protocols
  (which it mostly does or is close to).
- **KVM/QEMU, XWayland, PipeWire, etc. all "just work"** the way they do
  on any Ubuntu derivative, because it's the same base.
- **Fastest path to a usable daily-driver OS.**

### Cons
- **Kills the "40MB from-scratch" identity.** This is the point you and I
  both flagged — Tinexus stops being an OS and becomes a custom desktop
  environment *for* Ubuntu, architecturally similar to elementary OS,
  Zorin, or Pop!_OS. That's a legitimate product, but it's a different
  product than what P1–P4 of your session plan were building toward.
- **You inherit Ubuntu's full package/security surface.** `tinexus-serviced`
  as PID 1 either has to coexist with or replace `systemd`, `apt`,
  `dpkg`, `snapd`, and Ubuntu's own update mechanism — each of those is a
  potential source of the exact double-commit / stray-log-spam class of
  bugs you spent P1–P3 hunting down, except now in code you didn't write
  and don't fully control.
- **Image size balloons** — a minimal Ubuntu base alone is commonly
  several hundred MB before any desktop packages; "minimal OS" becomes a
  marketing claim you can no longer make truthfully.
- **Your custom compositor now has to be bulletproof against arbitrary
  upstream GTK/Qt/Electron apps**, not just your own `libtxui` windows.
  Bugs like your P1 double-commit become much harder to reproduce and
  fix when the client isn't code you own.

---

## Option B — Flatpak on native minimal Tinexus

Tinexus rootfs stays minimal and from-scratch. Flatpak (`flatpak` +
`bubblewrap` + a Tinexus-authored `xdg-desktop-portal-tinexus` backend)
is added as an optional, sandboxed app-runtime layer.

### Pros
- **Preserves the "minimal, pure, from-scratch" identity.** The base OS
  doesn't change; Flatpak is opt-in infrastructure, not a rebase.
- **Apps are sandboxed by default** — better security story than "give
  every app full filesystem access," which aligns with the
  privilege-broker thinking you already have in `tinexus-ipcd`.
- **You control exactly what capabilities leak into the sandbox** (via
  portals), which is a more deliberate security model than inheriting
  Ubuntu's ambient trust assumptions wholesale.
- **Image size stays small** — Flatpak runtimes (Freedesktop Platform,
  GNOME Platform, etc.) are downloaded per-app/per-runtime, not baked
  into the base image.

### Cons
- **`xdg-desktop-portal-tinexus` is a real sub-project, not a checkbox.**
  You will need to implement, at minimum:
  - `org.freedesktop.portal.FileChooser` (needs your file picker UI)
  - `org.freedesktop.portal.Notification`
  - `org.freedesktop.portal.Settings` (theme, accent color, etc. — apps
    query this instead of reading GTK config files directly)
  - `org.freedesktop.portal.OpenURI` / `Camera` / `ScreenCast` as apps
    demand them
  Each portal is its own D-Bus service with its own protocol surface —
  this is comparable in scope to the whole P1–P4 session plan you just
  finished, done again for a new subsystem.
- **You need a working D-Bus session bus and `bubblewrap`/`user
  namespaces`** as new OS-level dependencies — not huge, but not zero.
- **Not every app is on Flathub, and not every Flathub app is
  well-behaved** — some still assume host GTK theming or host fonts are
  present, so visual/behavioral inconsistency is possible even after the
  plumbing works.
- **App compatibility is high but not 100%** — apps needing deep host
  integration (some VMs, some hardware-access tools) fight the sandbox
  model by design.

---

## Option C — AppImage on native minimal Tinexus

Tinexus rootfs stays minimal and from-scratch. `libfuse3` and a
reasonably current `glibc` are added to the base image. A double-click
(or drag-drop) handler mounts the `.AppImage` via
`AppImage --appimage-mount` (or triggers `--appimage-extract-and-run` as
a fallback where FUSE is unavailable) and launches the contained binary,
which talks to `tinexus-comp` directly over the standard Wayland socket —
no portals, no sandbox, no container.

### Pros
- **Lowest implementation effort of the three options.** No new IPC
  subsystem, no D-Bus session bus, no portal protocol work. The bulk of
  the task is a mount-and-exec wrapper plus a UI affordance for it.
- **Small base image growth** — `libfuse3` + a current `glibc` is on the
  order of a few MB, not hundreds.
- **Wide real-world coverage today** — Chrome, VS Code, Figma, and many
  others ship official or well-maintained unofficial AppImages, so
  compatibility is immediately useful without waiting on portal work.
- **Fastest path to "Chrome/VS Code opens"** of any option that doesn't
  touch the base OS identity.

### Cons — the "invisible tax" here is security, not engineering effort
- **Zero sandboxing.** An AppImage gets full filesystem, network, and
  device access — the same ambient trust model as a native binary. This
  directly bypasses the privilege-broker philosophy already built into
  `tinexus-ipcd`; there is no mediation layer standing between an
  AppImage and the rest of the system. This must be surfaced to the user
  explicitly (e.g. an install-time notice: *"This app has full access to
  your system"*), not silently accepted.
- **glibc ABI is backward-compatible only, not forward-compatible.**
  Most AppImages bundle everything *except* glibc (bundling glibc itself
  is technically possible but uncommon and fragile). If an AppImage was
  built against a newer glibc than Tinexus's base ships, it will fail to
  load symbols or segfault at runtime — often silently, with an unhelpful
  error. This means "45–50MB and everything just works" is not a
  guarantee; Tinexus's base glibc version has to be kept reasonably
  current to keep this failure mode rare, which is an ongoing maintenance
  commitment, not a one-time fix.
- **FUSE is not universally available** — some restricted or nested
  environments block it, so the mount handler needs the
  `--appimage-extract-and-run` fallback path implemented from day one,
  not as an afterthought.
- **No portal-mediated integration** — file pickers, theming, and
  notifications are whatever the app bundles itself, so visual and
  behavioral consistency with the rest of Tinexus (including the P4
  chrome polish work) is not guaranteed and can't be enforced centrally.
- **Does not reduce future work if Flatpak is still wanted for its
  sandboxing benefits** — AppImage support and Flatpak support are
  additive, not substitutes for each other; shipping Option C doesn't
  shrink the Option B portal work later.

---

## Comparison Table

| Dimension | A: Debootstrap / Ubuntu Core | B: Flatpak on native Tinexus | C: AppImage on native Tinexus |
|---|---|---|---|
| Preserves "40MB from-scratch" identity | No | Yes | Yes (~45–50MB) |
| App compatibility | ~100%, immediate | High, but sandbox-limited | High, subject to glibc-version match |
| New subsystem to build | Minimal (mostly config) | `xdg-desktop-portal-tinexus` (large) | Mount/exec wrapper (small) |
| Security model | Inherits Ubuntu's ambient trust | Sandboxed, portal-mediated (fits your `tinexus-ipcd` philosophy) | None — full ambient trust, explicitly disclosed to user |
| Image size | Large (hundreds of MB+) | Stays small; runtimes fetched on demand | Stays small; `libfuse3` + current glibc only |
| Compositor risk surface | High — must handle arbitrary GTK/Qt/Electron clients | Same, but sandboxed apps are more predictable/contained | Same as A — untrusted, unsandboxed clients |
| Time to "Chrome/VS Code works" | Fast | Slower (portal work gates it) | Fast |
| Long-term maintenance burden | Track Ubuntu's security updates, apt/dpkg quirks | Own and evolve your own portal implementation | Keep base glibc current to avoid ABI breakage |
| Fits your stated goal ("minimal & pure, but daily-driver-capable") | Contradicts it | Directly serves it | Serves the "minimal" half; only partially serves "secure by design" |

---

## Recommendation

Given the goal you stated — **"Tinexus apni Minimal & Pure identity
maintain kare, par future mein log daily driver apps chala sakein"** —
**Option B (Flatpak) is the architecturally correct choice**, not Option A.
Debootstrap solves the app-compatibility problem by deleting the thing
that makes Tinexus interesting.

That said, Flatpak's cost is real and shouldn't be hidden behind
"add Flatpak" as a one-line item in a sprint plan. Recommend sequencing
it as its own phase, separate from and after the App Installer /
Gatekeeper work you already scoped:

1. **Phase now:** Finish `.txapp` drag-install + Gatekeeper signature
   check for **native `libtxui` apps only** (your own apps: Settings,
   Terminal, future first-party tools). This is the scope you already
   sized at 2.5–4 weeks and it's a self-contained, shippable milestone.
2. **Phase next (separate ADR-worthy effort):** Stand up a minimal
   `xdg-desktop-portal-tinexus` with just `FileChooser` + `Settings`,
   and get *one* real-world Flatpak app (something simple, e.g. a GTK
   text editor) running end-to-end as a proof of concept before
   committing to Chrome/VS Code-scale targets.
3. **Phase later:** Expand portal coverage based on which real apps
   people actually want (notifications, screen-share, camera) rather
   than building all portals speculatively.

This keeps every phase independently shippable and testable in QEMU,
matches how you've been sequencing P1–P4 already, and avoids ever
touching the "rebase the whole OS" decision, which is close to
irreversible once taken.

---

## Open Question for the Team

Do we want `tinexus-ipcd`'s existing privilege-broker pattern
(Settings → supervisor override) to become the same mechanism that
backs the Flatpak portals, or should portals be a fully separate D-Bus
service? Sharing the broker is more consistent architecturally; keeping
them separate limits blast radius if the portal implementation has bugs.
Recommend deciding this before writing any portal code, since it
determines whether `tinexus-ipcd`'s protocol needs to grow a new message
family now or later.
