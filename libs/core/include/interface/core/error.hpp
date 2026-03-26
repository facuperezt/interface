#pragma once

/// @file error.hpp
/// @brief Error handling utilities using std::expected (C++23).

#include <expected>
#include <string>
#include <source_location>
#include <format>

namespace interface {

/// Error category for classifying failures.
enum class e_error_category {
    generic,
    io,
    parse,
    protocol,
    hardware,
    config,
    timeout,
};

/// Structured error type carrying a message, category, and source location.
struct c_error {
    std::string message;
    e_error_category category{e_error_category::generic};
    std::string file{};
    std::uint32_t line{0};

    /// Construct an error with automatic source location capture.
    static auto make(
        std::string msg,
        e_error_category cat = e_error_category::generic,
        std::source_location loc = std::source_location::current()
    ) -> c_error {
        return c_error{
            .message  = std::move(msg),
            .category = cat,
            .file     = loc.file_name(),
            .line     = loc.line(),
        };
    }

    /// Format for display.
    [[nodiscard]] auto format() const -> std::string {
        return std::format("[{}:{}] {}", file, line, message);
    }
};

/// Result type alias — the primary way to return values or errors.
template <typename T>
using result_t = std::expected<T, c_error>;

/// Void result for operations that can fail but return nothing.
using void_result_t = std::expected<void, c_error>;

/// Helper to create an unexpected error.
inline auto make_error(
    std::string msg,
    e_error_category cat = e_error_category::generic,
    std::source_location loc = std::source_location::current()
) -> std::unexpected<c_error> {
    return std::unexpected(c_error::make(std::move(msg), cat, loc));
}

} // namespace interface
