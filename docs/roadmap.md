# Roadmap

Ten milestones from empty repository to a maintained 1.0. Each has a single goal, prioritized work, measurable exit criteria and the documentation it must leave behind.

**Rule:** a milestone is complete only when every **P0** item is done, every exit criterion is met or has a written, approved exception in `docs/status.md`, and the docs listed for that milestone are updated.

## Priority and size definitions

| Label | Meaning |
| --- | --- |
| **P0** | Blocks the milestone; do not proceed without it. |
| **P1** | Expected for the milestone, may slip once with a recorded reason. |
| **P2** | Nice to have; defaults to `docs/backlog.md`. |
| **S** | ≤ 1 week of solo part-time work. |
| **M** | 1–3 weeks. |
| **L** | 3–6 weeks. |

Sizes are planning aids for a solo developer working part-time, not commitments. Re-estimate at each milestone boundary and record changes in `docs/status.md`.

**Sequencing:** M0 → M1 → M2 → M3 → M4 → M5 → M6 → M7 → M8 → M9. M3 and M4 may run in parallel after M2. M8 items are independent of each other and may be reordered by demand, but not started before M7's performance gate passes.

---

## M0 — Foundations and quality gates  ·  v0.1.0-dev  ·  size S

**Goal:** a boring, reproducible project that both platforms build identically, before any engine feature exists.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 0.1 | CMake project, presets, `Debug`/`Release`/`RelWithDebInfo`/`ASan` configurations | P0 | One preset per platform; no ad-hoc flags. |
| 0.2 | Directory skeleton from `AGENTS.md` §4, created only as needed | P0 | Empty-but-documented structure is fine. |
| 0.3 | Pin SDL3 and OpenGL loader versions; license + provenance notes in `third_party/` | P0 | See ADR-0002, ADR-0004. |
| 0.4 | Compiler warning set as errors (`/W4` MSVC; `-Wall -Wextra -Wpedantic` GCC/Clang) | P0 | CI and local builds. |
| 0.5 | clang-format (pinned version) and clang-tidy configs | P0 | Enforcement in CI. |
| 0.6 | CI: Windows + Linux, Debug/Release, build + tests + format/tidy check | P0 | Cache dependencies; target < 10 min cold. |
| 0.7 | Core utilities with tests: fixed-size math types, `Result`/error type, logging, assertions, tracking allocator | P0 | Small, stable, no cleverness. |
| 0.8 | Test harness (unit + integration + a `soak` label) with a single documented command | P0 | Framework choice recorded in an ADR. |
| 0.9 | `CHANGELOG.md`, version header, license decision | P0 | Changelog starts here. |
| 0.10 | Confirm/close proposed ADRs from `docs/decisions/` | P0 | Mark each Accepted/Rejected with a one-line reason. |

**Exit criteria**
- Clean clone builds and tests pass on Windows 11 and Ubuntu LTS with documented commands.
- Zero warnings; formatter and tidy checks gate CI.
- Tracking allocator reports zero leaks in the test suite (ASan/LSan clean on Linux, CRT leak check on Windows).
- Every proposed ADR is resolved.
- `AGENTS.md` §5 contains the real commands.

**Docs:** `AGENTS.md` build section, `docs/status.md`, all ADRs, `CHANGELOG.md`.

---

## M1 — Platform, window, loop and input  ·  v0.2.0-dev  ·  size S–M

**Goal:** a window that opens, clears to a color, handles input and exits cleanly on both platforms — with a measured, stable loop.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 1.1 | SDL3 initialization, window creation, GL 3.3 context, clean shutdown (incl. `SDL_EVENT_QUIT`) | P0 | All SDL calls behind `engine/platform`. |
| 1.2 | Frame timing: high-resolution clock, fixed-step accumulator, render interpolation hooks | P0 | Simulation step configurable; default 60 Hz. |
| 1.3 | Frame limiter and VSync options; runtime toggle for benchmarking | P0 | Never hard-block progress on VSync availability. |
| 1.4 | Input: keyboard, mouse position/buttons, wheel, gamepad hot-plug, action-mapping layer | P0 | Actions decoupled from scancodes; text input for later UI. |
| 1.5 | Window events: resize, minimize/restore, focus loss, DPI/scale changes, fullscreen toggle | P0 | Behavior documented per platform. |
| 1.6 | File system basics: UTF-8 paths, base path discovery, read/write directory creation, atomic file replace | P0 | Used by saves and assets later. |
| 1.7 | Logging sink to console + rotating file; crash-safe flush policy | P1 | No hot-path formatting cost when disabled. |
| 1.8 | Frame stats overlay (text-only, no font dependency yet) and CSV frame log | P1 | Needed for benchmark M1 gate. |
| 1.9 | Physical-VSync evidence run: compositor-free fullscreen, external capture, interval controls | P0 | Method below; opt-in `window_opengl_alternation_manual` supplies the mode, refresh read-back, timestamp log and a decodable frame marker; the capture stays manual. Archive in `benchmarks/results/`. |

