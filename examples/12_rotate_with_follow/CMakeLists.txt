add_executable(cinerig_12_rotate_with_follow main.cpp)
target_link_libraries(cinerig_12_rotate_with_follow PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_12_rotate_with_follow PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_12_rotate_with_follow PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_rotate_with_follow_smoke
        COMMAND cinerig_12_rotate_with_follow --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_rotate_with_follow_smoke PROPERTIES LABELS "smoke;examples")
endif()
