# Platform: window, input, file system and OS boundaries.
#
# SDL is used here and nowhere else. Any OS header belongs in this module.
# Everything above depends on peri::platform through its own types.

add_library(peri_platform STATIC
    src/app.cpp
    src/fs.cpp
    src/file_log.cpp
    src/input.cpp
    src/sdl.cpp
    src/window.cpp
)

add_library(peri::platform ALIAS peri_platform)

target_include_directories(peri_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(peri_platform
    PUBLIC peri::core
    PRIVATE SDL3::SDL3-static peri::gl_loader
)

if(PERI_BUILD_TESTS)
    target_compile_definitions(peri_platform PRIVATE PERI_PLATFORM_TEST_HOOKS=1)
endif()

peri_apply_warnings(peri_platform)
