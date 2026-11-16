add_executable(cinerig_33_sequencer main.cpp)
target_link_libraries(cinerig_33_sequencer PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_33_sequencer PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_33_sequencer PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_sequencer_smoke
        COMMAND cinerig_33_sequencer --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_sequencer_smoke PROPERTIES LABELS "smoke;examples")
endif()
