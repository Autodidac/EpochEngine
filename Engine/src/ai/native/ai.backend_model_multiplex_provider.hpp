// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <memory>
#include <mutex>
#include <map>
#include <filesystem>

#include "ai.backend_core_config.hpp"
#include "../../../include/epoch.edition.hpp"
#include "ai.backend_model_lm_studio_provider.hpp"
#include "ai.backend_model_model_provider.hpp"
#include "ai.backend_model_qwen_output_journal.hpp"
#include "ai.backend_model_native_model_manager.hpp"

namespace epoch_ai::model {

class MultiplexProvider final : public IModelProvider {
public:
    MultiplexProvider(core::Config config,
        NativeModelManager& native_models);

    [[nodiscard]] std::expected<ModelResponse, core::Error> complete(const ModelRequest&) override;
    [[nodiscard]] std::expected<ModelResponse, core::Error> complete_stream(const ModelRequest&, const StreamCallback&) override;
    [[nodiscard]] ProviderCapabilities capabilities() const noexcept override;
    void report_task_outcome(std::string_view model, bool completed) override;
    [[nodiscard]] std::optional<double> observed_latency_ms(
        std::string_view model, std::string_view task_class) const override;
    [[nodiscard]] bool is_model_resident(std::string_view model) const override;
    [[nodiscard]] json performance() const;
    [[nodiscard]] std::expected<json, core::Error> discover() override;
    [[nodiscard]] std::expected<json, core::Error> prepare(std::string_view model) override;
    [[nodiscard]] std::expected<json, core::Error> residency() override;
    [[nodiscard]] std::expected<json, core::Error> release_idle() override;

    // Resolves only already-known local state. It never starts a model or
    // performs provider discovery, so UI/status callers can use it cheaply.
    [[nodiscard]] std::string resolve_model_hint(std::string_view requested) const;

    [[nodiscard]] LmStudioProvider& external() noexcept { return external_; }
    [[nodiscard]] NativeModelManager& native_models() noexcept { return native_models_; }
    [[nodiscard]] QwenOutputSnapshot qwen_output_after(std::uint64_t sequence) const { return qwen_output_.after(sequence); }

private:
    [[nodiscard]] bool native_request(std::string_view model) const noexcept;
    [[nodiscard]] std::string resolve_request_model(std::string_view requested) const;
    [[nodiscard]] std::expected<LmStudioProvider*, core::Error> native_provider(std::string_view model, std::chrono::steady_clock::time_point deadline, std::stop_token stop);

    void record_inference(std::string_view model, std::string_view task_class,
        double elapsed_ms, double first_token_ms, std::optional<std::uint64_t> tokens,
        bool succeeded, std::string_view error);
    void save_performance_locked() const;
    mutable std::mutex perf_mutex_;
    json perf_stats_ = json::object();
    // Member initialization follows DECLARATION order, not initializer-list order.
    // perf_file_ reads config_.data_root; construct config_ before perf_file_.
    core::Config config_;
    std::filesystem::path perf_file_;
    QwenOutputJournal qwen_output_;
    NativeModelManager& native_models_;
    LmStudioProvider external_;
    mutable std::mutex transports_mutex_;
    std::map<std::string, std::unique_ptr<LmStudioProvider>> native_transports_;
};

} // namespace epoch_ai::model
