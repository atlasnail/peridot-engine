#include "framework.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "../../engine/platform/src/sdl_internal.hpp"
#include "peri/platform/window.hpp"

namespace {

// SDL_VIDEODRIVER=dummy is used on headless CI. getenv is deprecated by MSVC's
// CRT, so use its safer variant on Windows.
bool using_dummy_driver() {
#if defined(_WIN32)
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, "SDL_VIDEODRIVER") != 0 || value == nullptr) {
        return false;
    }
    const bool dummy = std::strcmp(value, "dummy") == 0;
    std::free(value);
    return dummy;
#else
    const char* value = std::getenv("SDL_VIDEODRIVER");
    return value != nullptr && std::strcmp(value, "dummy") == 0;
#endif
}

}  // namespace

TEST_CASE(window_rejects_invalid_size) {
    peri::WindowConfig config;
    config.width = 0;
    config.height = 100;

    const peri::Result<std::unique_ptr<peri::Window>> window = peri::Window::create(config);
    CHECK(window.is_error());
    CHECK(window.error().code == peri::ErrorCode::invalid_argument);
}

TEST_CASE(window_gl_operations_require_an_opengl_window) {
    peri::WindowConfig config;
    config.hidden = true;
    auto result = peri::Window::create(config);
    REQUIRE(result.is_ok());
    auto window = std::move(result.value());
    const auto current = window->make_gl_current();
    REQUIRE(current.is_error());
    CHECK(current.error().code == peri::ErrorCode::unsupported);
    const auto set = window->set_vsync(false);
    REQUIRE(set.is_error());
    CHECK(set.error().code == peri::ErrorCode::unsupported);
    const auto interval = window->swap_interval();
    REQUIRE(interval.is_error());
    CHECK(interval.error().code == peri::ErrorCode::unsupported);
    const auto swap = window->swap_buffers();
    REQUIRE(swap.is_error());
    CHECK(swap.error().code == peri::ErrorCode::unsupported);
}

TEST_CASE(window_opengl_dummy_reports_unavailable) {
    if (!using_dummy_driver()) {
        SKIP_TEST("requires SDL's dummy video driver");
    }
    peri::WindowConfig config;
    config.hidden = true;
    config.opengl = true;
    const auto result = peri::Window::create(config);
    CHECK(result.is_error());
}

TEST_CASE(window_opengl_validation_failure_cleans_up) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL=1 on a GL-capable desktop");
    }
    peri::WindowConfig config;
    config.hidden = true;
    config.opengl = true;
    auto existing_result = peri::Window::create(config);
    REQUIRE(existing_result.is_ok());
    auto existing = std::move(existing_result.value());
    auto* native = static_cast<SDL_Window*>(existing->native_handle());
    const SDL_GLContext existing_context = SDL_GL_GetCurrentContext();
    REQUIRE(existing_context != nullptr);
    int before = 0;
    SDL_Window** windows = SDL_GetWindows(&before);
    REQUIRE(windows != nullptr);
    SDL_free(windows);

    // Force validation to reject a successfully created context. Restore the
    // real profile before checking results, even if a check fails.
    peri::platform::detail::set_test_gl_profile_override(0);
    const auto rejected = peri::Window::create(config);
    peri::platform::detail::set_test_gl_profile_override(-1);
    REQUIRE(rejected.is_error());
    CHECK(rejected.error().code == peri::ErrorCode::unsupported);
    int after = 0;
    windows = SDL_GetWindows(&after);
    REQUIRE(windows != nullptr);
    SDL_free(windows);
    CHECK(after == before);
    CHECK(SDL_GL_GetCurrentContext() == nullptr);
    CHECK(SDL_GL_GetCurrentWindow() == nullptr);
    REQUIRE(existing->make_gl_current().is_ok());
    CHECK(SDL_GL_GetCurrentContext() == existing_context);
    CHECK(SDL_GL_GetCurrentWindow() == native);
    auto recovery = peri::Window::create(config);
    REQUIRE(recovery.is_ok());
}

TEST_CASE(window_opengl_clear_readback) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL=1 on a GL-capable desktop");
    }
    peri::WindowConfig config;
    config.hidden = true;
    config.opengl = true;
    config.vsync = false;
    auto result = peri::Window::create(config);
    REQUIRE(result.is_ok());
    auto window = std::move(result.value());
    REQUIRE(window->make_gl_current().is_ok());
    REQUIRE(gladLoadGL(SDL_GL_GetProcAddress) != 0);
    const auto* renderer = glGetString(GL_RENDERER);
    REQUIRE(renderer != nullptr);
    std::printf("OpenGL clear renderer: %s\n", reinterpret_cast<const char*>(renderer));
    const peri::IVec2 size = window->size_pixels();
    REQUIRE(size.x > 0);
    REQUIRE(size.y > 0);

    // Read the default back buffer before swapping. Endpoint colors avoid
    // rounding assumptions about the window's framebuffer format.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    REQUIRE(glGetError() == GL_NO_ERROR);
    for (int color = 0; color < 2; ++color) {
        glClearColor(color == 0 ? 1.0f : 0.0f, color == 1 ? 1.0f : 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        for (int corner = 0; corner < 2; ++corner) {
            unsigned char pixel[4]{};
            glReadPixels(corner == 0 ? 0 : size.x - 1, corner == 0 ? 0 : size.y - 1, 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, pixel);
            REQUIRE(glGetError() == GL_NO_ERROR);
            CHECK(pixel[0] == (color == 0 ? 255 : 0));
            CHECK(pixel[1] == (color == 1 ? 255 : 0));
            CHECK(pixel[2] == 0);
        }
    }
    REQUIRE(window->swap_buffers().is_ok());
}

