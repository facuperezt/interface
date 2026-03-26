add_executable(test_can_db
    test_database.cpp
    test_signal_decoder.cpp
)

target_link_libraries(test_can_db
    PRIVATE
        interface::can_db
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_db)
