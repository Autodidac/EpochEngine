/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

module project.sprite_animation;

import platform.filesystem;

namespace epochengine::project_sprite_animation
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::array<std::byte, 8> kSourceMagic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'S'}, std::byte{'P'},
            std::byte{'R'}, std::byte{'S'}, std::byte{'R'}, std::byte{'1'}};
        constexpr std::array<std::byte, 8> kArtifactMagic{
            std::byte{'E'}, std::byte{'P'}, std::byte{'S'}, std::byte{'P'},
            std::byte{'R'}, std::byte{'A'}, std::byte{'R'}, std::byte{'1'}};
        constexpr std::uint16_t kCodecVersion{1u};
        constexpr std::uint64_t kEnvelopeBytes{52u};

        class HashBuilder final
        {
        public:
            HashBuilder() noexcept = default;

            void byte(std::uint8_t value) noexcept
            {
                builder_.append_u8(value);
            }

            void u32(std::uint32_t value) noexcept
            {
                builder_.append_u32(value);
            }

            void i32(std::int32_t value) noexcept
            {
                builder_.append_u32(static_cast<std::uint32_t>(value));
            }

            void u64(std::uint64_t value) noexcept
            {
                builder_.append_u64(value);
            }

            void text(std::string_view value) noexcept
            {
                builder_.append_string(value);
            }

            template<typename Hash>
            void hash(const Hash& value) noexcept
            {
                for (const std::uint64_t word : value.words)
                    builder_.append_u64(word);
            }

            [[nodiscard]] authoring::ContentHash finish() const noexcept
            {
                return builder_.finish();
            }

        private:
            authoring::DeterministicHashBuilder builder_{
                "project.sprite_animation.hash.v1"};
        };
        class ByteWriter final
        {
        public:
            explicit ByteWriter(std::uint64_t maximumBytes) noexcept
                : maximum_bytes_(maximumBytes)
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] std::span<const std::byte> view() const noexcept
            {
                return bytes_;
            }

            [[nodiscard]] std::vector<std::byte> release() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::byte> value)
            {
                if (!valid_ || value.size() > maximum_bytes_
                    || bytes_.size() > maximum_bytes_ - value.size())
                {
                    valid_ = false;
                    return;
                }
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }

            void u8(std::uint8_t value)
            {
                const std::byte byteValue{value};
                raw({&byteValue, 1u});
            }

            void u16(std::uint16_t value)
            {
                for (std::uint32_t shift = 0u; shift < 16u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void u32(std::uint32_t value)
            {
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            void i32(std::int32_t value)
            {
                u32(static_cast<std::uint32_t>(value));
            }

            void u64(std::uint64_t value)
            {
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }

            template<typename Hash>
            void hash(const Hash& value)
            {
                for (const std::uint64_t word : value.words)
                    u64(word);
            }

            void text(std::string_view value)
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                {
                    valid_ = false;
                    return;
                }
                u32(static_cast<std::uint32_t>(value.size()));
                raw(std::as_bytes(std::span{value.data(), value.size()}));
            }

        private:
            std::uint64_t maximum_bytes_{};
            std::vector<std::byte> bytes_{};
            bool valid_{true};
        };

        class ByteReader final
        {
        public:
            explicit ByteReader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool valid() const noexcept
            {
                return valid_;
            }

            [[nodiscard]] bool at_end() const noexcept
            {
                return valid_ && cursor_ == bytes_.size();
            }

            [[nodiscard]] std::size_t remaining() const noexcept
            {
                return valid_ ? bytes_.size() - cursor_ : 0u;
            }

            [[nodiscard]] bool matches(std::span<const std::byte> expected)
                noexcept
            {
                if (!available(expected.size()))
                    return false;
                const bool equal = std::equal(
                    expected.begin(), expected.end(), bytes_.begin() + cursor_);
                cursor_ += expected.size();
                if (!equal)
                    valid_ = false;
                return equal;
            }

            [[nodiscard]] std::uint8_t u8() noexcept
            {
                if (!available(1u))
                    return 0u;
                return std::to_integer<std::uint8_t>(bytes_[cursor_++]);
            }

            [[nodiscard]] std::uint16_t u16() noexcept
            {
                std::uint16_t value{};
                for (std::uint32_t shift = 0u; shift < 16u; shift += 8u)
                    value |= static_cast<std::uint16_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] std::uint32_t u32() noexcept
            {
                std::uint32_t value{};
                for (std::uint32_t shift = 0u; shift < 32u; shift += 8u)
                    value |= static_cast<std::uint32_t>(u8()) << shift;
                return value;
            }

            [[nodiscard]] std::int32_t i32() noexcept
            {
                return static_cast<std::int32_t>(u32());
            }

            [[nodiscard]] std::uint64_t u64() noexcept
            {
                std::uint64_t value{};
                for (std::uint32_t shift = 0u; shift < 64u; shift += 8u)
                    value |= static_cast<std::uint64_t>(u8()) << shift;
                return value;
            }

            template<typename Hash>
            [[nodiscard]] Hash hash() noexcept
            {
                Hash value{};
                for (std::uint64_t& word : value.words)
                    word = u64();
                return value;
            }

            [[nodiscard]] std::optional<std::string> text(
                std::uint32_t maximumBytes)
            {
                const std::uint32_t count = u32();
                if (!valid_ || count > maximumBytes || !available(count))
                    return std::nullopt;
                std::string result{};
                result.resize(count);
                for (std::uint32_t index = 0u; index < count; ++index)
                {
                    result[index] = static_cast<char>(
                        std::to_integer<std::uint8_t>(bytes_[cursor_ + index]));
                }
                cursor_ += count;
                return result;
            }

            [[nodiscard]] std::span<const std::byte> take(
                std::size_t count) noexcept
            {
                if (!available(count))
                    return {};
                const std::span<const std::byte> result = bytes_.subspan(
                    cursor_, count);
                cursor_ += count;
                return result;
            }

        private:
            [[nodiscard]] bool available(std::size_t count) noexcept
            {
                if (!valid_ || count > bytes_.size() - cursor_)
                {
                    valid_ = false;
                    return false;
                }
                return true;
            }

            std::span<const std::byte> bytes_{};
            std::size_t cursor_{};
            bool valid_{true};
        };

        struct EnvelopeView final
        {
            CodecCode code{CodecCode::malformed_data};
            std::span<const std::byte> payload{};
        };

        [[nodiscard]] AnimationArtifactDigest hash_bytes(
            std::span<const std::byte> bytes) noexcept
        {
            HashBuilder hash{};
            for (const std::byte value : bytes)
                hash.byte(std::to_integer<std::uint8_t>(value));
            return {hash.finish().words};
        }

        [[nodiscard]] EncodedBytes wrap_payload(
            const std::array<std::byte, 8>& magic,
            std::span<const std::byte> payload,
            std::uint64_t maximumBytes)
        {
            if (payload.size() > maximumBytes
                || kEnvelopeBytes > maximumBytes - payload.size())
            {
                return {CodecCode::serialized_budget_exceeded};
            }
            ByteWriter writer{maximumBytes};
            writer.raw(magic);
            writer.u16(kCodecVersion);
            writer.u16(0u);
            writer.u64(static_cast<std::uint64_t>(payload.size()));
            writer.hash(hash_bytes(payload));
            writer.raw(payload);
            return writer.valid()
                ? EncodedBytes{CodecCode::ready, writer.release()}
                : EncodedBytes{CodecCode::serialized_budget_exceeded};
        }

        [[nodiscard]] EnvelopeView unwrap_payload(
            std::span<const std::byte> bytes,
            const std::array<std::byte, 8>& magic,
            std::uint64_t maximumBytes) noexcept
        {
            if (bytes.size() > maximumBytes)
                return {CodecCode::serialized_budget_exceeded};
            if (bytes.size() < kEnvelopeBytes)
                return {CodecCode::malformed_data};
            ByteReader reader{bytes};
            if (!reader.matches(magic))
                return {CodecCode::malformed_data};
            const std::uint16_t version = reader.u16();
            const std::uint16_t reserved = reader.u16();
            const std::uint64_t payloadBytes = reader.u64();
            const AnimationArtifactDigest expected = reader.hash<AnimationArtifactDigest>();
            if (!reader.valid())
                return {CodecCode::malformed_data};
            if (version != kCodecVersion)
                return {CodecCode::unsupported_version};
            if (reserved != 0u || payloadBytes > maximumBytes
                || payloadBytes > (std::numeric_limits<std::size_t>::max)())
            {
                return {CodecCode::malformed_data};
            }
            if (payloadBytes != reader.remaining())
                return {CodecCode::trailing_data};
            const auto payload = reader.take(
                static_cast<std::size_t>(payloadBytes));
            if (!reader.at_end())
                return {CodecCode::trailing_data};
            if (hash_bytes(payload) != expected)
                return {CodecCode::integrity_failure};
            return {CodecCode::ready, payload};
        }

        [[nodiscard]] bool valid_text(
            std::string_view value,
            std::uint32_t maximumBytes,
            bool allowEmpty = false) noexcept
        {
            if ((!allowEmpty && value.empty()) || value.size() > maximumBytes)
                return false;
            for (const unsigned char character : value)
            {
                if (character == 0u || character < 0x20u || character == 0x7fu)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool valid_playback(PlaybackMode mode) noexcept
        {
            return mode == PlaybackMode::once || mode == PlaybackMode::loop
                || mode == PlaybackMode::ping_pong;
        }

        [[nodiscard]] bool valid_semantic(EventSemantic semantic) noexcept
        {
            return semantic == EventSemantic::audio_cue
                || semantic == EventSemantic::footstep
                || semantic == EventSemantic::impact
                || semantic == EventSemantic::custom;
        }

        [[nodiscard]] bool valid_direction(EventDirection direction) noexcept
        {
            return direction == EventDirection::forward_only
                || direction == EventDirection::reverse_only
                || direction == EventDirection::both;
        }

        [[nodiscard]] bool cue_required(EventSemantic semantic) noexcept
        {
            return semantic == EventSemantic::audio_cue
                || semantic == EventSemantic::footstep
                || semantic == EventSemantic::impact;
        }

        [[nodiscard]] bool canonical_path(
            std::string_view path,
            const project_assets::RegistryLimits& limits) noexcept
        {
            try
            {
                const auto canonical = project_assets::canonical_logical_path(
                    path, limits);
                return canonical && *canonical == path;
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] std::uint64_t stable_value(
            std::string_view domain,
            std::string_view name) noexcept
        {
            if (name.empty())
                return 0u;
            HashBuilder hash{};
            hash.text(domain);
            hash.text(name);
            const authoring::ContentHash content = hash.finish();
            std::uint64_t value = content.words[0] ^ content.words[1]
                ^ content.words[2] ^ content.words[3];
            if (value == 0u)
                value = 1u;
            return value;
        }

        [[nodiscard]] std::optional<std::uint32_t> frame_index(
            const AnimationSource& animation,
            FrameId id) noexcept
        {
            for (std::uint32_t index = 0u; index < animation.frames.size(); ++index)
            {
                if (animation.frames[index].id == id)
                    return index;
            }
            return std::nullopt;
        }

        [[nodiscard]] ValidationCode validate_source_impl(
            const SpriteAnimationSource& source,
            const AnimationLimits& limits,
            const project_assets::RegistryLimits& registryLimits,
            bool requireHash,
            bool requireCanonical) noexcept
        {
            if (!limits.valid() || !registryLimits.valid())
                return ValidationCode::invalid_limits;
            if (!source.id || source.revision.sequence == 0u)
                return ValidationCode::invalid_document;
            if (!valid_text(source.name, limits.maximum_name_bytes))
                return ValidationCode::invalid_name;
            if (source.animations.empty())
                return ValidationCode::invalid_animation;
            if (source.animations.size() > limits.maximum_animations)
                return ValidationCode::animation_limit_exceeded;

            try
            {
                std::vector<std::uint64_t> animationIds{};
                std::vector<std::uint64_t> frameIds{};
                std::vector<std::uint64_t> eventIds{};
                animationIds.reserve(source.animations.size());
                std::uint64_t totalFrames{};
                std::uint64_t totalEvents{};
                AnimationId previousAnimation{};

                for (const AnimationSource& animation : source.animations)
                {
                    if (!animation.id || !valid_playback(animation.playback))
                        return ValidationCode::invalid_animation;
                    if (!valid_text(animation.name, limits.maximum_name_bytes))
                        return ValidationCode::invalid_name;
                    if (requireCanonical && previousAnimation
                        && animation.id <= previousAnimation)
                    {
                        return ValidationCode::noncanonical_order;
                    }
                    previousAnimation = animation.id;
                    animationIds.push_back(animation.id.value);
                    if (animation.frames.empty())
                        return ValidationCode::invalid_frame;
                    if (animation.frames.size()
                        > limits.maximum_frames_per_animation)
                    {
                        return ValidationCode::frame_limit_exceeded;
                    }
                    if (animation.events.size()
                        > limits.maximum_events_per_animation)
                    {
                        return ValidationCode::event_limit_exceeded;
                    }
                    totalFrames += animation.frames.size();
                    totalEvents += animation.events.size();
                    if (totalFrames > limits.maximum_frames)
                        return ValidationCode::frame_limit_exceeded;
                    if (totalEvents > limits.maximum_events)
                        return ValidationCode::event_limit_exceeded;

                    std::uint64_t totalTicks{};
                    for (const FrameSource& frame : animation.frames)
                    {
                        if (!frame.id || frame.duration_ticks == 0u
                            || frame.duration_ticks
                                > limits.maximum_frame_duration_ticks)
                        {
                            return ValidationCode::invalid_frame;
                        }
                        frameIds.push_back(frame.id.value);
                        const MaterialSource& material = frame.material;
                        if (material.texture_artifact_key.empty()
                            || material.texture_artifact_revision == 0u
                            || material.stable_material_key == 0u
                            || material.texture_extent.x == 0u
                            || material.texture_extent.y == 0u
                            || material.texture_extent.x
                                > limits.maximum_texture_dimension
                            || material.texture_extent.y
                                > limits.maximum_texture_dimension)
                        {
                            return ValidationCode::invalid_material;
                        }
                        if (material.logical_texture_path.size()
                                > limits.maximum_path_bytes
                            || !canonical_path(
                                material.logical_texture_path, registryLimits))
                        {
                            return ValidationCode::invalid_path;
                        }
                        const SourceRectangle& rectangle = frame.source_rectangle;
                        if (rectangle.empty()
                            || rectangle.x >= material.texture_extent.x
                            || rectangle.y >= material.texture_extent.y
                            || rectangle.width
                                > material.texture_extent.x - rectangle.x
                            || rectangle.height
                                > material.texture_extent.y - rectangle.y)
                        {
                            return ValidationCode::invalid_rectangle;
                        }
                        if (totalTicks
                            > limits.maximum_animation_duration_ticks
                                - frame.duration_ticks)
                        {
                            return ValidationCode::duration_limit_exceeded;
                        }
                        totalTicks += frame.duration_ticks;
                    }

                    std::tuple<std::uint32_t, std::uint32_t, std::uint64_t>
                        previousEvent{};
                    bool hasPreviousEvent{};
                    std::uint64_t previousAbsoluteTick{
                        (std::numeric_limits<std::uint64_t>::max)()};
                    std::uint32_t eventsAtTick{};
                    for (const AnimationEventSource& event : animation.events)
                    {
                        if (!event.id || !valid_semantic(event.semantic)
                            || !valid_direction(event.direction))
                        {
                            return ValidationCode::invalid_event;
                        }
                        eventIds.push_back(event.id.value);
                        const auto index = frame_index(animation, event.frame);
                        if (!index)
                            return ValidationCode::dangling_frame;
                        const FrameSource& frame = animation.frames[*index];
                        if (event.tick_in_frame >= frame.duration_ticks)
                            return ValidationCode::invalid_event;
                        if (cue_required(event.semantic)
                            && event.logical_audio_path.empty())
                        {
                            return ValidationCode::invalid_event;
                        }
                        if (!event.logical_audio_path.empty()
                            && (event.logical_audio_path.size()
                                    > limits.maximum_path_bytes
                                || !canonical_path(
                                    event.logical_audio_path, registryLimits)))
                        {
                            return ValidationCode::invalid_path;
                        }
                        const auto key = std::tuple{
                            *index, event.tick_in_frame, event.id.value};
                        if (requireCanonical && hasPreviousEvent
                            && key <= previousEvent)
                        {
                            return ValidationCode::noncanonical_order;
                        }
                        previousEvent = key;
                        hasPreviousEvent = true;

                        std::uint64_t absoluteTick = event.tick_in_frame;
                        for (std::uint32_t frameIndex = 0u;
                            frameIndex < *index; ++frameIndex)
                        {
                            absoluteTick += animation.frames[frameIndex]
                                .duration_ticks;
                        }
                        if (absoluteTick == previousAbsoluteTick)
                            ++eventsAtTick;
                        else
                        {
                            previousAbsoluteTick = absoluteTick;
                            eventsAtTick = 1u;
                        }
                        if (eventsAtTick > limits.maximum_events_per_tick)
                            return ValidationCode::event_tick_limit_exceeded;
                    }
                }

                const auto duplicate = [](std::vector<std::uint64_t>& ids)
                {
                    std::sort(ids.begin(), ids.end());
                    return std::adjacent_find(ids.begin(), ids.end())
                        != ids.end();
                };
                if (duplicate(animationIds))
                    return ValidationCode::duplicate_animation;
                if (duplicate(frameIds))
                    return ValidationCode::duplicate_frame;
                if (duplicate(eventIds))
                    return ValidationCode::duplicate_event;
            }
            catch (...)
            {
                return ValidationCode::allocation_failure;
            }

            if (requireHash)
            {
                if (!source.revision
                    || source.revision.content != source_content_hash(source))
                {
                    return ValidationCode::content_hash_mismatch;
                }
            }
            return ValidationCode::ready;
        }

        void write_material(ByteWriter& writer, const MaterialSource& material)
        {
            writer.text(material.logical_texture_path);
            writer.hash(material.texture_artifact_key);
            writer.u64(material.texture_artifact_revision);
            writer.u32(material.stable_material_key);
            writer.u32(material.texture_extent.x);
            writer.u32(material.texture_extent.y);
        }

        [[nodiscard]] std::optional<MaterialSource> read_material(
            ByteReader& reader,
            const AnimationLimits& limits)
        {
            auto path = reader.text(limits.maximum_path_bytes);
            if (!path)
                return std::nullopt;
            MaterialSource material{};
            material.logical_texture_path = std::move(*path);
            material.texture_artifact_key = reader.hash<asset::texture::ContentHash>();
            material.texture_artifact_revision = reader.u64();
            material.stable_material_key = reader.u32();
            material.texture_extent = {reader.u32(), reader.u32()};
            return reader.valid()
                ? std::optional<MaterialSource>{std::move(material)}
                : std::nullopt;
        }

        void write_source_payload(
            ByteWriter& writer,
            const SpriteAnimationSource& source)
        {
            writer.u32(source_schema_version);
            writer.u64(source.id.value);
            writer.text(source.name);
            writer.hash(source.revision.content);
            writer.u64(source.revision.sequence);
            writer.u32(static_cast<std::uint32_t>(source.animations.size()));
            for (const AnimationSource& animation : source.animations)
            {
                writer.u64(animation.id.value);
                writer.text(animation.name);
                writer.u8(static_cast<std::uint8_t>(animation.playback));
                writer.u32(static_cast<std::uint32_t>(animation.frames.size()));
                writer.u32(static_cast<std::uint32_t>(animation.events.size()));
                for (const FrameSource& frame : animation.frames)
                {
                    writer.u64(frame.id.value);
                    write_material(writer, frame.material);
                    writer.u32(frame.source_rectangle.x);
                    writer.u32(frame.source_rectangle.y);
                    writer.u32(frame.source_rectangle.width);
                    writer.u32(frame.source_rectangle.height);
                    writer.u32(frame.duration_ticks);
                    writer.i32(frame.pivot_x_q16);
                    writer.i32(frame.pivot_y_q16);
                }
                for (const AnimationEventSource& event : animation.events)
                {
                    writer.u64(event.id.value);
                    writer.u64(event.frame.value);
                    writer.u32(event.tick_in_frame);
                    writer.u8(static_cast<std::uint8_t>(event.semantic));
                    writer.u8(static_cast<std::uint8_t>(event.direction));
                    writer.text(event.logical_audio_path);
                    writer.u64(event.payload);
                }
            }
        }

        [[nodiscard]] DecodedSource read_source_payload(
            std::span<const std::byte> payload,
            const AnimationLimits& limits,
            const project_assets::RegistryLimits& registryLimits)
        {
            ByteReader reader{payload};
            if (reader.u32() != source_schema_version)
                return {CodecCode::unsupported_version};
            SpriteAnimationSource source{};
            source.id.value = reader.u64();
            auto name = reader.text(limits.maximum_name_bytes);
            if (!name)
                return {CodecCode::malformed_data};
            source.name = std::move(*name);
            source.revision.content = reader.hash<authoring::ContentHash>();
            source.revision.sequence = reader.u64();
            const std::uint32_t animationCount = reader.u32();
            if (!reader.valid() || animationCount > limits.maximum_animations)
                return {CodecCode::malformed_data};
            source.animations.reserve(animationCount);
            std::uint64_t totalFrames{};
            std::uint64_t totalEvents{};
            for (std::uint32_t animationIndex = 0u;
                animationIndex < animationCount; ++animationIndex)
            {
                AnimationSource animation{};
                animation.id.value = reader.u64();
                auto animationName = reader.text(limits.maximum_name_bytes);
                if (!animationName)
                    return {CodecCode::malformed_data};
                animation.name = std::move(*animationName);
                animation.playback = static_cast<PlaybackMode>(reader.u8());
                const std::uint32_t frameCount = reader.u32();
                const std::uint32_t eventCount = reader.u32();
                totalFrames += frameCount;
                totalEvents += eventCount;
                if (!reader.valid()
                    || frameCount > limits.maximum_frames_per_animation
                    || eventCount > limits.maximum_events_per_animation
                    || totalFrames > limits.maximum_frames
                    || totalEvents > limits.maximum_events)
                {
                    return {CodecCode::malformed_data};
                }
                animation.frames.reserve(frameCount);
                animation.events.reserve(eventCount);
                for (std::uint32_t frameIndex = 0u;
                    frameIndex < frameCount; ++frameIndex)
                {
                    FrameSource frame{};
                    frame.id.value = reader.u64();
                    auto material = read_material(reader, limits);
                    if (!material)
                        return {CodecCode::malformed_data};
                    frame.material = std::move(*material);
                    frame.source_rectangle = {
                        reader.u32(), reader.u32(), reader.u32(), reader.u32()};
                    frame.duration_ticks = reader.u32();
                    frame.pivot_x_q16 = reader.i32();
                    frame.pivot_y_q16 = reader.i32();
                    animation.frames.push_back(std::move(frame));
                }
                for (std::uint32_t eventIndex = 0u;
                    eventIndex < eventCount; ++eventIndex)
                {
                    AnimationEventSource event{};
                    event.id.value = reader.u64();
                    event.frame.value = reader.u64();
                    event.tick_in_frame = reader.u32();
                    event.semantic = static_cast<EventSemantic>(reader.u8());
                    event.direction = static_cast<EventDirection>(reader.u8());
                    auto audioPath = reader.text(limits.maximum_path_bytes);
                    if (!audioPath)
                        return {CodecCode::malformed_data};
                    event.logical_audio_path = std::move(*audioPath);
                    event.payload = reader.u64();
                    animation.events.push_back(std::move(event));
                }
                source.animations.push_back(std::move(animation));
            }
            if (!reader.at_end())
                return {CodecCode::trailing_data};
            if (validate_source(source, limits, registryLimits)
                != ValidationCode::ready)
            {
                return {CodecCode::integrity_failure};
            }
            return {CodecCode::ready, std::move(source)};
        }

        void write_material_identity(
            ByteWriter& writer,
            const MaterialIdentity& material)
        {
            writer.u64(material.project_key);
            writer.u64(material.texture_asset_key);
            writer.text(material.logical_texture_path);
            writer.hash(material.texture_artifact_key);
            writer.u64(material.texture_artifact_revision);
            writer.u32(material.stable_material_key);
            writer.u32(material.texture_extent.x);
            writer.u32(material.texture_extent.y);
        }

        [[nodiscard]] std::optional<MaterialIdentity> read_material_identity(
            ByteReader& reader,
            const AnimationLimits& limits)
        {
            MaterialIdentity material{};
            material.project_key = reader.u64();
            material.texture_asset_key = reader.u64();
            auto path = reader.text(limits.maximum_path_bytes);
            if (!path)
                return std::nullopt;
            material.logical_texture_path = std::move(*path);
            material.texture_artifact_key = reader.hash<asset::texture::ContentHash>();
            material.texture_artifact_revision = reader.u64();
            material.stable_material_key = reader.u32();
            material.texture_extent = {reader.u32(), reader.u32()};
            return reader.valid()
                ? std::optional<MaterialIdentity>{std::move(material)}
                : std::nullopt;
        }

        void write_artifact_payload(
            ByteWriter& writer,
            const CompiledSpriteAnimationArtifact& artifact)
        {
            writer.u32(artifact.schema_version);
            writer.u64(artifact.project_key);
            writer.u64(artifact.asset_key);
            writer.u64(artifact.document_id.value);
            writer.text(artifact.name);
            writer.hash(artifact.source_revision.content);
            writer.u64(artifact.source_revision.sequence);
            writer.hash(artifact.artifact_hash);
            writer.u32(static_cast<std::uint32_t>(artifact.animations.size()));
            writer.u32(static_cast<std::uint32_t>(artifact.frames.size()));
            writer.u32(static_cast<std::uint32_t>(artifact.events.size()));
            for (const CompiledAnimation& animation : artifact.animations)
            {
                writer.u64(animation.id.value);
                writer.text(animation.name);
                writer.u8(static_cast<std::uint8_t>(animation.playback));
                writer.u32(animation.first_frame);
                writer.u32(animation.frame_count);
                writer.u32(animation.first_event);
                writer.u32(animation.event_count);
                writer.u64(animation.total_ticks);
            }
            for (const CompiledFrame& frame : artifact.frames)
            {
                writer.u64(frame.id.value);
                write_material_identity(writer, frame.material);
                writer.u32(frame.source_rectangle.x);
                writer.u32(frame.source_rectangle.y);
                writer.u32(frame.source_rectangle.width);
                writer.u32(frame.source_rectangle.height);
                writer.u64(frame.first_tick);
                writer.u32(frame.duration_ticks);
                writer.i32(frame.pivot_x_q16);
                writer.i32(frame.pivot_y_q16);
            }
            for (const CompiledEvent& event : artifact.events)
            {
                writer.u64(event.id.value);
                writer.u64(event.frame.value);
                writer.u64(event.animation_tick);
                writer.u32(event.tick_in_frame);
                writer.u8(static_cast<std::uint8_t>(event.semantic));
                writer.u8(static_cast<std::uint8_t>(event.direction));
                writer.text(event.logical_audio_path);
                writer.u64(event.audio_asset_key);
                writer.u64(event.payload);
            }
        }

        [[nodiscard]] DecodedArtifact read_artifact_payload(
            std::span<const std::byte> payload,
            const AnimationLimits& limits,
            const project_assets::RegistryLimits& registryLimits)
        {
            ByteReader reader{payload};
            CompiledSpriteAnimationArtifact artifact{};
            artifact.schema_version = reader.u32();
            if (artifact.schema_version != artifact_schema_version)
                return {CodecCode::unsupported_version};
            artifact.project_key = reader.u64();
            artifact.asset_key = reader.u64();
            artifact.document_id.value = reader.u64();
            auto name = reader.text(limits.maximum_name_bytes);
            if (!name)
                return {CodecCode::malformed_data};
            artifact.name = std::move(*name);
            artifact.source_revision.content = reader.hash<authoring::ContentHash>();
            artifact.source_revision.sequence = reader.u64();
            artifact.artifact_hash = reader.hash<AnimationArtifactDigest>();
            const std::uint32_t animationCount = reader.u32();
            const std::uint32_t frameCount = reader.u32();
            const std::uint32_t eventCount = reader.u32();
            if (!reader.valid() || animationCount > limits.maximum_animations
                || frameCount > limits.maximum_frames
                || eventCount > limits.maximum_events)
            {
                return {CodecCode::malformed_data};
            }
            artifact.animations.reserve(animationCount);
            artifact.frames.reserve(frameCount);
            artifact.events.reserve(eventCount);
            for (std::uint32_t index = 0u; index < animationCount; ++index)
            {
                CompiledAnimation animation{};
                animation.id.value = reader.u64();
                auto animationName = reader.text(limits.maximum_name_bytes);
                if (!animationName)
                    return {CodecCode::malformed_data};
                animation.name = std::move(*animationName);
                animation.playback = static_cast<PlaybackMode>(reader.u8());
                animation.first_frame = reader.u32();
                animation.frame_count = reader.u32();
                animation.first_event = reader.u32();
                animation.event_count = reader.u32();
                animation.total_ticks = reader.u64();
                artifact.animations.push_back(std::move(animation));
            }
            for (std::uint32_t index = 0u; index < frameCount; ++index)
            {
                CompiledFrame frame{};
                frame.id.value = reader.u64();
                auto material = read_material_identity(reader, limits);
                if (!material)
                    return {CodecCode::malformed_data};
                frame.material = std::move(*material);
                frame.source_rectangle = {
                    reader.u32(), reader.u32(), reader.u32(), reader.u32()};
                frame.first_tick = reader.u64();
                frame.duration_ticks = reader.u32();
                frame.pivot_x_q16 = reader.i32();
                frame.pivot_y_q16 = reader.i32();
                artifact.frames.push_back(std::move(frame));
            }
            for (std::uint32_t index = 0u; index < eventCount; ++index)
            {
                CompiledEvent event{};
                event.id.value = reader.u64();
                event.frame.value = reader.u64();
                event.animation_tick = reader.u64();
                event.tick_in_frame = reader.u32();
                event.semantic = static_cast<EventSemantic>(reader.u8());
                event.direction = static_cast<EventDirection>(reader.u8());
                auto audioPath = reader.text(limits.maximum_path_bytes);
                if (!audioPath)
                    return {CodecCode::malformed_data};
                event.logical_audio_path = std::move(*audioPath);
                event.audio_asset_key = reader.u64();
                event.payload = reader.u64();
                artifact.events.push_back(std::move(event));
            }
            if (!reader.at_end())
                return {CodecCode::trailing_data};
            if (validate_artifact(artifact, limits, registryLimits)
                != ValidationCode::ready)
            {
                return {CodecCode::integrity_failure};
            }
            return {CodecCode::ready, std::move(artifact)};
        }

        struct ReadFileResult final
        {
            StoreCode code{StoreCode::read_failure};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] ReadFileResult read_file(
            const fs::path& path,
            std::uint64_t maximumBytes) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::exists(path, error))
                    return {error ? StoreCode::read_failure : StoreCode::not_found};
                const std::uintmax_t size = fs::file_size(path, error);
                if (error || size > maximumBytes
                    || size > (std::numeric_limits<std::size_t>::max)())
                {
                    return {StoreCode::integrity_failure};
                }
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return {StoreCode::read_failure};
                std::vector<std::byte> bytes(static_cast<std::size_t>(size));
                if (!bytes.empty())
                {
                    input.read(
                        reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
                }
                if (!input || input.peek() != std::char_traits<char>::eof())
                    return {StoreCode::read_failure};
                return {StoreCode::ready, std::move(bytes)};
            }
            catch (...)
            {
                return {StoreCode::allocation_failure};
            }
        }

        [[nodiscard]] StoreCode write_file(
            const fs::path& path,
            std::span<const std::byte> bytes) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return StoreCode::write_failure;
                if (!bytes.empty())
                {
                    output.write(
                        reinterpret_cast<const char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
                }
                output.flush();
                return output ? StoreCode::ready : StoreCode::write_failure;
            }
            catch (...)
            {
                return StoreCode::allocation_failure;
            }
        }

        [[nodiscard]] fs::path temporary_path(const fs::path& destination)
        {
            static std::atomic<std::uint64_t> nonce{1u};
            return destination.string() + ".tmp."
                + std::to_string(nonce.fetch_add(
                    1u, std::memory_order_relaxed));
        }

        [[nodiscard]] StoreCode compare_revision(
            const authoring::DocumentRevision& current,
            const authoring::DocumentRevision& candidate) noexcept
        {
            if (current == candidate)
                return StoreCode::unchanged;
            if (current.sequence > candidate.sequence)
                return StoreCode::stale_revision;
            if (current.sequence == candidate.sequence)
                return StoreCode::revision_conflict;
            return StoreCode::ready;
        }

        [[nodiscard]] std::int64_t floor_div(
            std::int64_t value,
            std::int64_t divisor) noexcept
        {
            std::int64_t quotient = value / divisor;
            const std::int64_t remainder = value % divisor;
            if (remainder < 0)
                --quotient;
            return quotient;
        }

        [[nodiscard]] std::uint64_t floor_mod(
            std::int64_t value,
            std::uint64_t modulus) noexcept
        {
            const std::int64_t signedModulus = static_cast<std::int64_t>(modulus);
            std::int64_t remainder = value % signedModulus;
            if (remainder < 0)
                remainder += signedModulus;
            return static_cast<std::uint64_t>(remainder);
        }

        [[nodiscard]] bool event_allowed(
            EventDirection policy,
            bool movingForward) noexcept
        {
            return policy == EventDirection::both
                || (movingForward && policy == EventDirection::forward_only)
                || (!movingForward && policy == EventDirection::reverse_only);
        }
    }

    bool DefaultActorSheet::valid() const noexcept
    {
        constexpr std::uint32_t maximumQ16HalfExtent =
            static_cast<std::uint32_t>(
                (std::numeric_limits<std::int32_t>::max)() / 32'768);
        if (material.logical_texture_path.empty()
            || material.texture_artifact_key.empty()
            || material.texture_artifact_revision == 0u
            || material.stable_material_key == 0u
            || material.texture_extent.x == 0u
            || material.texture_extent.y == 0u
            || tile_extent.x == 0u || tile_extent.y == 0u
            || tile_extent.x > maximumQ16HalfExtent
            || tile_extent.y > maximumQ16HalfExtent
            || grid.x == 0u || grid.y == 0u || tile_count == 0u)
        {
            return false;
        }
        const std::uint64_t availableTiles =
            static_cast<std::uint64_t>(grid.x) * grid.y;
        const std::uint64_t requiredWidth =
            static_cast<std::uint64_t>(margin.x) * 2u
            + static_cast<std::uint64_t>(grid.x) * tile_extent.x
            + static_cast<std::uint64_t>(grid.x - 1u) * spacing.x;
        const std::uint64_t requiredHeight =
            static_cast<std::uint64_t>(margin.y) * 2u
            + static_cast<std::uint64_t>(grid.y) * tile_extent.y
            + static_cast<std::uint64_t>(grid.y - 1u) * spacing.y;
        return tile_count <= availableTiles
            && requiredWidth <= material.texture_extent.x
            && requiredHeight <= material.texture_extent.y;
    }

    ActorPose classify_actor_pose(const ActorMotionState& state) noexcept
    {
        if (state.paused || !std::isfinite(state.velocity_x)
            || !std::isfinite(state.velocity_y))
        {
            return ActorPose::idle;
        }
        if (!state.grounded)
            return state.velocity_y < 0.0 ? ActorPose::rise : ActorPose::fall;
        return std::abs(state.velocity_x) >= 0.05
            ? ActorPose::run
            : ActorPose::idle;
    }

    AnimationId default_actor_animation_id(ActorPose pose) noexcept
    {
        switch (pose)
        {
        case ActorPose::idle:
            return stable_animation_id("actor.idle");
        case ActorPose::run:
            return stable_animation_id("actor.run");
        case ActorPose::rise:
            return stable_animation_id("actor.rise");
        case ActorPose::fall:
            return stable_animation_id("actor.fall");
        }
        return {};
    }

    SpriteAnimationSource make_default_actor_source(
        const DefaultActorSheet& sheet,
        std::uint64_t sequence)
    {
        SpriteAnimationSource source{};
        if (!sheet.valid() || sequence == 0u)
            return source;

        const auto rectangle = [&sheet](std::uint32_t tile)
        {
            const std::uint32_t index = (std::min)(
                tile, sheet.tile_count - 1u);
            const std::uint32_t column = index % sheet.grid.x;
            const std::uint32_t row = index / sheet.grid.x;
            return SourceRectangle{
                sheet.margin.x
                    + column * (sheet.tile_extent.x + sheet.spacing.x),
                sheet.margin.y
                    + row * (sheet.tile_extent.y + sheet.spacing.y),
                sheet.tile_extent.x,
                sheet.tile_extent.y};
        };
        const auto frame = [&sheet, &rectangle](
            std::string_view animationName,
            std::uint32_t tile,
            std::uint32_t duration)
        {
            const std::string name = std::string{animationName}
                + ".frame." + std::to_string(tile);
            return FrameSource{
                .id = stable_frame_id(name),
                .material = sheet.material,
                .source_rectangle = rectangle(tile),
                .duration_ticks = duration,
                .pivot_x_q16 = static_cast<std::int32_t>(
                    sheet.tile_extent.x * 32'768u),
                .pivot_y_q16 = static_cast<std::int32_t>(
                    sheet.tile_extent.y * 32'768u)};
        };
        const auto single = [&frame](
            std::string_view name,
            std::uint32_t tile)
        {
            AnimationSource animation{};
            animation.id = stable_animation_id(name);
            animation.name = std::string{name};
            animation.playback = PlaybackMode::once;
            animation.frames.push_back(frame(name, tile, 1u));
            return animation;
        };

        source.id = stable_document_id("project.default_actor.animations");
        source.name = "Default Actor Sprite Animations";
        source.revision.sequence = sequence;

        AnimationSource idle{};
        idle.id = stable_animation_id("actor.idle");
        idle.name = "actor.idle";
        idle.playback = PlaybackMode::loop;
        idle.frames.push_back(frame("actor.idle", 0u, 12u));
        source.animations.push_back(std::move(idle));

        AnimationSource run{};
        run.id = stable_animation_id("actor.run");
        run.name = "actor.run";
        run.playback = PlaybackMode::loop;
        const std::uint32_t runFrames = (std::min)(sheet.tile_count, 4u);
        for (std::uint32_t index = 0u; index < runFrames; ++index)
            run.frames.push_back(frame("actor.run", index, 4u));
        source.animations.push_back(std::move(run));

        source.animations.push_back(single(
            "actor.rise", (std::min)(sheet.tile_count - 1u, 1u)));
        source.animations.push_back(single(
            "actor.fall", (std::min)(sheet.tile_count - 1u, 2u)));

        if (seal_source(source) != ValidationCode::ready)
            return {};
        return source;
    }
    DocumentId stable_document_id(std::string_view stableName) noexcept
    {
        return {stable_value("project.sprite_animation.document", stableName)};
    }

    AnimationId stable_animation_id(std::string_view stableName) noexcept
    {
        return {stable_value("project.sprite_animation.animation", stableName)};
    }

    FrameId stable_frame_id(std::string_view stableName) noexcept
    {
        return {stable_value("project.sprite_animation.frame", stableName)};
    }

    EventId stable_event_id(std::string_view stableName) noexcept
    {
        return {stable_value("project.sprite_animation.event", stableName)};
    }

    authoring::ContentHash source_content_hash(
        const SpriteAnimationSource& source) noexcept
    {
        HashBuilder hash{};
        hash.text("project.sprite_animation.source.v1");
        hash.u64(source.id.value);
        hash.text(source.name);
        hash.u64(source.animations.size());
        for (const AnimationSource& animation : source.animations)
        {
            hash.u64(animation.id.value);
            hash.text(animation.name);
            hash.byte(static_cast<std::uint8_t>(animation.playback));
            hash.u64(animation.frames.size());
            hash.u64(animation.events.size());
            for (const FrameSource& frame : animation.frames)
            {
                hash.u64(frame.id.value);
                hash.text(frame.material.logical_texture_path);
                hash.hash(frame.material.texture_artifact_key);
                hash.u64(frame.material.texture_artifact_revision);
                hash.u32(frame.material.stable_material_key);
                hash.u32(frame.material.texture_extent.x);
                hash.u32(frame.material.texture_extent.y);
                hash.u32(frame.source_rectangle.x);
                hash.u32(frame.source_rectangle.y);
                hash.u32(frame.source_rectangle.width);
                hash.u32(frame.source_rectangle.height);
                hash.u32(frame.duration_ticks);
                hash.i32(frame.pivot_x_q16);
                hash.i32(frame.pivot_y_q16);
            }
            for (const AnimationEventSource& event : animation.events)
            {
                hash.u64(event.id.value);
                hash.u64(event.frame.value);
                hash.u32(event.tick_in_frame);
                hash.byte(static_cast<std::uint8_t>(event.semantic));
                hash.byte(static_cast<std::uint8_t>(event.direction));
                hash.text(event.logical_audio_path);
                hash.u64(event.payload);
            }
        }
        return hash.finish();
    }

    ValidationCode validate_source(
        const SpriteAnimationSource& source,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        return validate_source_impl(
            source, limits, registryLimits, true, true);
    }

    ValidationCode seal_source(
        SpriteAnimationSource& source,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        const ValidationCode initial = validate_source_impl(
            source, limits, registryLimits, false, false);
        if (initial != ValidationCode::ready)
            return initial;
        try
        {
            std::sort(
                source.animations.begin(), source.animations.end(),
                [](const AnimationSource& left, const AnimationSource& right)
                {
                    return left.id < right.id;
                });
            for (AnimationSource& animation : source.animations)
            {
                std::sort(
                    animation.events.begin(), animation.events.end(),
                    [&animation](
                        const AnimationEventSource& left,
                        const AnimationEventSource& right)
                    {
                        const auto leftIndex = frame_index(animation, left.frame);
                        const auto rightIndex = frame_index(animation, right.frame);
                        return std::tuple{
                            *leftIndex, left.tick_in_frame, left.id.value}
                            < std::tuple{
                                *rightIndex, right.tick_in_frame, right.id.value};
                    });
            }
            source.revision.content = source_content_hash(source);
        }
        catch (...)
        {
            return ValidationCode::allocation_failure;
        }
        return validate_source(source, limits, registryLimits);
    }

    EncodedBytes serialize_source(
        const SpriteAnimationSource& source,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (validate_source(source, limits, registryLimits)
            != ValidationCode::ready)
        {
            return {CodecCode::invalid_value};
        }
        try
        {
            ByteWriter payload{limits.maximum_serialized_bytes};
            write_source_payload(payload, source);
            if (!payload.valid())
                return {CodecCode::serialized_budget_exceeded};
            return wrap_payload(
                kSourceMagic, payload.view(), limits.maximum_serialized_bytes);
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    DecodedSource deserialize_source(
        std::span<const std::byte> bytes,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (!limits.valid() || !registryLimits.valid())
            return {CodecCode::invalid_value};
        try
        {
            const EnvelopeView envelope = unwrap_payload(
                bytes, kSourceMagic, limits.maximum_serialized_bytes);
            if (envelope.code != CodecCode::ready)
                return {envelope.code};
            return read_source_payload(envelope.payload, limits, registryLimits);
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    AnimationArtifactDigest artifact_content_hash(
        const CompiledSpriteAnimationArtifact& artifact) noexcept
    {
        HashBuilder hash{};
        hash.text("project.sprite_animation.artifact.v1");
        hash.u32(artifact.schema_version);
        hash.u64(artifact.project_key);
        hash.u64(artifact.asset_key);
        hash.u64(artifact.document_id.value);
        hash.text(artifact.name);
        hash.hash(artifact.source_revision.content);
        hash.u64(artifact.source_revision.sequence);
        hash.u64(artifact.animations.size());
        hash.u64(artifact.frames.size());
        hash.u64(artifact.events.size());
        for (const CompiledAnimation& animation : artifact.animations)
        {
            hash.u64(animation.id.value);
            hash.text(animation.name);
            hash.byte(static_cast<std::uint8_t>(animation.playback));
            hash.u32(animation.first_frame);
            hash.u32(animation.frame_count);
            hash.u32(animation.first_event);
            hash.u32(animation.event_count);
            hash.u64(animation.total_ticks);
        }
        for (const CompiledFrame& frame : artifact.frames)
        {
            hash.u64(frame.id.value);
            hash.u64(frame.material.project_key);
            hash.u64(frame.material.texture_asset_key);
            hash.text(frame.material.logical_texture_path);
            hash.hash(frame.material.texture_artifact_key);
            hash.u64(frame.material.texture_artifact_revision);
            hash.u32(frame.material.stable_material_key);
            hash.u32(frame.material.texture_extent.x);
            hash.u32(frame.material.texture_extent.y);
            hash.u32(frame.source_rectangle.x);
            hash.u32(frame.source_rectangle.y);
            hash.u32(frame.source_rectangle.width);
            hash.u32(frame.source_rectangle.height);
            hash.u64(frame.first_tick);
            hash.u32(frame.duration_ticks);
            hash.i32(frame.pivot_x_q16);
            hash.i32(frame.pivot_y_q16);
        }
        for (const CompiledEvent& event : artifact.events)
        {
            hash.u64(event.id.value);
            hash.u64(event.frame.value);
            hash.u64(event.animation_tick);
            hash.u32(event.tick_in_frame);
            hash.byte(static_cast<std::uint8_t>(event.semantic));
            hash.byte(static_cast<std::uint8_t>(event.direction));
            hash.text(event.logical_audio_path);
            hash.u64(event.audio_asset_key);
            hash.u64(event.payload);
        }
        return {hash.finish().words};
    }

    ValidationCode validate_artifact(
        const CompiledSpriteAnimationArtifact& artifact,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (!limits.valid() || !registryLimits.valid())
            return ValidationCode::invalid_limits;
        if (artifact.schema_version != artifact_schema_version)
            return ValidationCode::unsupported_schema;
        if (artifact.project_key == 0u || artifact.asset_key == 0u
            || !artifact.document_id || !artifact.source_revision)
        {
            return ValidationCode::invalid_artifact;
        }
        if (!valid_text(artifact.name, limits.maximum_name_bytes))
            return ValidationCode::invalid_name;
        const std::uint64_t expectedAsset = project_assets::stable_asset_identity(
            artifact.project_key,
            project_assets::AssetKind::sprite_animation,
            canonical_source_path);
        if (artifact.asset_key != expectedAsset)
            return ValidationCode::invalid_artifact;
        if (artifact.animations.empty())
            return ValidationCode::invalid_animation;
        if (artifact.animations.size() > limits.maximum_animations)
            return ValidationCode::animation_limit_exceeded;
        if (artifact.frames.size() > limits.maximum_frames)
            return ValidationCode::frame_limit_exceeded;
        if (artifact.events.size() > limits.maximum_events)
            return ValidationCode::event_limit_exceeded;

        try
        {
            std::vector<std::uint64_t> animationIds{};
            std::vector<std::uint64_t> frameIds{};
            std::vector<std::uint64_t> eventIds{};
            animationIds.reserve(artifact.animations.size());
            frameIds.reserve(artifact.frames.size());
            eventIds.reserve(artifact.events.size());
            std::uint32_t nextFrame{};
            std::uint32_t nextEvent{};
            AnimationId previousAnimation{};
            for (const CompiledAnimation& animation : artifact.animations)
            {
                if (!animation.id || !valid_playback(animation.playback)
                    || !valid_text(animation.name, limits.maximum_name_bytes)
                    || animation.frame_count == 0u
                    || animation.frame_count
                        > limits.maximum_frames_per_animation
                    || animation.event_count
                        > limits.maximum_events_per_animation
                    || animation.first_frame != nextFrame
                    || animation.first_event != nextEvent
                    || animation.frame_count
                        > artifact.frames.size() - animation.first_frame
                    || animation.event_count
                        > artifact.events.size() - animation.first_event)
                {
                    return ValidationCode::invalid_animation;
                }
                if (previousAnimation && animation.id <= previousAnimation)
                    return ValidationCode::noncanonical_order;
                previousAnimation = animation.id;
                animationIds.push_back(animation.id.value);
                std::uint64_t nextTick{};
                for (std::uint32_t offset = 0u;
                    offset < animation.frame_count; ++offset)
                {
                    const CompiledFrame& frame = artifact.frames[
                        animation.first_frame + offset];
                    frameIds.push_back(frame.id.value);
                    if (!frame.id || frame.first_tick != nextTick
                        || frame.duration_ticks == 0u
                        || frame.duration_ticks
                            > limits.maximum_frame_duration_ticks)
                    {
                        return ValidationCode::invalid_frame;
                    }
                    const MaterialIdentity& material = frame.material;
                    if (material.project_key != artifact.project_key
                        || material.texture_asset_key == 0u
                        || material.texture_artifact_key.empty()
                        || material.texture_artifact_revision == 0u
                        || material.stable_material_key == 0u
                        || material.texture_extent.x == 0u
                        || material.texture_extent.y == 0u
                        || material.texture_extent.x
                            > limits.maximum_texture_dimension
                        || material.texture_extent.y
                            > limits.maximum_texture_dimension
                        || !canonical_path(
                            material.logical_texture_path, registryLimits)
                        || material.texture_asset_key
                            != project_assets::stable_asset_identity(
                                artifact.project_key,
                                project_assets::AssetKind::texture,
                                material.logical_texture_path))
                    {
                        return ValidationCode::invalid_material;
                    }
                    const SourceRectangle& rectangle = frame.source_rectangle;
                    if (rectangle.empty()
                        || rectangle.x >= material.texture_extent.x
                        || rectangle.y >= material.texture_extent.y
                        || rectangle.width
                            > material.texture_extent.x - rectangle.x
                        || rectangle.height
                            > material.texture_extent.y - rectangle.y)
                    {
                        return ValidationCode::invalid_rectangle;
                    }
                    if (nextTick > limits.maximum_animation_duration_ticks
                            - frame.duration_ticks)
                    {
                        return ValidationCode::duration_limit_exceeded;
                    }
                    nextTick += frame.duration_ticks;
                }
                if (animation.total_ticks != nextTick)
                    return ValidationCode::invalid_animation;

                std::tuple<std::uint64_t, std::uint64_t> previousEvent{};
                bool hasPreviousEvent{};
                std::uint64_t previousTick{
                    (std::numeric_limits<std::uint64_t>::max)()};
                std::uint32_t eventsAtTick{};
                for (std::uint32_t offset = 0u;
                    offset < animation.event_count; ++offset)
                {
                    const CompiledEvent& event = artifact.events[
                        animation.first_event + offset];
                    eventIds.push_back(event.id.value);
                    if (!event.id || !valid_semantic(event.semantic)
                        || !valid_direction(event.direction)
                        || event.animation_tick >= animation.total_ticks)
                    {
                        return ValidationCode::invalid_event;
                    }
                    const auto key = std::tuple{
                        event.animation_tick, event.id.value};
                    if (hasPreviousEvent && key <= previousEvent)
                        return ValidationCode::noncanonical_order;
                    previousEvent = key;
                    hasPreviousEvent = true;
                    const CompiledFrame* eventFrame{};
                    for (std::uint32_t frameOffset = 0u;
                        frameOffset < animation.frame_count; ++frameOffset)
                    {
                        const CompiledFrame& candidate = artifact.frames[
                            animation.first_frame + frameOffset];
                        if (candidate.id == event.frame)
                        {
                            eventFrame = &candidate;
                            break;
                        }
                    }
                    if (!eventFrame
                        || event.tick_in_frame >= eventFrame->duration_ticks
                        || event.animation_tick
                            != eventFrame->first_tick + event.tick_in_frame)
                    {
                        return ValidationCode::dangling_frame;
                    }
                    if (cue_required(event.semantic)
                        && event.logical_audio_path.empty())
                    {
                        return ValidationCode::invalid_event;
                    }
                    if (event.logical_audio_path.empty())
                    {
                        if (event.audio_asset_key != 0u)
                            return ValidationCode::invalid_event;
                    }
                    else if (!canonical_path(
                            event.logical_audio_path, registryLimits)
                        || event.audio_asset_key
                            != project_assets::stable_asset_identity(
                                artifact.project_key,
                                project_assets::AssetKind::audio,
                                event.logical_audio_path))
                    {
                        return ValidationCode::invalid_path;
                    }
                    if (event.animation_tick == previousTick)
                        ++eventsAtTick;
                    else
                    {
                        previousTick = event.animation_tick;
                        eventsAtTick = 1u;
                    }
                    if (eventsAtTick > limits.maximum_events_per_tick)
                        return ValidationCode::event_tick_limit_exceeded;
                }
                nextFrame += animation.frame_count;
                nextEvent += animation.event_count;
            }
            if (nextFrame != artifact.frames.size()
                || nextEvent != artifact.events.size())
            {
                return ValidationCode::invalid_artifact;
            }
            const auto duplicate = [](std::vector<std::uint64_t>& ids)
            {
                std::sort(ids.begin(), ids.end());
                return std::adjacent_find(ids.begin(), ids.end()) != ids.end();
            };
            if (duplicate(animationIds))
                return ValidationCode::duplicate_animation;
            if (duplicate(frameIds))
                return ValidationCode::duplicate_frame;
            if (duplicate(eventIds))
                return ValidationCode::duplicate_event;
        }
        catch (...)
        {
            return ValidationCode::allocation_failure;
        }
        if (artifact.artifact_hash != artifact_content_hash(artifact))
            return ValidationCode::content_hash_mismatch;
        return ValidationCode::ready;
    }

    CompileResult compile_artifact(
        std::string_view projectId,
        const SpriteAnimationSource& source,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        const ValidationCode sourceCode = validate_source(
            source, limits, registryLimits);
        if (sourceCode != ValidationCode::ready)
            return {sourceCode};
        const std::uint64_t projectKey = project_assets::stable_project_identity(
            projectId, registryLimits);
        if (projectKey == 0u)
            return {ValidationCode::invalid_project};
        try
        {
            CompiledSpriteAnimationArtifact artifact{};
            artifact.project_key = projectKey;
            artifact.asset_key = project_assets::stable_asset_identity(
                projectKey,
                project_assets::AssetKind::sprite_animation,
                canonical_source_path);
            artifact.document_id = source.id;
            artifact.name = source.name;
            artifact.source_revision = source.revision;
            artifact.animations.reserve(source.animations.size());
            std::size_t frameCount{};
            std::size_t eventCount{};
            for (const AnimationSource& animation : source.animations)
            {
                frameCount += animation.frames.size();
                eventCount += animation.events.size();
            }
            artifact.frames.reserve(frameCount);
            artifact.events.reserve(eventCount);

            for (const AnimationSource& sourceAnimation : source.animations)
            {
                CompiledAnimation animation{};
                animation.id = sourceAnimation.id;
                animation.name = sourceAnimation.name;
                animation.playback = sourceAnimation.playback;
                animation.first_frame = static_cast<std::uint32_t>(
                    artifact.frames.size());
                animation.frame_count = static_cast<std::uint32_t>(
                    sourceAnimation.frames.size());
                animation.first_event = static_cast<std::uint32_t>(
                    artifact.events.size());
                animation.event_count = static_cast<std::uint32_t>(
                    sourceAnimation.events.size());

                std::uint64_t firstTick{};
                for (const FrameSource& sourceFrame : sourceAnimation.frames)
                {
                    MaterialIdentity material{};
                    material.project_key = projectKey;
                    material.logical_texture_path =
                        sourceFrame.material.logical_texture_path;
                    material.texture_asset_key =
                        project_assets::stable_asset_identity(
                            projectKey,
                            project_assets::AssetKind::texture,
                            material.logical_texture_path);
                    material.texture_artifact_key =
                        sourceFrame.material.texture_artifact_key;
                    material.texture_artifact_revision =
                        sourceFrame.material.texture_artifact_revision;
                    material.stable_material_key =
                        sourceFrame.material.stable_material_key;
                    material.texture_extent =
                        sourceFrame.material.texture_extent;
                    artifact.frames.push_back({
                        sourceFrame.id,
                        std::move(material),
                        sourceFrame.source_rectangle,
                        firstTick,
                        sourceFrame.duration_ticks,
                        sourceFrame.pivot_x_q16,
                        sourceFrame.pivot_y_q16});
                    firstTick += sourceFrame.duration_ticks;
                }
                animation.total_ticks = firstTick;

                for (const AnimationEventSource& sourceEvent
                    : sourceAnimation.events)
                {
                    const auto sourceFrameIndex = frame_index(
                        sourceAnimation, sourceEvent.frame);
                    const CompiledFrame& frame = artifact.frames[
                        animation.first_frame + *sourceFrameIndex];
                    const std::uint64_t audioKey =
                        sourceEvent.logical_audio_path.empty()
                        ? 0u
                        : project_assets::stable_asset_identity(
                            projectKey,
                            project_assets::AssetKind::audio,
                            sourceEvent.logical_audio_path);
                    artifact.events.push_back({
                        sourceEvent.id,
                        sourceEvent.frame,
                        frame.first_tick + sourceEvent.tick_in_frame,
                        sourceEvent.tick_in_frame,
                        sourceEvent.semantic,
                        sourceEvent.direction,
                        sourceEvent.logical_audio_path,
                        audioKey,
                        sourceEvent.payload});
                }
                artifact.animations.push_back(std::move(animation));
            }
            artifact.artifact_hash = artifact_content_hash(artifact);
            const ValidationCode code = validate_artifact(
                artifact, limits, registryLimits);
            return code == ValidationCode::ready
                ? CompileResult{code, std::move(artifact)}
                : CompileResult{code};
        }
        catch (...)
        {
            return {ValidationCode::allocation_failure};
        }
    }

    EncodedBytes serialize_artifact(
        const CompiledSpriteAnimationArtifact& artifact,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (validate_artifact(artifact, limits, registryLimits)
            != ValidationCode::ready)
        {
            return {CodecCode::invalid_value};
        }
        try
        {
            ByteWriter payload{limits.maximum_serialized_bytes};
            write_artifact_payload(payload, artifact);
            if (!payload.valid())
                return {CodecCode::serialized_budget_exceeded};
            return wrap_payload(
                kArtifactMagic, payload.view(), limits.maximum_serialized_bytes);
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    DecodedArtifact deserialize_artifact(
        std::span<const std::byte> bytes,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (!limits.valid() || !registryLimits.valid())
            return {CodecCode::invalid_value};
        try
        {
            const EnvelopeView envelope = unwrap_payload(
                bytes, kArtifactMagic, limits.maximum_serialized_bytes);
            if (envelope.code != CodecCode::ready)
                return {envelope.code};
            return read_artifact_payload(
                envelope.payload, limits, registryLimits);
        }
        catch (...)
        {
            return {CodecCode::allocation_failure};
        }
    }

    RuntimeSpriteSample sample_animation(
        const CompiledSpriteAnimationArtifact& artifact,
        const PlaybackRequest& request,
        const AnimationLimits& limits,
        const project_assets::RegistryLimits& registryLimits) noexcept
    {
        if (validate_artifact(artifact, limits, registryLimits)
            != ValidationCode::ready)
        {
            return {SampleCode::invalid_artifact};
        }
        if (!request.animation
            || (request.direction != TemporalDirection::forward
                && request.direction != TemporalDirection::reverse
                && request.direction != TemporalDirection::frozen))
        {
            return {SampleCode::invalid_request};
        }
        const auto found = std::lower_bound(
            artifact.animations.begin(), artifact.animations.end(),
            request.animation,
            [](const CompiledAnimation& animation, AnimationId id)
            {
                return animation.id < id;
            });
        if (found == artifact.animations.end()
            || found->id != request.animation)
        {
            return {SampleCode::animation_not_found};
        }

        const CompiledAnimation& animation = *found;
        const std::uint64_t lastTick = animation.total_ticks - 1u;
        std::uint64_t mappedTick{};
        std::int64_t cycle{};
        bool movingForward{request.direction != TemporalDirection::reverse};
        bool terminal{};
        bool emitEvents{true};

        if (animation.playback == PlaybackMode::once)
        {
            if (request.direction == TemporalDirection::reverse)
            {
                movingForward = false;
                if (request.tick <= 0)
                {
                    mappedTick = lastTick;
                    terminal = request.tick < 0;
                    emitEvents = request.tick >= 0;
                }
                else if (static_cast<std::uint64_t>(request.tick) >= lastTick)
                {
                    mappedTick = 0u;
                    terminal = true;
                    emitEvents = static_cast<std::uint64_t>(request.tick)
                        == lastTick;
                }
                else
                {
                    mappedTick = lastTick
                        - static_cast<std::uint64_t>(request.tick);
                }
            }
            else
            {
                movingForward = true;
                if (request.tick <= 0)
                {
                    mappedTick = 0u;
                    terminal = request.tick < 0;
                    emitEvents = request.tick >= 0;
                }
                else if (static_cast<std::uint64_t>(request.tick) >= lastTick)
                {
                    mappedTick = lastTick;
                    terminal = true;
                    emitEvents = static_cast<std::uint64_t>(request.tick)
                        == lastTick;
                }
                else
                {
                    mappedTick = static_cast<std::uint64_t>(request.tick);
                }
            }
        }
        else if (animation.playback == PlaybackMode::loop)
        {
            mappedTick = floor_mod(request.tick, animation.total_ticks);
            cycle = floor_div(
                request.tick,
                static_cast<std::int64_t>(animation.total_ticks));
            if (request.direction == TemporalDirection::reverse)
            {
                mappedTick = lastTick - mappedTick;
                movingForward = false;
            }
        }
        else
        {
            const std::uint64_t period = animation.total_ticks == 1u
                ? 1u : lastTick * 2u;
            std::uint64_t phase = floor_mod(request.tick, period);
            cycle = floor_div(request.tick, static_cast<std::int64_t>(period));
            if (request.direction == TemporalDirection::reverse && phase != 0u)
                phase = period - phase;
            mappedTick = phase <= lastTick ? phase : period - phase;
            movingForward = animation.total_ticks == 1u
                ? request.direction != TemporalDirection::reverse
                : phase < lastTick;
            if (request.direction == TemporalDirection::frozen)
                movingForward = true;
        }
        if (request.direction == TemporalDirection::frozen)
            terminal = animation.playback == PlaybackMode::once
                && (request.tick < 0
                    || static_cast<std::uint64_t>(request.tick) >= lastTick);

        const CompiledFrame* selected{};
        for (std::uint32_t offset = 0u; offset < animation.frame_count; ++offset)
        {
            const CompiledFrame& frame = artifact.frames[
                animation.first_frame + offset];
            if (mappedTick >= frame.first_tick
                && mappedTick < frame.first_tick + frame.duration_ticks)
            {
                selected = &frame;
                break;
            }
        }
        if (!selected)
            return {SampleCode::invalid_artifact};

        try
        {
            RuntimeSpriteSample result{};
            result.code = SampleCode::ready;
            result.animation = animation.id;
            result.frame = selected->id;
            result.material = selected->material;
            result.source_rectangle = selected->source_rectangle;
            result.animation_tick = mappedTick;
            result.frame_tick = static_cast<std::uint32_t>(
                mappedTick - selected->first_tick);
            result.cycle = cycle;
            result.travel_forward = movingForward;
            result.terminal = terminal;
            if (request.direction != TemporalDirection::frozen
                && emitEvents)
            {
                for (std::uint32_t offset = 0u;
                    offset < animation.event_count; ++offset)
                {
                    const CompiledEvent& event = artifact.events[
                        animation.first_event + offset];
                    if (event.animation_tick == mappedTick
                        && event_allowed(event.direction, movingForward))
                    {
                        if (result.events.size()
                            >= limits.maximum_events_per_tick)
                        {
                            return {SampleCode::event_budget_exceeded};
                        }
                        result.events.push_back(event);
                    }
                }
            }
            return result;
        }
        catch (...)
        {
            return {SampleCode::allocation_failure};
        }
    }

    ProjectSpriteAnimationStore::ProjectSpriteAnimationStore(
        std::string projectId,
        std::filesystem::path projectRoot,
        StoreLimits limits) noexcept
        : project_id_(std::move(projectId)),
          project_root_(std::move(projectRoot)),
          limits_(limits)
    {
        if (!limits_.valid() || project_root_.empty())
            return;
        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0u)
            return;
        try
        {
            std::error_code error{};
            if (project_root_.is_relative())
                project_root_ = fs::absolute(project_root_, error);
            if (error || project_root_.empty())
            {
                project_key_ = 0u;
                project_root_.clear();
                return;
            }
            project_root_ = project_root_.lexically_normal();
        }
        catch (...)
        {
            project_key_ = 0u;
            project_root_.clear();
        }
    }

    bool ProjectSpriteAnimationStore::valid() const noexcept
    {
        return project_key_ != 0u && !project_root_.empty() && limits_.valid();
    }

    std::string_view ProjectSpriteAnimationStore::project_id() const noexcept
    {
        return project_id_;
    }

    std::uint64_t ProjectSpriteAnimationStore::project_key() const noexcept
    {
        return project_key_;
    }

    const std::filesystem::path&
        ProjectSpriteAnimationStore::project_root() const noexcept
    {
        return project_root_;
    }

    std::filesystem::path ProjectSpriteAnimationStore::source_path() const
    {
        return project_root_ / fs::path{canonical_source_path};
    }

    std::filesystem::path ProjectSpriteAnimationStore::artifact_path() const
    {
        return project_root_ / fs::path{canonical_artifact_path};
    }

    const StoreLimits& ProjectSpriteAnimationStore::limits() const noexcept
    {
        return limits_;
    }

    StoreMetrics ProjectSpriteAnimationStore::metrics() const noexcept
    {
        return metrics_;
    }

    StoredSource ProjectSpriteAnimationStore::save_source(
        const SpriteAnimationSource& source) noexcept
    {
        ++metrics_.source_save_requests;
        if (!valid())
            return reject_source(StoreCode::invalid_store);
        const EncodedBytes serialized = serialize_source(
            source, limits_.animation, limits_.registry);
        if (!serialized)
            return reject_source(StoreCode::invalid_value);
        try
        {
            const fs::path destination = source_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error || !fs::is_directory(destination.parent_path(), error)
                || error)
            {
                return reject_source(StoreCode::directory_failure);
            }
            const ReadFileResult existing = read_file(
                destination, limits_.animation.maximum_serialized_bytes);
            if (existing.code == StoreCode::ready)
            {
                metrics_.bytes_read += existing.bytes.size();
                const DecodedSource decoded = deserialize_source(
                    existing.bytes, limits_.animation, limits_.registry);
                if (!decoded)
                    return reject_source(StoreCode::integrity_failure);
                if (existing.bytes == serialized.bytes
                    && decoded.source == source)
                {
                    ++metrics_.unchanged_writes;
                    return {
                        StoreCode::unchanged,
                        destination,
                        source.revision,
                        serialized.bytes.size()};
                }
                const StoreCode comparison = compare_revision(
                    decoded.source.revision, source.revision);
                if (comparison == StoreCode::unchanged)
                    return reject_source(StoreCode::revision_conflict);
                if (comparison != StoreCode::ready)
                    return reject_source(comparison);
            }
            else if (existing.code != StoreCode::not_found)
            {
                return reject_source(existing.code);
            }

            const fs::path temporary = temporary_path(destination);
            const StoreCode written = write_file(temporary, serialized.bytes);
            if (written != StoreCode::ready)
            {
                fs::remove(temporary, error);
                return reject_source(written);
            }
            const ReadFileResult verified = read_file(
                temporary, limits_.animation.maximum_serialized_bytes);
            metrics_.bytes_read += verified.bytes.size();
            const DecodedSource decoded = verified.code == StoreCode::ready
                ? deserialize_source(
                    verified.bytes, limits_.animation, limits_.registry)
                : DecodedSource{};
            if (verified.code != StoreCode::ready
                || verified.bytes != serialized.bytes
                || !decoded || decoded.source != source)
            {
                fs::remove(temporary, error);
                return reject_source(StoreCode::integrity_failure);
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, destination, error))
            {
                fs::remove(temporary, error);
                return reject_source(StoreCode::atomic_replace_failure);
            }
            ++metrics_.source_saves;
            metrics_.bytes_written += serialized.bytes.size();
            return {
                StoreCode::ready,
                destination,
                source.revision,
                serialized.bytes.size()};
        }
        catch (...)
        {
            return reject_source(StoreCode::allocation_failure);
        }
    }

    LoadedSource ProjectSpriteAnimationStore::load_source() noexcept
    {
        ++metrics_.source_load_requests;
        if (!valid())
            return reject_source_load(StoreCode::invalid_store);
        try
        {
            const fs::path path = source_path();
            const ReadFileResult read = read_file(
                path, limits_.animation.maximum_serialized_bytes);
            if (read.code != StoreCode::ready)
                return reject_source_load(read.code);
            metrics_.bytes_read += read.bytes.size();
            DecodedSource decoded = deserialize_source(
                read.bytes, limits_.animation, limits_.registry);
            if (!decoded)
                return reject_source_load(StoreCode::integrity_failure);
            ++metrics_.source_loads;
            return {StoreCode::ready, path, std::move(decoded.source)};
        }
        catch (...)
        {
            return reject_source_load(StoreCode::allocation_failure);
        }
    }

    StoredArtifact ProjectSpriteAnimationStore::publish_artifact(
        const CompiledSpriteAnimationArtifact& artifact) noexcept
    {
        ++metrics_.artifact_save_requests;
        if (!valid())
            return reject_artifact(StoreCode::invalid_store);
        if (artifact.project_key != project_key_)
            return reject_artifact(StoreCode::invalid_value);
        const EncodedBytes serialized = serialize_artifact(
            artifact, limits_.animation, limits_.registry);
        if (!serialized)
            return reject_artifact(StoreCode::invalid_value);
        try
        {
            const ReadFileResult sourceRead = read_file(
                source_path(), limits_.animation.maximum_serialized_bytes);
            if (sourceRead.code != StoreCode::ready)
                return reject_artifact(sourceRead.code);
            metrics_.bytes_read += sourceRead.bytes.size();
            const DecodedSource currentSource = deserialize_source(
                sourceRead.bytes, limits_.animation, limits_.registry);
            if (!currentSource)
                return reject_artifact(StoreCode::integrity_failure);
            const StoreCode sourceComparison = compare_revision(
                currentSource.source.revision, artifact.source_revision);
            if (sourceComparison == StoreCode::stale_revision
                || sourceComparison == StoreCode::revision_conflict)
            {
                return reject_artifact(sourceComparison);
            }
            if (sourceComparison == StoreCode::ready)
                return reject_artifact(StoreCode::revision_conflict);
            const CompileResult expectedArtifact = compile_artifact(
                project_id_,
                currentSource.source,
                limits_.animation,
                limits_.registry);
            if (!expectedArtifact || expectedArtifact.artifact != artifact)
                return reject_artifact(StoreCode::invalid_value);

            const fs::path destination = artifact_path();
            std::error_code error{};
            fs::create_directories(destination.parent_path(), error);
            if (error || !fs::is_directory(destination.parent_path(), error)
                || error)
            {
                return reject_artifact(StoreCode::directory_failure);
            }
            const ReadFileResult existing = read_file(
                destination, limits_.animation.maximum_serialized_bytes);
            if (existing.code == StoreCode::ready)
            {
                metrics_.bytes_read += existing.bytes.size();
                const DecodedArtifact decoded = deserialize_artifact(
                    existing.bytes, limits_.animation, limits_.registry);
                if (!decoded)
                    return reject_artifact(StoreCode::integrity_failure);
                if (existing.bytes == serialized.bytes
                    && decoded.artifact == artifact)
                {
                    ++metrics_.unchanged_writes;
                    return {
                        StoreCode::unchanged,
                        destination,
                        artifact.artifact_hash,
                        artifact.source_revision,
                        serialized.bytes.size()};
                }
                const StoreCode comparison = compare_revision(
                    decoded.artifact.source_revision,
                    artifact.source_revision);
                if (comparison == StoreCode::unchanged)
                    return reject_artifact(StoreCode::revision_conflict);
                if (comparison != StoreCode::ready)
                    return reject_artifact(comparison);
            }
            else if (existing.code != StoreCode::not_found)
            {
                return reject_artifact(existing.code);
            }

            const fs::path temporary = temporary_path(destination);
            const StoreCode written = write_file(temporary, serialized.bytes);
            if (written != StoreCode::ready)
            {
                fs::remove(temporary, error);
                return reject_artifact(written);
            }
            const ReadFileResult verified = read_file(
                temporary, limits_.animation.maximum_serialized_bytes);
            metrics_.bytes_read += verified.bytes.size();
            const DecodedArtifact decoded = verified.code == StoreCode::ready
                ? deserialize_artifact(
                    verified.bytes, limits_.animation, limits_.registry)
                : DecodedArtifact{};
            if (verified.code != StoreCode::ready
                || verified.bytes != serialized.bytes
                || !decoded || decoded.artifact != artifact)
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
            metrics_.bytes_written += serialized.bytes.size();
            return {
                StoreCode::ready,
                destination,
                artifact.artifact_hash,
                artifact.source_revision,
                serialized.bytes.size()};
        }
        catch (...)
        {
            return reject_artifact(StoreCode::allocation_failure);
        }
    }

    LoadedArtifact ProjectSpriteAnimationStore::load_artifact() noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid())
            return reject_artifact_load(StoreCode::invalid_store);
        try
        {
            const fs::path path = artifact_path();
            const ReadFileResult read = read_file(
                path, limits_.animation.maximum_serialized_bytes);
            if (read.code != StoreCode::ready)
                return reject_artifact_load(read.code);
            metrics_.bytes_read += read.bytes.size();
            DecodedArtifact decoded = deserialize_artifact(
                read.bytes, limits_.animation, limits_.registry);
            if (!decoded || decoded.artifact.project_key != project_key_)
                return reject_artifact_load(StoreCode::integrity_failure);

            const ReadFileResult sourceRead = read_file(
                source_path(), limits_.animation.maximum_serialized_bytes);
            if (sourceRead.code == StoreCode::ready)
            {
                metrics_.bytes_read += sourceRead.bytes.size();
                const DecodedSource source = deserialize_source(
                    sourceRead.bytes, limits_.animation, limits_.registry);
                if (!source)
                    return reject_artifact_load(StoreCode::integrity_failure);
                const StoreCode comparison = compare_revision(
                    source.source.revision,
                    decoded.artifact.source_revision);
                if (comparison == StoreCode::ready)
                    return reject_artifact_load(StoreCode::revision_conflict);
                if (comparison != StoreCode::unchanged)
                    return reject_artifact_load(comparison);
                const CompileResult expectedArtifact = compile_artifact(
                    project_id_,
                    source.source,
                    limits_.animation,
                    limits_.registry);
                if (!expectedArtifact
                    || expectedArtifact.artifact != decoded.artifact)
                {
                    return reject_artifact_load(
                        StoreCode::integrity_failure);
                }
            }
            else if (sourceRead.code != StoreCode::not_found)
                return reject_artifact_load(sourceRead.code);

            ++metrics_.artifact_loads;
            return {StoreCode::ready, path, std::move(decoded.artifact)};
        }
        catch (...)
        {
            return reject_artifact_load(StoreCode::allocation_failure);
        }
    }
    PreparedSpriteAnimations prepare_project_sprite_animations(
        std::string_view projectId,
        const std::filesystem::path& projectRoot,
        std::string_view declaredPath,
        const DefaultActorSheet* fallbackSheet) noexcept
    {
        try
        {
            if (projectId.empty() || projectRoot.empty()
                || declaredPath != canonical_source_path)
            {
                return {
                    .code = PreparationCode::invalid_request,
                    .diagnostic = "sprite animation declaration is not canonical"};
            }

            ProjectSpriteAnimationStore store{
                std::string{projectId}, projectRoot};
            if (!store.valid())
            {
                return {
                    .code = PreparationCode::invalid_store,
                    .diagnostic = "project sprite animation store is invalid"};
            }

            std::error_code existsError{};
            const bool sourceExists = fs::exists(
                store.source_path(), existsError) && !existsError;
            if (existsError)
            {
                return {
                    .code = PreparationCode::source_failure,
                    .diagnostic = "sprite animation source existence check failed"};
            }

            if (sourceExists)
            {
                const LoadedSource loaded = store.load_source();
                if (!loaded)
                {
                    return {
                        .code = PreparationCode::source_failure,
                        .diagnostic = std::string{"sprite animation source "}
                            + std::string{store_code_name(loaded.code)}};
                }
                CompileResult compiled = compile_artifact(
                    projectId, loaded.source, store.limits().animation,
                    store.limits().registry);
                if (!compiled)
                {
                    return {
                        .code = PreparationCode::source_compile_failure,
                        .diagnostic = std::string{"sprite animation compile "}
                            + std::string{validation_code_name(compiled.code)}};
                }
                const StoredArtifact published =
                    store.publish_artifact(compiled.artifact);
                if (!published)
                {
                    return {
                        .code = PreparationCode::artifact_failure,
                        .diagnostic = std::string{"sprite animation cache "}
                            + std::string{store_code_name(published.code)}};
                }
                return {
                    .code = PreparationCode::ready,
                    .artifact = std::move(compiled.artifact),
                    .source_materialized = false,
                    .library_changed = published.code == StoreCode::ready,
                    .diagnostic = published.code == StoreCode::unchanged
                        ? "sprite animation source compiled; cache current"
                        : "sprite animation source compiled and cached"};
            }

            LoadedArtifact restored = store.load_artifact();
            if (restored)
            {
                return {
                    .code = PreparationCode::ready,
                    .artifact = std::move(restored.artifact),
                    .source_materialized = false,
                    .library_changed = false,
                    .diagnostic =
                        "compiled sprite animations restored without source"};
            }
            if (restored.code != StoreCode::not_found)
            {
                return {
                    .code = PreparationCode::artifact_failure,
                    .diagnostic = std::string{"sprite animation artifact "}
                        + std::string{store_code_name(restored.code)}};
            }
            if (!fallbackSheet || !fallbackSheet->valid())
            {
                return {
                    .code = PreparationCode::no_animation_data,
                    .diagnostic =
                        "no sprite animation source, artifact, or valid default sheet"};
            }

            SpriteAnimationSource source =
                make_default_actor_source(*fallbackSheet);
            if (!source.revision)
            {
                return {
                    .code = PreparationCode::source_compile_failure,
                    .diagnostic =
                        "default actor sprite animation source was invalid"};
            }
            const StoredSource saved = store.save_source(source);
            if (!saved)
            {
                return {
                    .code = PreparationCode::source_failure,
                    .diagnostic = std::string{"default animation source "}
                        + std::string{store_code_name(saved.code)}};
            }
            CompileResult compiled = compile_artifact(
                projectId, source, store.limits().animation,
                store.limits().registry);
            if (!compiled)
            {
                return {
                    .code = PreparationCode::source_compile_failure,
                    .diagnostic = std::string{"default animation compile "}
                        + std::string{validation_code_name(compiled.code)}};
            }
            const StoredArtifact published =
                store.publish_artifact(compiled.artifact);
            if (!published)
            {
                return {
                    .code = PreparationCode::artifact_failure,
                    .diagnostic = std::string{"default animation cache "}
                        + std::string{store_code_name(published.code)}};
            }
            return {
                .code = PreparationCode::ready,
                .artifact = std::move(compiled.artifact),
                .source_materialized = saved.code == StoreCode::ready,
                .library_changed = published.code == StoreCode::ready,
                .diagnostic =
                    "default actor sprite animations materialized and cached"};
        }
        catch (...)
        {
            return {
                .code = PreparationCode::allocation_failure,
                .diagnostic = "sprite animation preparation allocation failure"};
        }
    }
    StoredSource ProjectSpriteAnimationStore::reject_source(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedSource ProjectSpriteAnimationStore::reject_source_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    StoredArtifact ProjectSpriteAnimationStore::reject_artifact(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedArtifact ProjectSpriteAnimationStore::reject_artifact_load(
        StoreCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }
}
