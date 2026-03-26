# =============================================================================
# interface :: Build Options
# =============================================================================
# Feature toggles for selective compilation.
# Use CMake presets or -D flags to control which modules are built.
# =============================================================================

include(CMakeDependentOption)

# ---------------------------------------------------------------------------
# Module toggles
# ---------------------------------------------------------------------------
option(INTERFACE_ENABLE_CAN       "Enable CAN base layer"                ON)
option(INTERFACE_ENABLE_CAN_DB    "Enable CAN database parsing (.dbc, .eds, .cdd)" ON)
option(INTERFACE_ENABLE_CAN_TRACE "Enable trace file parsing (ASC, BLF, CSV)"      ON)
option(INTERFACE_ENABLE_CAN_HAL   "Enable CAN hardware abstraction layer"          ON)
option(INTERFACE_ENABLE_UDS       "Enable UDS client (ISO 14229)"                  ON)
option(INTERFACE_ENABLE_CANOPEN   "Enable CANopen stack (CiA 301/302)"             ON)
option(INTERFACE_ENABLE_TUI       "Enable FTXUI terminal user interface"            ON)
option(INTERFACE_ENABLE_TESTING   "Enable building tests (Catch2)"                  ON)

# ---------------------------------------------------------------------------
# HAL adapter toggles (only relevant when INTERFACE_ENABLE_CAN_HAL is ON)
# ---------------------------------------------------------------------------
cmake_dependent_option(INTERFACE_HAL_PCAN
    "Enable Peak PCAN Basic adapter"    ON
    "INTERFACE_ENABLE_CAN_HAL"          OFF
)
cmake_dependent_option(INTERFACE_HAL_IXXAT
    "Enable IXXAT VCI adapter"          ON
    "INTERFACE_ENABLE_CAN_HAL"          OFF
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    cmake_dependent_option(INTERFACE_HAL_SOCKETCAN
        "Enable SocketCAN adapter (Linux only)" ON
        "INTERFACE_ENABLE_CAN_HAL"              OFF
    )
else()
    set(INTERFACE_HAL_SOCKETCAN OFF CACHE BOOL "SocketCAN is Linux-only" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependency constraints
# ---------------------------------------------------------------------------
# CAN DB requires CAN base
if(INTERFACE_ENABLE_CAN_DB AND NOT INTERFACE_ENABLE_CAN)
    message(STATUS "INTERFACE_ENABLE_CAN_DB requires INTERFACE_ENABLE_CAN — enabling CAN")
    set(INTERFACE_ENABLE_CAN ON CACHE BOOL "" FORCE)
endif()

# CAN Trace requires CAN base
if(INTERFACE_ENABLE_CAN_TRACE AND NOT INTERFACE_ENABLE_CAN)
    message(STATUS "INTERFACE_ENABLE_CAN_TRACE requires INTERFACE_ENABLE_CAN — enabling CAN")
    set(INTERFACE_ENABLE_CAN ON CACHE BOOL "" FORCE)
endif()

# CAN HAL requires CAN base
if(INTERFACE_ENABLE_CAN_HAL AND NOT INTERFACE_ENABLE_CAN)
    message(STATUS "INTERFACE_ENABLE_CAN_HAL requires INTERFACE_ENABLE_CAN — enabling CAN")
    set(INTERFACE_ENABLE_CAN ON CACHE BOOL "" FORCE)
endif()

# UDS requires CAN + CAN HAL (for transport)
if(INTERFACE_ENABLE_UDS AND NOT INTERFACE_ENABLE_CAN)
    message(STATUS "INTERFACE_ENABLE_UDS requires INTERFACE_ENABLE_CAN — enabling CAN")
    set(INTERFACE_ENABLE_CAN ON CACHE BOOL "" FORCE)
endif()

# CANopen requires CAN + CAN DB (for EDS/OD)
if(INTERFACE_ENABLE_CANOPEN AND NOT INTERFACE_ENABLE_CAN)
    message(STATUS "INTERFACE_ENABLE_CANOPEN requires INTERFACE_ENABLE_CAN — enabling CAN")
    set(INTERFACE_ENABLE_CAN ON CACHE BOOL "" FORCE)
endif()
if(INTERFACE_ENABLE_CANOPEN AND NOT INTERFACE_ENABLE_CAN_DB)
    message(STATUS "INTERFACE_ENABLE_CANOPEN requires INTERFACE_ENABLE_CAN_DB — enabling CAN_DB")
    set(INTERFACE_ENABLE_CAN_DB ON CACHE BOOL "" FORCE)
endif()

# TUI requires core (always built), rest is optional
# TUI adapts to whatever modules are available

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== interface build configuration ===")
message(STATUS "  CAN base:       ${INTERFACE_ENABLE_CAN}")
message(STATUS "  CAN database:   ${INTERFACE_ENABLE_CAN_DB}")
message(STATUS "  CAN trace:      ${INTERFACE_ENABLE_CAN_TRACE}")
message(STATUS "  CAN HAL:        ${INTERFACE_ENABLE_CAN_HAL}")
if(INTERFACE_ENABLE_CAN_HAL)
    message(STATUS "    PCAN:         ${INTERFACE_HAL_PCAN}")
    message(STATUS "    IXXAT:        ${INTERFACE_HAL_IXXAT}")
    message(STATUS "    SocketCAN:    ${INTERFACE_HAL_SOCKETCAN}")
endif()
message(STATUS "  UDS client:     ${INTERFACE_ENABLE_UDS}")
message(STATUS "  CANopen:        ${INTERFACE_ENABLE_CANOPEN}")
message(STATUS "  TUI:            ${INTERFACE_ENABLE_TUI}")
message(STATUS "  Testing:        ${INTERFACE_ENABLE_TESTING}")
message(STATUS "=====================================")
message(STATUS "")
