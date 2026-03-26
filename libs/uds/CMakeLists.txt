# =============================================================================
# interface :: uds — UDS Client (ISO 14229)
# =============================================================================
add_library(interface_uds STATIC
    src/client.cpp
    src/iso_tp.cpp
)
add_library(interface::uds ALIAS interface_uds)

target_include_directories(interface_uds
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_uds
    PUBLIC
        interface::can
        interface::can_hal
        interface::core
)

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
