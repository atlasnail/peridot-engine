# Status and handoff

This is the source of truth for current implementation and verification.

Last updated: 2026-11-25

## Current position

- **M0 is complete on the evidence recorded below.** clang-tidy is blocking in
  CI, the OpenGL loader is pinned (glad 2.0.8, ADR-0010), and the Windows CRT
  leak check now fails the test run instead of being skipped. The only caveat is
  what that check measures; see the limitation list.
- **M1 is in progress:** SDL now has an explicit, reference-counted lifecycle
  (`peri::SdlSession`), so shutdown is deterministic. The opt-in GL context
  and swap-control groundwork exists, but clear, desktop and hardware checks,
  and a real CPU-time measurement remain open.
- **Physical-VSync evidence:** two exclusive-fullscreen runs put interval 1 at
  59.94/s and 60.00/s against a read-back 60 Hz mode, with the 0 and 2 controls
  scaling as expected, but the interval-1 gap distribution differed between the
  runs and no capture exists, so physical VSync is unverified and M1 stays open.
- Version `0.1.0-dev`; implementation: core and SDL3 platform, tests and
  `bench_blank`. No game renderer or audio.
- The existing remote is `https://github.com/atlasnail/engine.git`. The rename
  passed Linux CI using SDL's dummy video driver, but has not been verified on
  a real Linux display or gamepad.

## Latest session: M1 alternation rerun

- Ran `window_opengl_alternation_manual` a second time on 2026-11-25 after asking
  first, on the same machine, build and display: Windows 11 Pro 10.0.26200, MSVC
  Debug, RX 9070 XT driver `26.11.1.261109`, `24N5C` read back as `60/1` =
  60.0000 Hz. CSV:
  `benchmarks/results/windows-alternation-2026-11-25-run2.csv`. Exit 0, 20
  checks, none failed; the desktop was confirmed back at 1920x1080.
- The interval rates reproduced: 59.94/s then 60.00/s at interval 1, 29.94/s then
  30.00/s at interval 2, with interval 0 unthrottled both times (3517.71/s then
  4845.53/s, which varies with machine load and is not a like-for-like number).
  Both interval-1 rates sit within 0.1 % of the read-back mode and interval 2
  lands on exactly half, so the interval controls behave the same way twice.
- The gap distribution did not reproduce. Run 1 had a 13.480 ms median with a
  26.765 ms p90, bimodal, averaging to 60/s. Run 2 is uniform: interval 1 at a
  16.664 ms median (p10 16.276 ms, p90 17.055 ms) and interval 2 at 33.333 ms
  (p10 33.063 ms, p90 33.590 ms). The burst-queue explanation recorded below is
  therefore an unreproduced hypothesis rather than a property of this path;
  run-to-run variation is at least as likely to come from the mode switch, the
  still-active 75 Hz output or ordinary scheduling.
- What two runs establish: in exclusive fullscreen the submission rate follows
  the selected mode instead of the 75 Hz composition clock, and the 0/1/2
  controls scale as expected, twice. What they do not establish: physical VSync.
  Neither run captured the screen or measured the panel. Run 2's uniformity is
  what a vblank-locked present path looks like, but submission timestamps cannot
  separate that from a driver limiter using the same mode rate. VRR state, the
  second output and a named witness are still unaddressed, M1 stays open and no
  exception has been requested.

## Previous session: M1 exclusive-fullscreen alternation diagnostic

- Ran the new `window_opengl_alternation_manual` diagnostic once on 2026-11-25
  after asking for approval, because it switches the window to exclusive
  fullscreen. Windows 11 Pro 10.0.26200, MSVC Debug, AMD Radeon RX 9070 XT,
  OpenGL 3.3 core driver `26.11.1.261109`, SDL `windows` video driver. Command:
  `PERI_TEST_OPENGL_ALTERNATION=1
  PERI_TEST_OPENGL_ALTERNATION_CSV=benchmarks/results/windows-alternation-2026-11-25.csv
  build/windows-debug/bin/peri_platform_tests.exe window_opengl_alternation_manual`.
  Exit 0, one case, 20 checks, none failed, no case skipped.
