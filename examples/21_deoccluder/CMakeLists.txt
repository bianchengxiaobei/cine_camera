add_executable(cinerig_21_deoccluder main.cpp)
target_link_libraries(cinerig_21_deoccluder PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_21_deoccluder PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_21_deoccluder PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_deoccluder_smoke
        COMMAND cinerig_21_deoccluder --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_deoccluder_smoke PROPERTIES LABELS "smoke;examples")
endif()
