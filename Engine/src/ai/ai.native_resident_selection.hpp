// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
#include "ai.model_capability.hpp"
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::ai::native_runtime::detail
{
    // A fallback size hint, never a measured capability rating. For MoE names,
    // keep total parameters (35B-A3B -> 35), not the final active-count suffix.
    [[nodiscard]] inline std::optional<unsigned> named_parameter_billions(std::string_view id)
    {
        std::optional<unsigned> result;
        for (std::size_t i{}; i < id.size(); ++i) {
            if (id[i] < '0' || id[i] > '9' || (i && id[i-1] >= '0' && id[i-1] <= '9')) continue;
            unsigned count{};
            const auto parsed = std::from_chars(id.data()+i, id.data()+id.size(), count);
            if (parsed.ec != std::errc{} || parsed.ptr == id.data()+id.size()
                || (*parsed.ptr != 'b' && *parsed.ptr != 'B') || count == 0 || count >= 400) continue;
            const auto tail = parsed.ptr+1;
            if (tail != id.data()+id.size() && ((*tail >= 'a' && *tail <= 'z')
                || (*tail >= 'A' && *tail <= 'Z') || (*tail >= '0' && *tail <= '9'))) continue;
            result = (std::max)(result.value_or(0), count);
        }
        return result;
    }

    struct RoleCandidate final {
        std::string id;
        model_capability::Tier tier{model_capability::Tier::unclassified};
        bool eligible{true};
    };
    struct RoleDefaults final { std::string foundation, simple, middle, implementation; };
    [[nodiscard]] inline RoleDefaults resident_defaults(std::span<const RoleCandidate> candidates,
        std::string_view current_foundation = {})
    {
        std::vector<RoleCandidate> eligible;
        for (const auto& item : candidates)
            if (item.eligible && item.tier != model_capability::Tier::unclassified) eligible.push_back(item);
        if (eligible.empty()) return {};
        std::sort(eligible.begin(), eligible.end(), [&](const auto& a, const auto& b) {
            if (a.tier != b.tier) return a.tier < b.tier;
            if ((a.id == current_foundation) != (b.id == current_foundation)) return b.id == current_foundation;
            return a.id < b.id;
        });
        RoleDefaults out{.foundation=eligible.back().id};
        // Chat ownership and permission to generate code are independent. A
        // saved tier override cannot restore an observed coding restriction.
        for (auto it = eligible.rbegin(); it != eligible.rend(); ++it)
            if (model_capability::coding_restriction(it->id).empty()) {
                out.implementation = it->id;
                break;
            }
        for (const auto& item : eligible)
            if (item.id != out.foundation) { out.simple = item.id; break; }
        for (auto it = eligible.rbegin(); it != eligible.rend(); ++it)
            if (it->id != out.foundation && it->id != out.simple) { out.middle = it->id; break; }
        return out;
    }

    // The first loaded native model establishes chat ownership. An additional
    // resident model must not replace the foundation unless explicitly chosen.
    [[nodiscard]] constexpr bool should_establish_foundation(
        bool has_foundation, bool select_for_requests, bool requires_explicit_selection = false) noexcept
    {
        return select_for_requests || (!has_foundation && !requires_explicit_selection);
    }
}
