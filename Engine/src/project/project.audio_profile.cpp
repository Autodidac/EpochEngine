/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module project.audio_profile;

import platform.filesystem;

namespace epochengine::project_audio
{
    namespace fs = std::filesystem;

    namespace
    {
        inline constexpr std::uint32_t format_version{1u};
        inline constexpr std::size_t digest_size{32u};
        inline constexpr std::string_view magic{"EPAUDIO1"};

        class Writer final
        {
        public:
            void u8(std::uint8_t value)
            {
                bytes_.push_back(static_cast<std::byte>(value));
            }

            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift{}; shift < 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift{}; shift < 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void f32(float value)
            {
                u32(std::bit_cast<std::uint32_t>(value));
            }

            void text(std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                for (const char character : value)
                    u8(static_cast<std::uint8_t>(character));
            }

            [[nodiscard]] std::vector<std::byte> release() &&
            {
                return std::move(bytes_);
            }

        private:
            std::vector<std::byte> bytes_{};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool ok() const noexcept { return valid_; }
            [[nodiscard]] bool finished() const noexcept
            {
                return valid_ && cursor_ == bytes_.size();
            }

            [[nodiscard]] std::uint8_t u8() noexcept
            {
                if (cursor_ >= bytes_.size())
                {
                    valid_ = false;
                    return 0u;
                }
                return std::to_integer<std::uint8_t>(bytes_[cursor_++]);
            }

            [[nodiscard]] std::uint32_t u32() noexcept
            {
                std::uint32_t value{};
                for (std::uint32_t shift{}; shift < 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] std::uint64_t u64() noexcept
            {
                std::uint64_t value{};
                for (std::uint32_t shift{}; shift < 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] float f32() noexcept
            {
                return std::bit_cast<float>(u32());
            }

            [[nodiscard]] std::optional<std::string> text(
                std::uint32_t maximumBytes)
            {
                const std::uint32_t size = u32();
                if (!valid_ || size > maximumBytes
                    || size > bytes_.size() - cursor_)
                {
                    valid_ = false;
                    return std::nullopt;
                }
                std::string value(size, '\0');
                for (std::uint32_t index{}; index < size; ++index)
                    value[index] = static_cast<char>(u8());
                return valid_
                    ? std::optional<std::string>{std::move(value)}
                    : std::nullopt;
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t cursor_{};
            bool valid_{true};
        };

        [[nodiscard]] bool finite_gain(float gain) noexcept
        {
            return std::isfinite(gain) && gain >= 0.0f && gain <= 16.0f;
        }

        [[nodiscard]] char ascii_fold(char value) noexcept
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value + ('a' - 'A'))
                : value;
        }

        [[nodiscard]] bool portable_equal(
            std::string_view left,
            std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;
            for (std::size_t index{}; index < left.size(); ++index)
            {
                if (ascii_fold(left[index]) != ascii_fold(right[index]))
                    return false;
            }
            return true;
        }

