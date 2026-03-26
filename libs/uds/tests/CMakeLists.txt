add_executable(test_uds
    test_client.cpp
    test_nrc.cpp
)

target_link_libraries(test_uds
    PRIVATE
        interface::uds
        Catch2::Catch2WithMain
)

catch_discover_tests(test_uds)
