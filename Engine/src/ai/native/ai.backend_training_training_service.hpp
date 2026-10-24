// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
#include "../../../include/epoch.edition.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

#include <nlohmann/json.hpp>

#include "ai.backend_core_config.hpp"
#include "ai.backend_core_error.hpp"
#include "ai.backend_models_model_registry.hpp"
#include "ai.backend_residency_model_residency_manager.hpp"

namespace epoch_ai::training {

using json = nlohmann::json;


class TrainingService {
public:
    TrainingService(
        const core::Config& config,
        std::filesystem::path state_root,
        models::ModelRegistry& models,
        residency::ModelResidencyManager& residency);
    ~TrainingService();

    TrainingService(const TrainingService&) = delete;
    TrainingService& operator=(const TrainingService&) = delete;

    void begin_inference();
    void end_inference();

    [[nodiscard]] json status() const;
#if EPOCH_EDITION_PUBLIC
    [[nodiscard]] bool running() const noexcept { return false; }
#endif

private:
    std::atomic_uint32_t inference_users_{0};
};

} // namespace epoch_ai::training
