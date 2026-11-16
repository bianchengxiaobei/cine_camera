add_executable(cinerig_25_spline main.cpp)
target_link_libraries(cinerig_25_spline PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_25_spline PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_25_spline PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_smoke
        COMMAND cinerig_25_spline --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_smoke PROPERTIES LABELS "smoke;examples")
endif()
