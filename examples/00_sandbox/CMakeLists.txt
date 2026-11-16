add_executable(cinerig_00_sandbox main.cpp)
target_link_libraries(cinerig_00_sandbox PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_00_sandbox PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_00_sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_sandbox_smoke
        COMMAND cinerig_00_sandbox --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_sandbox_smoke PROPERTIES LABELS "smoke;examples")
endif()
