#pragma once

/// @file version.hpp
/// @brief Compile-time version information.

#include <string_view>

namespace interface {

inline constexpr int         k_version_major = 0;
inline constexpr int         k_version_minor = 1;
inline constexpr int         k_version_patch = 0;
inline constexpr std::string_view k_version_string = "0.1.0";

} // namespace interface
