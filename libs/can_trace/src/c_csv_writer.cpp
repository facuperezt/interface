/// @file c_csv_writer.cpp
/// @brief CSV trace file writer implementation.

#include "interface/can_trace/c_csv_writer.hpp"

#include <format>

namespace interface::can_trace {

c_csv_writer::c_csv_writer(c_csv_column_config config)
    : m_config{config} {}

auto c_csv_writer::open(const std::filesystem::path& path) -> void_result_t {
    if (m_opened) {
        m_file.close();
    }

    m_path = path;
    m_file.open(path, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        return make_error("Failed to open CSV file for writing: " + path.string(),
                          e_error_category::io);
    }

    m_opened = true;

    // Write header if configured
    if (m_config.has_header) {
        m_file << "timestamp" << m_config.delimiter
               << "id" << m_config.delimiter
               << "dlc" << m_config.delimiter
               << "data" << "\n";
    }

    return {};
}

auto c_csv_writer::write(const can::c_can_frame& frame) -> void_result_t {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    auto timestamp_sec = static_cast<double>(frame.timestamp) / 1'000'000.0;

    m_file << std::format("{:.6f}", timestamp_sec) << m_config.delimiter;
    m_file << std::format("{:X}", frame.id) << m_config.delimiter;
    m_file << static_cast<int>(frame.dlc) << m_config.delimiter;

    if (m_config.data_in_single_column) {
        // All data bytes in one column, space-separated
        auto byte_count = frame.data_length();
        for (std::size_t i = 0; i < byte_count; ++i) {
            if (i > 0) {
                m_file << " ";
            }
            m_file << std::format("{:02X}", frame.data[i]);
        }
        m_file << "\n";
    } else {
        // Each byte in a separate column
        auto byte_count = frame.data_length();
        for (std::size_t i = 0; i < byte_count; ++i) {
            m_file << std::format("{:02X}", frame.data[i]);
            if (i < byte_count - 1) {
                m_file << m_config.delimiter;
            }
        }
        m_file << "\n";
    }

    if (!m_file.good()) {
        return make_error("Write failed", e_error_category::io);
    }
    return {};
}

auto c_csv_writer::close() -> void {
    if (m_opened) {
        m_file.flush();
        m_file.close();
        m_opened = false;
    }
}

} // namespace interface::can_trace
