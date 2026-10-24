// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
#include "../../include/epoch.edition.hpp"
// Epoch-owned, native C++23 Epoch AI service. The public boundary deliberately
// does not leak nlohmann/json or backend headers into Epoch C++ module BMIs.
#include <stop_token>
#include <cstdint>
#include <vector>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <functional>

namespace epochengine::ai::native_runtime {
    struct Result final {
        bool ok{};
        std::string message{};
        // Actual provider-reported usage; absent means no measured t/s.
        std::uint32_t completion_tokens{};
        double generation_tps{};
        bool generation_tps_known{};
    };
    struct ModelTuning final {
        std::string runtime_policy{"recommended"};
        std::string gpu_layers{"auto"};
        std::uint32_t gpu_layer_count_hint{48};
        std::uint32_t context_tokens{81'920}, parallel_slots{1};
        std::uint32_t context_reserve_tokens{4'096};
        bool auto_compact{true};
        double compact_trigger_ratio{0.97}, compact_target_ratio{0.72};
        std::uint32_t threads{12}, batch_threads{12};
        std::uint32_t batch_size{1024}, ubatch_size{256};
        bool flash_attention{true};
        std::string kv_type_k{"q4_0"}, kv_type_v{"q4_0"};
        bool kv_offload{false}, kv_cpu_pinned{true};
        std::uint32_t kv_gpu_layers{0};
        std::uint32_t prompt_cache_mib{2048};
        std::string expert_cache_mode{"auto"};
        std::uint32_t expert_cache_mib{6144}, pinned_host_mib{12288};
        bool speculative_enabled{false};
        std::string speculative_model{};
        std::uint32_t speculative_tokens{2};
    };
    // llama-server -c is the shared context across -np slots. Use the actual
    // loaded slot count, never the configured total as one prompt's capacity.
    [[nodiscard]] constexpr std::uint32_t effective_loaded_context_tokens(
        const std::uint32_t total, const std::uint32_t slots) noexcept
    {
        return total == 0u ? 0u : total / (slots == 0u ? 1u : slots);
    }
    struct ModelEntry final {
        ModelTuning loaded_tuning{};
        bool loaded_tuning_known{};
        std::string loaded_gpu_layers{};
        std::string id{}, label{}, uri{}, runtime{}, role{};
        bool installed{}, active{}, trainable{};
        std::uint64_t cached_bytes{}, expected_bytes{};
        std::uint32_t gpu_offload_percent{100};
        // Actual context after runtime fallbacks, not the profile's requested size.
        // Zero means unavailable/unknown; never present a requested size as measured.
        std::uint32_t loaded_context_tokens{};
        std::uint32_t loaded_parallel_slots{};
        std::string effective_gpu_layers{};
        ModelTuning tuning{};
        std::uint64_t requests{}, inference_ok{}, agent_completed{}, agent_failed{};
        double avg_request_ms{}, avg_first_token_ms{}, reported_tps{};
        bool tps_known{}, first_token_known{}, simple_baseline_available{};
        double simple_latency_difference_ms{};
        bool image_input_known{}, image_input_supported{};
    };
    struct TrainingState final {
        bool running{}, enabled{}, progress_known{};
        std::string state{}, scope{}, error{};
        std::uint64_t step{}, maximum_steps{};
    };
    struct ModelBenchmark final {
        std::string model_id{}, status{}, reply{};
        double elapsed_ms{}, first_token_ms{}, output_tps{};
        std::uint32_t output_tokens{};
        bool completed{}, tps_known{}, first_token_known{};
    };
    struct Snapshot final {
        bool busy{};
        bool loaded{};
        bool selected{};
        bool inventory_preview{}; // Read-only manifest scan; cache state is not verified.
        std::string model{};
        std::string last_response_model{};
        std::string last_response_role{};
        std::string phase{};
        std::string recent_events{};
        std::string storage_root{};
        std::vector<ModelEntry> models{}, roles{};
        ModelBenchmark benchmark{};
        TrainingState training{};
        std::string routing{};
        std::string inventory_error{};
        std::uint64_t output_sequence{};
        bool image_input_known{}, image_input_supported{};
    };
    enum class OutputKind { boundary, text, reasoning, tool_arguments };
    struct EventBlock final {
        std::uint64_t sequence{};
        std::string text{};
        OutputKind kind{OutputKind::boundary};
    };
    struct ActivityBlock final {
        std::uint64_t sequence{};
        std::string group{}, text{};
    };
    [[nodiscard]] Snapshot snapshot(bool include_inventory = true);
    // Bounded native stream capture feeds the single chronological Activity tree by default.
    // This never changes model generation, tool permission, or host validation.
    void set_raw_stream_capture(bool enabled) noexcept;
    [[nodiscard]] bool raw_stream_capture_enabled() noexcept;
    [[nodiscard]] std::vector<EventBlock> events_since(std::uint64_t last_seen);
    [[nodiscard]] std::vector<ActivityBlock> activities_since(std::uint64_t last_seen);
    // Operations are queued off the editor/UI thread; they never launch Epoch AIServer.
    [[nodiscard]] bool provision();
    [[nodiscard]] bool install(std::string model_id);
    [[nodiscard]] bool load(std::string model_id, bool select_for_requests = true);
    // Explicit user-owned three-model load set: read-only inventory scan on opening; no weights loaded.
    [[nodiscard]] bool load_set(std::vector<std::string> model_ids);
    [[nodiscard]] bool save_load_set(const std::vector<std::string>& model_ids);
    [[nodiscard]] bool load_saved_set();
    // Select a confirmed resident model without restarting its GGUF server.
    [[nodiscard]] bool use_loaded(std::string model_id);
    [[nodiscard]] bool unload();
    [[nodiscard]] bool unload(std::string model_id);
    [[nodiscard]] bool refresh();
    // Selecting a method/model starts one owned readiness flow: reuse cache,
    // provision/install if required, load, select and report actual failures.
    [[nodiscard]] bool prepare(std::string model_id = {});
    void cancel_setup();
    // Two-phase application teardown: signal before context cleanup, join only
    // after request owners retire. Neither call initializes an unused runtime.
    void request_shutdown() noexcept;
    void finish_shutdown() noexcept;
    [[nodiscard]] bool shutdown_contract() noexcept;
    [[nodiscard]] bool register_model(std::string model_id, std::string local_path);
    // Explicit operator-selected metadata only; never loads or selects a model.
    [[nodiscard]] bool import_model_profiles(std::string selected_path);
    // Profile only: loads already in GPU memory require unload/reload.
    [[nodiscard]] bool set_gpu_offload(std::string model_id, std::uint32_t percent);
    [[nodiscard]] bool save_model_tuning(std::string model_id, ModelTuning tuning);
    // One tools-disabled, fixed-output model-specific test; async UI safe.
    [[nodiscard]] bool benchmark_model(std::string model_id, std::string prompt);
    // Removes only the Epoch model profile; never deletes GGUF weights.
    [[nodiscard]] bool unregister_model(std::string model_id);
    [[nodiscard]] bool assign_role(std::string role,std::string model_uri);
    [[nodiscard]] bool select_for_epoch_requests(bool enabled);
    [[nodiscard]] bool selected() noexcept;
    using StreamObserver = std::function<void(OutputKind, std::string_view)>;
    [[nodiscard]] Result complete_host_request(std::string_view request_json,
        std::uint32_t timeout_seconds, std::stop_token cancellation,
        const StreamObserver& observer = {});
}
