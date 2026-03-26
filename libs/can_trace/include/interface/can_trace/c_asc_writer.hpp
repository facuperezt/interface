#pragma once

/// @file c_asc_writer.hpp
/// @brief ASC (Vector) trace file writer.

#include "interface/can_trace/i_trace_reader.hpp"

#include <filesystem>
#include <fstream>

namespace interface::can_trace {

/// Writer for Vector ASC trace files.
/// Produces text output compatible with c_asc_reader.
class c_asc_writer final : public i_trace_writer {
public:
    c_asc_writer() = default;

    [[nodiscard]] auto open(const std::filesystem::path& path) -> void_result_t override;
    [[nodiscard]] auto write(const can::c_can_frame& frame) -> void_result_t override;
    auto close() -> void override;

private:
    std::filesystem::path m_path;
    std::ofstream m_file;
    bool m_opened{false};
    bool m_header_written{false};
};

} // namespace interface::can_trace
