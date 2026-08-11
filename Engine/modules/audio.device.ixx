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

export module audio.device;

export namespace epochengine::audio
{
    enum class AudioDeviceState : std::uint8_t
    {
        unavailable,
        closed,
        ready,
        paused,
        faulted
    };

    enum class AudioDeviceCode : std::uint8_t
    {
        success,
        unavailable,
        invalid_configuration,
        already_open,
        not_open,
        invalid_sample_block,
        queue_saturated,
        sink_failed
    };

    struct AudioDeviceConfiguration final
    {
        std::uint32_t sample_rate{48'000};
        std::uint16_t channel_count{2};
        std::uint32_t maximum_submission_frames{4'800};
        std::uint32_t maximum_queued_frames{24'000};

        [[nodiscard]] friend constexpr bool operator==(
            const AudioDeviceConfiguration&,
            const AudioDeviceConfiguration&) noexcept = default;
    };

    struct AudioDeviceMetrics final
    {
        std::uint64_t open_attempts{};
        std::uint64_t successful_opens{};
        std::uint64_t close_count{};
        std::uint64_t pause_transitions{};
        std::uint64_t clear_count{};
        std::uint64_t submitted_blocks{};
        std::uint64_t submitted_frames{};
        std::uint64_t rejected_blocks{};
        std::uint64_t sink_failures{};
        std::uint64_t peak_queued_frames{};

        [[nodiscard]] friend constexpr bool operator==(
            const AudioDeviceMetrics&,
            const AudioDeviceMetrics&) noexcept = default;
    };

    struct AudioDeviceSnapshot final
    {
        AudioDeviceState state{AudioDeviceState::unavailable};
        AudioDeviceConfiguration configuration{};
        AudioDeviceMetrics metrics{};
        std::uint64_t queued_frames{};
        std::uint64_t generation{};
        std::string diagnostic{};
    };

    class AudioDeviceSink
    {
    public:
        virtual ~AudioDeviceSink() = default;

        [[nodiscard]] virtual AudioDeviceCode open(
            const AudioDeviceConfiguration& configuration) = 0;
        virtual void close() noexcept = 0;
        [[nodiscard]] virtual AudioDeviceCode set_paused(bool paused) = 0;
        [[nodiscard]] virtual AudioDeviceCode clear() = 0;
        [[nodiscard]] virtual AudioDeviceCode submit(
            std::span<const float> interleavedSamples) = 0;
        [[nodiscard]] virtual std::uint64_t queued_frames() const noexcept = 0;
        [[nodiscard]] virtual std::string_view diagnostic() const noexcept = 0;
    };

    class PhysicalAudioDevice final
    {
    public:
        explicit PhysicalAudioDevice(std::unique_ptr<AudioDeviceSink> sink = {});
        ~PhysicalAudioDevice();

        PhysicalAudioDevice(PhysicalAudioDevice&&) noexcept;
        PhysicalAudioDevice& operator=(PhysicalAudioDevice&&) noexcept;

        PhysicalAudioDevice(const PhysicalAudioDevice&) = delete;
        PhysicalAudioDevice& operator=(const PhysicalAudioDevice&) = delete;

        [[nodiscard]] AudioDeviceCode open(
            const AudioDeviceConfiguration& configuration = {});
        [[nodiscard]] AudioDeviceCode submit(
            std::span<const float> interleavedSamples);
        [[nodiscard]] AudioDeviceCode set_paused(bool paused);
        [[nodiscard]] AudioDeviceCode clear();
        void close() noexcept;

        [[nodiscard]] AudioDeviceState state() const noexcept;
        [[nodiscard]] AudioDeviceSnapshot snapshot() const;
        [[nodiscard]] const AudioDeviceConfiguration& configuration() const noexcept;
        [[nodiscard]] AudioDeviceMetrics metrics() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };

    [[nodiscard]] constexpr std::string_view audio_device_state_name(
        AudioDeviceState state) noexcept
    {
        switch (state)
        {
        case AudioDeviceState::unavailable: return "unavailable";
        case AudioDeviceState::closed: return "closed";
        case AudioDeviceState::ready: return "ready";
        case AudioDeviceState::paused: return "paused";
        case AudioDeviceState::faulted: return "faulted";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view audio_device_code_name(
        AudioDeviceCode code) noexcept
    {
        switch (code)
        {
        case AudioDeviceCode::success: return "success";
        case AudioDeviceCode::unavailable: return "unavailable";
        case AudioDeviceCode::invalid_configuration: return "invalid configuration";
        case AudioDeviceCode::already_open: return "already open";
        case AudioDeviceCode::not_open: return "not open";
        case AudioDeviceCode::invalid_sample_block: return "invalid sample block";
        case AudioDeviceCode::queue_saturated: return "queue saturated";
        case AudioDeviceCode::sink_failed: return "sink failed";
        }
        return "unknown";
    }

    [[nodiscard]] int run_audio_device_contract_tests();
}
