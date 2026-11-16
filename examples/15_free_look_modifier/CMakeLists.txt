add_executable(cinerig_15_free_look_modifier main.cpp)
target_link_libraries(cinerig_15_free_look_modifier PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_15_free_look_modifier PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_15_free_look_modifier PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_free_look_modifier_smoke
        COMMAND cinerig_15_free_look_modifier --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_free_look_modifier_smoke PROPERTIES LABELS "smoke;examples")
endif()
