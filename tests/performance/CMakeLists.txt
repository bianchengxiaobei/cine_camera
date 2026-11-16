add_executable(cinerig_performance_tests
    allocation_counter.cpp
    performance_test.cpp
)

cinerig_register_test(cinerig_performance_tests performance)
