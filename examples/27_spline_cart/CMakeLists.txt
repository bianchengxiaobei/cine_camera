add_executable(cinerig_27_spline_cart main.cpp)
target_link_libraries(cinerig_27_spline_cart PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_27_spline_cart PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_27_spline_cart PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_spline_cart_smoke
        COMMAND cinerig_27_spline_cart --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_spline_cart_smoke PROPERTIES LABELS "smoke;examples")
endif()
