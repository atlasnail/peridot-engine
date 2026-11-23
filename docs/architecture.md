# Architecture

This document describes the **intended** architecture. Core and parts of the platform layer exist; see `docs/status.md` for verified behavior. Planned components remain intent, not implementation.

## 1. Design principles

1. **Thin, explicit layers.** Platform differences stop at one boundary. Everything above it is platform-agnostic.
2. **Own your data.** Engine-owned allocators, asset handles instead of raw pointers, no hidden global state beyond one documented engine context.
3. **Pay for what you use.** Zero-cost debug paths, no allocation in steady-state frames, no background threads the user did not ask for.
4. **Deterministic simulation.** Fixed step, stable ordering, controlled floating point usage.
5. **Small stable surface.** A narrow public API is easier to keep compatible than a wide one.
6. **Errors are values.** No exceptions escape the public API; every failure is reportable.
7. **Two platforms, one behavior.** Any intentional divergence is documented and tested.

## 2. Layers

```
        ┌───────────────────────────────────────────────────────┐
 games  │  game code (C++)        Lua scripts        C ABI users│
        └───────────────┬───────────────┬───────────────┬───────┘
                        │               │               │
        ┌───────────────▼───────────────▼───────────────▼───────┐
 public │ engine/public  (stable C++ API)   engine/c_api (C ABI) │
        └───────────────┬───────────────────────────────────────┘
                        │
        ┌───────────────▼───────────────────────────────────────┐
 engine │ scene · render · audio · assets · ui · script          │
        │            core (math, alloc, time, log, jobs)         │
        └───────────────┬───────────────────────────────────────┘
                        │  only this layer may include OS/SDL
        ┌───────────────▼───────────────────────────────────────┐
platform│ platform: window, input, files, threads, clocks, audio │
        └───────────────┬───────────────────────────────────────┘
                        │
        ┌───────────────▼───────────────────────────────────────┐
 system │ Windows (Win32)            Linux (X11 / Wayland / PipeWire) │
        └───────────────────────────────────────────────────────┘
```

**Dependency rule:** dependencies point downward only. `core` knows nothing about rendering. `render` never includes platform headers. `scene` never includes render internals — it emits render commands or references sprite resources through public types.

## 3. Module responsibilities

| Module | Owns | Must not |
| --- | --- | --- |
| `core` | Math (vec2, mat3, rect, color), handles/IDs, allocators, containers, `Result`, logging, assertions, time, job queue | Talk to the OS, allocate hidden globals, know about sprites |
| `platform` | SDL init/shutdown, window, GL context, input devices, file I/O, threads, clocks, audio device, clipboard, DPI | Contain gameplay or rendering policy |
| `render` | GL 3.3 backend, command buffer, batching, textures, shaders, camera math, sprite/text primitives, resolution scaling | Own asset decoding, block on disk, expose GL types publicly |
| `audio` | Device I/O, mixer graph, voice management, decoders, streaming, fades | Block the callback, allocate in the callback |
| `assets` | Registry, virtual paths, loaders, atlas descriptors, cache/eviction, hot reload, budget accounting | Give out owning raw pointers |
| `scene` | Entity/component storage, transforms, systems, collision, camera, scripted scene lifetime | Depend on a specific renderer backend |
| `ui` | Widgets, layout, focus navigation, in-game panels | Replace the debug overlay |
| `script` | Lua state, bindings, sandboxing, error isolation | Access engine internals directly |
| `public` | The supported C++ surface; forward declarations and handles | Leak implementation headers |
| `c_api` | Opaque handles, C types, ABI version check | Expose C++ types or exceptions |

## 4. Core concepts

### Engine context
One `Engine` object owns subsystems and their lifetimes in a fixed order: log → platform → allocators → assets → render → audio → scene → script. No other global singletons. Systems receive explicit references.

### Handles, not pointers
- `EntityId`: index + generation; stale IDs are detectable, never dereferenced blindly.
- `TextureHandle` / `SoundHandle` / `FontHandle`: registry indices, stable across hot reload.
- `AssetId`: interned path or content hash for release packing.

Rules: handles are cheap to copy, validated on use (debug assertion + runtime error), and never serialized as raw pointers.

