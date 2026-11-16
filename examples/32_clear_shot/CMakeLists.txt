add_executable(cinerig_32_clear_shot main.cpp)
target_link_libraries(cinerig_32_clear_shot PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_32_clear_shot PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_32_clear_shot PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_clear_shot_smoke
        COMMAND cinerig_32_clear_shot --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_clear_shot_smoke PROPERTIES LABELS "smoke;examples")
endif()
