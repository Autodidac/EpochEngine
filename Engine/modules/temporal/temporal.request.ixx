// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

export module temporal.request;

export namespace epochengine::temporal
{
    struct GlobalTime
    {
        double seconds{};
        friend constexpr bool operator==(GlobalTime, GlobalTime) noexcept = default;
        friend constexpr auto operator<=>(GlobalTime, GlobalTime) noexcept = default;
    };

    struct SampleTime
    {
        double seconds{};
        friend constexpr bool operator==(SampleTime, SampleTime) noexcept = default;
        friend constexpr auto operator<=>(SampleTime, SampleTime) noexcept = default;
    };

    struct Duration
    {
        double seconds{};
        friend constexpr bool operator==(Duration, Duration) noexcept = default;
        friend constexpr auto operator<=>(Duration, Duration) noexcept = default;
    };

    struct TemporalRate
    {
        double sample_seconds_per_global_second{ 1.0 };
        friend constexpr bool operator==(TemporalRate, TemporalRate) noexcept = default;
    };

    struct TemporalAnchor
    {
        GlobalTime global{};
        SampleTime sample{};
    };

    enum class TemporalDirection : std::uint8_t
    {
        Reverse = 0,
        Frozen,
        Forward
    };

    enum class TruthClass : std::uint8_t
    {
        Exact = 0,
        ErrorBounded,
        VisualOnly,
        Disposable
    };

    enum class ObservationMode : std::uint8_t
    {
        Exact = 0,
        Nearest,
        Bracket
    };

    enum class ObservationStatus : std::uint8_t
    {
        Ready = 0,
        Empty,
        InvalidRequest,
        ExactSampleMissing,
        OutsideHistory,
        StaleHandle
    };

    enum class AppendStatus : std::uint8_t
    {
        Inserted = 0,
        Replaced,
        RejectedInvalid,
        RejectedOlderSequence,
        RejectedOutsideRetention,
        StaleHandle
    };

    struct TemporalMapping
    {
        TemporalAnchor anchor{};
        TemporalRate rate{};
    };

    struct SubjectHandle
    {
        std::uint32_t index{ std::numeric_limits<std::uint32_t>::max() };
        std::uint64_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != std::numeric_limits<std::uint32_t>::max()
                && generation != 0u;
        }

