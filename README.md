# interface

Modern C++23 TUI tool collection for CAN trace analysis, UDS diagnostics, and CANopen exploration.

## Features

- **CAN Trace Viewer** — Load and analyse .asc, .blf, .csv trace files; live capture via CAN adapters
- **CAN Database Browser** — Parse and browse .dbc, .eds, .cdd files; decode signals in traces
- **UDS Console** — ISO 14229 diagnostic client with session control, security access, DID read/write, routine control
- **CANopen Explorer** — Object Dictionary browser, SDO read/write, PDO mapping and monitor, NMT state machine

## Building

Requires CMake 3.25+ and a C++23 compiler (GCC 13+, Clang 17+, MSVC 19.38+).

```bash
# Configure and build (development preset)
cmake --preset dev
cmake --build --preset dev

# Run tests
ctest --preset dev

# Other presets: release, minimal, can-tools, full
cmake --preset minimal
```

## Project Structure

```
interface/
├── app/            # Main application entry point
├── cmake/          # CMake modules (options, flags, dependencies)
├── libs/
│   ├── core/       # Shared utilities, types, error handling, logging
│   ├── can/        # CAN frame types and filtering
│   ├── can_db/     # Database parsing (.dbc, .eds, .cdd)
│   ├── can_trace/  # Trace file parsing (ASC, BLF, CSV)
│   ├── can_hal/    # Hardware abstraction (PCAN, IXXAT, SocketCAN)
│   ├── uds/        # UDS client (ISO 14229)
│   ├── canopen/    # CANopen stack (CiA 301/302)
│   └── tui/        # FTXUI terminal user interface
└── tests/          # Integration tests
```

## CMake Options

| Option | Default | Description |
|---|---|---|
| `INTERFACE_ENABLE_CAN` | ON | CAN base layer |
| `INTERFACE_ENABLE_CAN_DB` | ON | CAN database parsing |
| `INTERFACE_ENABLE_CAN_TRACE` | ON | Trace file parsing |
| `INTERFACE_ENABLE_CAN_HAL` | ON | Hardware abstraction |
| `INTERFACE_ENABLE_UDS` | ON | UDS client |
| `INTERFACE_ENABLE_CANOPEN` | ON | CANopen stack |
| `INTERFACE_ENABLE_TUI` | ON | Terminal UI |
| `INTERFACE_ENABLE_TESTING` | ON | Build tests |
| `INTERFACE_HAL_PCAN` | ON | Peak PCAN adapter |
| `INTERFACE_HAL_IXXAT` | ON | IXXAT VCI adapter |
| `INTERFACE_HAL_SOCKETCAN` | ON* | SocketCAN (Linux only) |

## Dependencies

All managed via CMake FetchContent:
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — Terminal UI
- [Catch2](https://github.com/catchorg/Catch2) v3 — Testing
- [spdlog](https://github.com/gabime/spdlog) — Logging
- [nlohmann/json](https://github.com/nlohmann/json) — JSON
- [dbcppp](https://github.com/xR3b0rn/dbcppp) — DBC parsing base

## Naming Conventions

- Namespace: `interface::`
- Classes: `c_` prefix (e.g., `c_can_frame`)
- Interfaces: `i_` prefix (e.g., `i_can_adapter`)
- Enums: `e_` prefix (e.g., `e_nmt_state`)
- Constants: `k_` prefix (e.g., `k_can_max_dlc`)
- Members: `m_` prefix

## License

MIT
