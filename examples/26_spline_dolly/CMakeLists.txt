add_executable(cinerig_26_spline_dolly main.cpp)
target_link_libraries(cinerig_26_spline_dolly PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_26_spline_dolly PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_26_spline_dolly PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_dolly_smoke
        COMMAND cinerig_26_spline_dolly --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_dolly_smoke PROPERTIES LABELS "smoke;examples")
endif()