**Exit criteria**
- Same binary behavior on Windows and Linux: window, resize, fullscreen, focus loss, clean exit.
- Input actions work for keyboard, mouse and gamepad; mapping is data-driven and documented.
- Resize to 0×0 (minimize) does not crash or spin.
- **Performance gates** (Tier A hardware, Release, VSync off, 60 s run after 5 s warm-up): empty frame CPU time **p99 ≤ 0.3 ms**, no frame > 2× target, private memory **≤ 60 MB**. Recorded in `benchmarks/`.
- Physical VSync verified by the evidence method below on at least one Tier A
  machine, or an approved exception recorded in `docs/status.md`.
- Clean shutdown with zero leaks and no driver warnings on both platforms.

**Physical-VSync evidence (unobserved)**

A composited window cannot show this. As recorded in `docs/status.md`, a
DWM-composited window is paced by the composition clock, which follows the
fastest active output, so interval-1 submissions describe the desktop rather
than the panel. Submission counts, `SDL_GL_GetSwapInterval` values, and the
absence of tearing in a windowed app are therefore not evidence, whatever they
report.

One run must supply all of the following:

1. **A display path outside the compositor.** Windows: exclusive fullscreen
   with an explicitly selected `SDL_DisplayMode`; the refresh is read back from
   `refresh_rate_numerator`/`refresh_rate_denominator` and, where available, the
   platform path, never from the integer `dmDisplayFrequency` alone.
   Linux: KMS/DRM, or an X11 fullscreen window with no compositor running; state
   which was used. VRR/FreeSync, Windows dynamic refresh and panel self-refresh
   are disabled or explicitly reported, because they decouple the panel rate
   from the mode.
2. **Presentation measured off the CPU.** A camera at ≥ 240 fps (phone slow
   motion is acceptable) filming a full-screen alternation, archived together
   with the app-side timestamp log for the same frames; or a photodiode or
   light sensor on the panel feeding an oscilloscope or audio input. The capture
   must resolve edges to better than one panel period. Driver-side counters
   (`WGL_OML_sync_control`/`glXGetSyncValuesOML` ust/msc/sbc, presentation
   statistics) are supporting evidence only, never the primary record.
3. **Interval controls in the same configuration and session.** Interval 1 at
   the panel rate, interval 0 clearly unthrottled, interval 2 halving the
   observed presentation rate. A rate that does not move with the interval is a
   limiter, not synchronization.
4. **A named witness and a platform record.** GPU, driver, display model,
   connection, OS build, exact command and environment variables, capture
   device and its frame rate, and the measured rates. Store the capture and log
   under `benchmarks/results/` and summarize the result, the observer and the
   date in `docs/status.md`.

Passing means the captured presentation cadence matches the reported mode within
the capture's resolution (target within 0.5 %, reproduced in two runs), no torn
frame or mid-frame edge appears in the capture, and the interval controls behave
as above. A machine that cannot provide exclusive fullscreen or a capture is an
approved exception written into `docs/status.md`, never a silent pass, and the
status stays "unverified" until a record of this kind exists.

**Docs:** `docs/architecture.md` platform section, input mapping format, benchmark record.

---

## M2 — 2D renderer core  ·  v0.3.0-dev  ·  size M–L

