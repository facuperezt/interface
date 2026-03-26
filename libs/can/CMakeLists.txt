# =============================================================================
# interface :: can — CAN base layer
# =============================================================================
add_library(interface_can STATIC
    src/can.cpp
    src/statistics.cpp
    src/dispatcher.cpp
)
add_library(interface::can ALIAS interface_can)

target_include_directories(interface_can
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_can
    PUBLIC
        interface::core
)

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
