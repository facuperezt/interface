#pragma once

/// @file c_asc_reader.hpp
/// @brief ASC (Vector) trace file parser.

#include "interface/can_trace/i_trace_reader.hpp"

#include <fstream>
#include <string>

namespace interface::can_trace {

/// Parser for Vector ASC trace files.
/// Format: text-based, each line is a timestamped CAN frame.
/// Example: "0.123456 1 100 Rx d 8 01 02 03 04 05 06 07 08"
class c_asc_reader final : public i_trace_reader {
public:
    c_asc_reader() = default;

    [[nodiscard]] auto open(const std::filesystem::path& path) -> void_result_t override;
    [[nodiscard]] auto read_next() -> result_t<std::optional<can::c_can_frame>> override;
    [[nodiscard]] auto read_all() -> result_t<std::vector<can::c_can_frame>> override;
    [[nodiscard]] auto info() const -> c_trace_info override;
    [[nodiscard]] auto supported_extensions() const -> std::vector<std::string> override;
    [[nodiscard]] auto reset() -> void_result_t override;

private:
    [[nodiscard]] auto parse_frame_line(const std::string& line) -> std::optional<can::c_can_frame>;

    std::filesystem::path m_path;
    std::ifstream m_file;
    c_trace_info m_info;
    bool m_opened{false};
};

} // namespace interface::can_trace
