#include "peri/platform/window.hpp"

#include <algorithm>
#include <array>

#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "input_access.hpp"
#include "peri/core/log.hpp"
#include "peri/platform/sdl.hpp"
#include "sdl_internal.hpp"

namespace peri {
namespace {

SDL_Gamepad* g_gamepad = nullptr;
SDL_JoystickID g_gamepad_id = 0;
#if defined(PERI_PLATFORM_TEST_HOOKS)
int g_test_gl_profile_override = -1;
#endif

Status validate_gl_context() {
    // SDL_GL_GetAttribute returns requested version/profile in SDL 3.4.20;
    // ask the current GL driver instead.
    if (gladLoadGL(SDL_GL_GetProcAddress) == 0 || glad_glGetIntegerv == nullptr ||
        glad_glGetError == nullptr) {
        PERI_LOG_ERROR("platform", "could not load OpenGL context query functions");
        return Status::failure(ErrorCode::unsupported, "could not query OpenGL context");
    }
    int major = 0;
    int minor = 0;
    int profile = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (glGetError() != GL_NO_ERROR) {
        PERI_LOG_ERROR("platform", "could not query actual OpenGL context properties");
        return Status::failure(ErrorCode::unsupported, "could not query OpenGL context");
    }
    PERI_LOG_INFO("platform", "actual OpenGL context: %d.%d, profile mask 0x%x", major, minor,
                  static_cast<unsigned>(profile));
#if defined(PERI_PLATFORM_TEST_HOOKS)
    if (g_test_gl_profile_override >= 0) {
        profile = g_test_gl_profile_override;
    }
#endif
    if (major < 3 || (major == 3 && minor < 3) || (profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
        PERI_LOG_ERROR("platform", "OpenGL 3.3 core context required");
        return Status::failure(ErrorCode::unsupported, "OpenGL 3.3 core context required");
    }
    return Status::ok();
}

SDL_Scancode to_sdl(Key key) noexcept {
    switch (key) {
    case Key::key_a:
        return SDL_SCANCODE_A;
    case Key::key_b:
        return SDL_SCANCODE_B;
    case Key::key_c:
        return SDL_SCANCODE_C;
    case Key::key_d:
        return SDL_SCANCODE_D;
    case Key::key_e:
        return SDL_SCANCODE_E;
    case Key::key_f:
        return SDL_SCANCODE_F;
    case Key::key_g:
        return SDL_SCANCODE_G;
    case Key::key_h:
        return SDL_SCANCODE_H;
    case Key::key_i:
        return SDL_SCANCODE_I;
    case Key::key_j:
        return SDL_SCANCODE_J;
    case Key::key_k:
        return SDL_SCANCODE_K;
    case Key::key_l:
        return SDL_SCANCODE_L;
    case Key::key_m:
        return SDL_SCANCODE_M;
    case Key::key_n:
        return SDL_SCANCODE_N;
    case Key::key_o:
        return SDL_SCANCODE_O;
    case Key::key_p:
        return SDL_SCANCODE_P;
    case Key::key_q:
        return SDL_SCANCODE_Q;
    case Key::key_r:
        return SDL_SCANCODE_R;
    case Key::key_s:
        return SDL_SCANCODE_S;
    case Key::key_t:
        return SDL_SCANCODE_T;
    case Key::key_u:
        return SDL_SCANCODE_U;
    case Key::key_v:
        return SDL_SCANCODE_V;
    case Key::key_w:
        return SDL_SCANCODE_W;
    case Key::key_x:
        return SDL_SCANCODE_X;
    case Key::key_y:
        return SDL_SCANCODE_Y;
    case Key::key_z:
        return SDL_SCANCODE_Z;
    case Key::digit_0:
        return SDL_SCANCODE_0;
    case Key::digit_1:
        return SDL_SCANCODE_1;
    case Key::digit_2:
        return SDL_SCANCODE_2;
    case Key::digit_3:
        return SDL_SCANCODE_3;
    case Key::digit_4:
        return SDL_SCANCODE_4;
    case Key::digit_5:
        return SDL_SCANCODE_5;
    case Key::digit_6:
        return SDL_SCANCODE_6;
    case Key::digit_7:
        return SDL_SCANCODE_7;
    case Key::digit_8:
        return SDL_SCANCODE_8;
    case Key::digit_9:
        return SDL_SCANCODE_9;
    case Key::escape:
        return SDL_SCANCODE_ESCAPE;
    case Key::space:
        return SDL_SCANCODE_SPACE;
    case Key::enter:
        return SDL_SCANCODE_RETURN;
    case Key::tab:
        return SDL_SCANCODE_TAB;
    case Key::backspace:
        return SDL_SCANCODE_BACKSPACE;
    case Key::left:
        return SDL_SCANCODE_LEFT;
    case Key::right:
        return SDL_SCANCODE_RIGHT;
    case Key::up:
        return SDL_SCANCODE_UP;
    case Key::down:
        return SDL_SCANCODE_DOWN;
    case Key::left_shift:
        return SDL_SCANCODE_LSHIFT;
    case Key::left_ctrl:
        return SDL_SCANCODE_LCTRL;
    case Key::left_alt:
        return SDL_SCANCODE_LALT;
    case Key::f1:
        return SDL_SCANCODE_F1;
    case Key::f2:
        return SDL_SCANCODE_F2;
    case Key::f3:
        return SDL_SCANCODE_F3;
    case Key::f4:
        return SDL_SCANCODE_F4;
    case Key::f5:
        return SDL_SCANCODE_F5;
    case Key::f6:
        return SDL_SCANCODE_F6;
    case Key::f7:
        return SDL_SCANCODE_F7;
    case Key::f8:
        return SDL_SCANCODE_F8;
    case Key::f9:
        return SDL_SCANCODE_F9;
    case Key::f10:
        return SDL_SCANCODE_F10;
    case Key::f11:
        return SDL_SCANCODE_F11;
    case Key::f12:
        return SDL_SCANCODE_F12;
    case Key::unknown:
    case Key::count:
        break;
    }
    return SDL_SCANCODE_UNKNOWN;
}

std::array<Key, static_cast<usize>(SDL_SCANCODE_COUNT)> make_reverse_key_map() noexcept {
    std::array<Key, static_cast<usize>(SDL_SCANCODE_COUNT)> map{};
    map.fill(Key::unknown);
    for (usize i = 1; i < k_key_count; ++i) {
        const Key key = static_cast<Key>(i);
        const usize index = static_cast<usize>(to_sdl(key));
        if (index < map.size()) {
            map[index] = key;
        }
    }
    return map;
}

Key from_sdl(SDL_Scancode scancode) noexcept {
    static const auto map = make_reverse_key_map();
    const usize index = static_cast<usize>(scancode);
    return index < map.size() ? map[index] : Key::unknown;
}

SDL_GamepadButton to_sdl(GamepadButton button) noexcept {
    switch (button) {
    case GamepadButton::south:
        return SDL_GAMEPAD_BUTTON_SOUTH;
    case GamepadButton::east:
        return SDL_GAMEPAD_BUTTON_EAST;
    case GamepadButton::west:
        return SDL_GAMEPAD_BUTTON_WEST;
    case GamepadButton::north:
        return SDL_GAMEPAD_BUTTON_NORTH;
    case GamepadButton::back:
        return SDL_GAMEPAD_BUTTON_BACK;
    case GamepadButton::start:
        return SDL_GAMEPAD_BUTTON_START;
    case GamepadButton::left_stick:
        return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    case GamepadButton::right_stick:
        return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case GamepadButton::left_shoulder:
        return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case GamepadButton::right_shoulder:
        return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case GamepadButton::dpad_up:
        return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case GamepadButton::dpad_down:
        return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case GamepadButton::dpad_left:
        return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case GamepadButton::dpad_right:
        return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case GamepadButton::count:
        break;
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}

GamepadButton from_sdl_button(u8 button) noexcept {
    for (usize i = 0; i < k_gamepad_button_count; ++i) {
        auto value = static_cast<GamepadButton>(i);
        if (static_cast<int>(button) == static_cast<int>(to_sdl(value))) {
            return value;
        }
    }
    return GamepadButton::count;
}

SDL_GamepadAxis to_sdl(GamepadAxis axis) noexcept {
    switch (axis) {
    case GamepadAxis::left_x:
        return SDL_GAMEPAD_AXIS_LEFTX;
    case GamepadAxis::left_y:
        return SDL_GAMEPAD_AXIS_LEFTY;
    case GamepadAxis::right_x:
        return SDL_GAMEPAD_AXIS_RIGHTX;
    case GamepadAxis::right_y:
        return SDL_GAMEPAD_AXIS_RIGHTY;
    case GamepadAxis::left_trigger:
        return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    case GamepadAxis::right_trigger:
        return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    case GamepadAxis::count:
        break;
    }
    return SDL_GAMEPAD_AXIS_INVALID;
}

MouseButton to_engine_button(u8 sdl_button) noexcept {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::left;
    case SDL_BUTTON_RIGHT:
        return MouseButton::right;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::middle;
    default:
        return MouseButton::count;
    }
}

void open_gamepad(SDL_JoystickID instance_id) noexcept {
    if (g_gamepad != nullptr) {
        return;
    }
    SDL_Gamepad* gamepad = SDL_OpenGamepad(instance_id);
    if (gamepad == nullptr) {
        PERI_LOG_WARN("platform", "could not open gamepad: %s", SDL_GetError());
        return;
    }
    g_gamepad = gamepad;
    g_gamepad_id = instance_id;
    PERI_LOG_INFO("platform", "gamepad connected: %s", SDL_GetGamepadName(gamepad));
}

void close_gamepad(SDL_JoystickID instance_id) noexcept {
    if (g_gamepad == nullptr || g_gamepad_id != instance_id) {
        return;
    }
    SDL_CloseGamepad(g_gamepad);
    g_gamepad = nullptr;
    g_gamepad_id = 0;
    PERI_LOG_INFO("platform", "gamepad disconnected");
}

void sync_gamepad(InputState& input) noexcept {
    if (g_gamepad == nullptr) {
        platform::detail::InputAccess::set_gamepad_connected(input, false);
        return;
    }

    platform::detail::InputAccess::set_gamepad_connected(input, true);

    for (usize i = 0; i < k_gamepad_button_count; ++i) {
        const auto button = static_cast<GamepadButton>(i);
        platform::detail::InputAccess::set_gamepad_button(
            input, button, SDL_GetGamepadButton(g_gamepad, to_sdl(button)));
    }

    for (usize i = 0; i < k_gamepad_axis_count; ++i) {
        const auto axis = static_cast<GamepadAxis>(i);
        float value =
            std::clamp(static_cast<float>(SDL_GetGamepadAxis(g_gamepad, to_sdl(axis))) / 32767.0f,
                       -1.0f, 1.0f);
        if (axis == GamepadAxis::left_trigger || axis == GamepadAxis::right_trigger) {
            value = value < 0.0f ? 0.0f : value;
        }
        platform::detail::InputAccess::set_gamepad_axis(input, axis, value);
    }
}

}  // namespace

namespace platform::detail {

#if defined(PERI_PLATFORM_TEST_HOOKS)
void set_test_gl_profile_override(int profile) noexcept {
    g_test_gl_profile_override = profile;
}
#endif

void shutdown_devices() noexcept {
    if (g_gamepad == nullptr) {
        return;
    }
    SDL_CloseGamepad(g_gamepad);
    g_gamepad = nullptr;
    g_gamepad_id = 0;
}

}  // namespace platform::detail

struct Window::Impl {
    SDL_Window* handle = nullptr;
    SDL_GLContext gl_context = nullptr;
    bool close_requested = false;
    bool minimized = false;
    bool zero_pixel_size = false;

