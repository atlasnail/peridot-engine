# Status and handoff

This is the source of truth for current implementation and verification.

Last updated: 2026-11-22

## Current position

- **M0 is complete on the evidence recorded below.** clang-tidy is blocking in
  CI, the OpenGL loader is pinned (glad 2.0.8, ADR-0010), and the Windows CRT
  leak check now fails the test run instead of being skipped. The only caveat is
  what that check measures; see the limitation list.
- **M1 is in progress:** SDL now has an explicit, reference-counted lifecycle
  (`peri::SdlSession`), so shutdown is deterministic. Window, input, loop and
  file-system work is done; the remaining M1 gates are the hardware and
  visible-window checks, and a real CPU-time measurement.
- Version `0.1.0-dev`; implementation: core and SDL3 platform, tests and
  `bench_blank`. No game renderer or audio.
- The existing remote is `https://github.com/atlasnail/engine.git`. The rename
  passed Linux CI using SDL's dummy video driver, but has not been verified on
  a real Linux display or gamepad.

## Latest session: Peridot Engine rename and brand

- Project name is Peridot Engine, shorthand `peri`; namespace, include paths,
  CMake targets/options, macros and test binaries use the new prefix. See
  ADR-0011 and `CHANGELOG.md` for the breaking migration. The repository URL,
  `engine/` source tree and historical evidence remain unchanged. The guide's
  Linux library name and the CMake cache migration instructions were corrected
  during review; `AGENTS.md` now matches the recorded M0/M1 status. The outlined
  wordmarks were aligned geometrically with the gem.
- `brand/` contains the faceted SVG mark, outlined wordmark variants, palette
  and the pinned, OFL-licensed Manrope font; see ADR-0011. The name and logo
  have not been legally cleared. Windows MSVC Debug configure/build and all
  three CTest entries passed locally; formatting passed. SVG files parsed as
  XML but were not visually reviewed. CI passed on both platforms for the rename
  as detailed below; Linux was headless. No milestone or performance gate is
  closed.

## Latest session: withhold Windows CPU percentiles

- `bench_blank` disables its Windows `GetThreadTimes` CPU sampling, prints that
  F1 CPU p99 is unavailable, and omits `cpu_ms` from Windows CSV output. Frame
  intervals remain separately reported. Linux CPU sampling and its CSV column
  are unchanged. The platform API still supports opt-in `GetThreadTimes`; this
  benchmark no longer presents those coarse values as a usable percentile.
- Windows MSVC 19.52 Release build and `ctest --preset windows-release -R
  bench_blank_output` passed. The new output test exercises paced and unlimited
  runs using SDL's dummy driver and checks the summary and CSV columns. The
  formatting check passed. Linux has not run these changes locally; see the CI
  result below. No F1 or M1 gate is closed.
- Developer measurement: Windows 11 26200, `dev-01` (Ryzen 9 9950X3D, RX
  9070 XT, 32 GB), MSVC 19.52 Release, hidden window, overlay and CSV off.
  `build/windows-release/bin/bench_blank.exe --seconds 5 --warmup 1` reported
  295 frames, interval p99 17.216 ms, 0 dropped steps, 5.5 MB exit private
  bytes. `--seconds 3 --warmup 1 --no-limit` reported 12,982,570 frames,
  interval p99 0.000 ms (1 µs buckets), 0 dropped steps, 5.7 MB. Both reported
  CPU p99 unavailable. These single short runs are not controlled comparisons,
  CPU measurements or Tier A gate evidence.

## Previous session: CI and Windows CPU-time investigation

