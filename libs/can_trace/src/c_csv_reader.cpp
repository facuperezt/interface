/// @file c_csv_reader.cpp
/// @brief CSV trace file parser implementation.

#include "interface/can_trace/c_csv_reader.hpp"

#include <charconv>
#include <cstdlib>
#include <sstream>

namespace interface::can_trace {

c_csv_reader::c_csv_reader(c_csv_column_config config)
    : m_config{config} {}

auto c_csv_reader::open(const std::filesystem::path& path) -> void_result_t {
    if (m_opened) {
        m_file.close();
    }

    m_path = path;
    m_file.open(path, std::ios::in);
    if (!m_file.is_open()) {
        return make_error("Failed to open CSV file: " + path.string(), e_error_category::io);
    }

    m_info = c_trace_info{
        .format = "CSV",
        .path = path,
    };
    m_opened = true;

    // Skip header line if configured
    if (m_config.has_header) {
        std::string header;
        std::getline(m_file, header);
    }

    return {};
}

auto c_csv_reader::read_next() -> result_t<std::optional<can::c_can_frame>> {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    std::string line;
    while (std::getline(m_file, line)) {
        if (line.empty()) {
            continue;
        }

        auto frame = parse_csv_line(line);
        if (frame) {
            m_info.frame_count++;
            if (m_info.frame_count == 1) {
                m_info.start_time = frame->timestamp;
            }
            m_info.end_time = frame->timestamp;
            return *frame;
        }
    }

    return std::optional<can::c_can_frame>{std::nullopt};
}

auto c_csv_reader::read_all() -> result_t<std::vector<can::c_can_frame>> {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    auto reset_result = reset();
    if (!reset_result) {
        return std::unexpected(reset_result.error());
    }

    std::vector<can::c_can_frame> frames;
    while (true) {
        auto result = read_next();
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!result->has_value()) {
            break;
        }
        frames.push_back(result->value());
    }
    return frames;
}

auto c_csv_reader::info() const -> c_trace_info {
    return m_info;
}

auto c_csv_reader::supported_extensions() const -> std::vector<std::string> {
    return {".csv", ".tsv"};
}

auto c_csv_reader::reset() -> void_result_t {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    m_file.clear();
    m_file.seekg(0, std::ios::beg);
    m_info.frame_count = 0;
    m_info.start_time = 0;
    m_info.end_time = 0;

    if (m_config.has_header) {
        std::string header;
        std::getline(m_file, header);
    }

    return {};
}

auto c_csv_reader::split_line(const std::string& line) const -> std::vector<std::string> {
    std::vector<std::string> columns;
    std::string current;

    for (char ch : line) {
        if (ch == m_config.delimiter) {
            columns.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    columns.push_back(current);
    return columns;
}

auto c_csv_reader::parse_csv_line(const std::string& line) -> std::optional<can::c_can_frame> {
    auto columns = split_line(line);

    auto max_col = static_cast<int>(columns.size());
    if (m_config.timestamp_col >= max_col ||
        m_config.id_col >= max_col ||
        m_config.dlc_col >= max_col) {
        return std::nullopt;
    }

    // Parse timestamp
    const auto& ts_str = columns[static_cast<std::size_t>(m_config.timestamp_col)];
    char* end_ptr = nullptr;
    double timestamp_sec = std::strtod(ts_str.c_str(), &end_ptr);
    if (end_ptr == ts_str.c_str()) {
        return std::nullopt;
    }

    // Parse CAN ID (may be hex with or without 0x prefix)
    const auto& id_str = columns[static_cast<std::size_t>(m_config.id_col)];
    std::uint32_t can_id = 0;
    const char* id_start = id_str.c_str();
    // Skip whitespace
    while (*id_start == ' ') { ++id_start; }
    // Skip 0x prefix
    if (id_start[0] == '0' && (id_start[1] == 'x' || id_start[1] == 'X')) {
        id_start += 2;
    }
    auto id_end = id_str.c_str() + id_str.size();
    auto [ptr, ec] = std::from_chars(id_start, id_end, can_id, 16);
    if (ec != std::errc{}) {
        return std::nullopt;
    }

    // Parse DLC
    const auto& dlc_str = columns[static_cast<std::size_t>(m_config.dlc_col)];
    std::uint8_t dlc = 0;
    {
        const char* dlc_start = dlc_str.c_str();
        while (*dlc_start == ' ') { ++dlc_start; }
        auto [p, e] = std::from_chars(dlc_start, dlc_str.c_str() + dlc_str.size(), dlc);
        if (e != std::errc{}) {
            return std::nullopt;
        }
    }

    can::c_can_frame frame{};
    frame.id = can_id;
    frame.dlc = dlc;
    frame.timestamp = static_cast<timestamp_us_t>(timestamp_sec * 1'000'000.0);

    // Parse data bytes
    if (m_config.data_col < max_col) {
        if (m_config.data_in_single_column) {
            // All data bytes in one column, space-separated
            const auto& data_str = columns[static_cast<std::size_t>(m_config.data_col)];
            std::istringstream dss(data_str);
            std::string byte_str;
            std::size_t idx = 0;
            while (dss >> byte_str && idx < frame.data_length()) {
                std::uint8_t byte_val = 0;
                auto [p, e] = std::from_chars(
                    byte_str.data(), byte_str.data() + byte_str.size(), byte_val, 16);
                if (e == std::errc{}) {
                    frame.data[idx++] = byte_val;
                }
            }
        } else {
            // Each data byte in a separate column
            for (std::size_t i = 0; i < frame.data_length(); ++i) {
                auto col_idx = static_cast<std::size_t>(m_config.data_col) + i;
                if (col_idx >= columns.size()) {
                    break;
                }
                const auto& byte_str = columns[col_idx];
                std::uint8_t byte_val = 0;
                const char* bs = byte_str.c_str();
                while (*bs == ' ') { ++bs; }
                auto [p, e] = std::from_chars(bs, byte_str.c_str() + byte_str.size(), byte_val, 16);
                if (e == std::errc{}) {
                    frame.data[i] = byte_val;
                }
            }
        }
    }

    return frame;
}

} // namespace interface::can_trace
