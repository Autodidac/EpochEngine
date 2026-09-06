// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

export module platform.work_admission;

// This controller schedules admission, not work. It creates no task, window,
// process, model request, listener or approval, and never waits a thread.
export namespace epochengine::platform::work_admission
{
    enum class WorkKind : std::uint8_t { model, compiler, validation, preview };

    struct Request final
    {
        std::uint64_t token{};
        WorkKind kind{WorkKind::model};
        [[nodiscard]] constexpr bool operator==(const Request&) const noexcept = default;
    };

    struct ResourceSample final
    {
        std::uint64_t captured_at_ms{};
        std::uint64_t total_memory_bytes{};
        std::uint64_t available_memory_bytes{};
        double cpu_busy_fraction{};
        bool memory_valid{};
        bool cpu_valid{};
    };

    struct Policy final
    {
        std::uint64_t cooldown_ms{30'000u};
        std::uint64_t maximum_sample_age_ms{5'000u};
        std::uint64_t minimum_available_memory_bytes{2ull * 1024ull * 1024ull * 1024ull};
        double minimum_available_memory_fraction{0.05};
        double maximum_cpu_busy_fraction{0.90};

        [[nodiscard]] bool valid() const noexcept
        {
            return cooldown_ms > 0u && maximum_sample_age_ms > 0u
                && minimum_available_memory_bytes > 0u
                && std::isfinite(minimum_available_memory_fraction)
                && minimum_available_memory_fraction >= 0.0
                && minimum_available_memory_fraction < 1.0
                && std::isfinite(maximum_cpu_busy_fraction)
                && maximum_cpu_busy_fraction > 0.0
                && maximum_cpu_busy_fraction <= 1.0;
        }
    };

    enum class Reason : std::uint8_t
    {
        idle,
        cooling_down,
        awaiting_choice,
        other_work_active,
        resources_unavailable,
        resources_stale,
        memory_pressure,
        cpu_pressure,
        invalid_policy,
        clock_changed,
        ready
    };

    [[nodiscard]] constexpr std::string_view reason_message(Reason reason) noexcept
    {
        switch (reason)
        {
        case Reason::idle: return "No heavyweight operation is queued.";
        case Reason::cooling_down: return "Giving the computer time to settle before the next operation.";
        case Reason::awaiting_choice: return "Waiting for Keep Current or Choose Candidate; no new AI or build work will start.";
        case Reason::other_work_active: return "Waiting for the previous heavyweight operation to finish.";
        case Reason::resources_unavailable: return "Host RAM or CPU measurements are unavailable; checking again without starting work.";
        case Reason::resources_stale: return "Host resource measurements are stale; waiting for a fresh sample.";
        case Reason::memory_pressure: return "Waiting for enough available host RAM before starting more work.";
        case Reason::cpu_pressure: return "Waiting for host CPU load to settle before starting more work.";
        case Reason::invalid_policy: return "The resource admission policy is invalid; no work was started.";
        case Reason::clock_changed: return "The timing reference changed; restarting the nonblocking cooldown.";
        case Reason::ready: return "Resource checks and cooldown passed; ready to start.";
        }
        return "Unknown resource admission state.";
    }

    struct Decision final
    {
        Reason reason{Reason::idle};
        std::uint64_t remaining_ms{};
        [[nodiscard]] constexpr bool ready() const noexcept { return reason == Reason::ready; }
    };

    [[nodiscard]] std::uint64_t now_milliseconds() noexcept;
    // Cached and synchronized. CPU requires two actual samples, at least one
    // second apart. No GPU/VRAM health is inferred from host memory or CPU.
    [[nodiscard]] ResourceSample sample_host_resources() noexcept;

    class Controller final
    {
    public:
        explicit Controller(Policy policy = {}) noexcept : policy_(policy) {}

        [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
        [[nodiscard]] Request pending() const noexcept { return pending_; }

        // Tokens are monotonic within this controller's lifetime. Retired
        // generations remain retired after any number of later operations.
        [[nodiscard]] bool queue(Request request, std::uint64_t now) noexcept
        {
            if (!policy_.valid() || request.token == 0u
                || request.token <= retired_token_ || !valid_kind(request.kind))
                return false;
            if (pending_.token != 0u)
                return pending_ == request;
            pending_ = request;
            not_before_ = (std::max)(next_allowed_, after_cooldown(now));
            healthy_since_.reset();
            return true;
        }

        [[nodiscard]] Decision poll(
            std::uint64_t now, const ResourceSample& sample,
            bool choice_pending, bool heavy_work_active) noexcept
        {
            if (!policy_.valid()) return {Reason::invalid_policy};
            if (pending_.token == 0u) return {};
            if (last_poll_.has_value() && now < *last_poll_)
            {
                last_poll_ = now;
                not_before_ = after_cooldown(now);
                next_allowed_ = not_before_;
                healthy_since_.reset();
                return {Reason::clock_changed, policy_.cooldown_ms};
            }
            last_poll_ = now;
            if (choice_pending || heavy_work_active)
            {
                was_blocked_ = true;
                healthy_since_.reset();
                return {choice_pending ? Reason::awaiting_choice : Reason::other_work_active};
            }
            if (was_blocked_)
            {
                was_blocked_ = false;
                not_before_ = (std::max)(not_before_, after_cooldown(now));
            }

            const Reason resource_reason = assess(sample, now);
            if (resource_reason != Reason::ready)
            {
                healthy_since_.reset();
                return {resource_reason};
            }
            if (!healthy_since_.has_value()) healthy_since_ = now;
            const auto ready_at = (std::max)(not_before_, after_cooldown(*healthy_since_));
            if (now < ready_at)
                return {Reason::cooling_down, ready_at - now};
            return {Reason::ready};
        }

        // Re-evaluate at consumption so a stale ready decision cannot launch
        // after the user opens comparison, resources change, or another actor starts.
        [[nodiscard]] bool consume(
            std::uint64_t token, std::uint64_t now, const ResourceSample& sample,
            bool choice_pending, bool heavy_work_active) noexcept
        {
            if (token == 0u || token != pending_.token
                || !poll(now, sample, choice_pending, heavy_work_active).ready())
                return false;
            retired_token_ = pending_.token;
            pending_ = {};
            next_allowed_ = after_cooldown(now);
            healthy_since_.reset();
            return true;
        }

        void note_finished(std::uint64_t now) noexcept
        {
            next_allowed_ = after_cooldown(now);
            not_before_ = (std::max)(not_before_, next_allowed_);
            healthy_since_.reset();
        }

        void cancel() noexcept
        {
            if (pending_.token != 0u) retired_token_ = pending_.token;
            pending_ = {};
            healthy_since_.reset();
            was_blocked_ = false;
        }

    private:
        [[nodiscard]] static constexpr bool valid_kind(WorkKind kind) noexcept
        {
            switch (kind)
            {
            case WorkKind::model: case WorkKind::compiler:
            case WorkKind::validation: case WorkKind::preview: return true;
            }
            return false;
        }

        [[nodiscard]] std::uint64_t after_cooldown(std::uint64_t now) const noexcept
        {
            const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
            return policy_.cooldown_ms > maximum - now
                ? maximum : now + policy_.cooldown_ms;
        }

        [[nodiscard]] Reason assess(const ResourceSample& sample, std::uint64_t now) const noexcept
        {
            if (!sample.memory_valid || !sample.cpu_valid
                || sample.total_memory_bytes == 0u
                || sample.available_memory_bytes > sample.total_memory_bytes
                || !std::isfinite(sample.cpu_busy_fraction)
                || sample.cpu_busy_fraction < 0.0 || sample.cpu_busy_fraction > 1.0)
                return Reason::resources_unavailable;
            if (sample.captured_at_ms > now
                || now - sample.captured_at_ms > policy_.maximum_sample_age_ms)
                return Reason::resources_stale;
            const double available_fraction = static_cast<double>(sample.available_memory_bytes)
                / static_cast<double>(sample.total_memory_bytes);
            if (sample.available_memory_bytes < policy_.minimum_available_memory_bytes
                || available_fraction < policy_.minimum_available_memory_fraction)
                return Reason::memory_pressure;
            if (sample.cpu_busy_fraction > policy_.maximum_cpu_busy_fraction)
                return Reason::cpu_pressure;
            return Reason::ready;
        }

        Policy policy_{};
        Request pending_{};
        std::uint64_t retired_token_{};
        std::uint64_t not_before_{};
        std::uint64_t next_allowed_{};
        std::optional<std::uint64_t> healthy_since_{};
        std::optional<std::uint64_t> last_poll_{};
        bool was_blocked_{};
    };
}
