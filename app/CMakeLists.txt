# =============================================================================
# interface :: app — Main application entry point
# =============================================================================
add_executable(interface_app
    main.cpp
)

set_target_properties(interface_app PROPERTIES
    OUTPUT_NAME "interface"
)

target_link_libraries(interface_app
    PRIVATE
        interface::tui
        interface::core
)
