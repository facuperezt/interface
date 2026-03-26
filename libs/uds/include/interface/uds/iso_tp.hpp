#pragma once

/// @file iso_tp.hpp
/// @brief ISO-TP (ISO 15765-2) multi-frame transport layer for UDS.

#include "interface/can/frame.hpp"
#include "interface/can_hal/i_can_adapter.hpp"
#include "interface/core/error.hpp"
#include "interface/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <memory>

namespace interface::uds {

/// ISO-TP frame types (upper nibble of first byte).
enum class e_isotp_frame_type : std::uint8_t {
    single_frame      = 0x00,
    first_frame       = 0x10,
    consecutive_frame = 0x20,
    flow_control      = 0x30,
};

/// Flow control flow status.
enum class e_flow_status : std::uint8_t {
    continue_to_send = 0x00,
    wait             = 0x01,
    overflow_abort   = 0x02,
};

/// ISO-TP transport configuration.
struct c_isotp_config {
    can_id_t tx_id{0x7DF};
    can_id_t rx_id{0x7E8};
    std::uint8_t block_size{0};         ///< FC block size (0 = no limit)
    std::uint8_t st_min{10};            ///< Separation time min (ms)
    std::chrono::milliseconds timeout{1000};
    std::uint8_t max_fc_wait{10};       ///< Max FC.Wait frames before abort
    std::uint8_t padding_byte{0xCC};    ///< Padding byte for frames
};

/// ISO-TP transport layer for segmented messaging.
class c_isotp_transport {
public:
    c_isotp_transport(
        std::shared_ptr<can_hal::i_can_adapter> adapter,
        c_isotp_config config = {}
    );

    /// Send a message (handles SF/FF+CF automatically based on length).
    [[nodiscard]] auto send(byte_span_t data) -> void_result_t;

    /// Receive a message (handles SF/FF+CF reassembly automatically).
    [[nodiscard]] auto receive() -> result_t<byte_buffer_t>;

    /// Update addressing.
    auto set_addressing(can_id_t tx_id, can_id_t rx_id) -> void;

    /// Access config.
    [[nodiscard]] auto config() const -> const c_isotp_config&;

private:
    [[nodiscard]] auto send_single_frame(byte_span_t data) -> void_result_t;
    [[nodiscard]] auto send_multi_frame(byte_span_t data) -> void_result_t;
    [[nodiscard]] auto wait_for_flow_control() -> result_t<can::c_can_frame>;
    [[nodiscard]] auto receive_single_frame(const can::c_can_frame& frame) -> result_t<byte_buffer_t>;
    [[nodiscard]] auto receive_multi_frame(const can::c_can_frame& ff) -> result_t<byte_buffer_t>;
    [[nodiscard]] auto send_flow_control(e_flow_status status) -> void_result_t;

    std::shared_ptr<can_hal::i_can_adapter> m_adapter;
    c_isotp_config m_config;
};

} // namespace interface::uds
