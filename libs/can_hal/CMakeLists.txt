# =============================================================================
# interface :: can_hal — CAN Hardware Abstraction Layer
# =============================================================================
set(CAN_HAL_SOURCES
    src/c_mock_adapter.cpp
)

# Platform-specific adapters
if(INTERFACE_HAL_PCAN)
    # list(APPEND CAN_HAL_SOURCES src/c_pcan_adapter.cpp)
    message(STATUS "PCAN adapter: stub (driver SDK required)")
endif()

if(INTERFACE_HAL_IXXAT)
    # list(APPEND CAN_HAL_SOURCES src/c_ixxat_adapter.cpp)
    message(STATUS "IXXAT adapter: stub (VCI SDK required)")
endif()

if(INTERFACE_HAL_SOCKETCAN)
    # list(APPEND CAN_HAL_SOURCES src/c_socketcan_adapter.cpp)
    message(STATUS "SocketCAN adapter: stub (implementation pending)")
endif()

add_library(interface_can_hal STATIC ${CAN_HAL_SOURCES})
add_library(interface::can_hal ALIAS interface_can_hal)

target_include_directories(interface_can_hal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_can_hal
    PUBLIC
        interface::can
        interface::core
)

# Link platform-specific libraries
if(INTERFACE_HAL_PCAN)
    # target_link_libraries(interface_can_hal PRIVATE PCANBasic)
endif()

if(INTERFACE_HAL_IXXAT)
    # target_link_libraries(interface_can_hal PRIVATE vcinpl2)
endif()

if(INTERFACE_HAL_SOCKETCAN)
    # SocketCAN uses standard socket APIs — no extra libs needed
endif()

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
