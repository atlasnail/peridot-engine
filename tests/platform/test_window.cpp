#include "framework.hpp"

#include <chrono>
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

// Closes a timestamp log on every exit path, including REQUIRE returns.
struct CsvFile {
    std::FILE* file = nullptr;

    ~CsvFile() {
        if (file != nullptr) {
            std::fclose(file);
        }
    }
};

// fopen is deprecated by MSVC's CRT, which treats the warning as an error.
bool open_csv(std::FILE** file, const char* path) {
#if defined(_WIN32)
    return fopen_s(file, path, "wb") == 0;
#else
    *file = std::fopen(path, "wb");
    return *file != nullptr;
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

TEST_CASE(window_opengl_swap_interval_manual) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL_SWAP_INTERVAL");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL_SWAP_INTERVAL=1 for a manual desktop comparison");
    }
    peri::WindowConfig config;
    config.title = "Peridot GL swap interval diagnostic";
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
    const SDL_DisplayID display = SDL_GetDisplayForWindow(native);
    REQUIRE(display != 0);
    const char* display_name = SDL_GetDisplayName(display);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    REQUIRE(display_name != nullptr);
    REQUIRE(mode != nullptr);
    std::printf("Swap diagnostic: %s; %s; SDL video driver: %s\n",
                reinterpret_cast<const char*>(renderer), reinterpret_cast<const char*>(version),
                video_driver);
    std::printf("Display: %s, %dx%d, %.3f Hz (reported mode)\n", display_name, mode->w, mode->h,
                static_cast<double>(mode->refresh_rate));
    std::printf("Keep the window on this display and unminimized. Interval 0 then 1; "
                "each phase has a 0.5 s warm-up and 4 s measurement.\n"
                "API results and elapsed timings do not prove physical VSync.\n");
    std::fflush(stdout);
    glDrawBuffer(GL_BACK);
    REQUIRE(glGetError() == GL_NO_ERROR);

    using Clock = std::chrono::steady_clock;
    peri::InputState input;
    bool complete = true;
    for (int interval = 0; interval <= 1; ++interval) {
        const auto set = window->set_vsync(interval == 1);
        if (set.is_error()) {
            std::printf("Interval %d: set failed: %s; phase not measured\n", interval,
                        set.error().message);
            complete = false;
            continue;
        }
        const auto actual = window->swap_interval();
        if (actual.is_error()) {
            std::printf("Interval %d: query failed: %s; phase not measured\n", interval,
                        actual.error().message);
            complete = false;
            continue;
        }
        std::printf("Interval %d: set succeeded, queried %d\n", interval, actual.value());
        if (actual.value() != interval) {
            std::printf("Interval %d: query mismatch; phase not measured\n", interval);
            complete = false;
            continue;
        }
        window->set_title(interval == 0 ? "Peridot GL swap interval 0"
                                        : "Peridot GL swap interval 1");
        std::fflush(stdout);
        const auto start = Clock::now();
        const auto measurement_start = start + std::chrono::milliseconds(500);
        unsigned long long frames = 0;
        double swap_seconds = 0.0;
        while (true) {
            window->pump_events(input);
            REQUIRE(!window->should_close());
            REQUIRE(!window->is_minimized());
            REQUIRE(SDL_GetDisplayForWindow(native) == display);
            const auto now = Clock::now();
            const double seconds = std::chrono::duration<double>(now - start).count();
            if (seconds >= 4.5) {
                break;
            }
            const peri::IVec2 size = window->size_pixels();
            REQUIRE(size.x > 0 && size.y > 0);
            // A moving vertical stripe makes tearing visible without a renderer.
            const int stripe_width = size.x / 5 + 1;
            const int stripe_x = static_cast<int>((seconds - static_cast<int>(seconds)) * size.x);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            glScissor(stripe_x, 0, stripe_width, size.y);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            REQUIRE(glGetError() == GL_NO_ERROR);
            const auto swap_start = Clock::now();
            REQUIRE(window->swap_buffers().is_ok());
            const auto swap_end = Clock::now();
            if (now >= measurement_start) {
                ++frames;
                swap_seconds += std::chrono::duration<double>(swap_end - swap_start).count();
            }
        }
        const double measured =
            std::chrono::duration<double>(Clock::now() - measurement_start).count();
        REQUIRE(frames > 0 && measured > 0.0);
        std::printf("Interval %d: %llu submissions in %.3f s (%.1f submissions/s); "
                    "mean elapsed swap_buffers call %.3f ms\n",
                    interval, frames, measured, static_cast<double>(frames) / measured,
                    1000.0 * swap_seconds / static_cast<double>(frames));
        std::fflush(stdout);
    }
    REQUIRE(complete);
    std::printf(
        "Timing and swap interval settings do not verify physical display synchronization.\n");
}

