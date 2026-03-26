#pragma once

/// @file c_csv_reader.hpp
/// @brief CSV trace file parser with configurable column mapping.

#include "interface/can_trace/i_trace_reader.hpp"

#include <fstream>
#include <string>

namespace interface::can_trace {

/// Column mapping configuration for CSV trace files.
struct c_csv_column_config {
    int timestamp_col{0};       ///< Column index for timestamp
    int id_col{1};              ///< Column index for CAN ID
    int dlc_col{2};             ///< Column index for DLC
    int data_col{3};            ///< Column index for first data byte (or space-separated data)
    bool data_in_single_column{true}; ///< If true, all data bytes in one column
    char delimiter{','};        ///< Column delimiter
    bool has_header{true};      ///< First line is header
};

/// Parser for CSV trace files with configurable column mapping.
class c_csv_reader final : public i_trace_reader {
public:
    explicit c_csv_reader(c_csv_column_config config = {});

    [[nodiscard]] auto open(const std::filesystem::path& path) -> void_result_t override;
    [[nodiscard]] auto read_next() -> result_t<std::optional<can::c_can_frame>> override;
    [[nodiscard]] auto read_all() -> result_t<std::vector<can::c_can_frame>> override;
    [[nodiscard]] auto info() const -> c_trace_info override;
    [[nodiscard]] auto supported_extensions() const -> std::vector<std::string> override;
    [[nodiscard]] auto reset() -> void_result_t override;

private:
    [[nodiscard]] auto parse_csv_line(const std::string& line) -> std::optional<can::c_can_frame>;
    [[nodiscard]] auto split_line(const std::string& line) const -> std::vector<std::string>;

    c_csv_column_config m_config;
    std::filesystem::path m_path;
    std::ifstream m_file;
    c_trace_info m_info;
    bool m_opened{false};
};

} // namespace interface::can_trace
