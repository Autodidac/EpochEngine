/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>

module audio.device_sdl;

namespace epochengine::audio
{
    namespace
    {
        class SdlAudioDeviceSink final : public AudioDeviceSink
        {
        public:
            ~SdlAudioDeviceSink() override
            {
                close();
            }

            [[nodiscard]] AudioDeviceCode open(
                const AudioDeviceConfiguration& configuration) override
            {
                close();
                if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
                {
                    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
                    {
                        set_error("SDL audio initialization failed");
                        return AudioDeviceCode::unavailable;
                    }
                    initialized_here_ = true;
                }

                SDL_AudioSpec specification{};
                specification.format = SDL_AUDIO_F32;
                specification.channels = static_cast<int>(configuration.channel_count);
                specification.freq = static_cast<int>(configuration.sample_rate);
                stream_ = SDL_OpenAudioDeviceStream(
                    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                    &specification,
                    nullptr,
                    nullptr);
                if (!stream_)
                {
                    set_error("SDL could not open the default playback device");
                    close();
                    return AudioDeviceCode::unavailable;
                }

                channel_count_ = configuration.channel_count;
                if (!SDL_ResumeAudioStreamDevice(stream_))
                {
                    set_error("SDL could not resume the playback device");
                    close();
                    return AudioDeviceCode::sink_failed;
                }
                diagnostic_ = "SDL3 default playback stream is ready";
                return AudioDeviceCode::success;
            }

            void close() noexcept override
            {
                if (stream_)
                {
                    SDL_DestroyAudioStream(stream_);
                    stream_ = nullptr;
                }
                channel_count_ = 0;
                if (initialized_here_)
                {
                    SDL_QuitSubSystem(SDL_INIT_AUDIO);
                    initialized_here_ = false;
                }
            }

            [[nodiscard]] AudioDeviceCode set_paused(bool paused) override
            {
                if (!stream_)
                    return AudioDeviceCode::not_open;
                const bool succeeded = paused
                    ? SDL_PauseAudioStreamDevice(stream_)
                    : SDL_ResumeAudioStreamDevice(stream_);
                if (!succeeded)
                {
                    set_error(paused
                        ? "SDL could not pause the playback device"
                        : "SDL could not resume the playback device");
                    return AudioDeviceCode::sink_failed;
                }
                diagnostic_ = paused
                    ? "SDL3 default playback stream is paused"
                    : "SDL3 default playback stream is ready";
                return AudioDeviceCode::success;
            }

            [[nodiscard]] AudioDeviceCode clear() override
            {
                if (!stream_)
                    return AudioDeviceCode::not_open;
                if (!SDL_ClearAudioStream(stream_))
                {
                    set_error("SDL could not clear queued playback samples");
                    return AudioDeviceCode::sink_failed;
                }
                return AudioDeviceCode::success;
            }

            [[nodiscard]] AudioDeviceCode submit(
                std::span<const float> interleavedSamples) override
            {
                if (!stream_)
                    return AudioDeviceCode::not_open;
                if (interleavedSamples.size()
                    > static_cast<std::size_t>((std::numeric_limits<int>::max)())
                        / sizeof(float))
                {
                    diagnostic_ = "SDL audio submission exceeds its byte-count limit";
                    return AudioDeviceCode::invalid_sample_block;
                }
                const int bytes = static_cast<int>(
                    interleavedSamples.size() * sizeof(float));
                if (!SDL_PutAudioStreamData(
                        stream_, interleavedSamples.data(), bytes))
                {
                    set_error("SDL rejected queued playback samples");
                    return AudioDeviceCode::sink_failed;
                }
                return AudioDeviceCode::success;
            }

            [[nodiscard]] std::uint64_t queued_frames() const noexcept override
            {
                if (!stream_ || channel_count_ == 0)
                    return 0;
                const int queuedBytes = SDL_GetAudioStreamQueued(stream_);
                if (queuedBytes <= 0)
                    return 0;
                const std::uint64_t bytesPerFrame =
                    static_cast<std::uint64_t>(channel_count_) * sizeof(float);
                return static_cast<std::uint64_t>(queuedBytes) / bytesPerFrame;
            }

            [[nodiscard]] std::string_view diagnostic() const noexcept override
            {
                return diagnostic_;
            }

        private:
            void set_error(std::string_view prefix)
            {
                diagnostic_.assign(prefix);
                if (const char* error = SDL_GetError(); error && error[0] != '\0')
                {
                    diagnostic_.append(": ");
                    diagnostic_.append(error);
                }
            }

            SDL_AudioStream* stream_{};
            std::uint16_t channel_count_{};
            bool initialized_here_{};
            std::string diagnostic_{"SDL3 audio sink is closed"};
        };
    }

    std::unique_ptr<AudioDeviceSink> make_sdl_audio_device_sink()
    {
        return std::make_unique<SdlAudioDeviceSink>();
    }
}
