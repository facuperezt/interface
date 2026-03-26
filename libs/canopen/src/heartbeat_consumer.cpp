/// @file heartbeat_consumer.cpp
/// @brief CANopen heartbeat consumer implementation.

#include "interface/canopen/heartbeat_consumer.hpp"

namespace interface::canopen {

c_heartbeat_consumer::c_heartbeat_consumer(timestamp_us_t timeout_us)
    : m_timeout_us{timeout_us} {}

auto c_heartbeat_consumer::process_frame(const can::c_can_frame& frame) -> void {
    // Heartbeat COB-ID: 0x700 + node_id, 1 byte data = NMT state
    if (frame.id < k_heartbeat_cob_base || frame.id > k_heartbeat_cob_base + 127) {
        return;
    }
    if (frame.dlc < 1) {
        return;
    }

    auto node_id = static_cast<node_id_t>(frame.id - k_heartbeat_cob_base);

    // Only process if we're monitoring this node
    if (m_monitored.find(node_id) == m_monitored.end()) {
        return;
    }

    auto new_state = static_cast<e_nmt_state>(frame.data[0]);
    auto& info = m_nodes[node_id];
    auto old_state = info.state;
    bool was_timed_out = info.timed_out;

    info.state = new_state;
    info.last_heartbeat = frame.timestamp;
    info.timed_out = false;

    // Fire state change callback if state changed (or recovering from timeout)
    if ((old_state != new_state || was_timed_out) && m_state_callback) {
        m_state_callback(node_id, old_state, new_state);
    }
}

auto c_heartbeat_consumer::monitor_node(node_id_t node_id) -> void {
    m_monitored.insert(node_id);
}

auto c_heartbeat_consumer::node_state(node_id_t node_id) const -> e_nmt_state {
    auto it = m_nodes.find(node_id);
    if (it == m_nodes.end()) {
        return e_nmt_state::initialising;
    }
    return it->second.state;
}

auto c_heartbeat_consumer::is_alive(node_id_t node_id, timestamp_us_t current_time) const -> bool {
    auto it = m_nodes.find(node_id);
    if (it == m_nodes.end()) {
        return false;
    }
    return (current_time - it->second.last_heartbeat) <= m_timeout_us;
}

auto c_heartbeat_consumer::set_state_change_callback(state_change_callback_t callback) -> void {
    m_state_callback = std::move(callback);
}

auto c_heartbeat_consumer::set_timeout_callback(timeout_callback_t callback) -> void {
    m_timeout_callback = std::move(callback);
}

auto c_heartbeat_consumer::check_timeouts(timestamp_us_t current_time) -> void {
    for (auto node_id : m_monitored) {
        auto it = m_nodes.find(node_id);
        if (it == m_nodes.end()) {
            // Node never sent a heartbeat — treat as timed out
            if (m_timeout_callback) {
                m_timeout_callback(node_id);
            }
            continue;
        }

        auto& info = it->second;
        if (!info.timed_out && (current_time - info.last_heartbeat) > m_timeout_us) {
            info.timed_out = true;
            if (m_timeout_callback) {
                m_timeout_callback(node_id);
            }
        }
    }
}

} // namespace interface::canopen
