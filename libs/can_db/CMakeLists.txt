# =============================================================================
# interface :: can_db — CAN Database Parsing (.dbc, .eds, .cdd)
# =============================================================================
add_library(interface_can_db STATIC
    src/database.cpp
)
add_library(interface::can_db ALIAS interface_can_db)

target_include_directories(interface_can_db
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_can_db
    PUBLIC
        interface::can
        interface::core
)

# Optional: link dbcppp if available
# TODO: Wrap dbcppp behind our i_database_parser interface
# if(TARGET dbcppp)
#     target_link_libraries(interface_can_db PRIVATE dbcppp)
#     target_compile_definitions(interface_can_db PRIVATE INTERFACE_HAS_DBCPPP=1)
# endif()

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
