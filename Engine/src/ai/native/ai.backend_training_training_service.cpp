// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_training_training_service.hpp"


#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>


#if EPOCH_EDITION_PUBLIC
namespace epoch_ai::training {
// Public retains inference lease accounting for the existing host/agent
// lifecycle, without a worker, corpus, backend discovery or mutation API.
TrainingService::TrainingService(const core::Config&, std::filesystem::path,
    models::ModelRegistry&, residency::ModelResidencyManager&) {}
TrainingService::~TrainingService() = default;

void TrainingService::begin_inference() {
    inference_users_.fetch_add(1u, std::memory_order_acq_rel);
}
void TrainingService::end_inference() {
    auto prior = inference_users_.load(std::memory_order_acquire);
    while (prior != 0u && !inference_users_.compare_exchange_weak(
        prior, prior - 1u, std::memory_order_acq_rel)) {}
}
json TrainingService::status() const {
    return json{{"enabled", false}, {"running", false},
        {"state", "not included in this edition"},
        {"inference_users", inference_users_.load(std::memory_order_acquire)}};
}
} // namespace epoch_ai::training
#endif
