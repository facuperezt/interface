# interface

Modern C++23 TUI tool collection for CAN bus analysis, UDS diagnostics, and CANopen exploration.

[![CI](https://github.com/facuperezt/interface/actions/workflows/ci.yml/badge.svg)](https://github.com/facuperezt/interface/actions/workflows/ci.yml)

## Features

### CAN Layer
- **CAN Frames** — CAN 2.0 and CAN FD frame types with formatting and payload views
- **CAN Filters** — ID/mask acceptance filtering (exact, range, standard-only, custom)
- **Bus Statistics** — Frame counting, bus load estimation, per-ID delta-time analysis with sliding windows
- **Message Dispatcher** — Thread-safe frame fan-out to multiple subscribers by ID or filter
- **Protocol Sequence Detector** — State-machine pattern matching for handshakes, request/response pairs, and protocol error detection with pre-built rules for UDS and CANopen

### CAN Database
- **DBC Parser** — Custom Vector DBC file parser (no external dependencies) with signal definitions, comments, and value descriptions
- **Signal Decoder** — Decode/encode physical values from raw CAN bytes using DBC signal definitions (little-endian and big-endian)

### CAN Trace
- **ASC Reader/Writer** — Read and write Vector ASC trace files
- **CSV Reader/Writer** — Read and write CSV traces with configurable column mapping and delimiters
- **Trace Replay Engine** — Replay captured traces through a CAN adapter with configurable speed multiplier and step mode

### CAN Hardware Abstraction
- **Abstract Adapter Interface** — Uniform API for all CAN hardware (send, receive, filter, async callback)
- **Mock Adapter** — Full-featured mock for testing (inject RX frames, inspect TX history)
- **HAL Options** — Build toggles for PCAN, IXXAT, and SocketCAN backends

### CAN Scripting Engine
- **JSON-based CAN Scripts** — Define CAN bus simulations (CANopen nodes, UDS servers, ECUs) as JSON scripts with triggers, actions, and flow control
- **Script Engine** — Execute scripts with immediate, delay, and receive-triggered steps, repeat/loop control, and goto-based branching
- **Multi-frame Support** — Send sequences of frames with configurable inter-frame delays for ISO-TP and protocol simulation

### UDS (ISO 14229)
- **UDS Client** — Session control, security access (seed/key), DID read/write, routine control, ECU reset, raw requests
- **ISO-TP Transport** — Full ISO 15765-2 with Single Frame, First Frame, Consecutive Frame, and Flow Control

### CANopen (CiA 301/302)
- **Object Dictionary** — In-memory OD with typed entries, sub-entries, and access control
- **EDS Parser** — Electronic Data Sheet parser populating the Object Dictionary
- **SDO Client** — Read/write OD entries via expedited and segmented SDO transfer
- **PDO Mapping & Decoding** — Configure and decode Process Data Objects
- **NMT** — Network Management state/command definitions with string conversion
- **Heartbeat Consumer** — Monitor node liveness, track NMT state transitions, detect timeouts
- **EMCY Consumer** — Parse and store emergency frames with error code history

### TUI
- **Tab-based Interface** — FTXUI terminal UI with Trace Viewer, Database Browser, UDS Console, and CANopen Explorer tabs

## Documentation

See **[docs/USAGE_GUIDE.md](docs/USAGE_GUIDE.md)** for complete API documentation with working code examples for every feature.

## Building

Requires CMake 3.25+ and a C++23 compiler (GCC 14+, Clang 18+, MSVC 19.38+).

```bash
# Development (Debug, all features, sanitizers)
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Release (optimized, no tests)
cmake --preset release
cmake --build --preset release

# Minimal (core + CAN + DB only)
cmake --preset minimal
cmake --build --preset minimal

# CAN tools only (no UDS/CANopen)
cmake --preset can-tools
cmake --build --preset can-tools

# Full (everything enabled)
cmake --preset full
cmake --build --preset full
```

## Project Structure

```
interface/
├── app/                # Main application entry point
├── cmake/              # CMake modules (options, flags, dependencies)
├── docs/               # Documentation and usage guides
├── libs/
│   ├── core/           # Error handling, types, logging, version
│   ├── can/            # Frames, filters, statistics, dispatcher, sequence detector
│   ├── can_db/         # DBC parser, signal decoder, database model
│   ├── can_trace/      # ASC/CSV readers and writers, trace replay
│   ├── can_hal/        # Hardware abstraction (mock, PCAN, IXXAT, SocketCAN)
│   ├── uds/            # UDS client (ISO 14229), ISO-TP transport (ISO 15765-2)
│   ├── canopen/        # Object Dictionary, EDS parser, SDO, PDO, NMT, heartbeat, EMCY
│   └── tui/            # FTXUI terminal user interface
└── .github/workflows/  # CI (GCC 14, Clang 18, MSVC)
```

## CMake Options

| Option | Default | Description |
|---|---|---|
| `INTERFACE_ENABLE_CAN` | ON | CAN base layer (frames, filters, stats, dispatcher, sequence detector) |
| `INTERFACE_ENABLE_CAN_DB` | ON | CAN database parsing (.dbc) and signal decoding |
| `INTERFACE_ENABLE_CAN_TRACE` | ON | Trace file reading/writing (ASC, CSV) and replay |
| `INTERFACE_ENABLE_CAN_HAL` | ON | Hardware abstraction layer |
| `INTERFACE_ENABLE_UDS` | ON | UDS client and ISO-TP transport |
| `INTERFACE_ENABLE_CANOPEN` | ON | CANopen stack (OD, EDS, SDO, PDO, NMT, heartbeat, EMCY) |
| `INTERFACE_ENABLE_TUI` | ON | Terminal UI |
| `INTERFACE_ENABLE_TESTING` | ON | Build Catch2 tests |
| `INTERFACE_HAL_PCAN` | ON | Peak PCAN adapter backend |
| `INTERFACE_HAL_IXXAT` | ON | IXXAT VCI adapter backend |
| `INTERFACE_HAL_SOCKETCAN` | ON* | SocketCAN backend (Linux only) |

## Dependencies

All managed via CMake FetchContent — no manual installation needed:

- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) v5.0 — Terminal UI
- [Catch2](https://github.com/catchorg/Catch2) v3.7 — Testing
- [spdlog](https://github.com/gabime/spdlog) v1.15 — Logging (header-only mode)
- [nlohmann/json](https://github.com/nlohmann/json) — JSON

## Naming Conventions

| Prefix | Meaning | Example |
|---|---|---|
| `c_` | Class | `c_can_frame`, `c_uds_client` |
| `i_` | Interface (abstract) | `i_can_adapter`, `i_trace_reader` |
| `e_` | Enum | `e_nmt_state`, `e_byte_order` |
| `k_` | Constant | `k_can_max_dlc`, `k_heartbeat_cob_base` |
| `m_` | Member variable | `m_adapter`, `m_config` |

## License

MIT
