add_executable(cinerig_11_hard_look_at main.cpp)
target_link_libraries(cinerig_11_hard_look_at PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_11_hard_look_at PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_11_hard_look_at PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_hard_look_at_smoke
        COMMAND cinerig_11_hard_look_at --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_hard_look_at_smoke PROPERTIES LABELS "smoke;examples")
endif()
