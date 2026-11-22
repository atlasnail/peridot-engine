# Peridot Engine

This is a small game engine for making 2D games on Windows and Linux. It is
written in C++20 and built from scratch, so the code base stays small enough to
read and understand.

The project is called **Peridot Engine**, with `peri` as its code namespace and
build prefix. The name has not been cleared by a trademark search. The logo,
font and usage guidance are in [`brand/`](brand/README.md).

## What it is meant to be

The short version: something sturdy, not something flashy.

- It should run 2D games at a steady frame rate without hogging memory.
- It should play sound without clicks or dropouts.
- Adding art should mean dropping a file in a folder, not running a pipeline.
- Game logic should be writable in C++ now and Lua later.
- Both Windows and Linux should behave the same way.
- The code and docs should stay clear enough that a person (or an AI helper)
  can pick the project up months later.

## What works today

This is early. Only the base of the engine is in place.

- A window opens, handles keyboard, mouse and gamepad input, and closes
  cleanly.
- The main loop runs game updates on a fixed clock and drawing at whatever rate
  the machine can manage.
- Keyboard, mouse and gamepad buttons can be tied to named actions like "jump"
  or "pause".
- Logs can go to the screen and to a file that rotates when it gets big.
- There is a small on-screen readout of frame rate and frame times, for
  debugging.
- Files can be read and written, including safely replacing a file.

There is **no way to draw a game yet**. No sprites, no textures, no text font,
no sound. That is the next big chunk of work. Right now the only thing that can
draw is the debug readout.

Windows is where the work has been done by hand. Linux GitHub Actions builds
and tests passed after the Peridot rename using a fake display driver. Nobody
has yet opened a window or used a controller on a real Linux desktop.

## Building it

You need CMake, Ninja, a C++20 compiler, and an internet connection the first
time (the build downloads SDL, the library that handles windows and input).

### Windows

Open "Developer PowerShell for VS 2026" from the Start menu, go to the project
folder, and run:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

If you prefer a normal shell or Git Bash, use the helper script, which finds
Visual Studio and sets up the compiler for you:

```bash
cmd //c 'tools\dev\msvc_env.bat cmake --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat cmake --build --preset windows-debug'
cmd //c 'tools\dev\msvc_env.bat ctest --preset windows-debug'
```

Use `windows-release` instead of `windows-debug` for an optimized build. There
is also `windows-clang-debug` if you want to build with Clang.

### Linux

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

This is what CI runs on Ubuntu. On a desktop you also need the SDL development
packages listed in `third_party/README.md`; the XTest ones are required, not
optional. Exact problems found so far are in `docs/status.md`.

### Where the programs end up

Each build type gets its own folder, so they do not overwrite each other:

```
build/windows-debug/bin/peri_tests.exe
build/windows-debug/bin/peri_platform_tests.exe
build/windows-release/bin/bench_blank.exe
```

The test programs take an optional name filter, for example
`peri_tests.exe memory` runs only the memory tests.

## Trying the frame counter

After building `windows-release`:

```bash
build/windows-release/bin/bench_blank.exe --seconds 5 --visible --overlay
```

That opens a window with the frame readout for five seconds. Without
`--overlay` it runs the same loop with nothing drawn, which is useful when you
want to know how much time the loop itself takes.

Do not copy the numbers from a Debug build, and do not compare numbers from
different machines. How to measure properly is written down in
`docs/performance.md`.

## Where things live

```
engine/core/       basic pieces: math, errors, logging, memory tracking, timing
engine/platform/   windows, input, file access, the main loop (this is the only
                   part that talks to SDL)
engine/public/     the API that games are meant to use (mostly still empty)
tests/             tests for the core and platform code
benchmarks/        small programs that measure speed, plus saved results
docs/              plans, decisions, rules, and a beginner's guide
games/example/     a small game to prove the engine works (not started)
```

A rule worth knowing: only `engine/platform/` is allowed to talk to the
operating system or SDL. Everything above it uses the engine's own types. That
keeps the messy platform code in one place.

## Reading order

If you are new here, read these in order:

1. `docs/status.md` - what actually works right now, and what is broken.
2. `docs/guide/README.md` - a slower walk through the code and the C++ ideas
   used in it.
3. `docs/roadmap.md` - the plan, milestone by milestone.
4. `docs/architecture.md` - how the parts fit together.
5. `AGENTS.md` - the rules for changing the code.

The `docs/decisions/` folder records choices that are expensive to change later,
and why they were made.

## What it is not trying to be

No 3D. No mobile, console or web builds. No level editor. No built-in physics
engine and no built-in networking for version 1.0. Those are written down in
`docs/backlog.md` so they do not creep into the current work.

Steam and GOG achievement support is not implemented either. There is a short
plan in `docs/backlog.md` describing how game-side adapters could work later,
without adding any store SDK to the engine now.

## License

Engine code is MIT. See `LICENSE`. The bundled Manrope font has its own SIL
Open Font License; see `brand/fonts/OFL.txt` and `brand/README.md`.
