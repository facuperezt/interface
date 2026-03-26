# =============================================================================
# interface :: core
# =============================================================================
add_library(interface_core STATIC
    src/core.cpp
)
add_library(interface::core ALIAS interface_core)

target_include_directories(interface_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(interface_core
    PUBLIC
        interface::compiler_flags
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(INTERFACE_ENABLE_TESTING)
    add_subdirectory(tests)
endif()
