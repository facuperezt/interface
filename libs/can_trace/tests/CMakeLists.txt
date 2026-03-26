add_executable(test_can_trace
    test_trace_interfaces.cpp
    test_asc_reader.cpp
    test_csv_reader.cpp
)

target_link_libraries(test_can_trace
    PRIVATE
        interface::can_trace
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_trace)