### Memory
| Allocator | Use |
| --- | --- |
| General heap | Startup, tools, rare growth. Tracked. |
| Arena | Per-frame scratch, per-scene state, UI strings. Reset wholesale. |
| Pool | Entities, particles, voices, transient objects with uniform size. |
| Frame scratch | Render command and batching scratch, cleared each frame. |
| Asset arena (optional) | Long-lived decoded data with explicit budgets. |

Every subsystem reports bytes and peak bytes. Budgets are configured per subsystem and enforced in Debug/CI.

### Frame model
1. Poll input and platform events.
2. Accumulate time; run fixed `update(dt)` steps (default 1/60 s, bounded catch-up).
3. Run `late_update` for camera/constraint resolution.
4. Interpolate render state.
5. Build render commands and submit.
6. Present; collect frame stats.

Rendering may interpolate; gameplay never depends on variable `dt`. Catch-up is capped to avoid death spirals; the cap is documented and observable (dropped-step counter).

### Threading
- **Main thread:** platform events, simulation, render submission, most asset registry mutation.
- **Worker pool:** image decode, audio decode, file prefetch, optional asset packing in tools.
- **Audio thread:** mixer callback only; lock-free queues for commands and metrics.
- Rules: no locks held across callbacks, no allocations in the audio callback, no engine callbacks from workers into gameplay code.

Threading model is deliberately conservative for 1.0. Parallel ECS systems are a P2/backlog item and must be justified by measurements.

### Rendering model
- Retained scene state, immediate-mode render submission per frame.
- Commands are sorted into batches by state (texture, blend, shader, layer), then streamed into one or a few dynamic vertex buffers.
- Command and vertex data come from frame arenas: no per-sprite allocations.
- Y axis: **screen-space y-down** is the default for 2D authoring; document conversions where physics math prefers y-up. (Open decision D3.)
- Color: linear or sRGB workflow must be decided once; textures document their color space.

### Assets
- Game assets live in `assets/` with virtual paths (`sprites/player.png`).
- Descriptors (atlas, animation, tilemap) are versioned JSON with a schema and validator.
- Development: files read directly from disk, hot reload enabled in Debug.
- Release: optional packed archive, deterministic ordering, integrity checked at startup.
- Failure policy: placeholder + structured error; the game can run degraded but never crash on a bad asset.

### Errors and logging
- Public API returns `Result<T>` / status enums. No exceptions cross module boundaries.
- `assert` is for programmer bugs in Debug only; recoverable conditions always use error values.
- Log levels: trace/debug/info/warn/error/fatal, with category tags. Hot-path logging is compiled out or sampled.
- Fatal errors flush the log, attempt a crash dump, and exit with a documented code.

### Determinism
- Fixed-step simulation; all gameplay randomness through a seeded engine RNG.
- Stable iteration: no unordered containers in simulation-affecting paths.
- Avoid platform-dependent math differences: restrict to well-defined float ops; document any use of transcendental functions; do not rely on FMA contraction differences (build flags pinned).
- Determinism test hashes world state and compares across platforms.

### Persistence and formats
Every persisted format carries a magic/identifier and integer version. Readers accept older versions only through explicit migrations. Writes are atomic: write temp file, flush, replace, keep one backup. Checksums detect truncation/corruption.

Formats requiring versioning from day one: save files, settings, atlas descriptors, animation clips, tilemaps, packed asset archive.

### Localization
All user-facing text uses UTF-8 string IDs resolved through localization tables. Fonts are loaded per script coverage. Layout supports wrapping and basic RTL mirroring; full shaping (Arabic/Indic) is a documented limitation or a later dependency decision.

### Public API tiers
| Tier | Location | Guarantee |
| --- | --- | --- |
| Stable | `engine/public/`, `engine/c_api/` | Semver-protected; deprecation window of at least one minor release. |
| Experimental | headers marked `experimental` | May change in any minor release; changelog note required. |
| Internal | everything else under `engine/` | No guarantees; games must not include it. |

## 5. Planned public surface (non-binding sketch)

Illustrative only; final names and shapes are decided when each milestone starts, and changes need an ADR if they alter a tier guarantee.

