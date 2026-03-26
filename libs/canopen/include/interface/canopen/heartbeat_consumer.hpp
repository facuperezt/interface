#pragma once

/// @file heartbeat_consumer.hpp
/// @brief CANopen heartbeat consumer — monitors node liveness via heartbeat frames.

#include "interface/can/frame.hpp"
#include "interface/canopen/nmt.hpp"
#include "interface/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace interface::canopen {

/// CANopen heartbeat COB-ID base (0x700 + node_id).
inline constexpr can_id_t k_heartbeat_cob_base = 0x700;

/// Callback for NMT state changes: (node_id, old_state, new_state).
using state_change_callback_t =
    std::function<void(node_id_t, e_nmt_state, e_nmt_state)>;

/// Callback for heartbeat timeout: (node_id).
using timeout_callback_t = std::function<void(node_id_t)>;

/// Monitors CANopen heartbeat frames and tracks node states.
///
/// Heartbeat frames have COB-ID = 0x700 + node_id and 1 byte of data
/// containing the NMT state.
class c_heartbeat_consumer {
public:
    /// Construct with a heartbeat timeout duration (in microseconds, matching timestamp_us_t).
    explicit c_heartbeat_consumer(timestamp_us_t timeout_us);

    /// Process a CAN frame — checks if it's a heartbeat and updates state.
    auto process_frame(const can::c_can_frame& frame) -> void;

    /// Start monitoring a specific node.
    auto monitor_node(node_id_t node_id) -> void;

    /// Get the last known NMT state for a node.
    [[nodiscard]] auto node_state(node_id_t node_id) const -> e_nmt_state;

    /// Check if a node's heartbeat was received within the timeout.
    /// Uses the provided current_time for testability (avoids wall clock).
    [[nodiscard]] auto is_alive(node_id_t node_id, timestamp_us_t current_time) const -> bool;

    /// Register a callback for NMT state transitions.
    auto set_state_change_callback(state_change_callback_t callback) -> void;

    /// Register a callback for heartbeat timeout detection.
    auto set_timeout_callback(timeout_callback_t callback) -> void;

    /// Check all monitored nodes for timeout at the given time.
    auto check_timeouts(timestamp_us_t current_time) -> void;

private:
    struct node_info {
        e_nmt_state state{e_nmt_state::initialising};
        timestamp_us_t last_heartbeat{0};
        bool timed_out{false};
    };

    timestamp_us_t m_timeout_us;
    std::unordered_set<node_id_t> m_monitored;
    std::unordered_map<node_id_t, node_info> m_nodes;
    state_change_callback_t m_state_callback;
    timeout_callback_t m_timeout_callback;
};

} // namespace interface::canopen