TEST_CASE(window_opengl_visible_clear_manual) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL_VISIBLE");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL_VISIBLE=1 for a manual desktop check");
    }
    peri::WindowConfig config;
    config.title = "Peridot GL diagnostic: red / green";
    config.width = 640;
    config.height = 480;
    config.opengl = true;
    config.vsync = false;
    auto result = peri::Window::create(config);
    REQUIRE(result.is_ok());
    auto window = std::move(result.value());
    REQUIRE(window->make_gl_current().is_ok());
    REQUIRE(gladLoadGL(SDL_GL_GetProcAddress) != 0);
    const auto* renderer = glGetString(GL_RENDERER);
    const auto* version = glGetString(GL_VERSION);
    const char* video_driver = SDL_GetCurrentVideoDriver();
    REQUIRE(renderer != nullptr);
    REQUIRE(version != nullptr);
    REQUIRE(video_driver != nullptr);
    auto* native = static_cast<SDL_Window*>(window->native_handle());
    REQUIRE((SDL_GetWindowFlags(native) & SDL_WINDOW_HIDDEN) == 0);
    std::printf("OpenGL visible diagnostic: %s; %s; SDL video driver: %s\n",
                reinterpret_cast<const char*>(renderer), reinterpret_cast<const char*>(version),
                video_driver);
    std::printf(
        "Observe alternating red and green each second; close the window or wait 10 seconds.\n"
        "A passing test does not confirm visible presentation.\n");
    std::fflush(stdout);

    // This fresh context has the default framebuffer and color write state.
    glDrawBuffer(GL_BACK);
    REQUIRE(glGetError() == GL_NO_ERROR);

    peri::InputState input;
    const Uint64 start = SDL_GetTicks();
    bool swapped_red = false;
    bool swapped_green = false;
    while (true) {
        const Uint64 elapsed = SDL_GetTicks() - start;
        if (elapsed >= 10000) {
            break;
        }
        window->pump_events(input);
        if (window->should_close()) {
            break;
        }
        if (!window->is_minimized()) {
            const bool red = (elapsed / 1000) % 2 == 0;
            glClearColor(red ? 1.0f : 0.0f, red ? 0.0f : 1.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            REQUIRE(glGetError() == GL_NO_ERROR);
            REQUIRE(window->swap_buffers().is_ok());
            swapped_red = swapped_red || red;
            swapped_green = swapped_green || !red;
        }
        SDL_Delay(50);
    }
    REQUIRE(swapped_red);
    REQUIRE(swapped_green);
    std::printf("Both colors submitted and swapped; presentation requires human observation.\n");
}

TEST_CASE(window_opengl_context_smoke) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL=1 on a GL-capable desktop");
    }
    peri::WindowConfig config;
    config.hidden = true;
    config.opengl = true;
    config.vsync = false;
    auto result = peri::Window::create(config);
    REQUIRE(result.is_ok());
    auto window = std::move(result.value());
    auto* native = static_cast<SDL_Window*>(window->native_handle());
    REQUIRE((SDL_GetWindowFlags(native) & SDL_WINDOW_OPENGL) != 0);
    REQUIRE(SDL_GL_GetCurrentContext() != nullptr);
    CHECK(SDL_GL_GetCurrentWindow() == native);
    const SDL_GLContext first_context = SDL_GL_GetCurrentContext();
    const SDL_WindowID first_id = SDL_GetWindowID(native);
    REQUIRE(first_id != 0);

    // SDL_GL_GetAttribute reports requested context fields in SDL 3.4.20;
    // query the current driver instead using the pinned loader.
    REQUIRE(gladLoadGL(SDL_GL_GetProcAddress) != 0);
    int major = 0;
    int minor = 0;
    int profile = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    REQUIRE(glGetError() == GL_NO_ERROR);
    std::printf("OpenGL driver context: %d.%d, profile mask 0x%x\n", major, minor,
                static_cast<unsigned>(profile));
    CHECK(major > 3 || (major == 3 && minor >= 3));
    CHECK((profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0);

    // Check what the driver accepted where swap-control is available. A
    // driver's refusal is allowed, but it must be returned to the caller.
    const auto off = window->set_vsync(false);
    if (off.is_ok()) {
        const auto interval = window->swap_interval();
        REQUIRE(interval.is_ok());
        CHECK(interval.value() == 0);
    } else {
        CHECK(off.error().code == peri::ErrorCode::unsupported);
    }
    const auto on = window->set_vsync(true);
    if (on.is_ok()) {
        const auto interval = window->swap_interval();
        REQUIRE(interval.is_ok());
        CHECK(interval.value() == 1);
    } else {
        CHECK(on.error().code == peri::ErrorCode::unsupported);
    }
    CHECK(window->swap_buffers().is_ok());

    auto second_result = peri::Window::create(config);
    REQUIRE(second_result.is_ok());
    auto second = std::move(second_result.value());
    auto* second_native = static_cast<SDL_Window*>(second->native_handle());
    const SDL_WindowID second_id = SDL_GetWindowID(second_native);
    const SDL_GLContext second_context = SDL_GL_GetCurrentContext();
    REQUIRE(second_id != 0);
    REQUIRE(second_context != nullptr);
    CHECK(second_context != first_context);
    CHECK(SDL_GL_GetCurrentWindow() == second_native);

    REQUIRE(window->make_gl_current().is_ok());
    CHECK(SDL_GL_GetCurrentContext() == first_context);
    CHECK(SDL_GL_GetCurrentWindow() == native);
    REQUIRE(second->swap_buffers().is_ok());
    CHECK(SDL_GL_GetCurrentContext() == second_context);
    CHECK(SDL_GL_GetCurrentWindow() == second_native);

    // Destroying a non-current context must leave the other one current.
    window.reset();
    CHECK(SDL_GetWindowFromID(first_id) == nullptr);
    CHECK(SDL_GL_GetCurrentContext() == second_context);
    CHECK(SDL_GL_GetCurrentWindow() == second_native);
    second.reset();
    CHECK(SDL_GetWindowFromID(second_id) == nullptr);
    CHECK(SDL_GL_GetCurrentContext() == nullptr);
    CHECK(SDL_GL_GetCurrentWindow() == nullptr);
}

