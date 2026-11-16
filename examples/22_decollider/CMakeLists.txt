add_executable(cinerig_22_decollider main.cpp)
target_link_libraries(cinerig_22_decollider PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_22_decollider PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_22_decollider PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_decollider_smoke
        COMMAND cinerig_22_decollider --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_decollider_smoke PROPERTIES LABELS "smoke;examples")
endif()
