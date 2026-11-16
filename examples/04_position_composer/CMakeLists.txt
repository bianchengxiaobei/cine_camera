add_executable(cinerig_04_position_composer main.cpp)
target_link_libraries(cinerig_04_position_composer PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_04_position_composer PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_04_position_composer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_position_composer_smoke
        COMMAND cinerig_04_position_composer --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_position_composer_smoke PROPERTIES LABELS "smoke;examples")
endif()
