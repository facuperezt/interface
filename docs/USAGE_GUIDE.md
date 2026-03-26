# interface — Usage Guide

Comprehensive usage guide for every feature in the `interface` library. Each section includes the relevant headers, class APIs, and complete working code examples.

All examples assume:

```cpp
#include <iostream>
#include <memory>
```

---

## Table of Contents

1. [Core — Error Handling & Types](#1-core--error-handling--types)
2. [CAN Frames & Filters](#2-can-frames--filters)
3. [CAN Bus Statistics](#3-can-bus-statistics)
4. [CAN Message Dispatcher](#4-can-message-dispatcher)
5. [CAN Protocol Sequence Detector](#5-can-protocol-sequence-detector)
6. [CAN Database — DBC Parser & Signal Decoder](#6-can-database--dbc-parser--signal-decoder)
7. [CAN Trace — Readers (ASC, CSV)](#7-can-trace--readers-asc-csv)
8. [CAN Trace — Writers (ASC, CSV)](#8-can-trace--writers-asc-csv)
9. [CAN Trace — Replay Engine](#9-can-trace--replay-engine)
10. [CAN HAL — Hardware Abstraction & Mock Adapter](#10-can-hal--hardware-abstraction--mock-adapter)
11. [UDS Client (ISO 14229)](#11-uds-client-iso-14229)
12. [UDS ISO-TP Transport (ISO 15765-2)](#12-uds-iso-tp-transport-iso-15765-2)
13. [CANopen — Object Dictionary & EDS Parser](#13-canopen--object-dictionary--eds-parser)
14. [CANopen — SDO Client](#14-canopen--sdo-client)
15. [CANopen — PDO Mapping & Decoding](#15-canopen--pdo-mapping--decoding)
16. [CANopen — NMT](#16-canopen--nmt)
17. [CANopen — Heartbeat Consumer](#17-canopen--heartbeat-consumer)
18. [CANopen — EMCY Consumer](#18-canopen--emcy-consumer)
19. [TUI Application](#19-tui-application)
20. [Customizable Keyboard Shortcuts](#20-customizable-keyboard-shortcuts)

---

## 1. Core — Error Handling & Types

**Headers:**
- `interface/core/types.hpp`
- `interface/core/error.hpp`
- `interface/core/log.hpp`
- `interface/core/version.hpp`

### Type Aliases

```cpp
#include "interface/core/types.hpp"

using interface::byte_t;           // std::uint8_t
using interface::byte_buffer_t;    // std::vector<byte_t>
using interface::byte_span_t;      // std::span<const byte_t>
using interface::mutable_byte_span_t;
using interface::timestamp_us_t;   // std::int64_t — microseconds
using interface::can_id_t;         // std::uint32_t — 29-bit max
using interface::node_id_t;        // std::uint8_t — 0–127
using interface::service_id_t;     // std::uint8_t
```

### Error Handling with `result_t<T>`

The library uses C++23 `std::expected` uniformly. Every fallible operation returns `result_t<T>` (or `void_result_t` for operations with no return value). Errors carry a message, category, file, and line number.

```cpp
#include "interface/core/error.hpp"

using namespace interface;

// Returning errors from your own functions:
auto divide(int a, int b) -> result_t<double> {
    if (b == 0) {
        return make_error("Division by zero", e_error_category::generic);
    }
    return static_cast<double>(a) / b;
}

// Consuming results:
auto result = divide(10, 0);
if (result.has_value()) {
    std::cout << "Result: " << *result << "\n";
} else {
    std::cout << "Error: " << result.error().format() << "\n";
    // Prints: [file.cpp:5] Division by zero
}

// Error categories:
// e_error_category::generic, io, parse, protocol, hardware, config, timeout
```

### Version

```cpp
#include "interface/core/version.hpp"

std::cout << "Version: " << interface::k_version_string << "\n";
```

---

## 2. CAN Frames & Filters

**Headers:**
- `interface/can/frame.hpp`
- `interface/can/filter.hpp`

### CAN 2.0 Frames

```cpp
#include "interface/can/frame.hpp"

using namespace interface::can;

// Create a CAN 2.0 frame
c_can_frame frame{};
frame.id  = 0x123;
frame.dlc = 4;
frame.data = {0xDE, 0xAD, 0xBE, 0xEF};
frame.timestamp = 1'500'000; // 1.5 seconds in microseconds

// Access payload as a span
byte_span_t payload = frame.payload(); // 4 bytes

// Display
std::cout << frame.format() << "\n";
// Output: 0x123 [4] DE AD BE EF

// Extended (29-bit) frame
c_can_frame ext_frame{};
ext_frame.id = 0x18FEF100;
ext_frame.flags.extended = true;
```

### CAN FD Frames

```cpp
c_canfd_frame fd_frame{};
fd_frame.id  = 0x200;
fd_frame.dlc = 15; // DLC 15 → 64 data bytes
fd_frame.flags.brs = true; // Bit Rate Switch enabled

std::cout << "FD data length: " << fd_frame.data_length() << "\n";
// Output: FD data length: 64
```

### CAN Filters

```cpp
#include "interface/can/filter.hpp"

using namespace interface::can;

// Exact match — only ID 0x100
auto exact = c_can_filter::exact(0x100);
assert(exact.matches(0x100));   // true
assert(!exact.matches(0x101));  // false

// Accept all frames
auto all = c_can_filter::accept_all();
assert(all.matches(0xABCDEF)); // true

// Standard (11-bit) IDs only
auto std_only = c_can_filter::standard_only();
assert(std_only.matches(0x7FF));   // true
assert(!std_only.matches(0x800));  // false

// Custom mask — match all IDs where bits 10:8 = 0b001 (0x100–0x1FF)
c_can_filter custom{.id = 0x100, .mask = 0x700};
assert(custom.matches(0x1AB));  // true
assert(!custom.matches(0x200)); // false
```

---

## 3. CAN Bus Statistics

**Header:** `interface/can/statistics.hpp`

Collects frame counts, bus load estimation, and per-ID delta-time statistics from a CAN frame stream. Supports a sliding time window for recent-only statistics.

```cpp
#include "interface/can/statistics.hpp"

using namespace interface::can;

// Create with 500 kbit/s bitrate and a 5-second sliding window
c_bus_statistics stats(500'000, 5'000'000);

// Feed frames as they arrive
c_can_frame frame1{.id = 0x100, .dlc = 8, .timestamp = 1'000'000};
c_can_frame frame2{.id = 0x100, .dlc = 8, .timestamp = 1'010'000}; // 10ms later
c_can_frame frame3{.id = 0x200, .dlc = 4, .timestamp = 1'020'000};
c_can_frame frame4{.id = 0x100, .dlc = 8, .timestamp = 1'030'000};

stats.record(frame1);
stats.record(frame2);
stats.record(frame3);
stats.record(frame4);

// Query statistics
std::cout << "Total frames: " << stats.frame_count() << "\n";     // 4
std::cout << "ID 0x100:     " << stats.frame_count(0x100) << "\n"; // 3
std::cout << "ID 0x200:     " << stats.frame_count(0x200) << "\n"; // 1
std::cout << "Bus load:     " << stats.bus_load_percent() << "%\n";

// Delta-time statistics for a specific ID
auto deltas = stats.delta_stats(0x100);
std::cout << "ID 0x100 delta min: " << deltas.min_us << " us\n";  // 10000
std::cout << "ID 0x100 delta max: " << deltas.max_us << " us\n";  // 20000
std::cout << "ID 0x100 delta avg: " << deltas.avg_us << " us\n";  // 15000

// Reset
stats.reset();
```

### Sliding Window

When a window is set, only frames within the most recent `window_us` microseconds are considered. Older frames are pruned automatically when querying.

```cpp
// 1-second window
c_bus_statistics windowed(500'000, 1'000'000);

// Record frames over several seconds
for (int i = 0; i < 100; ++i) {
    c_can_frame f{.id = 0x100, .dlc = 8, .timestamp = i * 50'000};
    windowed.record(f);
}
// frame_count() only returns frames from the last 1 second
// relative to the most recent frame's timestamp
```

---

## 4. CAN Message Dispatcher

**Header:** `interface/can/dispatcher.hpp`

Thread-safe message fan-out: register callbacks for specific CAN IDs or filters, and dispatch incoming frames to all matching subscribers.

```cpp
#include "interface/can/dispatcher.hpp"

using namespace interface::can;

c_dispatcher dispatcher;

// Subscribe to a specific CAN ID
auto token1 = dispatcher.subscribe(0x100, [](const c_can_frame& frame) {
    std::cout << "Got 0x100: " << frame.format() << "\n";
});

// Subscribe with a filter (all IDs 0x200–0x2FF)
auto token2 = dispatcher.subscribe(
    c_can_filter{.id = 0x200, .mask = 0x700},
    [](const c_can_frame& frame) {
        std::cout << "Got 0x2xx: " << frame.format() << "\n";
    }
);

// Dispatch a frame — all matching subscribers are notified
c_can_frame frame{.id = 0x100, .dlc = 2, .data = {0x01, 0x02}};
dispatcher.dispatch(frame);  // triggers callback for token1

c_can_frame frame2{.id = 0x2AB, .dlc = 1, .data = {0xFF}};
dispatcher.dispatch(frame2); // triggers callback for token2

// Unsubscribe when done
dispatcher.unsubscribe(token1);
```

### Integration with CAN HAL

```cpp
// Connect the dispatcher to a CAN adapter's receive callback
auto adapter = std::make_shared<can_hal::c_mock_adapter>();
adapter->open(can_hal::c_bitrate_config{.nominal_bps = 500000});

adapter->set_receive_callback([&dispatcher](const c_can_frame& frame) {
    dispatcher.dispatch(frame);
});

// Now all frames received by the adapter are automatically dispatched
// to all registered subscribers
```

---

## 5. CAN Protocol Sequence Detector

**Header:** `interface/can/sequence_detector.hpp`

Detects multi-frame CAN protocol patterns — handshakes, request/response pairs, and protocol errors. Define rules as ordered sequences of expected frames with CAN ID matching, payload byte matching, and per-step timeouts. The detector watches a frame stream and emits events.

### Byte Matchers

```cpp
#include "interface/can/sequence_detector.hpp"

using namespace interface::can;

// Match any byte value
auto m1 = c_byte_matcher::any();
assert(m1.matches(0x00)); // true
assert(m1.matches(0xFF)); // true

// Match exact byte
auto m2 = c_byte_matcher::exact(0x27);
assert(m2.matches(0x27)); // true
assert(!m2.matches(0x28)); // false

// Match with mask: (byte & 0xF0) == (0x30 & 0xF0)
auto m3 = c_byte_matcher::masked(0x30, 0xF0);
assert(m3.matches(0x30)); // true
assert(m3.matches(0x3F)); // true
assert(!m3.matches(0x40)); // false

// Match a range
auto m4 = c_byte_matcher::range(0x01, 0x05);
assert(m4.matches(0x03)); // true
assert(!m4.matches(0x06)); // false
```

### Defining Custom Sequence Rules

```cpp
// Define a custom 2-step handshake rule
c_sequence_rule my_rule{};
my_rule.name = "MyDevice Handshake";
my_rule.allow_interleaved = true;  // other traffic between steps is OK
my_rule.repeatable = true;         // keep watching after each completion

// Step 1: Request on ID 0x600 with first byte = 0x40
my_rule.steps.push_back(c_sequence_step{
    .label = "Handshake Request",
    .id = 0x600,
    .payload = {c_byte_matcher::exact(0x40), c_byte_matcher::any()},
    .timeout_us = 500'000, // 500ms
});

// Step 2: Response on ID 0x580 with first byte = 0x60
my_rule.steps.push_back(c_sequence_step{
    .label = "Handshake Response",
    .id = 0x580,
    .payload = {c_byte_matcher::exact(0x60)},
    .timeout_us = 1'000'000, // 1 second
});
```

### Using the Detector

```cpp
c_sequence_detector detector;
detector.add_rule(my_rule);

// Listen for events
detector.set_event_callback([](const c_sequence_event& event) {
    switch (event.type) {
        case e_sequence_event_type::sequence_started:
            std::cout << "[STARTED] " << event.rule_name << "\n";
            break;
        case e_sequence_event_type::step_matched:
            std::cout << "[STEP " << event.step_index << "] "
                      << event.step_label << "\n";
            break;
        case e_sequence_event_type::sequence_completed:
            std::cout << "[COMPLETED] " << event.rule_name << "\n";
            break;
        case e_sequence_event_type::step_timeout:
            std::cout << "[TIMEOUT] " << event.rule_name
                      << " at step " << event.step_index << ": "
                      << event.description << "\n";
            break;
        case e_sequence_event_type::unexpected_frame:
            std::cout << "[UNEXPECTED] " << event.description << "\n";
            break;
    }
});

// Feed frames (from a trace file, live adapter, etc.)
c_can_frame req{.id = 0x600, .dlc = 2, .data = {0x40, 0x01}, .timestamp = 1'000'000};
detector.process_frame(req);
// Output: [STARTED] MyDevice Handshake

c_can_frame resp{.id = 0x580, .dlc = 1, .data = {0x60}, .timestamp = 1'200'000};
detector.process_frame(resp);
// Output: [STEP 1] Handshake Response
// Output: [COMPLETED] MyDevice Handshake

// Explicitly check for timeouts (e.g., during trace replay or idle periods)
detector.check_timeouts(5'000'000);
```

### Pre-built Rules for Common Protocols

```cpp
using namespace interface::can::rules;

// UDS request/response — detects a single service request and its positive or
// negative response. TX_ID and RX_ID are the ISO-TP addressing pair.
auto rule1 = uds_request_response(0x7DF, 0x7E8, 0x22, "ReadDID");
// Matches: TX[0x22 ...] → RX[0x62 ...] (positive) or RX[0x7F 0x22 NRC] (negative)

// UDS SecurityAccess full 4-step handshake
auto rule2 = uds_security_access(0x7DF, 0x7E8, 0x01, "SecAccess L1");
// Matches: TX[0x27 0x01] → RX[0x67 0x01 seed...] → TX[0x27 0x02 key...] → RX[0x67 0x02]

// CANopen NMT boot-up detection
auto rule3 = canopen_nmt_bootup(5, "Node5 Bootup");
// Matches: NMT start command on 0x000 → boot-up message on 0x705

// CANopen SDO expedited upload
auto rule4 = canopen_sdo_upload(1, 0x1000, 0x00, "Read DeviceType");
// Matches: SDO upload request → SDO upload response for OD index 0x1000:00

// Generic request/response by first byte
auto rule5 = request_response(0x600, 0x40, 0x580, 0x60, 500'000, "Custom");

// Add all rules to a single detector
c_sequence_detector detector;
detector.add_rule(rule1);
detector.add_rule(rule2);
detector.add_rule(rule3);
detector.add_rule(rule4);
// All rules are checked concurrently on each process_frame() call
```

### Querying Active Sequences

```cpp
auto active = detector.active_sequences();
for (const auto& seq : active) {
    std::cout << seq.rule_name << ": step "
              << seq.current_step << "/" << seq.total_steps << "\n";
}

// Remove a rule (also stops its active tracking)
detector.remove_rule("ReadDID");

// Clear all in-progress tracking (rules remain registered)
detector.reset();
```

---

## 6. CAN Database — DBC Parser & Signal Decoder

**Headers:**
- `interface/can_db/i_database_parser.hpp`
- `interface/can_db/c_dbc_parser.hpp`

### Parsing a DBC File

```cpp
#include "interface/can_db/c_dbc_parser.hpp"

using namespace interface::can_db;

c_dbc_parser parser;

auto result = parser.parse("my_vehicle.dbc");
if (!result) {
    std::cerr << "Parse error: " << result.error().message << "\n";
    return;
}

auto& db = *result;
std::cout << "Database: " << db.name << " (v" << db.version << ")\n";
std::cout << "Messages: " << db.messages.size() << "\n";

// Browse messages
for (const auto& msg : db.messages) {
    std::cout << "  " << msg.name << " (0x"
              << std::hex << msg.id << std::dec
              << ") DLC=" << static_cast<int>(msg.dlc) << "\n";
    for (const auto& sig : msg.signals) {
        std::cout << "    " << sig.name
                  << " [" << sig.start_bit << ":" << sig.length << "]"
                  << " * " << sig.factor << " + " << sig.offset
                  << " " << sig.unit << "\n";
    }
}

// Find a message by CAN ID
auto msg_opt = db.find_message(0x123);
if (msg_opt) {
    std::cout << "Found: " << msg_opt->get().name << "\n";
}
```

### Parsing DBC from a String (for testing)

```cpp
std::string dbc_content = R"(
VERSION ""
NS_ :
BS_:
BU_: ECU1

BO_ 256 EngineData: 8 ECU1
 SG_ EngineSpeed : 0|16@1+ (0.1,0) [0|6500] "rpm" Vector__XXX
 SG_ Throttle : 16|8@1+ (0.4,0) [0|100] "%" Vector__XXX
)";

auto result = parser.parse_string(dbc_content);
```

### Decoding / Encoding Signals

```cpp
#include "interface/can/frame.hpp"

using namespace interface::can;

// Suppose we have a signal definition from the DBC
c_signal_def speed_signal{};
speed_signal.name       = "EngineSpeed";
speed_signal.start_bit  = 0;
speed_signal.length     = 16;
speed_signal.byte_order = e_byte_order::little_endian;
speed_signal.value_type = e_value_type::unsigned_int;
speed_signal.factor     = 0.1;
speed_signal.offset     = 0.0;

// Decode from raw CAN data
c_can_frame frame{.id = 0x100, .dlc = 8, .data = {0xE8, 0x03, 0x00, 0x00}};
double rpm = c_signal_decoder::decode(speed_signal, frame.payload());
std::cout << "Engine speed: " << rpm << " rpm\n"; // 100.0 rpm (raw=1000 * 0.1)

// Encode a physical value back to raw bytes
std::array<byte_t, 8> out_data{};
mutable_byte_span_t out_span{out_data};
auto enc_result = c_signal_decoder::encode(speed_signal, 250.0, out_span);
if (enc_result) {
    // out_data[0..1] now contains the raw value for 250 rpm
}
```

---

## 7. CAN Trace — Readers (ASC, CSV)

**Headers:**
- `interface/can_trace/i_trace_reader.hpp`
- `interface/can_trace/c_asc_reader.hpp`
- `interface/can_trace/c_csv_reader.hpp`

### Reading an ASC Trace File

```cpp
#include "interface/can_trace/c_asc_reader.hpp"

using namespace interface::can_trace;

c_asc_reader reader;
auto open_result = reader.open("capture.asc");
if (!open_result) {
    std::cerr << "Failed: " << open_result.error().message << "\n";
    return;
}

// Read all frames at once
auto all = reader.read_all();
if (all) {
    for (const auto& frame : *all) {
        std::cout << frame.format() << "\n";
    }
}

// Or read frame by frame
reader.reset();
while (true) {
    auto next = reader.read_next();
    if (!next || !next->has_value()) break;
    auto& frame = next->value();
    std::cout << "t=" << frame.timestamp << " " << frame.format() << "\n";
}

// Get trace metadata
auto info = reader.info();
std::cout << "Format: " << info.format
          << ", Frames: " << info.frame_count << "\n";
```

### Reading a CSV Trace File

```cpp
#include "interface/can_trace/c_csv_reader.hpp"

using namespace interface::can_trace;

// Default config: comma-delimited, columns = timestamp, id, dlc, data
c_csv_reader csv_reader;
csv_reader.open("capture.csv");

// Custom column mapping: semicolon delimiter, no header
c_csv_column_config config{};
config.delimiter = ';';
config.has_header = false;
config.timestamp_col = 0;
config.id_col = 1;
config.dlc_col = 2;
config.data_col = 3;
config.data_in_single_column = true;

c_csv_reader custom_reader(config);
custom_reader.open("capture_custom.csv");

auto frames = custom_reader.read_all();
```

### Using the Polymorphic Interface

```cpp
// Both readers implement i_trace_reader — use them interchangeably
std::unique_ptr<i_trace_reader> reader;

if (path.extension() == ".asc") {
    reader = std::make_unique<c_asc_reader>();
} else if (path.extension() == ".csv") {
    reader = std::make_unique<c_csv_reader>();
}

reader->open(path);
auto all_frames = reader->read_all();
```

---

## 8. CAN Trace — Writers (ASC, CSV)

**Headers:**
- `interface/can_trace/c_asc_writer.hpp`
- `interface/can_trace/c_csv_writer.hpp`

### Writing an ASC File

```cpp
#include "interface/can_trace/c_asc_writer.hpp"

using namespace interface::can_trace;
using namespace interface::can;

c_asc_writer writer;
writer.open("output.asc");

// Write frames
c_can_frame frame1{.id = 0x100, .dlc = 4, .data = {0xDE, 0xAD, 0xBE, 0xEF}, .timestamp = 0};
c_can_frame frame2{.id = 0x200, .dlc = 2, .data = {0x01, 0x02}, .timestamp = 500'000};

writer.write(frame1);
writer.write(frame2);

writer.close(); // Flush and finalize
```

### Writing a CSV File

```cpp
#include "interface/can_trace/c_csv_writer.hpp"

using namespace interface::can_trace;

// Default: comma delimiter, header row, data in single column
c_csv_writer csv_writer;
csv_writer.open("output.csv");
csv_writer.write(frame1);
csv_writer.write(frame2);
csv_writer.close();
// Produces:
//   timestamp,id,dlc,data
//   0.000000,100,4,DE AD BE EF
//   0.500000,200,2,01 02

// Semicolon-delimited, no header
c_csv_column_config config{.delimiter = ';', .has_header = false};
c_csv_writer custom_writer(config);
custom_writer.open("output_custom.csv");
```

### Trace Format Conversion

```cpp
// Convert ASC to CSV
c_asc_reader reader;
reader.open("input.asc");

c_csv_writer writer;
writer.open("output.csv");

while (true) {
    auto next = reader.read_next();
    if (!next || !next->has_value()) break;
    writer.write(next->value());
}

writer.close();
```

---

## 9. CAN Trace — Replay Engine

**Header:** `interface/can_trace/c_trace_replayer.hpp`

Replays captured CAN frames through a CAN adapter, respecting original inter-frame timing.

```cpp
#include "interface/can_trace/c_trace_replayer.hpp"
#include "interface/can_trace/c_asc_reader.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;

// Set up reader and adapter
auto reader = std::make_shared<can_trace::c_asc_reader>();
reader->open("capture.asc");

auto adapter = std::make_shared<can_hal::c_mock_adapter>();
adapter->open(can_hal::c_bitrate_config{.nominal_bps = 500000});

// Create replayer
can_trace::c_trace_replayer replayer(reader, adapter);

// Set speed: 1.0 = real-time, 2.0 = 2x speed, 0.0 = as fast as possible
replayer.set_speed_multiplier(1.0);

// Optional: get notified before each frame is sent
replayer.set_frame_callback([](const can::c_can_frame& frame) {
    std::cout << "Sending: " << frame.format() << "\n";
});

// Replay all frames (blocking)
auto result = replayer.replay_all();
if (result) {
    std::cout << "Replayed " << replayer.frames_replayed() << " frames\n";
}
```

### Step-by-Step Replay

```cpp
// Replay one frame at a time (e.g., for debugging)
while (true) {
    auto step = replayer.replay_next();
    if (!step || !step.value()) break;

    std::cout << "Frame " << replayer.frames_replayed() << " sent\n";
    // ... inspect adapter->get_tx_history(), wait for user input, etc.
}
```

---

## 10. CAN HAL — Hardware Abstraction & Mock Adapter

**Headers:**
- `interface/can_hal/i_can_adapter.hpp`
- `interface/can_hal/c_mock_adapter.hpp`

### The Adapter Interface

All CAN hardware backends implement `i_can_adapter`. The interface provides:
- `open(config)` / `close()` / `is_open()`
- `send(frame)` / `receive(timeout)` — blocking I/O
- `set_receive_callback(cb)` — async notification
- `set_filter(filter)` — hardware acceptance filtering
- `info()` — adapter metadata

### Using the Mock Adapter (for Testing)

```cpp
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface::can_hal;
using namespace interface::can;

c_mock_adapter adapter;

// Open the adapter
auto result = adapter.open(c_bitrate_config{.nominal_bps = 500000});
assert(result.has_value());

// Send a frame
c_can_frame tx_frame{.id = 0x100, .dlc = 2, .data = {0x01, 0x02}};
adapter.send(tx_frame);

// Check what was sent
auto history = adapter.get_tx_history();
assert(history.size() == 1);
assert(history[0].id == 0x100);

// Simulate incoming traffic
c_can_frame rx_frame{.id = 0x200, .dlc = 1, .data = {0xFF}};
adapter.inject_rx(rx_frame);

// Receive it
auto received = adapter.receive(std::chrono::milliseconds{100});
assert(received->has_value());
assert(received->value().id == 0x200);

// Set a hardware filter — only accept 0x300
adapter.set_filter(c_can_filter::exact(0x300));

adapter.inject_rx(c_can_frame{.id = 0x200}); // filtered out
adapter.inject_rx(c_can_frame{.id = 0x300}); // passes

auto result2 = adapter.receive(std::chrono::milliseconds{50});
assert(result2->value().id == 0x300);

// Async callback
adapter.set_receive_callback([](const c_can_frame& frame) {
    std::cout << "Async RX: " << frame.format() << "\n";
});
adapter.inject_rx(c_can_frame{.id = 0x300, .dlc = 0});
// Callback fires immediately for matching frames

// Cleanup
adapter.clear_tx_history();
adapter.close();
```

### Implementing a Custom Adapter

```cpp
class c_my_pcan_adapter final : public i_can_adapter {
public:
    auto open(const c_bitrate_config& config) -> void_result_t override {
        // Initialize PCAN hardware
    }
    auto close() -> void override { /* ... */ }
    auto is_open() const noexcept -> bool override { return m_open; }
    auto send(const can::c_can_frame& frame) -> void_result_t override {
        // Transmit via PCAN API
    }
    auto receive(std::chrono::milliseconds timeout)
        -> result_t<std::optional<can::c_can_frame>> override {
        // Read from PCAN API with timeout
    }
    // ... etc.
};
```

---

## 11. UDS Client (ISO 14229)

**Header:** `interface/uds/client.hpp`

Full UDS diagnostic client with session management, security access, DID read/write, routine control, ECU reset, and raw requests.

```cpp
#include "interface/uds/client.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;
using namespace interface::uds;

// Create adapter and client
auto adapter = std::make_shared<can_hal::c_mock_adapter>();
adapter->open(can_hal::c_bitrate_config{});

c_uds_client_config config{};
config.tx_id = 0x7DF;    // Functional request
config.rx_id = 0x7E8;    // ECU response
config.p2_timeout = std::chrono::milliseconds{50};

c_uds_client client(adapter, config);
```

### Diagnostic Session Control

```cpp
auto result = client.diagnostic_session_control(e_session::extended_diagnostic);
if (result && result->positive) {
    std::cout << "Switched to extended session\n";
    std::cout << "Current session: "
              << static_cast<int>(client.current_session()) << "\n";
}
```

### Security Access

```cpp
// Seed-key callback computes the key from the seed
auto key_callback = [](std::uint8_t level, byte_span_t seed) -> result_t<byte_buffer_t> {
    byte_buffer_t key;
    for (auto byte : seed) {
        key.push_back(byte ^ 0xFF); // Example: simple XOR
    }
    return key;
};

auto result = client.security_access(0x01, key_callback);
if (result && result->positive) {
    std::cout << "Security access granted\n";
}
```

### Read / Write Data by Identifier

```cpp
// Read DID 0xF190 (VIN Number)
auto read_result = client.read_data_by_identifier(0xF190);
if (read_result && read_result->positive) {
    std::cout << "DID data: " << read_result->data_hex() << "\n";
}

// Write DID
std::array<byte_t, 4> write_data = {0x01, 0x02, 0x03, 0x04};
auto write_result = client.write_data_by_identifier(0xF199, byte_span_t{write_data});
```

### Routine Control

```cpp
// Start a routine
std::array<byte_t, 2> params = {0x00, 0x01};
auto start = client.start_routine(0x0203, byte_span_t{params});

// Request results
auto results = client.request_routine_results(0x0203);

// Stop routine
auto stop = client.stop_routine(0x0203);
```

### ECU Reset & Tester Present

```cpp
client.tester_present(true); // suppress_response = true

client.ecu_reset(0x01); // hardReset
```

### NRC (Negative Response Code) Handling

```cpp
auto result = client.read_data_by_identifier(0xFFFF);
if (result && !result->positive) {
    std::cout << "Rejected: " << nrc_to_string(*result->nrc) << "\n";
    // e.g., "requestOutOfRange"
}
```

### Raw Requests

```cpp
std::array<byte_t, 3> raw = {0x22, 0xF1, 0x90}; // ReadDID 0xF190
auto result = client.send_raw(byte_span_t{raw});
```

### Runtime Address Change

```cpp
client.set_addressing(0x7E0, 0x7E8); // Switch to physical addressing
```

---

## 12. UDS ISO-TP Transport (ISO 15765-2)

**Header:** `interface/uds/iso_tp.hpp`

The ISO-TP transport handles segmented messaging for UDS payloads larger than 7 bytes. Supports Single Frame, First Frame, Consecutive Frame, and Flow Control.

```cpp
#include "interface/uds/iso_tp.hpp"

using namespace interface::uds;

auto adapter = std::make_shared<can_hal::c_mock_adapter>();
adapter->open(can_hal::c_bitrate_config{});

c_isotp_config config{};
config.tx_id = 0x7E0;
config.rx_id = 0x7E8;
config.block_size = 0;     // No block limit
config.st_min = 10;        // 10ms separation time
config.padding_byte = 0xCC;
config.timeout = std::chrono::milliseconds{1000};

c_isotp_transport transport(adapter, config);

// Send a short message (Single Frame, ≤7 bytes)
std::array<byte_t, 3> short_msg = {0x22, 0xF1, 0x90};
transport.send(byte_span_t{short_msg});

// Send a long message (Multi-Frame, >7 bytes — automatic FF+CF segmentation)
byte_buffer_t long_msg(50, 0xAA); // 50 bytes
transport.send(byte_span_t{long_msg});
// Sends: First Frame + waits for Flow Control + sends Consecutive Frames

// Receive a message (handles reassembly automatically)
auto received = transport.receive();
if (received) {
    std::cout << "Received " << received->size() << " bytes\n";
}

// Update addressing at runtime
transport.set_addressing(0x7E2, 0x7EA);
```

---

## 13. CANopen — Object Dictionary & EDS Parser

**Headers:**
- `interface/canopen/object_dictionary.hpp`
- `interface/canopen/c_eds_parser.hpp`

### Building an Object Dictionary Manually

```cpp
#include "interface/canopen/object_dictionary.hpp"

using namespace interface::canopen;

c_object_dictionary od;

// Add an entry
c_od_entry device_type{};
device_type.index = 0x1000;
device_type.name = "Device Type";
device_type.object_type = e_od_object_type::variable;
device_type.sub_entries.push_back(c_od_sub_entry{
    .sub_index = 0,
    .name = "Device Type",
    .data_type = e_od_data_type::unsigned32,
    .access = e_od_access::read_only,
    .default_value = std::uint32_t{0x000F0191},
});
od.add_entry(device_type);

// Look up
auto entry = od.find(0x1000);
if (entry) {
    std::cout << entry->get().name << "\n"; // "Device Type"
    auto sub = entry->get().find_sub(0);
    if (sub) {
        std::cout << "  Access: "
                  << (sub->get().access == e_od_access::read_only ? "ro" : "rw")
                  << "\n";
    }
}

std::cout << "OD entries: " << od.size() << "\n";
```

### Parsing an EDS File

```cpp
#include "interface/canopen/c_eds_parser.hpp"

using namespace interface::canopen;

c_eds_parser parser;
auto result = parser.parse("device.eds");
if (result) {
    auto& od = *result;
    std::cout << "Loaded " << od.size() << " OD entries from EDS\n";

    // Browse all entries
    for (const auto& [index, entry] : od.entries()) {
        std::cout << std::hex << "0x" << index << ": "
                  << entry.name << "\n";
    }
}
```

---

## 14. CANopen — SDO Client

**Header:** `interface/canopen/sdo_client.hpp`

Reads/writes Object Dictionary entries over SDO (Service Data Object). Supports both expedited transfer (≤4 bytes) and segmented transfer (>4 bytes).

```cpp
#include "interface/canopen/sdo_client.hpp"

using namespace interface::canopen;

auto adapter = std::make_shared<can_hal::c_mock_adapter>();
adapter->open(can_hal::c_bitrate_config{});

c_sdo_config config{};
config.node_id = 5;
config.timeout = std::chrono::milliseconds{1000};
// COB-IDs: TX = 0x600 + node_id = 0x605, RX = 0x580 + node_id = 0x585

c_sdo_client sdo(adapter, config);

// Upload (read) OD entry at index 0x1000, sub-index 0
auto result = sdo.upload(0x1000, 0x00);
if (result) {
    std::cout << "Read " << result->size() << " bytes\n";
    // For a UNSIGNED32: interpret as 4 bytes little-endian
}

// Download (write) OD entry
std::array<byte_t, 4> data = {0x01, 0x00, 0x00, 0x00};
auto write_result = sdo.download(0x2000, 0x01, byte_span_t{data});
if (!write_result) {
    std::cout << "Write failed: " << write_result.error().message << "\n";
}

// Change target node at runtime
sdo.set_node_id(10); // Now talks to node 10 (COB-IDs: 0x60A / 0x58A)
```

### SDO Abort Codes

If the remote node aborts the transfer, the error message includes the abort code:

```cpp
// e_sdo_abort::object_not_found       = 0x06020000
// e_sdo_abort::read_only              = 0x06010001
// e_sdo_abort::type_mismatch_length   = 0x06070010
// ... etc.
```

---

## 15. CANopen — PDO Mapping & Decoding

**Header:** `interface/canopen/pdo.hpp`

Decodes Process Data Object (PDO) frames using a mapping configuration and the Object Dictionary.

```cpp
#include "interface/canopen/pdo.hpp"
#include "interface/canopen/object_dictionary.hpp"
#include "interface/can/frame.hpp"

using namespace interface::canopen;
using namespace interface::can;

// Build an Object Dictionary with the mapped entries
c_object_dictionary od;
// ... (add OD entries for the mapped objects)

// Define a TPDO configuration
c_pdo_config tpdo1{};
tpdo1.pdo_number = 0;
tpdo1.enabled = true;
tpdo1.cob_id = 0x181; // Default TPDO1 for node 1
tpdo1.transmission_type = e_pdo_transmission_type::event_driven;

// Map 2 objects: 16-bit speed + 8-bit status
tpdo1.mapping.push_back(c_pdo_map_entry{
    .index = 0x6040, .sub_index = 0, .bit_length = 16
});
tpdo1.mapping.push_back(c_pdo_map_entry{
    .index = 0x6041, .sub_index = 0, .bit_length = 8
});

std::cout << "Total mapped: " << static_cast<int>(tpdo1.total_bytes())
          << " bytes\n"; // 3 bytes

// Decode a received PDO frame
c_can_frame pdo_frame{.id = 0x181, .dlc = 3, .data = {0xE8, 0x03, 0x05}};

auto decoded = decode_pdo(pdo_frame, tpdo1, od);
if (decoded) {
    for (const auto& val : *decoded) {
        std::cout << val.name << " (0x" << std::hex << val.index
                  << ":" << static_cast<int>(val.sub_index) << std::dec
                  << ") = " << val.value << "\n";
    }
}
```

---

## 16. CANopen — NMT

**Header:** `interface/canopen/nmt.hpp`

NMT state and command definitions with human-readable string conversion.

```cpp
#include "interface/canopen/nmt.hpp"

using namespace interface::canopen;

// NMT states
auto state = e_nmt_state::operational;
std::cout << nmt_state_to_string(state) << "\n"; // "Operational"

// NMT commands
auto cmd = e_nmt_command::start_remote_node;
std::cout << nmt_command_to_string(cmd) << "\n"; // "Start Remote Node"

// Build an NMT command frame (COB-ID = 0x000)
can::c_can_frame nmt_frame{};
nmt_frame.id = 0x000;
nmt_frame.dlc = 2;
nmt_frame.data[0] = static_cast<byte_t>(e_nmt_command::start_remote_node); // 0x01
nmt_frame.data[1] = 5; // Target node ID (0 = all nodes)
```

---

## 17. CANopen — Heartbeat Consumer

**Header:** `interface/canopen/heartbeat_consumer.hpp`

Monitors CANopen heartbeat frames (COB-ID 0x700 + node_id) to track node states and detect timeouts.

```cpp
#include "interface/canopen/heartbeat_consumer.hpp"

using namespace interface::canopen;
using namespace interface::can;

// 2-second timeout
c_heartbeat_consumer heartbeat(2'000'000);

// Monitor nodes 1 and 5
heartbeat.monitor_node(1);
heartbeat.monitor_node(5);

// Get notified on state changes
heartbeat.set_state_change_callback(
    [](node_id_t node, e_nmt_state old_state, e_nmt_state new_state) {
        std::cout << "Node " << static_cast<int>(node)
                  << ": " << nmt_state_to_string(old_state)
                  << " -> " << nmt_state_to_string(new_state) << "\n";
    }
);

// Get notified on timeouts
heartbeat.set_timeout_callback([](node_id_t node) {
    std::cerr << "Node " << static_cast<int>(node) << " heartbeat LOST\n";
});

// Process incoming CAN frames
c_can_frame hb_frame{};
hb_frame.id = 0x701;       // Heartbeat from node 1
hb_frame.dlc = 1;
hb_frame.data[0] = 0x05;   // NMT state: Operational
hb_frame.timestamp = 1'000'000;

heartbeat.process_frame(hb_frame);
// Callback fires: "Node 1: Initialising -> Operational"

// Query state
auto state = heartbeat.node_state(1); // e_nmt_state::operational
bool alive = heartbeat.is_alive(1, 2'500'000); // true (within 2s timeout)

// Check all nodes for timeout
heartbeat.check_timeouts(10'000'000); // 10 seconds — node 1 timed out
// Timeout callback fires: "Node 1 heartbeat LOST"
```

---

## 18. CANopen — EMCY Consumer

**Header:** `interface/canopen/emcy_consumer.hpp`

Parses and stores CANopen emergency frames (COB-ID 0x80 + node_id).

```cpp
#include "interface/canopen/emcy_consumer.hpp"

using namespace interface::canopen;
using namespace interface::can;

c_emcy_consumer emcy;

// Get notified on new emergencies
emcy.set_callback([](const c_emcy_event& event) {
    std::cout << "EMCY from node " << static_cast<int>(event.node)
              << ": error_code=0x" << std::hex << event.error_code
              << ", error_reg=0x" << static_cast<int>(event.error_register)
              << std::dec << "\n";
});

// Process an emergency frame
// EMCY format: bytes 0-1 = error code (LE), byte 2 = error register, bytes 3-7 = mfr data
c_can_frame emcy_frame{};
emcy_frame.id = 0x085;     // EMCY from node 5
emcy_frame.dlc = 8;
emcy_frame.data = {0x10, 0x81, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
emcy_frame.timestamp = 5'000'000;

emcy.process_frame(emcy_frame);
// Callback fires: "EMCY from node 5: error_code=0x8110, error_reg=0x01"

// Query history
auto node5_history = emcy.history(5);
for (const auto& event : node5_history) {
    std::cout << "Error 0x" << std::hex << event.error_code
              << " at t=" << event.timestamp << "\n";
}

// Clear history
emcy.clear_history(5);  // Clear for node 5
emcy.clear_history();   // Clear all
```

---

## 19. TUI Application

**Header:** `interface/tui/app.hpp`

Tab-based terminal UI shell built with FTXUI. Provides four tabs: Trace Viewer, Database Browser, UDS Console, and CANopen Explorer.

```cpp
#include "interface/tui/app.hpp"

using namespace interface::tui;

int main() {
    // Simple usage with free function (backward compatible):
    c_app_config config{};
    config.title = "interface";
    config.show_status_bar = true;

    return run(config); // Blocks until user exits

    // Or use the c_app class for more control:
    c_app app(config);
    app.load_keybindings("~/.config/interface/keybindings.json");
    return app.run();
}
```

The TUI runs in fullscreen terminal mode with keyboard navigation between tabs. See section 20 for customizable keyboard shortcuts.

---

## 20. Customizable Keyboard Shortcuts

**Headers:**
- `interface/tui/keybindings.hpp`
- `interface/tui/app.hpp`

The keybindings system lets users remap any keyboard shortcut via a JSON config file. It provides sensible defaults that follow standard TUI conventions.

### Config File Format

Create a JSON file with a `version` field and a `bindings` object mapping action names to key combo strings. Only listed bindings override defaults -- omitted actions keep their default keys.

```json
{
    "version": 1,
    "bindings": {
        "quit": "Ctrl+Q",
        "next_tab": "Tab",
        "prev_tab": "Shift+Tab",
        "search": "Ctrl+F",
        "scroll_up": "k",
        "scroll_down": "j",
        "toggle_hex_dec": "h",
        "export_data": "Ctrl+E"
    }
}
```

Key combo strings support modifiers `Ctrl+`, `Alt+`, `Shift+` followed by a key name. Keys include single characters (`a`, `Q`, `1`), function keys (`F1`-`F12`), and named keys (`Tab`, `Enter`, `Escape`, `Space`, `Up`, `Down`, `Left`, `Right`, `PgUp`, `PgDn`, `Home`, `End`, `Backspace`, `Delete`).

### All Bindable Actions with Defaults

| Category          | Action            | Default Key    |
|-------------------|-------------------|----------------|
| Navigation        | `next_tab`        | `Tab`          |
| Navigation        | `prev_tab`        | `Shift+Tab`    |
| Navigation        | `tab_1`           | `1`            |
| Navigation        | `tab_2`           | `2`            |
| Navigation        | `tab_3`           | `3`            |
| Navigation        | `tab_4`           | `4`            |
| Navigation        | `tab_5`           | `5`            |
| General           | `quit`            | `Ctrl+Q`       |
| General           | `help`            | `F1`           |
| General           | `toggle_focus`    | `F2`           |
| Trace Viewer      | `scroll_up`       | `k`            |
| Trace Viewer      | `scroll_down`     | `j`            |
| Trace Viewer      | `page_up`         | `PgUp`         |
| Trace Viewer      | `page_down`       | `PgDn`         |
| Trace Viewer      | `go_to_top`       | `g`            |
| Trace Viewer      | `go_to_bottom`    | `Shift+G`      |
| Trace Viewer      | `search`          | `Ctrl+F`       |
| Trace Viewer      | `filter`          | `f`            |
| Trace Viewer      | `clear_filter`    | `Escape`       |
| UDS Console       | `send_request`    | `Enter`        |
| UDS Console       | `clear_console`   | `Ctrl+L`       |
| UDS Console       | `history_prev`    | `Up`           |
| UDS Console       | `history_next`    | `Down`         |
| CANopen           | `refresh_od`      | `r`            |
| CANopen           | `start_node`      | `s`            |
| CANopen           | `stop_node`       | `Shift+S`      |
| Common            | `copy`            | `Ctrl+C`       |
| Common            | `export_data`     | `Ctrl+E`       |
| Common            | `toggle_pause`    | `Space`        |
| Common            | `toggle_hex_dec`  | `h`            |
| Sequence Detector | `add_rule`        | `a`            |
| Sequence Detector | `remove_rule`     | `d`            |
| Sequence Detector | `reset_detector`  | `Ctrl+R`       |

### Loading Custom Keybindings in Code

```cpp
#include "interface/tui/keybindings.hpp"
#include "interface/tui/app.hpp"

using namespace interface::tui;

int main() {
    c_app app;

    // Load user overrides (merges with defaults).
    auto result = app.load_keybindings("keybindings.json");
    if (!result) {
        std::cerr << "Warning: " << result.error().message << "\n";
        // Defaults are still active.
    }

    return app.run();
}
```

### Programmatic Keybinding Management

```cpp
#include "interface/tui/keybindings.hpp"

using namespace interface::tui;

c_keybindings kb; // populated with defaults

// Query current bindings
auto quit_combo = kb.combo_for(e_action::quit);       // Ctrl+Q
auto display    = kb.display_string(e_action::quit);   // "Ctrl+Q"
auto name       = c_keybindings::action_name(e_action::quit);     // "quit"
auto category   = c_keybindings::action_category(e_action::quit); // "General"

// Override a binding
kb.bind(e_action::quit, c_key_combo{"W", true}); // Now Ctrl+W

// Unbind and restore
kb.unbind(e_action::quit);
kb.reset_defaults();

// Enumerate all bindings (e.g., for a help screen)
for (const auto& [action, combo] : kb.all_bindings()) {
    std::cout << c_keybindings::action_name(action) << " = "
              << combo.to_string() << "\n";
}

// Save / load
kb.save_to_file("my_keybindings.json");
kb.load_from_file("my_keybindings.json");
```

### Example: Remapping Vim-style to Arrow-style Navigation

```json
{
    "version": 1,
    "bindings": {
        "scroll_up": "Up",
        "scroll_down": "Down",
        "go_to_top": "Home",
        "go_to_bottom": "End",
        "history_prev": "Ctrl+Up",
        "history_next": "Ctrl+Down"
    }
}
```

This replaces the default `j`/`k` Vim-style scrolling with arrow keys and maps go-to-top/bottom to Home/End. All other bindings remain at their defaults.

---

## Putting It All Together

Here's an example combining multiple features: load a trace file, decode signals using a DBC database, collect statistics, and detect protocol sequences.

```cpp
#include "interface/can/statistics.hpp"
#include "interface/can/dispatcher.hpp"
#include "interface/can/sequence_detector.hpp"
#include "interface/can_db/c_dbc_parser.hpp"
#include "interface/can_trace/c_asc_reader.hpp"

using namespace interface;

int main() {
    // Load the DBC database
    can_db::c_dbc_parser dbc_parser;
    auto db = dbc_parser.parse("vehicle.dbc");
    if (!db) return 1;

    // Open a trace file
    can_trace::c_asc_reader reader;
    reader.open("capture.asc");

    // Set up analytics
    can::c_bus_statistics stats(500'000);
    can::c_dispatcher dispatcher;
    can::c_sequence_detector detector;

    // Watch for UDS SecurityAccess handshakes
    detector.add_rule(can::rules::uds_security_access(0x7DF, 0x7E8, 0x01));
    detector.set_event_callback([](const can::c_sequence_event& event) {
        if (event.type == can::e_sequence_event_type::sequence_completed) {
            std::cout << "[OK] " << event.rule_name << " completed\n";
        }
        if (event.type == can::e_sequence_event_type::step_timeout) {
            std::cerr << "[FAIL] " << event.description << "\n";
        }
    });

    // Subscribe to a specific message for signal decoding
    auto engine_msg = db->find_message(0x100);
    if (engine_msg) {
        dispatcher.subscribe(0x100, [&](const can::c_can_frame& frame) {
            for (const auto& sig : engine_msg->get().signals) {
                double value = can_db::c_signal_decoder::decode(sig, frame.payload());
                std::cout << sig.name << " = " << value << " " << sig.unit << "\n";
            }
        });
    }

    // Process the entire trace
    while (true) {
        auto next = reader.read_next();
        if (!next || !next->has_value()) break;

        auto& frame = next->value();
        stats.record(frame);
        dispatcher.dispatch(frame);
        detector.process_frame(frame);
    }

    // Print summary
    std::cout << "\nTotal frames: " << stats.frame_count() << "\n";
    std::cout << "Bus load: " << stats.bus_load_percent() << "%\n";

    return 0;
}
```
