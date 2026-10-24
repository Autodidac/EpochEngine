// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include "ai.backend_controller_task_intent.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace epoch_ai::controller {

// Recipes contain only coarse task categories and one-way model fingerprints.
// They never store prompts, source code, tool arguments, or model paths.
[[nodiscard]] inline std::string recipe_category(std::string_view text) {
    std::string lower;
    lower.reserve(std::min<std::size_t>(text.size(), 4096));
    for (const unsigned char c : text.substr(0, 4096))
        lower.push_back(static_cast<char>(std::tolower(c)));
    const auto has = [&](std::string_view word) { return lower.find(word) != std::string::npos; };
    if (classify_task(text) == TaskIntent::simple) return "simple";
    if (classify_task(text) == TaskIntent::coding) {
        if (has("crash") || has("debug") || has("stack trace") || has("error")) return "coding-debug";
        if (has("refactor") || has("architect") || has("multiple") || has("across") ||
            has("engine") || has("repository") || has("codebase")) return "coding-project";
        if (has("example") || has("snippet") || has("hello world") || has("single function")) return "coding-snippet";
        return "coding-general";
    }
    if (requires_foundation_reasoning(text)) return "sensitive-or-quantitative";
    if (has("explain") || has("summarize") || has("describe")) return "explanation";
    return "general";
}

[[nodiscard]] inline bool contextual_followup(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const unsigned char c : text)
        lowered.push_back(static_cast<char>(std::tolower(c)));
    return lowered == "fix it" || lowered == "do it" ||
        lowered == "make it" || lowered == "continue" ||
        lowered == "finish it" || lowered == "that one";
}

[[nodiscard]] inline bool lightweight_code_request(std::string_view text) {
    return recipe_category(text) == "coding-snippet" &&
        text.size() <= 800 && !requires_foundation_reasoning(text);
}

// Routing is capability-first. Tool permission and human approval are entirely
// separate from response-tier selection. No model loads are triggered here.
[[nodiscard]] inline ResponseTier cheapest_eligible_tier(std::string_view text,
    bool small_ready, bool middle_ready, bool automatic) {
    if (!automatic) return ResponseTier::foundation;
    // Size/latency alone is not evidence that a smaller responder can handle
    // difficult work. Keep consequential and cross-system tasks on foundation.
    if (requires_foundation_reasoning(text) || warrants_specialist_handoff(text))
        return ResponseTier::foundation;
    // Short follow-ups inherit context that is not present in this isolated
    // message. Never give a tool-free small model authority to interpret them.
    if (contextual_followup(text))
        return ResponseTier::foundation;
    const auto intent = classify_task(text);
    if (intent == TaskIntent::simple || lightweight_code_request(text))
        return small_ready ? ResponseTier::small : middle_ready ? ResponseTier::middle : ResponseTier::foundation;
    if (intent == TaskIntent::coding) {
        // A measured recipe may admit bounded coding below. Unknown capability
        // must not silently become permission to handle project changes.
        return ResponseTier::foundation;
    }
    if (text.size() > 1200 || warrants_specialist_handoff(text))
        return ResponseTier::foundation;
    return middle_ready ? ResponseTier::middle : ResponseTier::foundation;
}

class RoutingRecipes final {
public:
    explicit RoutingRecipes(const std::filesystem::path& data_root) {
        if (data_root.empty()) return;
        const auto directory = data_root / "controller";
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) return;
        file_ = directory / "routing-recipes-v2.tsv";
        std::ifstream stream(file_, std::ios::binary);
        if (!stream) return;
        // A damaged or oversized journal must not stall native model startup.
        const auto size = std::filesystem::file_size(file_, ec);
        if (ec || size > max_bytes) return;
        std::string line;
        for (std::size_t lines = 0; lines < max_records && std::getline(stream, line); ++lines) {
            if (line.size() > 128) continue;
            const auto a = line.find('\t');
            const auto b = line.find('\t', a == std::string::npos ? 0 : a + 1);
            if (a == std::string::npos || b == std::string::npos || b + 2 != line.size()) continue;
            const auto category = line.substr(0, a);
            const auto identity = line.substr(a + 1, b - a - 1);
            if (!valid_key(category) || identity.size() != 16 ||
                !std::all_of(identity.begin(), identity.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) continue;
            auto& entry = records_[{category, identity}];
            if (line.back() == 'S') entry.successes = std::min(entry.successes + 1, 1000U);
            else if (line.back() == 'F') entry.failures = std::min(entry.failures + 1, 1000U);
        }
    }