**Goal:** fast, correct sprite rendering with explicit budgets; the visual foundation everything else uses.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 2.1 | Renderer2D API: begin/end frame, camera, layers, blend modes, scissor | P0 | Stable surface; see API tiers. |
| 2.2 | Textured quads, sprite sheets, UV/flip/anchor/tint, opacity | P0 | Atlas-friendly. |
| 2.3 | Automatic batching by texture/state; single-vertex-buffer streaming with orphaning | P0 | Target: ≤ 100 draw calls at 10k sprites. |
| 2.4 | Shader pipeline for sprites, plus a documented way to register custom shaders | P0 | Shader source embedded, compiled at startup, errors are explicit. |
| 2.5 | Texture creation from decoded pixels, nearest/linear filtering, mipmaps optional, alpha handling | P0 | Correct color/premultiply rules documented. |
| 2.6 | Primitive drawing: lines, rects, circles, debug wireframes | P1 | Debug-only paths stay cheap when unused. |
| 2.7 | Resolution/DPI scaling, letterboxing, virtual resolution and integer scaling | P1 | Pixel-art correctness is a stated goal. |
| 2.8 | Render thread discipline: single-thread command submission, documented | P0 | No hidden threading. |
| 2.9 | GPU timing via timer queries (debug builds) | P2 | Useful once CPU cost is understood. |

**Exit criteria**
- 10,000 sprites from one atlas render correctly at 1080p with **CPU submit p99 ≤ 1.0 ms** and **≤ 100 draw calls** (Tier A, Release).
- Batching flush count and vertex counts exposed in the debug overlay and CSV.
- No per-frame heap allocations in the render path (tracking allocator over a 60 s run).
- Pixel-art test scene renders crisp at 1×, 2×, 3× and fractional scaling with no seams or bleeding.
- Visual regression: reference screenshot comparison for a fixed scene on both platforms, tolerance documented.

**Docs:** renderer API notes, coordinate/color conventions, benchmark record, any renderer ADRs.

---

## M3 — Assets, fonts and text  ·  v0.4.0-dev  ·  size M

**Goal:** drop art into a folder and use it by name, with clear errors and predictable memory.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 3.1 | Folder-based asset registry: virtual paths, stable string/ID handles, no raw pointer ownership | P0 | Handles survive reload. |
| 3.2 | Image loading (PNG at minimum; decide WebP/KTX2 via ADR) on a worker thread, upload on the render thread | P0 | Decode off the main thread. |
| 3.3 | JSON atlas/sprite-sheet descriptor format (frames, pivots, animation clips, nine-slice) | P0 | Versioned, schema documented, validator. |
| 3.4 | Font loading (TTF/OTF subset) and glyph atlas; text measurement, wrapping, UTF-8 | P0 | Localization-ready. |
| 3.5 | Asset memory accounting per type; configurable budgets and eviction for optional assets | P1 | Numbers visible in overlay and CSV. |
| 3.6 | Hot reload of changed assets in Debug builds | P1 | Watch folder; never in shipping builds. |
| 3.7 | Missing/corrupt asset policy: placeholder + structured error, never a crash | P0 | Error includes path, reason, expected format. |
| 3.8 | Content-addressed packing tool for release builds (optional at this stage) | P2 | Tools live in `/tools`. |

**Exit criteria**
- 512 MB of PNG assets load, decode and upload in **≤ 3 s** on Tier A (multi-threaded decode), with peak RSS documented.
- Missing, misnamed and corrupt files produce actionable errors and a placeholder, verified by tests.
- Text renders correctly for Latin, CJK and RTL sample strings given the chosen font; UTF-8 round-trips through all APIs.
- Hot reload updates a sprite and a font without restarting; no leaks across 100 reload cycles.
- Asset handles remain valid across reloads; stale handles are detectable.

**Docs:** asset folder contract, atlas/descriptor schema, font notes, benchmark record.

---

## M4 — Audio  ·  v0.5.0-dev  ·  size M

**Goal:** clean, low-latency sound: effects and streamed music, no clicks, no underruns under normal load.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 4.1 | Device layer via SDL audio (or documented alternative), device loss/reopen handling | P0 | Sample rate 48 kHz, stereo default; resampling documented. |
| 4.2 | Mixer: voices, gain, pan, pitch, looping, priority, voice stealing | P0 | Mixing in the audio callback with no locks/allocation. |
| 4.3 | WAV + Ogg Vorbis decode; SFX fully resident, music streamed with ring buffer | P0 | Decoders run off the callback thread. |
| 4.4 | Bus/group volumes (master/SFX/music), fade helpers | P1 | Persisted in config. |
| 4.5 | Voice cap and CPU budget enforcement; metrics for active voices and callback time | P0 | Overrun behavior documented (degrade, never block). |
| 4.6 | Latency configuration and platform notes (WASAPI shared, PulseAudio/PipeWire) | P1 | Documented defaults per OS. |
| 4.7 | Optional effects (low-pass, reverb) | P2 | Backlog unless a game needs it. |

