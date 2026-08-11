/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <memory>

export module audio.device_sdl;

export import audio.device;

export namespace epochengine::audio
{
    [[nodiscard]] std::unique_ptr<AudioDeviceSink>
    make_sdl_audio_device_sink();
}
