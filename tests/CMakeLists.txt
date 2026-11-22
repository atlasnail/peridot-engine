add_executable(peri_tests
    main_core.cpp
    test_main.cpp
    test_fixed_step.cpp
    test_log.cpp
    test_math.cpp
    test_memory.cpp
    test_result.cpp
    test_time.cpp
    test_version.cpp
)

target_link_libraries(peri_tests PRIVATE peri::core)
target_include_directories(peri_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

peri_apply_warnings(peri_tests)

add_test(NAME peri_tests COMMAND peri_tests)
set_tests_properties(peri_tests PROPERTIES TIMEOUT 60 LABELS "unit")

add_executable(peri_platform_tests
    test_main.cpp
    platform/main_platform.cpp
    platform/test_app.cpp
    platform/test_actions.cpp
    platform/test_file_log.cpp
    platform/test_fs.cpp
    platform/test_sdl.cpp
    platform/test_window.cpp
)

target_link_libraries(peri_platform_tests PRIVATE peri::platform SDL3::SDL3-static peri::gl_loader)
target_include_directories(peri_platform_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(peri_platform_tests PRIVATE PERI_PLATFORM_TEST_HOOKS=1)

peri_apply_warnings(peri_platform_tests)

add_test(NAME peri_platform_tests COMMAND peri_platform_tests)
set_tests_properties(peri_platform_tests PROPERTIES TIMEOUT 60 LABELS "unit;platform")
