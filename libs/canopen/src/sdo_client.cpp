/// @file sdo_client.cpp
/// @brief SDO client implementation.

#include "interface/canopen/sdo_client.hpp"

#include <cstring>
#include <format>

namespace interface::canopen {

// SDO Command Specifiers (CS)
namespace sdo_cs {
    // Client -> Server (request)
    inline constexpr byte_t k_initiate_download_request = 0x20; // base, bits are set for e/s/n
    inline constexpr byte_t k_download_segment_request  = 0x00; // base
    inline constexpr byte_t k_initiate_upload_request   = 0x40;
    inline constexpr byte_t k_upload_segment_request    = 0x60;

    // Server -> Client (response)
    inline constexpr byte_t k_initiate_download_response = 0x60;
    inline constexpr byte_t k_download_segment_response  = 0x20;
    inline constexpr byte_t k_initiate_upload_response   = 0x40; // base, bits are set for e/s/n
    inline constexpr byte_t k_upload_segment_response    = 0x00; // base
    inline constexpr byte_t k_abort_transfer             = 0x80;
} // namespace sdo_cs

c_sdo_client::c_sdo_client(
    std::shared_ptr<can_hal::i_can_adapter> adapter,
    c_sdo_config config
) : m_adapter{std::move(adapter)}, m_config{config} {}

auto c_sdo_client::set_node_id(node_id_t node_id) -> void {
    m_config.node_id = node_id;
}

auto c_sdo_client::send_and_receive(const can::c_can_frame& request)
    -> result_t<can::c_can_frame>
{
    if (!m_adapter || !m_adapter->is_open()) {
        return make_error("CAN adapter not open", e_error_category::hardware);
    }

    auto send_result = m_adapter->send(request);
    if (!send_result) {
        return std::unexpected(send_result.error());
    }

    auto rx_result = m_adapter->receive(m_config.timeout);
    if (!rx_result) {
        return std::unexpected(rx_result.error());
    }
    if (!rx_result->has_value()) {
        return make_error("SDO response timeout", e_error_category::timeout);
    }

    return rx_result->value();
}

auto c_sdo_client::check_abort(const can::c_can_frame& response)
    -> std::optional<c_error>
{
    if ((response.data[0] & 0xE0) == sdo_cs::k_abort_transfer) {
        std::uint32_t abort_code = 0;
        abort_code |= static_cast<std::uint32_t>(response.data[4]);
        abort_code |= static_cast<std::uint32_t>(response.data[5]) << 8;
        abort_code |= static_cast<std::uint32_t>(response.data[6]) << 16;
        abort_code |= static_cast<std::uint32_t>(response.data[7]) << 24;

        return c_error::make(
            std::format("SDO abort: 0x{:08X}", abort_code),
            e_error_category::protocol
        );
    }
    return std::nullopt;
}

auto c_sdo_client::upload(std::uint16_t index, std::uint8_t sub_index)
    -> result_t<byte_buffer_t>
{
    can_id_t tx_cob = static_cast<can_id_t>(0x600 + m_config.node_id);
    can_id_t rx_cob = static_cast<can_id_t>(0x580 + m_config.node_id);

    // Initiate Upload Request
    can::c_can_frame request{};
    request.id = tx_cob;
    request.dlc = 8;
    request.data[0] = sdo_cs::k_initiate_upload_request;
    request.data[1] = static_cast<byte_t>(index & 0xFF);
    request.data[2] = static_cast<byte_t>((index >> 8) & 0xFF);
    request.data[3] = sub_index;

    auto response_result = send_and_receive(request);
    if (!response_result) {
        return std::unexpected(response_result.error());
    }

    auto& response = *response_result;
    if (response.id != rx_cob) {
        return make_error("Unexpected SDO response COB-ID", e_error_category::protocol);
    }

    if (auto abort = check_abort(response)) {
        return std::unexpected(*abort);
    }

    auto cs = response.data[0];

    // Check if expedited (bit 1 set = e=1)
    bool expedited = (cs & 0x02) != 0;
    bool size_indicated = (cs & 0x01) != 0;

    if (expedited) {
        // Expedited upload: data in bytes 4-7
        std::size_t data_size = 4;
        if (size_indicated) {
            auto n = (cs >> 2) & 0x03; // Number of bytes that do NOT contain data
            data_size = 4 - static_cast<std::size_t>(n);
        }
        byte_buffer_t data(response.data.begin() + 4,
                           response.data.begin() + 4 + static_cast<std::ptrdiff_t>(data_size));
        return data;
    }

    // Segmented upload
    std::uint32_t total_size = 0;
    if (size_indicated) {
        total_size |= static_cast<std::uint32_t>(response.data[4]);
        total_size |= static_cast<std::uint32_t>(response.data[5]) << 8;
        total_size |= static_cast<std::uint32_t>(response.data[6]) << 16;
        total_size |= static_cast<std::uint32_t>(response.data[7]) << 24;
    }

    byte_buffer_t data;
    if (total_size > 0) {
        data.reserve(total_size);
    }

    bool toggle = false;
    bool last = false;
    while (!last) {
        can::c_can_frame seg_req{};
        seg_req.id = tx_cob;
        seg_req.dlc = 8;
        seg_req.data[0] = static_cast<byte_t>(sdo_cs::k_upload_segment_request | (toggle ? 0x10 : 0x00));

        auto seg_result = send_and_receive(seg_req);
        if (!seg_result) {
            return std::unexpected(seg_result.error());
        }

        auto& seg_response = *seg_result;
        if (auto abort = check_abort(seg_response)) {
            return std::unexpected(*abort);
        }

        auto seg_cs = seg_response.data[0];
        // Verify toggle bit
        bool resp_toggle = (seg_cs & 0x10) != 0;
        if (resp_toggle != toggle) {
            return make_error("SDO segment toggle bit mismatch", e_error_category::protocol);
        }

        last = (seg_cs & 0x01) != 0;
        auto n = (seg_cs >> 1) & 0x07; // Bytes that don't contain data
        auto seg_data_size = 7 - static_cast<std::size_t>(n);

        for (std::size_t i = 0; i < seg_data_size; ++i) {
            data.push_back(seg_response.data[1 + i]);
        }

        toggle = !toggle;
    }

    return data;
}

auto c_sdo_client::download(
    std::uint16_t index,
    std::uint8_t sub_index,
    byte_span_t data
) -> void_result_t
{
    can_id_t tx_cob = static_cast<can_id_t>(0x600 + m_config.node_id);
    can_id_t rx_cob = static_cast<can_id_t>(0x580 + m_config.node_id);

    if (data.size() <= 4) {
        // Expedited download
        auto n = static_cast<byte_t>(4 - data.size()); // Unused bytes
        byte_t cs = static_cast<byte_t>(sdo_cs::k_initiate_download_request | 0x03 | (n << 2));
        // 0x03 = e=1, s=1

        can::c_can_frame request{};
        request.id = tx_cob;
        request.dlc = 8;
        request.data[0] = cs;
        request.data[1] = static_cast<byte_t>(index & 0xFF);
        request.data[2] = static_cast<byte_t>((index >> 8) & 0xFF);
        request.data[3] = sub_index;
        for (std::size_t i = 0; i < data.size(); ++i) {
            request.data[4 + i] = data[i];
        }

        auto response_result = send_and_receive(request);
        if (!response_result) {
            return std::unexpected(response_result.error());
        }

        if (response_result->id != rx_cob) {
            return make_error("Unexpected SDO response COB-ID", e_error_category::protocol);
        }
        if (auto abort = check_abort(*response_result)) {
            return std::unexpected(*abort);
        }

        return {};
    }

    // Segmented download
    // Initiate download request with size indication
    can::c_can_frame init_req{};
    init_req.id = tx_cob;
    init_req.dlc = 8;
    init_req.data[0] = static_cast<byte_t>(sdo_cs::k_initiate_download_request | 0x01); // s=1, e=0
    init_req.data[1] = static_cast<byte_t>(index & 0xFF);
    init_req.data[2] = static_cast<byte_t>((index >> 8) & 0xFF);
    init_req.data[3] = sub_index;
    auto size = static_cast<std::uint32_t>(data.size());
    init_req.data[4] = static_cast<byte_t>(size & 0xFF);
    init_req.data[5] = static_cast<byte_t>((size >> 8) & 0xFF);
    init_req.data[6] = static_cast<byte_t>((size >> 16) & 0xFF);
    init_req.data[7] = static_cast<byte_t>((size >> 24) & 0xFF);

    auto init_result = send_and_receive(init_req);
    if (!init_result) {
        return std::unexpected(init_result.error());
    }
    if (init_result->id != rx_cob) {
        return make_error("Unexpected SDO response COB-ID", e_error_category::protocol);
    }
    if (auto abort = check_abort(*init_result)) {
        return std::unexpected(*abort);
    }

    // Send segments
    bool toggle = false;
    std::size_t offset = 0;
    while (offset < data.size()) {
        auto remaining = data.size() - offset;
        auto seg_size = std::min(remaining, static_cast<std::size_t>(7));
        bool last_segment = (offset + seg_size >= data.size());
        auto n = static_cast<byte_t>(7 - seg_size);

        can::c_can_frame seg_req{};
        seg_req.id = tx_cob;
        seg_req.dlc = 8;
        seg_req.data[0] = static_cast<byte_t>(
            (toggle ? 0x10 : 0x00) | (n << 1) | (last_segment ? 0x01 : 0x00)
        );

        for (std::size_t i = 0; i < seg_size; ++i) {
            seg_req.data[1 + i] = data[offset + i];
        }

        auto seg_result = send_and_receive(seg_req);
        if (!seg_result) {
            return std::unexpected(seg_result.error());
        }
        if (auto abort = check_abort(*seg_result)) {
            return std::unexpected(*abort);
        }

        // Verify toggle
        bool resp_toggle = (seg_result->data[0] & 0x10) != 0;
        if (resp_toggle != toggle) {
            return make_error("SDO segment toggle bit mismatch", e_error_category::protocol);
        }

        offset += seg_size;
        toggle = !toggle;
    }

    return {};
}

} // namespace interface::canopen
