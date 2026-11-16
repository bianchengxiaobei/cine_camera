add_executable(cinerig_14_recomposer main.cpp)
target_link_libraries(cinerig_14_recomposer PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_14_recomposer PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_14_recomposer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_recomposer_smoke
        COMMAND cinerig_14_recomposer --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_recomposer_smoke PROPERTIES LABELS "smoke;examples")
endif()
