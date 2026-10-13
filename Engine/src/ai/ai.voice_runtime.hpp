// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace epochengine::ai::voice_runtime {
struct Snapshot final {
    bool probed{},available{},speech_available{},listening{},speaking{};
    std::uint64_t owner{},generation{};
    std::string status{};
};
struct Transcript final {std::uint64_t sequence{},owner{},generation{};std::string text{};};
// Probe enumerates installed OS providers without opening an input device.
void probe();
[[nodiscard]] Snapshot snapshot();
[[nodiscard]] bool start(std::uint64_t owner);
void stop(std::uint64_t owner);
[[nodiscard]] bool speak(std::uint64_t owner,std::string text);
[[nodiscard]] std::vector<Transcript> transcripts_since(std::uint64_t sequence,std::uint64_t owner);
}
