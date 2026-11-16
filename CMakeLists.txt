cmake_minimum_required(VERSION 3.24)

project(CineRig VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CINERIG_BUILD_TESTS "Build CineRig tests" ON)
option(CINERIG_BUILD_EXAMPLES "Build CineRig examples" OFF)
option(CINERIG_WITH_SDL3 "Enable SDL3 example support" OFF)
option(CINERIG_WITH_CLIPPER2 "Enable optional Clipper2 support" OFF)

if(CINERIG_BUILD_EXAMPLES AND NOT CINERIG_WITH_SDL3)
    message(STATUS "Enabling CINERIG_WITH_SDL3 because CINERIG_BUILD_EXAMPLES=ON")
    set(CINERIG_WITH_SDL3 ON CACHE BOOL "Enable SDL3 example support" FORCE)
endif()

set(CINERIG_MATH_BACKEND "native" CACHE STRING "Math backend: native or glm")
set_property(CACHE CINERIG_MATH_BACKEND PROPERTY STRINGS native glm)

if(NOT CINERIG_MATH_BACKEND STREQUAL "native" AND NOT CINERIG_MATH_BACKEND STREQUAL "glm")
    message(FATAL_ERROR "CINERIG_MATH_BACKEND must be 'native' or 'glm'")
endif()

add_library(cinerig STATIC
    src/cinerig.cpp
    src/math/math.cpp
    src/core/bezier_spline.cpp
    src/core/camera_state_blend.cpp
    src/core/blend.cpp
    src/core/input_axis.cpp
    src/core/input_axis_controller.cpp
    src/core/orbit_rig.cpp
    src/core/spline.cpp
    src/core/spline_auto_dolly.cpp
    src/core/spline_helpers.cpp
    src/core/tracker.cpp
    src/camera/camera_component.cpp
    src/camera/pipeline.cpp
    src/camera/virtual_camera.cpp
    src/camera/virtual_camera_manager_base.cpp
    src/camera/external_camera.cpp
    src/components/auto_focus.cpp
    src/components/basic_multi_channel_perlin.cpp
    src/components/deoccluder.cpp
    src/components/shot_quality_evaluator.cpp
    src/components/decollider.cpp
    src/components/confiner3d.cpp
    src/components/confiner_oven.cpp
    src/components/confiner2d.cpp
    src/components/follow.cpp
    src/components/spline_cart.cpp
    src/components/spline_dolly.cpp
    src/components/spline_dolly_look_at_targets.cpp
    src/components/spline_roll.cpp
    src/components/spline_smoother.cpp
    src/components/orbital_follow.cpp
    src/components/pan_tilt.cpp
    src/components/hard_lock_to_target.cpp
    src/components/hard_look_at.cpp
    src/components/camera_offset.cpp
    src/components/recomposer.cpp
    src/components/free_look_modifier.cpp
    src/components/follow_zoom.cpp
    src/components/rotate_with_follow_target.cpp
    src/components/third_person_aim.cpp
    src/components/third_person_follow.cpp
    src/components/post_processing.cpp
    src/components/storyboard.cpp
    src/components/pixel_perfect.cpp
    src/components/position_composer.cpp
    src/components/rotation_composer.cpp
    src/impulse/impulse_manager.cpp
    src/impulse/impulse_definition.cpp
    src/impulse/fixed_signal.cpp
    src/impulse/external_impulse_listener.cpp
    src/impulse/impulse_source.cpp
    src/impulse/collision_impulse_source.cpp
    src/impulse/impulse_listener.cpp
    src/managers/clear_shot.cpp
    src/managers/mixing_camera.cpp
    src/managers/sequencer_camera.cpp
    src/managers/state_driven_camera.cpp
    src/brain/blend_lookup.cpp
    src/brain/camera_blend_stack.cpp
    src/brain/blend_manager.cpp
    src/brain/camera_brain.cpp
    src/brain/timeline_mixer.cpp
    src/targets/target_group.cpp
    src/targets/group_framing.cpp
    src/runtime/camera_system.cpp
    src/runtime/virtual_camera_registry.cpp
    src/runtime/update_manager.cpp
    src/runtime/update_tracker.cpp
)
add_library(cinerig::cinerig ALIAS cinerig)

target_compile_features(cinerig PUBLIC cxx_std_20)
target_include_directories(cinerig
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(cinerig PRIVATE /W4 /permissive-)
else()
    target_compile_options(cinerig PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CINERIG_MATH_BACKEND STREQUAL "native")
    target_compile_definitions(cinerig PUBLIC CINERIG_MATH_BACKEND_NATIVE=1)
else()
    target_compile_definitions(cinerig PUBLIC CINERIG_MATH_BACKEND_GLM=1)
    message(STATUS "CINERIG_MATH_BACKEND=glm is accepted for compatibility, but GLM math is not implemented")
endif()

if(CINERIG_WITH_SDL3)
    message(STATUS "SDL3 support enabled; SDL3 and OpenGL remain isolated in examples_common")
endif()

if(CINERIG_WITH_CLIPPER2)
    message(STATUS "CINERIG_WITH_CLIPPER2 is accepted for compatibility; ConfinerOven uses the internal polygon engine")
endif()

if(CINERIG_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

if(CINERIG_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS cinerig
    EXPORT CineRigTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/CineRigConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CineRigConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CineRig
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CineRigConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)
install(EXPORT CineRigTargets
    FILE CineRigTargets.cmake
    NAMESPACE cinerig::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CineRig
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CineRigConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CineRigConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CineRig
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
    ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/CineRig
)

