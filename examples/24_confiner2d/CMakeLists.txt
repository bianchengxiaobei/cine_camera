add_executable(cinerig_24_confiner2d main.cpp)
target_link_libraries(cinerig_24_confiner2d PRIVATE examples_common)

if(MSVC)
    target_compile_options(cinerig_24_confiner2d PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig_24_confiner2d PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS AND BUILD_TESTING)
    add_test(
        NAME example_confiner2d_smoke
        COMMAND cinerig_24_confiner2d --frames 2 --hidden --no-vsync
    )
    set_tests_properties(example_confiner2d_smoke PROPERTIES LABELS "smoke;examples")
endif()
