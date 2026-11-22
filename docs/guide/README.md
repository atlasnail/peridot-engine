# Start here: how this project works

This guide is for someone who wants to understand the code without being a C++
expert. It explains what exists today (M0 foundations and M1 work), how to build it, what each
part does, and the C++ ideas the code uses. It is updated as the engine grows.

If you only read one section, read [Build and run](#build-and-run) and
[The 60-second mental model](#the-60-second-mental-model).

---

## What exists today

A basic game window and loop exist. The Peridot rename passed headless Linux
CI; real Linux desktop and M1 hardware gates remain open:

| Piece | What it does | Where |
| --- | --- | --- |
| Build system | Turns source files into libraries, tests and a benchmark | `CMakeLists.txt`, `CMakePresets.json`, `cmake/` |
| `peri_core` | Math, errors, logging, memory tracking, time, fixed-step timing | `engine/core/` |
| `peri_platform` | Window, OS events, input, application loop, file system; wraps SDL | `engine/platform/` |
| Tests | Core and platform test cases, run explicitly via CTest | `tests/` |
| Benchmark | `bench_blank`: measures an empty frame | `benchmarks/` |
| Tooling | Formatting, static analysis, CI | `tools/`, `.clang-format`, `.clang-tidy`, `.github/workflows/ci.yml` |
| Documentation | Plan, architecture, rules, decisions, this guide | `docs/`, `AGENTS.md`, `README.md` |

The window, loop and input exist; an optional diagnostic text display can draw
its own frame. The game renderer remains planned for M2.

---

## The 60-second mental model

1. You write **source files** (`.cpp`) and **headers** (`.hpp`).
2. A **compiler** (MSVC on Windows, GCC/Clang on Linux) turns each `.cpp` into an
   object file. It reads headers to learn what exists in other files.
3. A **linker** combines object files into either:
   - a **static library** — a bag of compiled code (`peri_core.lib` / `libperi_core.a`), or
   - an **executable** — a program with a `main()` (`peri_tests.exe`).
4. **CMake** does not compile anything itself. It reads `CMakeLists.txt` and
   generates build files (here: Ninja files) that tell Ninja how to call the
   compiler and linker in the right order.
5. **CTest** runs the test executable and reports pass/fail.

There is no `.sln` checked in and no IDE project file to maintain: the build is
described once in CMake and works on both platforms.

---

## Build and run

### Windows, easiest path

Open **Developer PowerShell for VS 2026** from the Start menu (it sets up the
compiler for you), then from the project folder:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

`--preset windows-release` builds an optimized version. For Clang, use
`windows-clang-debug` (the Developer PowerShell must have LLVM on PATH; the
helper below adds it).

### Windows, from Git Bash or any shell

The helper finds Visual Studio, enters its compiler environment, adds the
CMake/Ninja/LLVM that ship with Visual Studio to `PATH`, then runs your command:

```bash
cmd //c 'tools\dev\msvc_env.bat cmake --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat cmake --build --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat ctest --preset windows-debug'
```

### Linux

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

`linux-asan` adds AddressSanitizer and UndefinedBehaviorSanitizer, which catch
memory errors and undefined behavior that normal builds hide.

### Where the output goes

Builds are per-preset, so Debug and Release never overwrite each other:

```
build/windows-debug/bin/peri_tests.exe
build/windows-release/bin/peri_tests.exe
build/linux-asan/bin/peri_tests
```

The test program also accepts a filter:
`peri_tests.exe memory` runs only tests whose name contains "memory".

---

## Tour of the code

### `engine/core/include/peri/core/`

Headers: the *interface* of the core. Other code includes these.

| File | Purpose |
| --- | --- |
| `types.hpp` | `u8`/`i32`/`usize` etc. so integer sizes are identical on Windows and Linux |
| `math.hpp` | `Vec2`, `Rect`, `Color`; small, inline, no dependencies |
| `result.hpp` | `Result<T>` and `Status`: how failures are returned instead of thrown |
| `log.hpp` | Logging API, levels, replaceable sink |
| `memory.hpp` | Tracked allocation API and memory statistics |
| `time.hpp` | Monotonic clock and `Stopwatch` |
| `assert.hpp` | `PERI_ASSERT` for programmer bugs only |
| `version.hpp` | Generated from the root `CMakeLists.txt`; do not edit the generated copy |

### `engine/core/src/`

Implementations (`.cpp`). Each one is compiled once into `peri_core`.

- `log.cpp` — formats records into a fixed buffer, then calls the sink. It uses
  a mutex for sink selection; sink callbacks can run concurrently and must synchronize.
- `memory.cpp` — a thin layer over `malloc`/`_aligned_malloc` that records size,
  tag and a magic number before the returned pointer. `deallocate` reads that
  header back. Counters are atomics because several threads may allocate.
- `time.cpp` — `std::chrono::steady_clock`, which never jumps when the system
  clock is adjusted.

### `engine/platform/`

This is the only module that talks to the operating system directly (through
SDL). Everything above it uses engine types.

| File | Purpose |
| --- | --- |
| `window.hpp` | Creates a window, pumps OS events, answers size/focus/fullscreen questions |
| `input.hpp` | Keyboard/mouse/gamepad state with `down`/`pressed`/`released` queries |
| `app.hpp` | Owns the window and runs the loop: events → fixed updates → render → limit |
| `fs.hpp` | UTF-8 file read/write, atomic replace, directories, executable path |
| `src/window.cpp` | SDL event translation, key/gamepad mapping, focus-loss handling |
| `src/app.cpp` | Fixed-step accumulator, FPS limiter, optional CSV frame log |

SDL is pinned in `third_party/` and downloaded on the first configure. It is a
dependency of the platform layer, not part of the engine's public API: no SDL
type appears in these headers.

### `tests/`

- `framework.hpp` — ~130 lines: registers test cases, `CHECK`/`REQUIRE`/`SKIP`,
  a filter and a summary. No third-party dependency.
- `test_main.cpp` — runs the registered cases, then fails if the engine leaked
  any tracked allocation.
- `test_*.cpp` — one file per area (math, result, memory, log, time, fixed step,
  version).
- `platform/` — window, file system and app-loop tests. They skip when no video
  driver exists instead of failing.

---

## The C++ ideas used here

You do not need to master these to follow the code; this is a working map.

### Headers, includes and translation units

A `.cpp` file plus everything it `#include`s is a **translation unit**. It is
compiled independently. A header should describe what other files may use, not
contain implementation. `#pragma once` stops a header being read twice in one
translation unit.

### Namespaces

`namespace peri { ... }` puts names in a named scope, so `peri::Result` cannot
clash with another library's `Result`. `peri::mem`, `peri::time` group related
functions. Internal helpers live in an unnamed `namespace { ... }` inside a
`.cpp`, which makes them private to that file.

### `constexpr` and `inline`

`constexpr` means "can be computed at compile time"; these functions are also
usable at runtime. `inline` on a function defined in a header avoids duplicate
symbol errors when several `.cpp` files include it.

### RAII and destructors

C++ runs a destructor when an object goes out of scope ("Resource Acquisition Is
Initialization"). `Result<T>` uses this: its destructor destroys whichever value
it currently holds. The test `result_error_destructor_is_called` proves it.

### Templates (lightly)

`Result<T>` is a template: one written class that works for `int`, `std::string`
or a move-only type. The compiler generates a version per type. Errors are
caught at compile time — a template's body is only checked in detail when
instantiated, which is why one test deliberately exercises move-only values.

### Unions and placement new (the one spiky part)

`Result<T>` must hold *either* a `T` or an `Error`, not both. A `union` overlaps
them in memory. Because `T` may be a `std::string` with its own constructor and
destructor, the code uses **placement new** (`new (&storage_) T(...)`) to build
the value in that memory and calls the destructor explicitly when done. This is
the only place in the core where that complexity is needed; if it looks dense,
that is expected, and the tests around it are the safety net.

### Variadic and printf-style logging

`log_message(level, category, format, ...)` takes extra arguments like
`printf`. The compiler checks the format string on GCC/Clang, and the result is
formatted into a fixed 1024-byte buffer, so logging does not allocate. Long
messages are visibly marked `...[truncated]`.

### Atomics and threads

Memory counters are `std::atomic`: several threads can update them without a
lock. Logging uses `std::mutex` because records are written to a shared sink.
The audio thread rules (no locks, no allocation) come later and are stricter.

### Static local initialization

A `static` variable inside a function is created on first use, exactly once,
thread-safely. `time.cpp` uses that for the clock epoch. Be careful with
expressions that combine such a call with another call: C++ does not promise
which operand is evaluated first. That exact bug produced a negative timestamp
and is now fixed and tested.

---

## Window, input and the main loop

`peri::App` is the smallest useful game loop:

```cpp
peri::AppConfig config;               // window size, title, fixed step, FPS limit
config.window.title = "my game";

auto app = peri::App::create(config);
if (app.is_error()) { /* log and exit */ }

app.value()->run(
    [](double dt) { /* gameplay: always dt == fixed step */ },
    [](double alpha) { /* draw; alpha blends between simulation states */ });
```

What happens each iteration in `engine/platform/src/app.cpp`:

1. `pump_events` clears one-frame input edges (pressed/released, mouse delta,
   wheel), translates every pending SDL event, and records each button change
   in order. Named actions keep those changes until a fixed update consumes them.
2. The frame's duration goes into `FixedTimestep`, which returns 0..8 fixed
   steps. Gameplay never sees a variable `dt`, so behaviour is stable.
3. `render(alpha)` is called once, where `alpha` is the fraction of the next
   step already elapsed (used to interpolate positions).
4. If frame limiting is on, the loop sleeps until the 60 FPS target using
   `SDL_DelayNS`, which paces better than a naive `sleep_for` on Windows.

Why fixed steps: physics and animation remain deterministic and independent of
the monitor's refresh rate. Why a cap: after a debugger pause or a window drag,
the loop must not try to simulate seconds of catch-up in one frame; extra steps
are dropped and counted.

A game should not call `SDL_*` directly. If a feature is missing, add it to
`peri_platform` and expose an engine type.

## Actions, logging, and the diagnostic display

Bindings are startup data: bind one named action to one or more engine-owned
buttons. `App` updates the map once after each event pump; no JSON format or
persistent remapping is implemented. For example:

```cpp
auto& actions = app.value()->actions();
if (actions.bind("jump", peri::ActionBinding::key(peri::Key::space)).is_error() ||
    actions.bind("jump", peri::ActionBinding::gamepad(peri::GamepadButton::south)).is_error()) {
    // Report invalid configuration or capacity exhaustion.
}
// In a callback: actions.down("jump"), actions.pressed("jump"), etc.
```

There are at most 32 actions and 8 bindings per action; names are copied and are
at most 63 bytes. Buttons are combined with OR, so an action only releases when
every bound button is up.

`App` records each button change in the order it happened, keeps those changes
until the game uses them, and hands one change per action to each fixed update.
That fixes two old bugs: a quick tap was lost when a frame ran no fixed update,
and the same press was seen again by every fixed update in a frame. Two quick
taps now produce two presses and two releases, in order. The queues are fixed
size; if input arrives far faster than it is consumed, `run` stops with an
error rather than quietly throwing events away.

Edges are only valid during the fixed update that receives them. The raw
`App::input()` state is still frame-scoped and is meant for drawing, not for
gameplay decisions. Axis bindings, text input and saved/remappable controls are
not implemented yet.

To enable file logging, include `peri/platform/file_log.hpp`, call
`FileLog::open("peri.log", 1024 * 1024, 3)`, check the result, then install
`set_log_sink(&peri::FileLog::sink, log.get())`. Records also go to stderr and
are flushed to the OS after each write, but this is **not** a durable sync or
crash handler. Check `log->status()` for write failures. Stop all logging
threads, restore `set_log_sink(nullptr, nullptr)`, and only then destroy the log;
callbacks already in flight are not waited for. Rotation overwrites the oldest
backup; use a dedicated log path. File logging does disk I/O and is not suited
to the frame or audio hot path.

`AppConfig::frame_stats_overlay = true`, or simply passing a `StatsDisplay`
callback to `run`, shows frame rate, frame interval, update time, fixed steps and
dropped steps. A callback enables the readout on its own, so the flag and the
callback cannot disagree. It works in two ways:

- On its own, it uses SDL's software renderer. That clears the window and draws
  only the text, so it is for benchmarks and debugging before a game renderer
  exists.
- With a `StatsDisplay` callback passed to `run`, the game renderer stays in
  charge. Draw the game in the render callback without presenting, then draw
  the supplied text there and present. The text pointer is only valid during
  the call; copy it if you need to keep it.

Returning an error from the callback stops the loop with a nonzero exit code.
With no callback and the flag off, the readout costs nothing. Try `bench_blank --visible --overlay`, and do not
use those numbers as baseline measurements. It has been exercised in a hidden
window on Windows only, so the actual on-screen look has not been reviewed.

## Measuring an empty frame

After building `windows-release` (or `linux-release`):

```bash
build/windows-release/bin/bench_blank.exe --seconds 5 --warmup 1
```

It prints p50/p99 frame-start intervals, dropped steps and process memory. The
unlimited mode (`--no-limit`) observes elapsed intervals, not isolated CPU time;
its sample cap can exclude later frames. Paced mode checks the limiter.
Targets and the recording format are in `docs/performance.md`. Numbers are only
meaningful from a Release build, and the machine plus commit must be recorded.

## How the core pieces behave

### Memory tracking

Every allocation goes through `peri::mem::allocate(bytes, alignment, tag)`.
The pointer it returns is preceded by a small header:

```
[ raw pointer | size | magic | tag ][ your bytes ... ]
                                    ^ pointer returned to you
```

`deallocate` steps back by the header size, checks the magic number, updates the
counters, then frees the original pointer. `stats(tag)` reports current and peak
bytes plus allocation/free counts. Tests assert that every allocation is
released and that the test run leaves zero live allocations.

Tags (`render`, `audio`, `assets`, ...) exist so a future memory overlay can say
"audio is using 4 MB" instead of only showing a total.

Why this design: it is simple, works on both platforms, and makes leaks
visible. Arenas and pools (M6) will be built on top of it.

### Errors

Functions that can fail return `Result<T>` or `Status`. The caller must look at
the result; `[[nodiscard]]` makes ignoring it a compiler warning. No exceptions
cross a module boundary, so the C ABI (M8) and callbacks stay simple. An
`PERI_ASSERT` is only for "this is a programming mistake", never for a missing
file or a lost audio device.

### Logging

Call `PERI_LOG_INFO("render", "loaded %d sprites", count)`. The category is a
short static string. Tests install their own sink, so they can assert on records
without parsing stderr. An opt-in platform file sink also writes to stderr and
rotates files. Stop logging threads and restore the default sink before destroying it.

### Time

`now_seconds()` is monotonic and useful only for differences. Wall-clock time
surfaces later for save files, where jumps are handled explicitly.

---

## Common tasks

### Add a math function

1. Put it in `engine/core/include/peri/core/math.hpp`, marked `constexpr`/`inline`
   where possible.
2. Add a `TEST_CASE` in `tests/test_math.cpp`.
3. Build and run the tests; formatting is checked by `tools/check_format.py`.

### Add a test

Add a `TEST_CASE(name)` block in the relevant `tests/test_*.cpp`. Use `CHECK` for
a failure that should not stop the test, `REQUIRE` when continuing makes no
sense. New test files must be listed in `tests/CMakeLists.txt`.

### Add a new core module

Create `engine/core/include/peri/core/<name>.hpp` and
`engine/core/src/<name>.cpp`, add the `.cpp` to `engine/core/CMakeLists.txt`,
add tests, and update this guide's file table.

### Change the engine version

Edit `VERSION` in the root `CMakeLists.txt` only. The generated header and
`k_engine_version` follow it. Add a `CHANGELOG.md` entry.

---

## Glossary

| Term | Meaning |
| --- | --- |
| **Preset** | A named, checked-in build configuration (`windows-debug`) |
| **Target** | A buildable thing: library or executable |
| **Translation unit** | One `.cpp` after preprocessing |
| **Linker** | Combines compiled objects into a library/executable |
| **Static library** | Compiled code bundled into the executable at link time |
| **Sanitizer** | Runtime checker (ASan/UBSan) for memory errors and UB |
| **Monotonic clock** | A clock that only moves forward |
| **RAII** | Tying a resource's lifetime to an object's scope |
| **UB** | Undefined behavior: code whose result the language does not define |
| **SDL** | Cross-platform library for windows, input, audio and threads; wrapped by `peri_platform` |
| **Fixed step** | Gameplay always advances by the same `dt`, regardless of frame rate |
| **Interpolation alpha** | 0..1 fraction between two simulation steps, used for smooth drawing |

---

## Where to go next

- `docs/architecture.md` — why the layers and rules are what they are.
- `docs/performance.md` — the numbers the engine must hit, and how they are measured.
- `docs/conventions.md` — the definition of done for any change.
- `docs/status.md` — what is finished, what is next, what is blocked.
