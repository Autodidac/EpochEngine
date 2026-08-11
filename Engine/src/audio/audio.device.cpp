/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>

module audio.device;

namespace epochengine::audio
{
    namespace
    {
        constexpr std::uint32_t minimum_sample_rate = 8'000;
        constexpr std::uint32_t maximum_sample_rate = 384'000;
        constexpr std::uint16_t maximum_channels = 8;
        constexpr std::uint32_t maximum_submission_frames = 1'048'576;
        constexpr std::uint32_t maximum_queued_frames = 4'194'304;

        [[nodiscard]] constexpr bool valid_configuration(
            const AudioDeviceConfiguration& configuration) noexcept
        {
            return configuration.sample_rate >= minimum_sample_rate
                && configuration.sample_rate <= maximum_sample_rate
                && configuration.channel_count > 0
                && configuration.channel_count <= maximum_channels
                && configuration.maximum_submission_frames > 0
                && configuration.maximum_submission_frames
                    <= maximum_submission_frames
                && configuration.maximum_queued_frames
                    >= configuration.maximum_submission_frames
                && configuration.maximum_queued_frames
                    <= maximum_queued_frames;
        }

        [[nodiscard]] constexpr std::uint64_t next_generation(
            std::uint64_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }
    }

    struct PhysicalAudioDevice::Implementation final
    {
        explicit Implementation(std::unique_ptr<AudioDeviceSink> requestedSink)
            : sink{std::move(requestedSink)}
            , state{sink ? AudioDeviceState::closed : AudioDeviceState::unavailable}
        {
            diagnostic = sink
                ? "physical audio device is closed"
                : "physical audio support is not compiled or available";
        }

        void fail_from_sink() noexcept
        {
            state = AudioDeviceState::faulted;
            ++metrics.sink_failures;
            diagnostic = sink ? std::string{sink->diagnostic()} : "audio sink unavailable";
            if (diagnostic.empty())
                diagnostic = "physical audio sink failed";
        }

        std::unique_ptr<AudioDeviceSink> sink{};
        AudioDeviceState state{AudioDeviceState::unavailable};
        AudioDeviceConfiguration configuration{};
        AudioDeviceMetrics metrics{};
        std::uint64_t generation{};
        std::string diagnostic{};
    };

    PhysicalAudioDevice::PhysicalAudioDevice(std::unique_ptr<AudioDeviceSink> sink)
        : implementation_{std::make_unique<Implementation>(std::move(sink))}
    {
    }

    PhysicalAudioDevice::~PhysicalAudioDevice()
    {
        close();
    }

    PhysicalAudioDevice::PhysicalAudioDevice(PhysicalAudioDevice&&) noexcept = default;
    PhysicalAudioDevice& PhysicalAudioDevice::operator=(PhysicalAudioDevice&&) noexcept = default;

    AudioDeviceCode PhysicalAudioDevice::open(
        const AudioDeviceConfiguration& configuration)
    {
        auto& implementation = *implementation_;
        ++implementation.metrics.open_attempts;
        if (!implementation.sink)
            return AudioDeviceCode::unavailable;
        if (!valid_configuration(configuration))
        {
            implementation.diagnostic = "physical audio configuration is invalid";
            return AudioDeviceCode::invalid_configuration;
        }
        if (implementation.state == AudioDeviceState::ready
            || implementation.state == AudioDeviceState::paused)
        {
            return AudioDeviceCode::already_open;
        }

        implementation.sink->close();
        const AudioDeviceCode code = implementation.sink->open(configuration);
        if (code != AudioDeviceCode::success)
        {
            implementation.fail_from_sink();
            return code == AudioDeviceCode::unavailable
                ? code
                : AudioDeviceCode::sink_failed;
        }

        implementation.configuration = configuration;
        implementation.state = AudioDeviceState::ready;
        implementation.generation = next_generation(implementation.generation);
        ++implementation.metrics.successful_opens;
        implementation.diagnostic = "physical audio device is ready";
        return AudioDeviceCode::success;
    }

