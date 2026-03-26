# =============================================================================
# interface :: tui — FTXUI Terminal User Interface
# =============================================================================
add_library(interface_tui STATIC
    src/app.cpp
)
add_library(interface::tui ALIAS interface_tui)

target_include_directories(interface_tui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_tui
    PUBLIC
        interface::core
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Conditionally link available modules so the TUI can adapt
if(INTERFACE_ENABLE_CAN)
    target_link_libraries(interface_tui PUBLIC interface::can)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_CAN=1)
endif()

if(INTERFACE_ENABLE_CAN_DB)
    target_link_libraries(interface_tui PUBLIC interface::can_db)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_CAN_DB=1)
endif()

if(INTERFACE_ENABLE_CAN_TRACE)
    target_link_libraries(interface_tui PUBLIC interface::can_trace)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_CAN_TRACE=1)
endif()

if(INTERFACE_ENABLE_CAN_HAL)
    target_link_libraries(interface_tui PUBLIC interface::can_hal)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_CAN_HAL=1)
endif()

if(INTERFACE_ENABLE_UDS)
    target_link_libraries(interface_tui PUBLIC interface::uds)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_UDS=1)
endif()

if(INTERFACE_ENABLE_CANOPEN)
    target_link_libraries(interface_tui PUBLIC interface::canopen)
    target_compile_definitions(interface_tui PUBLIC INTERFACE_HAS_CANOPEN=1)
endif()

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