        [[nodiscard]] const BusSource* find_bus(
            const ProfileSource& source,
            BusId id) noexcept
        {
            const auto found = std::find_if(
                source.buses.begin(),
                source.buses.end(),
                [id](const BusSource& bus) noexcept
                {
                    return bus.id == id;
                });
            return found == source.buses.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool bus_has_cycle(
            const ProfileSource& source,
            const BusSource& start) noexcept
        {
            BusId cursor = start.parent;
            for (std::size_t depth{}; depth <= source.buses.size(); ++depth)
            {
                if (!cursor)
                    return false;
                if (cursor == start.id)
                    return true;
                const BusSource* const parent = find_bus(source, cursor);
                if (!parent)
                    return false;
                cursor = parent->parent;
            }
            return true;
        }

        [[nodiscard]] std::uint32_t bus_depth(
            const ProfileSource& source,
            const BusSource& bus) noexcept
        {
            std::uint32_t depth{};
            BusId cursor = bus.parent;
            while (cursor)
            {
                ++depth;
                const BusSource* const parent = find_bus(source, cursor);
                if (!parent)
                    break;
                cursor = parent->parent;
            }
            return depth;
        }

        [[nodiscard]] std::vector<std::byte> encode_payload(
            const ProfileSource& source)
        {
            Writer writer{};
            for (const char character : magic)
                writer.u8(static_cast<std::uint8_t>(character));
            writer.u32(format_version);
            writer.u64(source.profile_id);
            writer.u64(source.sequence);
            writer.u32(static_cast<std::uint32_t>(source.buses.size()));
            writer.u32(static_cast<std::uint32_t>(source.cues.size()));
            for (const BusSource& bus : source.buses)
            {
                writer.u64(bus.id.value);
                writer.u64(bus.parent.value);
                writer.text(bus.name);
                writer.f32(bus.gain);
                writer.u8(bus.muted ? 1u : 0u);
            }
            for (const CueSource& cue : source.cues)
            {
                writer.u64(cue.id.value);
                writer.u8(static_cast<std::uint8_t>(cue.semantic));
                writer.text(cue.name);
                writer.text(cue.logical_audio_path);
                writer.u64(cue.bus.value);
                writer.f32(cue.gain);
                writer.u8(cue.looping ? 1u : 0u);
                writer.u8(cue.autoplay ? 1u : 0u);
            }
            return std::move(writer).release();
        }

        inline constexpr std::string_view artifact_magic{"EPAUDC01"};

        [[nodiscard]] constexpr std::uint64_t channel_count(
            audio::PcmChannelLayout layout) noexcept
        {
            switch (layout)
            {
            case audio::PcmChannelLayout::mono: return 1u;
            case audio::PcmChannelLayout::stereo: return 2u;
            case audio::PcmChannelLayout::surround_5_1: return 6u;
            }
            return 0u;
        }

        [[nodiscard]] bool compiled_artifact_valid(
            const CompiledSession& artifact,
            std::uint64_t expectedProjectKey,
            const ProfileLimits& limits,
            const project_assets::RegistryLimits& registryLimits) noexcept
        {
            if (!limits.valid() || !registryLimits.valid()
                || expectedProjectKey == 0u
                || artifact.code != CompileCode::ready
                || artifact.project_key != expectedProjectKey
                || artifact.profile_id == 0u
                || artifact.source_sequence == 0u
                || !artifact.source_digest.valid()
                || artifact.request.buses.size() > limits.maximum_buses
                || artifact.request.cues.empty()
                || artifact.request.cues.size() > limits.maximum_cues
                || artifact.request.cues.size() != artifact.bindings.size())
            {
                return false;
            }

            const std::uint64_t expectedArtifactKey =
                project_assets::stable_asset_identity(
                    expectedProjectKey,
                    project_assets::AssetKind::audio,
                    canonical_source_path);
            if (expectedArtifactKey == 0u
                || artifact.artifact_key != expectedArtifactKey
                || artifact.request.stable_session_id != expectedArtifactKey)
            {
                return false;
            }

            std::vector<audio::LogicalResourceId> buses{};
            buses.reserve(artifact.request.buses.size());
            for (const auto& bus : artifact.request.buses)
            {
                if (!bus.id.valid()
                    || bus.id.domain != expectedProjectKey
                    || !finite_gain(bus.gain)
                    || std::find(buses.begin(), buses.end(), bus.id)
                        != buses.end())
                {
                    return false;
                }
                if (bus.parent.valid()
                    && (bus.parent.domain != expectedProjectKey
                        || std::find(
                            buses.begin(), buses.end(), bus.parent)
                            == buses.end()))
                {
                    return false;
                }
                buses.push_back(bus.id);
            }

            std::uint64_t decodedBytes{};
            std::vector<CueId> cueIds{};
            std::vector<CueSemantic> semantics{};
            std::vector<std::string_view> logicalPaths{};
            cueIds.reserve(artifact.bindings.size());
            semantics.reserve(artifact.bindings.size());
            logicalPaths.reserve(artifact.bindings.size());

            for (std::size_t index{}; index < artifact.request.cues.size();
                 ++index)
            {
                const auto& cue = artifact.request.cues[index];
                const CueBinding& binding = artifact.bindings[index];
                const std::uint64_t channels =
                    channel_count(cue.clip.format.channel_layout);
                if (!cue.clip.id.valid()
                    || cue.clip.id.domain != expectedProjectKey
                    || cue.clip.format.sample_format
                        != audio::PcmSampleFormat::float32_interleaved
                    || channels == 0u || channels > limits.import.maximum_channels
                    || cue.clip.format.sample_rate
                        < limits.import.minimum_sample_rate
                    || cue.clip.format.sample_rate
                        > limits.import.maximum_sample_rate
                    || cue.clip.frame_count == 0u
                    || cue.clip.frame_count > limits.import.maximum_frames
                    || cue.clip.frame_count
                        > (std::numeric_limits<std::uint64_t>::max)() / channels)
                {
                    return false;
                }

                const std::uint64_t expectedSamples =
                    cue.clip.frame_count * channels;
                if (expectedSamples != cue.clip.interleaved_samples.size()
                    || expectedSamples
                        > limits.maximum_total_decoded_bytes / sizeof(float)
                    || !finite_gain(cue.gain))
                {
                    return false;
                }
                if (cue.bus.valid()
                    && (cue.bus.domain != expectedProjectKey
                        || std::find(buses.begin(), buses.end(), cue.bus)
                            == buses.end()))
                {
                    return false;
                }
                for (const float sample : cue.clip.interleaved_samples)
                {
                    if (!std::isfinite(sample)
                        || sample < -1.0001f || sample > 1.0001f)
                    {
                        return false;
                    }
                }

                const std::uint64_t clipBytes =
                    expectedSamples * sizeof(float);
                if (decodedBytes > limits.maximum_total_decoded_bytes
                        - clipBytes)
                {
                    return false;
                }
                decodedBytes += clipBytes;

                if (!binding.source_id
                    || binding.semantic > CueSemantic::user_interface
                    || binding.runtime_clip != cue.clip.id
                    || binding.audio_asset_key == 0u
                    || binding.audio_asset_key != cue.clip.id.value
                    || binding.looping != cue.looping)
                {
                    return false;
                }
                const auto canonical = project_assets::canonical_logical_path(
                    binding.logical_audio_path, registryLimits);
                if (!canonical || *canonical != binding.logical_audio_path
                    || project_assets::stable_asset_identity(
                        expectedProjectKey,
                        project_assets::AssetKind::audio,
                        *canonical) != binding.audio_asset_key)
                {
                    return false;
                }
                if (std::find(
                        cueIds.begin(), cueIds.end(), binding.source_id)
                        != cueIds.end())
                {
                    return false;
                }
                for (const std::string_view existing : logicalPaths)
                {
                    if (portable_equal(
                            existing, binding.logical_audio_path))
                    {
                        return false;
                    }
                }
                if (binding.semantic != CueSemantic::custom
                    && std::find(
                        semantics.begin(), semantics.end(), binding.semantic)
                        != semantics.end())
                {
                    return false;
                }
                cueIds.push_back(binding.source_id);
                logicalPaths.push_back(binding.logical_audio_path);
                if (binding.semantic != CueSemantic::custom)
                    semantics.push_back(binding.semantic);
            }
            return decodedBytes == artifact.decoded_bytes;
        }

        [[nodiscard]] std::vector<std::byte> encode_artifact_payload(
            const CompiledSession& artifact)
        {
            Writer writer{};
            for (const char character : artifact_magic)
                writer.u8(static_cast<std::uint8_t>(character));
            writer.u32(artifact_schema_version);
            writer.u64(artifact.project_key);
            writer.u64(artifact.artifact_key);
            writer.u64(artifact.profile_id);
            writer.u64(artifact.source_sequence);
            for (const std::uint8_t value : artifact.source_digest.bytes)
                writer.u8(value);
            writer.u64(artifact.request.stable_session_id);
            writer.u64(artifact.decoded_bytes);
            writer.u32(static_cast<std::uint32_t>(
                artifact.request.buses.size()));
            writer.u32(static_cast<std::uint32_t>(
                artifact.request.cues.size()));

            for (const auto& bus : artifact.request.buses)
            {
                writer.u64(bus.id.domain);
                writer.u64(bus.id.value);
                writer.u64(bus.parent.domain);
                writer.u64(bus.parent.value);
                writer.f32(bus.gain);
                writer.u8(bus.muted ? 1u : 0u);
            }

            for (std::size_t index{}; index < artifact.request.cues.size();
                 ++index)
            {
                const auto& cue = artifact.request.cues[index];
                const CueBinding& binding = artifact.bindings[index];
                writer.u64(binding.source_id.value);
                writer.u8(static_cast<std::uint8_t>(binding.semantic));
                writer.text(binding.logical_audio_path);
                writer.u64(binding.audio_asset_key);
                writer.u64(binding.runtime_clip.domain);
                writer.u64(binding.runtime_clip.value);
                writer.u8(binding.autoplay ? 1u : 0u);
                writer.u8(binding.looping ? 1u : 0u);

                writer.u64(cue.clip.id.domain);
                writer.u64(cue.clip.id.value);
                writer.u8(static_cast<std::uint8_t>(
                    cue.clip.format.sample_format));
                writer.u8(static_cast<std::uint8_t>(
                    cue.clip.format.channel_layout));
                writer.u32(cue.clip.format.sample_rate);
                writer.u64(cue.clip.frame_count);
                writer.u64(cue.clip.interleaved_samples.size());
                for (const float sample : cue.clip.interleaved_samples)
                    writer.f32(sample);
                writer.u64(cue.bus.domain);
                writer.u64(cue.bus.value);
                writer.f32(cue.gain);
                writer.u8(cue.looping ? 1u : 0u);
            }
            return std::move(writer).release();
        }
        [[nodiscard]] core::sha256::Digest digest_of(
            std::span<const std::byte> bytes) noexcept
        {
            return core::sha256::hash(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(bytes.data()),
                bytes.size()});
        }

        [[nodiscard]] bool path_is_within(
            const fs::path& root,
            const fs::path& candidate) noexcept
        {
            auto rootIt = root.begin();
            auto candidateIt = candidate.begin();
            for (; rootIt != root.end(); ++rootIt, ++candidateIt)
            {
                if (candidateIt == candidate.end())
                    return false;
#if defined(_WIN32)
                if (!portable_equal(
                        rootIt->generic_string(),
                        candidateIt->generic_string()))
                    return false;
#else
                if (*rootIt != *candidateIt)
                    return false;
#endif
            }
            return true;
        }

        [[nodiscard]] fs::path temporary_path_for(
            const fs::path& destination)
        {
            static std::atomic<std::uint64_t> next{1u};
            fs::path temporary = destination;
            temporary += ".tmp.";
            temporary += std::to_string(
                next.fetch_add(1u, std::memory_order_relaxed));
            return temporary;
        }

        [[nodiscard]] bool read_file(
            const fs::path& path,
            std::uint64_t maximumBytes,
            std::vector<std::byte>& bytes,
            std::error_code& error)
        {
            error.clear();
            const std::uintmax_t size = fs::file_size(path, error);
            if (error || size > maximumBytes
                || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
                return false;
            bytes.resize(static_cast<std::size_t>(size));
            std::ifstream input(path, std::ios::binary);
            return input
                && static_cast<bool>(input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())));
        }

        [[nodiscard]] BusSource* find_bus(
            ProfileSource& source,
            BusId id) noexcept
        {
            const auto found = std::find_if(
                source.buses.begin(), source.buses.end(),
                [id](const BusSource& bus) noexcept
                {
                    return bus.id == id;
                });
            return found == source.buses.end() ? nullptr : &*found;
        }

        [[nodiscard]] CueSource* find_cue(
            ProfileSource& source,
            CueId id) noexcept
        {
            const auto found = std::find_if(
                source.cues.begin(), source.cues.end(),
                [id](const CueSource& cue) noexcept
                {
                    return cue.id == id;
                });
            return found == source.cues.end() ? nullptr : &*found;
        }

