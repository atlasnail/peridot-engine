# ADR-0004: CMake presets and pinned dependencies

**Status:** Accepted (2026-11-19)
**Date:** 2026-11-19

## Context

The project must build identically on Windows (MSVC) and Linux (GCC/Clang), support Debug/Release/ASan, fetch a handful of C dependencies, and be reproducible for agents who cannot rely on machine-specific setup. Options: raw CMake with per-developer flags, a custom build script, or a meta-build system.

CMake is already installed with the Visual Studio C++ workload, is the norm for C/C++ engines, and supports both toolchains and CI cleanly.

## Decision

- **CMake ≥ 3.25** with **CMake Presets** (`CMakePresets.json`), one configure preset per platform/configuration; no undocumented flags.
- Configurations: `debug`, `relwithdebinfo`, `release`, plus `asan` (Linux) and leak-checked MSVC debug.
- Dependencies pinned by exact version and integrity hash, fetched or vendored via `third_party/` with a `README` recording origin, license and update procedure. No floating branches; updates are explicit commits.
- Build settings that affect behavior (exceptions, RTTI, float contraction, LTO) are set in presets, not ad hoc.
- For M1 CI only, use Ubuntu 24.04's version-pinned Xvfb, xauth, Mesa, Mesa
  utilities and GLVND packages to test SDL's X11/GLX path with software GL.
  These system tools are not vendored or shipped as engine dependencies; their
  package versions, upstreams and licenses are recorded in `third_party/README.md`.
  Resolve CI packages against an isolated, dated Ubuntu archive snapshot rather
  than the moving Ubuntu archives. Record the snapshot ID with the pins and
  update them together in a reviewed change. The hosted image and already
  installed system packages are not pinned; Ubuntu's snapshot retention is
  finite. This check does not replace a real desktop or hardware-driver test.
- The installed SDK ships a CMake package config so games can `find_package`.

## Consequences

- One command per platform; CI can mirror it exactly.
- Presets are duplicated per platform by design; drift between them must be checked in review.
- Offline builds require a populated dependency cache or vendored sources; document the workflow before M0 ends.
- Updating a dependency is a visible, reviewable change with a benchmark check.

## Revisit when

CMake cannot express a required build capability, or the dependency-fetching approach breaks reproducibility/offline builds in practice.
