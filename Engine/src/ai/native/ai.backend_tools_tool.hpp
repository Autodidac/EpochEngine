// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <expected>
#include <functional>
#include <stop_token>
#include <string>
#include <nlohmann/json.hpp>
#include "ai.backend_core_error.hpp"

namespace epoch_ai::tools {
using json = nlohmann::json;

struct Context {
    std::string project_id;
    std::string session_id;
    std::string task_id;
    std::string workspace_root;
    std::stop_token stop;
    json addon_models = json::object(); // per-session model assignments, never tool authority
};

struct Definition {
    std::string name;
    std::string description;
    json parameters;
};

using Handler = std::function<std::expected<json, core::Error>(const json&, const Context&)>;

struct Tool {
    Definition definition;
    Handler handler;
};

} // namespace epoch_ai::tools
