add_executable(cinerig_34_state_driven main.cpp)
target_link_libraries(cinerig_34_state_driven PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_34_state_driven PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_34_state_driven PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_state_driven_smoke
        COMMAND cinerig_34_state_driven --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_state_driven_smoke PROPERTIES LABELS "smoke;examples")
endif()