    ~Impl() {
        if (gl_context != nullptr && !SDL_GL_DestroyContext(gl_context)) {
            PERI_LOG_ERROR("platform", "could not destroy OpenGL context: %s", SDL_GetError());
        }
        if (handle != nullptr) {
            SDL_DestroyWindow(handle);
        }
    }
};

Result<std::unique_ptr<Window>> Window::create(const WindowConfig& config) {
    if (config.width <= 0 || config.height <= 0) {
        return Error{ErrorCode::invalid_argument, "window size must be positive"};
    }

    if (!sdl_session_active()) {
        PERI_LOG_ERROR("platform", "no SDL session; acquire peri::SdlSession first");
        return Error{ErrorCode::internal_error, "no SDL session; acquire peri::SdlSession first"};
    }

    SDL_WindowFlags flags = 0;
    if (config.opengl) {
        SDL_GL_ResetAttributes();
        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
            !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
            PERI_LOG_ERROR("platform", "could not set OpenGL attributes: %s", SDL_GetError());
            return Error{ErrorCode::internal_error, "could not set OpenGL attributes"};
        }
        flags |= SDL_WINDOW_OPENGL;
    }
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (config.hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    if (config.high_density) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    // Allocate the owner before acquiring native resources so allocation failure
    // cannot leak a window or context.
    auto impl = std::make_unique<Impl>();
    SDL_Window* handle = SDL_CreateWindow(config.title, config.width, config.height, flags);
    if (handle == nullptr) {
        PERI_LOG_ERROR("platform", "SDL_CreateWindow failed: %s", SDL_GetError());
        return Error{ErrorCode::internal_error, "could not create window"};
    }

    impl->handle = handle;
    if (config.opengl) {
        impl->gl_context = SDL_GL_CreateContext(handle);
        if (impl->gl_context == nullptr) {
            PERI_LOG_ERROR("platform", "SDL_GL_CreateContext failed: %s", SDL_GetError());
            return Error{ErrorCode::internal_error, "could not create OpenGL context"};
        }
        const Status validation = validate_gl_context();
        if (validation.is_error()) {
            return validation.error();
        }
        if (!SDL_GL_SetSwapInterval(config.vsync ? 1 : 0)) {
            PERI_LOG_WARN("platform", "initial swap interval unavailable: %s", SDL_GetError());
        }
    }

    return std::unique_ptr<Window>(new Window(std::move(impl)));
}

Window::Window(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Window::~Window() = default;

void Window::pump_events(InputState& input) {
    platform::detail::InputAccess::begin_frame(input);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            impl_->close_requested = true;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID == SDL_GetWindowID(impl_->handle)) {
                impl_->close_requested = true;
            }
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            if (event.window.windowID == SDL_GetWindowID(impl_->handle)) {
                impl_->minimized = true;
            }
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            if (event.window.windowID == SDL_GetWindowID(impl_->handle)) {
                impl_->minimized = false;
                int width = 0;
                int height = 0;
                if (SDL_GetWindowSizeInPixels(impl_->handle, &width, &height) && width > 0 &&
                    height > 0) {
                    impl_->zero_pixel_size = false;
                }
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (event.window.windowID == SDL_GetWindowID(impl_->handle)) {
                impl_->zero_pixel_size = event.window.data1 <= 0 || event.window.data2 <= 0;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (event.window.windowID == SDL_GetWindowID(impl_->handle)) {
                platform::detail::InputAccess::release_all_keys(input);
            }
            break;
        case SDL_EVENT_KEY_DOWN: {
            const Key key = from_sdl(event.key.scancode);
            if (key != Key::unknown) {
                platform::detail::InputAccess::set_key(input, key, true);
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            const Key key = from_sdl(event.key.scancode);
            if (key != Key::unknown) {
                platform::detail::InputAccess::set_key(input, key, false);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            platform::detail::InputAccess::mouse_motion(input, event.motion.x, event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const MouseButton button = to_engine_button(event.button.button);
            if (button != MouseButton::count) {
                platform::detail::InputAccess::mouse_button(
                    input, button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            platform::detail::InputAccess::wheel(input, event.wheel.y);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            open_gamepad(event.gdevice.which);
            if (g_gamepad != nullptr && event.gdevice.which == g_gamepad_id) {
                sync_gamepad(input);
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (event.gdevice.which == g_gamepad_id && g_gamepad != nullptr) {
                platform::detail::InputAccess::set_gamepad_connected(input, false);
                close_gamepad(event.gdevice.which);
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (g_gamepad != nullptr && event.gbutton.which == g_gamepad_id) {
                const GamepadButton button = from_sdl_button(event.gbutton.button);
                if (button != GamepadButton::count) {
                    platform::detail::InputAccess::set_gamepad_button(
                        input, button, event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                }
            }
            break;
        default:
            break;
        }
    }

    sync_gamepad(input);
}

bool Window::should_close() const noexcept {
    return impl_->close_requested;
}

void Window::request_close() noexcept {
    impl_->close_requested = true;
}

void Window::set_title(const char* title) {
    SDL_SetWindowTitle(impl_->handle, title);
}

void Window::toggle_fullscreen() {
    if (!SDL_SetWindowFullscreen(impl_->handle, !is_fullscreen())) {
        PERI_LOG_WARN("platform", "could not toggle fullscreen: %s", SDL_GetError());
    }
}

Status Window::make_gl_current() {
    if (impl_->gl_context == nullptr) {
        return Status::failure(ErrorCode::unsupported, "window has no OpenGL context");
    }
    if (!SDL_GL_MakeCurrent(impl_->handle, impl_->gl_context)) {
        PERI_LOG_ERROR("platform", "could not activate OpenGL context: %s", SDL_GetError());
        return Status::failure(ErrorCode::internal_error, "could not activate OpenGL context");
    }
    return Status::ok();
}

Status Window::set_vsync(bool enabled) {
    const Status current = make_gl_current();
    if (current.is_error()) {
        return current;
    }
    if (!SDL_GL_SetSwapInterval(enabled ? 1 : 0)) {
        PERI_LOG_WARN("platform", "could not set swap interval: %s", SDL_GetError());
        return Status::failure(ErrorCode::unsupported, "could not set swap interval");
    }
    return Status::ok();
}

Result<int> Window::swap_interval() {
    const Status current = make_gl_current();
    if (current.is_error()) {
        return current.error();
    }
    int interval = 0;
    if (!SDL_GL_GetSwapInterval(&interval)) {
        PERI_LOG_WARN("platform", "could not query swap interval: %s", SDL_GetError());
        return Error{ErrorCode::unsupported, "could not query swap interval"};
    }
    return interval;
}

Status Window::swap_buffers() {
    const Status current = make_gl_current();
    if (current.is_error()) {
        return current;
    }
    if (!SDL_GL_SwapWindow(impl_->handle)) {
        PERI_LOG_ERROR("platform", "could not swap OpenGL buffers: %s", SDL_GetError());
        return Status::failure(ErrorCode::internal_error, "could not swap OpenGL buffers");
    }
    return Status::ok();
}

IVec2 Window::size_pixels() const noexcept {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(impl_->handle, &width, &height);
    return IVec2{width, height};
}

IVec2 Window::size_points() const noexcept {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(impl_->handle, &width, &height);
    return IVec2{width, height};
}

float Window::display_scale() const noexcept {
    return SDL_GetWindowDisplayScale(impl_->handle);
}

bool Window::is_minimized() const noexcept {
    return impl_->minimized || impl_->zero_pixel_size ||
           (SDL_GetWindowFlags(impl_->handle) & SDL_WINDOW_MINIMIZED) != 0;
}

bool Window::has_focus() const noexcept {
    return (SDL_GetWindowFlags(impl_->handle) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

bool Window::is_fullscreen() const noexcept {
    return (SDL_GetWindowFlags(impl_->handle) & SDL_WINDOW_FULLSCREEN) != 0;
}

void* Window::native_handle() const noexcept {
    return impl_->handle;
}

}  // namespace peri
