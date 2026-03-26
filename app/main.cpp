/// @file main.cpp
/// @brief interface — entry point.

#include "interface/core/log.hpp"
#include "interface/core/version.hpp"
#include "interface/tui/app.hpp"

#include <cstdlib>
#include <iostream>

auto main(int argc, char* argv[]) -> int {
    // Check for --version flag
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--version") {
            std::cout << "interface " << interface::k_version_string << '\n';
            return EXIT_SUCCESS;
        }
    }

    // Initialise logging
    interface::init_logging(spdlog::level::info);
    interface::log_info("Starting interface v{}", interface::k_version_string);

    // Run the TUI
    return interface::tui::run();
}
