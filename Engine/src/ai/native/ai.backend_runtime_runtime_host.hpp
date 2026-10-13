// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#include <nlohmann/json.hpp>

#include "ai.backend_core_config.hpp"
#include "ai.backend_core_error.hpp"
#include "ai.backend_core_event.hpp"
#include "ai.backend_model_model_provider.hpp"

namespace epoch_ai::runtime {

using json = nlohmann::json;
using EventSink = std::function<void(const core::Event&)>;

enum class RunTerminal {
    completed,
    failed,
    cancelled,
    awaiting_approval,
};

struct RunOutcome {
    std::string project_id;
    std::string session_id;
    std::string task_id;
    RunTerminal terminal{RunTerminal::completed};
    std::string final_text;
    json terminal_data = json::object();
};

class RuntimeHost {
public:
    explicit RuntimeHost(core::Config config = core::Config::from_environment());
    ~RuntimeHost();

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    [[nodiscard]] const core::Config& config() const noexcept;
    [[nodiscard]] std::expected<json, core::Error> model_discovery();
    [[nodiscard]] json architecture() const;
    [[nodiscard]] json native_model_library() const;
    [[nodiscard]] std::expected<json, core::Error> register_native_model(const json& spec);
    [[nodiscard]] std::expected<json, core::Error> provision_native_runtime();
    [[nodiscard]] std::expected<json, core::Error> install_native_model(std::string_view id_or_uri);
    [[nodiscard]] std::expected<json, core::Error> load_native_model(
        std::string_view id_or_uri,
        std::string_view local_path_override = {},
        std::stop_token cancellation = {});
    void unload_native_model();
    void cancel_native_installation();
    [[nodiscard]] std::expected<json, core::Error> remove_native_model(std::string_view id_or_uri, bool delete_cache);
    [[nodiscard]] std::expected<json, core::Error> clear_native_model_cache(std::string_view id_or_uri);
    [[nodiscard]] std::expected<json, core::Error> delete_native_model_weights(std::string_view id_or_uri);
    [[nodiscard]] std::expected<json, core::Error> set_foundation_model(std::string_view provider_model);
    [[nodiscard]] std::string foundation_model() const;
    [[nodiscard]] std::expected<model::ModelResponse, core::Error> complete_host_request(
        const json& body, std::stop_token stop,
        std::chrono::steady_clock::time_point deadline, const model::StreamCallback& callback);

    [[nodiscard]] std::expected<RunOutcome, core::Error> run_task(
        std::string_view project_id,
        std::string_view session_id,
        std::string_view message,
        std::stop_token stop = {},
        EventSink on_event = {});

    [[nodiscard]] std::string decision_mode() const;
    [[nodiscard]] json training_status() const;
    [[nodiscard]] std::expected<json, core::Error> prepare_training(std::string_view profile, std::stop_token cancellation = {});
    [[nodiscard]] std::expected<json, core::Error> start_training(
        std::string_view scope, std::uint32_t steps, bool require_cuda);
    void cancel_training();
    void pause_training(bool paused);
    [[nodiscard]] std::vector<json> tool_catalog() const;
    [[nodiscard]] std::expected<json, core::Error> assign_model_role(std::string_view role,std::string_view model);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] std::string make_session_id(std::string_view prefix = "session");

} // namespace epoch_ai::runtime
