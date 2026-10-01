/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch Project.   *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 ************************************************/
module;

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <string_view>

export module render.event_debug;

export namespace epochengine::render_event_debug
{
    enum class Path : std::uint8_t
    {
        unavailable = 0,
        conventional,
        cached,
        partial,
        full
    };

    enum class FallbackReason : std::uint8_t
    {
        none = 0,
        event_path_disabled,
        unsupported_preview_mode,
        preview_pipeline_unavailable,
        scene_cache_unavailable,
        cache_invalid,
        camera_changed,
        sampled_surface_active,
        global_invalidation,
        change_frame_unavailable,
        dirty_region_limit,
        dirty_coverage_limit,
        neuromorphic_dense_activity,
        benchmark_full_baseline
    };

    enum class BenchmarkLane : std::uint8_t
    {
        disabled = 0,
        selective,
        full_baseline
    };

    struct BenchmarkTiming final
    {
        std::uint64_t samples{};
        std::uint64_t total_time_ns{};
        std::uint64_t last_time_ns{};
        std::uint64_t min_time_ns{};
        std::uint64_t max_time_ns{};
        double average_time_us{};
    };

    struct Snapshot final
    {
        bool event_path_enabled{ true };
        bool dirty_overlay_enabled{};
        bool vacated_overlay_enabled{};
        bool benchmark_enabled{};
        BenchmarkLane benchmark_lane{ BenchmarkLane::disabled };
        Path path{ Path::unavailable };
        FallbackReason fallback{ FallbackReason::none };
        std::uint32_t dirty_regions{};
        std::uint32_t vacated_regions{};
        float dirty_coverage_percent{};
        std::uint64_t camera_revision{};
        std::uint64_t geometry_revision{};
        std::uint64_t render_time_us{};
        std::uint64_t cached_frames{};
        std::uint64_t partial_frames{};
        std::uint64_t full_frames{};
        std::uint64_t conventional_frames{};
        BenchmarkTiming cached{};
        BenchmarkTiming partial{};
        BenchmarkTiming full{};
        BenchmarkTiming conventional{};
        BenchmarkTiming selective{};
        BenchmarkTiming full_baseline{};
        double average_speedup{};
        double average_time_saved_percent{};
    };

    namespace detail
    {
        inline std::atomic_bool event_path_enabled{ true };
        inline std::atomic_bool dirty_overlay_enabled{ false };
        inline std::atomic_bool vacated_overlay_enabled{ false };
        inline std::atomic_bool benchmark_enabled{ false };
        inline std::atomic_uint64_t benchmark_frame_index{};
        inline std::atomic<BenchmarkLane> benchmark_lane{ BenchmarkLane::disabled };
        inline std::atomic<Path> path{ Path::unavailable };
        inline std::atomic<FallbackReason> fallback{ FallbackReason::none };
        inline std::atomic_uint32_t dirty_regions{};
        inline std::atomic_uint32_t vacated_regions{};
        inline std::atomic_uint32_t dirty_coverage_milli_percent{};
        inline std::atomic_uint64_t camera_revision{};
        inline std::atomic_uint64_t geometry_revision{};
        inline std::atomic_uint64_t render_time_us{};
        inline std::atomic_uint64_t cached_frames{};
        inline std::atomic_uint64_t partial_frames{};
        inline std::atomic_uint64_t full_frames{};
        inline std::atomic_uint64_t conventional_frames{};

        struct TimingAccumulator final
        {
            std::atomic_uint64_t samples{};
            std::atomic_uint64_t total_time_ns{};
            std::atomic_uint64_t last_time_ns{};
            std::atomic_uint64_t min_time_ns{ std::numeric_limits<std::uint64_t>::max() };
            std::atomic_uint64_t max_time_ns{};
        };

        inline TimingAccumulator cached_timing{};
        inline TimingAccumulator partial_timing{};
        inline TimingAccumulator full_timing{};
        inline TimingAccumulator conventional_timing{};
        inline TimingAccumulator selective_timing{};
        inline TimingAccumulator full_baseline_timing{};

        inline void reset_timing(TimingAccumulator& timing) noexcept
        {
            timing.samples.store(0u, std::memory_order_relaxed);
            timing.total_time_ns.store(0u, std::memory_order_relaxed);
            timing.last_time_ns.store(0u, std::memory_order_relaxed);
            timing.min_time_ns.store(std::numeric_limits<std::uint64_t>::max(), std::memory_order_relaxed);
            timing.max_time_ns.store(0u, std::memory_order_relaxed);
        }

