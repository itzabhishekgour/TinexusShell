# ADR 0005: Generic ActionRequest Protocol & Launch Authority

## Status
ACCEPTED (Architecture Freeze v1.1)

## Context
UI surfaces (`launcher`, `searchd`) should not directly invoke process execution routines or parse arbitrary shell strings.

## Decision
All system actions, app launches, URL openings, and wallpaper changes are dispatched via a generic `ActionRequest` protocol to `tinexus-serviced`. `tinexus-serviced` serves as the sole launch authority executing targets via structured `execvp()` without shell string injection.

## Consequences
- Eliminates shell command injection vulnerabilities (`|`, `;`, `&`, `` ` ``).
- Provides a unified action dispatch model for UI applications and SDK clients.
