# =============================================================================
# interface — Modern C++23 TUI Tool Collection
# =============================================================================
cmake_minimum_required(VERSION 3.25)

project(interface
    VERSION      0.1.0
    DESCRIPTION  "Modern C++23 TUI tool collection for CAN, UDS, and CANopen"
    HOMEPAGE_URL "https://github.com/facuperezt/interface"
    LANGUAGES    CXX
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use a build directory.")
endif()

# Only set defaults when this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Default to Release if not specified
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Build configuration
# ---------------------------------------------------------------------------
include(cmake/Options.cmake)
include(cmake/CompilerFlags.cmake)
include(cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
# Core is always built
add_subdirectory(libs/core)

if(INTERFACE_ENABLE_CAN)
    add_subdirectory(libs/can)
endif()

if(INTERFACE_ENABLE_CAN_DB)
    add_subdirectory(libs/can_db)
endif()

if(INTERFACE_ENABLE_CAN_TRACE)
    add_subdirectory(libs/can_trace)
endif()

if(INTERFACE_ENABLE_CAN_HAL)
    add_subdirectory(libs/can_hal)
endif()

if(INTERFACE_ENABLE_UDS)
    add_subdirectory(libs/uds)
endif()

if(INTERFACE_ENABLE_CANOPEN)
    add_subdirectory(libs/canopen)
endif()

if(INTERFACE_ENABLE_TUI)
    add_subdirectory(libs/tui)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
if(INTERFACE_ENABLE_TUI)
    add_subdirectory(app)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(INTERFACE_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
