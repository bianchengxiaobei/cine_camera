add_executable(cinerig_07_third_person main.cpp)
target_link_libraries(cinerig_07_third_person PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_07_third_person PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_07_third_person PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_third_person_smoke
        COMMAND cinerig_07_third_person --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_third_person_smoke PROPERTIES LABELS "smoke;examples")
endif()