        [[nodiscard]] ProfileEditResult commit_profile_edit(
            ProfileSource source,
            const ProfileLimits& limits,
            const project_assets::RegistryLimits& registryLimits) noexcept
        {
            if (source.sequence
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return {
                    ProfileEditCode::revision_exhausted,
                    ValidationCode::invalid_profile,
                    {}};
            }
            ++source.sequence;
            const ValidationCode validation = validate_profile(
                source, limits, registryLimits);
            if (validation != ValidationCode::ready)
            {
                return {
                    ProfileEditCode::validation_failed,
                    validation,
                    {}};
            }
            return {
                ProfileEditCode::ready,
                ValidationCode::ready,
                std::move(source)};
        }

        [[nodiscard]] std::string portable_audio_stem(
            std::string_view stem)
        {
            std::string result{};
            result.reserve((std::min)(stem.size(), std::size_t{80u}));
            bool previousUnderscore{};
            for (const char character : stem)
            {
                const bool alphaNumeric =
                    (character >= 'a' && character <= 'z')
                    || (character >= 'A' && character <= 'Z')
                    || (character >= '0' && character <= '9');
                if (alphaNumeric || character == '-' || character == '_')
                {
                    result.push_back(character);
                    previousUnderscore = character == '_';
                }
                else if (!result.empty() && !previousUnderscore)
                {
                    result.push_back('_');
                    previousUnderscore = true;
                }
                if (result.size() == 80u)
                    break;
            }
            while (!result.empty() && result.back() == '_')
                result.pop_back();
            return result.empty() ? std::string{"audio"} : result;
        }

        [[nodiscard]] bool wave_extension(std::string extension) noexcept
        {
            for (char& character : extension)
            {
                if (character >= 'A' && character <= 'Z')
                    character = static_cast<char>(character + ('a' - 'A'));
            }
            return extension == ".wav" || extension == ".wave";
        }
    }


    ProfileSource make_default_2d_profile() noexcept
    {
        try
        {
            return {
                .profile_id = 0x4550'4f43'485f'3244ull,
                .sequence = 1u,
                .buses = {
                    {
                        .id = {1u},
                        .name = "Effects",
                        .gain = 0.85f
                    },
                    {
                        .id = {2u},
                        .name = "Music",
                        .gain = 0.55f
                    },
                    {
                        .id = {3u},
                        .parent = {2u},
                        .name = "Ambient",
                        .gain = 0.75f
                    }
                },
                .cues = {
                    {
                        .id = {1u},
                        .semantic = CueSemantic::jump,
                        .name = "Actor Jump",
                        .logical_audio_path =
                            "Assets/Audio/default_jump.wav",
                        .bus = {1u},
                        .gain = 0.9f
                    },
                    {
                        .id = {2u},
                        .semantic = CueSemantic::land,
                        .name = "Actor Land",
                        .logical_audio_path =
                            "Assets/Audio/default_land.wav",
                        .bus = {1u},
                        .gain = 0.9f
                    },
                    {
                        .id = {3u},
                        .semantic = CueSemantic::ambient,
                        .name = "World Ambient",
                        .logical_audio_path =
                            "Assets/Audio/default_ambient.wav",
                        .bus = {3u},
                        .gain = 0.35f,
                        .looping = true,
                        .autoplay = true
                    }
                }
            };
        }
        catch (...)
        {
            return {};
        }
    }

    ValidationCode validate_profile(
        const ProfileSource& source,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (!limits.valid() || !registryLimits.valid())
            return ValidationCode::invalid_limits;
        if (source.profile_id == 0u || source.sequence == 0u
            || source.cues.empty())
            return ValidationCode::invalid_profile;
        if (source.buses.size() > limits.maximum_buses)
            return ValidationCode::bus_limit_exceeded;
        if (source.cues.size() > limits.maximum_cues)
            return ValidationCode::cue_limit_exceeded;

        try
        {
            for (std::size_t index{}; index < source.buses.size(); ++index)
            {
                const BusSource& bus = source.buses[index];
                if (!bus.id || bus.parent == bus.id || bus.name.empty()
                    || bus.name.size() > limits.maximum_name_bytes
                    || !finite_gain(bus.gain))
                    return ValidationCode::invalid_bus;

                for (std::size_t previous{}; previous < index; ++previous)
                {
                    if (source.buses[previous].id == bus.id
                        || portable_equal(source.buses[previous].name, bus.name))
                        return ValidationCode::duplicate_bus;
                }
                if (bus.parent && !find_bus(source, bus.parent))
                    return ValidationCode::missing_parent;
                if (bus_has_cycle(source, bus))
                    return ValidationCode::bus_cycle;
            }

            for (std::size_t index{}; index < source.cues.size(); ++index)
            {
                const CueSource& cue = source.cues[index];
                if (static_cast<std::uint8_t>(cue.semantic)
                        > static_cast<std::uint8_t>(
                            CueSemantic::user_interface)
                    || !cue.id || cue.name.empty()
                    || cue.name.size() > limits.maximum_name_bytes
                    || cue.logical_audio_path.empty()
                    || cue.logical_audio_path.size() > limits.maximum_path_bytes
                    || !finite_gain(cue.gain))
                    return ValidationCode::invalid_cue;

                for (std::size_t previous{}; previous < index; ++previous)
                {
                    if (source.cues[previous].id == cue.id
                        || portable_equal(source.cues[previous].name, cue.name))
                        return ValidationCode::duplicate_cue;
                    if (cue.semantic != CueSemantic::custom
                        && source.cues[previous].semantic == cue.semantic)
                        return ValidationCode::duplicate_semantic;
                    if (portable_equal(
                            source.cues[previous].logical_audio_path,
                            cue.logical_audio_path))
                        return ValidationCode::duplicate_audio_path;
                }
                if (cue.bus && !find_bus(source, cue.bus))
                    return ValidationCode::missing_bus;

                const auto canonical = project_assets::canonical_logical_path(
                    cue.logical_audio_path, registryLimits);
                if (!canonical || *canonical != cue.logical_audio_path)
                    return ValidationCode::invalid_path;
            }
            return ValidationCode::ready;
        }
        catch (...)
        {
            return ValidationCode::invalid_profile;
        }
    }

