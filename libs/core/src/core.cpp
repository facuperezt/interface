/// @file core.cpp
/// @brief Core module translation unit. Currently a placeholder to ensure the
///        static library has at least one object file.

#include "interface/core/version.hpp"

namespace interface {

auto get_version_string() -> std::string_view {
    return k_version_string;
}

} // namespace interface
