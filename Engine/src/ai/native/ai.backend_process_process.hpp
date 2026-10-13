// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <chrono>
#include <expected>
#include <filesystem>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>
#include "ai.backend_core_error.hpp"

namespace epoch_ai::process {

struct Request {
    std::string executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    std::vector<std::pair<std::string, std::string>> environment;
    std::chrono::milliseconds timeout{300000};
    std::size_t max_output_bytes{256 * 1024};
};

struct Result {
    int exit_code{-1};
    bool timed_out{};
    bool cancelled{};
    std::string stdout_text;
    std::string stderr_text;
};

[[nodiscard]] std::expected<Result, core::Error> run(const Request&, std::stop_token = {});

namespace detail {
[[nodiscard]] std::expected<Result, core::Error> run_windows(const Request&, std::stop_token);
[[nodiscard]] std::expected<Result, core::Error> run_posix(const Request&, std::stop_token);
}

} // namespace epoch_ai::process