**Exit criteria**
- **No underruns over a 30-minute soak** at 128 active voices on Tier A and Tier B.
- Mixer p99 **≤ 15 %** of the audio buffer duration (1024 frames @ 48 kHz ≈ 21.3 ms → ≤ 3.2 ms).
- Total output latency at default settings **≤ 50 ms** on both platforms, measured and documented.
- Music loop points are gapless; SFX start latency ≤ 1 mixer buffer.
- Device removal/reconnect does not crash or leak; state is restored.
- No allocation, locking or file I/O inside the audio callback (asserted in Debug).

**Docs:** audio settings, latency table per platform, benchmark record, decoder licensing notes.

---

## M5 — Scene, entities, animation and collision  ·  v0.6.0-dev  ·  size L

**Goal:** the everyday 2D game model: entities, transforms, sprites, animations, cameras and reliable collision.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 5.1 | Entity/component model with stable entity IDs (generation-counted handles) | P0 | No dangling references. |
| 5.2 | Transform hierarchy (2D), world/local, z-order/layers, sorting stability | P0 | Deterministic ordering. |
| 5.3 | Sprite and animation components; clip playback, events, per-frame timing | P0 | Ties to atlas clips from M3. |
| 5.4 | Camera2D: follow, smoothing, bounds, zoom, screen↔world conversions | P0 | Used by examples and tests. |
| 5.5 | Collision: AABB + circle, layers/masks, broadphase (spatial hash/grid), swept queries | P0 | Own implementation, no full physics dependency. |
| 5.6 | Simple kinematic resolution: move-and-slide with tile/AABB world | P1 | Deterministic; no angular dynamics. |
| 5.7 | Scene lifecycle: load/unload, serialization hooks for M6, scene transitions | P1 | Unload returns memory to baseline. |
| 5.8 | Determinism test: scripted inputs, fixed step, identical state hashes on both platforms | P0 | CI job on both OSes. |
| 5.9 | Trigger events and basic spatial queries for gameplay code | P1 | Documented iteration order. |
| 5.10 | Entity memory budget accounting (`sizeof` report + runtime counters) | P1 | Feeds M7 gate. |

**Exit criteria**
- 5,000 dynamic entities + 10,000 static colliders simulate at 60 Hz with **update p99 ≤ 3 ms** on Tier A.
- Determinism suite produces identical hashes for Windows and Linux Release builds over 10,000 steps.
- Scene load/unload 100× returns memory to baseline (±1 %); no handle reuse bugs (tested).
- Collision correctness tests: corners, fast-moving objects (no tunnelling within documented speed), layer masks.
- Base entity footprint **≤ 256 bytes** recorded; plan to reduce in M7 if over 128 bytes.

**Docs:** entity model, collision semantics, determinism rules, benchmark record.

---

## M6 — Persistence, diagnostics and memory hardening  ·  v0.7.0-dev  ·  size M

**Goal:** games that keep their data and fail loudly but safely.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 6.1 | Save system: versioned format, atomic write (temp + replace), backup slot, checksum | P0 | See ADR-0006. |
| 6.2 | Save migration framework: ordered migrations, tested from each old version | P0 | Never silently drop data. |
| 6.3 | Settings/config file (window, audio, input, video) separate from saves | P0 | Corrupt config → defaults + warning. |
| 6.4 | Crash handler: log flush, minidump/core note, safe-mode flag on next launch | P0 | Saves remain valid after `kill -9` during write (tested). |
| 6.5 | Assert/error policy enforced: no `assert` for recoverable runtime errors | P0 | Documented error-handling table. |
| 6.6 | Memory hardening: arenas, frame allocator, pools for frequent types; budget enforcement | P0 | Allocation counters per subsystem. |
| 6.7 | Leak/UB CI: ASan+UBSan (Linux), CRT/leak checks (Windows), fuzz corpus for asset parsers | P1 | Parsers must reject malformed input without crashing. |
| 6.8 | Structured diagnostics overlay: frame, memory, assets, audio, entities | P1 | Toggleable, near-zero cost when off. |
| 6.9 | Localization string tables (UTF-8), language switch at runtime | P1 | Complements font work from M3. |

