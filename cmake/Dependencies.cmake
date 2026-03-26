# =============================================================================
# interface :: Dependencies
# =============================================================================
# All third-party dependencies managed via FetchContent.
# Each dependency is only fetched if the corresponding module is enabled.
# =============================================================================

include(FetchContent)

# ---------------------------------------------------------------------------
# spdlog — structured logging (always needed by core)
# ---------------------------------------------------------------------------
# Use spdlog in header-only mode to avoid ABI issues between different
# compiler/stdlib combinations (e.g., Clang 17 with GCC 14 libstdc++).
# Mark as SYSTEM includes to suppress warnings from spdlog headers.
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_SYSTEM_INCLUDES ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# nlohmann/json — JSON serialization (used by core, config, etc.)
# ---------------------------------------------------------------------------
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# FTXUI — terminal UI framework (only if TUI enabled)
# ---------------------------------------------------------------------------
if(INTERFACE_ENABLE_TUI)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG        v5.0.0
        GIT_SHALLOW    TRUE
    )
endif()

# ---------------------------------------------------------------------------
# Catch2 — testing framework (only if testing enabled)
# ---------------------------------------------------------------------------
if(INTERFACE_ENABLE_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
endif()

# ---------------------------------------------------------------------------
# dbcppp — DBC parser base (only if CAN DB enabled)
# NOTE: dbcppp integration is declared but not yet made available.
# It will be enabled when the DBC parser wrapper is implemented.
# The library has dependencies (LibXml2 for KCD, Boost) that require
# careful handling across platforms.
# ---------------------------------------------------------------------------
# if(INTERFACE_ENABLE_CAN_DB)
#     set(build_kcd OFF CACHE BOOL "" FORCE)
#     set(build_tools OFF CACHE BOOL "" FORCE)
#     set(build_tests OFF CACHE BOOL "" FORCE)
#     set(build_examples OFF CACHE BOOL "" FORCE)
#     FetchContent_Declare(
#         dbcppp
#         GIT_REPOSITORY https://github.com/xR3b0rn/dbcppp.git
#         GIT_TAG        master
#         GIT_SHALLOW    TRUE
#     )
# endif()

# ---------------------------------------------------------------------------
# Make available
# ---------------------------------------------------------------------------
message(STATUS "Fetching dependencies...")

FetchContent_MakeAvailable(spdlog nlohmann_json)

if(INTERFACE_ENABLE_TUI)
    FetchContent_MakeAvailable(ftxui)
endif()

if(INTERFACE_ENABLE_TESTING)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
endif()

# dbcppp: not yet made available (see note above)
# if(INTERFACE_ENABLE_CAN_DB)
#     FetchContent_MakeAvailable(dbcppp)
# endif()

message(STATUS "Dependencies ready")
