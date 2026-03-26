#pragma once

/// @file i_can_adapter.hpp
/// @brief Abstract hardware abstraction interface for CAN adapters.

#include "interface/can/frame.hpp"
#include "interface/can/filter.hpp"
#include "interface/core/error.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <chrono>

namespace interface::can_hal {

/// Bitrate configuration for CAN channels.
struct c_bitrate_config {
    std::uint32_t nominal_bps{500000};    ///< Nominal bitrate (e.g., 500 kbit/s)
    std::uint32_t data_bps{0};            ///< Data bitrate for CAN FD (0 = CAN 2.0 only)
};

/// Information about a discovered CAN adapter/channel.
struct c_adapter_info {
    std::string name;                      ///< Human-readable adapter name
    std::string serial;                    ///< Serial number or unique ID
    std::uint32_t channel_index{0};        ///< Channel index on the adapter
    std::string driver;                    ///< Driver/backend identifier
};

/// Receive callback: called when a frame arrives asynchronously.
using receive_callback_t = std::function<void(const can::c_can_frame&)>;

/// Abstract CAN adapter interface.
/// All hardware-specific adapters implement this interface.
class i_can_adapter {
public:
    virtual ~i_can_adapter() = default;

    // -- Lifecycle -----------------------------------------------------------

    /// Open the adapter channel with the given bitrate.
    [[nodiscard]] virtual auto open(const c_bitrate_config& config) -> void_result_t = 0;

    /// Close the adapter channel.
    virtual auto close() -> void = 0;

    /// Check if the adapter is currently open.
    [[nodiscard]] virtual auto is_open() const noexcept -> bool = 0;

    // -- Transmit / Receive --------------------------------------------------

    /// Send a single CAN frame. Blocking.
    [[nodiscard]] virtual auto send(const can::c_can_frame& frame) -> void_result_t = 0;

    /// Try to receive a single frame within the timeout.
    /// Returns std::nullopt on timeout (not an error).
    [[nodiscard]] virtual auto receive(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{100}
    ) -> result_t<std::optional<can::c_can_frame>> = 0;

    /// Register a callback for asynchronous receive.
    virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

    // -- Configuration -------------------------------------------------------

    /// Set a hardware acceptance filter.
    [[nodiscard]] virtual auto set_filter(const can::c_can_filter& filter) -> void_result_t = 0;

    /// Get adapter information.
    [[nodiscard]] virtual auto info() const -> c_adapter_info = 0;

    // -- Discovery -----------------------------------------------------------

    /// Enumerate available adapters. Static per implementation.
    // (Derived classes provide a static `enumerate()` method)

protected:
    i_can_adapter() = default;
    i_can_adapter(const i_can_adapter&) = default;
    i_can_adapter& operator=(const i_can_adapter&) = default;
    i_can_adapter(i_can_adapter&&) = default;
    i_can_adapter& operator=(i_can_adapter&&) = default;
};

} // namespace interface::can_hal
