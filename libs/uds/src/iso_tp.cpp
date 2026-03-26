/// @file iso_tp.cpp
/// @brief ISO-TP transport layer implementation.

#include "interface/uds/iso_tp.hpp"

#include <algorithm>
#include <format>
#include <thread>

namespace interface::uds {

c_isotp_transport::c_isotp_transport(
    std::shared_ptr<can_hal::i_can_adapter> adapter,
    c_isotp_config config
) : m_adapter{std::move(adapter)}, m_config{config} {}

auto c_isotp_transport::set_addressing(can_id_t tx_id, can_id_t rx_id) -> void {
    m_config.tx_id = tx_id;
    m_config.rx_id = rx_id;
}

auto c_isotp_transport::config() const -> const c_isotp_config& {
    return m_config;
}

auto c_isotp_transport::send(byte_span_t data) -> void_result_t {
    if (!m_adapter || !m_adapter->is_open()) {
        return make_error("CAN adapter not open", e_error_category::hardware);
    }

    if (data.empty()) {
        return make_error("Cannot send empty ISO-TP message", e_error_category::protocol);
    }

    if (data.size() <= 7) {
        return send_single_frame(data);
    }
    return send_multi_frame(data);
}

auto c_isotp_transport::receive() -> result_t<byte_buffer_t> {
    if (!m_adapter || !m_adapter->is_open()) {
        return make_error("CAN adapter not open", e_error_category::hardware);
    }

    auto rx_result = m_adapter->receive(m_config.timeout);
    if (!rx_result) {
        return std::unexpected(rx_result.error());
    }
    if (!rx_result->has_value()) {
        return make_error("ISO-TP receive timeout", e_error_category::timeout);
    }

    auto& frame = rx_result->value();
    auto frame_type = static_cast<std::uint8_t>(frame.data[0] & 0xF0);

    switch (frame_type) {
        case static_cast<std::uint8_t>(e_isotp_frame_type::single_frame):
            return receive_single_frame(frame);
        case static_cast<std::uint8_t>(e_isotp_frame_type::first_frame):
            return receive_multi_frame(frame);
        default:
            return make_error(
                std::format("Unexpected ISO-TP frame type: 0x{:02X}", frame_type),
                e_error_category::protocol
            );
    }
}

auto c_isotp_transport::send_single_frame(byte_span_t data) -> void_result_t {
    can::c_can_frame frame{};
    frame.id = m_config.tx_id;
    frame.dlc = 8;

    // SF PCI: upper nibble = 0, lower nibble = data length
    frame.data[0] = static_cast<byte_t>(data.size());

    for (std::size_t i = 0; i < data.size(); ++i) {
        frame.data[i + 1] = data[i];
    }
    // Pad remaining bytes
    for (auto i = data.size() + 1; i < 8; ++i) {
        frame.data[i] = m_config.padding_byte;
    }

    return m_adapter->send(frame);
}

auto c_isotp_transport::send_multi_frame(byte_span_t data) -> void_result_t {
    // Send First Frame
    can::c_can_frame ff{};
    ff.id = m_config.tx_id;
    ff.dlc = 8;

    auto total_len = static_cast<std::uint16_t>(data.size());
    // FF PCI: upper nibble = 1, next 12 bits = length
    ff.data[0] = static_cast<byte_t>(0x10 | ((total_len >> 8) & 0x0F));
    ff.data[1] = static_cast<byte_t>(total_len & 0xFF);

    // First 6 bytes of data in the first frame
    std::size_t ff_data_len = std::min(data.size(), static_cast<std::size_t>(6));
    for (std::size_t i = 0; i < ff_data_len; ++i) {
        ff.data[2 + i] = data[i];
    }

    auto send_result = m_adapter->send(ff);
    if (!send_result) {
        return send_result;
    }

    // Wait for Flow Control
    auto fc_result = wait_for_flow_control();
    if (!fc_result) {
        return std::unexpected(fc_result.error());
    }

    auto& fc_frame = *fc_result;
    auto fc_status = static_cast<e_flow_status>(fc_frame.data[0] & 0x0F);
    if (fc_status == e_flow_status::overflow_abort) {
        return make_error("ISO-TP receiver reported overflow", e_error_category::protocol);
    }

    auto fc_block_size = fc_frame.data[1];
    auto fc_st_min = fc_frame.data[2];

    // Determine separation time
    std::chrono::microseconds st_min_us;
    if (fc_st_min <= 127) {
        st_min_us = std::chrono::milliseconds{fc_st_min};
    } else if (fc_st_min >= 0xF1 && fc_st_min <= 0xF9) {
        st_min_us = std::chrono::microseconds{(fc_st_min - 0xF0) * 100};
    } else {
        st_min_us = std::chrono::milliseconds{fc_st_min};
    }

    // Send Consecutive Frames
    std::size_t offset = ff_data_len;
    std::uint8_t sequence_number = 1;
    std::uint8_t block_count = 0;

    while (offset < data.size()) {
        // Check block size — wait for new FC if needed
        if (fc_block_size > 0 && block_count >= fc_block_size) {
            auto new_fc = wait_for_flow_control();
            if (!new_fc) {
                return std::unexpected(new_fc.error());
            }
            fc_status = static_cast<e_flow_status>(new_fc->data[0] & 0x0F);
            if (fc_status == e_flow_status::overflow_abort) {
                return make_error("ISO-TP receiver reported overflow", e_error_category::protocol);
            }
            fc_block_size = new_fc->data[1];
            fc_st_min = new_fc->data[2];
            block_count = 0;
        }

        can::c_can_frame cf{};
        cf.id = m_config.tx_id;
        cf.dlc = 8;

        // CF PCI: upper nibble = 2, lower nibble = sequence number (0-F, wraps)
        cf.data[0] = static_cast<byte_t>(0x20 | (sequence_number & 0x0F));

        auto remaining = data.size() - offset;
        auto cf_data_len = std::min(remaining, static_cast<std::size_t>(7));
        for (std::size_t i = 0; i < cf_data_len; ++i) {
            cf.data[1 + i] = data[offset + i];
        }
        // Pad
        for (auto i = cf_data_len + 1; i < 8; ++i) {
            cf.data[i] = m_config.padding_byte;
        }

        // Separation time between consecutive frames
        if (st_min_us.count() > 0 && offset > ff_data_len) {
            std::this_thread::sleep_for(st_min_us);
        }

        auto cf_result = m_adapter->send(cf);
        if (!cf_result) {
            return cf_result;
        }

        offset += cf_data_len;
        sequence_number = static_cast<std::uint8_t>((sequence_number + 1) & 0x0F);
        block_count++;
    }

    return {};
}

auto c_isotp_transport::wait_for_flow_control() -> result_t<can::c_can_frame> {
    std::uint8_t wait_count = 0;

    while (true) {
        auto rx_result = m_adapter->receive(m_config.timeout);
        if (!rx_result) {
            return std::unexpected(rx_result.error());
        }
        if (!rx_result->has_value()) {
            return make_error("ISO-TP Flow Control timeout", e_error_category::timeout);
        }

        auto& frame = rx_result->value();
        auto frame_type = static_cast<std::uint8_t>(frame.data[0] & 0xF0);
        if (frame_type != static_cast<std::uint8_t>(e_isotp_frame_type::flow_control)) {
            return make_error(
                std::format("Expected Flow Control, got frame type 0x{:02X}", frame_type),
                e_error_category::protocol
            );
        }

        auto status = static_cast<e_flow_status>(frame.data[0] & 0x0F);
        if (status == e_flow_status::wait) {
            wait_count++;
            if (wait_count >= m_config.max_fc_wait) {
                return make_error("ISO-TP max FC.Wait count exceeded", e_error_category::protocol);
            }
            continue;
        }

        return frame;
    }
}

auto c_isotp_transport::receive_single_frame(const can::c_can_frame& frame)
    -> result_t<byte_buffer_t>
{
    auto length = frame.data[0] & 0x0F;
    if (length == 0 || length > 7) {
        return make_error(
            std::format("Invalid SF length: {}", length),
            e_error_category::protocol
        );
    }

    byte_buffer_t data(frame.data.begin() + 1,
                       frame.data.begin() + 1 + length);
    return data;
}

auto c_isotp_transport::receive_multi_frame(const can::c_can_frame& ff)
    -> result_t<byte_buffer_t>
{
    // Parse First Frame
    auto total_len = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(ff.data[0] & 0x0F) << 8) | ff.data[1]
    );

    if (total_len == 0) {
        return make_error("ISO-TP First Frame with zero length", e_error_category::protocol);
    }

    byte_buffer_t data;
    data.reserve(total_len);

    // FF carries up to 6 bytes
    auto ff_data_len = std::min(static_cast<std::size_t>(total_len), static_cast<std::size_t>(6));
    for (std::size_t i = 0; i < ff_data_len; ++i) {
        data.push_back(ff.data[2 + i]);
    }

    // Send Flow Control
    auto fc_result = send_flow_control(e_flow_status::continue_to_send);
    if (!fc_result) {
        return std::unexpected(fc_result.error());
    }

    // Receive Consecutive Frames
    std::uint8_t expected_sn = 1;
    while (data.size() < total_len) {
        auto rx_result = m_adapter->receive(m_config.timeout);
        if (!rx_result) {
            return std::unexpected(rx_result.error());
        }
        if (!rx_result->has_value()) {
            return make_error("ISO-TP Consecutive Frame timeout", e_error_category::timeout);
        }

        auto& cf = rx_result->value();
        auto frame_type = static_cast<std::uint8_t>(cf.data[0] & 0xF0);
        if (frame_type != static_cast<std::uint8_t>(e_isotp_frame_type::consecutive_frame)) {
            return make_error(
                std::format("Expected Consecutive Frame, got 0x{:02X}", frame_type),
                e_error_category::protocol
            );
        }

        auto sn = static_cast<std::uint8_t>(cf.data[0] & 0x0F);
        if (sn != (expected_sn & 0x0F)) {
            return make_error(
                std::format("CF sequence number mismatch: expected {}, got {}", expected_sn & 0x0F, sn),
                e_error_category::protocol
            );
        }

        auto remaining = static_cast<std::size_t>(total_len) - data.size();
        auto cf_data_len = std::min(remaining, static_cast<std::size_t>(7));
        for (std::size_t i = 0; i < cf_data_len; ++i) {
            data.push_back(cf.data[1 + i]);
        }

        expected_sn++;
    }

    return data;
}

auto c_isotp_transport::send_flow_control(e_flow_status status) -> void_result_t {
    can::c_can_frame frame{};
    frame.id = m_config.tx_id;
    frame.dlc = 8;

    frame.data[0] = static_cast<byte_t>(0x30 | static_cast<byte_t>(status));
    frame.data[1] = m_config.block_size;
    frame.data[2] = m_config.st_min;
    for (std::size_t i = 3; i < 8; ++i) {
        frame.data[i] = m_config.padding_byte;
    }

    return m_adapter->send(frame);
}

} // namespace interface::uds
