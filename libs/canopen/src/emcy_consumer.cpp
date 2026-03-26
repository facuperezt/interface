/// @file emcy_consumer.cpp
/// @brief CANopen EMCY consumer implementation.

#include "interface/canopen/emcy_consumer.hpp"

namespace interface::canopen {

auto c_emcy_consumer::process_frame(const can::c_can_frame& frame) -> void {
    // EMCY COB-ID: 0x80 + node_id, 8 bytes data
    if (frame.id < k_emcy_cob_base + 1 || frame.id > k_emcy_cob_base + 127) {
        return;
    }
    if (frame.dlc < 8) {
        return;
    }

    auto node_id = static_cast<node_id_t>(frame.id - k_emcy_cob_base);

    c_emcy_event event{};
    event.node = node_id;
    // Error code: bytes 0-1, little-endian
    event.error_code = static_cast<std::uint16_t>(frame.data[0]) |
                       (static_cast<std::uint16_t>(frame.data[1]) << 8);
    event.error_register = frame.data[2];
    // Manufacturer-specific data: bytes 3-7
    for (std::size_t i = 0; i < 5; ++i) {
        event.mfr_data[i] = frame.data[3 + i];
    }
    event.timestamp = frame.timestamp;

    m_history[node_id].push_back(event);

    if (m_callback) {
        m_callback(event);
    }
}

auto c_emcy_consumer::set_callback(emcy_callback_t callback) -> void {
    m_callback = std::move(callback);
}

auto c_emcy_consumer::history(node_id_t node_id) const -> std::vector<c_emcy_event> {
    auto it = m_history.find(node_id);
    if (it == m_history.end()) {
        return {};
    }
    return it->second;
}

auto c_emcy_consumer::clear_history() -> void {
    m_history.clear();
}

auto c_emcy_consumer::clear_history(node_id_t node_id) -> void {
    m_history.erase(node_id);
}

} // namespace interface::canopen