```cpp
// engine/public/engine.h  (sketch, not implemented)
namespace peri {

struct Config { int width, height; const char* title; bool vsync; };

Result<Engine*> create_engine(const Config&);
void destroy_engine(Engine*);

struct FrameTimings { double update_ms, render_ms, total_ms; };

class Game {
public:
    virtual ~Game() = default;
    virtual void on_start(Engine&) {}
    virtual void on_fixed_update(Engine&, double dt) {}
    virtual void on_render(Engine&) {}
    virtual void on_shutdown(Engine&) {}
};

int run(Engine&, Game&);

} // namespace peri
```

Structural intent: an explicit context, a game interface, and free functions for subsystem access (`renderer(e)`, `audio(e)`, `assets(e)`), rather than deep inheritance or singletons.

## 6. Open decisions

Tracked here until resolved by ADR; each becomes an ADR at M0.

| ID | Question | Recommendation | Blocks |
| --- | --- | --- | --- |
| D1 | SDL3 vs. native Win32/X11/Wayland backends | SDL3 for window/input/audio, keep the boundary thin and swappable | M1 |
| D2 | OpenGL 3.3 vs. Vulkan/other | GL 3.3 core + one loader; revisit only with measured driver problems | M2 |
| D3 | y-down vs. y-up world coordinates | y-down for authoring; document one conversion for math-heavy code | M2 |
| D4 | Color pipeline (sRGB textures, blending space) | sRGB textures, linear blending, documented per-asset flag | M2 |
| D5 | ECS style (sparse sets vs. archetypes vs. simple pools) | Simple typed pools + component arrays first; optimize only if measured | M5 |
| D6 | Save format (binary vs. JSON) | Versioned JSON + optional binary payloads for debuggability; revisit if size/latency demands | M6 |
| D7 | Exception policy | No exceptions in engine hot paths or public API; tools/tests may use them | M0 |
| D8 | Third-party use policy | SDL3 and glad are pinned (ADR-0002, ADR-0010); stb_image, Lua, Ogg/Vorbis and a JSON parser are added when their milestone needs them, each pinned and ADR'd | M0 |
| D9 | Editor scope | None for 1.0; command-line asset tools only | M9 |
| D10 | Script binding style (Lua) | Explicit context table, no exposed raw pointers, script errors isolated | M8 |

## 7. Implementation status (M1)

What is actually true today, as opposed to intended:

- `platform` wraps SDL 3.4.20 and exposes `Window`, `InputState`, `App`,
  `SdlSession` and `peri::fs`. SDL types stay inside the module; the only leak is
  the opaque `void* native_handle()` intended for the renderer backend.
- SDL's global state has one owner: `SdlSession`. The first acquire initialises
  video and the gamepad subsystem, the last release closes the selected gamepad
  and calls `SDL_Quit`, and `Window::create` refuses to run without an active
  session. `App` holds a session, so nothing above the platform layer has to
  think about SDL's lifetime beyond not outliving its own App.
- The OpenGL loader is pinned as glad 2.0.8, GL 3.3 core with no extensions
  (ADR-0010). The platform layer and tests link it to query actual context
  properties. Its function table is process-global; a rendering backend must
  load functions for its current context and after context changes.
- An opt-in `WindowConfig::opengl` requests an SDL GL 3.3 core context; the
  window owns and destroys it and rejects an unqueryable, pre-3.3 or non-core
  context. `make_gl_current` selects it before drawing; `set_vsync`,
  `swap_interval` and `swap_buffers` also make it current. Initial swap control
  is best-effort; errors from runtime requests are returned. Ordinary windows
  remain headless-compatible. An opt-in test-only diagnostic clears and reads
  the default back buffer under ADR-0003; no production clear or renderer exists.
  Linux context version and desktop behavior need further checks.
- `app` runs fixed updates with an interpolation alpha, an optional FPS limiter
  and an optional CSV frame log. It is internal API until the public surface is
  defined at M2.
- One gamepad is supported at a time; hot-plug is handled.
- D1 (SDL3), D2 (OpenGL 3.3) and D7 (error model) are settled by ADRs; D3 (y-down)
  and D4 (color pipeline) are still open for M2.

## 8. What agents must not assume

- That a module exists because it is listed here.
- That a sketch API compiles, is stable, or has been reviewed.
- That a library is approved because it is named; only `third_party/` plus an ADR makes it real.
- That performance numbers are known; only `benchmarks/` records are evidence.
