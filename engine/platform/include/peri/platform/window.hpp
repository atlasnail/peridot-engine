#pragma once

// Window and OS event handling.
//
// SDL is an implementation detail: this header does not include any SDL type.
// A window is created once and destroyed with its owner; pump_events() must be
// called once per frame and feeds InputState.
//
// OpenGL is opt-in so ordinary windows and headless CI do not require a GPU.
// All window and context operations, including destruction, run on the main
// thread; an active SdlSession must outlive the window.

#include <memory>

#include "peri/core/math.hpp"
#include "peri/core/result.hpp"
#include "peri/platform/input.hpp"

namespace peri {

struct WindowConfig {
    const char* title = "Peridot Engine";
    int width = 1280;
    int height = 720;
    bool resizable = true;
    bool hidden = false;
    bool high_density = true;
    // Initial swap interval request for an OpenGL window; ignored otherwise.
    // Unsupported swap control is logged but does not prevent window creation.
    bool vsync = true;
    // Request a GL 3.3 core context; false preserves non-GL window creation.
    bool opengl = false;
};

class Window {
public:
    // Fails if video/window creation fails, or if the actual GL context is
    // not queryable GL 3.3 core or newer. The driver version/profile is logged.
    // GL is unavailable on SDL's dummy driver; non-GL windows work headlessly.
    static Result<std::unique_ptr<Window>> create(const WindowConfig& config);

    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // Clears per-frame input, pumps every pending OS event, updates window
    // state and the provided input. Call exactly once per frame.
    void pump_events(InputState& input);

    [[nodiscard]] bool should_close() const noexcept;
    void request_close() noexcept;

    void set_title(const char* title);
    void toggle_fullscreen();

    // Make this window's context current before issuing GL commands. The GL
    // operations below also make it current, replacing any current context.
    // These return errors for non-GL windows or SDL failures; VSync requests
    // may fail on unsupported drivers, so query the interval separately.
    [[nodiscard]] Status make_gl_current();
    [[nodiscard]] Status set_vsync(bool enabled);
    [[nodiscard]] Result<int> swap_interval();
    [[nodiscard]] Status swap_buffers();

    // Pixel size is what the renderer draws into; point size is what the OS
    // reports for UI/mouse coordinates.
    [[nodiscard]] IVec2 size_pixels() const noexcept;
    [[nodiscard]] IVec2 size_points() const noexcept;
    [[nodiscard]] float display_scale() const noexcept;

    [[nodiscard]] bool is_minimized() const noexcept;
    [[nodiscard]] bool has_focus() const noexcept;
    [[nodiscard]] bool is_fullscreen() const noexcept;

    // Opaque native handle for the renderer backend (SDL_Window* today).
    [[nodiscard]] void* native_handle() const noexcept;

private:
    struct Impl;
    explicit Window(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

}  // namespace peri
