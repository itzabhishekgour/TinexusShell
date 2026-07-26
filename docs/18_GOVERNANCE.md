# Tinexus Platform — Monorepo Governance & Release Engineering

> **Document:** 18_GOVERNANCE.md  
> **Version:** 1.1.0  
> **Status:** FROZEN  
> **Classification:** Public — Open Source  
> **Depends on:** 04_FOLDER_STRUCTURE.md, 09_BUILD_SYSTEM.md

---

## Table of Contents

1. [Monorepo Branching Model](#1-monorepo-branching-model)
2. [Pull Request (PR) Requirements & Quality Gates](#2-pull-request-pr-requirements--quality-gates)
3. [Commit Message Standardization](#3-commit-message-standardization)
4. [Versioning & Tagging Protocol](#4-versioning--tagging-protocol)
5. [Release Engineering Checklist](#5-release-engineering-checklist)
6. [Contributor License Agreement (CLA) & Developer Certificate of Origin (DCO)](#6-contributor-license-agreement-cla--developer-certificate-of-origin-dco)

---

## 1. Monorepo Branching Model

```
main                ← Stable production releases (v1.0.0, v1.1.0). Always green.
  ▲
  │ (Release PR after milestone completion)
  │
develop             ← Active integration branch for v1.x development.
  ▲
  ├── feature/searchd-trigram-index
  ├── feature/serviced-watchdog
  ├── fix/compositor-vblank-lag
  └── refactor/ipcd-shm-allocator
```

- **Feature branches:** `feature/<component>-<slug>`
- **Bug fix branches:** `fix/<component>-<slug>`
- **Refactoring branches:** `refactor/<component>-<slug>`

---

## 2. Pull Request (PR) Requirements & Quality Gates

Every PR to `develop` or `main` MUST satisfy all 5 quality gates:

1. **Build Success:** Compiles clean on GCC 13 and Clang 16 with `-Wall -Wextra -Werror`.
2. **Test Success:** Unit, integration, and UI tests pass 100%.
3. **Coverage Requirement:** Minimum 80% line coverage maintained.
4. **Code Format & Linter:** Passes `clang-format-16` and `clang-tidy-16`.
5. **Architectural Approval:** Requires 2 reviewer approvals (including Lead Architect approval for IPC, API, or compositor changes).

---

## 3. Commit Message Standardization

Conventional Commits 1.0.0 specification:

```
<type>(<scope>): <short summary>

[optional body]

[optional footer]
```

### Examples:
- `feat(searchd): implement trigram score ranking pipeline`
- `fix(comp): resolve layer-shell input region passthrough bug`
- `docs(ipc): add shared memory protocol diagram`
- `perf(indexer): optimize trigram lookup using cache-hot arena`

---

## 4. Versioning & Tagging Protocol

- Semantic Versioning 2.0.0 (`MAJOR.MINOR.PATCH`).
- Canonical version stored in single root `VERSION` file.
- Tags created automatically on release merge: `git tag -a v1.0.0 -m "Release v1.0.0"`

---

## 5. Release Engineering Checklist

- [ ] All milestone issues closed.
- [ ] `CHANGELOG.md` updated with user-facing features & breaking changes.
- [ ] 72-hour stress test passed on T2 hardware.
- [ ] Security audit completed.
- [ ] Debian `.deb`, Arch `PKGBUILD`, and Flatpak packages built.

---

## 6. Developer Certificate of Origin (DCO)

All commits MUST include a DCO sign-off line (`git commit -s`):

```
Signed-off-by: Developer Name <developer@example.com>
```

Ensures GPL-2.0 / Apache-2.0 IP clarity.

---

*Document End: 18_GOVERNANCE.md*
