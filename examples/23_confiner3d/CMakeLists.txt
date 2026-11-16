add_executable(cinerig_23_confiner3d main.cpp)
target_link_libraries(cinerig_23_confiner3d PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_23_confiner3d PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_23_confiner3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_confiner3d_smoke
        COMMAND cinerig_23_confiner3d --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_confiner3d_smoke PROPERTIES LABELS "smoke;examples")
endif()