TEST_CASE(window_focus_loss_releases_keyboard_and_mouse_actions) {
    peri::WindowConfig config;
    config.hidden = true;
    auto result = peri::Window::create(config);
    REQUIRE(result.is_ok());
    auto window = std::move(result.value());
    const SDL_WindowID id = SDL_GetWindowID(static_cast<SDL_Window*>(window->native_handle()));
    REQUIRE(id != 0);

    peri::InputState input;
    peri::ActionMap actions;
    REQUIRE(actions.bind("jump", peri::ActionBinding::key(peri::Key::space)).is_ok());
    REQUIRE(actions.bind("fire", peri::ActionBinding::mouse(peri::MouseButton::left)).is_ok());
    window->pump_events(input);

    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = id;
    key.key.scancode = SDL_SCANCODE_SPACE;
    REQUIRE(SDL_PushEvent(&key));
    SDL_Event mouse{};
    mouse.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouse.button.windowID = id;
    mouse.button.button = SDL_BUTTON_LEFT;
    REQUIRE(SDL_PushEvent(&mouse));
    window->pump_events(input);
    REQUIRE(input.key_down(peri::Key::space));
    REQUIRE(input.mouse_down(peri::MouseButton::left));
    REQUIRE(actions.update(input).is_ok());
    actions.begin_step();
    CHECK(actions.down("jump"));
    CHECK(actions.down("fire"));

    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    focus.window.windowID = id;
    REQUIRE(SDL_PushEvent(&focus));
    window->pump_events(input);
    CHECK(!input.key_down(peri::Key::space));
    CHECK(input.key_released(peri::Key::space));
    CHECK(!input.mouse_down(peri::MouseButton::left));
    CHECK(input.mouse_released(peri::MouseButton::left));
    REQUIRE(actions.update(input).is_ok());
    actions.begin_step();
    CHECK(!actions.down("jump"));
    CHECK(actions.released("jump"));
    CHECK(!actions.down("fire"));
    CHECK(actions.released("fire"));
}

TEST_CASE(window_create_pump_and_close) {
    peri::WindowConfig config;
    config.title = "engine window test";
    config.width = 320;
    config.height = 240;
    config.hidden = true;
    config.resizable = false;
    config.vsync = false;

    peri::Result<std::unique_ptr<peri::Window>> result = peri::Window::create(config);
    if (result.is_error()) {
        SKIP_TEST("video driver unavailable");
    }

    std::unique_ptr<peri::Window> window = std::move(result.value());
    REQUIRE(window != nullptr);

    peri::InputState input;
    for (int frame = 0; frame < 3; ++frame) {
        window->pump_events(input);
    }

    CHECK(!window->should_close());

    // The dummy driver used on headless CI does not guarantee real sizes.
    const bool dummy_driver = using_dummy_driver();

    const peri::IVec2 size_points = window->size_points();
    const peri::IVec2 size_pixels = window->size_pixels();
    if (!dummy_driver) {
        CHECK(size_points.x > 0);
        CHECK(size_points.y > 0);
        CHECK(size_pixels.x > 0);
        CHECK(size_pixels.y > 0);
        CHECK(window->display_scale() > 0.0f);
    }

    // No keys are held in a headless test, so repeated pumping must not leave
    // pressed edges behind.
    window->pump_events(input);
    CHECK(!input.key_pressed(peri::Key::space));

    window->set_title("engine window test (renamed)");
    window->request_close();
    CHECK(window->should_close());
}
