add_executable(cinerig_16_follow_zoom main.cpp)
target_link_libraries(cinerig_16_follow_zoom PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_16_follow_zoom PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_16_follow_zoom PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_follow_zoom_smoke
        COMMAND cinerig_16_follow_zoom --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_follow_zoom_smoke PROPERTIES LABELS "smoke;examples")
endif()
