add_executable(test_can_hal
    test_mock_adapter.cpp
)

target_link_libraries(test_can_hal
    PRIVATE
        interface::can_hal
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_hal)
