#include "framework.hpp"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include <SDL3/SDL.h>

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
