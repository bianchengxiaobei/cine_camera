add_executable(cinerig_05_orbital_follow main.cpp)
target_link_libraries(cinerig_05_orbital_follow PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_05_orbital_follow PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_05_orbital_follow PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_orbital_follow_smoke
        COMMAND cinerig_05_orbital_follow --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_orbital_follow_smoke PROPERTIES LABELS "smoke;examples")
endif()
