add_executable(cinerig_37_storyboard main.cpp)
target_link_libraries(cinerig_37_storyboard PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_37_storyboard PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_37_storyboard PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_storyboard_smoke
        COMMAND cinerig_37_storyboard --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_storyboard_smoke PROPERTIES LABELS "smoke;examples")
endif()
