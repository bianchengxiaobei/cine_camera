add_executable(cinerig_35_timeline main.cpp)
target_link_libraries(cinerig_35_timeline PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_35_timeline PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_35_timeline PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_timeline_smoke
        COMMAND cinerig_35_timeline --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_timeline_smoke PROPERTIES LABELS "smoke;examples")
endif()
