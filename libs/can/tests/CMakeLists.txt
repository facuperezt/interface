add_executable(test_can
    test_frame.cpp
    test_filter.cpp
)

target_link_libraries(test_can
    PRIVATE
        interface::can
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can)
