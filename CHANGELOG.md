# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). While the version is `0.x`, minor
releases may contain breaking changes, and they are always described below.

## [Unreleased]

### Added

- Peridot Engine brand kit: faceted line-art logo and outlined wordmarks, palette,
  usage guidance and Manrope 4.505 under the SIL OFL 1.1; see `brand/`.
- `peri::SdlSession`: an explicit, reference-counted SDL lifecycle in
  `engine/platform`. The first acquire initialises video and gamepads, the last
  release closes devices and calls `SDL_Quit`; `App` owns one, and
  `Window::create` now requires an active session. Replaces lazy `call_once`
  initialisation that was never undone.
- Windows CRT leak gate in both test executables: blocks allocated during a run
  that are still live at the end fail the suite and are dumped to stderr.
- glad 2.0.8 as the pinned OpenGL 3.3 core loader (`peri::gl_loader`), generated
  once and vendored; see ADR-0010. Nothing links it until the M2 renderer.
- MIT license, selected in `QA.md`.
- Fixed-capacity named digital action bindings for keyboard, mouse and gamepad;
  opt-in rotating file/console log sink and a frame stats display.
- `StatsDisplay` callback so the frame readout can be drawn by the game renderer
  instead of a separate software renderer. Supplying the callback enables the
  readout by itself.
- Planning notes for optional Steam and GOG integrations without introducing SDKs.
- SDL 3.4.20 pinned by URL and SHA-256 and built statically inside `engine/platform`.
- `peri_platform`:
  - window creation, OS event pump, resize/focus/minimize/fullscreen handling;
  - engine-owned keyboard, mouse and gamepad state with pressed/released edges;
  - fixed-step application loop with catch-up cap, interpolation alpha, optional
    FPS limiter and per-frame CSV logging;
  - UTF-8 file helpers with atomic replace, directory creation and executable
    directory lookup.
- Fixed-step timing utilities in `peri_core`.
- `bench_blank` benchmark driver with warm-up, sample cap, pacing/unlimited modes
  and process-memory reporting.
- Platform test suite (window, file system, app loop).

### Changed

- **Breaking rename:** `eng::` becomes `peri::`, includes `eng/…` become
  `peri/…`, `eng_*` CMake targets/test executables become `peri_*`, aliases
  `eng::…` become `peri::…`, and `ENG_*` / `ENGINE_VERSION_*` macros and
  CMake options become `PERI_*` / `PERI_VERSION_*`. Update downstream code and
  scripts. Old CMake cache options are not migrated: run
  `cmake --fresh --preset <preset>` and reapply any custom overrides under
  their new names (for example, `-DPERI_SANITIZE=ON`). Existing build directories may
  also contain old generated headers and binaries; use a new build directory
  if those stale artifacts would cause confusion. Old aliases are not retained.
  The next 0.x minor release must carry this migration note. No stable
  `engine/public/` or `engine/c_api/` API exists yet (ADR-0007/0011).
- The default window title is `Peridot Engine`; the CMake project name is
  `peridot_engine`. The `engine/` source directory and existing repository URL
  do not change.
- `bench_blank` reports elapsed frame intervals and, on Linux, main-thread CPU
  time using bounded histograms covering all measured frames. Linux CSV adds
  `cpu_ms`. Windows reports F1 CPU p99 unavailable and omits CPU samples from
  its CSV because its current clock is not validated for short frames.
- The first CMake configure now downloads SDL; offline builds reuse
  `build/<preset>/_deps/`.
- clang-tidy is a blocking CI check over all engine sources, pinned to 22.1.8
  together with clang-format. `.clang-tidy` now also covers `engine/platform`
  headers.
- The root project enables C alongside C++ because SDL and the GL loader are C
  libraries built inside the project.

### Fixed

- File-system operations reject embedded NUL bytes in paths rather than acting
  on a truncated path. `exists` returns false for such paths.
- Linux CI: added the XTest headers SDL requires, and replaced the whole-file
  read that tripped a GCC 13 null-dereference false positive at -O3.
- Action edges were lost when a frame ran no fixed update and repeated when a
  frame ran several. Button changes are now queued and delivered one per fixed
  update, in order. Gamepad buttons are event-driven, so quick taps are no
  longer missed between polls.
- Gamepad disconnect now reports held-button releases; stick readings are clamped.
- Reject non-finite app timing values and correct milestone/benchmark claims.

### Notes

- OpenGL context creation moved to M2 with the renderer so headless window tests
  do not require a GPU.
- Input queues are fixed size (256 raw button changes per pump, 128 pending
  changes per action) and stop the loop with an error on overflow rather than
  dropping events.
- Baseline numbers are from `dev-01`, not Tier A hardware; see
  `benchmarks/results/2026-11-19-bench_blank-win-x64-dev01.md`.

## [0.1.0-dev] - 2026-11-19

First implementation milestone (M0: foundations). No rendering, input or audio
yet; this is the dependency-free core the rest of the engine builds on.

### Added

- CMake project with per-platform presets (`windows-*`, `linux-*`, `linux-asan`,
  `windows-clang-debug`), warnings-as-errors, and `check` target.
- `eng_core` static library with:
  - fixed-width integer aliases and 2D math (`Vec2`, `IVec2`, `Rect`, `Color`);
  - `Result<T>` / `Status` error values, with no exceptions across boundaries;
  - thread-safe logging with replaceable sinks, fixed-buffer formatting and
    truncation markers;
  - tracked allocations with per-subsystem tags, peak/current counters and leak
    counting;
  - monotonic time and a `Stopwatch`.
- Self-contained test harness and 25 test cases covering math, results, logging,
  memory tracking, time and versioning.
- Windows developer environment helper (`tools/dev/msvc_env.bat`) that locates
  Visual Studio, enters the MSVC x64 environment and adds CMake/Ninja/LLVM.
- Formatting check (`tools/check_format.py`) and clang-tidy configuration.
- GitHub Actions CI for Windows (MSVC Debug/Release) and Linux (GCC
  Debug/Release, ASan + UBSan, formatting, advisory clang-tidy).

### Fixed

- `time::now_seconds()` could return a small negative value on its first call
  because the epoch and the current time were read in one expression with
  unspecified evaluation order.
- Over-aligned allocations now use `std::align` pointer arithmetic instead of
  integer-to-pointer casts.

### Notes

- Linux builds are configured in CI but have not been run locally on this
  machine (no WSL/Docker/native Linux toolchain available). See
  `docs/status.md`.
- At that point the project name and license were open; see the Unreleased entry for the later decision.
