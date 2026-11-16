add_executable(cinerig_06_pan_tilt main.cpp)
target_link_libraries(cinerig_06_pan_tilt PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_06_pan_tilt PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_06_pan_tilt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_pan_tilt_smoke
        COMMAND cinerig_06_pan_tilt --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_pan_tilt_smoke PROPERTIES LABELS "smoke;examples")
endif()