- CI run [44764299141](https://github.com/atlasnail/engine/actions/runs/44764299141)
  for `92ccfc0` completed successfully. Linux GCC Debug, ASan+UBSan and Release
  builds/tests, formatting and blocking clang-tidy passed; Windows MSVC Debug
  and Release builds/tests and formatting passed. Linux used SDL's dummy video
  driver; this is not a Linux desktop or performance-gate run. No CI fix was
  needed.
- Investigated a finer Windows per-frame CPU timer. The reproducible 100-sample
  `QueryThreadCycleTime` probe on `dev-01` distinguished empty, 10 ms sleeping
  and 300 µs busy sections, but the values are cycles, not verified seconds.
  Microsoft explicitly warns against converting them to time. QPC alone is
  elapsed time and includes descheduling. See `docs/performance.md` and the
  developer benchmark record for sources, numbers and a candidate ETW
  scheduling-trace validation protocol.
- No ETW trace was captured: the current medium-integrity token was a member of
  Performance Log Users but did not list the user profile privilege. System
  tracing requires appropriate privilege/session access. No system permissions
  were changed, and ETW has not been proven to work on this machine. A
  frame-correlated, loss-checked trace and sleep/busy/preemption validation are
  still needed before adopting it. F1 and M1 remain open.

## Previous session: benchmark CPU instrumentation (M1 gate still open)

- `bench_blank` opts into main-thread CPU accounting around the event pump,
  fixed updates, render and optional stats display. The limiter, observer and
  CSV writes, other threads and GPU work are excluded. Linux uses
  `CLOCK_THREAD_CPUTIME_ID`; Windows uses `GetThreadTimes`. Clock failures stop
  the run. The optional CSV adds `cpu_ms` and includes warm-up frames.
- Two fixed-size 1 µs-bucket histograms cover every measured frame, with an
  overflow bucket at 100 ms; the first-200,000-frame truncation is gone. Clock
  overhead and histogram updates affect the benchmark. Detailed scope and
  quantization limits are in `docs/performance.md`.
- Windows 11 26200, `dev-01` (Ryzen 9 9950X3D, RX 9070 XT, 32 GB), MSVC
  19.52 Release, hidden 1280×720, overlay off: before instrumentation,
  `bench_blank.exe --seconds 5 --warmup 1` gave 295 samples, interval p99
  17.253 ms and 4.5 MB exit private bytes; `--seconds 3 --warmup 1 --no-limit`
  gave 200,000 capped samples and 8.4 MB. After instrumentation, paced with
  `--csv build/windows-release/cpu-after.csv` gave 294 samples, interval p99
  17.233 ms, CPU p99 0.000 ms and 6.5 MB; unlimited gave 5,744,481 samples,
  interval p99 0.000 ms, CPU p99 0.000 ms, CPU maximum 15.625 ms and 6.2 MB.
  CSV logging differs between the paced runs. These single short developer runs
  are not a regression assessment, a Tier A run, or F1 evidence: Windows
  CPU accounting was visibly quantized and per-frame p99 is misleading.
- Windows Debug and Release builds/CTest passed, including a sleep-versus-CPU
  test. Formatting passed; local clang-tidy passed with 22.1.3. The later
  Linux headless CI result is recorded above. A finer validated Windows
  measurement and Tier A runs remain necessary; do not close the gate.

## Latest session: minimized-window work (M1 gate still open)

- The application loop pauses updates, rendering and presentation while the
  window is minimized or a zero-pixel-size event is active. It continues to pump
  input and events, sleeping 50 ms between iterations even with the frame
  limiter disabled. On restore it resets the timestep and excludes paused time
  rather than running catch-up steps. Renderers must handle their own target
  size when drawing resumes; no GL render target exists yet.
- The previous test measured after joining the restore thread, so it could pass
  even when callbacks occurred before restoration. It now checks callback timing
  against the published restore event and measures the run before the join.
- Paused input previously filled the 128-transition action queue without fixed
  steps to consume it, risking overflow and stale input on resume. During pause
  and the restore pump, transient edges and frame deltas are discarded; actions
  reconcile to the latest held state, delivering at most one reconciliation edge
  at the next fixed step. Tests cover overflow-sized bursts, repeated paused
  pumps, overlapping bindings, and queued pre-pause taps.
- Windows MSVC Debug and Release builds and CTest pass for the fix, as do local
  formatting and clang-tidy. The synthetic event tests do not replace a real
  desktop minimize test; 10-second idle-CPU and Linux X11/Wayland verification
  remain open. Linux CI also passed the fix using the dummy video driver; see
  the verification record below.
- On `dev-01`, Windows 11 26200, MSVC 19.52 Release, hidden window, overlay off,
  `build/windows-release/bin/bench_blank.exe --seconds 5 --warmup 1 --csv build/windows-release/minimize-before.csv`
  and the same command with `minimize-after.csv` reported paced frame-interval
  p99 17.346 ms before and 17.255 ms after, with
  295 samples each, no dropped steps, and 4.4 / 4.3 MB exit private bytes.
  These single short wall-clock runs do not measure minimized CPU or establish a
  performance improvement, F1, or a Tier A gate; raw CSVs remain in the local
  build directory. After the input fix, the same command with
  `--csv build/windows-release/pause-input-after.csv` reported p99 17.187 ms,
  295 samples, zero dropped steps and 4.6 MB exit private bytes; this is also
  a single non-gated wall-clock run, not a minimized CPU measurement.

## Previous session: SDL lifecycle and the CRT leak gate

- **`eng::SdlSession`** (`engine/platform/include/eng/platform/sdl.hpp`) owns the
  process-wide SDL lifetime explicitly. The first `acquire()` initialises video
  and the gamepad subsystem; the last release closes the selected gamepad and
  calls `SDL_Quit`. It is move-only and reference counted, so an `App` and a test
  harness can both hold one without shutting SDL down early.
- `App` owns a session, so `App::create` still works on its own, and the session
  is declared before the window so it is destroyed after it. `Window::create`
  now fails with a clear error if no session is active instead of initialising
  SDL lazily and hoping nobody noticed.
- The lazy `std::call_once` initialisation and the `ErrorCode` global that went
  with it are gone: initialisation failure is returned to the caller that
  acquired the session.
- The test binaries now have separate entry points: `tests/main_core.cpp` for the
  core suite and `tests/platform/main_platform.cpp`, which holds one session for
  the whole process and releases it before the leak check reports.
- **Windows CRT leak gate** (`tests/leak_check.hpp`): both test executables
  compare CRT heap snapshots at the start and end of the run and fail with a
  non-zero exit code when blocks allocated during the run are still live. It is
  Debug-only and Windows-only; elsewhere it compiles to a no-op.
- Verified by hand: a deliberate 64-byte `malloc` in a temporary test case made
  the platform suite exit 1 and dump the block; removing it returned both
  suites to exit 0.

## Earlier session: input delivery and stats display

- Action edges are now queued from ordered button events and delivered one per
  fixed update, so a frame with no fixed step no longer loses a tap and a frame
  with several steps no longer repeats one. Keyboard, mouse and gamepad share
  the path; two taps in one pump produce two press/release pairs.
- Gamepad buttons now also arrive as events instead of only polling, so a quick
  tap between pumps is captured. Disconnect still releases held buttons.
- Queues are fixed size and report overflow instead of dropping events. `App::run`
  stops with a nonzero code if input or the stats display fails.
- `frame_stats_overlay` shows FPS, frame and update times, steps and dropped
  steps. A `StatsDisplay` callback lets a game renderer draw that text and
  present, instead of a second renderer clearing the window; without a callback
  the old standalone software renderer is used.
- Tests added for ordering, taps with zero or several steps, overlapping
  bindings, queue overflow, the callback after game rendering, and callback
  failure.

## Earlier M1 work in this branch

- MIT license (`LICENSE`, copyright Dylan Alexander); name stays `engine`/`eng`.
- Fixed-capacity named digital actions; opt-in rotating file log sink; standalone
  frame stats display; gamepad disconnect fix; non-finite timing rejected;
  directory creation on a file rejected; temporary file cleanup on write failure;
  corrected M0/benchmark claims; storefront integration planning in the backlog.
- `QA.md` holds the user's answers and is kept as-is.

## Verification

- **CI on `3e9a761` passed** ([run 44768627443](https://github.com/atlasnail/peridot-engine/actions/runs/44768627443)):
  Linux GCC Debug, ASan+UBSan and Release configure/build/test, formatting and
  blocking static analysis passed; Windows MSVC Debug and Release
  configure/build/test and formatting passed. Linux used SDL's dummy video
  driver; this is not real-desktop, hardware parity or performance-gate evidence.
- **CI on `980aad1` passed** ([run 44765609894](https://github.com/atlasnail/engine/actions/runs/44765609894)):
  Linux GCC Debug, ASan+UBSan and Release builds and CTest, formatting, and
  blocking clang-tidy passed; Windows MSVC Debug and Release builds, CTest and
  formatting passed. The workflow invokes unfiltered CTest in all configurations;
  `bench_blank_output` is registered by default and tests paced/unlimited
  summaries and CSV on SDL's dummy driver. The individual CTest log lines were
  not accessible, so their execution is inferred from the registration and
  successful CTest steps, not directly inspected. This is headless verification,
  not Linux desktop, hardware parity or Tier A/F1 evidence. No fix was needed.
- **CI on `f067dbb` passed** (run `44760210247`): Linux GCC Debug,
  ASan+UBSan and Release builds/tests, formatting and blocking clang-tidy all
  passed; Windows MSVC Debug and Release builds/tests and formatting passed.
  Linux used the dummy video driver, not a real desktop.
- **CI on `8dd8db0` is green** (run `44759373083`): Linux GCC and Windows
  MSVC jobs both completed successfully. Linux is headless and this run predates
  the pause/input fixes described above.
- **CI on `46384e1` is green** (run `44599648554`), as is the docs-only
  follow-up `3418537` (run `44600361030`). Windows (MSVC): format,
  Debug build/tests, Release build/tests. Linux (GCC 13, `ubuntu-24.04`):
  format, Debug build/tests, ASan+UBSan build/tests, Release build/tests,
  blocking clang-tidy. This run covers the SDL lifecycle: ASan+LSan on Linux
  passes with `SDL_Quit` actually executing, which is the first run where
  shutdown happens at all. Linux tests use `SDL_VIDEODRIVER=dummy`; window tests
  that need a real driver still skip.
- The CRT leak gate is Windows Debug only, so CI exercises it in the Windows
  Debug job and the Release jobs compile it out by design.
- Per-step log detail is not readable without repository admin rights, and the
  unauthenticated API hit its hourly limit mid-session; the gate above was read
  from the workflow badge and the public job page.
- Windows MSVC Debug and Release locally: build clean, CTest passes (2 suites,
  26 platform cases). `tools/check_format.py` and
  `tools/dev/ci_clang_tidy.sh build/windows-debug` both pass at tool version
  22.1.8 (installed from PyPI, same as CI).
- The leak gate was proved able to fail: adding a 64-byte `malloc` to a temporary
  test case made `eng_platform_tests` exit 1 and dump the block; removing it
  restored exit 0 for both binaries.
- Not verified: real Linux desktop (X11/Wayland) behavior, real controller
  hardware, a current local Clang build, visible-window appearance of the stats
  readout, and any Tier A performance gate.
- Release benchmark from the previous session (overlay, file log and actions
  disabled): `dev-01` paced p99 17.491 ms, 58.9 FPS, 0 dropped steps, 4.5 MB
  exit private bytes. One short run, not a gate; see
  `benchmarks/results/2026-11-19-bench-blank-review-win-x64-dev01.md`.
- Earlier benchmark records use elapsed frame-start intervals and the former
  first-200,000-sample cap; they do not establish F1.

## Open gates and known limitations

1. The Windows leak check compares CRT heap snapshots taken at the start and end
   of the test run and fails on blocks allocated during it that were never
   freed. It therefore covers everything the tests allocate, including SDL and
   the standard library, but it does not flag state that is alive for the whole
   process by design (the harness registries, CRT stdio buffers). The engine's
   tracking allocator still fails on engine allocations that are never released.
2. Linux verification of the rename was headless only. X11/Wayland window
   behavior, DPI, fullscreen and focus handling on a real desktop are untested.
3. M1 parity and hardware tests are incomplete: real gamepad, visible window,
   fullscreen, focus, DPI/minimize behavior, Linux display drivers, shutdown.
   Window tests skip when no video driver is available.
4. M1's GL context, clear and VSync need the later renderer; no approved
   roadmap exception is recorded. Minimized-window pause is implemented but
   real desktop behavior and the 10-second idle-CPU gate are not verified.
5. Fixed-step input keeps at most 128 pending changes per action and 256 raw
   button changes per pump, then fails explicitly during normal operation.
   Paused pumps instead discard transient edges and reconcile held state.
   Dropped fixed steps mean those simulation steps do not consume input; the
   queue is not a replay log.
   Mouse movement and wheel remain frame-scoped. SDL and the selected gamepad
   are process-global, but their lifetime is now explicit: `peri::SdlSession`
   initialises on the first acquire and calls `SDL_Quit` on the last release.
6. Per-frame tracked-allocation claims do not cover ordinary STL/SDL allocation;
   no comprehensive allocation audit was done. CSV logging uses a stream.
7. Atomic replacement is not power-loss durable: file and directory sync are
   not implemented. A single writer per path is assumed.
8. Tier A hardware and the 60-second gate are unavailable. Windows' current
   per-thread CPU accounting is too coarse for short frames; Linux's clock is
   covered by headless CI but has not had a performance-gate run. Neither
   platform can claim F1 on current evidence.
9. `Result<T>` error messages are non-owning. Existing code may still allow
   allocation/standard-library exceptions across boundaries.

## Next steps

- Remaining M1 gates: visible-window checks on both platforms, real gamepad,
  minimized behavior, a real CPU-time measurement, and the GL clear/VSync pass
  through a real context.
- Do not start the renderer (M2) or add store SDKs until sequencing is agreed.

## Gates closed in an earlier session

- **M0 is closed.** With the CRT leak check wired into both test entry points,
  every M0 exit criterion now has evidence on Windows locally and on Linux in
  CI, except the caveat about what the Windows check covers (limitation 1).
- **clang-tidy is blocking.** The advisory step is gone: CI installs clang-tidy
  22.1.8 from PyPI (same version as clang-format, so local and CI findings
  match) and runs `tools/dev/ci_clang_tidy.sh` over all engine sources with
  `--warnings-as-errors=*`. The header filter now covers `engine/.*`, previously
  excluding `engine/platform` headers. It is clean today; findings are annotated.
- **OpenGL loader pinned.** glad 2.0.8, GL 3.3 core, no extensions, generated
  once and vendored in `third_party/glad/` (ADR-0010). Now builds as
  `peri::gl_loader`; it needs no Python or network. The root project enables
  the C language because this loader and SDL are C.

## CI fixes found by the first Linux run

- SDL 3.4.20 treats XTest as required with X11; `libxtst-dev` was missing, so
  configure failed. The dependency list also gained pkg-config, xkbcommon-x11,
  libdecor, libdrm, libgbm and libudev.
- Linux Release failed with a GCC 13 `-Wnull-dereference` false positive inside
  libstdc++ triggered by `istreambuf_iterator`. `read_text_file` now sizes and
  reads the file directly instead of suppressing the warning.
- `tools/dev/ci_configure.sh` and `tools/dev/ci_build.sh` turn configure and
  compiler failures into annotations, because job logs need admin access.

## Historical context

Prior Windows tests covered 30 core / 186 checks and 10 platform / 43 checks.
SDL 3.4.20 is pinned by URL and SHA-256. The earlier developer-machine baseline
is `benchmarks/results/2026-11-19-bench_blank-win-x64-dev01.md`, corrected to
identify its elapsed-time limitation. No Linux or Tier A result is implied.
