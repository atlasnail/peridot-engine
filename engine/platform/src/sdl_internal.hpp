#pragma once

// Internal hooks shared by the platform layer's translation units. Not installed
// and not part of the public surface.

#include "peri/core/result.hpp"
#include "peri/platform/window.hpp"

namespace peri::platform::detail {

// Closes SDL devices owned by the platform layer (the selected gamepad) before
// SDL_Quit. Called by SdlSession when the last reference is released, because
// those handles must not outlive the library's own shutdown.
void shutdown_devices() noexcept;

#if defined(PERI_PLATFORM_TEST_HOOKS)
// Override the profile reported by a newly created GL context for failure tests;
// -1 restores the real driver value. Main-thread only.
void set_test_gl_profile_override(int profile) noexcept;

// The display mode a window's exclusive fullscreen actually uses, read back
// after the platform applied it. The refresh ratio is the precise value a VSync
// measurement must be compared against; SDL_DisplayMode::refresh_rate is only
// the rounded form of the same mode.
struct TestExclusiveMode {
    int width = 0;
    int height = 0;
    int refresh_numerator = 0;
    int refresh_denominator = 0;
};

// Switch the window to exclusive fullscreen using an explicit display mode on
// its current display: the mode matching width/height whose refresh is closest
// to requested_hz, or the display's desktop mode when width/height are zero.
// Fails, leaving the window windowed, when the platform falls back to borderless
// desktop fullscreen or reports no precise refresh. Main-thread only; the caller
// must call leave_test_fullscreen and must never treat success as proof of
// physical VSync, because exclusive mode is only a precondition for measuring it.
Result<TestExclusiveMode> enter_test_exclusive_fullscreen(Window& window, int width, int height,
                                                          float requested_hz);

// Leave fullscreen and clear the explicit mode, so a later request is borderless
// desktop fullscreen again. Returns false if the platform refused either step.
bool leave_test_fullscreen(Window& window) noexcept;

// Set an arbitrary swap interval (0, 1, 2, ...) and report what the driver
// accepted. Test-only because Window::set_vsync models only on and off.
Result<int> set_test_swap_interval(Window& window, int interval);
#endif

}  // namespace peri::platform::detail
