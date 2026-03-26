/// @file c_mock_adapter.cpp
/// @brief Mock CAN adapter implementation.

#include "interface/can_hal/c_mock_adapter.hpp"
#include "interface/core/log.hpp"

#include <thread>

namespace interface::can_hal {

auto c_mock_adapter::open(const c_bitrate_config& config) -> void_result_t {
    std::lock_guard lock{m_mutex};
    if (m_open) {
        return make_error("Mock adapter already open", e_error_category::hardware);
    }
    m_config = config;
    m_open = true;
    log_info("Mock CAN adapter opened ({}bps)", config.nominal_bps);
    return {};
}

auto c_mock_adapter::close() -> void {
    std::lock_guard lock{m_mutex};
    m_open = false;
    m_rx_queue.clear();
}

auto c_mock_adapter::is_open() const noexcept -> bool {
    return m_open;
}

auto c_mock_adapter::send(const can::c_can_frame& frame) -> void_result_t {
    std::lock_guard lock{m_mutex};
    if (!m_open) {
        return make_error("Adapter not open", e_error_category::hardware);
    }
    m_tx_history.push_back(frame);
    return {};
}

auto c_mock_adapter::receive(
    std::chrono::milliseconds timeout
) -> result_t<std::optional<can::c_can_frame>> {
    if (!m_open) {
        return make_error("Adapter not open", e_error_category::hardware);
    }

    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard lock{m_mutex};
            if (!m_rx_queue.empty()) {
                auto frame = m_rx_queue.front();
                m_rx_queue.pop_front();
                if (m_filter.matches(frame.id)) {
                    return frame;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return std::optional<can::c_can_frame>{std::nullopt};
}

auto c_mock_adapter::set_receive_callback(receive_callback_t callback) -> void {
    std::lock_guard lock{m_mutex};
    m_rx_callback = std::move(callback);
}

auto c_mock_adapter::set_filter(const can::c_can_filter& filter) -> void_result_t {
    std::lock_guard lock{m_mutex};
    m_filter = filter;
    return {};
}

auto c_mock_adapter::info() const -> c_adapter_info {
    return c_adapter_info{
        .name = "Mock CAN Adapter",
        .serial = "MOCK-0001",
        .channel_index = 0,
        .driver = "mock",
    };
}

auto c_mock_adapter::inject_rx(const can::c_can_frame& frame) -> void {
    std::lock_guard lock{m_mutex};
    m_rx_queue.push_back(frame);
    if (m_rx_callback && m_filter.matches(frame.id)) {
        m_rx_callback(frame);
    }
}

auto c_mock_adapter::get_tx_history() const -> std::vector<can::c_can_frame> {
    std::lock_guard lock{m_mutex};
    return m_tx_history;
}

auto c_mock_adapter::clear_tx_history() -> void {
    std::lock_guard lock{m_mutex};
    m_tx_history.clear();
}

} // namespace interface::can_hal
