// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_core_ids.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace epoch_ai::core {
std::string make_id(std::string_view prefix) {
    static std::atomic_uint64_t counter{0};
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream out;
    out << prefix << '_' << std::hex << now << '_' << counter.fetch_add(1, std::memory_order_relaxed) << '_' << rng();
    return out.str();
}
}
