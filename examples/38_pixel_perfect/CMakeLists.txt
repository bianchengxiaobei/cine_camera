add_executable(cinerig_38_pixel_perfect main.cpp)
target_link_libraries(cinerig_38_pixel_perfect PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_38_pixel_perfect PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_38_pixel_perfect PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_pixel_perfect_smoke
        COMMAND cinerig_38_pixel_perfect --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_pixel_perfect_smoke PROPERTIES LABELS "smoke;examples")
endif()
