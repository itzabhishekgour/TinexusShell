# ADR 0005: Token-Driven Design System over Hardcoded Styling

- **Status**: Accepted
- **Date**: 2026-07-28
- **Applies to**: `libtxui::theme` (`Theme`, `ColorTokens`, `TypographyTokens`)

## Context
Hardcoding color hex codes, font sizes, or corner radii directly inside application widget code makes platform-wide theming (such as Dark Mode, High Contrast, or Accent Color switching) impossible without extensive code modification.

## Decision
All styling parameters in `libtxui` must be referenced via centralized **Theme Design Tokens**:
- Semantic Color Tokens: `BackgroundPrimary`, `BackgroundSecondary`, `TextPrimary`, `AccentActive`, `BorderDefault`.
- Typography Tokens: `FontTitle`, `FontBody`, `FontCaption`, `FontMonospace`.
- Spatial Tokens: `RadiusSmall` (4px), `RadiusMedium` (8px), `RadiusLarge` (12px), `ElevationShadow`.
- Themes are loadable and switchable at runtime via JSON/TOML token maps.

## Rationale
1. **Instant Dark/Light Mode Switching**: Changing the active token map instantly updates all widgets on the next frame without recreating UI trees.
2. **Platform Consistency**: Ensures Finder-style Tinexus Files, Dock, Launcher, and Settings share an identical visual language.

## Consequences
- UI widgets must never use literal color constructors (`Color(255, 0, 0)`) in production layout code; they must query `Theme::current().color(ColorToken::AccentActive)`.
