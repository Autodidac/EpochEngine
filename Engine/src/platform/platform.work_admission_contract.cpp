// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cstdint>
#include <limits>

import platform.work_admission;

namespace
{
    using namespace epochengine::platform::work_admission;
    constexpr std::uint64_t gib = 1024ull * 1024ull * 1024ull;

    [[nodiscard]] ResourceSample healthy(std::uint64_t now) noexcept
    {
        return {now, 32u * gib, 16u * gib, 0.20, true, true};
    }

    [[nodiscard]] int run_contract()
    {
        Controller gate{};
        if (gate.poll(0u, {}, false, false).reason != Reason::idle) return 1;
        if (gate.queue({0u, WorkKind::model}, 0u)) return 2;
        if (gate.queue({1u, static_cast<WorkKind>(255u)}, 0u)) return 3;
        if (!gate.queue({1u, WorkKind::compiler}, 1'000u)
            || !gate.queue({1u, WorkKind::compiler}, 9'000u)
            || gate.queue({2u, WorkKind::preview}, 9'000u)) return 4;
        if (gate.poll(1'000u, healthy(1'000u), false, false).remaining_ms != 30'000u)
            return 5;
        if (gate.poll(30'999u, healthy(30'999u), false, false).remaining_ms != 1u)
            return 6;
        if (gate.consume(2u, 31'000u, healthy(31'000u), false, false)
            || !gate.consume(1u, 31'000u, healthy(31'000u), false, false)
            || gate.consume(1u, 31'000u, healthy(31'000u), false, false)
            || gate.queue({1u, WorkKind::compiler}, 31'000u)) return 7;
        gate.note_finished(50'000u);
        if (!gate.queue({2u, WorkKind::validation}, 50'000u)) return 8;
        if (gate.poll(50'000u, healthy(50'000u), false, false).remaining_ms != 30'000u)
            return 9;
        if (gate.poll(80'000u, healthy(80'000u), true, false).reason != Reason::awaiting_choice)
            return 10;
        if (gate.consume(2u, 200'000u, healthy(200'000u), true, false)) return 11;
        if (gate.poll(201'000u, healthy(201'000u), false, false).remaining_ms != 30'000u)
            return 12;
        if (gate.poll(220'000u, healthy(220'000u), false, true).reason != Reason::other_work_active)
            return 13;
        if (gate.poll(250'000u, healthy(250'000u), false, false).remaining_ms != 30'000u)
            return 14;
        if (!gate.consume(2u, 280'000u, healthy(280'000u), false, false)) return 15;
        if (gate.queue({1u, WorkKind::compiler}, 280'000u)
            || gate.queue({2u, WorkKind::validation}, 280'000u)) return 35;

        if (!gate.queue({3u, WorkKind::model}, 300'000u)) return 16;
        auto sample = healthy(300'000u);
        sample.cpu_valid = false;
        if (gate.poll(300'000u, sample, false, false).reason != Reason::resources_unavailable)
            return 17;
        sample = healthy(300'000u);
        if (gate.poll(305'001u, sample, false, false).reason != Reason::resources_stale)
            return 18;
        sample = healthy(306'000u);
        sample.available_memory_bytes = gib;
        if (gate.poll(306'000u, sample, false, false).reason != Reason::memory_pressure)
            return 19;
        sample = healthy(307'000u);
        sample.total_memory_bytes = 256u * gib;
        sample.available_memory_bytes = 3u * gib;
        if (gate.poll(307'000u, sample, false, false).reason != Reason::memory_pressure)
            return 20;
        sample = healthy(308'000u);
        sample.cpu_busy_fraction = 0.95;
        if (gate.poll(308'000u, sample, false, false).reason != Reason::cpu_pressure)
            return 21;
        sample.cpu_busy_fraction = (std::numeric_limits<double>::quiet_NaN)();
        if (gate.poll(308'000u, sample, false, false).reason != Reason::resources_unavailable)
            return 22;
        sample = healthy(309'000u);
        sample.available_memory_bytes = sample.total_memory_bytes + 1u;
        if (gate.poll(309'000u, sample, false, false).reason != Reason::resources_unavailable)
            return 23;
        sample = healthy(311'000u);
        if (gate.poll(310'000u, sample, false, false).reason != Reason::resources_stale)
            return 24;
        if (gate.poll(312'000u, healthy(312'000u), false, false).remaining_ms != 30'000u)
            return 25;
        if (!gate.poll(342'000u, healthy(342'000u), false, false).ready()) return 26;
        sample = healthy(342'000u);
        sample.cpu_busy_fraction = 0.99;
        if (gate.consume(3u, 342'000u, sample, false, false)) return 27;
        if (gate.poll(343'000u, healthy(343'000u), false, false).remaining_ms != 30'000u)
            return 28;
        gate.cancel();
        if (gate.pending().token != 0u
            || gate.poll(500'000u, healthy(500'000u), false, false).ready()
            || gate.consume(3u, 500'000u, healthy(500'000u), false, false)) return 29;

        if (!gate.queue({4u, WorkKind::preview}, 600'000u)) return 30;
        (void)gate.poll(600'000u, healthy(600'000u), false, false);
        if (gate.poll(500'000u, healthy(500'000u), false, false).reason != Reason::clock_changed)
            return 31;
        if (gate.poll(500'001u, healthy(500'001u), false, false).remaining_ms != 30'000u)
            return 32;
        if (!gate.consume(4u, 530'001u, healthy(530'001u), false, false)) return 33;
        if (gate.queue({3u, WorkKind::model}, 530'001u)
            || gate.queue({1u, WorkKind::compiler}, 530'001u)) return 36;

        Policy invalid{};
        invalid.maximum_cpu_busy_fraction = (std::numeric_limits<double>::infinity)();
        Controller invalid_gate{invalid};
        if (invalid_gate.queue({1u, WorkKind::model}, 0u)
            || invalid_gate.poll(0u, healthy(0u), false, false).reason != Reason::invalid_policy)
            return 34;
        return 0;
    }
}

int main() { return run_contract(); }
