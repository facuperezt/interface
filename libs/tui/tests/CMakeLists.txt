add_executable(test_tui
    test_app_config.cpp
    test_keybindings.cpp
)

target_link_libraries(test_tui
    PRIVATE
        interface::tui
        Catch2::Catch2WithMain
)

catch_discover_tests(test_tui)
