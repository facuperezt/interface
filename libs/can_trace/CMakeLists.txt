# =============================================================================
# interface :: can_trace — Trace File Parsing (ASC, BLF, CSV)
# =============================================================================
add_library(interface_can_trace STATIC
    src/trace.cpp
    src/c_asc_reader.cpp
    src/c_csv_reader.cpp
    src/c_asc_writer.cpp
    src/c_csv_writer.cpp
    src/c_trace_replayer.cpp
)
add_library(interface::can_trace ALIAS interface_can_trace)

target_include_directories(interface_can_trace
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_can_trace
    PUBLIC
        interface::can
        interface::can_hal
        interface::core
)

if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
