add_executable(cinerig_36_post_processing main.cpp)
target_link_libraries(cinerig_36_post_processing PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_36_post_processing PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_36_post_processing PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_post_processing_smoke
        COMMAND cinerig_36_post_processing --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_post_processing_smoke PROPERTIES LABELS "smoke;examples")
endif()
