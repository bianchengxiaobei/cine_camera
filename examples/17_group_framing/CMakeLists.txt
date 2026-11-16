add_executable(cinerig_17_group_framing main.cpp)
target_link_libraries(cinerig_17_group_framing PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_17_group_framing PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_17_group_framing PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_group_framing_smoke
        COMMAND cinerig_17_group_framing --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_group_framing_smoke PROPERTIES LABELS "smoke;examples")
endif()
