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
        Controller quick{};
        if (quick.policy().cooldown_ms != 6'000u
            || !quick.queue({1u, WorkKind::model}, 0u)
            || quick.poll(0u, healthy(0u), false, false).remaining_ms != 6'000u
            || quick.poll(5'999u, healthy(5'999u), false, false).ready()
            || !quick.consume(1u, 6'000u, healthy(6'000u), false, false)) return 37;
        // Also exercise a non-default configurable policy.
        Controller gate{Policy{.cooldown_ms = 30'000u}};
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

        Controller approved{};
        auto lowMemory = healthy(0u);
        lowMemory.available_memory_bytes = gib;
        if (approved.approve_memory_pressure(1u)
            || !approved.queue({1u, WorkKind::model}, 0u)
            || approved.poll(0u, lowMemory, false, false).reason != Reason::memory_pressure
            || approved.approve_memory_pressure(2u)
            || !approved.approve_memory_pressure(1u)
            || !approved.memory_pressure_approved()) return 38;
        if (approved.poll(0u, lowMemory, false, false).remaining_ms != 6'000u)
            return 39;
        lowMemory.captured_at_ms = 6'000u;
        lowMemory.cpu_busy_fraction = 0.95;
        if (approved.poll(6'000u, lowMemory, false, false).reason != Reason::cpu_pressure)
            return 40;
        lowMemory.cpu_busy_fraction = 0.20;
        lowMemory.memory_valid = false;
        if (approved.poll(6'000u, lowMemory, false, false).reason != Reason::resources_unavailable)
            return 41;
        lowMemory.memory_valid = true;
        lowMemory.captured_at_ms = 0u;
        if (approved.poll(6'000u, lowMemory, false, false).reason != Reason::resources_stale)
            return 46;
        lowMemory.captured_at_ms = 6'000u;
        if (!approved.queue({1u, WorkKind::model}, 6'000u)
            || !approved.memory_pressure_approved()) return 47;
        if (approved.poll(6'000u, lowMemory, true, false).reason != Reason::awaiting_choice
            || approved.poll(6'000u, lowMemory, false, true).reason != Reason::other_work_active
            || approved.poll(6'000u, lowMemory, false, false).remaining_ms != 6'000u)
            return 42;
        lowMemory.captured_at_ms = 12'000u;
        if (!approved.consume(1u, 12'000u, lowMemory, false, false)
            || approved.memory_pressure_approved()
            || approved.approve_memory_pressure(1u)
            || !approved.queue({2u, WorkKind::model}, 12'000u)
            || approved.memory_pressure_approved()
            || approved.poll(12'000u, lowMemory, false, false).reason != Reason::memory_pressure)
            return 43;
        if (!approved.approve_memory_pressure(2u)) return 44;
        approved.cancel();
        if (approved.memory_pressure_approved()
            || !approved.queue({3u, WorkKind::compiler}, 12'000u)
            || approved.approve_memory_pressure(3u)
            || approved.poll(12'000u, lowMemory, false, false).reason != Reason::memory_pressure)
            return 45;
        Controller fractionApproved{};
        lowMemory.total_memory_bytes = 256u * gib;
        lowMemory.available_memory_bytes = 3u * gib;
        lowMemory.captured_at_ms = 0u;
        if (!fractionApproved.queue({1u, WorkKind::model}, 0u)
            || fractionApproved.poll(0u, lowMemory, false, false).reason != Reason::memory_pressure
            || !fractionApproved.approve_memory_pressure(1u)
            || fractionApproved.poll(0u, lowMemory, false, false).remaining_ms != 6'000u)
            return 48;
        lowMemory.captured_at_ms = 6'000u;
        if (!fractionApproved.consume(1u, 6'000u, lowMemory, false, false)) return 49;
        ModelMemoryApproval sessionApproval{};
        ModelMemoryScope scope{1u, "selected-model", "http://127.0.0.1:14321/v1", "http"};
        Controller sessionGate{};
        lowMemory = healthy(0u);
        lowMemory.available_memory_bytes = gib;
        if (sessionApproval.approve(sessionGate, 1u, scope)
            || !sessionGate.queue({1u, WorkKind::model}, 0u)
            || sessionApproval.apply(sessionGate, scope)
            || sessionApproval.approve(sessionGate, 2u, scope)
            || sessionApproval.approve(sessionGate, 1u, {})
            || !sessionApproval.approve(sessionGate, 1u, scope)) return 50;
        (void)sessionGate.poll(0u, lowMemory, false, false);
        lowMemory.captured_at_ms = 6'000u;
        if (!sessionGate.consume(1u, 6'000u, lowMemory, false, false)
            || !sessionGate.queue({2u, WorkKind::model}, 6'000u)
            || sessionGate.memory_pressure_approved()
            || !sessionApproval.apply(sessionGate, scope)
            || !sessionGate.memory_pressure_approved()) return 51;
        sessionGate.cancel();
        std::uint64_t nextToken = 3u;
        for (const auto kind : {WorkKind::compiler, WorkKind::validation, WorkKind::preview})
        {
            if (!sessionGate.queue({nextToken++, kind}, 6'000u)
                || sessionApproval.apply(sessionGate, scope)
                || sessionApproval.approve(sessionGate, sessionGate.pending().token, scope)
                || sessionGate.memory_pressure_approved()
                || sessionGate.poll(6'000u, lowMemory, false, false).reason != Reason::memory_pressure)
                return 52;
            sessionGate.cancel();
        }
        // Changing any scope component revokes consent, even when changing back.
        for (unsigned field = 0u; field < 4u; ++field)
        {
            if (!sessionGate.queue({10u + field, WorkKind::model}, 6'000u)
                || !sessionApproval.approve(sessionGate, sessionGate.pending().token, scope)) return 53;
            auto changed = scope;
            if (field == 0u) ++changed.session;
            if (field == 1u) changed.model += "-other";
            if (field == 2u) changed.endpoint += "/other";
            if (field == 3u) changed.transport = "external-mcp";
            if (sessionApproval.apply(sessionGate, changed)
                || sessionGate.memory_pressure_approved()
                || sessionApproval.apply(sessionGate, scope)) return 54;
            sessionGate.cancel();
        }
        if (!sessionGate.queue({20u, WorkKind::model}, 6'000u)
            || !sessionApproval.approve(sessionGate, 20u, scope)) return 55;
        sessionApproval.revoke(sessionGate);
        if (sessionGate.memory_pressure_approved() || sessionApproval.matches(scope)
            || sessionApproval.apply(sessionGate, scope)) return 56;
        return 0;
    }
}

int main() { return run_contract(); }
