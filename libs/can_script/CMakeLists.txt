# =============================================================================
# interface :: CAN Script
# =============================================================================

add_library(interface_can_script STATIC
    src/script.cpp
    src/script_engine.cpp
)
add_library(interface::can_script ALIAS interface_can_script)

target_include_directories(interface_can_script
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_can_script
    PUBLIC
        interface::can
        interface::can_hal
        interface::core
        nlohmann_json::nlohmann_json
)

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
