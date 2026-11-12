// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

    enum class GpuTelemetrySource : std::uint8_t { unavailable, nvidia_nvml };
    struct GpuResourceSample final
    {
        // Whole physical adapter0, including every application and driver use.
        // Never interpret these as this Editor's/process's allocation or load.
        GpuTelemetrySource source{GpuTelemetrySource::unavailable};
        std::uint32_t adapter_index{}, adapter_count{};
        std::array<char, 96u> adapter_name{};
        std::uint64_t dedicated_total_bytes{}, dedicated_used_bytes{};
        double busy_fraction{};
        bool busy_valid{}, memory_valid{};
        // Independent driver observations, never percentages of allocated VRAM.
        // Kernel activity above does not distinguish graphics from compute.
        double memory_activity_fraction{}, encoder_busy_fraction{}, decoder_busy_fraction{};
        std::uint32_t encoder_sample_period_us{}, decoder_sample_period_us{};
        bool memory_activity_valid{}, encoder_valid{}, decoder_valid{};
        // GPU query start/completion are independent of the CPU/RAM sample.
        std::uint64_t sampling_started_at_ms{}, captured_at_ms{};
    };

    struct ResourceSample final
    {
        std::uint64_t captured_at_ms{};
        std::uint64_t total_memory_bytes{};
        std::uint64_t available_memory_bytes{};
        double cpu_busy_fraction{};
        bool memory_valid{};
        bool cpu_valid{};
        GpuResourceSample gpu{};
    };

    struct Policy final
    {
        std::uint64_t cooldown_ms{6'000u};
        std::uint64_t maximum_sample_age_ms{5'000u};
        std::uint64_t minimum_available_memory_bytes{2ull * 1024ull * 1024ull * 1024ull};
        double minimum_available_memory_fraction{0.05};
        double maximum_cpu_busy_fraction{0.90};
        bool resource_checks{true};

        [[nodiscard]] bool valid() const noexcept
        {
            if (!resource_checks) return true;
            return maximum_sample_age_ms > 0u
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
        case Reason::ready: return "Ready to start the queued operation.";
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
    // second apart. This read never initializes or queries a GPU driver. GPU
    // observations expire after five seconds and are independent of admission.
    [[nodiscard]] ResourceSample sample_host_resources() noexcept;

    // Explicit session-owner lifecycle, never started by status reads. The optional
    // worker owns its driver library. OFF/unsupported builds create no worker.
    [[nodiscard]] bool start_gpu_telemetry() noexcept;
    // Requests cancellation; zero budget never waits. Budget is capped at 100ms.
    // False means a driver call/cleanup is still owned, not forcibly cancelled.
    // Its joinable worker and state remain process-owned until a later stop can
    // join it; process exit is the final boundary for an unreturning driver call.
    [[nodiscard]] bool stop_gpu_telemetry(std::uint32_t wait_budget_ms = 0u) noexcept;

    // One nonblocking RAM check for an already scheduled allocation. This is
    // not a reservation, a CPU throttle, or a replacement work controller.
    // Keep the policy's free-memory margin AFTER the estimated allocation.
    [[nodiscard]] inline Reason assess_memory_budget(const ResourceSample& sample,
        std::uint64_t now, std::uint64_t additional_bytes, Policy policy = {}) noexcept
    {
        if (!policy.valid()) return Reason::invalid_policy;
        if (!policy.resource_checks) return Reason::ready;
        if (!sample.memory_valid || sample.total_memory_bytes == 0u
            || sample.available_memory_bytes > sample.total_memory_bytes)
            return Reason::resources_unavailable;
        if (sample.captured_at_ms > now
            || now - sample.captured_at_ms > policy.maximum_sample_age_ms)
            return Reason::resources_stale;
        if (additional_bytes > sample.available_memory_bytes)
            return Reason::memory_pressure;
        const auto remaining = sample.available_memory_bytes - additional_bytes;
        if (remaining < policy.minimum_available_memory_bytes
            || static_cast<double>(remaining) / static_cast<double>(sample.total_memory_bytes)
                < policy.minimum_available_memory_fraction)
            return Reason::memory_pressure;
        return Reason::ready;
    }

    class Controller final
    {
    public:
        explicit Controller(Policy policy = {}) noexcept : policy_(policy) {}

        [[nodiscard]] const Policy& policy() const noexcept { return policy_; }
        [[nodiscard]] Request pending() const noexcept { return pending_; }

        // Explicit operator approval belongs to one queued model operation.
        // It cannot override worker/choice ownership, stale metrics or CPU load.
        [[nodiscard]] bool approve_memory_pressure(std::uint64_t token) noexcept
        {
            if (token == 0u || token != pending_.token
                || pending_.kind != WorkKind::model) return false;
            memory_pressure_approved_ = true;
            return true;
        }

        [[nodiscard]] bool memory_pressure_approved() const noexcept
        {
            return pending_.token != 0u && memory_pressure_approved_;
        }

        void revoke_memory_pressure() noexcept { memory_pressure_approved_ = false; }

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
            memory_pressure_approved_ = false;
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
            memory_pressure_approved_ = false;
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
            memory_pressure_approved_ = false;
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
            if (!policy_.resource_checks) return Reason::ready;
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
            if (!memory_pressure_approved_
                && (sample.available_memory_bytes < policy_.minimum_available_memory_bytes
                    || available_fraction < policy_.minimum_available_memory_fraction))
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
        bool memory_pressure_approved_{};
    };

    // In-memory operator consent, owned by one Editor context. Never serialize
    // it with a checkpoint or inherit it from another campaign/context.
    struct ModelMemoryScope final
    {
        std::uint64_t session{};
        std::string model{};
        std::string endpoint{};
        std::string transport{};
        [[nodiscard]] bool valid() const noexcept
        {
            return session != 0u && !model.empty() && !endpoint.empty() && !transport.empty();
        }
        [[nodiscard]] bool operator==(const ModelMemoryScope&) const noexcept = default;
    };

    class ModelMemoryApproval final
    {
    public:
        [[nodiscard]] bool approve(Controller& controller, std::uint64_t token,
            const ModelMemoryScope& scope)
        {
            if (!scope.valid() || token == 0u || controller.pending().token != token
                || controller.pending().kind != WorkKind::model) return false;
            auto retained = scope;
            if (!controller.approve_memory_pressure(token)) return false;
            scope_ = std::move(retained);
            return true;
        }

        [[nodiscard]] bool apply(Controller& controller, const ModelMemoryScope& scope)
        {
            if (!scope_) return false;
            if (!scope.valid() || *scope_ != scope)
            {
                revoke(controller);
                return false;
            }
            return controller.approve_memory_pressure(controller.pending().token);
        }

        [[nodiscard]] bool matches(const ModelMemoryScope& scope) const noexcept
        {
            return scope_ && scope.valid() && *scope_ == scope;
        }

        void revoke(Controller& controller) noexcept
        {
            scope_.reset();
            controller.revoke_memory_pressure();
        }

    private:
        std::optional<ModelMemoryScope> scope_{};
    };
}