**Exit criteria**
- Simulated power loss / `kill` during save: no corrupt or unreadable save; at worst the previous good save loads.
- Migration tests pass from every historical save version in the test corpus.
- 1 MB save **≤ 30 ms** p99; 10 MB save ≤ 300 ms, on Tier A.
- Malformed-asset fuzz run: 1M iterations, no crash, no leak, bounded memory.
- Crash handler produces a log + dump in **≤ 2 s**; next launch reports the previous crash once.
- Memory budgets enforced: exceeding a subsystem budget logs an error and fails a Debug assertion.

**Docs:** save format spec + compatibility promise, config schema, crash-reporting behavior, error-handling table.

---

## M7 — Example game and performance hardening  ·  v0.8.0-dev  ·  size L

**Goal:** prove the engine on a real, small 2D game and reach the full 1.0 performance targets — or publish honest revised targets.

The example game must exercise sprites, animation, camera, collision, audio, text, input (keyboard + gamepad), pauses, saves and scene transitions. Scope: a compact arcade-style game or short platformer; not a showcase.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 7.1 | Example game vertical slice (one complete level + menu + pause + save) | P0 | `/games/example`. |
| 7.2 | Profile and remove frame-path allocations, locks and redundant copies | P0 | Driven by measurements only. |
| 7.3 | Optimize batching, sorting, collision broadphase against recorded numbers | P0 | Keep changes justified and tested. |
| 7.4 | Asset pipeline: atlas packing + release packaging for the example game | P1 | Deterministic output. |
| 7.5 | Soak test: 24 h automated run on both platforms (headless where possible) | P0 | Memory flat, no crash, no audio underrun. |
| 7.6 | Input latency measurement and tuning | P1 | Target ≤ 2 frames at 60 Hz. |
| 7.7 | Startup time budget and measurement | P1 | See `docs/performance.md`. |
| 7.8 | Public performance report with hardware, methodology and raw logs | P0 | Honest, reproducible. |

**Exit criteria**
- All **1.0 performance targets** in `docs/performance.md` met on Tier A, or explicitly revised by ADR with rationale.
- Example game stable over 24 h soak on Windows and Linux; memory growth ≤ 1 % after warm-up.
- End-to-end input latency ≤ 2 frames at 60 Hz on both platforms.
- No per-frame allocations anywhere in engine or example game hot paths.
- Zero known crashes in the example game; all known issues triaged in `docs/status.md`.

**Docs:** example game README, performance report, any target revisions, updated roadmap gates.

---

## M8 — Extensibility: Lua, C ABI, tilemaps, particles, UI  ·  v0.9.0-dev  ·  size L

**Goal:** the "support a few languages" and modern-2D-feature layer, added only on top of a proven core.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 8.1 | Lua 5.4 embedding: engine module, hot-reloadable scripts, sandboxed error handling | P0 | Script errors never crash the engine. |
| 8.2 | Script API design: explicit context table, no raw pointers, documented ownership | P0 | Binding surface is versioned. |
| 8.3 | C ABI header covering lifetime, window, input, render, audio, assets, scene, saves | P0 | Opaque handles, no C++ types, ABI-version check. |
| 8.4 | Example binding or foreign-function test proving the C ABI is usable (C or Python via ctypes) | P1 | Not a supported binding, a proof. |
| 8.5 | Tilemaps: Tiled-compatible import or documented native format, layers, collision data, chunked rendering | P0 | Deterministic and batched. |
| 8.6 | Particle system: CPU-emitted, batched, budgeted, data-driven presets | P1 | Fixed particle cap. |
| 8.7 | In-game UI: immediate-mode widgets (label, button, slider, image, panel), keyboard/gamepad navigation | P1 | Debug overlay is separate. |
| 8.8 | Scripting + UI example integrated into the example game | P1 | Proves the API in real use. |
| 8.9 | C# / Python / other first-party bindings | P2 | Backlog; C ABI is the prerequisite. |

