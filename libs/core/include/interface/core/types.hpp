#pragma once

/// @file types.hpp
/// @brief Common type aliases and fundamental types used across the library.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace interface {

/// Byte type alias
using byte_t = std::uint8_t;

/// Byte buffer
using byte_buffer_t = std::vector<byte_t>;

/// Non-owning view into a byte buffer
using byte_span_t = std::span<const byte_t>;

/// Mutable byte span
using mutable_byte_span_t = std::span<byte_t>;

/// Timestamp in microseconds since epoch
using timestamp_us_t = std::int64_t;

/// CAN identifier (29-bit max for extended frames)
using can_id_t = std::uint32_t;

/// Node ID for CANopen (0–127)
using node_id_t = std::uint8_t;

/// UDS service ID
using service_id_t = std::uint8_t;

} // namespace interface
