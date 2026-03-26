add_executable(test_can_trace
    test_trace_interfaces.cpp
)

target_link_libraries(test_can_trace
    PRIVATE
        interface::can_trace
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_trace)