**Exit criteria**
- Lua scripts drive gameplay in the example game; a script error logs, disables the script and keeps the game running.
- C ABI: a non-C++ consumer creates a window, draws a sprite, plays audio and saves state; ABI mismatch is detected at startup.
- 10,000-tile map at 1080p renders within the M2 draw-call and CPU budgets.
- Particle system holds its cap without frame-time spikes (p99 within budget).
- UI works with keyboard and gamepad only; focus and navigation tested.
- No public API break without an ADR, changelog entry and deprecation warning.

**Docs:** scripting guide, C ABI reference, tilemap format, UI API notes, binding guidance.

---

## M9 — Release engineering and 1.0  ·  v1.0.0  ·  size M

**Goal:** something other people can adopt, upgrade and maintain.

| # | Item | Priority | Notes |
| --- | --- | --- | --- |
| 9.1 | Versioned installable SDK layout: headers, static libs, CMake package config, licenses | P0 | `find_package` works from a sample project. |
| 9.2 | Platform packaging: Windows zip/MSI, Linux tarball + `.desktop`; reproducible build notes | P0 | No runtime dependency surprises. |
| 9.3 | Project template (`peri new`) and documented game project layout | P1 | Minimal, no generator magic. |
| 9.4 | Complete docs pass: getting started, architecture, API reference, upgrade guide, FAQ | P0 | Docs are a 1.0 deliverable. |
| 9.5 | Compatibility promises published: API tiers, save/asset format, deprecation window | P0 | See ADR-0007. |
| 9.6 | Release checklist: clean-machine test on both OSes, fresh clone build, soak pass | P0 | Checklist lives in `docs/release-checklist.md`. |
| 9.7 | Maintenance process: issue triage labels, regression benchmarks on PRs, release cadence | P1 | Cadence decided at release. |
| 9.8 | 1.0 postmortem and next-cycle roadmap | P1 | Carries context forward. |

**Exit criteria**
- Fresh Windows and Linux machines build the example game from the installed SDK with no undocumented steps.
- All 1.0 quality gates in `docs/conventions.md` and `docs/performance.md` pass; results published.
- Every public symbol is classified stable/experimental/internal; undocumented public API is not allowed.
- Upgrade guide validated by migrating a game from the previous milestone's API.
- No P0 bug open; known issues documented with workarounds.
- `AGENTS.md`, `docs/status.md` and ADRs reviewed and consistent with the released code.

---

## Cross-milestone workstreams

These run continuously and are never "done":

| Workstream | Cadence | Owner behavior |
| --- | --- | --- |
| Benchmarks and regression tracking | Every milestone + any perf-touching PR | Record in `benchmarks/`, compare to baseline, block on unexplained regressions. |
| Documentation and ADRs | Same change as the code | No doc drift; ADR for cross-cutting choices. |
| Agent handoff | Every work session | `docs/status.md` refreshed; commands verified. |
| Cross-platform parity | Every feature | Both platforms build and run; differences documented. |
| Dependency hygiene | Each dependency addition/update | Pinned version, license, CVE check, update note. |
| Compatibility | All format/API changes | Version bump + migration + changelog. |

## Top risks

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Scope creep (editor, 3D, networking) | Never ships | Non-goals are explicit; new ideas go to the backlog; milestone gates. |
| Performance targets set before measurement | False confidence | Targets are M-gated and revisable by ADR with data. |
| Cross-platform graphics/audio differences | "Works on my machine" | Parity in CI from M0; measure both platforms each milestone. |
| Allocation discipline erodes | Memory and frame-time regressions | Tracking allocator, per-frame allocation tests, budgets in M6. |
| Public API churn | Breaks downstream games | Stability tiers, deprecation policy, example game as canary. |
| Thin agent context | Agents invent APIs or skip gates | `AGENTS.md` invariants, `status.md`, ADRs, one milestone at a time. |
| Solo-maintainer burnout / lost context | Project stalls | Small milestones, written handoff, no gold-plating. |

## Indicative timeline

Solo, part-time: M0–M2 ≈ 2–3 months; M3–M6 ≈ 2–3 months; M7 ≈ 1–2 months; M8–M9 ≈ 2–3 months. **1.0 in roughly 7–11 months of consistent part-time work.** Treat as an order of magnitude only; update after M2 using actual velocity.
