#pragma once

/// @file filter.hpp
/// @brief CAN ID filtering utilities.

#include "interface/core/types.hpp"

namespace interface::can {

/// A CAN acceptance filter defined by ID and mask.
/// A frame passes if (frame.id & mask) == (id & mask).
struct c_can_filter {
    can_id_t id{0};
    can_id_t mask{0xFFFFFFFF};

    /// Check if a given CAN ID passes this filter.
    [[nodiscard]] constexpr auto matches(can_id_t frame_id) const noexcept -> bool {
        return (frame_id & mask) == (id & mask);
    }

    /// Accept-all filter.
    [[nodiscard]] static constexpr auto accept_all() noexcept -> c_can_filter {
        return c_can_filter{.id = 0, .mask = 0};
    }

    /// Exact-match filter for a single ID.
    [[nodiscard]] static constexpr auto exact(can_id_t target) noexcept -> c_can_filter {
        return c_can_filter{.id = target, .mask = 0x1FFFFFFF};
    }

    /// Range filter for standard 11-bit IDs.
    [[nodiscard]] static constexpr auto standard_only() noexcept -> c_can_filter {
        return c_can_filter{.id = 0, .mask = 0x1FFFF800};
    }
};

} // namespace interface::can
