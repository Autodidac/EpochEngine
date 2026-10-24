// SPDX-License-Identifier: MPL-2.0
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string_view>
#include <vector>
import audio.mixer;

namespace {
void little_endian(std::ofstream& out, std::uint32_t value, unsigned bytes) {
    for (unsigned n = 0; n < bytes; ++n) out.put(static_cast<char>((value >> (n * 8)) & 255));
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const bool self_test = std::string_view{argv[1]} == std::string_view{"--self-test"};
    constexpr std::uint32_t rate = 48000;
    using namespace epochengine::audio;
    AudioManager manager;
    AudioMixer mixer;
    const ClipId clip{1, 1};
    std::vector<float> samples(rate);
    for (std::uint32_t n = 0; n < rate; ++n)
        samples[n] = 0.25f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0 * n / rate));
    if (mixer.register_clip({.id = clip,
            .format = {.channel_layout = PcmChannelLayout::mono, .sample_rate = rate},
            .frame_count = rate, .interleaved_samples = std::move(samples)}) != AudioMixerCode::success) return 3;
    const auto bus = manager.bus_snapshot(manager.master_bus());
    if (!bus || mixer.bind_bus({bus->id, manager.master_bus()}) != AudioMixerCode::success) return 3;
    const auto voice = manager.create_source({
        .clip = {.id = clip, .duration_seconds = 1.0, .nominal_sample_rate = rate,
            .channel_count = 1, .seekable = true}, .bus = manager.master_bus()});
    if (!voice.succeeded() || !manager.submit({.target_frame = 1,
            .payload = PlayCommand{.source = voice.handle}}).accepted()) return 3;
    MixedAudioFrame mixed;
    std::vector<std::int16_t> output;
    output.reserve(rate * 2u);
    for (std::uint64_t block = 0; block < 48; ++block) {
        const auto plan = manager.prepare_frame({.frame_index = block + 1,
            .timeline_seconds = static_cast<double>(block) / 48.0, .duration_seconds = 1.0 / 48.0});
        mixed = mixer.mix(plan, {.output_format = {.sample_rate = rate}, .output_frame_count = 1000});
        if (!mixed || mixed.interleaved_samples.size() != 2000u) return 3;
        for (float sample : mixed.interleaved_samples)
            output.push_back(static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f)));
    }
    if (output.size() != rate * 2u || std::none_of(output.begin(), output.end(), [](auto v) { return v != 0; })) return 3;
    if (self_test) return 0;
    std::error_code error;
    if (std::filesystem::exists(argv[1], error) || error) return 2;
    std::ofstream out{argv[1], std::ios::binary | std::ios::noreplace};
    if (!out) return 4;
    const auto bytes = static_cast<std::uint32_t>(output.size() * 2u);
    out.write("RIFF", 4); little_endian(out, bytes + 36, 4); out.write("WAVEfmt ", 8);
    little_endian(out, 16, 4); little_endian(out, 1, 2); little_endian(out, 2, 2);
    little_endian(out, rate, 4); little_endian(out, rate * 4, 4);
    little_endian(out, 4, 2); little_endian(out, 16, 2);
    out.write("data", 4); little_endian(out, bytes, 4);
    for (auto sample : output) little_endian(out, static_cast<std::uint16_t>(sample), 2);
    out.close();
    return out ? 0 : 4;
}