    ProfileEditResult set_bus_mix(
        ProfileSource source,
        BusId bus,
        float gain,
        bool muted,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        try
        {
            const ValidationCode sourceCode = validate_profile(
                source, limits, registryLimits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (!finite_gain(gain))
            {
                return {
                    ProfileEditCode::invalid_gain,
                    ValidationCode::invalid_bus,
                    {}};
            }
            BusSource* const target = find_bus(source, bus);
            if (!target)
            {
                return {
                    ProfileEditCode::invalid_bus,
                    ValidationCode::invalid_bus,
                    {}};
            }
            if (target->gain == gain && target->muted == muted)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            target->gain = gain;
            target->muted = muted;
            return commit_profile_edit(
                std::move(source), limits, registryLimits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::invalid_profile,
                {}};
        }
    }

    ProfileEditResult configure_cue(
        ProfileSource source,
        CueId cue,
        CueSemantic semantic,
        BusId bus,
        float gain,
        bool looping,
        bool autoplay,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        try
        {
            const ValidationCode sourceCode = validate_profile(
                source, limits, registryLimits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (!finite_gain(gain))
            {
                return {
                    ProfileEditCode::invalid_gain,
                    ValidationCode::invalid_cue,
                    {}};
            }
            if (static_cast<std::uint8_t>(semantic)
                    > static_cast<std::uint8_t>(
                        CueSemantic::user_interface)
                || (bus && !find_bus(source, bus)))
            {
                return {
                    ProfileEditCode::invalid_cue,
                    bus ? ValidationCode::missing_bus
                        : ValidationCode::invalid_cue,
                    {}};
            }
            CueSource* const target = find_cue(source, cue);
            if (!target)
            {
                return {
                    ProfileEditCode::invalid_cue,
                    ValidationCode::invalid_cue,
                    {}};
            }
            if (target->semantic == semantic
                && target->bus == bus
                && target->gain == gain
                && target->looping == looping
                && target->autoplay == autoplay)
            {
                return {
                    ProfileEditCode::unchanged,
                    ValidationCode::ready,
                    std::move(source)};
            }
            target->semantic = semantic;
            target->bus = bus;
            target->gain = gain;
            target->looping = looping;
            target->autoplay = autoplay;
            return commit_profile_edit(
                std::move(source), limits, registryLimits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::invalid_profile,
                {}};
        }
    }

    ProfileEditResult add_cue(
        ProfileSource source,
        CueSource cue,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        try
        {
            const ValidationCode sourceCode = validate_profile(
                source, limits, registryLimits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            if (source.cues.size() >= limits.maximum_cues)
            {
                return {
                    ProfileEditCode::validation_failed,
                    ValidationCode::cue_limit_exceeded,
                    {}};
            }
            if (!cue.id)
            {
                std::uint64_t maximum{};
                for (const CueSource& existing : source.cues)
                    maximum = (std::max)(maximum, existing.id.value);
                if (maximum
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    return {
                        ProfileEditCode::id_exhausted,
                        ValidationCode::invalid_cue,
                        {}};
                }
                cue.id.value = maximum + 1u;
            }
            source.cues.push_back(std::move(cue));
            return commit_profile_edit(
                std::move(source), limits, registryLimits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::invalid_profile,
                {}};
        }
    }

    ProfileEditResult remove_cue(
        ProfileSource source,
        CueId cue,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        try
        {
            const ValidationCode sourceCode = validate_profile(
                source, limits, registryLimits);
            if (sourceCode != ValidationCode::ready)
                return {ProfileEditCode::invalid_source, sourceCode, {}};
            const auto found = std::find_if(
                source.cues.begin(), source.cues.end(),
                [cue](const CueSource& candidate) noexcept
                {
                    return candidate.id == cue;
                });
            if (found == source.cues.end())
            {
                return {
                    ProfileEditCode::invalid_cue,
                    ValidationCode::invalid_cue,
                    {}};
            }
            source.cues.erase(found);
            return commit_profile_edit(
                std::move(source), limits, registryLimits);
        }
        catch (...)
        {
            return {
                ProfileEditCode::allocation_failure,
                ValidationCode::invalid_profile,
                {}};
        }
    }

    core::sha256::Digest profile_digest(
        const ProfileSource& source,
        const ProfileLimits& limits) noexcept
    {
        if (validate_profile(source, limits) != ValidationCode::ready)
            return {};
        try
        {
            const std::vector<std::byte> payload = encode_payload(source);
            return digest_of(payload);
        }
        catch (...)
        {
            return {};
        }
    }

    SerializedProfile serialize_profile(
        const ProfileSource& source,
        const ProfileLimits& limits) noexcept
    {
        if (validate_profile(source, limits) != ValidationCode::ready)
            return {.code = CodecCode::invalid_source};

        try
        {
            std::vector<std::byte> bytes = encode_payload(source);
            if (bytes.size() + digest_size > limits.maximum_serialized_bytes)
                return {.code = CodecCode::size_limit_exceeded};

            const core::sha256::Digest digest = digest_of(bytes);
            bytes.reserve(bytes.size() + digest.bytes.size());
            for (const std::uint8_t value : digest.bytes)
                bytes.push_back(static_cast<std::byte>(value));
            return {
                .code = CodecCode::ready,
                .digest = digest,
                .bytes = std::move(bytes)};
        }
        catch (const std::bad_alloc&)
        {
            return {.code = CodecCode::allocation_failure};
        }
        catch (...)
        {
            return {.code = CodecCode::invalid_source};
        }
    }

    DeserializedProfile deserialize_profile(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        constexpr std::size_t fixedBytes = 8u + 4u + 8u + 8u + 4u + 4u;
        if (!limits.valid() || !registryLimits.valid())
            return {.code = CodecCode::invalid_source};
        if (bytes.size() < fixedBytes + digest_size)
            return {.code = CodecCode::malformed_data};
        if (bytes.size() > limits.maximum_serialized_bytes)
            return {.code = CodecCode::size_limit_exceeded};

        try
        {
            const std::span<const std::byte> payload =
                bytes.first(bytes.size() - digest_size);
            core::sha256::Digest stored{};
            for (std::size_t index{}; index < stored.bytes.size(); ++index)
            {
                stored.bytes[index] = std::to_integer<std::uint8_t>(
                    bytes[payload.size() + index]);
            }
            const core::sha256::Digest computed = digest_of(payload);
            if (stored != computed)
                return {.code = CodecCode::integrity_failure};

            Reader reader{payload};
            for (const char expected : magic)
            {
                if (reader.u8() != static_cast<std::uint8_t>(expected))
                    return {.code = CodecCode::malformed_data};
            }
            if (reader.u32() != format_version)
                return {.code = CodecCode::version_unsupported};

            ProfileSource source{};
            source.profile_id = reader.u64();
            source.sequence = reader.u64();
            const std::uint32_t busCount = reader.u32();
            const std::uint32_t cueCount = reader.u32();
            if (!reader.ok()
                || busCount > limits.maximum_buses
                || cueCount > limits.maximum_cues)
                return {.code = CodecCode::size_limit_exceeded};

            source.buses.reserve(busCount);
            source.cues.reserve(cueCount);
            for (std::uint32_t index{}; index < busCount; ++index)
            {
                BusSource bus{};
                bus.id.value = reader.u64();
                bus.parent.value = reader.u64();
                auto name = reader.text(limits.maximum_name_bytes);
                bus.gain = reader.f32();
                bus.muted = reader.u8() != 0u;
                if (!name || !reader.ok())
                    return {.code = CodecCode::malformed_data};
                bus.name = std::move(*name);
                source.buses.push_back(std::move(bus));
            }

            for (std::uint32_t index{}; index < cueCount; ++index)
            {
                CueSource cue{};
                cue.id.value = reader.u64();
                cue.semantic = static_cast<CueSemantic>(reader.u8());
                auto name = reader.text(limits.maximum_name_bytes);
                auto path = reader.text(limits.maximum_path_bytes);
                cue.bus.value = reader.u64();
                cue.gain = reader.f32();
                cue.looping = reader.u8() != 0u;
                cue.autoplay = reader.u8() != 0u;
                if (!name || !path || !reader.ok())
                    return {.code = CodecCode::malformed_data};
                cue.name = std::move(*name);
                cue.logical_audio_path = std::move(*path);
                source.cues.push_back(std::move(cue));
            }

            if (!reader.finished())
                return {.code = CodecCode::malformed_data};
            if (validate_profile(source, limits, registryLimits)
                != ValidationCode::ready)
                return {.code = CodecCode::invalid_source};

            return {
                .code = CodecCode::ready,
                .digest = computed,
                .source = std::move(source)};
        }
        catch (const std::bad_alloc&)
        {
            return {.code = CodecCode::allocation_failure};
        }
        catch (...)
        {
            return {.code = CodecCode::malformed_data};
        }
    }

    SerializedAudioArtifact serialize_audio_artifact(
        const CompiledSession& artifact,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (!compiled_artifact_valid(
                artifact, artifact.project_key, limits, registryLimits))
        {
            return {.code = CodecCode::invalid_source};
        }

        try
        {
            std::vector<std::byte> bytes =
                encode_artifact_payload(artifact);
            if (bytes.size() + digest_size > limits.maximum_artifact_bytes)
                return {.code = CodecCode::size_limit_exceeded};

            const core::sha256::Digest digest = digest_of(bytes);
            bytes.reserve(bytes.size() + digest.bytes.size());
            for (const std::uint8_t value : digest.bytes)
                bytes.push_back(static_cast<std::byte>(value));
            return {
                .code = CodecCode::ready,
                .digest = digest,
                .bytes = std::move(bytes)};
        }
        catch (const std::bad_alloc&)
        {
            return {.code = CodecCode::allocation_failure};
        }
        catch (...)
        {
            return {.code = CodecCode::invalid_source};
        }
    }

    DeserializedAudioArtifact deserialize_audio_artifact(
        std::span<const std::byte> bytes,
        std::string_view projectId,
        bool requestPhysicalOutput,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        constexpr std::size_t fixedBytes =
            8u + 4u + 8u + 8u + 8u + 8u + 32u + 8u + 8u + 4u + 4u;
        if (!limits.valid() || !registryLimits.valid() || projectId.empty())
            return {.code = CodecCode::invalid_source};
        if (bytes.size() < fixedBytes + digest_size)
            return {.code = CodecCode::malformed_data};
        if (bytes.size() > limits.maximum_artifact_bytes)
            return {.code = CodecCode::size_limit_exceeded};

        try
        {
            const std::span<const std::byte> payload =
                bytes.first(bytes.size() - digest_size);
            core::sha256::Digest stored{};
            for (std::size_t index{}; index < stored.bytes.size(); ++index)
            {
                stored.bytes[index] = std::to_integer<std::uint8_t>(
                    bytes[payload.size() + index]);
            }
            const core::sha256::Digest computed = digest_of(payload);
            if (stored != computed)
                return {.code = CodecCode::integrity_failure};

            Reader reader{payload};
            for (const char expected : artifact_magic)
            {
                if (reader.u8() != static_cast<std::uint8_t>(expected))
                    return {.code = CodecCode::malformed_data};
            }
            if (reader.u32() != artifact_schema_version)
                return {.code = CodecCode::version_unsupported};

            CompiledSession artifact{};
            artifact.code = CompileCode::ready;
            artifact.project_key = reader.u64();
            artifact.artifact_key = reader.u64();
            artifact.profile_id = reader.u64();
            artifact.source_sequence = reader.u64();
            for (std::uint8_t& value : artifact.source_digest.bytes)
                value = reader.u8();
            artifact.request.stable_session_id = reader.u64();
            artifact.decoded_bytes = reader.u64();
            const std::uint32_t busCount = reader.u32();
            const std::uint32_t cueCount = reader.u32();
            if (!reader.ok()
                || busCount > limits.maximum_buses
                || cueCount == 0u
                || cueCount > limits.maximum_cues)
            {
                return {.code = CodecCode::size_limit_exceeded};
            }

            const std::uint64_t expectedProjectKey =
                project_assets::stable_project_identity(
                    projectId, registryLimits);
            if (expectedProjectKey == 0u
                || artifact.project_key != expectedProjectKey)
            {
                return {.code = CodecCode::invalid_source};
            }

            artifact.request.request_physical_output =
                requestPhysicalOutput;
            artifact.request.buses.reserve(busCount);
            artifact.request.cues.reserve(cueCount);
            artifact.bindings.reserve(cueCount);
            for (std::uint32_t index{}; index < busCount; ++index)
            {
                audio::PlaybackBusDefinition bus{};
                bus.id.domain = reader.u64();
                bus.id.value = reader.u64();
                bus.parent.domain = reader.u64();
                bus.parent.value = reader.u64();
                bus.gain = reader.f32();
                const std::uint8_t muted = reader.u8();
                if (!reader.ok() || muted > 1u)
                    return {.code = CodecCode::malformed_data};
                bus.muted = muted != 0u;
                artifact.request.buses.push_back(bus);
            }

            std::uint64_t decodedBytes{};
            for (std::uint32_t index{}; index < cueCount; ++index)
            {
                CueBinding binding{};
                binding.source_id.value = reader.u64();
                const std::uint8_t semantic = reader.u8();
                auto logicalPath = reader.text(limits.maximum_path_bytes);
                binding.audio_asset_key = reader.u64();
                binding.runtime_clip.domain = reader.u64();
                binding.runtime_clip.value = reader.u64();
                const std::uint8_t autoplay = reader.u8();
                const std::uint8_t bindingLooping = reader.u8();

                audio::PlaybackCueDefinition cue{};
                cue.clip.id.domain = reader.u64();
                cue.clip.id.value = reader.u64();
                const std::uint8_t sampleFormat = reader.u8();
                const std::uint8_t channelLayout = reader.u8();
                cue.clip.format.sample_rate = reader.u32();
                cue.clip.frame_count = reader.u64();
                const std::uint64_t sampleCount = reader.u64();
                if (!logicalPath || !reader.ok()
                    || semantic > static_cast<std::uint8_t>(
                        CueSemantic::user_interface)
                    || autoplay > 1u || bindingLooping > 1u
                    || sampleFormat != static_cast<std::uint8_t>(
                        audio::PcmSampleFormat::float32_interleaved)
                    || channelLayout > static_cast<std::uint8_t>(
                        audio::PcmChannelLayout::stereo)
                    || sampleCount
                        > limits.maximum_total_decoded_bytes / sizeof(float)
                    || sampleCount
                        > static_cast<std::uint64_t>(
                            (std::numeric_limits<std::size_t>::max)()))
                {
                    return {.code = CodecCode::malformed_data};
                }
                binding.semantic = static_cast<CueSemantic>(semantic);
                binding.logical_audio_path = std::move(*logicalPath);
                binding.autoplay = autoplay != 0u;
                binding.looping = bindingLooping != 0u;
                cue.clip.format.sample_format =
                    static_cast<audio::PcmSampleFormat>(sampleFormat);
                cue.clip.format.channel_layout =
                    static_cast<audio::PcmChannelLayout>(channelLayout);
                cue.clip.interleaved_samples.resize(
                    static_cast<std::size_t>(sampleCount));
                for (float& sample : cue.clip.interleaved_samples)
                    sample = reader.f32();
                cue.bus.domain = reader.u64();
                cue.bus.value = reader.u64();
                cue.gain = reader.f32();
                const std::uint8_t cueLooping = reader.u8();
                if (!reader.ok() || cueLooping > 1u)
                    return {.code = CodecCode::malformed_data};
                cue.looping = cueLooping != 0u;

                const std::uint64_t clipBytes =
                    sampleCount * sizeof(float);
                if (decodedBytes > limits.maximum_total_decoded_bytes
                        - clipBytes)
                {
                    return {.code = CodecCode::size_limit_exceeded};
                }
                decodedBytes += clipBytes;
                artifact.bindings.push_back(std::move(binding));
                artifact.request.cues.push_back(std::move(cue));
            }

            if (!reader.finished()
                || decodedBytes != artifact.decoded_bytes
                || !compiled_artifact_valid(
                    artifact,
                    expectedProjectKey,
                    limits,
                    registryLimits))
            {
                return {.code = CodecCode::invalid_source};
            }
            artifact.diagnostic =
                "project audio Library artifact restored";
            return {
                .code = CodecCode::ready,
                .digest = computed,
                .artifact = std::move(artifact)};
        }
        catch (const std::bad_alloc&)
        {
            return {.code = CodecCode::allocation_failure};
        }
        catch (...)
        {
            return {.code = CodecCode::malformed_data};
        }
    }
    const CueBinding* CompiledSession::find(CueId id) const noexcept
    {
        const auto found = std::find_if(
            bindings.begin(),
            bindings.end(),
            [id](const CueBinding& binding) noexcept
            {
                return binding.source_id == id;
            });
        return found == bindings.end() ? nullptr : &*found;
    }

    const CueBinding* CompiledSession::find(CueSemantic semantic) const noexcept
    {
        const auto found = std::find_if(
            bindings.begin(),
            bindings.end(),
            [semantic](const CueBinding& binding) noexcept
            {
                return binding.semantic == semantic;
            });
        return found == bindings.end() ? nullptr : &*found;
    }

    std::vector<audio::ClipId> CompiledSession::autoplay_clips() const
    {
        std::vector<audio::ClipId> clips{};
        clips.reserve(bindings.size());
        for (const CueBinding& binding : bindings)
        {
            if (binding.autoplay)
                clips.push_back(binding.runtime_clip);
        }
        return clips;
    }

    CompiledSession compile_profile(
        std::string_view projectId,
        const fs::path& projectRoot,
        const ProfileSource& source,
        bool requestPhysicalOutput,
        const ProfileLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        const std::uint64_t projectKey =
            project_assets::stable_project_identity(projectId, registryLimits);
        if (projectKey == 0u || projectRoot.empty())
        {
            return {
                .code = CompileCode::invalid_project,
                .diagnostic = "project audio requires a stable project and root"};
        }

        const ValidationCode validation =
            validate_profile(source, limits, registryLimits);
        if (validation != ValidationCode::ready)
        {
            return {
                .code = CompileCode::invalid_source,
                .diagnostic = std::string{"audio profile validation failed: "}
                    + std::string{validation_code_name(validation)}};
        }

        try
        {
            std::error_code error{};
            fs::path absoluteRoot = fs::absolute(projectRoot, error);
            if (error)
            {
                return {
                    .code = CompileCode::invalid_project,
                    .diagnostic = "project audio root could not be resolved"};
            }
            const fs::path canonicalRoot =
                fs::weakly_canonical(absoluteRoot, error);
            if (error)
            {
                return {
                    .code = CompileCode::invalid_project,
                    .diagnostic = "project audio root could not be canonicalized"};
            }

            CompiledSession compiled{};
            compiled.code = CompileCode::ready;
            compiled.project_key = projectKey;
            compiled.artifact_key =
                project_assets::stable_asset_identity(
                    projectKey,
                    project_assets::AssetKind::audio,
                    canonical_source_path);
            compiled.profile_id = source.profile_id;
            compiled.source_sequence = source.sequence;
            compiled.source_digest = profile_digest(source, limits);
            compiled.request.stable_session_id = compiled.artifact_key;
            compiled.request.request_physical_output = requestPhysicalOutput;
            compiled.request.buses.reserve(source.buses.size());
            compiled.request.cues.reserve(source.cues.size());
            compiled.bindings.reserve(source.cues.size());

            std::vector<const BusSource*> ordered{};
            ordered.reserve(source.buses.size());
            for (const BusSource& bus : source.buses)
                ordered.push_back(&bus);
            std::stable_sort(
                ordered.begin(),
                ordered.end(),
                [&source](
                    const BusSource* left,
                    const BusSource* right) noexcept
                {
                    return bus_depth(source, *left)
                        < bus_depth(source, *right);
                });
            for (const BusSource* bus : ordered)
            {
                compiled.request.buses.push_back({
                    .id = {projectKey, bus->id.value},
                    .parent = bus->parent
                        ? audio::LogicalResourceId{
                            projectKey, bus->parent.value}
                        : audio::LogicalResourceId{},
                    .gain = bus->gain,
                    .muted = bus->muted});
            }

            for (const CueSource& cue : source.cues)
            {
                const auto canonical = project_assets::canonical_logical_path(
                    cue.logical_audio_path, registryLimits);
                if (!canonical)
                {
                    return {
                        .code = CompileCode::invalid_source,
                        .diagnostic = "audio cue path is not canonical"};
                }

                const fs::path sourcePath = fs::weakly_canonical(
                    canonicalRoot / fs::path{*canonical}, error);
                if (error || !fs::is_regular_file(sourcePath, error) || error)
                {
                    return {
                        .code = CompileCode::source_not_found,
                        .diagnostic = std::string{"audio source missing: "}
                            + *canonical};
                }
                if (!path_is_within(canonicalRoot, sourcePath))
                {
                    return {
                        .code = CompileCode::source_outside_project,
                        .diagnostic = std::string{
                            "audio source resolves outside project: "}
                            + *canonical};
                }

                const std::uint64_t assetKey =
                    project_assets::stable_asset_identity(
                        projectKey,
                        project_assets::AssetKind::audio,
                        *canonical);
                const audio::ClipId clipId{projectKey, assetKey};
                const std::uint64_t remaining =
                    limits.maximum_total_decoded_bytes
                    - compiled.decoded_bytes;
                asset::audio::AudioImportLimits importLimits = limits.import;
                importLimits.maximum_decoded_bytes =
                    (std::min)(importLimits.maximum_decoded_bytes, remaining);

                asset::audio::AudioImportResult imported =
                    asset::audio::import_audio_file({
                        .source_path = sourcePath,
                        .clip_id = clipId,
                        .limits = importLimits});
                if (!imported)
                {
                    return {
                        .code = imported.status
                                == asset::audio::AudioImportStatus::
                                    decoded_budget_exceeded
                            ? CompileCode::decoded_budget_exceeded
                            : CompileCode::import_failed,
                        .diagnostic = std::string{"audio import failed for "}
                            + *canonical + ": "
                            + std::string{
                                asset::audio::audio_import_status_name(
                                    imported.status)}};
                }
                if (imported.decoded_bytes > remaining)
                {
                    return {
                        .code = CompileCode::decoded_budget_exceeded,
                        .diagnostic =
                            "project audio decoded budget was exceeded"};
                }

                compiled.decoded_bytes += imported.decoded_bytes;
                compiled.request.cues.push_back({
                    .clip = std::move(imported.clip),
                    .bus = cue.bus
                        ? audio::LogicalResourceId{projectKey, cue.bus.value}
                        : audio::LogicalResourceId{},
                    .gain = cue.gain,
                    .looping = cue.looping});
                compiled.bindings.push_back({
                    .source_id = cue.id,
                    .semantic = cue.semantic,
                    .logical_audio_path = *canonical,
                    .audio_asset_key = assetKey,
                    .runtime_clip = clipId,
                    .autoplay = cue.autoplay,
                    .looping = cue.looping});
            }

            compiled.diagnostic =
                "project audio source compiled to a bounded playback session";
            return compiled;
        }
        catch (const std::bad_alloc&)
        {
            return {
                .code = CompileCode::allocation_failure,
                .diagnostic = "project audio compilation allocation failed"};
        }
        catch (const fs::filesystem_error& error)
        {
            return {
                .code = CompileCode::source_not_found,
                .diagnostic = error.what()};
        }
        catch (...)
        {
            return {
                .code = CompileCode::invalid_source,
                .diagnostic = "project audio compilation failed"};
        }
    }

    ProjectAudioProfileStore::ProjectAudioProfileStore(
        std::string projectId,
        fs::path projectRoot,
        ProfileLimits limits,
        project_assets::RegistryLimits registryLimits) noexcept
        : project_id_(std::move(projectId))
        , project_root_(std::move(projectRoot))
        , limits_(limits)
        , registry_limits_(registryLimits)
        , project_key_(project_assets::stable_project_identity(
            project_id_, registry_limits_))
    {
        std::error_code error{};
        project_root_ = fs::absolute(project_root_, error).lexically_normal();
        if (error)
            project_root_.clear();
    }

    bool ProjectAudioProfileStore::valid() const noexcept
    {
        return project_key_ != 0u
            && !project_root_.empty()
            && limits_.valid()
            && registry_limits_.valid();
    }

    std::string_view ProjectAudioProfileStore::project_id() const noexcept
    {
        return project_id_;
    }

    std::uint64_t ProjectAudioProfileStore::project_key() const noexcept
    {
        return project_key_;
    }

    const fs::path& ProjectAudioProfileStore::project_root() const noexcept
    {
        return project_root_;
    }

    fs::path ProjectAudioProfileStore::source_path() const
    {
        return project_root_ / fs::path{canonical_source_path};
    }

    fs::path ProjectAudioProfileStore::artifact_path() const
    {
        return project_root_ / fs::path{canonical_artifact_path};
    }

    const ProfileLimits& ProjectAudioProfileStore::limits() const noexcept
    {
        return limits_;
    }

    StoreMetrics ProjectAudioProfileStore::metrics() const noexcept
    {
        return metrics_;
    }

    StoredProfile ProjectAudioProfileStore::reject_store(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    LoadedProfile ProjectAudioProfileStore::reject_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    StoredAudioArtifact ProjectAudioProfileStore::reject_artifact(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    LoadedAudioArtifact ProjectAudioProfileStore::reject_artifact_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    StoredProfile ProjectAudioProfileStore::save(
        const ProfileSource& source) noexcept
    {
        ++metrics_.save_requests;
        if (!valid())
            return reject_store(StoreCode::invalid_store);

        const SerializedProfile serialized = serialize_profile(source, limits_);
        if (!serialized)
            return reject_store(StoreCode::invalid_source);

        try
        {
            const fs::path destination = source_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error)
                return reject_store(StoreCode::directory_failure);

            if (fs::is_regular_file(destination, error) && !error)
            {
                std::vector<std::byte> existing{};
                if (read_file(
                        destination,
                        limits_.maximum_serialized_bytes,
                        existing,
                        error)
                    && existing == serialized.bytes)
                {
                    ++metrics_.unchanged_writes;
                    return {
                        .code = StoreCode::unchanged,
                        .path = destination,
                        .digest = serialized.digest,
                        .serialized_bytes = serialized.bytes.size()};
                }
                error.clear();
            }

            const fs::path temporary = temporary_path_for(destination);
            if (!platform::filesystem::exclusive_create_and_write(
                    temporary, serialized.bytes, error))
                return reject_store(StoreCode::write_failure);

            std::vector<std::byte> verified{};
            if (!read_file(
                    temporary,
                    limits_.maximum_serialized_bytes,
                    verified,
                    error))
            {
                fs::remove(temporary, error);
                return reject_store(StoreCode::read_failure);
            }
            const DeserializedProfile decoded = deserialize_profile(
                verified, limits_, registry_limits_);
            if (!decoded || decoded.source != source
                || decoded.digest != serialized.digest)
            {
                fs::remove(temporary, error);
                return reject_store(StoreCode::integrity_failure);
            }

            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                fs::remove(temporary, error);
                return reject_store(StoreCode::atomic_replace_failure);
            }

            ++metrics_.saves;
            metrics_.bytes_written += serialized.bytes.size();
            return {
                .code = StoreCode::ready,
                .path = destination,
                .digest = serialized.digest,
                .serialized_bytes = serialized.bytes.size()};
        }
        catch (const std::bad_alloc&)
        {
            return reject_store(StoreCode::allocation_failure);
        }
        catch (...)
        {
            return reject_store(StoreCode::write_failure);
        }
    }

    LoadedProfile ProjectAudioProfileStore::load() noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(StoreCode::invalid_store);

        try
        {
            const fs::path path = source_path();
            std::error_code error{};
            if (!fs::is_regular_file(path, error) || error)
                return reject_load(StoreCode::not_found);

            std::vector<std::byte> bytes{};
            if (!read_file(
                    path,
                    limits_.maximum_serialized_bytes,
                    bytes,
                    error))
            {
                return reject_load(error
                    ? StoreCode::read_failure
                    : StoreCode::size_limit_exceeded);
            }

            const DeserializedProfile decoded = deserialize_profile(
                bytes, limits_, registry_limits_);
            if (!decoded)
                return reject_load(StoreCode::integrity_failure);

            ++metrics_.loads;
            metrics_.bytes_read += bytes.size();
            return {
                .code = StoreCode::ready,
                .path = path,
                .digest = decoded.digest,
                .source = decoded.source};
        }
        catch (const std::bad_alloc&)
        {
            return reject_load(StoreCode::allocation_failure);
        }
        catch (...)
        {
            return reject_load(StoreCode::read_failure);
        }
    }

    StoredAudioArtifact ProjectAudioProfileStore::publish_artifact(
        const CompiledSession& artifact) noexcept
    {
        ++metrics_.artifact_save_requests;
        if (!valid())
            return reject_artifact(StoreCode::invalid_store);
        if (artifact.project_key != project_key_)
            return reject_artifact(StoreCode::invalid_source);

        const SerializedAudioArtifact serialized =
            serialize_audio_artifact(artifact, limits_, registry_limits_);
        if (!serialized)
            return reject_artifact(StoreCode::invalid_source);

        try
        {
            const fs::path destination = artifact_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error)
                return reject_artifact(StoreCode::directory_failure);

            if (fs::is_regular_file(destination, error) && !error)
            {
                std::vector<std::byte> existing{};
                if (read_file(
                        destination,
                        limits_.maximum_artifact_bytes,
                        existing,
                        error)
                    && existing == serialized.bytes)
                {
                    ++metrics_.artifact_unchanged_writes;
                    return {
                        .code = StoreCode::unchanged,
                        .path = destination,
                        .digest = serialized.digest,
                        .source_digest = artifact.source_digest,
                        .serialized_bytes = serialized.bytes.size()};
                }
                error.clear();
            }

            const fs::path temporary = temporary_path_for(destination);
            if (!platform::filesystem::exclusive_create_and_write(
                    temporary, serialized.bytes, error))
            {
                return reject_artifact(StoreCode::write_failure);
            }

            std::vector<std::byte> verified{};
            if (!read_file(
                    temporary,
                    limits_.maximum_artifact_bytes,
                    verified,
                    error))
            {
                fs::remove(temporary, error);
                return reject_artifact(StoreCode::read_failure);
            }
            const DeserializedAudioArtifact decoded =
                deserialize_audio_artifact(
                    verified,
                    project_id_,
                    false,
                    limits_,
                    registry_limits_);
            if (!decoded
                || decoded.digest != serialized.digest
                || decoded.artifact.project_key != artifact.project_key
                || decoded.artifact.artifact_key != artifact.artifact_key
                || decoded.artifact.profile_id != artifact.profile_id
                || decoded.artifact.source_sequence
                    != artifact.source_sequence
                || decoded.artifact.source_digest != artifact.source_digest
                || decoded.artifact.decoded_bytes != artifact.decoded_bytes)
            {
                fs::remove(temporary, error);
                return reject_artifact(StoreCode::integrity_failure);
            }

            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                fs::remove(temporary, error);
                return reject_artifact(StoreCode::atomic_replace_failure);
            }

            ++metrics_.artifact_saves;
            metrics_.artifact_bytes_written += serialized.bytes.size();
            return {
                .code = StoreCode::ready,
                .path = destination,
                .digest = serialized.digest,
                .source_digest = artifact.source_digest,
                .serialized_bytes = serialized.bytes.size()};
        }
        catch (const std::bad_alloc&)
        {
            return reject_artifact(StoreCode::allocation_failure);
        }
        catch (...)
        {
            return reject_artifact(StoreCode::write_failure);
        }
    }

