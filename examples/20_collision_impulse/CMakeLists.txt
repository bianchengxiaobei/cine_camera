add_executable(cinerig_20_collision_impulse main.cpp)
target_link_libraries(cinerig_20_collision_impulse PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_20_collision_impulse PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_20_collision_impulse PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_collision_impulse_smoke
        COMMAND cinerig_20_collision_impulse --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_collision_impulse_smoke PROPERTIES LABELS "smoke;examples")
endif()
