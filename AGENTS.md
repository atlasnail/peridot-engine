# AGENTS.md — working on this repository

This file is for AI agents and new human contributors. It is deliberately short, factual and stable. If reality and this file disagree, fix the file in the same change and note it in `docs/status.md`.

## 1. What this project is

**Peridot Engine** (`peri`) is a from-scratch, native, **2D-only** game engine for **Windows and Linux**, written in **C++20**, with Lua scripting and a C ABI for other language bindings. Priorities, in order: correctness and clarity, low memory use, high and predictable frame rate, cross-platform parity, long-term maintainability.

**Current phase: foundations and platform work.** M0 was completed on the evidence recorded in `docs/status.md`; M1 remains open until its exit criteria are verified on both platforms. Implemented facts belong in `docs/status.md`; everything else is a plan.

## 2. Read order

1. `README.md` — scope and non-goals.
2. `docs/status.md` — current milestone, next tasks, known issues, last benchmark.
3. `docs/roadmap.md` — the milestone you are working in and its exit criteria.
4. `docs/architecture.md` — module boundaries and dependency rules.
5. `docs/conventions.md` — standards and definition of done.
6. `docs/decisions/` — ADRs before changing a cross-cutting choice.
7. `docs/performance.md` — before touching anything on the frame-time or memory path.

## 3. Hard rules

1. **One milestone at a time.** Do not start work from a later milestone while an earlier one has unmet exit criteria, unless the roadmap explicitly marks the item as parallel.
2. **Do not add scope.** New ideas go to `docs/backlog.md`. Non-goals in `README.md` stay non-goals until an ADR changes them.
3. **No performance claims without measurements.** State hardware, build type, command and result, or say "not measured".
4. **No new third-party dependency without an ADR.** Pin exact versions; record license and maintenance status.
5. **Compatibility formats are contracts.** Save file, asset descriptor and project file formats change only with a version bump, a migration path and a changelog entry.
6. **Public API stability tiers are binding.** `engine/public/` and `engine/c_api/` are semver-protected. Anything marked `internal` or `experimental` may change.
7. **Platform isolation.** OS-specific headers, path rules and windowing calls live only in `engine/platform/`. Public headers must compile on both target platforms without platform conditionals beyond `#if defined(_WIN32)` feature checks.
8. **No steady-state allocation in the frame loop.** Allocations during `update`/`render` must go through frame or pool allocators. Verified with the tracking allocator.
9. **Determinism matters.** Fixed-step simulation must produce identical results for identical inputs on both platforms for the tested suite. Do not introduce unordered iteration into simulation-affecting code.
10. **Keep the handoff current.** Any change to milestone state, targets, commands or open questions updates `docs/status.md`.
11. **No silent failures.** Every fallible engine API returns an error value or a documented result type; failures are logged with context.
12. **Do not commit generated build output, assets you do not have rights to, or secrets.** Game projects and engine stay separate directories.

## 4. Planned layout

Target structure; create directories only when a milestone needs them.

```
/AGENTS.md              this file
/README.md              scope, non-goals, document map
/docs/                  roadmap, architecture, performance, conventions, status, ADRs
/brand/                 logo, typography, palette and usage guidelines
/engine/
  /public/              stable C++ headers (installed)
  /c_api/               stable C ABI header (installed)
  /core/                math, containers, allocators, logging, time, job system
  /platform/            window, input, files, threads, clocks (SDL3 + OS specifics)
  /render/              OpenGL 3.3 backend, batching, textures, camera, sprites, text
  /audio/               mixer, decoder, device, streaming
  /assets/              asset registry, loaders, atlases, hot reload
  /scene/               entities, components, systems, collision, world
  /ui/                  in-game UI widgets and debug overlay
  /script/              Lua bindings (M8)
/games/example/         reference game and compatibility canary
/tests/                 unit, integration, determinism, soak tests
/benchmarks/            benchmark drivers and recorded results
/cmake/                 build helpers, presets, toolchain notes
/third_party/           pinned vendored/built dependencies + license notes
/tools/                 asset packing, migration and dev utilities
```

## 5. Build and test commands

Windows Debug is verified locally with MSVC 19.52 after the Peridot rename.
CI also passed on Windows (MSVC Debug/Release) and headless Linux (GCC 13,
Debug/Release/ASan+UBSan). The Windows Clang 22.1.3 result predates the rename;
Linux has not run on this local machine. See `docs/status.md` for details.

**Windows, Developer PowerShell for VS 2026:**

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Release: `windows-release`. Second compiler: `windows-clang-debug`.

**Windows, any shell (Git Bash):**

```bash
cmd //c 'tools\dev\msvc_env.bat cmake --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat cmake --build --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat ctest --preset windows-debug'
```

**Linux:**

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Sanitizers: `linux-asan`. Formatting: `python tools/check_format.py`
(`--fix` rewrites); run it through `tools\dev\msvc_env.bat` on Windows so
a clang-format is found. CI uses pinned PyPI builds, currently 22.1.8 for both
clang-format and clang-tidy.

Static analysis (blocking in CI, engine sources only):

```bash
bash tools/dev/ci_clang_tidy.sh build/windows-debug
bash tools/dev/ci_clang_tidy.sh build/linux-debug
```

Benchmark (Release only, after building `windows-release`):

```bash
build/windows-release/bin/bench_blank.exe --seconds 5 --warmup 1
```

Read `docs/performance.md` before quoting a number; record every gated run in
`benchmarks/results/`.

The first configure downloads SDL 3.4.20 (pinned in `third_party/`). After that,
`build/<preset>/_deps/` is reused. A configure without network fails; use a
populated dependency cache or a local archive override.

Artifacts: `build/<preset>/bin/peri_tests(.exe)`,
`build/<preset>/bin/peri_platform_tests(.exe)`, `bench_blank(.exe)`; libraries in
`build/<preset>/lib/`. Test executables accept a substring filter
(`peri_tests.exe memory`).

Keep this section, `README.md` and `docs/status.md` consistent when commands or
presets change.

## 6. Change workflow

1. Read `docs/status.md`; identify the milestone and one task.
2. Check ADRs and invariants that apply.
3. Make the smallest change that satisfies the task.
4. Build and test on **both** platforms, or state clearly which platform was not exercised. A Windows-only result is not cross-platform proof.
5. Run the relevant benchmark if the change affects frame time, memory, load time or audio; record the result.
6. Update the affected docs: `status.md` always; roadmap if scope/exit criteria moved; ADR if a cross-cutting choice changed; conventions if a new rule is needed.
7. Report honestly: what was verified, what was not, and what remains.

## 7. Definition of done (summary)

- Builds on Windows and Linux with zero warnings under the project's warning set.
- Tests for new logic pass; determinism and sanitizer suites pass where applicable.
- No unexplained regression in the benchmark suite.
- Public API changes documented and versioned; save/asset format changes versioned and migrated.
- `docs/status.md` updated; committed docs match the code.

Full checklist: `docs/conventions.md`.

## 8. Common agent failure modes to avoid

- Inventing APIs, files or metrics that do not exist yet.
- Starting a later milestone because it is more interesting.
- Adding a dependency, custom allocator or abstraction "just in case".
- Optimizing without a recorded baseline.
- Treating a single-platform build as cross-platform verification.
- Editing generated files or vendored dependencies instead of pinning an upstream version.
- Rewriting stable public API or formats without deprecation and migration.
- Letting `docs/` drift from reality.
