add_executable(cinerig_18_noise main.cpp)
target_link_libraries(cinerig_18_noise PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_18_noise PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_18_noise PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_noise_smoke
        COMMAND cinerig_18_noise --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_noise_smoke PROPERTIES LABELS "smoke;examples")
endif()
