# ADR-0010: glad 2.0.8 as the pinned OpenGL 3.3 core loader

**Status:** Accepted (2026-11-19)
**Date:** 2026-11-19

## Context

ADR-0003 chose OpenGL 3.3 core and requires a single pinned loader. Windows
exposes only OpenGL 1.1 through the system headers, so every function above 1.1
must be resolved at runtime. Options considered:

- **glad 2.x**: generates exactly the requested API surface, no runtime init
  beyond `gladLoadGL`, MIT licensed, widely used.
- **GLEW 2.2**: mature and simple, but ships its whole API surface, needs
  `glewExperimental` handling for core profiles, and its CMake configuration
  performs an OpenGL package lookup at configure time.
- **gl3w**: generation tied to Python, smaller ecosystem, no advantage here.
- **Hand-written loader**: would mean maintaining ~120 function pointers and
  their typedefs by hand — exactly the code generation exists to avoid.

glad generates its sources with Python and Jinja2. That is a real build-time
dependency, and on this machine two Python installations exist, only one of
which has Jinja2. Making every configure and clean build depend on the "right"
Python is a fragility the project should not take on for a file set that never
changes between updates.

## Decision

- Pin **glad 2.0.8** (`v2.0.8`, archive SHA-256
  `44f06f9195427c7017f5028d0894f57eb216b0a8f7c4eda7ce883732aeb2d0fc`).
- Generate **GL 3.3 core with no extensions** once:
  `python -m glad --out-path third_party/glad --api gl:core=3.3 --extensions "" --reproducible c`.
- **Vendor the generated output** in `third_party/glad/` instead of generating at
  build time. The generator version, archive hash, exact command and license are
  recorded in `third_party/glad/README.md`.
- Expose it as the static target `peri::gl_loader`. M1 platform context creation
  and its tests link it to inspect actual driver context properties. Glad's
  generated function table is process-global: a rendering backend must load GL
  functions for its current context before use and after changing contexts.
- M1 platform work creates an opt-in SDL GL 3.3 core context, verifies its
  actual version/profile using glad and controls swap intervals. This corrects
  the earlier M2-only context sequencing, which conflicted with the M1 roadmap.
  ADR-0003 also permits opt-in, test-only clear/readback, visible-presentation and
  swap-interval comparison diagnostics; production rendering and clearing remain
  behind the rendering backend.
- Never hand-edit the generated files. Updates replace the directory wholesale
  and record the new version and hash.

## Consequences

- Builds need no Python, no Jinja2 and no extra fetch; behaviour is identical on
  both platforms and offline.
- About 3,000 lines of generated code live in the repository. It is isolated in
  one directory, excluded from the project's warning policy, and documented, so
  review burden stays small compared with writing and maintaining a loader.
- Extensions are not available until the directory is regenerated with the ones
  a feature needs; that is deliberate, since unused entry points are dead weight
  and dead surface to keep compatible.
- Regeneration is a deliberate, reviewable commit, in the same spirit as a
  dependency bump under ADR-0004.

## Revisit when

A required extension or a higher GL version is needed (regenerate with the
appropriate API and record it), or glad's output stops working with a supported
compiler/driver combination.
