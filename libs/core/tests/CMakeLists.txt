add_executable(test_core
    test_error.cpp
    test_types.cpp
)

target_link_libraries(test_core
    PRIVATE
        interface::core
        Catch2::Catch2WithMain
)

catch_discover_tests(test_core)
