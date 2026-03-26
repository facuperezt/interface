add_executable(test_can_trace
    test_trace_interfaces.cpp
    test_asc_reader.cpp
    test_csv_reader.cpp
    test_asc_writer.cpp
    test_csv_writer.cpp
    test_trace_replayer.cpp
)

target_link_libraries(test_can_trace
    PRIVATE
        interface::can_trace
        interface::can_hal
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_trace)
