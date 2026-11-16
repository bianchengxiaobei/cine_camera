add_executable(cinerig_parity_tests
    camera_state_parity_test.cpp
    damping_parity_test.cpp
    input_axis_parity_test.cpp
    priority_parity_test.cpp
    blend_manager_parity_test.cpp
    camera_position_parity_test.cpp
)

cinerig_register_test(cinerig_parity_tests parity)
