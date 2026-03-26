#pragma once

/// @file emcy_consumer.hpp
/// @brief CANopen emergency (EMCY) message consumer.

#include "interface/can/frame.hpp"
#include "interface/core/types.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace interface::canopen {

/// CANopen EMCY COB-ID base (0x80 + node_id).
inline constexpr can_id_t k_emcy_cob_base = 0x080;

/// Parsed emergency event.
struct c_emcy_event {
    node_id_t node;
    std::uint16_t error_code;
    std::uint8_t error_register;
    std::array<byte_t, 5> mfr_data;
    timestamp_us_t timestamp;
};

/// Callback for new emergency events.
using emcy_callback_t = std::function<void(const c_emcy_event&)>;

/// Consumes and stores CANopen emergency frames.
///
/// EMCY frames have COB-ID = 0x80 + node_id and 8 bytes of data:
///   bytes 0-1: error code (little-endian)
///   byte 2:    error register
///   bytes 3-7: manufacturer-specific data
class c_emcy_consumer {
public:
    c_emcy_consumer() = default;

    /// Process a CAN frame — checks if it's an EMCY frame and records it.
    auto process_frame(const can::c_can_frame& frame) -> void;

    /// Register a callback for new emergency events.
    auto set_callback(emcy_callback_t callback) -> void;

    /// Get emergency history for a specific node.
    [[nodiscard]] auto history(node_id_t node_id) const -> std::vector<c_emcy_event>;

    /// Clear all emergency history.
    auto clear_history() -> void;

    /// Clear emergency history for a specific node.
    auto clear_history(node_id_t node_id) -> void;

private:
    std::unordered_map<node_id_t, std::vector<c_emcy_event>> m_history;
    emcy_callback_t m_callback;
};

} // namespace interface::canopen
