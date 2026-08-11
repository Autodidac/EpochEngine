/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module audio.device;

namespace epochengine::audio
{
    namespace
    {
        class ContractSink final : public AudioDeviceSink
        {
        public:
            [[nodiscard]] AudioDeviceCode open(
                const AudioDeviceConfiguration& configuration) override
            {
                ++open_count;
                if (fail_open)
                {
                    diagnostic_text = "injected open failure";
                    return AudioDeviceCode::unavailable;
                }
                channels = configuration.channel_count;
                opened = true;
                paused = false;
                return AudioDeviceCode::success;
            }

            void close() noexcept override
            {
                if (opened)
                    ++close_count;
                opened = false;
                paused = false;
                queued = 0;
            }

            [[nodiscard]] AudioDeviceCode set_paused(bool requested) override
            {
                if (!opened)
                    return AudioDeviceCode::not_open;
                paused = requested;
                return AudioDeviceCode::success;
            }

            [[nodiscard]] AudioDeviceCode clear() override
            {
                if (!opened)
                    return AudioDeviceCode::not_open;
                queued = 0;
                return AudioDeviceCode::success;
            }

            [[nodiscard]] AudioDeviceCode submit(
                std::span<const float> samples) override
            {
                if (!opened)
                    return AudioDeviceCode::not_open;
                if (fail_submit)
                {
                    diagnostic_text = "injected submit failure";
                    return AudioDeviceCode::sink_failed;
                }
                queued += samples.size() / channels;
                received.assign(samples.begin(), samples.end());
                return AudioDeviceCode::success;
            }

            [[nodiscard]] std::uint64_t queued_frames() const noexcept override
            {
                return queued;
            }

            [[nodiscard]] std::string_view diagnostic() const noexcept override
            {
                return diagnostic_text;
            }

            bool fail_open{};
            bool fail_submit{};
            bool opened{};
            bool paused{};
            std::uint16_t channels{};
            std::uint64_t queued{};
            std::uint64_t open_count{};
            std::uint64_t close_count{};
            std::vector<float> received{};
            std::string diagnostic_text{"contract sink ready"};
        };

        [[nodiscard]] int unavailable_and_validation_contract()
        {
            PhysicalAudioDevice unavailable{};
            if (unavailable.state() != AudioDeviceState::unavailable
                || unavailable.open() != AudioDeviceCode::unavailable)
            {
                return 1;
            }

            auto sink = std::make_unique<ContractSink>();
            PhysicalAudioDevice device{std::move(sink)};
            AudioDeviceConfiguration invalid{};
            invalid.channel_count = 0;
            if (device.open(invalid) != AudioDeviceCode::invalid_configuration
                || device.state() != AudioDeviceState::closed)
            {
                return 2;
            }
            return 0;
        }

        [[nodiscard]] int lifecycle_and_backpressure_contract()
        {
            auto owned = std::make_unique<ContractSink>();
            ContractSink* sink = owned.get();
            PhysicalAudioDevice device{std::move(owned)};
            const AudioDeviceConfiguration configuration{
                .sample_rate = 48'000,
                .channel_count = 2,
                .maximum_submission_frames = 4,
                .maximum_queued_frames = 6
            };
            if (device.open(configuration) != AudioDeviceCode::success
                || device.state() != AudioDeviceState::ready
                || device.open(configuration) != AudioDeviceCode::already_open)
            {
                return 1;
            }

            const std::vector<float> first{1.0f, -1.0f, 0.5f, -0.5f,
                0.25f, -0.25f, 0.0f, 0.0f};
            if (device.submit(first) != AudioDeviceCode::success
                || sink->received != first
                || sink->queued != 4)
            {
                return 2;
            }

            const std::vector<float> next{0.0f, 0.0f, 0.0f, 0.0f};
            if (device.submit(next) != AudioDeviceCode::success
                || device.submit(next) != AudioDeviceCode::queue_saturated)
            {
                return 3;
            }
            if (device.set_paused(true) != AudioDeviceCode::success
                || device.state() != AudioDeviceState::paused
                || device.submit(next) != AudioDeviceCode::not_open
                || device.clear() != AudioDeviceCode::success
                || sink->queued != 0
                || device.set_paused(false) != AudioDeviceCode::success)
            {
                return 4;
            }

            const AudioDeviceSnapshot snapshot = device.snapshot();
            if (snapshot.generation != 1
                || snapshot.metrics.successful_opens != 1
                || snapshot.metrics.submitted_blocks != 2
                || snapshot.metrics.submitted_frames != 6
                || snapshot.metrics.rejected_blocks != 2
                || snapshot.metrics.pause_transitions != 2
                || snapshot.metrics.clear_count != 1
                || snapshot.metrics.peak_queued_frames != 6)
            {
                return 5;
            }

            device.close();
            if (device.state() != AudioDeviceState::closed
                || sink->close_count != 1
                || device.open(configuration) != AudioDeviceCode::success
                || device.snapshot().generation != 2)
            {
                return 6;
            }
            return 0;
        }

        [[nodiscard]] int fault_and_recovery_contract()
        {
            auto owned = std::make_unique<ContractSink>();
            ContractSink* sink = owned.get();
            PhysicalAudioDevice device{std::move(owned)};
            if (device.open() != AudioDeviceCode::success)
                return 1;
            sink->fail_submit = true;
            const std::vector<float> block(16, 0.0f);
            if (device.submit(block) != AudioDeviceCode::sink_failed
                || device.state() != AudioDeviceState::faulted
                || device.snapshot().diagnostic != "injected submit failure")
            {
                return 2;
            }
            device.close();
            sink->fail_submit = false;
            if (device.open() != AudioDeviceCode::success
                || device.metrics().sink_failures != 1)
            {
                return 3;
            }
            return 0;
        }
    }

    int run_audio_device_contract_tests()
    {
        if (const int code = unavailable_and_validation_contract(); code != 0)
            return 100 + code;
        if (const int code = lifecycle_and_backpressure_contract(); code != 0)
            return 200 + code;
        if (const int code = fault_and_recovery_contract(); code != 0)
            return 300 + code;
        return 0;
    }
}

#if defined(EPOCH_AUDIO_DEVICE_CONTRACT_MAIN)
int main()
{
    return epochengine::audio::run_audio_device_contract_tests();
}
#endif
