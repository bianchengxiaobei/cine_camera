add_executable(cinerig_30_spline_smoother main.cpp)
target_link_libraries(cinerig_30_spline_smoother PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_30_spline_smoother PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_30_spline_smoother PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_smoother_smoke
        COMMAND cinerig_30_spline_smoother --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_smoother_smoke PROPERTIES LABELS "smoke;examples")
endif()
