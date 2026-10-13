#pragma once
// Epoch-owned, native C++23 Epoch AI service. The public boundary deliberately
// does not leak nlohmann/json or backend headers into Epoch C++ module BMIs.
#include <stop_token>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

namespace epochengine::ai::native_runtime {
    struct Result final { bool ok{}; std::string message{}; };
    struct ModelEntry final {
        std::string id{}, label{}, uri{}, runtime{}, role{};
        bool installed{}, active{}, trainable{};
        std::uint64_t cached_bytes{}, expected_bytes{};
    };
    struct ToolEntry final { std::string name{}, description{}; };
    struct TrainingState final {
        bool running{}, enabled{}, progress_known{};
        std::string state{}, scope{}, error{};
        std::uint64_t step{}, maximum_steps{};
    };
    struct Snapshot final {
        bool busy{};
        bool loaded{};
        bool selected{};
        std::string model{};
        std::string phase{};
        std::string recent_events{};
        std::string storage_root{};
        std::vector<ModelEntry> models{}, roles{};
        std::vector<ToolEntry> tools{};
        TrainingState training{};
        std::string routing{};
        std::string inventory_error{};
        std::uint64_t output_sequence{};
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
    [[nodiscard]] std::vector<EventBlock> events_since(std::uint64_t last_seen);
    [[nodiscard]] std::vector<ActivityBlock> activities_since(std::uint64_t last_seen);
    // Operations are queued off the editor/UI thread; they never launch Epoch AIServer.
    [[nodiscard]] bool provision();
    [[nodiscard]] bool install(std::string model_id);
    [[nodiscard]] bool load(std::string model_id);
    [[nodiscard]] bool unload();
    [[nodiscard]] bool refresh();
    // Selecting a method/model starts one owned readiness flow: reuse cache,
    // provision/install if required, load, select and report actual failures.
    [[nodiscard]] bool prepare(std::string model_id = {});
    void cancel_setup();
    [[nodiscard]] bool register_model(std::string model_id, std::string local_path);
    [[nodiscard]] bool assign_role(std::string role,std::string model_uri);
    [[nodiscard]] bool prepare_training(std::string profile = "micro");
    [[nodiscard]] bool start_training(std::string scope, std::uint32_t steps, bool require_cuda);
    [[nodiscard]] bool cancel_training();
    [[nodiscard]] bool pause_training(bool paused);
    [[nodiscard]] bool select_for_epoch_requests(bool enabled);
    [[nodiscard]] bool selected() noexcept;
    [[nodiscard]] Result run_agent(std::string_view message, std::stop_token cancellation = {});
    using StreamObserver = std::function<void(OutputKind, std::string_view)>;
    [[nodiscard]] Result complete_host_request(std::string_view request_json,
        std::uint32_t timeout_seconds, std::stop_token cancellation,
        const StreamObserver& observer = {});
}