- The applied mode was `24N5C`, 1920x1080, read back as `60/1`, i.e. 60.0000 Hz
  exactly, which the rounded `refresh_rate` float cannot express. The window
  reported the same 1920x1080 pixels and stayed in exclusive mode through every
  phase. The run selected the desktop's own mode, so no resolution or refresh had
  to be undone, and the desktop was confirmed back at 1920x1080 afterwards.
- Per-interval submissions from the logged steady window (0.5 s warm-up, then
  4 s; `rows` are the CSV rows in that window):

| Interval | Rows | Rate | Median gap | p10 gap | p90 gap |
| --- | --- | --- | --- | --- | --- |
| 1 | 240 | 59.94/s | 13.480 ms | 12.890 ms | 26.765 ms |
| 0 | 14,070 | 3517.71/s | 0.250 ms | 0.235 ms | 0.336 ms |
| 2 | 120 | 29.94/s | 36.252 ms | 26.743 ms | 39.969 ms |

- With interval 1 the submission rate follows the selected 60 Hz mode (59.94/s,
  0.1 % under the read-back value) rather than the 74.8/s the windowed diagnostic
  measured on the same machine, interval 2 halves it to 29.94/s, and interval 0
  runs at 3517/s. The composition clamp recorded in the session below does not
  survive exclusive fullscreen, which is the first software-level evidence that
  the ~75 Hz pacing came from DWM composition rather than the driver.
- The interval-1 gaps in this run were not a smooth 16.667 ms cadence: the
  median was 13.480 ms with a 26.765 ms p90, a bimodal pattern averaging to the
  60 Hz rate. It did not reproduce in the rerun above, so it is recorded as a
  single-run observation; submission timestamps cannot say which frame was
  displayed when, and only a capture can.
- The CSV holds `interval,submission,wall_seconds,elapsed_seconds,background`,
  14,430 rows covering 3.999 s, 3.987 s and 3.974 s of steady time. Each phase's
  wall-clock span matched its steady span, so a camera recording can be aligned by
  `wall_seconds` (system clock) and matched frame by frame. The log is committed
  at `benchmarks/results/windows-alternation-2026-11-25.csv`; it is a submission
  log, not evidence of physical VSync, and no capture accompanies it.
- What this run does not establish: physical VSync. The panel's real scan rate
  was not measured, no tear line was checked, and nothing observed a displayed
  frame. The bimodal interval-1 gaps are consistent with both a queued but
  synchronized present path and a driver-pacing artifact. The other output was
  still active at 75 Hz, VRR state was not inspected, and only one run was made,
  not the two the method asks for. M1 remains open on both platforms and no
  exception has been requested.

## Previous session: M1 swap-interval diagnostic

- Added the separately opt-in, test-only `window_opengl_swap_interval_manual`.
  It compares requested intervals 0 and 1 in a visible window using a moving
  stripe made with scissored clears, a 0.5-second warm-up and four seconds of
  unrestricted submissions per phase. It reports set/query outcomes, the SDL
  display mode, submission rate and mean elapsed `Window::swap_buffers` duration
  (including context selection). It fails rather than reporting a comparison if
  either interval is unsupported or mismatched, the window closes or minimizes,
  or it moves between displays.
- Windows MSVC Debug build and the diagnostic passed at `9a15001` plus these
  uncommitted changes, on Windows 11 Pro 10.0.26200, SDL Windows, AMD Radeon RX
  9070 XT, OpenGL 3.3 core driver `26.11.1.261109`. SDL reported display `24N5C`,
  1920x1080 at 60 Hz. Interval 0 set and queried as 0: 47,285 submissions in
  4.000 s, 11,821.2 submissions/s, mean elapsed swap_buffers call 0.078 ms.
  Interval 1 set and queried as 1: 300 submissions in 4.010 s, 74.8 submissions/s,
  mean elapsed swap_buffers call 13.305 ms. These are a Debug diagnostic, not a
  performance gate; elapsed call time includes blocking and is not CPU time.
  Formatting passed and the diagnostic skipped without its opt-in under the
  dummy driver. No GPU-backed Linux desktop run was performed.