        inline void update_min(std::atomic_uint64_t& target, std::uint64_t value) noexcept
        {
            auto current = target.load(std::memory_order_relaxed);
            while (value < current
                && !target.compare_exchange_weak(
                    current, value,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
            }
        }

        inline void update_max(std::atomic_uint64_t& target, std::uint64_t value) noexcept
        {
            auto current = target.load(std::memory_order_relaxed);
            while (value > current
                && !target.compare_exchange_weak(
                    current, value,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
            }
        }

        inline void record(TimingAccumulator& timing, std::uint64_t duration_ns) noexcept
        {
            timing.last_time_ns.store(duration_ns, std::memory_order_relaxed);
            timing.total_time_ns.fetch_add(duration_ns, std::memory_order_relaxed);
            timing.samples.fetch_add(1u, std::memory_order_relaxed);
            update_min(timing.min_time_ns, duration_ns);
            update_max(timing.max_time_ns, duration_ns);
        }

        [[nodiscard]] inline BenchmarkTiming snapshot_timing(const TimingAccumulator& timing) noexcept
        {
            BenchmarkTiming out{};
            out.samples = timing.samples.load(std::memory_order_relaxed);
            out.total_time_ns = timing.total_time_ns.load(std::memory_order_relaxed);
            out.last_time_ns = timing.last_time_ns.load(std::memory_order_relaxed);
            out.max_time_ns = timing.max_time_ns.load(std::memory_order_relaxed);
            const auto minimum = timing.min_time_ns.load(std::memory_order_relaxed);
            out.min_time_ns = minimum == std::numeric_limits<std::uint64_t>::max()
                ? 0u : minimum;
            out.average_time_us = out.samples == 0u
                ? 0.0
                : static_cast<double>(out.total_time_ns)
                    / static_cast<double>(out.samples) / 1000.0;
            return out;
        }
    }

    inline void reset_benchmark_statistics() noexcept;

    [[nodiscard]] inline bool event_path_enabled() noexcept
    {
        return detail::event_path_enabled.load(std::memory_order_relaxed);
    }

    inline void set_event_path_enabled(bool enabled) noexcept
    {
        detail::event_path_enabled.store(enabled, std::memory_order_relaxed);
        if (!enabled)
        {
            detail::benchmark_enabled.store(false, std::memory_order_relaxed);
            detail::benchmark_lane.store(BenchmarkLane::disabled, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] inline bool dirty_overlay_enabled() noexcept
    {
        return detail::dirty_overlay_enabled.load(std::memory_order_relaxed);
    }

    inline void set_dirty_overlay_enabled(bool enabled) noexcept
    {
        detail::dirty_overlay_enabled.store(enabled, std::memory_order_relaxed);
    }

    [[nodiscard]] inline bool vacated_overlay_enabled() noexcept
    {
        return detail::vacated_overlay_enabled.load(std::memory_order_relaxed);
    }

    inline void set_vacated_overlay_enabled(bool enabled) noexcept
    {
        detail::vacated_overlay_enabled.store(enabled, std::memory_order_relaxed);
    }

    [[nodiscard]] inline bool benchmark_enabled() noexcept
    {
        return detail::benchmark_enabled.load(std::memory_order_relaxed);
    }

    inline void reset_benchmark_statistics() noexcept
    {
        detail::benchmark_frame_index.store(0u, std::memory_order_relaxed);
        detail::benchmark_lane.store(BenchmarkLane::disabled, std::memory_order_relaxed);
        detail::reset_timing(detail::selective_timing);
        detail::reset_timing(detail::full_baseline_timing);
    }

    inline void reset_path_statistics() noexcept
    {
        detail::cached_frames.store(0u, std::memory_order_relaxed);
        detail::partial_frames.store(0u, std::memory_order_relaxed);
        detail::full_frames.store(0u, std::memory_order_relaxed);
        detail::conventional_frames.store(0u, std::memory_order_relaxed);
        detail::reset_timing(detail::cached_timing);
        detail::reset_timing(detail::partial_timing);
        detail::reset_timing(detail::full_timing);
        detail::reset_timing(detail::conventional_timing);
    }

    inline void set_benchmark_enabled(bool enabled) noexcept
    {
        detail::benchmark_enabled.store(enabled, std::memory_order_relaxed);
        if (enabled)
            detail::event_path_enabled.store(true, std::memory_order_relaxed);
        else
            detail::benchmark_lane.store(BenchmarkLane::disabled, std::memory_order_relaxed);
    }

    [[nodiscard]] inline BenchmarkLane begin_benchmark_frame() noexcept
    {
        if (!benchmark_enabled())
        {
            detail::benchmark_lane.store(BenchmarkLane::disabled, std::memory_order_relaxed);
            return BenchmarkLane::disabled;
        }

        const std::uint64_t index = detail::benchmark_frame_index.fetch_add(1u, std::memory_order_relaxed);
        const BenchmarkLane lane = (index & 1u) == 0u
            ? BenchmarkLane::selective
            : BenchmarkLane::full_baseline;
        detail::benchmark_lane.store(lane, std::memory_order_relaxed);
        return lane;
    }

    inline void record_benchmark_sample(BenchmarkLane lane, std::uint64_t duration_ns) noexcept
    {
        if (duration_ns == 0u)
            duration_ns = 1u;

        switch (lane)
        {
        case BenchmarkLane::selective:
            detail::record(detail::selective_timing, duration_ns);
            break;
        case BenchmarkLane::full_baseline:
            detail::record(detail::full_baseline_timing, duration_ns);
            break;
        case BenchmarkLane::disabled:
        default:
            break;
        }
    }

    inline void publish(
        Path path,
        FallbackReason fallback,
        std::uint32_t dirty_regions,
        std::uint32_t vacated_regions,
        float dirty_coverage_percent,
        std::uint64_t camera_revision,
        std::uint64_t geometry_revision,
        std::uint64_t render_time_us) noexcept
    {
        if (dirty_coverage_percent < 0.0f)
            dirty_coverage_percent = 0.0f;
        if (dirty_coverage_percent > 100.0f)
            dirty_coverage_percent = 100.0f;

        detail::path.store(path, std::memory_order_relaxed);
        detail::fallback.store(fallback, std::memory_order_relaxed);
        detail::dirty_regions.store(dirty_regions, std::memory_order_relaxed);
        detail::vacated_regions.store(vacated_regions, std::memory_order_relaxed);
        detail::dirty_coverage_milli_percent.store(
            static_cast<std::uint32_t>(dirty_coverage_percent * 1000.0f + 0.5f),
            std::memory_order_relaxed);
        detail::camera_revision.store(camera_revision, std::memory_order_relaxed);
        detail::geometry_revision.store(geometry_revision, std::memory_order_relaxed);
        detail::render_time_us.store(render_time_us, std::memory_order_relaxed);

        const std::uint64_t sample_ns = (std::max<std::uint64_t>)(1u, render_time_us * 1000u);
        switch (path)
        {
        case Path::cached:
            detail::cached_frames.fetch_add(1u, std::memory_order_relaxed);
            detail::record(detail::cached_timing, sample_ns);
            break;
        case Path::partial:
            detail::partial_frames.fetch_add(1u, std::memory_order_relaxed);
            detail::record(detail::partial_timing, sample_ns);
            break;
        case Path::full:
            detail::full_frames.fetch_add(1u, std::memory_order_relaxed);
            detail::record(detail::full_timing, sample_ns);
            break;
        case Path::conventional:
            detail::conventional_frames.fetch_add(1u, std::memory_order_relaxed);
            detail::record(detail::conventional_timing, sample_ns);
            break;
        case Path::unavailable:
        default:
            break;
        }
    }

    [[nodiscard]] inline Snapshot snapshot() noexcept
    {
        Snapshot out{};
        out.event_path_enabled = event_path_enabled();
        out.dirty_overlay_enabled = dirty_overlay_enabled();
        out.vacated_overlay_enabled = vacated_overlay_enabled();
        out.benchmark_enabled = benchmark_enabled();
        out.benchmark_lane = detail::benchmark_lane.load(std::memory_order_relaxed);
        out.path = detail::path.load(std::memory_order_relaxed);
        out.fallback = detail::fallback.load(std::memory_order_relaxed);
        out.dirty_regions = detail::dirty_regions.load(std::memory_order_relaxed);
        out.vacated_regions = detail::vacated_regions.load(std::memory_order_relaxed);
        out.dirty_coverage_percent =
            static_cast<float>(detail::dirty_coverage_milli_percent.load(std::memory_order_relaxed)) / 1000.0f;
        out.camera_revision = detail::camera_revision.load(std::memory_order_relaxed);
        out.geometry_revision = detail::geometry_revision.load(std::memory_order_relaxed);
        out.render_time_us = detail::render_time_us.load(std::memory_order_relaxed);
        out.cached_frames = detail::cached_frames.load(std::memory_order_relaxed);
        out.partial_frames = detail::partial_frames.load(std::memory_order_relaxed);
        out.full_frames = detail::full_frames.load(std::memory_order_relaxed);
        out.conventional_frames = detail::conventional_frames.load(std::memory_order_relaxed);
        out.cached = detail::snapshot_timing(detail::cached_timing);
        out.partial = detail::snapshot_timing(detail::partial_timing);
        out.full = detail::snapshot_timing(detail::full_timing);
        out.conventional = detail::snapshot_timing(detail::conventional_timing);
        out.selective = detail::snapshot_timing(detail::selective_timing);
        out.full_baseline = detail::snapshot_timing(detail::full_baseline_timing);

        if (out.selective.average_time_us > 0.0 && out.full_baseline.average_time_us > 0.0)
        {
            out.average_speedup = out.full_baseline.average_time_us / out.selective.average_time_us;
            out.average_time_saved_percent =
                (1.0 - out.selective.average_time_us / out.full_baseline.average_time_us) * 100.0;
        }
        return out;
    }

    [[nodiscard]] inline double render_fps(const BenchmarkTiming& timing) noexcept
    {
        return timing.average_time_us > 0.0
            ? 1'000'000.0 / timing.average_time_us
            : 0.0;
    }

    [[nodiscard]] inline const BenchmarkTiming& timing_for_path(
        const Snapshot& snapshot, Path path) noexcept
    {
        switch (path)
        {
        case Path::cached: return snapshot.cached;
        case Path::partial: return snapshot.partial;
        case Path::full: return snapshot.full;
        case Path::conventional: return snapshot.conventional;
        case Path::unavailable:
        default:
        {
            static const BenchmarkTiming unavailable{};
            return unavailable;
        }
        }
    }

    [[nodiscard]] constexpr std::string_view path_name(Path path) noexcept
    {
        switch (path)
        {
        case Path::conventional: return "CONVENTIONAL";
        case Path::cached: return "CACHED";
        case Path::partial: return "PARTIAL";
        case Path::full: return "FULL";
        case Path::unavailable:
        default: return "UNAVAILABLE";
        }
    }

    [[nodiscard]] constexpr std::string_view benchmark_lane_name(BenchmarkLane lane) noexcept
    {
        switch (lane)
        {
        case BenchmarkLane::selective: return "SELECTIVE";
        case BenchmarkLane::full_baseline: return "FULL BASELINE";
        case BenchmarkLane::disabled:
        default: return "OFF";
        }
    }

    [[nodiscard]] constexpr std::string_view fallback_name(FallbackReason reason) noexcept
    {
        switch (reason)
        {
        case FallbackReason::event_path_disabled: return "event path disabled";
        case FallbackReason::unsupported_preview_mode: return "unsupported preview mode";
        case FallbackReason::preview_pipeline_unavailable: return "preview pipeline unavailable";
        case FallbackReason::scene_cache_unavailable: return "scene cache unavailable";
        case FallbackReason::cache_invalid: return "cache invalid";
        case FallbackReason::camera_changed: return "camera changed";
        case FallbackReason::sampled_surface_active: return "sampled surface active";
        case FallbackReason::global_invalidation: return "global scene invalidation";
        case FallbackReason::change_frame_unavailable: return "change frame unavailable";
        case FallbackReason::dirty_region_limit: return "dirty-region count limit";
        case FallbackReason::dirty_coverage_limit: return "dirty coverage limit";
        case FallbackReason::neuromorphic_dense_activity: return "neuromorphic dense activity";
        case FallbackReason::benchmark_full_baseline: return "benchmark full baseline";
        case FallbackReason::none:
        default: return "none";
        }
    }
}
