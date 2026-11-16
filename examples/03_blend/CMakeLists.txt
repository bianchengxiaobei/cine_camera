add_executable(cinerig_03_blend main.cpp)
target_link_libraries(cinerig_03_blend PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_03_blend PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_03_blend PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_blend_smoke
        COMMAND cinerig_03_blend --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_blend_smoke PROPERTIES LABELS "smoke;examples")
endif()
