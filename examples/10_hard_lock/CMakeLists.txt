add_executable(cinerig_10_hard_lock main.cpp)
target_link_libraries(cinerig_10_hard_lock PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_10_hard_lock PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_10_hard_lock PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_hard_lock_smoke
        COMMAND cinerig_10_hard_lock --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_hard_lock_smoke PROPERTIES LABELS "smoke;examples")
endif()
