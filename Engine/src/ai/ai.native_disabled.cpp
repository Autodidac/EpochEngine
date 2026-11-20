// Epoch Engine - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "ai.native_runtime.hpp"

namespace epochengine::ai::native_runtime
{
    namespace
    {
        constexpr char unavailable[] =
            "Local models are unavailable in this build. Use an Epoch build with native AI enabled.";
    }

    namespace detail
    {
        // The ordinary project compiler shares the handoff API. Its permit is
        // scoped to the calling thread and expires when the callback returns.
        struct AssetStagePermitOwner final
        {
            std::shared_ptr<std::atomic<std::uint64_t>> generation =
                std::make_shared<std::atomic<std::uint64_t>>(1u);
            AssetStagePermit permit{generation, 1u, std::this_thread::get_id()};

            ~AssetStagePermitOwner() { generation->store(0u, std::memory_order_release); }
        };
    }

    SharedRuntimeEnvironment capture_shared_runtime_environment(
        const std::filesystem::path&, std::span<const std::filesystem::path>)
    {
        return {.diagnostic = unavailable};
    }

    AssetStageOutcome run_asset_stage(AssetStageCallback callback, std::stop_token cancellation,
        std::chrono::seconds queue_timeout, AssetStageRestoration restoration)
    {
        if (cancellation.stop_requested())
            return {.code = AssetStageCode::cancelled, .workers_retired = true,
                .diagnostic = "Project handoff cancelled before starting."};
        if (restoration != AssetStageRestoration::leave_unloaded)
            return {.workers_retired = true, .diagnostic = unavailable};
        if (!callback || queue_timeout <= std::chrono::seconds::zero()
            || queue_timeout > std::chrono::hours{1})
            return {.workers_retired = true, .diagnostic = "A bounded project callback is required."};

        detail::AssetStagePermitOwner authority;
        try
        {
            const auto completed = callback(cancellation, authority.permit);
            const auto code = !completed.workers_retired ? AssetStageCode::workers_unretired
                : completed.succeeded ? AssetStageCode::completed
                : cancellation.stop_requested() ? AssetStageCode::cancelled : AssetStageCode::stage_failed;
            return {.code = code, .stage_started = true, .stage_succeeded = completed.succeeded,
                .workers_retired = completed.workers_retired,
                .diagnostic = completed.workers_retired ? "Project handoff ran without local models."
                    : "Project worker retirement is unverified."};
        }
        catch (...)
        {
            return {.code = AssetStageCode::workers_unretired, .stage_started = true,
                .diagnostic = "Project callback failed and worker retirement is unverified."};
        }
    }

    Snapshot snapshot(bool include_inventory)
    {
        Snapshot state{};
        state.phase = "Local models unavailable in this build";
        if (include_inventory)
            state.inventory_error = unavailable;
        return state;
    }

    void set_raw_stream_capture(bool) noexcept {}
    bool raw_stream_capture_enabled() noexcept { return false; }
    std::vector<EventBlock> events_since(std::uint64_t) { return {}; }
    std::vector<ActivityBlock> activities_since(std::uint64_t) { return {}; }

    bool provision() { return false; }
    bool install(std::string) { return false; }
    std::filesystem::path model_install_cache_root() noexcept { return {}; }
    bool load(std::string, bool) { return false; }
    bool load_set(std::vector<std::string>) { return false; }
    bool save_load_set(const std::vector<std::string>&) { return false; }
    bool load_saved_set() { return false; }
    bool use_loaded(std::string) { return false; }
    bool unload() { return false; }
    bool unload(std::string) { return false; }
    bool refresh() { return false; }
    bool discover_model_files(std::filesystem::path) { return false; }
    bool reconnect_model_file(std::string, std::string) { return false; }
    bool prepare(std::string) { return false; }
    bool reload_idle_selected(std::string_view, std::uint64_t, std::stop_token) { return false; }
    void cancel_setup() {}
    void request_shutdown() noexcept {}
    void finish_shutdown() noexcept {}
    bool shutdown_contract() noexcept { return !selected() && !raw_stream_capture_enabled(); }

    bool register_model(std::string, std::string, bool) { return false; }
    bool import_model_profiles(std::string) { return false; }
    bool set_gpu_offload(std::string, std::uint32_t) { return false; }
    bool save_model_tuning(std::string, ModelTuning) { return false; }
    bool benchmark_model(std::string, std::string) { return false; }
    bool unregister_model(std::string) { return false; }
    bool delete_model_offer_download(std::string) { return false; }
    bool assign_role(std::string, std::string) { return false; }
    bool select_for_epoch_requests(bool enabled) { return !enabled; }
    bool selected() noexcept { return false; }

    Result complete_host_request(std::string_view, std::uint32_t, std::stop_token,
        const StreamObserver&, const std::function<void()>&)
    {
        return {.message = unavailable};
    }

}
