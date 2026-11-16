add_executable(cinerig_28_spline_roll main.cpp)
target_link_libraries(cinerig_28_spline_roll PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_28_spline_roll PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_28_spline_roll PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_roll_smoke
        COMMAND cinerig_28_spline_roll --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_roll_smoke PROPERTIES LABELS "smoke;examples")
endif()
