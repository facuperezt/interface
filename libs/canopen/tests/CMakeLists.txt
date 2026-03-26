add_executable(test_canopen
    test_object_dictionary.cpp
    test_nmt.cpp
    test_pdo.cpp
)

target_link_libraries(test_canopen
    PRIVATE
        interface::canopen
        Catch2::Catch2WithMain
)

catch_discover_tests(test_canopen)
