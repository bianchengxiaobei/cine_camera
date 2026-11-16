add_executable(cinerig_13_camera_offset main.cpp)
target_link_libraries(cinerig_13_camera_offset PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_13_camera_offset PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_13_camera_offset PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_camera_offset_smoke
        COMMAND cinerig_13_camera_offset --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_camera_offset_smoke PROPERTIES LABELS "smoke;examples")
endif()
