// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <mutex>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "ai.backend_core_config.hpp"
#include "ai.backend_core_error.hpp"
#include "ai.backend_model_qwen_flash_runtime.hpp"
#include "ai.backend_model_runtime_model.hpp"
#include "../ai.native_idle_policy.hpp"

namespace epoch_ai::model {

class NativeResidency final {
public:
    [[nodiscard]] std::vector<std::string> ids() const;
    [[nodiscard]] std::vector<std::uint64_t> process_ids() const;
private:
    friend class NativeModelManager;
    struct State;
    explicit NativeResidency(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};

// Owns Epoch AI-native downloadable runtimes and their catalog. This layer is
// intentionally separate from both external providers and the agent loop.
class NativeModelManager {
public:
    NativeModelManager(const core::Config& config, std::filesystem::path state_root);
    ~NativeModelManager();

    NativeModelManager(const NativeModelManager&) = delete;
    NativeModelManager& operator=(const NativeModelManager&) = delete;

    [[nodiscard]] std::vector<RuntimeModelProfile> models() const;
    [[nodiscard]] std::optional<RuntimeModelProfile> find(std::string_view id_or_uri) const;
    [[nodiscard]] std::expected<nlohmann::json, core::Error> set_gpu_offload(
        std::string_view id_or_uri, std::uint32_t percent);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> update_tuning(
        std::string_view id_or_uri, const nlohmann::json& changes);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> register_model(const nlohmann::json& spec);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> install(std::string_view id_or_uri);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> provision_runtime();
    [[nodiscard]] std::expected<nlohmann::json, core::Error> remove(std::string_view id_or_uri, bool delete_cache);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> clear_transient_cache(std::string_view id_or_uri);
    [[nodiscard]] std::expected<nlohmann::json, core::Error> delete_downloaded_weights(std::string_view id_or_uri);
    [[nodiscard]] std::expected<std::string, core::Error> prepare(std::string_view id_or_uri, std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(), std::stop_token stop = {},
        std::optional<std::uint32_t> gpu_offload_cap_percent = std::nullopt);
    [[nodiscard]] std::expected<std::string, core::Error> prepare_with_local_override(
        std::string_view id_or_uri,
        const std::filesystem::path& local_path_override,
        std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(),
        std::stop_token stop = {});
    [[nodiscard]] std::optional<std::string> active_model_uri() const;
    void stop_active();
    void stop_model(std::string_view id_or_uri);
    void prefer_model(std::string_view id_or_uri);
    [[nodiscard]] bool model_loaded(std::string_view id_or_uri) const;
    [[nodiscard]] nlohmann::json model_runtime_status(std::string_view id_or_uri) const;
    [[nodiscard]] std::expected<std::shared_ptr<const NativeResidency>, core::Error> capture_residency(
        std::string_view exact_selected_id = {}) const;
    [[nodiscard]] std::expected<void, core::Error> suspend_residency(
        const NativeResidency&, const epochengine::ai::native_runtime::idle::RetirementClaim&);
    [[nodiscard]] std::expected<void, core::Error> restore_residency(
        const NativeResidency&, const epochengine::ai::native_runtime::idle::RetirementClaim&,
        std::chrono::steady_clock::time_point deadline, std::stop_token,
        const std::function<bool()>& still_current);
    [[nodiscard]] bool residency_settings_match(const NativeResidency&) const;
    void note_model_use(std::string_view id_or_uri,
        epochengine::ai::native_runtime::idle::Clock::time_point now =
            epochengine::ai::native_runtime::idle::Clock::now());
    [[nodiscard]] nlohmann::json retire_idle_models(
        epochengine::ai::native_runtime::idle::Clock::time_point now,
        const std::function<void(std::string_view)>& before_stop);
    void cancel_installation();

    [[nodiscard]] nlohmann::json describe() const;
    [[nodiscard]] nlohmann::json runtime_types() const;
    [[nodiscard]] QwenFlashRuntime& qwen_flash() noexcept { return qwen_flash_; }

private:
    friend struct NativeResidency::State;
    void seed_defaults();
    void load_profiles_locked();
    [[nodiscard]] std::filesystem::path manifest_path(std::string_view id) const;
    [[nodiscard]] std::string normalize_id(std::string_view value) const;
    [[nodiscard]] std::optional<RuntimeModelProfile> find_locked(std::string_view id_or_uri) const;
    void save_profile(const RuntimeModelProfile& profile, bool only_if_absent = false) const;
    void set_install_state(nlohmann::json state);
    struct GpuOwnership;
    struct GpuUse;
    [[nodiscard]] std::expected<std::shared_ptr<GpuOwnership>, core::Error> acquire_gpu_ownership();
    void release_gpu_ownership_if_retired() noexcept;
    mutable std::mutex gpu_owner_mutex_;
    // One OS coordination lease across all slots; no catalog-only acquisition.
    // Declared before slots so every runtime retires before final lease release.
    std::shared_ptr<GpuOwnership> gpu_owner_;

    const core::Config& config_;
    std::filesystem::path state_root_;
    std::filesystem::path catalog_root_;
    mutable std::mutex mutex_;
    std::vector<RuntimeModelProfile> profiles_;
    std::jthread install_worker_;
    std::jthread runtime_worker_;
    nlohmann::json install_state_ = nlohmann::json{{"state", "idle"}};
    nlohmann::json runtime_install_state_ = nlohmann::json{{"state", "idle"}};
    // Each resident model owns a process, an immutable configuration and a
    // unique loopback port. The shared_ptr keeps a slot alive while loading or
    // stopping even if the registry is refreshed concurrently.
    struct RuntimeSlot;
    [[nodiscard]] std::shared_ptr<RuntimeSlot> slot_for(const RuntimeModelProfile& profile);
    [[nodiscard]] std::shared_ptr<RuntimeSlot> find_slot(std::string_view id_or_uri) const;
    mutable std::mutex slots_mutex_;
    std::map<std::string, std::shared_ptr<RuntimeSlot>> slots_;
    std::string last_active_id_;
    QwenFlashRuntime qwen_flash_; // non-resident cache/runtime-tool utilities

};

[[nodiscard]] bool is_native_runtime_model(std::string_view model) noexcept;

} // namespace epoch_ai::model
