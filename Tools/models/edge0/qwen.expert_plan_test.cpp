// CPU-only check of the routing plan used by the Edge0 decoder overlay.
#include "qwen.edge0_pack.hpp"

using llama_edge0_detail::expert_slots;
using llama_edge0_detail::plan_expert_uploads;
using llama_edge0_detail::routed_experts;

static bool check_plan(const expert_slots & resident, const expert_slots & requested, int expected_misses) {
    const auto plan = plan_expert_uploads(resident, requested.data());
    std::array<bool, routed_experts> used{};
    int misses = 0;
    for (int route = 0; route < routed_experts; ++route) {
        const int slot = plan.slots_by_route[route];
        if (slot < 0 || slot >= routed_experts || used[slot] || plan.experts_by_slot[slot] != requested[route]) {
            return false;
        }
        used[slot] = true;
        if (plan.misses[slot] != (resident[slot] != requested[route])) return false;
        if (plan.misses[slot]) ++misses;
    }
    return misses == expected_misses;
}

int main() {
    // Disabled diagnostics do not read clocks or mutate totals. Enabled
    // accumulation uses a controlled prior timestamp, not a sleep threshold.
    llama_edge0_detail::phase_times phases;
    phases.enabled = false;
    phases.end(phases.begin(), phases.read_ns);
    if (phases.read_ns != 0) return 7;
    phases.enabled = true;
    phases.end(llama_edge0_detail::phase_times::clock::now() - std::chrono::seconds{1}, phases.read_ns);
    if (phases.read_ns < 1'000'000'000ULL || phases.enqueue_ns != 0) return 8;

    const expert_slots filled{0, 1, 2, 3, 4, 5, 6, 7, 8, 511};
    const expert_slots reversed{511, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    expert_slots empty;
    empty.fill(-1);
    if (!check_plan(empty, filled, routed_experts)) return 1;
    if (!check_plan(filled, reversed, 0)) return 2;

    // Misses precede late hits in request order. A hit must never be evicted
    // to satisfy an earlier miss.
    const expert_slots partial{42, 43, 0, 2, 3, 4, 5, 6, 8, 511};
    const auto partial_plan = plan_expert_uploads(filled, partial.data());
    if (!check_plan(filled, partial, 2) || !partial_plan.misses[1] || !partial_plan.misses[7]) return 3;

    // Repeated expert IDs must occupy distinct physical slots on CUDA paths
    // that group by expert. Only the missing occurrence needs another upload.
    const expert_slots duplicate{511, 0, 511, 1, 2, 3, 4, 5, 6, 7};
    const auto duplicate_plan = plan_expert_uploads(filled, duplicate.data());
    if (!check_plan(filled, duplicate, 1)) return 4;
    const expert_slots duplicate_reordered{511, 7, 6, 5, 4, 3, 2, 1, 0, 511};
    if (!check_plan(duplicate_plan.experts_by_slot, duplicate_reordered, 0)) return 5;

    expert_slots repeat_all;
    repeat_all.fill(511);
    if (!check_plan(empty, repeat_all, routed_experts) || !check_plan(repeat_all, repeat_all, 0)) return 6;
    return 0;
}
