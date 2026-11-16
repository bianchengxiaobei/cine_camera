add_executable(cinerig_09_target_group main.cpp)
target_link_libraries(cinerig_09_target_group PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_09_target_group PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_09_target_group PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_target_group_smoke
        COMMAND cinerig_09_target_group --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_target_group_smoke PROPERTIES LABELS "smoke;examples")
endif()
