#pragma once

/// @file c_mock_adapter.hpp
/// @brief Mock CAN adapter for testing without hardware.

#include "interface/can_hal/i_can_adapter.hpp"

#include <deque>
#include <mutex>

namespace interface::can_hal {

/// Mock CAN adapter that buffers sent/received frames for testing.
class c_mock_adapter final : public i_can_adapter {
public:
    c_mock_adapter() = default;
    ~c_mock_adapter() override { close(); }

    [[nodiscard]] auto open(const c_bitrate_config& config) -> void_result_t override;
    auto close() -> void override;
    [[nodiscard]] auto is_open() const noexcept -> bool override;

    [[nodiscard]] auto send(const can::c_can_frame& frame) -> void_result_t override;
    [[nodiscard]] auto receive(
        std::chrono::milliseconds timeout
    ) -> result_t<std::optional<can::c_can_frame>> override;
    auto set_receive_callback(receive_callback_t callback) -> void override;
    [[nodiscard]] auto set_filter(const can::c_can_filter& filter) -> void_result_t override;
    [[nodiscard]] auto info() const -> c_adapter_info override;

    // -- Test helpers --------------------------------------------------------

    /// Inject a frame into the receive queue (simulates incoming traffic).
    auto inject_rx(const can::c_can_frame& frame) -> void;

    /// Get a copy of all transmitted frames.
    [[nodiscard]] auto get_tx_history() const -> std::vector<can::c_can_frame>;

    /// Clear TX history.
    auto clear_tx_history() -> void;

private:
    bool m_open{false};
    c_bitrate_config m_config{};
    can::c_can_filter m_filter{can::c_can_filter::accept_all()};
    receive_callback_t m_rx_callback{};

    mutable std::mutex m_mutex{};
    std::deque<can::c_can_frame> m_rx_queue{};
    std::vector<can::c_can_frame> m_tx_history{};
};

} // namespace interface::can_hal
