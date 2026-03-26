#pragma once

/// @file sdo_client.hpp
/// @brief CANopen SDO (Service Data Object) client for OD access.

#include "interface/can_hal/i_can_adapter.hpp"
#include "interface/canopen/object_dictionary.hpp"
#include "interface/core/error.hpp"
#include "interface/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

namespace interface::canopen {

/// SDO abort codes (CiA 301 §7.2.4.3.17).
enum class e_sdo_abort : std::uint32_t {
    toggle_bit_not_alternated  = 0x05030000,
    timed_out                  = 0x05040000,
    invalid_command            = 0x05040001,
    invalid_block_size         = 0x05040002,
    invalid_sequence           = 0x05040003,
    crc_error                  = 0x05040004,
    out_of_memory              = 0x05040005,
    unsupported_access         = 0x06010000,
    read_only                  = 0x06010001,
    write_only                 = 0x06010002,
    object_not_found           = 0x06020000,
    hardware_error             = 0x06060000,
    type_mismatch_length       = 0x06070010,
    type_mismatch_too_long     = 0x06070012,
    type_mismatch_too_short    = 0x06070013,
    sub_index_not_found        = 0x06090011,
    value_range_exceeded       = 0x06090030,
    general_error              = 0x08000000,
};

/// SDO client configuration.
struct c_sdo_config {
    node_id_t node_id{1};
    std::chrono::milliseconds timeout{1000};
};

/// SDO client for reading/writing CANopen Object Dictionary entries.
/// Supports expedited transfer (<=4 bytes) and segmented transfer (>4 bytes).
class c_sdo_client {
public:
    c_sdo_client(
        std::shared_ptr<can_hal::i_can_adapter> adapter,
        c_sdo_config config = {}
    );

    /// Upload (read) an OD entry. COB-IDs: TX=0x600+node_id, RX=0x580+node_id.
    [[nodiscard]] auto upload(std::uint16_t index, std::uint8_t sub_index)
        -> result_t<byte_buffer_t>;

    /// Download (write) an OD entry.
    [[nodiscard]] auto download(
        std::uint16_t index,
        std::uint8_t sub_index,
        byte_span_t data
    ) -> void_result_t;

    /// Update node ID.
    auto set_node_id(node_id_t node_id) -> void;

private:
    [[nodiscard]] auto send_and_receive(const can::c_can_frame& request)
        -> result_t<can::c_can_frame>;
    [[nodiscard]] auto check_abort(const can::c_can_frame& response)
        -> std::optional<c_error>;

    std::shared_ptr<can_hal::i_can_adapter> m_adapter;
    c_sdo_config m_config;
};

} // namespace interface::canopen
