#pragma once

/// @file i_trace_reader.hpp
/// @brief Abstract interface for reading CAN trace files.

#include "interface/can/frame.hpp"
#include "interface/core/error.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace interface::can_trace {

/// Metadata about a trace file.
struct c_trace_info {
    std::string format;                 ///< Format name (e.g., "ASC", "BLF", "CSV")
    std::filesystem::path path;
    std::size_t frame_count{0};
    timestamp_us_t start_time{0};
    timestamp_us_t end_time{0};
};

/// Abstract trace file reader interface.
class i_trace_reader {
public:
    virtual ~i_trace_reader() = default;

    /// Open a trace file for reading.
    [[nodiscard]] virtual auto open(const std::filesystem::path& path) -> void_result_t = 0;

    /// Read the next frame. Returns nullopt at EOF.
    [[nodiscard]] virtual auto read_next() -> result_t<std::optional<can::c_can_frame>> = 0;

    /// Read all frames into a vector.
    [[nodiscard]] virtual auto read_all() -> result_t<std::vector<can::c_can_frame>> = 0;

    /// Get trace file metadata.
    [[nodiscard]] virtual auto info() const -> c_trace_info = 0;

    /// Supported file extensions.
    [[nodiscard]] virtual auto supported_extensions() const -> std::vector<std::string> = 0;

    /// Reset to the beginning of the file.
    [[nodiscard]] virtual auto reset() -> void_result_t = 0;

protected:
    i_trace_reader() = default;
};

/// Abstract trace file writer interface.
class i_trace_writer {
public:
    virtual ~i_trace_writer() = default;

    /// Open a file for writing.
    [[nodiscard]] virtual auto open(const std::filesystem::path& path) -> void_result_t = 0;

    /// Write a single frame.
    [[nodiscard]] virtual auto write(const can::c_can_frame& frame) -> void_result_t = 0;

    /// Flush and close the file.
    virtual auto close() -> void = 0;

protected:
    i_trace_writer() = default;
};

} // namespace interface::can_trace
