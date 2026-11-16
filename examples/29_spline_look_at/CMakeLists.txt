add_executable(cinerig_29_spline_look_at main.cpp)
target_link_libraries(cinerig_29_spline_look_at PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_29_spline_look_at PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_29_spline_look_at PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_look_at_smoke
        COMMAND cinerig_29_spline_look_at --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_look_at_smoke PROPERTIES LABELS "smoke;examples")
endif()
