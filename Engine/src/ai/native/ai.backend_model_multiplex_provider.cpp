// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_model_multiplex_provider.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <algorithm>
#include "ai.backend_controller_task_intent.hpp"
#include "ai.backend_core_file_replace.hpp"
#include "ai.backend_core_ids.hpp"

namespace epoch_ai::model {
namespace {
[[nodiscard]] bool is_internal_model(std::string_view model) {
    return model.starts_with("engcoder://") || model.starts_with("threadspire://") ||
           model.starts_with("internal://");
}


[[nodiscard]] core::Error native_transport_error(core::Error error) {
    constexpr std::string_view from = "LM Studio";
    constexpr std::string_view to = "local Qwen Flash runtime";
    std::size_t position = 0;
    while ((position = error.message.find(from, position)) != std::string::npos) {
        error.message.replace(position, from.size(), to);
        position += to.size();
    }
    return error;
}
} // namespace

MultiplexProvider::MultiplexProvider(
    core::Config config,
    NativeModelManager& native_models)
    : config_(std::move(config)),
      perf_file_(config_.data_root / "model-performance-v2.json"),
      native_models_(native_models),
      external_(config_) {
    std::ifstream input(perf_file_);
    if (input) {
        try {
            auto restored = json::parse(input);
            if (restored.is_object() && restored.value("format", std::string{}) == "epoch.model-performance.v2"
                && restored.contains("models") && restored["models"].is_object())
                perf_stats_ = std::move(restored["models"]);
        } catch (...) { /* invalid old telemetry cannot prevent startup */ }
    }
}

namespace {
std::optional<std::uint64_t> reported_output_tokens(const ModelResponse& response) {
    if (!response.raw.is_object()) return std::nullopt;
    const json* usage = &response.raw;
    if (response.raw.contains("usage") && response.raw["usage"].is_object())
        usage = &response.raw["usage"];
    if (response.raw.contains("timings") && response.raw["timings"].is_object())
        usage = &response.raw["timings"];
    for (const auto* key : {"completion_tokens", "output_tokens", "predicted_n", "tokens_predicted"}) {
        const auto it = usage->find(key);
        if (it != usage->end() && it->is_number_unsigned()) return it->get<std::uint64_t>();
        if (it != usage->end() && it->is_number_integer() && it->get<std::int64_t>() >= 0)
            return static_cast<std::uint64_t>(it->get<std::int64_t>());
    }
    return std::nullopt;
}
std::string request_class(const ModelRequest& request) {
    for (auto it = request.messages.rbegin(); it != request.messages.rend(); ++it)
        if (it->role == "user")
            return std::string{controller::to_string(controller::classify_task(it->content))};
    return "unknown";
}
}

void MultiplexProvider::save_performance_locked() const {
    if (perf_file_.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(perf_file_.parent_path(), ec);
    if (ec) return;
    const auto temp = perf_file_.parent_path() /
        (perf_file_.filename().string() + "." + core::make_id("tmp"));
    {
        std::ofstream output(temp, std::ios::trunc | std::ios::binary);
        if (!output) return;
        output << json{{"format", "epoch.model-performance.v2"},
            {"models", perf_stats_}}.dump(2) << '\n';
        output.flush();
        if (!output) { std::filesystem::remove(temp, ec); return; }
    }
    if (core::replace_file(temp, perf_file_)) std::filesystem::remove(temp, ec);
}

void MultiplexProvider::record_inference(std::string_view model, std::string_view task_class,
    double elapsed_ms, double first_token_ms, std::optional<std::uint64_t> tokens,
    bool succeeded, std::string_view error) {
    std::scoped_lock lock(perf_mutex_);
    auto& item = perf_stats_[std::string{model}][std::string{task_class}];
    if (!item.is_object()) item = json::object();
    const auto inc = [&](const char* key, double delta) {
        item[key] = item.value(key, 0.0) + delta;
    };
    inc("requests", 1);
    inc(succeeded ? "inference_ok" : "inference_failed", 1);
    inc("total_ms", std::max(0.0, elapsed_ms));
    if (first_token_ms >= 0.0) {
        inc("ttft_samples", 1); inc("ttft_ms", first_token_ms);
    }
    if (tokens && *tokens > 0) {
        inc("reported_token_samples", 1);
        if (first_token_ms >= 0.0 && elapsed_ms > first_token_ms) {
            inc("token_samples", 1);
            inc("completion_tokens", static_cast<double>(*tokens));
            inc("generation_ms", elapsed_ms - first_token_ms);
        }
    }
    if (!error.empty()) item["last_error"] = std::string{error};
    save_performance_locked();
}

bool MultiplexProvider::is_model_resident(std::string_view model) const {
#if EPOCH_EDITION_PUBLIC
    if (is_internal_model(model)) return false;
#endif
    return !is_native_runtime_model(model) || native_models_.model_loaded(model);
}

void MultiplexProvider::report_task_outcome(std::string_view model, bool completed) {
    std::scoped_lock lock(perf_mutex_);
    auto& per_model = perf_stats_[std::string{model}];
    // Per-class request measurements are separate from agent outcomes; this
    // count is NOT labeled validated job completion.
    auto& outcome = per_model["agent_outcomes"];
    if (!outcome.is_object()) outcome = json::object();
    const char* key = completed ? "completed" : "failed";
    outcome[key] = outcome.value(key, 0.0) + 1.0;
    save_performance_locked();
}

std::optional<double> MultiplexProvider::observed_latency_ms(
    std::string_view model, std::string_view task_class) const {
    std::scoped_lock lock(perf_mutex_);
    if (!perf_stats_.is_object()) return std::nullopt;
    const auto model_it = perf_stats_.find(std::string{model});
    if (model_it == perf_stats_.end() || !model_it->is_object()) return std::nullopt;
    const auto class_it = model_it->find(std::string{task_class});
    if (class_it == model_it->end() || !class_it->is_object()) return std::nullopt;
    // Provider failures do not count as successful response latency.
    const double samples = class_it->value("requests", 0.0);
    const double ok = class_it->value("inference_ok", 0.0);
    const double total = class_it->value("total_ms", 0.0);
    if (samples < 3.0 || ok < 3.0 || total <= 0.0 || ok / samples < 0.85)
        return std::nullopt;
    const double mean = total / samples;
    if (!std::isfinite(mean) || mean < 1.0) return std::nullopt;
    return mean;
}

json MultiplexProvider::performance() const {
    std::scoped_lock lock(perf_mutex_);
    return json{{"format", "epoch.model-performance.v2"},
        {"models", perf_stats_},
        {"notes", "TPS requires runtime-reported completion token counts. Inference OK and agent completion are not verified builds or user acceptance. Time comparisons are observational, not controlled A/B savings."}};
}


bool MultiplexProvider::native_request(std::string_view model) const noexcept {
    return is_native_runtime_model(model);
}

std::string MultiplexProvider::resolve_request_model(std::string_view requested) const {
    if (!requested.empty() && requested != "auto") {
        return std::string{requested};
    }

    if (const auto native = native_models_.active_model_uri(); native.has_value()) {
        return *native;
    }

    return requested.empty() ? std::string{"auto"} : std::string{requested};
}

std::string MultiplexProvider::resolve_model_hint(std::string_view requested) const {
    return resolve_request_model(requested);
}


std::expected<LmStudioProvider*, core::Error> MultiplexProvider::native_provider(
    std::string_view model,
    std::chrono::steady_clock::time_point deadline,
    std::stop_token stop) {
    // Inference dispatch is resident-only. Model loading belongs to the
    // operator's explicit Model Manager action, not the hidden HTTP path.
    if (stop.stop_requested() || std::chrono::steady_clock::now() >= deadline)
        return std::unexpected(core::Error{core::make_error_code(core::Errc::cancelled),
            "Native request cancelled or expired before dispatch"});
    if (!native_models_.model_loaded(model))
        return std::unexpected(core::Error{core::make_error_code(core::Errc::model_unavailable),
            "Selected native model is no longer loaded; select an already loaded model"});
    const auto status = native_models_.model_runtime_status(model);
    const auto endpoint = status.contains("endpoint") && status.at("endpoint").is_string()
        ? status.at("endpoint").get<std::string>() : std::string{};
    if (endpoint.empty())
        return std::unexpected(core::Error{core::make_error_code(core::Errc::model_unavailable),
            "Selected native model has no running endpoint; load it explicitly in Model Manager"});
    // Providers are stable per native URI and loopback endpoint; selecting a
    // second model never destroys the first model's in-flight HTTP transport.
    const std::string key = std::string{model} + "@" + endpoint;
    std::scoped_lock lock(transports_mutex_);
    if (const auto it = native_transports_.find(key); it != native_transports_.end())
        return it->second.get();
    auto native_config = config_;
    native_config.lm_studio_url = endpoint;
    native_config.model = "auto";
    native_config.model_api = core::ModelApi::chat_completions;
    native_config.model_auto_load = false;
    native_config.lm_studio_token.clear();
    auto provider = std::make_unique<LmStudioProvider>(std::move(native_config));
    const auto result = provider.get();
    native_transports_.emplace(key, std::move(provider));
    return result;
}

std::expected<ModelResponse, core::Error> MultiplexProvider::complete(const ModelRequest& request) {
    auto translated = request;
    if (translated.deadline == std::chrono::steady_clock::time_point::max()) {
        translated.deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds{config_.inference_timeout_seconds};
    }
    const auto resolved_model = resolve_request_model(translated.model);
    translated.model = resolved_model;

    if (native_request(resolved_model)) {
        // Native non-streaming OpenAI/MCP clients must still expose live Qwen
        // fragments to Epoch AI's monitor. Only the wire response is buffered.
        return complete_stream(request, {});
    }

    if (!is_internal_model(resolved_model)) {
        const auto start = std::chrono::steady_clock::now();
        auto result = external_.complete(translated);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        record_inference(resolved_model, request_class(request), elapsed, -1.0,
            result ? reported_output_tokens(*result) : std::nullopt,
            result.has_value(), result ? "" : result.error().message);
        return result;
    }

#if EPOCH_EDITION_PUBLIC
    return std::unexpected(core::Error{core::make_error_code(core::Errc::model_unavailable),
        "This model runtime is unavailable in this edition. Select a loaded local model or configured provider."});
#endif

}

std::expected<ModelResponse, core::Error> MultiplexProvider::complete_stream(
    const ModelRequest& request,
    const StreamCallback& callback) {
    auto translated = request;
    if (translated.deadline == std::chrono::steady_clock::time_point::max()) {
        translated.deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds{config_.inference_timeout_seconds};
    }
    const auto resolved_model = resolve_request_model(translated.model);
    translated.model = resolved_model;
    const auto started = std::chrono::steady_clock::now();
    double first_token_ms = -1.0;
    const auto observed_callback = [&](const StreamEvent& event) {
        if (first_token_ms < 0.0 && !event.delta.empty())
            first_token_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
        if (callback) callback(event);
    };
    const auto finish = [&](std::expected<ModelResponse, core::Error> outcome) {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        record_inference(resolved_model, request_class(request), elapsed,
            first_token_ms, outcome ? reported_output_tokens(*outcome) : std::nullopt,
            outcome.has_value(), outcome ? "" : outcome.error().message);
        return outcome;
    };

    if (native_request(resolved_model)) {
        auto provider = native_provider(resolved_model, translated.deadline, translated.stop);
        if (!provider) return finish(std::unexpected(provider.error()));
        const auto trace_id = qwen_output_.begin(resolved_model, request.request_id);
        translated.model = "auto";
        const auto forward = [&](const StreamEvent& event) {
            switch (event.kind) {
                case StreamEventKind::text_delta:
                    qwen_output_.append(trace_id, resolved_model, "text", event.delta);
                    break;
                case StreamEventKind::reasoning_delta:
                    qwen_output_.append(trace_id, resolved_model, "reasoning", event.delta);
                    break;
                case StreamEventKind::tool_argument_delta:
                    qwen_output_.append(trace_id, resolved_model, "arguments", event.delta);
                    break;
                case StreamEventKind::tool_call_started: break;
            }
            observed_callback(event);
        };
        auto result = (*provider)->complete_stream(translated, forward);
        qwen_output_.finish(trace_id, resolved_model);
        if (!result) return finish(std::unexpected(native_transport_error(result.error())));
        return finish(std::move(result));
    }

    if (!is_internal_model(resolved_model)) {
        return finish(external_.complete_stream(translated, observed_callback));
    }

#if EPOCH_EDITION_PUBLIC
    return std::unexpected(core::Error{core::make_error_code(core::Errc::model_unavailable),
        "This model runtime is unavailable in this edition. Select a loaded local model or configured provider."});
#endif

}

ProviderCapabilities MultiplexProvider::capabilities() const noexcept {
    auto caps = external_.capabilities();
    caps.native_model_management = true;
    return caps;
}

std::expected<json, core::Error> MultiplexProvider::prepare(std::string_view model) {
    const auto resolved_model = resolve_request_model(model);

    if (is_native_runtime_model(resolved_model)) {
        auto profile = native_models_.find(resolved_model);
        if (!profile) {
            return std::unexpected(core::Error{
                core::make_error_code(core::Errc::model_unavailable),
                "Unknown native model: " + resolved_model,
            });
        }
        auto endpoint = native_models_.prepare(resolved_model);
        if (!endpoint) return std::unexpected(endpoint.error());
        auto profile_json = to_json(*profile);
        const auto runtime_state = native_models_.model_runtime_status(resolved_model);
        const auto active_context = runtime_state.value("active_context_tokens", profile->context_tokens);
        const auto active_slots = std::max(1U, runtime_state.value("active_parallel_slots", profile->parallel_slots));
        const auto active_effective_context = active_context / active_slots;
        profile_json["active_context_tokens"] = active_context;
        profile_json["active_parallel_slots"] = active_slots;
        profile_json["safety_fallback"] = runtime_state.value("safety_fallback", false);
        profile_json["startup_note"] = runtime_state.value("startup_note", nlohmann::json(nullptr));
        return json{
            {"model", resolved_model},
            {"management", "engcoder-native-runtime"},
            {"runtime", "qwen-flash"},
            {"role", "system1-foundation"},
            {"endpoint", *endpoint},
            {"profile", profile_json},
            {"context_tokens", active_context},
            {"parallel_slots", active_slots},
            {"effective_context_tokens", active_effective_context},
            {"auto_compact", profile->auto_compact},
            {"context_reserve_tokens", profile->context_reserve_tokens},
            {"compact_trigger_ratio", profile->compact_trigger_ratio},
            {"compact_target_ratio", profile->compact_target_ratio},
#if EPOCH_EDITION_PUBLIC
            {"harness", "native-inference"},
#endif
        };
    }
    if (!is_internal_model(resolved_model)) {
        const std::string requested{model};
        std::string resolved = resolved_model;
        if (resolved.empty() || resolved == "auto") {
            auto discovered = external_.discover();
            if (!discovered) return std::unexpected(discovered.error());
            resolved = discovered->value("selected_model", std::string{});
            if (resolved.empty() || resolved == "auto") {
                return std::unexpected(core::Error{
                    core::make_error_code(core::Errc::model_unavailable),
                    "No external model could be assigned to the request."
                });
            }
        }
        return json{
            {"model", resolved},
            {"requested_model", requested.empty() ? std::string{"auto"} : requested},
            {"management", "external-provider"}
        };
    }
#if EPOCH_EDITION_PUBLIC
    return std::unexpected(core::Error{core::make_error_code(core::Errc::model_unavailable),
        "This model runtime is unavailable in this edition. Select a loaded local model or configured provider."});
#endif
}

std::expected<json, core::Error> MultiplexProvider::residency() {
    json out{
        {"native", native_models_.describe()},
        {"performance", performance()},
    };
    return out;
}

std::expected<json, core::Error> MultiplexProvider::release_idle() {
#if EPOCH_EDITION_PUBLIC
    return json{{"native", native_models_.describe()},
        {"management", "Native residency is controlled by Model Manager"}};
#endif

}

std::expected<json, core::Error> MultiplexProvider::discover() {
    json out{
        {"provider", "engcoder-multiplex"},
        {"native", native_models_.describe()},
        {"models", json::array()},
    };

    for (const auto& profile : native_models_.models()) {
        auto cache = native_models_.qwen_flash().cache_status(profile);
        out["models"].push_back(json{
            {"id", runtime_model_uri(profile)},
            {"display_name", profile.display_name},
            {"runtime", to_string(profile.runtime)},
            {"native", true},
            {"installed", cache.value("installed", false)},
            {"downloadable", profile.source == ModelSourceKind::hugging_face},
        });
    }

    auto external = external_.discover();
    if (external) {
        out["external"] = *external;
        for (const auto& item : external->value("models", json::array())) out["models"].push_back(item);
        if (const auto it = external->find("selected_model"); it != external->end() && it->is_string()) {
            out["selected_model"] = it->get<std::string>();
        } else {
            out["selected_model"] = config_.model.empty() ? std::string{"auto"} : config_.model;
        }
    } else {
        out["external_error"] = external.error().message;
        out["selected_model"] = config_.model;
    }

    return out;
}

} // namespace epoch_ai::model
