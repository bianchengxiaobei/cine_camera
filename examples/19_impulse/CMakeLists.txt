add_executable(cinerig_19_impulse main.cpp)
target_link_libraries(cinerig_19_impulse PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_19_impulse PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_19_impulse PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_impulse_smoke
        COMMAND cinerig_19_impulse --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_impulse_smoke PROPERTIES LABELS "smoke;examples")
endif()
