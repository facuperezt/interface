#pragma once

/// @file dispatcher.hpp
/// @brief CAN message dispatcher — fan-out frames to registered callbacks.

#include "interface/can/frame.hpp"
#include "interface/can/filter.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace interface::can {

/// Opaque subscription token returned by subscribe().
using subscription_token_t = std::uint64_t;

/// Thread-safe CAN message dispatcher.
///
/// Allows multiple consumers to subscribe by exact CAN ID or by filter.
/// When dispatch() is called with a frame, all matching subscribers are notified.
class c_dispatcher {
public:
    c_dispatcher() = default;

    /// Subscribe to frames matching a specific CAN ID.
    /// Returns a token that can be used to unsubscribe.
    auto subscribe(can_id_t id, std::function<void(const c_can_frame&)> callback)
        -> subscription_token_t;

    /// Subscribe to frames matching a filter (id + mask).
    /// Returns a token that can be used to unsubscribe.
    auto subscribe(c_can_filter filter, std::function<void(const c_can_frame&)> callback)
        -> subscription_token_t;

    /// Remove a subscription by token.
    auto unsubscribe(subscription_token_t token) -> void;

    /// Dispatch a frame to all matching subscribers.
    auto dispatch(const c_can_frame& frame) -> void;

private:
    struct subscription {
        subscription_token_t token;
        c_can_filter filter;
        std::function<void(const c_can_frame&)> callback;
    };

    std::mutex m_mutex;
    std::vector<subscription> m_subscriptions;
    subscription_token_t m_next_token{1};
};

} // namespace interface::can
