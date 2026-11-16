add_executable(cinerig_unit_tests
    header_smoke_test.cpp
    skeleton_test.cpp
    math_vec_test.cpp
    math_quat_test.cpp
    math_curve_test.cpp
    math_geometry_test.cpp
    damp_test.cpp
    predictor_test.cpp
    gaussian_filter_test.cpp
    camera_state_test.cpp
    lens_settings_test.cpp
    screen_composer_test.cpp
    priority_channel_test.cpp
    noise_profile_test.cpp
    camera_target_test.cpp
    lookahead_settings_test.cpp
    lens_lerp_test.cpp
    camera_state_lerp_test.cpp
    blend_definition_test.cpp
    blend_test.cpp
    engine_interfaces_test.cpp
    follow_minimal_test.cpp
    follow_binding_test.cpp
    target_tracking_test.cpp
    input_axis_test.cpp
    hard_lock_to_target_test.cpp
    hard_look_at_test.cpp
    rotate_with_follow_target_test.cpp
    camera_offset_test.cpp
    recomposer_test.cpp
    free_look_modifier_test.cpp
    basic_multi_channel_perlin_test.cpp
    impulse_test.cpp
    collision_impulse_source_test.cpp
    deoccluder_test.cpp
    shot_quality_evaluator_test.cpp
    decollider_test.cpp
    confiner3d_test.cpp
    confiner_oven_test.cpp
    confiner2d_test.cpp
    spline_test.cpp
    spline_dolly_test.cpp
    spline_cart_test.cpp
    spline_roll_test.cpp
    spline_dolly_look_at_targets_test.cpp
    spline_smoother_test.cpp
    follow_zoom_test.cpp
    input_axis_controller_test.cpp
    orbital_follow_test.cpp
    pan_tilt_test.cpp
    target_group_test.cpp
    group_framing_test.cpp
    third_person_aim_test.cpp
    third_person_follow_test.cpp
    position_composer_test.cpp
    rotation_composer_test.cpp
)

cinerig_register_test(cinerig_unit_tests unit)


















