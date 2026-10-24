#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace epochengine::renderhost::frame_rate
{
    // Both chrome surfaces observe the native completion counter. This is a
    // render-frame rate, not UI transaction throughput or monitor scanout FPS.
    class Sample final
    {
        struct Interval final { double begin{}, end{}, rate{}; };
        std::array<Interval, 6> intervals_{};
        std::size_t interval_count_{};
        const void* owner_{};
        std::uint64_t generation_{};
        double sampled_at_{};
        double live_rate_{};
        double average_rate_{};
        bool started_{};
        bool has_sample_{};

    public:
        [[nodiscard]] bool observe(std::uint64_t generation, double now,
            const void* owner = nullptr) noexcept
        {
            if (!std::isfinite(now))
                return false;
            if (!started_ || owner != owner_ || generation < generation_ || now < sampled_at_)
            {
                *this = {};
                started_ = true;
                owner_ = owner;
                generation_ = generation;
                sampled_at_ = now;
                return false;
            }
            const double elapsed = now - sampled_at_;
            if (elapsed < 1.0)
                return false;

            live_rate_ = static_cast<double>(generation - generation_) / elapsed;
            const double cutoff = now - 5.0;
            std::size_t retained{};
            for (std::size_t i = 0; i < interval_count_; ++i)
                if (intervals_[i].end > cutoff)
                    intervals_[retained++] = intervals_[i];
            interval_count_ = retained;
            // Observations are at least one second apart, so a five-second
            // weighted window needs at most six crossing intervals.
            if (interval_count_ == intervals_.size())
            {
                for (std::size_t i = 1; i < interval_count_; ++i)
                    intervals_[i - 1] = intervals_[i];
                --interval_count_;
            }
            intervals_[interval_count_++] = {sampled_at_, now, live_rate_};
            double frames{}, duration{};
            for (std::size_t i = 0; i < interval_count_; ++i)
            {
                const auto& interval = intervals_[i];
                const double seconds = interval.end - (std::max)(interval.begin, cutoff);
                frames += interval.rate * seconds;
                duration += seconds;
            }
            average_rate_ = duration > 0.0 ? frames / duration : 0.0;
            has_sample_ = true;
            generation_ = generation;
            sampled_at_ = now;
            return true;
        }

        [[nodiscard]] double live_rate() const noexcept { return live_rate_; }
        [[nodiscard]] double average_rate() const noexcept { return average_rate_; }
        [[nodiscard]] bool has_sample() const noexcept { return has_sample_; }
    };
}