TEST_CASE(window_opengl_alternation_manual) {
    const char* requested = SDL_getenv("PERI_TEST_OPENGL_ALTERNATION");
    if (requested == nullptr || std::strcmp(requested, "1") != 0) {
        SKIP_TEST("set PERI_TEST_OPENGL_ALTERNATION=1 on a GL desktop; it takes the display "
                  "exclusive");
    }
    peri::WindowConfig config;
    config.title = "Peridot GL alternation diagnostic";
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
    const SDL_DisplayID display = SDL_GetDisplayForWindow(native);
    REQUIRE(display != 0);
    const char* display_name = SDL_GetDisplayName(display);
    REQUIRE(display_name != nullptr);

    // An explicit mode on this display: its desktop size and refresh. The helper
    // refuses when the platform falls back to borderless fullscreen, which stays
    // composited and cannot show swap pacing. A failure here leaves the window
    // windowed, so a skipped run changes nothing about the desktop.
    const auto exclusive =
        peri::platform::detail::enter_test_exclusive_fullscreen(*window, 0, 0, 0.0f);
    if (exclusive.is_error()) {
        std::printf("Exclusive fullscreen unavailable: %s\n", exclusive.error().message);
        SKIP_TEST("exclusive fullscreen is unavailable on this display path");
    }
    const auto mode = exclusive.value();
    REQUIRE(window->is_fullscreen());
    const peri::IVec2 pixels = window->size_pixels();
    CHECK(pixels.x == mode.width);
    CHECK(pixels.y == mode.height);
    REQUIRE(mode.refresh_numerator > 0);
    REQUIRE(mode.refresh_denominator > 0);
    const double refresh_hz =
        static_cast<double>(mode.refresh_numerator) / static_cast<double>(mode.refresh_denominator);
    std::printf("Alternation diagnostic: %s; %s; SDL video driver: %s\n",
                reinterpret_cast<const char*>(renderer), reinterpret_cast<const char*>(version),
                video_driver);
    std::printf("Exclusive fullscreen: %s, %dx%d, read-back mode %d/%d (%.4f Hz)\n", display_name,
                mode.width, mode.height, mode.refresh_numerator, mode.refresh_denominator,
                refresh_hz);

    const char* csv_requested = SDL_getenv("PERI_TEST_OPENGL_ALTERNATION_CSV");
    const char* csv_path = csv_requested != nullptr && csv_requested[0] != '\0'
                               ? csv_requested
                               : "peri_alternation.csv";
    CsvFile csv;
    REQUIRE(open_csv(&csv.file, csv_path));
    std::fprintf(csv.file, "phase,interval,submission,wall_seconds,elapsed_seconds,background\n");
    std::printf("Frame timestamps: %s (wall_seconds is system_clock; compare it with the "
                "capture)\n",
                csv_path);
    std::printf("Film this screen at 240 fps or more for capture evidence. The background "
                "alternates black and white four times a second; each phase warms up 0.5 s, "
                "then logs 4 s. Interval 1 should pace at the read-back refresh, 0 should not, "
                "2 should halve it, and the last phase repeats interval 1 so its spacing can be "
                "compared within one session. This run alone does not verify physical VSync.\n");
    std::fflush(stdout);
    glDrawBuffer(GL_BACK);
    REQUIRE(glGetError() == GL_NO_ERROR);

    using Clock = std::chrono::steady_clock;
    // The interval-1 repeat at the end tests whether the first phase's spacing
    // depended on the display still settling after the mode switch.
    const int intervals[] = {1, 0, 2, 1};
    constexpr int phase_count = static_cast<int>(sizeof(intervals) / sizeof(intervals[0]));
    peri::InputState input;
    bool measured_any = false;
    for (int phase = 1; phase <= phase_count; ++phase) {
        const int interval = intervals[phase - 1];
        const bool repeat = phase > 1 && interval == 1;
        const peri::Result<int> applied =
            peri::platform::detail::set_test_swap_interval(*window, interval);
        if (applied.is_error()) {
            std::printf("Phase %d (interval %d): not set (%s); phase skipped\n", phase, interval,
                        applied.error().message);
            continue;
        }
        if (applied.value() != interval) {
            std::printf("Phase %d (interval %d): driver applied %d; phase skipped\n", phase,
                        interval, applied.value());
            continue;
        }
        char title[64]{};
        std::snprintf(title, sizeof(title), "Peridot alternation: phase %d", phase);
        window->set_title(title);
        std::printf("Phase %d (interval %d%s): measuring 4 s after a 0.5 s warm-up\n", phase,
                    interval, repeat ? ", repeat" : "");
        std::fflush(stdout);

        const auto start = Clock::now();
        const auto measurement_start = start + std::chrono::milliseconds(500);
        unsigned long long frames = 0;
        unsigned long long changes = 0;
        bool have_previous = false;
        bool previous_white = false;
        bool phase_complete = true;
        while (true) {
            window->pump_events(input);
            if (window->should_close() || window->is_minimized() || !window->is_fullscreen()) {
                phase_complete = false;
                break;
            }
            const auto now = Clock::now();
            const double elapsed = std::chrono::duration<double>(now - start).count();
            if (elapsed >= 4.5) {
                break;
            }
            const peri::IVec2 size = window->size_pixels();
            if (size.x <= 0 || size.y <= 0) {
                phase_complete = false;
                break;
            }
            // Four full black/white cycles a second: slow enough for a 240 fps
            // capture to resolve every edge, fast enough to expose a tear.
            const bool white = static_cast<long long>(elapsed * 4.0) % 2 != 0;
            glDisable(GL_SCISSOR_TEST);
            glClearColor(white ? 1.0f : 0.0f, white ? 1.0f : 0.0f, white ? 1.0f : 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            if (glGetError() != GL_NO_ERROR) {
                std::printf("Phase %d (interval %d): GL error during clear; phase stopped\n", phase,
                            interval);
                phase_complete = false;
                break;
            }
            const auto wall = std::chrono::system_clock::now();
            if (!window->swap_buffers().is_ok()) {
                std::printf("Phase %d (interval %d): swap failed; phase stopped\n", phase,
                            interval);
                phase_complete = false;
                break;
            }
            if (now >= measurement_start) {
                const double wall_seconds =
                    std::chrono::duration<double>(wall.time_since_epoch()).count();
                std::fprintf(csv.file, "%d,%d,%llu,%.6f,%.6f,%s\n", phase, interval, frames,
                             wall_seconds, elapsed, white ? "white" : "black");
                if (have_previous && white != previous_white) {
                    ++changes;
                }
                have_previous = true;
                previous_white = white;
                ++frames;
            }
        }

        window->pump_events(input);
        const double measured =
            std::chrono::duration<double>(Clock::now() - measurement_start).count();
        CHECK(SDL_GetWindowFullscreenMode(native) != nullptr);
        if (frames > 0 && measured > 0.0) {
            std::printf("Phase %d (interval %d%s): %llu submissions in %.3f s (%.1f/s), %llu "
                        "background changes\n",
                        phase, interval, repeat ? ", repeat" : "", frames, measured,
                        static_cast<double>(frames) / measured, changes);
            measured_any = true;
        } else if (phase_complete) {
            std::printf("Phase %d (interval %d): no submissions logged\n", phase, interval);
        }
        std::fflush(csv.file);
    }

    // Restore the desktop before asserting, so a failed run does not leave the
    // display in exclusive mode.
    const bool left = peri::platform::detail::leave_test_fullscreen(*window);
    window->pump_events(input);
    CHECK(left);
    std::printf("Left exclusive fullscreen. Timestamps are logged, but no capture was taken, so "
                "physical VSync remains unverified.\n");
    REQUIRE(measured_any);
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