- The roughly 75 submissions/s against a reported 60 Hz mode was investigated on
  2026-11-25. The first checks were read-only and changed no display settings,
  source files or window placement. The existing binary reproduced it: `24N5C`
  reported at 60.000 Hz, interval 0 at 11,963.1 submissions/s (mean elapsed call
  0.077 ms), interval 1 at 74.8 submissions/s (mean 13.300 ms), both intervals
  set and queried as requested.
- Native Win32 queries show the desktop is not running at the window monitor's
  rate. The RX 9070 XT drives two active outputs, `\\.\DISPLAY1` at 1920x1080
  60 Hz (primary) and `\\.\DISPLAY2` at 1920x1080 75 Hz. `GetWindowRect` and
  `MonitorFromWindow` placed the diagnostic window at 632,269-1288,788, wholly
  inside `DISPLAY1`'s 0,0-1920,1080 bounds during both intervals, matching the
  display SDL reported.
- The composition clock runs at 75.0 Hz. Repeated `DwmFlush` waits, which return
  on the next DWM composition, gave 225 waits at 75.0 per second with a
  13.352 ms median gap (p10 12.401 ms, p90 14.244 ms). Interval-1 submissions
  and mean elapsed swap time (74.8/s, 13.300 ms) track that cadence rather than
  the 16.667 ms of a 60 Hz refresh. `DwmGetCompositionTimingInfo` returned
  `DWM_E_NOT_QUEUING_PRESENTS` here, so DWM's own `rateRefresh` was unavailable.
