# ADR-0003: OpenGL 3.3 core as the first renderer

**Status:** Accepted (2026-11-19)
**Date:** 2026-11-19

## Context

The engine targets Windows and Linux, 2D only, on integrated GPUs with 2015+ hardware. Options: OpenGL 3.3 core, OpenGL ES/WebGL (irrelevant: no web target), Vulkan, D3D11/12 (not cross-platform), or a software renderer.

- **Vulkan** offers explicit control and better modern driver behavior, but multiplies the implementation and debugging surface (instance/device/swapchain/synchronization) for a 2D engine whose CPU-side budgets are the real gate. It also brings driver-variance work disproportionate to the benefit.
- **D3D** fails the Linux requirement.
- **Software rendering** cannot meet 10k-sprite and scaling targets on Tier A integrated GPUs.
- **OpenGL 3.3 core** is available on all target hardware, well documented, supported by SDL's context creation, and simple enough to keep the 2D backend small: one shader, dynamic vertex buffers, batching, timer queries for debug.

## Decision

Implement the first renderer on **OpenGL 3.3 core profile** with a single GL loader (pinned). Keep rendering behind the `render` interface: no GL types in public headers. M1 platform context creation is the narrow exception to the no-GL-calls-outside-backend rule: it queries the actual driver version and profile to reject an unsuitable context; it does not render or clear.

## Consequences

- Fastest route to correct sprites, batching and font rendering on both platforms.
- Driver bugs and macOS-style deprecation are not concerns; Windows/Linux GL 3.3 is stable.
- No compute shaders or modern synchronization; not needed for 2D at this scope.
- If a future target or feature needs more, a second backend is an additive project behind the same interface, not a redesign.
- GPU-bound performance remains game-dependent; our gates are CPU-side engine overhead.

## Revisit when

A required feature or target cannot be delivered on GL 3.3, or measured driver problems on supported hardware cannot be worked around.
