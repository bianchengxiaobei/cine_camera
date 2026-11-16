include(FetchContent)

option(CINERIG_USE_SYSTEM_CATCH2 "Use an installed Catch2 v3 package" OFF)
option(CINERIG_USE_CUSTOM_TEST_MAIN "Use the CineRig Catch2 main instead of Catch2WithMain" OFF)
set(CINERIG_CATCH2_TAG "v3.8.1" CACHE STRING "Catch2 version tag")

if(CINERIG_USE_SYSTEM_CATCH2)
    find_package(Catch2 3 REQUIRED)
else()
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${CINERIG_CATCH2_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

add_library(cinerig_test_support STATIC
    support/fake_entity_world.cpp
    support/fake_camera_output.cpp
    support/fake_physics_world.cpp
)
target_link_libraries(cinerig_test_support PUBLIC cinerig::cinerig)
target_include_directories(cinerig_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/support
)

function(cinerig_register_test target label)
    target_link_libraries(${target} PRIVATE cinerig::cinerig cinerig_test_support)
    if(CINERIG_USE_CUSTOM_TEST_MAIN)
        target_sources(${target} PRIVATE ${PROJECT_SOURCE_DIR}/tests/test_main.cpp)
        target_link_libraries(${target} PRIVATE Catch2::Catch2)
    else()
        target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    endif()
    if(COMMAND catch_discover_tests)
        catch_discover_tests(${target} PROPERTIES LABELS ${label})
    else()
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS ${label})
    endif()
endfunction()

add_subdirectory(unit)
add_subdirectory(scenario)
add_subdirectory(parity)
add_subdirectory(performance)
add_subdirectory(golden)