- This is consistent with windowed WGL interval-1 pacing following the DWM
  composition/present path instead of the window monitor's mode, which fits how
  mixed-refresh desktops compose at the highest active refresh
  ([Chromium's DWM note](https://issues.chromium.org/issues/40273080) reports
  `DwmGetCompositionTimingInfo` returning DWM's highest rate in such setups).
  Before the control below, that was correlation rather than proof: a single
  75 Hz observation cannot separate DWM queueing from a driver or control-panel
  VSync override.
- The single-output control was run after asking first, using
  `DisplaySwitch.exe /internal` (PC screen only) with `/extend` restored
  immediately afterwards. The desktop was confirmed back at two active outputs,
  a 60 Hz primary and a 75 Hz secondary, and composition returned to 75 Hz.
  With only the 60 Hz output active, `DwmFlush` measured 60.02 per second
  (16.655 ms median over 151 waits) and the same diagnostic reported interval 0
  at 9,289.7 submissions/s (mean elapsed call 0.089 ms) and interval 1 at 59.8
  submissions/s, 240 submissions in 4.010 s with a 16.625 ms mean. Interval 1
  tracked the composition clock in both topologies: 74.8/s against 74.97/s with
  two outputs, and 59.8/s against 60.02/s with only the 60 Hz output. The ~75 Hz
  pacing is therefore a desktop-composition clamp, not a 75 Hz panel and not
  synchronization to the window monitor's own mode.
- What this still does not establish: physical VSync. Submissions are not
  displayed frames, no screen or capture was observed, no tear line was checked
  and the panel's real scan rate was not measured. It also cannot separate DWM
  queueing from a driver that consults the same desktop refresh, does not show
  what pacing a window on the 75 Hz output would get, and does not rule out a
  control-panel VSync override; both outputs were never forced to one rate and
  VRR state was not inspected. Physical VSync and the Windows/Linux desktop
  gates remain open.
- The M1 physical-VSync method is now written into `docs/roadmap.md` as roadmap
  item 1.9 and an M1 exit criterion, so it is a defined gate rather than a vague
  note. One run must provide: a compositor-free display path (Windows exclusive
  fullscreen with a read-back path refresh; Linux KMS/DRM or uncomposited
  fullscreen) with VRR, dynamic refresh and panel self-refresh off or reported;
  presentation captured off the CPU by a camera at 240 fps or faster, or a panel
  photodiode, archived with the app-side frame log; interval 1/0/2 controls in
  the same session; and a named witness with the full platform record stored
  under `benchmarks/results/`. It passes only if the captured cadence matches
  the reported mode within 0.5 % across two runs with no torn frame.
- The first test-only pieces of that method now exist, behind
  `PERI_PLATFORM_TEST_HOOKS` so no production API changed: `platform::detail`
  gained `enter_test_exclusive_fullscreen` (selects an explicit `SDL_DisplayMode`
  on the window's display, refuses a borderless fallback or a mode without a
  precise refresh, and returns the applied mode's `refresh_rate_numerator` and
  `refresh_rate_denominator`), `leave_test_fullscreen`, and
  `set_test_swap_interval`. On top sits the opt-in
  `window_opengl_alternation_manual` diagnostic (`PERI_TEST_OPENGL_ALTERNATION=1`):
  it enters exclusive fullscreen at the desktop size, then for intervals 1, 0 and
  2 alternates the whole screen black and white four times a second for 4 s
  after a 0.5 s warm-up and writes `peri_alternation.csv`
  (`interval,submission,wall_seconds,elapsed_seconds,background`) so an external
  capture can be matched to submissions frame by frame. Interval 2 is skipped,
  not failed, when the driver does not accept it.
- `Window::toggle_fullscreen()` still means borderless desktop fullscreen, and no
  production path selects exclusive mode; the hook exists only for measurement.
- At that point the diagnostic had not been run, because it changes the display
  mode by design and that needed approval first; it has since been run once (see
  the session above). It builds under MSVC Debug, the full local suite passes and
  its case skips without the environment variable, and it is not enabled in CI.
  The windowed diagnostics still cannot satisfy the gate, since a DWM-composited
  window cannot tear, so physical VSync remained unverified on both platforms and
  no exception has been requested.
- CI on `b99eede` passed on Linux and Windows in
  [run 45222728304](https://github.com/atlasnail/peridot-engine/actions/runs/45222728304).
  Linux Debug, ASan+UBSan and Release builds and CTest passed, as did static
  analysis and the existing Xvfb/llvmpipe hidden-context/readback step. The new
  manual swap-interval diagnostic was not enabled in headless CI; this establishes
  compilation and headless coverage, not Linux interval behavior or physical VSync.
- To repeat from Git Bash, use `PERI_TEST_OPENGL_SWAP_INTERVAL=1
  build/windows-debug/bin/peri_platform_tests.exe window_opengl_swap_interval_manual`.
  The window title identifies each interval. Keep it on the selected display and
  unminimized. The diagnostic has no physical-VSync pass criterion: a
  DWM-composited window cannot tear, so confirmation is the separate method in
  `docs/roadmap.md` (M1 physical-VSync evidence) and M1 remains open. No M2
  renderer was added.

## Previous session: visible M1 GL diagnostic

- Added the separately opt-in, test-only `window_opengl_visible_clear_manual`.
  It creates a visible GL window and alternates red and green once per second,
  pumping events and swapping until closed or after ten seconds. It requires
  successful clears and swaps for both colors; closing too early or minimizing
  throughout can fail it. The diagnostic uses the fresh context's default
  framebuffer state and explicitly selects its back buffer. No production
  rendering API or M2 renderer was added.
- Windows MSVC Debug build and the diagnostic passed on the Windows SDL driver,
  AMD Radeon RX 9070 XT, GL 3.3 core, driver `26.11.1.261109`: 408 checks,
  zero skipped or failed. Formatting passed. Without the opt-in, the diagnostic
  skipped under SDL's dummy driver. This was an automated API-level run; no
  person confirmed the window's contents during that run. No GPU-backed Linux
  run was performed.
- At `b1ce709`, the user reported seeing the Windows diagnostic window alternate
  colors during a subsequent run of `PERI_TEST_OPENGL_VISIBLE=1
  build/windows-debug/bin/peri_platform_tests.exe window_opengl_visible_clear_manual`.
  That run reported the Windows SDL driver, AMD Radeon RX 9070 XT, GL 3.3 core,
  driver `26.11.1.261109`, and passed with 374 checks and no skips or failures.
  This is user-reported visible alternation plus an automated clear/swap result,
  not proof of physical VSync or Linux desktop behavior.
- CI on `89c1310` passed on Linux and Windows in
  [run 45220364019](https://github.com/atlasnail/peridot-engine/actions/runs/45220364019).
  Linux Debug, ASan+UBSan and Release builds and CTest passed, as did static
  analysis and the existing Xvfb/llvmpipe hidden-context/readback step. The new
  manual visible diagnostic was not enabled in headless CI, so the Linux result
  establishes compilation and headless checks, not visible presentation.
- For a human desktop check, run the following from Git Bash on Windows (or use
  the analogous `build/linux-debug/bin/peri_platform_tests` on Linux):

  ```bash
  PERI_TEST_OPENGL_VISIBLE=1 build/windows-debug/bin/peri_platform_tests.exe window_opengl_visible_clear_manual
  ```

  Observe whether the window visibly alternates red and green each second. Wait
  for both colors before closing, record the observation, OS/display backend,
  GPU/driver, command and commit, and do not treat the test's `ok` as human
  confirmation. Visible presentation on a GPU-backed Linux desktop remains
  unverified; M1 remains open.

## Previous session: M1 GL clear diagnostic

- Added `window_opengl_clear_readback`, an opt-in, test-only diagnostic under
  ADR-0003: it clears the default back buffer to two colors, reads two corner
  pixels for each, checks GL errors and swaps. This is not a production clear
  API or the M2 renderer. The Linux Xvfb/llvmpipe CI step now requires its
  explicit success line and its own renderer string to identify llvmpipe,
  along with the existing context tests. On test failure, the CI loop also
  prints captured stdout before exiting so diagnostics are not lost.
- Windows MSVC Debug build and `PERI_TEST_OPENGL=1
  build/windows-debug/bin/peri_platform_tests.exe window_opengl_clear_readback`
  passed on an AMD Radeon RX 9070 XT, GL 3.3 core (profile mask `0x1`):
  24 checks, no skips or failures. The same test with `SDL_VIDEODRIVER=dummy`
  and opt-in set failed as expected because dummy cannot create a GL window.
  Formatting and CI shell syntax checks passed. The test window was hidden:
  back-buffer readback and an accepted swap do not establish visible presentation
  or physical VSync.
- CI on `9f2201b` passed on Windows and Linux in
  [run 45218565438](https://github.com/atlasnail/peridot-engine/actions/runs/45218565438).
  The Linux Debug Xvfb/Mesa software-GL step succeeded; its fail-closed commands
  require the clear/readback test's success line and its own llvmpipe renderer
  string, as well as the two existing context test success lines. Individual
  job logs returned HTTP 403, so their context/version and pixel output were not
  inspected. Linux Debug, ASan+UBSan and Release CTest and static analysis also
  passed; the software-GL diagnostic ran in Debug only. WSL was unavailable
  locally. Software readback does not establish visible GPU-backed presentation,
  physical VSync or performance; M1 remains open.

## Previous session: Linux software OpenGL CI

- Added a separate Linux Debug CI step intended to run the existing M1 context
  smoke and forced-validation cleanup cases on Xvfb and Mesa llvmpipe. It forces
  SDL X11 and software GL, checks the renderer, and requires an explicit success
  line for each named test so a skip or unmatched filter cannot pass. The
  existing dummy-driver CTest runs remain unchanged. The CI-only system package
  pins and licenses are documented in `third_party/README.md` and ADR-0004.
- Review found a blocking package issue: Ubuntu's amd64 Noble updates archive
  reports `libgbm-dev`, `libgl1-mesa-dri` and `libglx-mesa0` version
  `25.2.8-0ubuntu0.24.04.4` superseded and removed; `.5` is published instead
  ([libgbm-dev history](https://launchpad.net/ubuntu/noble/amd64/libgbm-dev)).
  The three pins were updated together to `.5` and installed in CI run
  [45071444511](https://github.com/atlasnail/peridot-engine/actions/runs/45071444511).
  The workflow now resolves packages from Ubuntu snapshot `20261122T210000Z`
  instead of moving archives, with a guard requiring snapshot URLs for all three
  Ubuntu suites. CI on `077b718` passed the guard, install and software-GL step
  ([run 45072427353](https://github.com/atlasnail/peridot-engine/actions/runs/45072427353)).
  Individual logs were not accessible for inspection; success follows from the
  step status and its fail-closed commands. The runner image and already-installed
  transitive packages are not pinned; snapshot retention is finite.
- Workflow YAML and outer/inner shell syntax passed locally. Both named tests
  passed on the Windows real context through the success-line filter, which also
  rejected skipped, failed and unmatched dummy-driver cases. The full pipeline
  could not run on Git Bash because `tee /dev/stderr` failed; the filter was
  tested without `tee`. `glxinfo` verifies its own renderer, not directly the
  engine test's renderer. No engine code was changed.
- CI on branch commit `db39b9a` passed on Ubuntu 24.04 and Windows. The Linux
  Xvfb/llvmpipe step passed: its shell requires `glxinfo` to identify llvmpipe,
  both named test success lines and successful command exit statuses. Individual
  log lines were not available for inspection, so no context version is recorded.
  The software-GL cases ran in Debug only, not under the sanitizer configuration.
  WSL and other Linux runtimes were unavailable locally. This does not verify a
  physical GPU, real desktop, Wayland, actual VSync, GL clear or performance;
  M1 remains open.

## Previous session: M1 GL context groundwork

- M1 requires a GL context and VSync control; deferring the context to M2 was
  inconsistent with that sequencing. The platform now optionally requests an
  SDL GL 3.3 core context, checks and logs the driver's actual version/profile,
  owns its lifetime and supports runtime swap-interval requests, queries and
  swaps. Non-GL windows remain the default. Unsupported initial VSync requests
  warn without blocking creation; runtime failures are returned. SDL's dummy
  driver explicitly reports GL unavailable. ADR-0003 records the narrow
  context-validation exception to the no-platform-GL-call rule.
- Review exposed `make_gl_current()` so a caller can select the right context
  before drawing, not just when swapping. Windows MSVC Debug opt-in hidden-window
  smoke test queried the driver through the pinned glad loader: GL 3.3, core
  profile (mask `0x1`). It exercised two contexts, explicit and swap-triggered
  switching, destruction of a non-current window without disturbing the current
  context, and destruction of the current window. The test passed with
  `PERI_TEST_OPENGL=1 build/windows-debug/bin/peri_platform_tests.exe window_opengl_context_smoke`.
  A test-only profile override forces rejection after a real context is created;
  it checked window cleanup, no current context and successful recovery. The
  Windows platform CTest, dummy-driver unavailable path, formatting and local
  clang-tidy passed. The test override is compiled only when tests are enabled.
- CI on `742d5ad` passed on Windows and headless Linux (details below). Linux
  used the dummy driver; the real-context and forced post-creation cleanup tests
  are opt-in and did not run in that job. They passed later in software-GL CI
  on `db39b9a` as described above; run both on a GPU-backed Linux desktop as
  well. This does not prove GPU resources are leak-free, actual display
  synchronization or visible-window behavior. A GL
  clear remains unimplemented because ADR-0003 confines drawing to a rendering
  backend. No M1 gate is closed; no M2 renderer was started. No performance
  benchmark was run for this opt-in context path.

## Previous session: M1 action-mapping documentation

- Expanded the existing guide with the actual in-memory action binding contract,
  capacity and error behavior, fixed-step delivery and pause limitations. No
  saved mapping format or input behavior was added; the M1 input and hardware
  gates remain open.

## Previous session: M1 file-system path safety

- File operations now reject embedded NUL bytes in paths before accessing the
  filesystem, preventing truncated-path reads, overwrites or atomic-write cleanup.
  `exists` returns false for such paths. A regression test first reproduced the
  read bug. Windows MSVC Debug build, platform CTest and formatting passed locally;
  CI passed on both platforms as recorded below. Real-desktop, hardware and
  performance M1 gates remain open.

## Previous session: brand review and M1 focus-loss check

- Visually reviewed green, black and white mark and lockup variants on white,
  deep forest and stone backgrounds. The gem and wordmark align consistently;
  fine facets were difficult to distinguish at 32 px. The on-screen minimum
  is now 48 px. Print legibility has not been tested.
- Added an M1 synthetic focus-loss regression case: held keyboard and mouse
  buttons release, and their action releases are delivered on the next fixed
  step. Windows MSVC Debug build, platform CTest, the focused test using SDL's
  dummy video driver (21 checks) and formatting passed. Linux and real desktop
  focus behavior were not tested in this session. M1 remains open.

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
  XML; the subsequent visual review is recorded above. CI passed on both
  platforms for the rename as detailed below; Linux was headless. No milestone
  or performance gate is closed.

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

- **CI on `077b718` passed** ([run 45072427353](https://github.com/atlasnail/peridot-engine/actions/runs/45072427353)):
  snapshot URL guard and package install passed, as did Linux Debug,
  ASan+UBSan and Release builds/tests, formatting, blocking static analysis and
  the separate Debug software-GL check. Windows MSVC Debug/Release builds/tests
  and formatting passed. Individual log lines were not available for inspection;
  the software-GL step's success requires both named cases and the `glxinfo`
  llvmpipe check. Linux GPU-backed desktop and performance gates remain open.
- **CI on `db39b9a` passed** ([run 45071444511](https://github.com/atlasnail/peridot-engine/actions/runs/45071444511)):
  Linux GCC Debug, ASan+UBSan and Release build/test, formatting and blocking
  static analysis passed; Windows MSVC Debug/Release build/test and formatting
  passed. A separate Linux Debug Xvfb/Mesa software-GL step passed the renderer
  and both named test checks; the ordinary Linux CTest runs used SDL's dummy
  driver. The individual software-GL output was unavailable for inspection.
  This is not GPU-driver, real desktop or performance-gate evidence.
- **CI on `742d5ad` passed** ([run 45070217448](https://github.com/atlasnail/peridot-engine/actions/runs/45070217448)):
  Linux GCC Debug, ASan+UBSan and Release configure/build/test, formatting and
  blocking static analysis passed; Windows MSVC Debug and Release
  configure/build/test and formatting passed. Linux used SDL's dummy driver;
  the opt-in real-GL smoke and failure-cleanup cases did not run there. This is
  not Linux desktop, real-driver, VSync or performance-gate evidence.
- **CI on `c507a18` passed** ([run 45068658429](https://github.com/atlasnail/peridot-engine/actions/runs/45068658429)):
  Windows MSVC formatting and Debug/Release configure, build and test passed.
  Linux GCC formatting, Debug/ASan+UBSan/Release configure, build and test, and
  blocking static analysis passed. Linux used SDL's dummy video driver; this is
  not real-desktop, controller, hardware parity or performance-gate evidence.
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
4. M1 has an opt-in GL context, swap control and test-only clear/readback and
   visible-clear diagnostics, but no production clear backend. Version/profile
   validation passed on Windows and on Linux's software context in CI; that context's
   reported values were not inspected. The new clear diagnostic passed on Windows
   and in Linux software CI, not on a GPU-backed Linux context. A user observed
   alternating colors on Windows; the interval comparison reports API and
   host-side elapsed timing only. Linux visible presentation and physical VSync
   remain unverified. Minimized pause is implemented, but real desktop behavior
   and the 10-second idle-CPU gate are not verified.
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
  minimized behavior, a real CPU-time measurement, and a GL clear/VSync pass.
  Run the test-only GL clear/readback diagnostic on a GPU-backed Linux desktop.
  Have a person observe the visible-clear diagnostic on GPU-backed Linux; record
  context version/profile and renderer there.
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
