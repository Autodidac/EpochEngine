// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_runtime_runtime_host.hpp"
#include "ai.backend_host_request.hpp"

#include "ai.backend_core_ids.hpp"
#include "ai.backend_addons_addon_registry.hpp"
#include "ai.backend_controller_architect_controller.hpp"
#include "ai.backend_model_multiplex_provider.hpp"
#include "ai.backend_model_native_model_manager.hpp"
#include "ai.backend_models_model_registry.hpp"
#include "ai.backend_residency_model_residency_manager.hpp"
#include "ai.backend_training_training_service.hpp"

#include <filesystem>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <random>
#include <sstream>
#include <thread>
#include <utility>

namespace epoch_ai::runtime {
namespace {



} // namespace


#if EPOCH_EDITION_PUBLIC
struct RuntimeHost::Impl {
    explicit Impl(core::Config initial_config)
        : config(std::move(initial_config)),
          models(config, config.data_root), residency(config.vram_budget_gib),
          native_models(config, config.data_root), model(config, native_models),
          addons(config, model, models), controller(config, model, addons, models, residency),
          training(config, config.data_root, models, residency) {
        models.refresh();
        residency.sync(models);
    }
    core::Config config;
    models::ModelRegistry models;
    residency::ModelResidencyManager residency;
    model::NativeModelManager native_models;
    model::MultiplexProvider model;
    addons::AddonRegistry addons;
    controller::ArchitectController controller;
    training::TrainingService training;
};
#endif

RuntimeHost::RuntimeHost(core::Config config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

RuntimeHost::~RuntimeHost() = default;

const core::Config& RuntimeHost::config() const noexcept {
    return impl_->config;
}

std::expected<json, core::Error> RuntimeHost::model_discovery() {
    auto discovery = impl_->model.discover();
    if (!discovery) return std::unexpected(discovery.error());
    const auto configured_foundation = impl_->models.foundation_model();
    const auto effective_foundation = impl_->model.resolve_model_hint(configured_foundation);
    (*discovery)["selected_model"] = effective_foundation;
    (*discovery)["foundation_model"] = effective_foundation;
    (*discovery)["configured_foundation_model"] = configured_foundation;
    return discovery;
}

json RuntimeHost::architecture() const {
    const auto addon_description = impl_->addons.describe();
    const auto configured_foundation = impl_->models.foundation_model();
    const auto effective_foundation = impl_->model.resolve_model_hint(configured_foundation);
    return json{
        {"format", "engcoder.architecture.v4"},
        {"strategy", "architect+keymaker-foundation+on-demand-addons"},
        {"controller", impl_->controller.describe()},
        {"foundation", {
            {"id", "keymaker"},
            {"role", "resident KeyMaker language/reasoning foundation"},
            {"model", effective_foundation},
            {"configured_model", configured_foundation},
            {"auto_resolved", effective_foundation != configured_foundation},
            {"recommended_size", "250M-400M"},
            {"user_facing", true},
        }},
        {"addons", addon_description.value("addons", json::array())},
        {"model_registry", impl_->models.describe()},
        {"residency", impl_->residency.describe()},
        {"training", impl_->training.status()},
        {"role_models",[&]{json roles={{"controller",impl_->models.controller_model()},{"foundation",effective_foundation},
            {"simple",impl_->models.role_model("simple")},
            {"implementation",impl_->models.role_model("implementation")},
            {"reviewer",impl_->models.role_model("reviewer")}};
            for(const auto& entry:impl_->config.addons)roles[entry.id]=impl_->addons.model_for(entry.id);return roles;}()},
        {"native_models", impl_->native_models.describe()},
        {"tool_policy", "deterministic C++ policy remains authoritative"},
    };
}

json RuntimeHost::native_model_library() const {
    auto library=impl_->native_models.describe();
    library["preferred_model"]=impl_->models.selected_foundation_model();
    library["performance"]=impl_->model.performance();
    return library;
}

std::expected<json, core::Error> RuntimeHost::register_native_model(const json& spec) {
    auto result = impl_->native_models.register_model(spec);
    if (!result) return std::unexpected(result.error());
    impl_->models.refresh();
    impl_->residency.sync(impl_->models);
    return *result;
}

std::expected<json, core::Error> RuntimeHost::set_native_gpu_offload(
    std::string_view id_or_uri, std::uint32_t percent) {
    return impl_->native_models.set_gpu_offload(id_or_uri, percent);
}

std::expected<json, core::Error> RuntimeHost::set_native_model_tuning(
    std::string_view id_or_uri, const json& changes) {
    return impl_->native_models.update_tuning(id_or_uri, changes);
}

std::expected<json, core::Error> RuntimeHost::provision_native_runtime() {
    return impl_->native_models.provision_runtime();
}

std::expected<json, core::Error> RuntimeHost::install_native_model(std::string_view id_or_uri) {
    return impl_->native_models.install(id_or_uri);
}

std::expected<json, core::Error> RuntimeHost::load_native_model(
    std::string_view id_or_uri,
    std::string_view local_path_override,
    std::stop_token cancellation,
    std::optional<std::uint32_t> gpu_offload_cap_percent) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes{30};
    auto endpoint = local_path_override.empty()
        ? impl_->native_models.prepare(id_or_uri,deadline,cancellation,gpu_offload_cap_percent)
        : impl_->native_models.prepare_with_local_override(id_or_uri, std::filesystem::path{local_path_override},deadline,cancellation);
    if (!endpoint) return std::unexpected(endpoint.error());
    const auto configured_foundation = impl_->models.foundation_model();
    const auto effective_foundation = impl_->model.resolve_model_hint(configured_foundation);
    return json{
        {"loaded", true},
        {"model", std::string{id_or_uri}},
        {"endpoint", *endpoint},
        {"local_path_override", local_path_override.empty() ? json(nullptr) : json(local_path_override)},
        {"effective_foundation_model", effective_foundation},
        {"library", impl_->native_models.describe()},
    };
}

void RuntimeHost::unload_native_model() {
    impl_->native_models.stop_active();
}
void RuntimeHost::unload_native_model(std::string_view id_or_uri) {
    impl_->native_models.stop_model(id_or_uri);
}
void RuntimeHost::cancel_native_installation() { impl_->native_models.cancel_installation(); }

std::expected<json, core::Error> RuntimeHost::remove_native_model(std::string_view id_or_uri, bool delete_cache) {
    auto result = impl_->native_models.remove(id_or_uri, delete_cache);
    if (!result) return std::unexpected(result.error());
    impl_->models.refresh();
    impl_->residency.sync(impl_->models);
    return *result;
}

std::expected<json, core::Error> RuntimeHost::clear_native_model_cache(std::string_view id_or_uri) {
    return impl_->native_models.clear_transient_cache(id_or_uri);
}

std::expected<json, core::Error> RuntimeHost::delete_native_model_weights(std::string_view id_or_uri) {
    auto result = impl_->native_models.delete_downloaded_weights(id_or_uri);
    if (!result) return std::unexpected(result.error());
    impl_->models.refresh();
    impl_->residency.sync(impl_->models);
    return *result;
}

std::expected<json, core::Error> RuntimeHost::set_foundation_model(std::string_view provider_model) {
    if (!impl_->models.set_foundation_model(provider_model)) {
        return std::unexpected(core::Error{
            core::make_error_code(core::Errc::model_unavailable),
            "Foundation model is not available: " + std::string{provider_model},
        });
    }
    impl_->native_models.prefer_model(provider_model);
    return json{
        {"foundation_model", impl_->models.foundation_model()},
        {"registry", impl_->models.describe()},
    };
}

std::string RuntimeHost::foundation_model() const {
    return impl_->model.resolve_model_hint(impl_->models.foundation_model());
}

json RuntimeHost::training_status() const { return impl_->training.status(); }
std::expected<json,core::Error> RuntimeHost::assign_model_role(std::string_view role,std::string_view model) {
    if(role=="foundation")return set_foundation_model(model);
    // System 1 roles are persisted in the same model registry as the addon roles.
    // A role selects an existing registered model; it does not load weights or
    // confer tool authority to the selected responder/reviewer.
    if ((role != "controller" && role != "simple" && role != "implementation" &&
         role != "reviewer" && role != "escalation" &&
         impl_->addons.find(role) == nullptr) || !impl_->models.set_role_model(role, model))
        return std::unexpected(core::Error{core::make_error_code(core::Errc::invalid_argument),"Choose a registered model and a supported role"});
    return architecture();
}

std::expected<model::ModelResponse, core::Error> RuntimeHost::complete_host_request(
    const json& body, std::stop_token stop,
    std::chrono::steady_clock::time_point deadline, const model::StreamCallback& callback) {
    struct InferenceLease final {
        training::TrainingService& service;
        explicit InferenceLease(training::TrainingService& value) : service(value) { service.begin_inference(); }
        ~InferenceLease() { service.end_inference(); }
    } lease{impl_->training};
    std::string chosen = foundation_model();
    // Source work uses the explicit resident implementation assignment or the
    // operator-selected resident model. Do not insert another generation or
    // silently switch models for review; Epoch owns independent validation.
    const auto stage = body.value("epoch_host_stage", std::string{});
    // Stage caps are enforced before model admission, not merely after routing.
    // Otherwise max_tokens=32768 disqualifies a resident 27B model with a
    // smaller active context and silently switches the coding pass to Flash.
    json boundedBody = body;
    // The Model Manager's test prompt must exercise the requested resident
    // model, not the foundation/router. Only this privileged internal stage
    // accepts a model override; regular host requests retain role routing.
    if (stage == "model_benchmark") {
        const auto requested = body.value("model", std::string{});
        if (!model::is_native_runtime_model(requested)
            || !impl_->native_models.model_loaded(requested))
            return std::unexpected(core::Error{
                core::make_error_code(core::Errc::model_unavailable),
                "Benchmark target is not a resident native model"});
        chosen = impl_->model.resolve_model_hint(requested);
    }
    const bool contains_images = boundedBody.contains("messages") && boundedBody.at("messages").is_array()
        && std::ranges::any_of(boundedBody.at("messages"), [](const json& message) {
            return message.is_object() && message.contains("content") && message.at("content").is_array()
                && std::ranges::any_of(message.at("content"), [](const json& part) {
                    return part.is_object() && part.value("type", std::string{}) == "image_url";
                });
        });
    if (contains_images) {
        const auto runtime = impl_->native_models.model_runtime_status(chosen);
        if (!runtime.is_object() || !runtime.contains("image_input")
            || !runtime.at("image_input").is_boolean() || !runtime.at("image_input").get<bool>())
            return std::unexpected(core::Error{
                core::make_error_code(core::Errc::model_unavailable),
                "The selected native runtime has no confirmed image input. Configure a matching vision loader "
                "or select a vision-capable Local API connection. No model request was sent."});
    }
    auto result = model::complete_host_request(impl_->model, boundedBody, chosen, stop, deadline, callback);
    if (result && result->model.empty()) result->model = chosen;
    return result;
}


std::string RuntimeHost::decision_mode() const {
    return "deterministic policy + local Architect controller";
}

std::string make_session_id(std::string_view prefix) {
    const auto now = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::random_device random;
    const auto entropy = (static_cast<std::uint64_t>(random()) << 32U) | random();

    std::ostringstream out;
    out << prefix << '-' << std::hex << now << '-' << entropy;
    return out.str();
}

} // namespace epoch_ai::runtime
