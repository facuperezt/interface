add_executable(test_can_script
    test_script.cpp
    test_script_engine.cpp
)

target_link_libraries(test_can_script
    PRIVATE
        interface::can_script
        Catch2::Catch2WithMain
)

catch_discover_tests(test_can_script)
