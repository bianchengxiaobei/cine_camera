add_executable(cinerig_31_mixing_camera main.cpp)
target_link_libraries(cinerig_31_mixing_camera PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_31_mixing_camera PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_31_mixing_camera PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_mixing_camera_smoke
        COMMAND cinerig_31_mixing_camera --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_mixing_camera_smoke PROPERTIES LABELS "smoke;examples")
endif()