    LoadedAudioArtifact ProjectAudioProfileStore::load_artifact(
        bool requestPhysicalOutput) noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid())
            return reject_artifact_load(StoreCode::invalid_store);

        try
        {
            const fs::path path = artifact_path();
            std::error_code error{};
            if (!fs::is_regular_file(path, error) || error)
                return reject_artifact_load(StoreCode::not_found);

            std::vector<std::byte> bytes{};
            if (!read_file(
                    path,
                    limits_.maximum_artifact_bytes,
                    bytes,
                    error))
            {
                return reject_artifact_load(error
                    ? StoreCode::read_failure
                    : StoreCode::size_limit_exceeded);
            }

            DeserializedAudioArtifact decoded =
                deserialize_audio_artifact(
                    bytes,
                    project_id_,
                    requestPhysicalOutput,
                    limits_,
                    registry_limits_);
            if (!decoded)
                return reject_artifact_load(StoreCode::integrity_failure);

            ++metrics_.artifact_loads;
            metrics_.artifact_bytes_read += bytes.size();
            return {
                .code = StoreCode::ready,
                .path = path,
                .digest = decoded.digest,
                .artifact = std::move(decoded.artifact)};
        }
        catch (const std::bad_alloc&)
        {
            return reject_artifact_load(StoreCode::allocation_failure);
        }
        catch (...)
        {
            return reject_artifact_load(StoreCode::read_failure);
        }
    }

    PreparedAudioSession ProjectAudioProfileStore::prepare(
        bool requestPhysicalOutput) noexcept
    {
        if (!valid())
        {
            return {
                .code = PreparationCode::invalid_store,
                .diagnostic = "project audio store is invalid"};
        }

        LoadedProfile source = load();
        if (source)
        {
            CompiledSession compiled = compile(
                source.source, requestPhysicalOutput);
            if (!compiled)
            {
                return {
                    .code = PreparationCode::compile_failure,
                    .diagnostic = compiled.diagnostic};
            }
            const StoredAudioArtifact published =
                publish_artifact(compiled);
            if (!published)
            {
                return {
                    .code = PreparationCode::artifact_failure,
                    .diagnostic = std::string{
                        "project audio Library publish "}
                        + std::string{store_code_name(published.code)}};
            }
            compiled.diagnostic = published.code == StoreCode::unchanged
                ? "project audio source compiled; Library artifact current"
                : "project audio source compiled and published to Library";
            return {
                .code = PreparationCode::ready,
                .session = std::move(compiled),
                .compiled_from_source = true,
                .diagnostic = published.code == StoreCode::unchanged
                    ? "project audio source compiled; Library artifact current"
                    : "project audio source compiled and published to Library"};
        }
        if (source.code != StoreCode::not_found)
        {
            return {
                .code = PreparationCode::source_failure,
                .diagnostic = std::string{"project audio source "}
                    + std::string{store_code_name(source.code)}};
        }

        LoadedAudioArtifact artifact =
            load_artifact(requestPhysicalOutput);
        if (!artifact)
        {
            return {
                .code = PreparationCode::artifact_failure,
                .diagnostic = std::string{"project audio Library artifact "}
                    + std::string{store_code_name(artifact.code)}};
        }
        artifact.artifact.diagnostic =
            "project audio restored from compiled Library artifact";
        return {
            .code = PreparationCode::ready,
            .session = std::move(artifact.artifact),
            .restored_from_artifact = true,
            .diagnostic =
                "project audio restored from compiled Library artifact"};
    }
    ImportedAudioSource ProjectAudioProfileStore::import_source(
        const fs::path& externalSource) const noexcept
    {
        if (!valid())
        {
            return {
                .code = SourceImportCode::invalid_store,
                .diagnostic = "project audio store is invalid"};
        }
        if (externalSource.empty())
        {
            return {
                .code = SourceImportCode::invalid_source,
                .diagnostic = "audio import source is empty"};
        }

        try
        {
            std::error_code error{};
            const fs::path source = fs::weakly_canonical(
                fs::absolute(externalSource, error), error);
            if (error || !fs::is_regular_file(source, error) || error)
            {
                return {
                    .code = SourceImportCode::invalid_source,
                    .diagnostic = "audio import source is not a regular file"};
            }
            if (!wave_extension(source.extension().string()))
            {
                return {
                    .code = SourceImportCode::unsupported_source,
                    .diagnostic = "project audio import currently accepts WAV files"};
            }

            const std::uintmax_t sourceSize = fs::file_size(source, error);
            if (error || sourceSize == 0u)
            {
                return {
                    .code = SourceImportCode::read_failure,
                    .diagnostic = "audio import source size could not be read"};
            }
            if (sourceSize > limits_.import.maximum_source_bytes)
            {
                return {
                    .code = SourceImportCode::source_too_large,
                    .source_bytes = static_cast<std::uint64_t>(sourceSize),
                    .diagnostic = "audio import source exceeds the project budget"};
            }

            asset::audio::AudioImportResult decoded =
                asset::audio::import_audio_file({
                    .source_path = source,
                    .clip_id = {project_key_, 1u},
                    .limits = limits_.import});
            if (!decoded)
            {
                const bool unsupported = decoded.status
                        == asset::audio::AudioImportStatus::
                            unsupported_container
                    || decoded.status
                        == asset::audio::AudioImportStatus::
                            unsupported_encoding;
                return {
                    .code = unsupported
                        ? SourceImportCode::unsupported_source
                        : SourceImportCode::decode_failed,
                    .source_bytes = decoded.source_bytes,
                    .decoded_bytes = decoded.decoded_bytes,
                    .sample_rate = decoded.sample_rate,
                    .channel_count = decoded.channel_count,
                    .diagnostic = std::string{"audio decode failed: "}
                        + std::string{
                            asset::audio::audio_import_status_name(
                                decoded.status)}};
            }

            ImportedAudioSource result{
                .source_bytes = decoded.source_bytes,
                .decoded_bytes = decoded.decoded_bytes,
                .frame_count = decoded.clip.frame_count,
                .sample_rate = decoded.sample_rate,
                .channel_count = decoded.channel_count};
            decoded.clip = {};

            std::vector<std::byte> sourceBytes{};
            if (!read_file(
                    source,
                    limits_.import.maximum_source_bytes,
                    sourceBytes,
                    error))
            {
                result.code = SourceImportCode::read_failure;
                result.diagnostic =
                    "validated audio source could not be copied";
                return result;
            }
            result.source_digest = digest_of(sourceBytes);
            const std::string digestHex = core::sha256::hex(
                result.source_digest);
            result.logical_path = "Assets/Audio/imported/"
                + portable_audio_stem(source.stem().string())
                + "_" + digestHex + ".wav";
            const auto canonical = project_assets::canonical_logical_path(
                result.logical_path, registry_limits_);
            if (!canonical || *canonical != result.logical_path)
            {
                result.code = SourceImportCode::invalid_source;
                result.diagnostic =
                    "audio import destination is not a portable project path";
                return result;
            }
            result.destination_path = project_root_
                / fs::path{result.logical_path};
            fs::create_directories(
                result.destination_path.parent_path(), error);
            if (error)
            {
                result.code = SourceImportCode::directory_failure;
                result.diagnostic =
                    "audio import directory could not be created";
                return result;
            }

            if (fs::is_regular_file(result.destination_path, error) && !error)
            {
                std::vector<std::byte> existing{};
                if (!read_file(
                        result.destination_path,
                        limits_.import.maximum_source_bytes,
                        existing,
                        error))
                {
                    result.code = SourceImportCode::read_failure;
                    result.diagnostic =
                        "existing project audio source could not be verified";
                    return result;
                }
                if (existing != sourceBytes
                    || digest_of(existing) != result.source_digest)
                {
                    result.code = SourceImportCode::integrity_failure;
                    result.diagnostic =
                        "content-addressed project audio path contains different bytes";
                    return result;
                }
                result.code = SourceImportCode::unchanged;
                result.diagnostic =
                    "identical decoded audio source is already in the project";
                return result;
            }
            error.clear();

            const fs::path temporary = temporary_path_for(
                result.destination_path);
            if (!platform::filesystem::exclusive_create_and_write(
                    temporary, sourceBytes, error))
            {
                result.code = SourceImportCode::write_failure;
                result.diagnostic =
                    "project audio staging write failed";
                return result;
            }
            std::vector<std::byte> verified{};
            if (!read_file(
                    temporary,
                    limits_.import.maximum_source_bytes,
                    verified,
                    error)
                || verified != sourceBytes
                || digest_of(verified) != result.source_digest)
            {
                fs::remove(temporary, error);
                result.code = SourceImportCode::integrity_failure;
                result.diagnostic =
                    "project audio staging verification failed";
                return result;
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, result.destination_path, error))
            {
                fs::remove(temporary, error);
                result.code = SourceImportCode::atomic_replace_failure;
                result.diagnostic =
                    "project audio source could not be published atomically";
                return result;
            }

            result.code = SourceImportCode::ready;
            result.created = true;
            result.diagnostic =
                "decoded WAV source imported into project audio";
            return result;
        }
        catch (const std::bad_alloc&)
        {
            return {
                .code = SourceImportCode::allocation_failure,
                .diagnostic = "project audio import allocation failed"};
        }
        catch (...)
        {
            return {
                .code = SourceImportCode::invalid_source,
                .diagnostic = "project audio import failed"};
        }
    }

    CompiledSession ProjectAudioProfileStore::compile(
        const ProfileSource& source,
        bool requestPhysicalOutput) const noexcept
    {
        if (!valid())
        {
            return {
                .code = CompileCode::invalid_project,
                .diagnostic = "project audio store is invalid"};
        }
        return compile_profile(
            project_id_,
            project_root_,
            source,
            requestPhysicalOutput,
            limits_,
            registry_limits_);
    }
}
