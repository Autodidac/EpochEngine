#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace epochengine::context
{
    enum class PassiveContextBackend : std::uint8_t
    {
        None = 0,
        OpenGL,
        SDL,
        SFML,
        RayLib,
        Vulkan,
        DirectX,
        Software,
        Custom,
        Noop
    };

    enum class PassiveContextSampleKind : std::uint8_t
    {
        Unknown = 0,
        SingleContextBackend,
        MultiContextBackend,
        DiagnosticGrid,
        RuntimeProbe
    };

    enum class PassiveSignalStatus : std::uint8_t
    {
        Unknown = 0,
        NotSampled,
        Healthy,
        Watch,
        Unstable
    };

    enum class PassiveEvidenceSource : std::uint8_t
    {
        None = 0,
        StaticContract,
        BuildMetadata,
        PassiveHostObservation,
        RuntimeProbe
    };

    enum class PassiveSampleRejection : std::uint8_t
    {
        None = 0,
        NotSingleContext,
        MultiContextSample,
        DiagnosticGridSample,
        RequiresRuntimeLaunch
    };

    struct PassiveContextEvidence
    {
        PassiveEvidenceSource source{ PassiveEvidenceSource::None };
        std::optional<double> fps{};
        std::optional<double> frame_time_ms{};
        std::optional<double> frame_time_jitter_ms{};
        PassiveSignalStatus stability{ PassiveSignalStatus::Unknown };
        std::optional<double> input_latency_ms{};
        PassiveSignalStatus input_latency_status{ PassiveSignalStatus::Unknown };
        bool runtime_launched{ false };
    };

    struct PassiveContextSample
    {
        PassiveContextBackend backend{ PassiveContextBackend::None };
        PassiveContextSampleKind kind{ PassiveContextSampleKind::Unknown };
        PassiveContextEvidence evidence{};
        std::string_view label{};
    };

    struct PassiveContextScore
    {
        bool accepted{ false };
        PassiveSampleRejection rejection{ PassiveSampleRejection::NotSingleContext };
        PassiveContextBackend backend{ PassiveContextBackend::None };
        PassiveContextEvidence evidence{};
        double score{ 0.0 };
    };

    struct PassiveBackendScoreSummary
    {
        PassiveContextBackend backend{ PassiveContextBackend::None };
        std::uint32_t sample_count{ 0 };
        double average_score{ 0.0 };
        double recent_score{ 0.0 };
        bool has_recent_score{ false };
    };

    struct PassiveContextRecommendation
    {
        bool available{ false };
        PassiveContextBackend backend{ PassiveContextBackend::None };
        std::uint32_t sample_count{ 0 };
        double average_score{ 0.0 };
        double recent_score{ 0.0 };
    };

    inline constexpr std::size_t passive_context_backend_count =
        static_cast<std::size_t>(PassiveContextBackend::Noop) + 1U;

    [[nodiscard]] constexpr std::size_t backend_index(PassiveContextBackend backend) noexcept
    {
        const auto index = static_cast<std::size_t>(backend);
        return index < passive_context_backend_count ? index : 0U;
    }

    [[nodiscard]] constexpr std::string_view backend_name(PassiveContextBackend backend) noexcept
    {
        switch (backend)
        {
        case PassiveContextBackend::OpenGL: return "OpenGL";
        case PassiveContextBackend::SDL: return "SDL";
        case PassiveContextBackend::SFML: return "SFML";
        case PassiveContextBackend::RayLib: return "Raylib";
        case PassiveContextBackend::Vulkan: return "Vulkan";
        case PassiveContextBackend::DirectX: return "DirectX";
        case PassiveContextBackend::Software: return "Software";
        case PassiveContextBackend::Custom: return "Custom";
        case PassiveContextBackend::Noop: return "Noop";
        case PassiveContextBackend::None:
        default: return "None";
        }
    }

    [[nodiscard]] constexpr std::string_view signal_status_name(PassiveSignalStatus status) noexcept
    {
        switch (status)
        {
        case PassiveSignalStatus::NotSampled: return "Not sampled";
        case PassiveSignalStatus::Healthy: return "Healthy";
        case PassiveSignalStatus::Watch: return "Watch";
        case PassiveSignalStatus::Unstable: return "Unstable";
        case PassiveSignalStatus::Unknown:
        default: return "Unknown";
        }
    }

    [[nodiscard]] constexpr std::string_view rejection_name(PassiveSampleRejection rejection) noexcept
    {
        switch (rejection)
        {
        case PassiveSampleRejection::None: return "Accepted";
        case PassiveSampleRejection::NotSingleContext: return "Not single-context";
        case PassiveSampleRejection::MultiContextSample: return "Multi-context sample";
        case PassiveSampleRejection::DiagnosticGridSample: return "Diagnostic grid sample";
        case PassiveSampleRejection::RequiresRuntimeLaunch: return "Requires runtime launch";
        default: return "Unknown rejection";
        }
    }

    [[nodiscard]] constexpr bool is_single_context_sample(PassiveContextSampleKind kind) noexcept
    {
        return kind == PassiveContextSampleKind::SingleContextBackend;
    }

    [[nodiscard]] constexpr PassiveSampleRejection rejection_for(const PassiveContextSample& sample) noexcept
    {
        if (sample.kind == PassiveContextSampleKind::MultiContextBackend)
            return PassiveSampleRejection::MultiContextSample;
        if (sample.kind == PassiveContextSampleKind::DiagnosticGrid)
            return PassiveSampleRejection::DiagnosticGridSample;
        if (sample.kind == PassiveContextSampleKind::RuntimeProbe || sample.evidence.runtime_launched
            || sample.evidence.source == PassiveEvidenceSource::RuntimeProbe)
            return PassiveSampleRejection::RequiresRuntimeLaunch;
        if (!is_single_context_sample(sample.kind))
            return PassiveSampleRejection::NotSingleContext;
        return PassiveSampleRejection::None;
    }

    [[nodiscard]] constexpr bool accepts_passive_context_sample(const PassiveContextSample& sample) noexcept
    {
        return rejection_for(sample) == PassiveSampleRejection::None;
    }

    [[nodiscard]] inline double passive_context_score_value(const PassiveContextEvidence& evidence) noexcept
    {
        double score = 50.0;

        if (evidence.fps)
            score += (std::clamp)(*evidence.fps, 0.0, 240.0) / 6.0;

        if (evidence.frame_time_ms)
            score += 20.0 - (std::clamp)(*evidence.frame_time_ms, 0.0, 80.0) / 4.0;

        if (evidence.frame_time_jitter_ms)
            score -= (std::clamp)(*evidence.frame_time_jitter_ms, 0.0, 40.0) / 2.0;

        switch (evidence.stability)
        {
        case PassiveSignalStatus::Healthy: score += 12.0; break;
        case PassiveSignalStatus::Watch: score -= 6.0; break;
        case PassiveSignalStatus::Unstable: score -= 24.0; break;
        case PassiveSignalStatus::NotSampled: break;
        case PassiveSignalStatus::Unknown:
        default: score -= 2.0; break;
        }

        if (evidence.input_latency_ms)
            score += 10.0 - (std::clamp)(*evidence.input_latency_ms, 0.0, 80.0) / 8.0;

        switch (evidence.input_latency_status)
        {
        case PassiveSignalStatus::Healthy: score += 8.0; break;
        case PassiveSignalStatus::Watch: score -= 4.0; break;
        case PassiveSignalStatus::Unstable: score -= 16.0; break;
        case PassiveSignalStatus::NotSampled: break;
        case PassiveSignalStatus::Unknown:
        default: score -= 1.0; break;
        }

        return (std::clamp)(score, 0.0, 100.0);
    }

    [[nodiscard]] inline PassiveContextScore score_passive_context_sample(const PassiveContextSample& sample) noexcept
    {
        PassiveContextScore result{
            .accepted = false,
            .rejection = rejection_for(sample),
            .backend = sample.backend,
            .evidence = sample.evidence,
            .score = 0.0
        };

        if (result.rejection != PassiveSampleRejection::None)
            return result;

        result.accepted = true;
        result.score = passive_context_score_value(sample.evidence);
        return result;
    }

    class PassiveContextScoreboard
    {
    public:
        [[nodiscard]] constexpr bool record(const PassiveContextScore& score) noexcept
        {
            if (!score.accepted || score.rejection != PassiveSampleRejection::None)
            {
                ++rejected_sample_count_;
                last_rejection_ = score.rejection;
                return false;
            }

            auto& bucket = buckets_[backend_index(score.backend)];
            bucket.backend = score.backend;
            ++bucket.sample_count;
            bucket.total_score += score.score;
            bucket.recent_score = score.score;
            bucket.has_recent_score = true;
            ++accepted_sample_count_;
            last_rejection_ = PassiveSampleRejection::None;
            return true;
        }

        [[nodiscard]] inline bool record(const PassiveContextSample& sample) noexcept
        {
            return record(score_passive_context_sample(sample));
        }

        [[nodiscard]] constexpr PassiveBackendScoreSummary summary_for(PassiveContextBackend backend) const noexcept
        {
            const auto& bucket = buckets_[backend_index(backend)];
            return PassiveBackendScoreSummary{
                .backend = bucket.backend,
                .sample_count = bucket.sample_count,
                .average_score = bucket.sample_count == 0U
                    ? 0.0
                    : bucket.total_score / static_cast<double>(bucket.sample_count),
                .recent_score = bucket.recent_score,
                .has_recent_score = bucket.has_recent_score
            };
        }

        [[nodiscard]] constexpr PassiveContextRecommendation best_recommendation() const noexcept
        {
            PassiveContextRecommendation best{};

            for (const auto& bucket : buckets_)
            {
                if (bucket.sample_count == 0U)
                    continue;

                const double average = bucket.total_score / static_cast<double>(bucket.sample_count);
                if (!best.available || average > best.average_score
                    || (average == best.average_score && bucket.recent_score > best.recent_score))
                {
                    best = PassiveContextRecommendation{
                        .available = true,
                        .backend = bucket.backend,
                        .sample_count = bucket.sample_count,
                        .average_score = average,
                        .recent_score = bucket.recent_score
                    };
                }
            }

            return best;
        }

        [[nodiscard]] constexpr std::uint32_t accepted_sample_count() const noexcept
        {
            return accepted_sample_count_;
        }

        [[nodiscard]] constexpr std::uint32_t rejected_sample_count() const noexcept
        {
            return rejected_sample_count_;
        }

        [[nodiscard]] constexpr PassiveSampleRejection last_rejection() const noexcept
        {
            return last_rejection_;
        }

    private:
        struct Bucket
        {
            PassiveContextBackend backend{ PassiveContextBackend::None };
            std::uint32_t sample_count{ 0 };
            double total_score{ 0.0 };
            double recent_score{ 0.0 };
            bool has_recent_score{ false };
        };

        std::array<Bucket, passive_context_backend_count> buckets_{ [] {
            std::array<Bucket, passive_context_backend_count> initialized{};
            for (std::size_t index = 0; index < initialized.size(); ++index)
                initialized[index].backend = static_cast<PassiveContextBackend>(index);
            return initialized;
        }() };
        std::uint32_t accepted_sample_count_{ 0 };
        std::uint32_t rejected_sample_count_{ 0 };
        PassiveSampleRejection last_rejection_{ PassiveSampleRejection::None };
    };

    [[nodiscard]] constexpr PassiveContextSample make_passive_single_context_sample(
        PassiveContextBackend backend,
        PassiveContextEvidence evidence = {},
        std::string_view label = {}) noexcept
    {
        return PassiveContextSample{
            .backend = backend,
            .kind = PassiveContextSampleKind::SingleContextBackend,
            .evidence = evidence,
            .label = label
        };
    }
}
