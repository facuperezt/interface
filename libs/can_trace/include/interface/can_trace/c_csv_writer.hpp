#pragma once

/// @file c_csv_writer.hpp
/// @brief CSV trace file writer with configurable delimiter and columns.

#include "interface/can_trace/i_trace_reader.hpp"
#include "interface/can_trace/c_csv_reader.hpp"

#include <filesystem>
#include <fstream>

namespace interface::can_trace {

/// Writer for CSV trace files.
/// Produces output compatible with c_csv_reader using matching column config.
class c_csv_writer final : public i_trace_writer {
public:
    explicit c_csv_writer(c_csv_column_config config = {});

    [[nodiscard]] auto open(const std::filesystem::path& path) -> void_result_t override;
    [[nodiscard]] auto write(const can::c_can_frame& frame) -> void_result_t override;
    auto close() -> void override;

private:
    c_csv_column_config m_config;
    std::filesystem::path m_path;
    std::ofstream m_file;
    bool m_opened{false};
};

} // namespace interface::can_trace