    AudioDeviceCode PhysicalAudioDevice::submit(
        std::span<const float> interleavedSamples)
    {
        auto& implementation = *implementation_;
        if (!implementation.sink)
            return AudioDeviceCode::unavailable;
        if (implementation.state != AudioDeviceState::ready)
        {
            ++implementation.metrics.rejected_blocks;
            return AudioDeviceCode::not_open;
        }

        const std::uint64_t channels = implementation.configuration.channel_count;
        if (interleavedSamples.empty()
            || interleavedSamples.size() % channels != 0)
        {
            ++implementation.metrics.rejected_blocks;
            return AudioDeviceCode::invalid_sample_block;
        }

        const std::uint64_t frameCount = interleavedSamples.size() / channels;
        if (frameCount > implementation.configuration.maximum_submission_frames)
        {
            ++implementation.metrics.rejected_blocks;
            return AudioDeviceCode::invalid_sample_block;
        }

        const std::uint64_t queued = implementation.sink->queued_frames();
        implementation.metrics.peak_queued_frames = (std::max)(
            implementation.metrics.peak_queued_frames, queued);
        if (queued > implementation.configuration.maximum_queued_frames
            || frameCount > implementation.configuration.maximum_queued_frames - queued)
        {
            ++implementation.metrics.rejected_blocks;
            return AudioDeviceCode::queue_saturated;
        }

        const AudioDeviceCode code = implementation.sink->submit(interleavedSamples);
        if (code != AudioDeviceCode::success)
        {
            ++implementation.metrics.rejected_blocks;
            implementation.fail_from_sink();
            return AudioDeviceCode::sink_failed;
        }

        ++implementation.metrics.submitted_blocks;
        implementation.metrics.submitted_frames += frameCount;
        implementation.metrics.peak_queued_frames = (std::max)(
            implementation.metrics.peak_queued_frames,
            implementation.sink->queued_frames());
        return AudioDeviceCode::success;
    }

    AudioDeviceCode PhysicalAudioDevice::set_paused(bool paused)
    {
        auto& implementation = *implementation_;
        if (!implementation.sink)
            return AudioDeviceCode::unavailable;
        if (implementation.state != AudioDeviceState::ready
            && implementation.state != AudioDeviceState::paused)
        {
            return AudioDeviceCode::not_open;
        }
        if ((implementation.state == AudioDeviceState::paused) == paused)
            return AudioDeviceCode::success;

        const AudioDeviceCode code = implementation.sink->set_paused(paused);
        if (code != AudioDeviceCode::success)
        {
            implementation.fail_from_sink();
            return AudioDeviceCode::sink_failed;
        }
        implementation.state = paused
            ? AudioDeviceState::paused
            : AudioDeviceState::ready;
        ++implementation.metrics.pause_transitions;
        implementation.diagnostic = paused
            ? "physical audio device is paused"
            : "physical audio device is ready";
        return AudioDeviceCode::success;
    }

    AudioDeviceCode PhysicalAudioDevice::clear()
    {
        auto& implementation = *implementation_;
        if (!implementation.sink)
            return AudioDeviceCode::unavailable;
        if (implementation.state != AudioDeviceState::ready
            && implementation.state != AudioDeviceState::paused)
        {
            return AudioDeviceCode::not_open;
        }
        const AudioDeviceCode code = implementation.sink->clear();
        if (code != AudioDeviceCode::success)
        {
            implementation.fail_from_sink();
            return AudioDeviceCode::sink_failed;
        }
        ++implementation.metrics.clear_count;
        return AudioDeviceCode::success;
    }

    void PhysicalAudioDevice::close() noexcept
    {
        if (!implementation_ || !implementation_->sink)
            return;
        auto& implementation = *implementation_;
        const bool wasOpen = implementation.state == AudioDeviceState::ready
            || implementation.state == AudioDeviceState::paused
            || implementation.state == AudioDeviceState::faulted;
        implementation.sink->close();
        implementation.state = AudioDeviceState::closed;
        implementation.diagnostic = "physical audio device is closed";
        if (wasOpen)
            ++implementation.metrics.close_count;
    }

    AudioDeviceState PhysicalAudioDevice::state() const noexcept
    {
        return implementation_->state;
    }

    AudioDeviceSnapshot PhysicalAudioDevice::snapshot() const
    {
        const auto& implementation = *implementation_;
        return {
            .state = implementation.state,
            .configuration = implementation.configuration,
            .metrics = implementation.metrics,
            .queued_frames = implementation.sink
                && (implementation.state == AudioDeviceState::ready
                    || implementation.state == AudioDeviceState::paused)
                ? implementation.sink->queued_frames()
                : 0,
            .generation = implementation.generation,
            .diagnostic = implementation.diagnostic
        };
    }

    const AudioDeviceConfiguration& PhysicalAudioDevice::configuration() const noexcept
    {
        return implementation_->configuration;
    }

    AudioDeviceMetrics PhysicalAudioDevice::metrics() const noexcept
    {
        return implementation_->metrics;
    }
}
