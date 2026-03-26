# =============================================================================
# interface :: canopen — CANopen Stack (CiA 301/302)
# =============================================================================
add_library(interface_canopen STATIC
    src/object_dictionary.cpp
    src/nmt.cpp
    src/pdo.cpp
    src/sdo_client.cpp
    src/c_eds_parser.cpp
)
add_library(interface::canopen ALIAS interface_canopen)

target_include_directories(interface_canopen
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_canopen
    PUBLIC
        interface::can
        interface::can_db
        interface::can_hal
        interface::core
)

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
