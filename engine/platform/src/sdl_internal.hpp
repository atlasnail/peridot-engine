#pragma once

// Internal hooks shared by the platform layer's translation units. Not installed
// and not part of the public surface.

namespace peri::platform::detail {

// Closes SDL devices owned by the platform layer (the selected gamepad) before
// SDL_Quit. Called by SdlSession when the last reference is released, because
// those handles must not outlive the library's own shutdown.
void shutdown_devices() noexcept;

#if defined(PERI_PLATFORM_TEST_HOOKS)
// Override the profile reported by a newly created GL context for failure tests;
// -1 restores the real driver value. Main-thread only.
void set_test_gl_profile_override(int profile) noexcept;
#endif

}  // namespace peri::platform::detail
