#pragma once

/// @file frame.hpp
/// @brief CAN 2.0 and CAN FD frame definitions.

#include "interface/core/types.hpp"
#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace interface::can {

/// Maximum data length for CAN 2.0 frames.
inline constexpr std::size_t k_can_max_dlc = 8;

/// Maximum data length for CAN FD frames.
inline constexpr std::size_t k_canfd_max_dlc = 64;

/// CAN frame flags.
struct c_frame_flags {
    bool extended{false};  ///< 29-bit (extended) identifier
    bool remote{false};    ///< Remote Transmission Request
    bool error{false};     ///< Error frame
    bool fd{false};        ///< CAN FD frame
    bool brs{false};       ///< Bit Rate Switch (FD only)
    bool esi{false};       ///< Error State Indicator (FD only)
};

/// CAN 2.0 frame.
struct c_can_frame {
    can_id_t                        id{0};
    std::uint8_t                    dlc{0};
    std::array<byte_t, k_can_max_dlc> data{};
    timestamp_us_t                  timestamp{0};
    c_frame_flags                   flags{};

    /// Effective data length (capped to DLC).
    [[nodiscard]] constexpr auto data_length() const noexcept -> std::size_t {
        return dlc <= k_can_max_dlc ? static_cast<std::size_t>(dlc) : k_can_max_dlc;
    }

    /// Non-owning view of the payload.
    [[nodiscard]] auto payload() const noexcept -> byte_span_t {
        return byte_span_t{data.data(), data_length()};
    }

    /// Format for display: "0x123 [8] DE AD BE EF ..."
    [[nodiscard]] auto format() const -> std::string {
        std::string s = std::format("{:#05x} [{}]", id, dlc);
        for (std::size_t i = 0; i < data_length(); ++i) {
            s += std::format(" {:02X}", data[i]);
        }
        return s;
    }
};

/// CAN FD frame.
struct c_canfd_frame {
    can_id_t                          id{0};
    std::uint8_t                      dlc{0};
    std::array<byte_t, k_canfd_max_dlc> data{};
    timestamp_us_t                    timestamp{0};
    c_frame_flags                     flags{.fd = true};

    /// Effective data length for CAN FD (DLC to byte count mapping).
    [[nodiscard]] constexpr auto data_length() const noexcept -> std::size_t {
        // CAN FD DLC-to-length: 0-8 → 0-8, 9→12, 10→16, 11→20, 12→24,
        //                       13→32, 14→48, 15→64
        constexpr std::array<std::size_t, 16> map{
            0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
        };
        return dlc < 16 ? map[dlc] : k_canfd_max_dlc;
    }

    [[nodiscard]] auto payload() const noexcept -> byte_span_t {
        return byte_span_t{data.data(), data_length()};
    }
};

} // namespace interface::can
