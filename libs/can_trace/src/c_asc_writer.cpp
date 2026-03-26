/// @file c_asc_writer.cpp
/// @brief ASC trace file writer implementation.

#include "interface/can_trace/c_asc_writer.hpp"

#include <format>
#include <iomanip>

namespace interface::can_trace {

auto c_asc_writer::open(const std::filesystem::path& path) -> void_result_t {
    if (m_opened) {
        m_file.close();
    }

    m_path = path;
    m_file.open(path, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        return make_error("Failed to open ASC file for writing: " + path.string(),
                          e_error_category::io);
    }

    m_opened = true;
    m_header_written = false;
    return {};
}

auto c_asc_writer::write(const can::c_can_frame& frame) -> void_result_t {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    if (!m_header_written) {
        m_file << "date Thu Jan 01 00:00:00 AM 1970\n";
        m_file << "base hex timestamps absolute\n";
        m_file << "no internal events logged\n";
        m_header_written = true;
    }

    // Format: "   <timestamp> <channel> <id>[x] <dir> d <dlc> <data bytes>"
    auto timestamp_sec = static_cast<double>(frame.timestamp) / 1'000'000.0;

    std::string id_str = std::format("{:X}", frame.id);
    if (frame.flags.extended) {
        id_str += "x";
    }

    // Pad fields to match Vector ASC format
    m_file << std::format("   {:.6f} 1  {:16s} Rx   {} {}",
                          timestamp_sec, id_str,
                          frame.flags.remote ? "r" : "d",
                          static_cast<int>(frame.dlc));

    auto byte_count = frame.data_length();
    for (std::size_t i = 0; i < byte_count; ++i) {
        m_file << std::format(" {:02X}", frame.data[i]);
    }
    m_file << "\n";

    if (!m_file.good()) {
        return make_error("Write failed", e_error_category::io);
    }
    return {};
}

auto c_asc_writer::close() -> void {
    if (m_opened) {
        m_file.flush();
        m_file.close();
        m_opened = false;
    }
}

} // namespace interface::can_trace