    // Only verified goals/validation evidence count as S. A model merely
    // returning prose is not proof that code or a project objective works.
    void record(std::string_view category, std::string_view model, bool verified) {
        if (file_.empty() || model.empty() || !valid_key(category)) return;
        const auto identity = fingerprint(model);
        std::scoped_lock lock(mutex_);
        auto& entry = records_[{std::string{category}, identity}];
        if (verified) entry.successes = std::min(entry.successes + 1, 1000U);
        else entry.failures = std::min(entry.failures + 1, 1000U);
        std::error_code ec;
        if (std::filesystem::exists(file_, ec) && !ec &&
            std::filesystem::file_size(file_, ec) > max_bytes) return;
        std::ofstream out(file_, std::ios::binary | std::ios::app);
        if (out) out << category << '\t' << identity << '\t' << (verified ? 'S' : 'F') << '\n';
    }

    // Three meaningful failures with no verified wins exclude a model from
    // that task family. One flaky transport call never blacklists a model.
    [[nodiscard]] bool excluded(std::string_view category, std::string_view model) const {
        if (model.empty()) return false;
        std::scoped_lock lock(mutex_);
        const auto it = records_.find({std::string{category}, fingerprint(model)});
        return it != records_.end() && it->second.failures >= 3 && it->second.successes == 0;
    }

    [[nodiscard]] bool verified(std::string_view category, std::string_view model) const {
        if (model.empty()) return false;
        std::scoped_lock lock(mutex_);
        const auto it = records_.find({std::string{category}, fingerprint(model)});
        // Two host-verified successes in this family are a limited capability
        // signal, not a model-wide certification. Regressions revoke it.
        return it != records_.end() && it->second.successes >= 2
            && it->second.successes > it->second.failures;
    }

    // Measured durations must be based on >=3 real responses for the same
    // intent class. No synthetic throughput estimates from parameter counts.
    // Flash-Next is a user-reported slow last resort when a verified-capable
    // non-Flash middle model is resident. This is a default, not a blacklist.
    [[nodiscard]] ResponseTier select(std::string_view text,
        std::string_view small, bool small_ready, std::string_view middle,
        bool middle_ready, bool automatic,
        std::optional<double> small_latency_ms = {},
        std::optional<double> middle_latency_ms = {}) const {
        auto tier = cheapest_eligible_tier(text, small_ready, middle_ready, automatic);
        if (!automatic) return tier;
        const auto category = recipe_category(text);
        const bool small_eligible = small_ready && !excluded(category, small);
        const bool middle_eligible = middle_ready && !excluded(category, middle);
        if (tier == ResponseTier::foundation && middle_eligible
            && classify_task(text) == TaskIntent::coding
            && !requires_foundation_reasoning(text)
            && !warrants_specialist_handoff(text) && text.size() <= 1200
            && verified(category, middle))
            tier = ResponseTier::middle;
        if (tier == ResponseTier::small && !small_eligible)
            tier = middle_eligible ? ResponseTier::middle : ResponseTier::foundation;
        if (tier == ResponseTier::middle && !middle_eligible)
            tier = ResponseTier::foundation;
        // For simple responses, a larger model may actually be faster if its
        // offload and context settings are better. Only switch on a meaningful
        // measured latency advantage, never just because of its name.
        if (tier == ResponseTier::small && middle_eligible &&
            ((is_flash_next(small) && !is_flash_next(middle)) ||
             (small_latency_ms && middle_latency_ms &&
              *small_latency_ms > 0.0 && *middle_latency_ms > 0.0 &&
              *middle_latency_ms < *small_latency_ms * 0.75)))
            tier = ResponseTier::middle;
        return tier;
    }

    [[nodiscard]] static bool is_flash_next(std::string_view model) {
        std::string lower;
        lower.reserve(model.size());
        for (unsigned char c : model) lower.push_back(static_cast<char>(std::tolower(c)));
        return lower.find("flash-next") != std::string::npos ||
               lower.find("flash_next") != std::string::npos ||
               lower.find("flash next") != std::string::npos;
    }
private:
    struct Tally { unsigned successes{}; unsigned failures{}; };
    static constexpr std::uintmax_t max_bytes = 256 * 1024;
    static constexpr std::size_t max_records = 4096;
    [[nodiscard]] static bool valid_key(std::string_view key) {
        return !key.empty() && key.size() < 40 &&
            std::all_of(key.begin(), key.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || c == '-';
            });
    }
    [[nodiscard]] static std::string fingerprint(std::string_view text) {
        // Stable FNV-1a fingerprint. It is an index, not a cryptographic secret.
        std::uint64_t hash = 14695981039346656037ULL;
        for (const unsigned char c : text) { hash ^= c; hash *= 1099511628211ULL; }
        constexpr char hex[] = "0123456789abcdef";
        std::string result(16, '0');
        for (int i = 15; i >= 0; --i) { result[static_cast<std::size_t>(i)] = hex[hash & 15]; hash >>= 4; }
        return result;
    }
    std::filesystem::path file_;
    mutable std::mutex mutex_;
    std::map<std::pair<std::string, std::string>, Tally> records_;
};

} // namespace epoch_ai::controller
