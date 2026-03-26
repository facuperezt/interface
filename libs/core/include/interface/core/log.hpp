#pragma once

/// @file log.hpp
/// @brief Logging facade wrapping spdlog.

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string_view>

namespace interface {

/// Initialise the library-wide logger. Call once at startup.
/// @param level  spdlog log level (default: info)
inline void init_logging(spdlog::level::level_enum level = spdlog::level::info) {
    auto logger = spdlog::stdout_color_mt("interface");
    logger->set_level(level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    spdlog::set_default_logger(logger);
}

/// Convenience accessors — forward to spdlog default logger.
template <typename... Args>
void log_trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    spdlog::error(fmt, std::forward<Args>(args)...);
}

} // namespace interface
