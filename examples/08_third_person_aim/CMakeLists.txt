add_executable(cinerig_08_third_person_aim main.cpp)
target_link_libraries(cinerig_08_third_person_aim PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_08_third_person_aim PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_08_third_person_aim PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_third_person_aim_smoke
        COMMAND cinerig_08_third_person_aim --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_third_person_aim_smoke PROPERTIES LABELS "smoke;examples")
endif()
