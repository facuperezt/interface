/// @file dispatcher.cpp
/// @brief CAN message dispatcher implementation.

#include "interface/can/dispatcher.hpp"

#include <algorithm>

namespace interface::can {

auto c_dispatcher::subscribe(can_id_t id, std::function<void(const c_can_frame&)> callback)
    -> subscription_token_t {
    return subscribe(c_can_filter::exact(id), std::move(callback));
}

auto c_dispatcher::subscribe(c_can_filter filter, std::function<void(const c_can_frame&)> callback)
    -> subscription_token_t {
    std::lock_guard lock(m_mutex);
    auto token = m_next_token++;
    m_subscriptions.push_back(subscription{
        .token = token,
        .filter = filter,
        .callback = std::move(callback),
    });
    return token;
}

auto c_dispatcher::unsubscribe(subscription_token_t token) -> void {
    std::lock_guard lock(m_mutex);
    std::erase_if(m_subscriptions, [token](const subscription& sub) {
        return sub.token == token;
    });
}

auto c_dispatcher::dispatch(const c_can_frame& frame) -> void {
    // Copy subscriptions under the lock to avoid holding it during callbacks.
    std::vector<subscription> snapshot;
    {
        std::lock_guard lock(m_mutex);
        snapshot = m_subscriptions;
    }

    for (const auto& sub : snapshot) {
        if (sub.filter.matches(frame.id)) {
            sub.callback(frame);
        }
    }
}

} // namespace interface::can