        friend constexpr bool operator==(SubjectHandle, SubjectHandle) noexcept = default;
    };

    struct ObservationRequest
    {
        SubjectHandle subject{};
        TemporalMapping mapping{};
        GlobalTime global_time{};
        ObservationMode mode{ ObservationMode::Bracket };
        Duration maximum_distance{ std::numeric_limits<double>::infinity() };
        bool allow_boundary_clamp{ false };
    };

    struct ObservationPlan
    {
        SubjectHandle subject{};
        SampleTime requested_time{};
        SampleTime lower_time{};
        SampleTime upper_time{};
        double interpolation_alpha{};
        bool exact{};
        bool clamped{};
        bool nearest{};
        bool requires_reconstruction{};
        TruthClass truth{ TruthClass::Exact };
        ObservationStatus status{ ObservationStatus::InvalidRequest };
    };

    struct StoreMetrics
    {
        std::size_t live_subjects{};
        std::size_t retained_samples{};
        std::size_t sample_capacity{};
        std::uint64_t append_count{};
        std::uint64_t replacement_count{};
        std::uint64_t eviction_count{};
        std::uint64_t rejected_append_count{};
        std::uint64_t request_count{};
        std::uint64_t exact_hit_count{};
        std::uint64_t nearest_hit_count{};
        std::uint64_t bracket_hit_count{};
        std::uint64_t clamp_count{};
        std::uint64_t miss_count{};
    };

    [[nodiscard]] inline bool finite(GlobalTime value) noexcept
    {
        return std::isfinite(value.seconds);
    }

    [[nodiscard]] inline bool finite(SampleTime value) noexcept
    {
        return std::isfinite(value.seconds);
    }

    [[nodiscard]] inline bool finite(Duration value) noexcept
    {
        return std::isfinite(value.seconds);
    }

    [[nodiscard]] inline bool finite(TemporalRate value) noexcept
    {
        return std::isfinite(value.sample_seconds_per_global_second);
    }

    [[nodiscard]] inline bool valid(const TemporalMapping& mapping) noexcept
    {
        return finite(mapping.anchor.global)
            && finite(mapping.anchor.sample)
            && finite(mapping.rate)
            && std::abs(mapping.rate.sample_seconds_per_global_second) <= 64.0;
    }

    inline constexpr double kSampleTimeToleranceSeconds = 1.0e-9;

    [[nodiscard]] inline bool same_sample_time(SampleTime lhs, SampleTime rhs) noexcept
    {
        return std::abs(lhs.seconds - rhs.seconds) <= kSampleTimeToleranceSeconds;
    }

    [[nodiscard]] constexpr TemporalDirection direction(TemporalRate rate) noexcept
    {
        if (rate.sample_seconds_per_global_second < 0.0)
            return TemporalDirection::Reverse;
        if (rate.sample_seconds_per_global_second > 0.0)
            return TemporalDirection::Forward;
        return TemporalDirection::Frozen;
    }

    [[nodiscard]] inline SampleTime sample_time(
        const TemporalMapping& mapping,
        GlobalTime now) noexcept
    {
        if (!valid(mapping) || !finite(now))
            return { std::numeric_limits<double>::quiet_NaN() };

        const double elapsed = now.seconds - mapping.anchor.global.seconds;
        return {
            mapping.anchor.sample.seconds
                + elapsed * mapping.rate.sample_seconds_per_global_second
        };
    }

    [[nodiscard]] inline std::optional<GlobalTime> global_time(
        const TemporalMapping& mapping,
        SampleTime sample) noexcept
    {
        if (!valid(mapping)
            || !finite(sample)
            || mapping.rate.sample_seconds_per_global_second == 0.0)
        {
            return std::nullopt;
        }

        const GlobalTime result{
            mapping.anchor.global.seconds
                + (sample.seconds - mapping.anchor.sample.seconds)
                / mapping.rate.sample_seconds_per_global_second
        };
        return finite(result) ? std::optional<GlobalTime>{ result } : std::nullopt;
    }

    [[nodiscard]] inline ObservationRequest make_observation_request(
        SubjectHandle subject,
        TemporalMapping mapping,
        GlobalTime global_time_value,
        ObservationMode mode = ObservationMode::Bracket,
        Duration maximum_distance = { std::numeric_limits<double>::infinity() },
        bool allow_boundary_clamp = false) noexcept
    {
        return {
            .subject = subject,
            .mapping = mapping,
            .global_time = global_time_value,
            .mode = mode,
            .maximum_distance = maximum_distance,
            .allow_boundary_clamp = allow_boundary_clamp
        };
    }

    [[nodiscard]] inline bool valid(const ObservationRequest& request) noexcept
    {
        return request.subject.valid()
            && valid(request.mapping)
            && finite(request.global_time)
            && !std::isnan(request.maximum_distance.seconds)
            && request.maximum_distance.seconds >= 0.0;
    }

    template <std::copyable State>
    struct HistorySample
    {
        SampleTime time{};
        std::uint64_t sequence{};
        State state{};
    };

    template <std::copyable State>
    struct Observation
    {
        ObservationPlan plan{};
        std::optional<HistorySample<State>> lower{};
        std::optional<HistorySample<State>> upper{};

        [[nodiscard]] bool ready() const noexcept
        {
            return plan.status == ObservationStatus::Ready;
        }
    };

    template <std::copyable State>
    class HistorySeries
    {
    public:
        explicit HistorySeries(
            std::size_t capacity = 256u,
            TruthClass truth = TruthClass::Exact)
            : capacity_{ (std::max)(std::size_t{ 2u }, capacity) },
              truth_{ truth }
        {
            samples_.reserve(capacity_);
        }

        [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
        [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
        [[nodiscard]] bool empty() const noexcept { return samples_.empty(); }
        [[nodiscard]] TruthClass truth_class() const noexcept { return truth_; }
        // Borrowed views are invalidated by mutation. Use copy_samples for retained work.
        [[nodiscard]] std::span<const HistorySample<State>> samples() const noexcept { return samples_; }
        [[nodiscard]] std::vector<HistorySample<State>> copy_samples() const { return samples_; }

        void set_capacity(std::size_t value)
        {
            capacity_ = (std::max)(std::size_t{ 2u }, value);
            trim_to_capacity();
        }

        [[nodiscard]] AppendStatus append(HistorySample<State> sample)
        {
            if (!finite(sample.time) || sample.sequence == 0u)
            {
                ++rejected_append_count_;
                return AppendStatus::RejectedInvalid;
            }

            const auto position = std::lower_bound(
                samples_.begin(),
                samples_.end(),
                sample.time,
                [](const HistorySample<State>& candidate, SampleTime time)
                {
                    return candidate.time < time;
                });

            auto matching = samples_.end();
            if (position != samples_.end() && same_sample_time(position->time, sample.time))
                matching = position;
            else if (position != samples_.begin())
            {
                const auto previous = std::prev(position);
                if (same_sample_time(previous->time, sample.time))
                    matching = previous;
            }

            if (matching != samples_.end())
            {
                if (sample.sequence < matching->sequence)
                {
                    ++rejected_append_count_;
                    return AppendStatus::RejectedOlderSequence;
                }
                sample.time = matching->time;
                *matching = std::move(sample);
                ++replacement_count_;
                return AppendStatus::Replaced;
            }

            if (samples_.size() == capacity_
                && position == samples_.begin()
                && sample.time < samples_.front().time)
            {
                ++rejected_append_count_;
                return AppendStatus::RejectedOutsideRetention;
            }

            samples_.insert(position, std::move(sample));
            ++append_count_;
            trim_to_capacity();
            return AppendStatus::Inserted;
        }

        [[nodiscard]] Observation<State> observe(const ObservationRequest& request) const
        {
            Observation<State> result{};
            result.plan.subject = request.subject;
            result.plan.truth = truth_;
            result.plan.status = ObservationStatus::InvalidRequest;

            if (!valid(request))
                return result;

            const SampleTime requested = sample_time(request.mapping, request.global_time);
            result.plan.requested_time = requested;
            if (!finite(requested))
                return result;

            if (samples_.empty())
            {
                result.plan.status = ObservationStatus::Empty;
                return result;
            }

            const auto upper = std::lower_bound(
                samples_.begin(),
                samples_.end(),
                requested,
                [](const HistorySample<State>& candidate, SampleTime time)
                {
                    return candidate.time < time;
                });

            auto exact = samples_.end();
            if (upper != samples_.end() && same_sample_time(upper->time, requested))
                exact = upper;
            else if (upper != samples_.begin())
            {
                const auto previous = std::prev(upper);
                if (same_sample_time(previous->time, requested))
                    exact = previous;
            }

            if (exact != samples_.end())
            {
                result.lower = *exact;
                result.upper = *exact;
                result.plan.lower_time = exact->time;
                result.plan.upper_time = exact->time;
                result.plan.exact = true;
                result.plan.status = ObservationStatus::Ready;
                return result;
            }

            if (request.mode == ObservationMode::Exact)
            {
                result.plan.status = ObservationStatus::ExactSampleMissing;
                result.plan.requires_reconstruction = true;
                return result;
            }

            const bool before_first = upper == samples_.begin();
            const bool after_last = upper == samples_.end();
            if (before_first || after_last)
            {
                if (!request.allow_boundary_clamp)
                {
                    result.plan.status = ObservationStatus::OutsideHistory;
                    result.plan.requires_reconstruction = true;
                    return result;
                }

                const auto& boundary = before_first ? samples_.front() : samples_.back();
                const double distance = std::abs(boundary.time.seconds - requested.seconds);
                if (distance > request.maximum_distance.seconds)
                {
                    result.plan.status = ObservationStatus::OutsideHistory;
                    result.plan.requires_reconstruction = true;
                    return result;
                }

                result.lower = boundary;
                result.upper = boundary;
                result.plan.lower_time = boundary.time;
                result.plan.upper_time = boundary.time;
                result.plan.clamped = true;
                result.plan.requires_reconstruction = truth_ == TruthClass::Exact;
                result.plan.status = ObservationStatus::Ready;
                return result;
            }

            const auto lower = std::prev(upper);
            const double lower_distance = requested.seconds - lower->time.seconds;
            const double upper_distance = upper->time.seconds - requested.seconds;
            const double required_distance = request.mode == ObservationMode::Nearest
                ? (std::min)(lower_distance, upper_distance)
                : (std::max)(lower_distance, upper_distance);
            if (required_distance > request.maximum_distance.seconds)
            {
                result.plan.status = ObservationStatus::OutsideHistory;
                result.plan.requires_reconstruction = true;
                return result;
            }

            if (request.mode == ObservationMode::Nearest)
            {
                const auto& nearest = lower_distance <= upper_distance ? *lower : *upper;
                result.lower = nearest;
                result.upper = nearest;
                result.plan.lower_time = nearest.time;
                result.plan.upper_time = nearest.time;
                result.plan.nearest = true;
                result.plan.requires_reconstruction = truth_ == TruthClass::Exact;
                result.plan.status = ObservationStatus::Ready;
                return result;
            }

            result.lower = *lower;
            result.upper = *upper;
            result.plan.lower_time = lower->time;
            result.plan.upper_time = upper->time;
            const double span = upper->time.seconds - lower->time.seconds;
            result.plan.interpolation_alpha = span > 0.0 ? lower_distance / span : 0.0;
            result.plan.requires_reconstruction = truth_ == TruthClass::Exact;
            result.plan.status = ObservationStatus::Ready;
            return result;
        }

        [[nodiscard]] std::uint64_t append_count() const noexcept { return append_count_; }
        [[nodiscard]] std::uint64_t replacement_count() const noexcept { return replacement_count_; }
        [[nodiscard]] std::uint64_t eviction_count() const noexcept { return eviction_count_; }
        [[nodiscard]] std::uint64_t rejected_append_count() const noexcept { return rejected_append_count_; }

    private:
        void trim_to_capacity()
        {
            if (samples_.size() <= capacity_)
                return;
            const std::size_t remove_count = samples_.size() - capacity_;
            samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(remove_count));
            eviction_count_ += remove_count;
        }

        std::size_t capacity_{};
        TruthClass truth_{ TruthClass::Exact };
        std::vector<HistorySample<State>> samples_{};
        std::uint64_t append_count_{};
        std::uint64_t replacement_count_{};
        std::uint64_t eviction_count_{};
        std::uint64_t rejected_append_count_{};
    };

    template <std::copyable State>
    class RequestDrivenHistory
    {
    public:
        explicit RequestDrivenHistory(std::size_t default_capacity = 256u)
            : default_capacity_{ (std::max)(std::size_t{ 2u }, default_capacity) }
        {
        }

        [[nodiscard]] SubjectHandle create_subject(
            TruthClass truth = TruthClass::Exact,
            std::size_t capacity = 0u)
        {
            std::uint32_t index = 0u;
            for (; static_cast<std::size_t>(index) < slots_.size(); ++index)
            {
                Slot& candidate = slots_[index];
                if (!candidate.live
                    && !candidate.retired
                    && candidate.generation != std::numeric_limits<std::uint64_t>::max())
                {
                    break;
                }
                if (!candidate.live
                    && candidate.generation == std::numeric_limits<std::uint64_t>::max())
                {
                    candidate.retired = true;
                }
            }

            if (static_cast<std::size_t>(index) == slots_.size())
            {
                if (slots_.size() >= std::numeric_limits<std::uint32_t>::max())
                    return {};
                slots_.push_back({});
            }

            Slot& slot = slots_[index];
            ++slot.generation;
            slot.live = true;
            slot.series.emplace(capacity == 0u ? default_capacity_ : capacity, truth);
            return { index, slot.generation };
        }

        [[nodiscard]] bool destroy_subject(SubjectHandle handle) noexcept
        {
            Slot* slot = resolve(handle);
            if (!slot)
                return false;
            retired_append_count_ += slot->series->append_count();
            retired_replacement_count_ += slot->series->replacement_count();
            retired_eviction_count_ += slot->series->eviction_count();
            retired_rejected_append_count_ += slot->series->rejected_append_count();
            slot->series.reset();
            slot->live = false;
            slot->retired = slot->generation == std::numeric_limits<std::uint64_t>::max();
            return true;
        }

        [[nodiscard]] AppendStatus append(SubjectHandle handle, HistorySample<State> sample)
        {
            Slot* slot = resolve(handle);
            if (!slot || !slot->series)
            {
                ++stale_append_count_;
                return AppendStatus::StaleHandle;
            }
            return slot->series->append(std::move(sample));
        }

        [[nodiscard]] Observation<State> observe(const ObservationRequest& request)
        {
            ++request_count_;
            Slot* slot = resolve(request.subject);
            if (!slot || !slot->series)
            {
                ++miss_count_;
                Observation<State> result{};
                result.plan.subject = request.subject;
                result.plan.status = ObservationStatus::StaleHandle;
                return result;
            }

            auto result = slot->series->observe(request);
            if (!result.ready())
            {
                ++miss_count_;
                return result;
            }

            if (result.plan.exact)
                ++exact_hit_count_;
            else if (result.plan.nearest)
                ++nearest_hit_count_;
            else if (result.plan.clamped)
                ++clamp_count_;
            else
                ++bracket_hit_count_;
            return result;
        }

        [[nodiscard]] StoreMetrics metrics() const noexcept
        {
            StoreMetrics result{};
            result.request_count = request_count_;
            result.exact_hit_count = exact_hit_count_;
            result.nearest_hit_count = nearest_hit_count_;
            result.bracket_hit_count = bracket_hit_count_;
            result.clamp_count = clamp_count_;
            result.miss_count = miss_count_;
            result.append_count = retired_append_count_;
            result.replacement_count = retired_replacement_count_;
            result.eviction_count = retired_eviction_count_;
            result.rejected_append_count = retired_rejected_append_count_ + stale_append_count_;

            for (const Slot& slot : slots_)
            {
                if (!slot.live || !slot.series)
                    continue;
                ++result.live_subjects;
                result.retained_samples += slot.series->size();
                result.sample_capacity += slot.series->capacity();
                result.append_count += slot.series->append_count();
                result.replacement_count += slot.series->replacement_count();
                result.eviction_count += slot.series->eviction_count();
                result.rejected_append_count += slot.series->rejected_append_count();
            }
            return result;
        }

    private:
        struct Slot
        {
            std::uint64_t generation{};
            bool live{};
            bool retired{};
            std::optional<HistorySeries<State>> series{};
        };

        [[nodiscard]] Slot* resolve(SubjectHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
                return nullptr;
            Slot& slot = slots_[handle.index];
            return slot.live && slot.generation == handle.generation ? &slot : nullptr;
        }

        std::size_t default_capacity_{};
        std::vector<Slot> slots_{};
        std::uint64_t request_count_{};
        std::uint64_t exact_hit_count_{};
        std::uint64_t nearest_hit_count_{};
        std::uint64_t bracket_hit_count_{};
        std::uint64_t clamp_count_{};
        std::uint64_t miss_count_{};
        std::uint64_t stale_append_count_{};
        std::uint64_t retired_append_count_{};
        std::uint64_t retired_replacement_count_{};
        std::uint64_t retired_eviction_count_{};
        std::uint64_t retired_rejected_append_count_{};
    };

    struct RequestContractResult
    {
        bool mapping{};
        bool reverse{};
        bool frozen{};
        bool exact{};
        bool bracket{};
        bool nearest{};
        bool clamped{};
        bool bounded{};
        bool stale_handle{};
        bool metrics{};

        [[nodiscard]] bool passed() const noexcept
        {
            return mapping
                && reverse
                && frozen
                && exact
                && bracket
                && nearest
                && clamped
                && bounded
                && stale_handle
                && metrics;
        }
    };

    [[nodiscard]] inline RequestContractResult run_request_contract()
    {
        struct State
        {
            int value{};
        };

        RequestContractResult result{};
        const TemporalMapping forward{
            .anchor = { .global = { 100.0 }, .sample = { 40.0 } },
            .rate = { 1.0 }
        };
        const TemporalMapping reverse{
            .anchor = { .global = { 100.0 }, .sample = { 40.0 } },
            .rate = { -1.0 }
        };
        const TemporalMapping frozen{
            .anchor = { .global = { 100.0 }, .sample = { 40.0 } },
            .rate = { 0.0 }
        };
        result.mapping = sample_time(forward, { 103.0 }) == SampleTime{ 43.0 };
        result.reverse = direction(reverse.rate) == TemporalDirection::Reverse
            && sample_time(reverse, { 103.0 }) == SampleTime{ 37.0 };
        result.frozen = direction(frozen.rate) == TemporalDirection::Frozen
            && sample_time(frozen, { 103.0 }) == SampleTime{ 40.0 }
            && !global_time(frozen, { 40.0 }).has_value();

        RequestDrivenHistory<State> history{ 3u };
        const SubjectHandle subject = history.create_subject(TruthClass::Exact, 3u);
        const bool initial_append =
            history.append(subject, { .time = { 10.0 }, .sequence = 1u, .state = { 10 } })
                == AppendStatus::Inserted
            && history.append(subject, { .time = { 20.0 }, .sequence = 2u, .state = { 20 } })
                == AppendStatus::Inserted
            && history.append(subject, { .time = { 30.0 }, .sequence = 3u, .state = { 30 } })
                == AppendStatus::Inserted;

        const TemporalMapping direct{
            .anchor = { .global = { 0.0 }, .sample = { 0.0 } },
            .rate = { 1.0 }
        };
        const auto exact_observation = history.observe(make_observation_request(
            subject, direct, { 20.0 }, ObservationMode::Exact));
        result.exact = exact_observation.ready()
            && exact_observation.plan.exact
            && exact_observation.lower
            && exact_observation.lower->state.value == 20;

        const auto bracket_observation = history.observe(make_observation_request(
            subject, direct, { 25.0 }, ObservationMode::Bracket));
        result.bracket = bracket_observation.ready()
            && bracket_observation.lower
            && bracket_observation.upper
            && bracket_observation.lower->state.value == 20
            && bracket_observation.upper->state.value == 30
            && std::abs(bracket_observation.plan.interpolation_alpha - 0.5) < 0.0001
            && bracket_observation.plan.requires_reconstruction;

        const auto nearest_observation = history.observe(make_observation_request(
            subject, direct, { 27.0 }, ObservationMode::Nearest));
        result.nearest = nearest_observation.ready()
            && nearest_observation.lower
            && nearest_observation.lower->state.value == 30
            && nearest_observation.plan.nearest
            && nearest_observation.plan.requires_reconstruction;

        const auto clamped_observation = history.observe(make_observation_request(
            subject, direct, { 5.0 }, ObservationMode::Nearest, { 5.0 }, true));
        result.clamped = clamped_observation.ready()
            && clamped_observation.plan.clamped
            && clamped_observation.plan.requires_reconstruction
            && clamped_observation.lower
            && clamped_observation.lower->state.value == 10;

        const auto strict_bracket = history.observe(make_observation_request(
            subject, direct, { 24.0 }, ObservationMode::Bracket, { 4.5 }));
        const bool bracket_bound_honored =
            strict_bracket.plan.status == ObservationStatus::OutsideHistory;

        const bool append_and_evict =
            history.append(subject, { .time = { 40.0 }, .sequence = 4u, .state = { 40 } })
                == AppendStatus::Inserted;
        const bool old_append_rejected =
            history.append(subject, { .time = { 5.0 }, .sequence = 5u, .state = { 5 } })
                == AppendStatus::RejectedOutsideRetention;
        const auto bounded_observation = history.observe(make_observation_request(
            subject, direct, { 10.0 }, ObservationMode::Nearest));
        result.bounded = initial_append
            && append_and_evict
            && old_append_rejected
            && bracket_bound_honored
            && bounded_observation.plan.status == ObservationStatus::OutsideHistory;

        const SubjectHandle stale = subject;
        result.stale_handle = history.destroy_subject(subject)
            && history.observe(make_observation_request(
                stale, direct, { 20.0 }, ObservationMode::Exact)).plan.status
                == ObservationStatus::StaleHandle;

        const auto metrics = history.metrics();
        result.metrics = metrics.eviction_count == 1u
            && metrics.append_count == 4u
            && metrics.rejected_append_count == 1u
            && metrics.request_count == 7u
            && metrics.exact_hit_count == 1u
            && metrics.nearest_hit_count == 1u
            && metrics.bracket_hit_count == 1u
            && metrics.clamp_count == 1u
            && metrics.miss_count == 3u;
        return result;
    }

    inline constexpr std::string_view kBenchmarkEvidenceRepository =
        "https://github.com/Autodidac/VoxelRayBenchmark";
}
