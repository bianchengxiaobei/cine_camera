include(FetchContent)
enable_language(C)

option(CINERIG_USE_SYSTEM_SDL3 "Use an installed SDL3 package instead of FetchContent" OFF)
set(CINERIG_SDL3_TAG "release-3.2.10" CACHE STRING "SDL3 version tag")
option(CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS "Run windowed example smoke tests" OFF)

find_package(OpenGL REQUIRED)

if(CINERIG_USE_SYSTEM_SDL3)
    find_package(SDL3 CONFIG REQUIRED)
    if(TARGET SDL3::SDL3-static)
        set(CINERIG_SDL3_TARGET SDL3::SDL3-static)
    else()
        set(CINERIG_SDL3_TARGET SDL3::SDL3)
    endif()
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${CINERIG_SDL3_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(SDL3)
    if(TARGET SDL3::SDL3-static)
        set(CINERIG_SDL3_TARGET SDL3::SDL3-static)
    else()
        set(CINERIG_SDL3_TARGET SDL3::SDL3)
    endif()
endif()

add_library(cinerig_glad STATIC
    ${PROJECT_SOURCE_DIR}/third_party/glad/src/gl.c
)
target_include_directories(cinerig_glad
    PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/glad/include
)
set_target_properties(cinerig_glad PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
add_library(cinerig::glad ALIAS cinerig_glad)

add_library(examples_common STATIC
    common/app.cpp
    common/gl_renderer.cpp
    common/hud.cpp
    common/sdl_world.cpp
    common/sdl_camera_output.cpp
    common/sdl_input.cpp
    common/sdl_physics.cpp
)
add_library(cinerig::examples_common ALIAS examples_common)

target_compile_features(examples_common PUBLIC cxx_std_20)
target_include_directories(examples_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/third_party/stb
        ${PROJECT_SOURCE_DIR}/third_party/glad/include
)
target_link_libraries(examples_common
    PUBLIC
        cinerig::cinerig
        cinerig_glad
        ${CINERIG_SDL3_TARGET}
        OpenGL::GL
)
target_compile_definitions(examples_common
    PUBLIC
        SDL_MAIN_HANDLED
        CINERIG_EXAMPLE_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/common/shaders"
)

if(MSVC)
    target_compile_options(examples_common PRIVATE /W4 /permissive-)
else()
    target_compile_options(examples_common PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(00_sandbox)
add_subdirectory(01_follow)
add_subdirectory(03_blend)
add_subdirectory(04_position_composer)
add_subdirectory(05_orbital_follow)
add_subdirectory(06_pan_tilt)
add_subdirectory(07_third_person)
add_subdirectory(08_third_person_aim)
add_subdirectory(09_target_group)
add_subdirectory(10_hard_lock)
add_subdirectory(11_hard_look_at)
add_subdirectory(12_rotate_with_follow)
add_subdirectory(13_camera_offset)
add_subdirectory(14_recomposer)
add_subdirectory(15_free_look_modifier)
add_subdirectory(16_follow_zoom)
add_subdirectory(17_group_framing)
add_subdirectory(18_noise)
add_subdirectory(19_impulse)
add_subdirectory(20_collision_impulse)
add_subdirectory(21_deoccluder)
add_subdirectory(22_decollider)
add_subdirectory(23_confiner3d)
add_subdirectory(24_confiner2d)
add_subdirectory(25_spline)
add_subdirectory(26_spline_dolly)
add_subdirectory(27_spline_cart)
add_subdirectory(28_spline_roll)
add_subdirectory(29_spline_look_at)
add_subdirectory(30_spline_smoother)
add_subdirectory(31_mixing_camera)
add_subdirectory(32_clear_shot)
add_subdirectory(33_sequencer)
add_subdirectory(34_state_driven)
add_subdirectory(35_timeline)
add_subdirectory(36_post_processing)
add_subdirectory(37_storyboard)
add_subdirectory(38_pixel_perfect)

if(BUILD_TESTING AND COMMAND cinerig_register_test)
    add_subdirectory(tests)
endif()

















