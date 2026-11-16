add_executable(cinerig_scenario_tests
    scenario_smoke_test.cpp
    test_support_test.cpp
    camera_system_priority_test.cpp
    camera_system_update_clock_test.cpp
    camera_system_standby_test.cpp
    camera_system_lifecycle_test.cpp
    pipeline_order_test.cpp
    pipeline_extension_test.cpp
    virtual_camera_target_test.cpp
    manager_camera_base_test.cpp
    clear_shot_test.cpp
    mixing_camera_test.cpp
    sequencer_camera_test.cpp
    state_driven_camera_test.cpp
    external_camera_test.cpp
    follow_pipeline_test.cpp
    brain_priority_test.cpp
    brain_blend_test.cpp
    brain_nested_blend_test.cpp
    brain_override_stack_test.cpp
    timeline_mixer_test.cpp
    post_processing_test.cpp
    storyboard_test.cpp
    pixel_perfect_test.cpp
    brain_events_test.cpp
    blend_style_test.cpp
)

cinerig_register_test(cinerig_scenario_tests scenario)

