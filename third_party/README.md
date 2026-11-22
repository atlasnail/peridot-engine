# Third-party dependencies

Pinned dependencies only. Changing a version means changing the URL and hash
here, recording it in this file and in `CHANGELOG.md`, and running the test and
benchmark suites on both platforms.

| Dependency | Version | License | Used by | Pin |
| --- | --- | --- | --- | --- |
| SDL | **3.4.20** | Zlib | `engine/platform` | URL + SHA-256 in `third_party/CMakeLists.txt` |
| glad (OpenGL loader) | **2.0.8** | MIT (Khronos headers keep their notice) | Platform context validation and tests, via `peri::gl_loader` | Generated output vendored in `third_party/glad/`; generator archive pinned by SHA-256 in its README |
| Manrope variable font | **4.505** | SIL OFL 1.1 | Brand assets only; not linked into engine | Google Fonts commit `b31870aff700ab7a1d74fa0c6887d95beb9e0037`, SHA-256 `3ae11c49db0455a3cc33e37d380f20fdb8c7f8b41dc07625c177e3d87a9d6ae6` |

## Manrope 4.505

- Source: [Google Fonts, pinned commit](https://github.com/google/fonts/tree/b31870aff700ab7a1d74fa0c6887d95beb9e0037/ofl/manrope).
- Vendored font and license: `brand/fonts/Manrope-Variable.ttf` and
  `brand/fonts/OFL.txt`. Used for brand work, not engine builds. Upstream is
  maintained through Google Fonts; the pin updates only in a reviewed change.
- Why: modern, commercially usable typography under the SIL OFL 1.1. See
  ADR-0011 and `brand/README.md` for license and logo-use details.

## SDL 3.4.20

- Source: <https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20>
- Archive: `SDL3-3.4.20.tar.gz`
- SHA-256: `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`
- Why: windowing, input/gamepads, timers, audio devices and threads on Windows
  and Linux, without making SDL part of the engine's API. See ADR-0002.
- Build: static, tests/examples disabled, fetched by CMake's FetchContent on the
  first configure into `build/<preset>/_deps/`.
- Update policy: check upstream release notes, bump the pin, run the full test
  suite and `bench_blank` on both platforms, and note behavioral changes.

## glad 2.0.8

- Generator source: <https://github.com/Dav1dde/glad/releases/tag/v2.0.8>
- Archive: `glad-2.0.8.tar.gz`, SHA-256
  `44f06f9195427c7017f5028d0894f57eb216b0a8f7c4eda7ce883732aeb2d0fc`
- Output: GL 3.3 core, no extensions, C.
- Why vendored: glad generates with Python and Jinja2. Requiring a specific
  Python for every build is fragile; the output never changes between
  dependency updates, so it is generated once and committed. See ADR-0010.
- Details, exact command and regeneration steps: `third_party/glad/README.md`.
- The platform layer and tests link the loader for context validation; the
  renderer arrives in M2 (ADR-0010).

## Offline builds

FetchContent downloads on the first configure. To build offline, populate
`build/<preset>/_deps/sdl3-src` beforehand (or use a local archive URL in a
`CMakeUserPresets.json` override). CI has network access; local agents should
not need it after the first configure.

## Linux build prerequisites

SDL needs development headers for the windowing/audio backends it enables.
On Debian/Ubuntu:

```
sudo apt-get install build-essential cmake ninja-build pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev \
    libxfixes-dev libxss-dev libxtst-dev libasound2-dev libpulse-dev \
    libwayland-dev libxkbcommon-dev libxkbcommon-x11-dev libdecor-0-dev \
    libdrm-dev libgbm-dev libudev-dev
```

SDL 3.4.20 treats XTest as required when X11 is enabled, so `libxtst-dev` is
not optional: without it the configure step fails with
`Couldn't find dependency package for XTEST`.

Most optional backends are disabled by SDL when their headers are absent, but
some X11 dependencies are hard requirements; see the XTest note above. The
engine targets X11 and Wayland for real desktops.

## CI note

`ubuntu-24.04` runners do not ship all of the above packages. The Linux CI job
installs the windowing/audio development headers before configuring, and
`tools/dev/ci_configure.sh` prints configure errors as annotations so failures
are readable without admin access to the job logs.

For a separate M1 software-GL check, CI uses these Ubuntu 24.04 packages (see
ADR-0004). They are CI system tools, not engine-shipped dependencies; the
workflow pins their named packages against Ubuntu snapshot `20261122T210000Z`.
APT uses only snapshot-enabled Noble, Noble updates and Noble security sources
for that install; the update must report the snapshot URL for each suite, and
repository signatures are still verified. The workflow allows package
downgrades on the disposable hosted runner, where newer system packages may
already be installed. The runner image and already-installed transitive
packages are not pinned, so the whole environment is not reproducible. Ubuntu
intends to retain snapshots for at least two years; review and update the
snapshot and exact pins together before expiration or for security updates.

| CI package | Pinned Ubuntu version | Upstream / license / maintenance |
| --- | --- | --- |
| `libgl1-mesa-dri`, `libglx-mesa0`, `libgbm-dev` | `25.2.8-0ubuntu0.24.04.5` | [Mesa](https://www.mesa3d.org/): maintained open-source project; core and Gallium MIT, other component licenses vary ([details](https://docs.mesa3d.org/license.html)) |
| `libgl-dev`, `libglx-dev` | `1.7.0-1build1` | [libglvnd](https://gitlab.freedesktop.org/glvnd/libglvnd): maintained vendor-neutral GL dispatch; MIT-style and component licenses ([details](https://github.com/NVIDIA/libglvnd#licensing)) |
| `xvfb`, `xserver-common` | `2:21.1.12-1ubuntu1.9` | [X.Org server](https://gitlab.freedesktop.org/xorg/xserver): maintained, predominantly MIT/X11; see [Ubuntu copyright](https://changelogs.ubuntu.com/changelogs/pool/main/x/xorg-server/xorg-server_21.1.12-1ubuntu1.9/copyright) for components |
| `xauth` | `1:1.1.2-1build1` | [X.Org xauth](https://gitlab.freedesktop.org/xorg/app/xauth): maintained; MIT/X11 ([Ubuntu copyright](https://changelogs.ubuntu.com/changelogs/pool/main/x/xauth/xauth_1.1.2-1build1/copyright)) |
| `mesa-utils`, `mesa-utils-bin` | `9.0.0-2` | [Mesa demos](https://gitlab.freedesktop.org/mesa/demos): maintained; mixed permissive component licenses ([Ubuntu copyright](https://changelogs.ubuntu.com/changelogs/pool/universe/m/mesa-demos/mesa-demos_9.0.0-2/copyright)) |

The check selects SDL's X11 backend and Mesa llvmpipe under Xvfb. It is separate
from the dummy-driver CTest run, and it does not verify a physical display,
GPU driver, real VSync, Wayland or performance gates. Its first hosted execution
passed on `db39b9a`; see `docs/status.md` for the evidence and limitations.
