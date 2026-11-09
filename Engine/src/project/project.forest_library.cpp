/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

module project.forest_library;

import authoring.document;
import authoring.morphology;
import core.sha256;
import platform.filesystem;
import voxel.field;

namespace epochengine::project_forests
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::array<std::uint8_t, 8u> kSourceMagic{
            'E', 'P', 'F', 'S', 'R', 'C', '1', 0u};
        constexpr std::array<std::uint8_t, 8u> kArtifactMagic{
            'E', 'P', 'F', 'A', 'R', 'T', '1', 0u};
        constexpr std::uint16_t kSchemaVersion = 1u;
        constexpr std::uint16_t kIndependentLengthSourceVersion = 2u;
        constexpr std::uint16_t kCanopySourceVersion = 3u;
        constexpr std::uint16_t kDurationHistorySourceVersion = 4u;
        constexpr std::uint16_t kBarkSourceVersion = 5u;
        constexpr std::uint16_t kMaterialSourceVersion = 6u;
        constexpr std::uint16_t kCanopyArtifactVersion = 3u;
        constexpr std::uint32_t kCanopyCompilerRevision = 2u;
        constexpr std::string_view kSourcePrefix{"Assets/Forest/"};
        constexpr std::string_view kSourceExtension{".epoch_forest"};
        constexpr std::string_view kArtifactExtension{
            ".epoch_forest_artifact"};
        constexpr std::size_t kDigestBytes = 32u;

        struct CanonicalAsset final
        {
            std::string logical_path{};
            fs::path source_path{};
            fs::path artifact_directory{};
            std::uint64_t asset_key{};
        };

        struct SourceBytes final
        {
            ForestLibraryCode code{ForestLibraryCode::invalid_source};
            forest::ForestAssetDocument document{};
            project_assets::AssetContentHash digest{};
            std::vector<std::byte> bytes{};
        };

        struct ArtifactBytes final
        {
            ForestLibraryCode code{ForestLibraryCode::invalid_artifact};
            forest::CompiledForestAsset artifact{};
            project_assets::AssetContentHash digest{};
            std::vector<std::byte> bytes{};
        };

        class Writer final
        {
        public:
            explicit Writer(std::uint64_t maximumBytes) noexcept
                : maximum_bytes_(maximumBytes)
            {
            }

            void raw(std::span<const std::uint8_t> source) noexcept
            {
                if (!ok_ || source.size() > remaining())
                {
                    ok_ = false;
                    return;
                }
                try
                {
                    bytes_.reserve(bytes_.size() + source.size());
                    for (const std::uint8_t value : source)
                        bytes_.push_back(static_cast<std::byte>(value));
                }
                catch (...)
                {
                    ok_ = false;
                }
            }

            template<typename Integer>
                requires std::is_integral_v<Integer>
            void integer(Integer value) noexcept
            {
                using Unsigned = std::make_unsigned_t<Integer>;
                Unsigned bits = static_cast<Unsigned>(value);
                std::array<std::uint8_t, sizeof(Integer)> encoded{};
                for (std::size_t index = 0u; index < encoded.size(); ++index)
                {
                    encoded[index] = static_cast<std::uint8_t>(
                        bits >> (index * 8u));
                }
                raw(encoded);
            }

            void boolean(bool value) noexcept
            {
                integer<std::uint8_t>(value ? 1u : 0u);
            }

            void real(float value) noexcept
            {
                integer(std::bit_cast<std::uint32_t>(value));
            }

            void text(std::string_view value) noexcept
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                {
                    ok_ = false;
                    return;
                }
                integer(static_cast<std::uint32_t>(value.size()));
                raw(std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(value.data()),
                    value.size()});
            }

            void byte_vector(std::span<const std::byte> value) noexcept
            {
                if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
                {
                    ok_ = false;
                    return;
                }
                integer(static_cast<std::uint32_t>(value.size()));
                raw(std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(value.data()),
                    value.size()});
            }

            [[nodiscard]] bool ok() const noexcept { return ok_; }
            [[nodiscard]] std::uint64_t remaining() const noexcept
            {
                return bytes_.size() < maximum_bytes_
                    ? maximum_bytes_ - bytes_.size()
                    : 0u;
            }
            [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept
            {
                return bytes_;
            }
            [[nodiscard]] std::vector<std::byte> release() noexcept
            {
                return std::move(bytes_);
            }

        private:
            std::uint64_t maximum_bytes_{};
            std::vector<std::byte> bytes_{};
            bool ok_{true};
        };

        class Reader final
        {
        public:
            explicit Reader(std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool raw(
                std::span<std::uint8_t> destination) noexcept
            {
                if (!ok_ || destination.size() > remaining())
                {
                    ok_ = false;
                    return false;
                }
                for (std::size_t index = 0u; index < destination.size(); ++index)
                {
                    destination[index] = std::to_integer<std::uint8_t>(
                        bytes_[position_ + index]);
                }
                position_ += destination.size();
                return true;
            }

            template<typename Integer>
                requires std::is_integral_v<Integer>
            [[nodiscard]] Integer integer() noexcept
            {
                using Unsigned = std::make_unsigned_t<Integer>;
                std::array<std::uint8_t, sizeof(Integer)> encoded{};
                if (!raw(encoded))
                    return {};
                Unsigned value{};
                for (std::size_t index = 0u; index < encoded.size(); ++index)
                {
                    value |= static_cast<Unsigned>(encoded[index])
                        << (index * 8u);
                }
                return static_cast<Integer>(value);
            }

            [[nodiscard]] bool boolean() noexcept
            {
                const std::uint8_t value = integer<std::uint8_t>();
                if (value > 1u)
                    ok_ = false;
                return value != 0u;
            }

            [[nodiscard]] float real() noexcept
            {
                return std::bit_cast<float>(integer<std::uint32_t>());
            }

            [[nodiscard]] std::string text(std::size_t maximumBytes) noexcept
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (!ok_ || size > maximumBytes || size > remaining())
                {
                    ok_ = false;
                    return {};
                }
                std::string value{};
                try
                {
                    value.resize(size);
                }
                catch (...)
                {
                    ok_ = false;
                    return {};
                }
                for (std::size_t index = 0u; index < size; ++index)
                {
                    value[index] = static_cast<char>(
                        std::to_integer<std::uint8_t>(bytes_[position_ + index]));
                }
                position_ += size;
                return value;
            }

            [[nodiscard]] std::vector<std::byte> byte_vector(
                std::size_t maximumBytes) noexcept
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (!ok_ || size > maximumBytes || size > remaining())
                {
                    ok_ = false;
                    return {};
                }
                std::vector<std::byte> value{};
                try
                {
                    value.assign(
                        bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
                        bytes_.begin() + static_cast<std::ptrdiff_t>(
                            position_ + size));
                }
                catch (...)
                {
                    ok_ = false;
                    return {};
                }
                position_ += size;
                return value;
            }

            [[nodiscard]] std::span<const std::byte> byte_span(std::size_t maximumBytes) noexcept
            {
                const auto size = integer<std::uint32_t>();
                if (!ok_ || size > maximumBytes || size > remaining())
                {
                    ok_ = false;
                    return {};
                }
                const auto result = bytes_.subspan(position_, size);
                position_ += size;
                return result;
            }

            [[nodiscard]] bool finished() const noexcept
            {
                return ok_ && position_ == bytes_.size();
            }
            [[nodiscard]] bool ok() const noexcept { return ok_; }
            [[nodiscard]] std::size_t remaining() const noexcept
            {
                return position_ <= bytes_.size()
                    ? bytes_.size() - position_
                    : 0u;
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t position_{};
            bool ok_{true};
        };

        [[nodiscard]] core::sha256::Digest digest_bytes(
            std::span<const std::byte> bytes) noexcept
        {
            return core::sha256::hash(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(bytes.data()),
                bytes.size()});
        }

        [[nodiscard]] project_assets::AssetContentHash asset_hash(
            const core::sha256::Digest& digest) noexcept
        {
            project_assets::AssetContentHash result{};
            for (std::size_t word = 0u; word < result.words.size(); ++word)
            {
                std::uint64_t value{};
                for (std::size_t byte = 0u; byte < 8u; ++byte)
                {
                    value = (value << 8u)
                        | digest.bytes[word * 8u + byte];
                }
                result.words[word] = value;
            }
            return result;
        }

        [[nodiscard]] std::string hex_hash(
            const project_assets::AssetContentHash& hash)
        {
            static constexpr std::array<char, 16u> digits{
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            std::string result(64u, '0');
            std::size_t offset{};
            for (const std::uint64_t word : hash.words)
            {
                for (int shift = 60; shift >= 0; shift -= 4)
                    result[offset++] = digits[(word >> shift) & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] std::string hex_u64(std::uint64_t value)
        {
            static constexpr std::array<char, 16u> digits{
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
            std::string result(16u, '0');
            for (std::size_t index = 0u; index < result.size(); ++index)
            {
                const std::size_t shift = (result.size() - 1u - index) * 4u;
                result[index] = digits[(value >> shift) & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] bool append_digest(
            std::vector<std::byte>& bytes,
            std::uint64_t maximumBytes) noexcept
        {
            if (bytes.size() + kDigestBytes > maximumBytes)
                return false;
            const core::sha256::Digest digest = digest_bytes(bytes);
            try
            {
                bytes.reserve(bytes.size() + digest.bytes.size());
                for (const std::uint8_t value : digest.bytes)
                    bytes.push_back(static_cast<std::byte>(value));
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] std::optional<std::span<const std::byte>>
            integrity_verified_body(std::span<const std::byte> bytes) noexcept
        {
            if (bytes.size() <= kDigestBytes)
                return std::nullopt;
            const std::span<const std::byte> body =
                bytes.first(bytes.size() - kDigestBytes);
            const core::sha256::Digest expected = digest_bytes(body);
            for (std::size_t index = 0u; index < kDigestBytes; ++index)
            {
                if (bytes[body.size() + index]
                    != static_cast<std::byte>(expected.bytes[index]))
                {
                    return std::nullopt;
                }
            }
            return body;
        }

        [[nodiscard]] bool valid_text(
            std::string_view value,
            std::uint32_t maximumBytes) noexcept
        {
            if (value.empty() || value.size() > maximumBytes)
                return false;
            for (const unsigned char character : value)
            {
                if (character == 0u || character == 0x7fu
                    || character < 0x20u)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool finite_profile(
            const forest::ForestFactoryProfile& profile) noexcept
        {
            const std::array values{
                profile.temporal.timeSeconds,
                profile.temporal.speed,
                profile.temporal.durationSeconds,
                profile.branch.branchesPerNode,
                profile.branch.nodeStepMeters,
                profile.branch.startHeightMeters,
                profile.branch.branchLengthMeters,
                profile.branch.lengthReferenceMeters,
                profile.leafSizeMeters,
                profile.branch.angleDegrees,
                profile.branch.spreadDegrees,
                profile.branch.twistDegrees,
                profile.branch.jitterDegrees,
                profile.branch.upwardBend,
                profile.branch.outwardBias,
                profile.branch.curve,
                profile.branch.sag,
                profile.config.trunkRadiusMeters,
                profile.config.targetHeightMeters};
            return std::all_of(
                values.begin(), values.end(),
                [](float value) { return std::isfinite(value); });
        }

        [[nodiscard]] bool valid_source_document(
            const forest::ForestAssetDocument& document,
            const ForestLibraryLimits& limits,
            bool requireCompilation = true) noexcept
        {
            if (document.genome.stableHash == 0u
                || !valid_text(document.name, limits.maximum_name_bytes)
                || document.revision == 0u
                || document.contentHash == 0u
                || document.contentHash
                    != forest::forest_profile_content_hash(document.profile)
                || document.nextSequence == 0u
                || document.journal.size()
                    > forest::kForestMaximumProfileEdits
                || document.historyCursor > document.journal.size()
                || !finite_profile(document.profile)
                || static_cast<std::uint8_t>(document.profile.preset)
                    > static_cast<std::uint8_t>(forest::ForestPreset::Fern)
                || static_cast<std::uint8_t>(document.profile.previewMode)
                    > static_cast<std::uint8_t>(
                        forest::ForestPreviewMode::Mode3D)
                || static_cast<std::uint8_t>(document.profile.editStage)
                    > static_cast<std::uint8_t>(
                        forest::ForestEditStage::Foliage)
                || document.profile.branch.levels > 32u
                || document.profile.branch.childrenPerNode == 0u
                || document.profile.branch.childrenPerNode > 8u
                || document.profile.branch.lengthReferenceMeters < 0.0f
                || document.profile.branch.lengthReferenceMeters > 128.0f
                || document.profile.leafSizeMeters < 0.0f
                || document.profile.leafSizeMeters > 2.0f
                || document.profile.leafSitesPerBranch > 16u
                || document.profile.barkTileIndex >= 16u
                || (!document.profile.barkTexture.empty() && !document.profile.barkTexture.valid())
                || (!document.profile.foliageTexture.empty() && !document.profile.foliageTexture.valid())
                || document.profile.config.maxBranchDepth == 0u
                || document.profile.config.maxBranchDepth > 32u
                || document.profile.config.maxPreviewSegments == 0u
                || document.profile.config.maxPreviewSegments > 65'536u
                || document.profile.config.targetHeightMeters < 0.1f
                || document.profile.config.targetHeightMeters > 256.0f
                || document.profile.config.trunkRadiusMeters < 0.005f
                || document.profile.config.trunkRadiusMeters > 8.0f
                || document.profile.temporal.durationSeconds <= 0.0f
                || document.profile.temporal.timeSeconds < 0.0f
                || document.profile.temporal.timeSeconds
                    > document.profile.temporal.durationSeconds)
            {
                return false;
            }

            std::uint64_t previous{};
            for (const forest::ForestProfileEdit& edit : document.journal)
            {
                if (edit.sequence == 0u || edit.sequence <= previous
                    || edit.sequence >= document.nextSequence
                    || !std::isfinite(edit.beforeValue)
                    || !std::isfinite(edit.afterValue)
                    || !std::isfinite(edit.beforeLengthReferenceMeters)
                    || !std::isfinite(edit.afterLengthReferenceMeters)
                    || edit.beforeLengthReferenceMeters < 0.0f
                    || edit.beforeLengthReferenceMeters > 128.0f
                    || edit.afterLengthReferenceMeters < 0.0f
                    || edit.afterLengthReferenceMeters > 128.0f
                    || static_cast<std::uint8_t>(edit.property)
                        > static_cast<std::uint8_t>(
                            forest::ForestProfileProperty::FoliageTexture))
                {
                    return false;
                }
                if (!std::isfinite(edit.beforeDurationTimeSeconds) || !std::isfinite(edit.afterDurationTimeSeconds)) return false;
                if (edit.hasDurationTimeSnapshot)
                {
                    if (edit.property != forest::ForestProfileProperty::GrowthDurationSeconds
                        || edit.beforeValue < 0.001f || edit.beforeValue > 86400.0f
                        || edit.afterValue < 0.001f || edit.afterValue > 86400.0f
                        || edit.beforeDurationTimeSeconds < 0.0f || edit.beforeDurationTimeSeconds > edit.beforeValue
                        || edit.afterDurationTimeSeconds < 0.0f || edit.afterDurationTimeSeconds > edit.afterValue) return false;
                }
                else if (edit.beforeDurationTimeSeconds != 0.0f || edit.afterDurationTimeSeconds != 0.0f) return false;
                const bool material = edit.property == forest::ForestProfileProperty::BarkTexture
                    || edit.property == forest::ForestProfileProperty::FoliageTexture;
                if (material)
                {
                    if ((!edit.beforeTexture.empty() && !edit.beforeTexture.valid())
                        || (!edit.afterTexture.empty() && !edit.afterTexture.valid())
                        || edit.beforeTexture == edit.afterTexture || edit.beforeValue != 0.0f || edit.afterValue != 0.0f
                        || edit.beforeLengthReferenceMeters != edit.afterLengthReferenceMeters) return false;
                }
                else if (!edit.beforeTexture.empty() || !edit.afterTexture.empty()) return false;
                previous = edit.sequence;
            }

            if (!requireCompilation) return true;
            const forest::CompiledForestAsset compiled =
                forest::compile_forest_asset(document);
            return compiled.valid
                && compiled.sourceRevision == document.revision
                && compiled.sourceContentHash == document.contentHash
                && compiled.genome.stableHash == document.genome.stableHash;
        }

        void write_profile(
            Writer& writer,
            const forest::ForestFactoryProfile& profile) noexcept
        {
            writer.integer(static_cast<std::uint8_t>(profile.preset));
            writer.integer(static_cast<std::uint8_t>(profile.previewMode));
            writer.integer(static_cast<std::uint8_t>(profile.editStage));
            writer.integer(profile.seed.value);
            writer.real(profile.temporal.timeSeconds);
            writer.real(profile.temporal.speed);
            writer.real(profile.temporal.durationSeconds);
            writer.boolean(profile.temporal.playing);
            writer.boolean(profile.temporal.reverse);
            writer.real(profile.branch.branchesPerNode);
            writer.real(profile.branch.nodeStepMeters);
            writer.real(profile.branch.startHeightMeters);
            writer.real(profile.branch.branchLengthMeters);
            writer.integer(profile.branch.levels);
            writer.integer(profile.branch.childrenPerNode);
            writer.real(profile.branch.angleDegrees);
            writer.real(profile.branch.spreadDegrees);
            writer.real(profile.branch.twistDegrees);
            writer.real(profile.branch.jitterDegrees);
            writer.real(profile.branch.upwardBend);
            writer.real(profile.branch.outwardBias);
            writer.real(profile.branch.curve);
            writer.real(profile.branch.sag);
            writer.boolean(profile.display.veins);
            writer.boolean(profile.display.nodes);
            writer.boolean(profile.display.wire);
            writer.boolean(profile.display.texture);
            writer.integer(profile.config.maxBranchDepth);
            writer.integer(profile.config.maxPreviewSegments);
            writer.real(profile.config.trunkRadiusMeters);
            writer.real(profile.config.targetHeightMeters);
            writer.boolean(profile.config.deterministicWindProfile);
            writer.boolean(profile.config.emitVoxelOccupancy);
            writer.integer(profile.atlasIndex);
        }

        [[nodiscard]] forest::ForestFactoryProfile read_profile(
            Reader& reader) noexcept
        {
            forest::ForestFactoryProfile profile{};
            profile.preset = static_cast<forest::ForestPreset>(
                reader.integer<std::uint8_t>());
            profile.previewMode = static_cast<forest::ForestPreviewMode>(
                reader.integer<std::uint8_t>());
            profile.editStage = static_cast<forest::ForestEditStage>(
                reader.integer<std::uint8_t>());
            profile.seed.value = reader.integer<std::uint64_t>();
            profile.temporal.timeSeconds = reader.real();
            profile.temporal.speed = reader.real();
            profile.temporal.durationSeconds = reader.real();
            profile.temporal.playing = reader.boolean();
            profile.temporal.reverse = reader.boolean();
            profile.branch.branchesPerNode = reader.real();
            profile.branch.nodeStepMeters = reader.real();
            profile.branch.startHeightMeters = reader.real();
            profile.branch.branchLengthMeters = reader.real();
            profile.branch.levels = reader.integer<std::uint32_t>();
            profile.branch.childrenPerNode = reader.integer<std::uint32_t>();
            profile.branch.angleDegrees = reader.real();
            profile.branch.spreadDegrees = reader.real();
            profile.branch.twistDegrees = reader.real();
            profile.branch.jitterDegrees = reader.real();
            profile.branch.upwardBend = reader.real();
            profile.branch.outwardBias = reader.real();
            profile.branch.curve = reader.real();
            profile.branch.sag = reader.real();
            profile.display.veins = reader.boolean();
            profile.display.nodes = reader.boolean();
            profile.display.wire = reader.boolean();
            profile.display.texture = reader.boolean();
            profile.config.maxBranchDepth = reader.integer<std::uint32_t>();
            profile.config.maxPreviewSegments = reader.integer<std::uint32_t>();
            profile.config.trunkRadiusMeters = reader.real();
            profile.config.targetHeightMeters = reader.real();
            profile.config.deterministicWindProfile = reader.boolean();
            profile.config.emitVoxelOccupancy = reader.boolean();
            profile.atlasIndex = reader.integer<std::uint32_t>();
            return profile;
        }

        void write_texture_reference(Writer& writer, const forest::ForestTextureReference& reference) noexcept
        {
            writer.text(reference.logicalPath);
            for (const auto word : reference.artifactKey) writer.integer(word);
        }

        [[nodiscard]] forest::ForestTextureReference read_texture_reference(Reader& reader) noexcept
        {
            forest::ForestTextureReference reference;
            reference.logicalPath = reader.text(1024u);
            for (auto& word : reference.artifactKey) word = reader.integer<std::uint64_t>();
            return reference;
        }

        [[nodiscard]] std::vector<std::byte> serialize_source(
            const forest::ForestAssetDocument& document,
            const ForestLibraryLimits& limits,
            bool requireCompilation = true) noexcept
        {
            if (!valid_source_document(document, limits, requireCompilation))
                return {};
            Writer writer{limits.maximum_source_bytes - kDigestBytes};
            writer.raw(kSourceMagic);
            const bool independentLength = document.profile.branch.lengthReferenceMeters != 0.0f
                || std::any_of(document.journal.begin(), document.journal.end(), [](const auto& edit) {
                    return edit.beforeLengthReferenceMeters != 0.0f
                        || edit.afterLengthReferenceMeters != 0.0f;
                });
            const bool canopy = document.profile.leafSizeMeters != 0.0f || document.profile.leafSitesPerBranch != 0u
                || std::any_of(document.journal.begin(), document.journal.end(), [](const auto& edit) {
                    return edit.property == forest::ForestProfileProperty::LeafSizeMeters
                        || edit.property == forest::ForestProfileProperty::LeafSitesPerBranch; });
            const bool durationHistory = std::any_of(document.journal.begin(), document.journal.end(),
                [](const auto& edit) { return edit.hasDurationTimeSnapshot; });
            const bool bark = document.profile.barkTileIndex != 0u
                || std::any_of(document.journal.begin(), document.journal.end(), [](const auto& edit) {
                    return edit.property == forest::ForestProfileProperty::BarkTileIndex; });
            const bool material = !document.profile.barkTexture.empty() || !document.profile.foliageTexture.empty()
                || std::any_of(document.journal.begin(), document.journal.end(), [](const auto& edit) {
                    return edit.property == forest::ForestProfileProperty::BarkTexture
                        || edit.property == forest::ForestProfileProperty::FoliageTexture; });
            const auto version = material ? kMaterialSourceVersion : bark ? kBarkSourceVersion : durationHistory ? kDurationHistorySourceVersion
                : canopy ? kCanopySourceVersion : independentLength ? kIndependentLengthSourceVersion : kSchemaVersion;
            writer.integer(version);
            writer.integer(document.genome.stableHash);
            writer.text(document.name);
            write_profile(writer, document.profile);
            if (version >= kIndependentLengthSourceVersion) writer.real(document.profile.branch.lengthReferenceMeters);
            if (version >= kCanopySourceVersion) { writer.real(document.profile.leafSizeMeters); writer.integer(document.profile.leafSitesPerBranch); }
            if (version >= kBarkSourceVersion) writer.integer(document.profile.barkTileIndex);
            if (version >= kMaterialSourceVersion)
            {
                write_texture_reference(writer, document.profile.barkTexture);
                write_texture_reference(writer, document.profile.foliageTexture);
            }
            writer.integer(document.revision);
            writer.integer(document.contentHash);
            writer.integer(document.nextSequence);
            writer.integer(static_cast<std::uint32_t>(document.journal.size()));
            writer.integer(static_cast<std::uint32_t>(document.historyCursor));
            for (const forest::ForestProfileEdit& edit : document.journal)
            {
                writer.integer(edit.sequence);
                writer.integer(static_cast<std::uint8_t>(edit.property));
                writer.real(edit.beforeValue);
                writer.real(edit.afterValue);
                if (version >= kIndependentLengthSourceVersion)
                {
                    writer.real(edit.beforeLengthReferenceMeters);
                    writer.real(edit.afterLengthReferenceMeters);
                }
                if (version >= kDurationHistorySourceVersion)
                {
                    writer.integer(static_cast<std::uint8_t>(edit.hasDurationTimeSnapshot));
                    writer.real(edit.beforeDurationTimeSeconds);
                    writer.real(edit.afterDurationTimeSeconds);
                }
                if (version >= kMaterialSourceVersion)
                {
                    write_texture_reference(writer, edit.beforeTexture);
                    write_texture_reference(writer, edit.afterTexture);
                }
            }
            if (!writer.ok())
                return {};
            std::vector<std::byte> bytes = writer.release();
            return append_digest(bytes, limits.maximum_source_bytes)
                ? std::move(bytes)
                : std::vector<std::byte>{};
        }

        [[nodiscard]] SourceBytes deserialize_source(
            std::span<const std::byte> bytes,
            const ForestLibraryLimits& limits,
            bool requireCompilation = true) noexcept
        {
            if (bytes.size() > limits.maximum_source_bytes)
                return {ForestLibraryCode::source_too_large};
            const auto body = integrity_verified_body(bytes);
            if (!body)
                return {ForestLibraryCode::integrity_failure};
            Reader reader{*body};
            std::array<std::uint8_t, kSourceMagic.size()> magic{};
            if (!reader.raw(magic) || magic != kSourceMagic)
            {
                return {ForestLibraryCode::invalid_source};
            }
            const auto version = reader.integer<std::uint16_t>();
            if (version != kSchemaVersion && version != kIndependentLengthSourceVersion && version != kCanopySourceVersion && version != kDurationHistorySourceVersion && version != kBarkSourceVersion && version != kMaterialSourceVersion)
                return {ForestLibraryCode::invalid_source};

            forest::ForestAssetDocument document{};
            document.genome.stableHash = reader.integer<std::uint64_t>();
            document.name = reader.text(limits.maximum_name_bytes);
            document.profile = read_profile(reader);
            if (version >= kIndependentLengthSourceVersion)
                document.profile.branch.lengthReferenceMeters = reader.real();
            if (version >= kCanopySourceVersion) { document.profile.leafSizeMeters = reader.real(); document.profile.leafSitesPerBranch = reader.integer<std::uint32_t>(); }
            if (version >= kBarkSourceVersion) document.profile.barkTileIndex = reader.integer<std::uint32_t>();
            if (version >= kMaterialSourceVersion)
            {
                document.profile.barkTexture = read_texture_reference(reader);
                document.profile.foliageTexture = read_texture_reference(reader);
            }
            document.revision = reader.integer<std::uint64_t>();
            document.contentHash = reader.integer<std::uint64_t>();
            document.nextSequence = reader.integer<std::uint64_t>();
            const std::uint32_t journalCount = reader.integer<std::uint32_t>();
            document.historyCursor = reader.integer<std::uint32_t>();
            if (!reader.ok() || journalCount > forest::kForestMaximumProfileEdits)
                return {ForestLibraryCode::invalid_source};
            try
            {
                document.journal.reserve(journalCount);
                for (std::uint32_t index = 0u; index < journalCount; ++index)
                {
                    document.journal.push_back({
                        reader.integer<std::uint64_t>(),
                        static_cast<forest::ForestProfileProperty>(
                            reader.integer<std::uint8_t>()),
                        reader.real(),
                        reader.real()});
                    if (version >= kIndependentLengthSourceVersion)
                    {
                        document.journal.back().beforeLengthReferenceMeters = reader.real();
                        document.journal.back().afterLengthReferenceMeters = reader.real();
                    }
                    if (version >= kDurationHistorySourceVersion)
                    {
                        const auto present = reader.integer<std::uint8_t>();
                        if (present > 1u) return {ForestLibraryCode::invalid_source};
                        auto& edit = document.journal.back();
                        edit.hasDurationTimeSnapshot = present != 0u;
                        edit.beforeDurationTimeSeconds = reader.real();
                        edit.afterDurationTimeSeconds = reader.real();
                    }
                    if (version >= kMaterialSourceVersion)
                    {
                        document.journal.back().beforeTexture = read_texture_reference(reader);
                        document.journal.back().afterTexture = read_texture_reference(reader);
                    }
                }
            }
            catch (...)
            {
                return {ForestLibraryCode::allocation_failure};
            }
            if (!reader.finished() || !valid_source_document(document, limits, requireCompilation))
                return {ForestLibraryCode::invalid_source};
            const auto digest = asset_hash(digest_bytes(*body));
            std::vector<std::byte> storedBytes{};
            try
            {
                storedBytes.assign(bytes.begin(), bytes.end());
            }
            catch (...)
            {
                return {ForestLibraryCode::allocation_failure};
            }
            return {
                ForestLibraryCode::ready,
                std::move(document),
                digest,
                std::move(storedBytes)};
        }

        void write_float3(Writer& writer, voxel::Float3 value) noexcept
        {
            writer.real(value.x);
            writer.real(value.y);
            writer.real(value.z);
        }

        void write_chunk(Writer& writer, const voxel::ChunkDesc& chunk) noexcept
        {
            writer.integer(chunk.cellsX);
            writer.integer(chunk.cellsY);
            writer.integer(chunk.cellsZ);
            writer.real(chunk.cellSizeMeters);
            writer.integer(chunk.lodLevel);
        }

        [[nodiscard]] std::vector<std::byte> compiled_descriptor(
            const forest::CompiledForestAsset& artifact,
            std::uint64_t maximumBytes) noexcept
        {
            if (!artifact.valid
                || !authoring::morphology::validate(artifact.graph)
                || artifact.preview.segmentCount
                    > forest::kForestPreviewMaxSegments
                || artifact.preview.leafCount
                    > forest::kForestPreviewMaxLeaves)
            {
                return {};
            }
            Writer writer{maximumBytes};
            writer.integer(artifact.genome.stableHash);
            writer.integer(artifact.sourceRevision);
            writer.integer(artifact.sourceContentHash);
            writer.integer(artifact.graph.content_hash);
            writer.integer(static_cast<std::uint32_t>(artifact.graph.nodes.size()));
            writer.integer(static_cast<std::uint32_t>(artifact.graph.segments.size()));
            writer.integer(static_cast<std::uint32_t>(artifact.graph.terminals.size()));
            writer.boolean(artifact.graph.truncated);
            writer.real(artifact.sample.time_seconds);
            writer.integer(static_cast<std::uint32_t>(artifact.sample.segments.size()));
            writer.integer(static_cast<std::uint32_t>(artifact.sample.terminals.size()));
            writer.integer(static_cast<std::uint32_t>(artifact.preview.segmentCount));
            for (std::size_t index = 0u;
                index < artifact.preview.segmentCount; ++index)
            {
                const forest::ForestPreviewSegment& segment =
                    artifact.preview.segments[index];
                write_float3(writer, segment.start);
                write_float3(writer, segment.end);
                writer.real(segment.radius);
                writer.integer(segment.depth);
            }
            writer.integer(static_cast<std::uint32_t>(artifact.preview.leafCount));
            for (std::size_t index = 0u;
                index < artifact.preview.leafCount; ++index)
            {
                const forest::ForestPreviewLeaf& leaf =
                    artifact.preview.leaves[index];
                write_float3(writer, leaf.position);
                writer.real(leaf.size);
                writer.integer(leaf.sourceSegment);
            }
            writer.integer(artifact.preview.stats.nodes);
            writer.integer(artifact.preview.stats.branches);
            writer.integer(artifact.preview.stats.leaves);
            writer.integer(artifact.preview.stats.vertices);
            writer.integer(artifact.preview.stats.triangles);
            writer.integer(static_cast<std::uint32_t>(
                artifact.voxelLods.levels.size()));
            for (const authoring::morphology::VoxelLodLevel& level
                : artifact.voxelLods.levels)
            {
                write_chunk(writer, level.chunk);
                for (const float value : level.bounds_min_meters)
                    writer.real(value);
                for (const float value : level.bounds_max_meters)
                    writer.real(value);
                writer.real(level.minimum_view_distance_meters);
                writer.integer(level.estimated_active_cells);
                writer.integer(level.dense_equivalent_bytes);
            }
            writer.integer(static_cast<std::uint16_t>(
                artifact.voxelLods.semantics));
            writer.integer(artifact.voxelLods.source_content_hash);
            write_chunk(writer, artifact.voxelOccupancy.chunk);
            writer.integer(static_cast<std::uint16_t>(
                artifact.voxelOccupancy.semantics));
            for (const float value : artifact.voxelOccupancy.boundsMinMeters)
                writer.real(value);
            for (const float value : artifact.voxelOccupancy.boundsMaxMeters)
                writer.real(value);
            writer.integer(artifact.voxelOccupancy.trunkCells);
            writer.integer(artifact.voxelOccupancy.branchCells);
            writer.integer(artifact.voxelOccupancy.foliageCells);
            writer.integer(artifact.voxelOccupancy.activeCells);
            writer.integer(artifact.voxelOccupancy.denseBytes);
            writer.boolean(artifact.valid);
            return writer.ok() ? writer.release() : std::vector<std::byte>{};
        }


        constexpr std::uint16_t kMaterializedArtifactVersion = 2u;
        constexpr std::string_view kMaterializedProducer{"epoch.forest.factory.materialized.v1"};

        template<typename Id>
        void write_materialized_id(Writer& writer, const Id& id) noexcept
        {
            writer.integer(id.index);
            writer.integer(id.generation);
        }

        template<typename Id>
        Id read_materialized_id(Reader& reader) noexcept
        {
            return Id{reader.integer<std::uint32_t>(), reader.integer<std::uint32_t>()};
        }

        constexpr auto growth_float_members() noexcept
        {
            using Growth = authoring::morphology::GrowthParameters;
            return std::array{
                &Growth::root_length_meters, &Growth::segment_length_meters,
                &Growth::length_decay, &Growth::root_radius_meters, &Growth::radius_decay,
                &Growth::branch_angle_degrees, &Growth::spread_degrees, &Growth::twist_degrees,
                &Growth::jitter_degrees, &Growth::upward_bias, &Growth::outward_bias,
                &Growth::sag, &Growth::sapling_pre_age_seconds, &Growth::generation_delay_seconds,
                &Growth::segment_growth_seconds, &Growth::terminal_delay_seconds,
                &Growth::terminal_growth_seconds};
        }

        void write_materialized_recipe(Writer& writer,
            const authoring::morphology::Recipe& recipe) noexcept
        {
            write_materialized_id(writer, recipe.document);
            for (const auto word : recipe.revision.content.words) writer.integer(word);
            writer.integer(recipe.revision.sequence);
            writer.integer(static_cast<std::uint8_t>(recipe.domain));
            writer.integer(static_cast<std::uint8_t>(recipe.dimension));
            writer.integer(recipe.growth.seed);
            writer.integer(recipe.growth.generations);
            writer.integer(recipe.growth.children_per_node);
            for (const auto member : growth_float_members()) writer.real(recipe.growth.*member);
            writer.integer(recipe.limits.maximum_depth);
            writer.integer(recipe.limits.maximum_children_per_node);
            writer.integer(recipe.limits.maximum_nodes);
            writer.integer(recipe.limits.maximum_segments);
            writer.integer(recipe.limits.maximum_terminals);
            writer.real(recipe.duration_seconds);
        }

        authoring::morphology::Recipe read_materialized_recipe(Reader& reader) noexcept
        {
            authoring::morphology::Recipe recipe{};
            recipe.document = read_materialized_id<authoring::DocumentHandle>(reader);
            for (auto& word : recipe.revision.content.words) word = reader.integer<std::uint64_t>();
            recipe.revision.sequence = reader.integer<std::uint64_t>();
            recipe.domain = static_cast<authoring::morphology::Domain>(reader.integer<std::uint8_t>());
            recipe.dimension = static_cast<authoring::morphology::DimensionMode>(reader.integer<std::uint8_t>());
            recipe.growth.seed = reader.integer<std::uint64_t>();
            recipe.growth.generations = reader.integer<std::uint32_t>();
            recipe.growth.children_per_node = reader.integer<std::uint32_t>();
            for (const auto member : growth_float_members()) recipe.growth.*member = reader.real();
            recipe.limits.maximum_depth = reader.integer<std::uint32_t>();
            recipe.limits.maximum_children_per_node = reader.integer<std::uint32_t>();
            recipe.limits.maximum_nodes = reader.integer<std::uint32_t>();
            recipe.limits.maximum_segments = reader.integer<std::uint32_t>();
            recipe.limits.maximum_terminals = reader.integer<std::uint32_t>();
            recipe.duration_seconds = reader.real();
            return recipe;
        }

        voxel::Float3 read_materialized_float3(Reader& reader) noexcept
        {
            return {reader.real(), reader.real(), reader.real()};
        }

        void write_materialized_lifetime(Writer& writer,
            authoring::morphology::TemporalRange range) noexcept
        {
            writer.real(range.birth_seconds);
            writer.real(range.end_seconds);
        }

        authoring::morphology::TemporalRange read_materialized_lifetime(Reader& reader) noexcept
        {
            return {reader.real(), reader.real()};
        }

        std::vector<std::byte> materialized_graph_bytes(
            const authoring::morphology::Graph& graph, std::uint64_t maximumBytes) noexcept
        {
            Writer writer{maximumBytes};
            write_materialized_recipe(writer, graph.recipe);
            writer.integer(graph.content_hash);
            writer.boolean(graph.truncated);
            writer.integer(static_cast<std::uint32_t>(graph.nodes.size()));
            for (const auto& node : graph.nodes)
            {
                write_materialized_id(writer, node.id);
                write_materialized_id(writer, node.parent);
                writer.integer(static_cast<std::uint8_t>(node.role));
                writer.integer(node.depth);
                write_float3(writer, node.position);
                write_float3(writer, node.tangent);
                writer.real(node.radius_meters);
                write_materialized_lifetime(writer, node.lifetime);
            }
            writer.integer(static_cast<std::uint32_t>(graph.segments.size()));
            for (const auto& segment : graph.segments)
            {
                write_materialized_id(writer, segment.id);
                write_materialized_id(writer, segment.parent_node);
                write_materialized_id(writer, segment.child_node);
                write_float3(writer, segment.start);
                write_float3(writer, segment.end);
                write_float3(writer, segment.tangent_start);
                write_float3(writer, segment.tangent_end);
                writer.real(segment.radius_start_meters);
                writer.real(segment.radius_end_meters);
                writer.integer(segment.depth);
                write_materialized_lifetime(writer, segment.lifetime);
            }
            writer.integer(static_cast<std::uint32_t>(graph.terminals.size()));
            for (const auto& terminal : graph.terminals)
            {
                write_materialized_id(writer, terminal.id);
                write_materialized_id(writer, terminal.node);
                writer.integer(static_cast<std::uint8_t>(terminal.kind));
                write_float3(writer, terminal.position);
                write_float3(writer, terminal.outward);
                write_float3(writer, terminal.up);
                writer.real(terminal.scale_meters);
                writer.integer(terminal.variant);
                write_materialized_lifetime(writer, terminal.lifetime);
            }
            return writer.ok() ? writer.release() : std::vector<std::byte>{};
        }

        template<typename Record, typename ReadRecord>
        bool read_materialized_records(Reader& reader, std::vector<Record>& output,
            std::size_t maximumCount, std::size_t minimumRecordBytes, ReadRecord read)
        {
            const auto count = reader.integer<std::uint32_t>();
            if (!reader.ok() || count > maximumCount
                || count > reader.remaining() / minimumRecordBytes) return false;
            output.resize(count);
            for (auto& value : output)
            {
                read(value);
                if (!reader.ok()) return false;
            }
            return true;
        }

        bool read_materialized_graph(std::span<const std::byte> bytes,
            authoring::morphology::Graph& graph)
        {
            namespace morphology = authoring::morphology;
            Reader reader{bytes};
            graph.recipe = read_materialized_recipe(reader);
            graph.content_hash = reader.integer<std::uint64_t>();
            graph.truncated = reader.boolean();
            if (!reader.ok() || !morphology::validate(graph.recipe)
                || graph.recipe.limits.maximum_nodes > 16384u
                || graph.recipe.limits.maximum_segments > 16383u
                || graph.recipe.limits.maximum_terminals > 16384u) return false;
            if (!read_materialized_records(reader, graph.nodes,
                graph.recipe.limits.maximum_nodes, 57u, [&](morphology::Node& node) {
                    node.id = read_materialized_id<morphology::NodeId>(reader);
                    node.parent = read_materialized_id<morphology::NodeId>(reader);
                    node.role = static_cast<morphology::NodeRole>(reader.integer<std::uint8_t>());
                    node.depth = reader.integer<std::uint32_t>();
                    node.position = read_materialized_float3(reader);
                    node.tangent = read_materialized_float3(reader);
                    node.radius_meters = reader.real();
                    node.lifetime = read_materialized_lifetime(reader);
                })) return false;
            if (!read_materialized_records(reader, graph.segments,
                graph.recipe.limits.maximum_segments, 92u, [&](morphology::Segment& segment) {
                    segment.id = read_materialized_id<morphology::SegmentId>(reader);
                    segment.parent_node = read_materialized_id<morphology::NodeId>(reader);
                    segment.child_node = read_materialized_id<morphology::NodeId>(reader);
                    segment.start = read_materialized_float3(reader);
                    segment.end = read_materialized_float3(reader);
                    segment.tangent_start = read_materialized_float3(reader);
                    segment.tangent_end = read_materialized_float3(reader);
                    segment.radius_start_meters = reader.real();
                    segment.radius_end_meters = reader.real();
                    segment.depth = reader.integer<std::uint32_t>();
                    segment.lifetime = read_materialized_lifetime(reader);
                })) return false;
            if (!read_materialized_records(reader, graph.terminals,
                graph.recipe.limits.maximum_terminals, 69u, [&](morphology::Terminal& terminal) {
                    terminal.id = read_materialized_id<morphology::TerminalId>(reader);
                    terminal.node = read_materialized_id<morphology::NodeId>(reader);
                    terminal.kind = static_cast<morphology::TerminalKind>(reader.integer<std::uint8_t>());
                    terminal.position = read_materialized_float3(reader);
                    terminal.outward = read_materialized_float3(reader);
                    terminal.up = read_materialized_float3(reader);
                    terminal.scale_meters = reader.real();
                    terminal.variant = reader.integer<std::uint32_t>();
                    terminal.lifetime = read_materialized_lifetime(reader);
                })) return false;
            return reader.finished() && static_cast<bool>(morphology::validate(graph));
        }

        std::vector<std::byte> materialized_sample_bytes(
            const authoring::morphology::Sample& sample, std::uint64_t maximumBytes) noexcept
        {
            Writer writer{maximumBytes};
            writer.real(sample.time_seconds);
            writer.integer(static_cast<std::uint32_t>(sample.segments.size()));
            for (const auto& segment : sample.segments)
            {
                write_materialized_id(writer, segment.source);
                write_float3(writer, segment.start);
                write_float3(writer, segment.end);
                writer.real(segment.radius_start_meters);
                writer.real(segment.radius_end_meters);
                writer.real(segment.progress);
                writer.integer(segment.depth);
            }
            writer.integer(static_cast<std::uint32_t>(sample.terminals.size()));
            for (const auto& terminal : sample.terminals)
            {
                write_materialized_id(writer, terminal.source);
                writer.integer(static_cast<std::uint8_t>(terminal.kind));
                write_float3(writer, terminal.position);
                writer.real(terminal.scale_meters);
                writer.real(terminal.progress);
            }
            return writer.ok() ? writer.release() : std::vector<std::byte>{};
        }

        bool read_materialized_sample(std::span<const std::byte> bytes,
            const authoring::morphology::Graph& graph, authoring::morphology::Sample& sample)
        {
            namespace morphology = authoring::morphology;
            Reader reader{bytes};
            sample.time_seconds = reader.real();
            if (!std::isfinite(sample.time_seconds) || sample.time_seconds < 0.0f
                || sample.time_seconds > graph.recipe.duration_seconds) return false;
            if (!read_materialized_records(reader, sample.segments,
                graph.segments.size(), 48u, [&](morphology::SampledSegment& segment) {
                    segment.source = read_materialized_id<morphology::SegmentId>(reader);
                    segment.start = read_materialized_float3(reader);
                    segment.end = read_materialized_float3(reader);
                    segment.radius_start_meters = reader.real();
                    segment.radius_end_meters = reader.real();
                    segment.progress = reader.real();
                    segment.depth = reader.integer<std::uint32_t>();
                })) return false;
            if (!read_materialized_records(reader, sample.terminals,
                graph.terminals.size(), 29u, [&](morphology::SampledTerminal& terminal) {
                    terminal.source = read_materialized_id<morphology::TerminalId>(reader);
                    terminal.kind = static_cast<morphology::TerminalKind>(reader.integer<std::uint8_t>());
                    terminal.position = read_materialized_float3(reader);
                    terminal.scale_meters = reader.real();
                    terminal.progress = reader.real();
                })) return false;
            if (!reader.finished()) return false;
            std::size_t sampled{};
            const auto same = [](voxel::Float3 a, voxel::Float3 b) noexcept {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            };
            for (const auto& segment : graph.segments)
            {
                const float progress = morphology::sample_progress(segment.lifetime, sample.time_seconds);
                if (!(progress > 0.0f)) continue;
                if (sampled >= sample.segments.size()) return false;
                const auto& actual = sample.segments[sampled++];
                if (actual.source != segment.id || actual.depth != segment.depth
                    || actual.progress != progress || !same(actual.start, segment.start)
                    || !same(actual.end, morphology::lerp(segment.start, segment.end, progress))
                    || actual.radius_start_meters != segment.radius_start_meters
                    || actual.radius_end_meters != segment.radius_end_meters * progress) return false;
            }
            if (sampled != sample.segments.size()) return false;
            sampled = 0u;
            for (const auto& terminal : graph.terminals)
            {
                const float progress = morphology::sample_progress(terminal.lifetime, sample.time_seconds);
                if (!(progress > 0.0f)) continue;
                if (sampled >= sample.terminals.size()) return false;
                const auto& actual = sample.terminals[sampled++];
                if (actual.source != terminal.id || actual.kind != terminal.kind
                    || actual.progress != progress || !same(actual.position, terminal.position)
                    || actual.scale_meters != terminal.scale_meters * progress) return false;
            }
            return sampled == sample.terminals.size();
        }

        voxel::ChunkDesc read_materialized_chunk(Reader& reader) noexcept
        {
            voxel::ChunkDesc result;
            result.cellsX = reader.integer<std::uint32_t>();
            result.cellsY = reader.integer<std::uint32_t>();
            result.cellsZ = reader.integer<std::uint32_t>();
            result.cellSizeMeters = reader.real();
            result.lodLevel = reader.integer<std::uint8_t>();
            return result;
        }

        bool valid_materialized_chunk(const voxel::ChunkDesc& value) noexcept
        {
            return value.cellsX >= 4u && value.cellsX <= 128u
                && value.cellsY >= 4u && value.cellsY <= 128u
                && value.cellsZ >= 4u && value.cellsZ <= 128u
                && std::isfinite(value.cellSizeMeters) && value.cellSizeMeters > 0.0f
                && value.lodLevel < 16u;
        }

        bool read_materialized_descriptor(std::span<const std::byte> bytes,
            forest::CompiledForestAsset& artifact)
        {
            Reader reader{bytes};
            artifact.genome.stableHash = reader.integer<std::uint64_t>();
            artifact.sourceRevision = reader.integer<std::uint64_t>();
            artifact.sourceContentHash = reader.integer<std::uint64_t>();
            if (reader.integer<std::uint64_t>() != artifact.graph.content_hash
                || reader.integer<std::uint32_t>() != artifact.graph.nodes.size()
                || reader.integer<std::uint32_t>() != artifact.graph.segments.size()
                || reader.integer<std::uint32_t>() != artifact.graph.terminals.size()
                || reader.boolean() != artifact.graph.truncated
                || reader.real() != artifact.sample.time_seconds
                || reader.integer<std::uint32_t>() != artifact.sample.segments.size()
                || reader.integer<std::uint32_t>() != artifact.sample.terminals.size()) return false;
            artifact.preview.segmentCount = reader.integer<std::uint32_t>();
            if (artifact.preview.segmentCount > artifact.preview.segments.size()) return false;
            for (std::size_t index = 0; index < artifact.preview.segmentCount; ++index)
            {
                auto& segment = artifact.preview.segments[index];
                segment.start = read_materialized_float3(reader);
                segment.end = read_materialized_float3(reader);
                segment.radius = reader.real();
                segment.depth = reader.integer<std::uint32_t>();
                if (!authoring::morphology::finite(segment.start)
                    || !authoring::morphology::finite(segment.end)
                    || !std::isfinite(segment.radius) || segment.radius <= 0.0f) return false;
            }
            artifact.preview.leafCount = reader.integer<std::uint32_t>();
            if (artifact.preview.leafCount > artifact.preview.leaves.size()) return false;
            for (std::size_t index = 0; index < artifact.preview.leafCount; ++index)
            {
                auto& leaf = artifact.preview.leaves[index];
                leaf.position = read_materialized_float3(reader);
                leaf.size = reader.real();
                leaf.sourceSegment = reader.integer<std::uint32_t>();
                if (!authoring::morphology::finite(leaf.position)
                    || !std::isfinite(leaf.size) || leaf.size <= 0.0f
                    || leaf.sourceSegment >= artifact.sample.segments.size()) return false;
            }
            auto& stats = artifact.preview.stats;
            stats.nodes = reader.integer<std::uint32_t>();
            stats.branches = reader.integer<std::uint32_t>();
            stats.leaves = reader.integer<std::uint32_t>();
            stats.vertices = reader.integer<std::uint32_t>();
            stats.triangles = reader.integer<std::uint32_t>();
            if (!artifact.preview.segmentCount || stats.nodes != artifact.preview.segmentCount + 1u
                || stats.branches != artifact.preview.segmentCount - 1u
                || stats.leaves != artifact.preview.leafCount
                || stats.vertices != artifact.preview.segmentCount * 12u + artifact.preview.leafCount * 6u
                || stats.triangles != artifact.preview.segmentCount * 8u + artifact.preview.leafCount * 2u) return false;
            const auto levels = reader.integer<std::uint32_t>();
            if (!levels || levels > 16u || levels > reader.remaining() / 61u) return false;
            artifact.voxelLods.levels.resize(levels);
            const auto validBounds = [](const auto& minimum, const auto& maximum) noexcept {
                for (std::size_t axis = 0u; axis < 3u; ++axis)
                    if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis])
                        || minimum[axis] > maximum[axis]) return false;
                return true;
            };
            for (auto& level : artifact.voxelLods.levels)
            {
                level.chunk = read_materialized_chunk(reader);
                for (auto& value : level.bounds_min_meters) value = reader.real();
                for (auto& value : level.bounds_max_meters) value = reader.real();
                level.minimum_view_distance_meters = reader.real();
                level.estimated_active_cells = reader.integer<std::uint64_t>();
                level.dense_equivalent_bytes = reader.integer<std::uint64_t>();
                if (!valid_materialized_chunk(level.chunk)
                    || !validBounds(level.bounds_min_meters, level.bounds_max_meters)
                    || !std::isfinite(level.minimum_view_distance_meters)
                    || level.minimum_view_distance_meters < 0.0f
                    || level.estimated_active_cells > voxel::dense_cell_count(level.chunk)
                    || level.dense_equivalent_bytes != voxel::dense_cell_bytes(level.chunk)) return false;
            }
            artifact.voxelLods.semantics = static_cast<voxel::CellSemantic>(reader.integer<std::uint16_t>());
            artifact.voxelLods.source_content_hash = reader.integer<std::uint64_t>();
            if (static_cast<std::uint16_t>(artifact.voxelLods.semantics)
                != static_cast<std::uint16_t>(authoring::morphology::VoxelLodPlan{}.semantics)
                || artifact.voxelLods.source_content_hash != artifact.graph.content_hash) return false;
            auto& occupancy = artifact.voxelOccupancy;
            occupancy.chunk = read_materialized_chunk(reader);
            occupancy.semantics = static_cast<voxel::CellSemantic>(reader.integer<std::uint16_t>());
            for (auto& value : occupancy.boundsMinMeters) value = reader.real();
            for (auto& value : occupancy.boundsMaxMeters) value = reader.real();
            occupancy.trunkCells = reader.integer<std::uint64_t>();
            occupancy.branchCells = reader.integer<std::uint64_t>();
            occupancy.foliageCells = reader.integer<std::uint64_t>();
            occupancy.activeCells = reader.integer<std::uint64_t>();
            occupancy.denseBytes = reader.integer<std::uint64_t>();
            artifact.valid = reader.boolean();
            if (!valid_materialized_chunk(occupancy.chunk)
                || !validBounds(occupancy.boundsMinMeters, occupancy.boundsMaxMeters)
                || static_cast<std::uint16_t>(occupancy.semantics)
                    != static_cast<std::uint16_t>(forest::ForestVoxelOccupancySummary{}.semantics)
                || occupancy.activeCells > voxel::dense_cell_count(occupancy.chunk)
                || occupancy.denseBytes != voxel::dense_cell_bytes(occupancy.chunk)) return false;
            return reader.finished() && artifact.valid && !artifact.sample.segments.empty();
        }

        std::vector<std::byte> serialize_materialized_artifact(
            const forest::ForestAssetDocument& document,
            const forest::CompiledForestAsset& compiled,
            std::span<const std::byte> sourceBytes, const ForestLibraryLimits& limits) noexcept
        {
            if (!compiled.valid || compiled.genome.stableHash != document.genome.stableHash
                || compiled.sourceRevision != document.revision
                || compiled.sourceContentHash != document.contentHash) return {};
            const auto graph = materialized_graph_bytes(compiled.graph, limits.maximum_artifact_bytes);
            const auto sample = materialized_sample_bytes(compiled.sample, limits.maximum_artifact_bytes);
            const auto descriptor = compiled_descriptor(compiled, limits.maximum_artifact_bytes);
            if (graph.empty() || sample.empty() || descriptor.empty()
                || limits.maximum_artifact_bytes < kDigestBytes) return {};
            Writer writer{limits.maximum_artifact_bytes - kDigestBytes};
            writer.raw(kArtifactMagic);
            writer.integer(kMaterializedArtifactVersion);
            writer.byte_vector(sourceBytes);
            writer.text(kMaterializedProducer);
            writer.byte_vector(graph);
            writer.byte_vector(sample);
            writer.byte_vector(descriptor);
            if (!writer.ok()) return {};
            auto bytes = writer.release();
            return append_digest(bytes, limits.maximum_artifact_bytes)
                ? std::move(bytes) : std::vector<std::byte>{};
        }

        // Structural decoding is not producer proof. Only exact-key host-owned
        // publication/read paths may call this; generic resolution refuses v2.
        ArtifactBytes deserialize_materialized_artifact(std::span<const std::byte> bytes,
            const ForestLibraryLimits& limits) noexcept
        {
            try
            {
                const auto body = integrity_verified_body(bytes);
                if (!body) return {ForestLibraryCode::integrity_failure};
                Reader reader{*body};
                std::array<std::uint8_t, kArtifactMagic.size()> magic{};
                if (!reader.raw(magic) || magic != kArtifactMagic
                    || reader.integer<std::uint16_t>() != kMaterializedArtifactVersion)
                    return {ForestLibraryCode::invalid_artifact};
                const auto sourceData = reader.byte_span(static_cast<std::size_t>(limits.maximum_source_bytes));
                const auto producer = reader.text(64u);
                if (!reader.ok() || producer != kMaterializedProducer) return {ForestLibraryCode::invalid_artifact};
                const auto source = deserialize_source(sourceData, limits, false);
                if (source.code != ForestLibraryCode::ready) return {ForestLibraryCode::invalid_artifact};
                const auto graph = reader.byte_span(static_cast<std::size_t>(limits.maximum_artifact_bytes));
                const auto sample = reader.byte_span(static_cast<std::size_t>(limits.maximum_artifact_bytes));
                const auto descriptor = reader.byte_span(static_cast<std::size_t>(limits.maximum_artifact_bytes));
                if (!reader.finished()) return {ForestLibraryCode::invalid_artifact};
                forest::CompiledForestAsset artifact;
                if (!read_materialized_graph(graph, artifact.graph)
                    || !read_materialized_sample(sample, artifact.graph, artifact.sample)
                    || !read_materialized_descriptor(descriptor, artifact))
                    return {ForestLibraryCode::invalid_artifact};
                Writer expectedRecipe{1024u}, actualRecipe{1024u};
                write_materialized_recipe(expectedRecipe, forest::morphology_recipe(source.document.profile));
                write_materialized_recipe(actualRecipe, artifact.graph.recipe);
                const float duration = (std::max)(0.001f, source.document.profile.temporal.durationSeconds);
                const float requestedTime = source.document.profile.temporal.reverse
                    ? duration - (std::clamp)(source.document.profile.temporal.timeSeconds, 0.0f, duration)
                    : source.document.profile.temporal.timeSeconds;
                if (!expectedRecipe.ok() || !actualRecipe.ok()
                    || expectedRecipe.bytes() != actualRecipe.bytes()
                    || artifact.sample.time_seconds != (std::clamp)(requestedTime, 0.0f, duration)
                    || artifact.genome.stableHash != source.document.genome.stableHash
                    || artifact.sourceRevision != source.document.revision
                    || artifact.sourceContentHash != source.document.contentHash)
                    return {ForestLibraryCode::integrity_failure};
                const auto canonical = compiled_descriptor(artifact, limits.maximum_artifact_bytes);
                if (canonical.size() != descriptor.size()
                    || !std::equal(canonical.begin(), canonical.end(), descriptor.begin()))
                    return {ForestLibraryCode::invalid_artifact};
                return {ForestLibraryCode::ready, std::move(artifact),
                    asset_hash(digest_bytes(*body)), std::vector<std::byte>{bytes.begin(), bytes.end()}};
            }
            catch (...) { return {ForestLibraryCode::allocation_failure}; }
        }

        [[nodiscard]] std::vector<std::byte> serialize_artifact(
            const forest::ForestAssetDocument& document,
            std::span<const std::byte> sourceBytes,
            const ForestLibraryLimits& limits) noexcept
        {
            const forest::CompiledForestAsset artifact =
                forest::compile_forest_asset(document);
            const std::vector<std::byte> descriptor = compiled_descriptor(
                artifact, limits.maximum_artifact_bytes);
            if (descriptor.empty())
                return {};
            Writer writer{limits.maximum_artifact_bytes - kDigestBytes};
            writer.raw(kArtifactMagic);
            const bool canopy = forest::forest_canopy_settings(document.profile).enabled;
            writer.integer(canopy ? kCanopyArtifactVersion : kSchemaVersion);
            if (canopy) writer.integer(kCanopyCompilerRevision);
            writer.byte_vector(sourceBytes);
            writer.raw(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(descriptor.data()),
                descriptor.size()});
            if (!writer.ok())
                return {};
            std::vector<std::byte> bytes = writer.release();
            return append_digest(bytes, limits.maximum_artifact_bytes)
                ? std::move(bytes)
                : std::vector<std::byte>{};
        }

        [[nodiscard]] ArtifactBytes deserialize_artifact(
            std::span<const std::byte> bytes,
            const ForestLibraryLimits& limits,
            bool allowMaterialized = false) noexcept
        {
            if (bytes.size() > limits.maximum_artifact_bytes)
                return {ForestLibraryCode::artifact_too_large};
            const auto body = integrity_verified_body(bytes);
            if (!body)
                return {ForestLibraryCode::integrity_failure};
            Reader reader{*body};
            std::array<std::uint8_t, kArtifactMagic.size()> magic{};
            if (!reader.raw(magic) || magic != kArtifactMagic)
            {
                return {ForestLibraryCode::invalid_artifact};
            }
            const auto version = reader.integer<std::uint16_t>();
            if (version == kMaterializedArtifactVersion)
                return allowMaterialized ? deserialize_materialized_artifact(bytes, limits)
                    : ArtifactBytes{ForestLibraryCode::verified_key_required};
            if (version != kSchemaVersion && version != kCanopyArtifactVersion)
                return {ForestLibraryCode::invalid_artifact};
            const auto compilerRevision = version == kCanopyArtifactVersion
                ? reader.integer<std::uint32_t>() : 0u;
            std::vector<std::byte> sourceBytes = reader.byte_vector(
                static_cast<std::size_t>(limits.maximum_source_bytes));
            if (!reader.ok())
                return {ForestLibraryCode::invalid_artifact};
            SourceBytes source = deserialize_source(sourceBytes, limits);
            if (source.code != ForestLibraryCode::ready)
                return {ForestLibraryCode::invalid_artifact};
            // The sealed authoring source is reusable, but an old compiler's
            // descriptor is never admitted as current compiled geometry.
            const auto stale = [&]() {
                forest::CompiledForestAsset metadata{};
                metadata.sourceRevision = source.document.revision;
                metadata.sourceContentHash = source.document.contentHash;
                return ArtifactBytes{ForestLibraryCode::rebuild_required,
                    std::move(metadata), asset_hash(digest_bytes(*body)), {}};
            };
            if (version == kCanopyArtifactVersion && compilerRevision != kCanopyCompilerRevision)
                return stale();
            const forest::CompiledForestAsset artifact =
                forest::compile_forest_asset(source.document);
            const std::vector<std::byte> expected = compiled_descriptor(
                artifact, limits.maximum_artifact_bytes);
            if (expected.empty()) return {ForestLibraryCode::invalid_artifact};
            if (reader.remaining() != expected.size())
                return version == kSchemaVersion && forest::forest_canopy_settings(source.document.profile).enabled
                    ? stale() : ArtifactBytes{ForestLibraryCode::invalid_artifact};
            std::vector<std::uint8_t> actual{};
            try
            {
                actual.resize(expected.size());
            }
            catch (...)
            {
                return {ForestLibraryCode::allocation_failure};
            }
            if (!reader.raw(actual) || !reader.finished())
                return {ForestLibraryCode::invalid_artifact};
            for (std::size_t index = 0u; index < expected.size(); ++index)
            {
                if (actual[index]
                    != std::to_integer<std::uint8_t>(expected[index]))
                {
                    return version == kSchemaVersion && forest::forest_canopy_settings(source.document.profile).enabled
                        ? stale() : ArtifactBytes{ForestLibraryCode::integrity_failure};
                }
            }
            std::vector<std::byte> storedBytes{};
            try
            {
                storedBytes.assign(bytes.begin(), bytes.end());
            }
            catch (...)
            {
                return {ForestLibraryCode::allocation_failure};
            }
            return {
                ForestLibraryCode::ready,
                artifact,
                asset_hash(digest_bytes(*body)),
                std::move(storedBytes)};
        }

        [[nodiscard]] std::optional<CanonicalAsset> canonical_asset(
            const fs::path& projectRoot,
            std::uint64_t projectKey,
            std::string_view logicalPath,
            const ForestLibraryLimits& limits,
            ForestLibraryCode& failure)
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits.registry);
            if (!canonical)
            {
                failure = ForestLibraryCode::invalid_path;
                return std::nullopt;
            }
            if (canonical->size() <= kSourcePrefix.size()
                || !canonical->starts_with(kSourcePrefix))
            {
                failure = ForestLibraryCode::outside_forest_assets;
                return std::nullopt;
            }
            const fs::path relative{*canonical};
            if (relative.extension().generic_string() != kSourceExtension)
            {
                failure = ForestLibraryCode::unsupported_extension;
                return std::nullopt;
            }
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                projectKey, project_assets::AssetKind::forest_asset, *canonical);
            if (assetKey == 0u)
            {
                failure = ForestLibraryCode::invalid_path;
                return std::nullopt;
            }
            failure = ForestLibraryCode::ready;
            return CanonicalAsset{
                *canonical,
                (projectRoot / relative).lexically_normal(),
                (projectRoot / "Library" / "Forest" / hex_u64(assetKey))
                    .lexically_normal(),
                assetKey};
        }

        [[nodiscard]] fs::path artifact_path(
            const CanonicalAsset& asset,
            const project_assets::AssetContentHash& key)
        {
            return asset.artifact_directory
                / (hex_hash(key) + std::string{kArtifactExtension});
        }

        [[nodiscard]] ForestLibraryCode read_file(
            const fs::path& path,
            std::uint64_t maximumBytes,
            ForestLibraryCode tooLargeCode,
            std::vector<std::byte>& bytes) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::exists(path, error) || error)
                    return error ? ForestLibraryCode::read_failure
                                 : ForestLibraryCode::not_found;
                if (!fs::is_regular_file(path, error) || error)
                    return ForestLibraryCode::read_failure;
                const std::uintmax_t size = fs::file_size(path, error);
                if (error || size == 0u || size > maximumBytes
                    || size > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::streamsize>::max)()))
                {
                    return size > maximumBytes ? tooLargeCode
                                               : ForestLibraryCode::read_failure;
                }
                bytes.resize(static_cast<std::size_t>(size));
                std::ifstream input{path, std::ios::binary};
                if (!input)
                    return ForestLibraryCode::read_failure;
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                return input
                        && input.gcount()
                            == static_cast<std::streamsize>(bytes.size())
                    ? ForestLibraryCode::ready
                    : ForestLibraryCode::read_failure;
            }
            catch (...)
            {
                return ForestLibraryCode::allocation_failure;
            }
        }

        [[nodiscard]] ForestLibraryCode write_file(
            const fs::path& path,
            std::span<const std::byte> bytes) noexcept
        {
            try
            {
                std::ofstream output{
                    path, std::ios::binary | std::ios::trunc};
                if (!output)
                    return ForestLibraryCode::write_failure;
                output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                    return ForestLibraryCode::write_failure;
                output.close();
                return output ? ForestLibraryCode::ready
                              : ForestLibraryCode::write_failure;
            }
            catch (...)
            {
                return ForestLibraryCode::write_failure;
            }
        }

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] ForestSourceLocator source_locator(
            ForestLibraryCode code,
            const CanonicalAsset& asset,
            std::uint64_t projectKey,
            const SourceBytes& source)
        {
            return {
                code,
                asset.logical_path,
                asset.source_path,
                projectKey,
                asset.asset_key,
                {source.digest, source.document.revision},
                source.document.contentHash,
                source.bytes.size()};
        }

        [[nodiscard]] ForestArtifactLocator artifact_locator(
            ForestLibraryCode code,
            const CanonicalAsset& asset,
            const fs::path& path,
            std::uint64_t projectKey,
            const ArtifactBytes& artifact)
        {
            return {
                code,
                asset.logical_path,
                path,
                projectKey,
                asset.asset_key,
                artifact.digest,
                artifact.artifact.sourceRevision,
                artifact.artifact.sourceContentHash,
                artifact.bytes.size()};
        }

        [[nodiscard]] bool artifact_file(const fs::path& path) noexcept
        {
            const std::string name = path.filename().generic_string();
            return name.size() == 64u + kArtifactExtension.size()
                && name.ends_with(kArtifactExtension);
        }

        constexpr std::string_view kSelectedReferenceName{"selected.epoch_forest_reference"};
        constexpr std::array<std::uint8_t, 8u> kSelectedReferenceMagic{
            'E', 'P', 'F', 'S', 'E', 'L', '1', 0u};
        constexpr std::uint64_t kMaximumSelectedReferenceBytes = 8192u;
        constexpr std::uint64_t kMaximumProjectManifestBytes = 65536u;

        // Only this new explicit path uses these stronger filesystem checks;
        // historical v1 publication and its immutable bytes remain unchanged.
        bool ordinary_selected_path(const fs::path& path, bool directory) noexcept
        {
            try
            {
                if (!path.is_absolute() || path != path.lexically_normal()) return false;
                auto current = path.root_path();
                for (const auto& component : path.relative_path())
                {
                    current /= component;
                    const auto status = fs::symlink_status(current);
                    if (fs::is_symlink(status)) return false;
                    std::error_code redirected;
                    (void)fs::read_symlink(current, redirected);
                    if (!redirected) return false; // Includes Windows junctions.
                    if (current == path && !directory)
                    {
                        if (!fs::is_regular_file(status) || fs::hard_link_count(current) != 1u) return false;
                    }
                    else if (!fs::is_directory(status)) return false;
                }
                return true;
            }
            catch (...) { return false; }
        }

        struct SelectedMaterializedState final
        {
            ForestLibraryCode code{ForestLibraryCode::invalid_artifact};
            fs::path root{}, reference_path{};
            SourceBytes source{};
            std::vector<std::byte> manifest{}, reference{};
            ArtifactBytes artifact{};
            ForestArtifactLocator locator{};
        };

        std::vector<std::byte> selected_reference_bytes(
            const SelectedMaterializedState& state, std::uint64_t maximumRootBytes,
            const ForestLibraryLimits& limits)
        {
            Writer writer{kMaximumSelectedReferenceBytes - kDigestBytes};
            const auto root = state.root.generic_string();
            if (root.size() > maximumRootBytes) return {};
            writer.raw(kSelectedReferenceMagic);
            writer.integer<std::uint16_t>(1u);
            writer.text(root);
            writer.integer(state.locator.project_key);
            writer.integer(state.locator.asset_key);
            writer.text(state.locator.canonical_logical_path);
            for (const auto word : asset_hash(digest_bytes(state.manifest)).words) writer.integer(word);
            for (const auto word : state.source.digest.words) writer.integer(word);
            writer.integer(state.locator.source_revision);
            writer.integer(state.locator.source_content_hash);
            for (const auto word : state.locator.artifact_key.words) writer.integer(word);
            writer.integer(state.locator.serialized_bytes);
            if (!writer.ok() || state.locator.serialized_bytes > limits.maximum_artifact_bytes) return {};
            auto bytes = writer.release();
            return append_digest(bytes, kMaximumSelectedReferenceBytes) ? std::move(bytes) : std::vector<std::byte>{};
        }

        bool valid_selected_reference(std::span<const std::byte> bytes,
            const fs::path& root, std::uint64_t projectKey, std::uint64_t assetKey, std::string_view logicalPath,
            const ForestLibraryLimits& limits)
        {
            const auto body = integrity_verified_body(bytes);
            if (!body) return false;
            Reader reader{*body};
            std::array<std::uint8_t, 8u> magic{};
            if (!reader.raw(magic) || magic != kSelectedReferenceMagic
                || reader.integer<std::uint16_t>() != 1u
                || reader.text(limits.maximum_project_root_bytes) != root.generic_string()
                || reader.integer<std::uint64_t>() != projectKey
                || reader.integer<std::uint64_t>() != assetKey
                || reader.text(limits.registry.maximum_path_bytes) != logicalPath) return false;
            const auto nonzero_hash = [&reader] {
                std::uint64_t nonzero{};
                for (unsigned word = 0; word < 4u; ++word) nonzero |= reader.integer<std::uint64_t>();
                return nonzero != 0u;
            };
            if (!nonzero_hash() || !nonzero_hash() // Manifest and complete source envelopes.
                || reader.integer<std::uint64_t>() == 0u
                || reader.integer<std::uint64_t>() == 0u || !nonzero_hash()) return false;
            const auto size = reader.integer<std::uint64_t>();
            return size > kDigestBytes && size <= limits.maximum_artifact_bytes && reader.finished();
        }

        // Derive the complete expected record from source, never from a saved
        // key/producer label. This is source replay, not approval restoration.
        SelectedMaterializedState verify_selected_materialized(
            const fs::path& projectRoot, std::uint64_t projectKey,
            std::string_view logicalPath, const ForestLibraryLimits& limits,
            std::uint64_t& bytesRead)
        {
            SelectedMaterializedState state;
            if (!ordinary_selected_path(projectRoot, true))
            { state.code = ForestLibraryCode::invalid_path; return state; }
            state.root = fs::canonical(projectRoot);
            ForestLibraryCode failure{};
            const auto asset = canonical_asset(state.root, projectKey, logicalPath, limits, failure);
            if (!asset) { state.code = failure; return state; }
            const auto manifestPath = state.root / "project.epoch.json";
            if (!ordinary_selected_path(manifestPath, false)
                || !ordinary_selected_path(asset->source_path, false)
                || !ordinary_selected_path(asset->artifact_directory, true))
            { state.code = ForestLibraryCode::invalid_path; return state; }
            state.code = read_file(manifestPath, kMaximumProjectManifestBytes,
                ForestLibraryCode::source_too_large, state.manifest);
            bytesRead += state.manifest.size();
            if (state.code != ForestLibraryCode::ready) return state;
            std::vector<std::byte> sourceBytes;
            state.code = read_file(asset->source_path, limits.maximum_source_bytes,
                ForestLibraryCode::source_too_large, sourceBytes);
            bytesRead += sourceBytes.size();
            if (state.code != ForestLibraryCode::ready) return state;
            state.source = deserialize_source(sourceBytes, limits, false);
            if (state.source.code != ForestLibraryCode::ready)
            { state.code = state.source.code; return state; }
            auto compiled = forest::compile_forest_asset(state.source.document);
            auto expected = serialize_materialized_artifact(
                state.source.document, compiled, state.source.bytes, limits);
            const auto body = integrity_verified_body(expected);
            if (!body) { state.code = ForestLibraryCode::invalid_artifact; return state; }
            const auto key = asset_hash(digest_bytes(*body));
            const auto path = artifact_path(*asset, key);
            if (!ordinary_selected_path(path, false))
            { state.code = ForestLibraryCode::invalid_path; return state; }
            std::vector<std::byte> actual;
            state.code = read_file(path, limits.maximum_artifact_bytes,
                ForestLibraryCode::artifact_too_large, actual);
            bytesRead += actual.size();
            if (state.code != ForestLibraryCode::ready) return state;
            if (actual != expected)
            { state.code = ForestLibraryCode::integrity_failure; return state; }
            state.artifact = {ForestLibraryCode::ready, std::move(compiled), key, std::move(expected)};
            state.locator = artifact_locator(ForestLibraryCode::ready, *asset, path, projectKey, state.artifact);
            state.reference_path = asset->artifact_directory / kSelectedReferenceName;
            state.reference = selected_reference_bytes(state, limits.maximum_project_root_bytes, limits);
            if (state.reference.empty()) state.code = ForestLibraryCode::artifact_too_large;
            return state;
        }

        bool selected_source_is_current(const SelectedMaterializedState& state,
            const ForestLibraryLimits& limits, std::uint64_t& bytesRead) noexcept
        {
            try
            {
            if (!ordinary_selected_path(state.root, true)
                || !ordinary_selected_path(state.root / "project.epoch.json", false)
                || !ordinary_selected_path(state.root / state.locator.canonical_logical_path, false)
                || !ordinary_selected_path(state.locator.storage_path, false)) return false;
            std::vector<std::byte> source, manifest, artifact;
            if (read_file(state.root / "project.epoch.json", kMaximumProjectManifestBytes,
                    ForestLibraryCode::source_too_large, manifest) != ForestLibraryCode::ready
                || read_file(state.root / state.locator.canonical_logical_path, limits.maximum_source_bytes,
                    ForestLibraryCode::source_too_large, source) != ForestLibraryCode::ready
                || read_file(state.locator.storage_path, limits.maximum_artifact_bytes,
                    ForestLibraryCode::artifact_too_large, artifact) != ForestLibraryCode::ready) return false;
            bytesRead += source.size() + manifest.size() + artifact.size();
            return source == state.source.bytes && manifest == state.manifest && artifact == state.artifact.bytes;
            }
            catch (...) { return false; }
        }

        struct ContractRoot final
        {
            fs::path path{};
            ContractRoot()
            {
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_project_forest_library_contract_"
                    + std::to_string(next_temporary_id());
                fs::remove_all(path, error);
                error.clear();
                fs::create_directories(path, error);
                if (error)
                    path.clear();
            }
            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }
            [[nodiscard]] bool valid() const noexcept { return !path.empty(); }
        };

        [[nodiscard]] bool corrupt_last_byte(const fs::path& path) noexcept
        {
            try
            {
                std::fstream stream{
                    path, std::ios::binary | std::ios::in | std::ios::out};
                if (!stream)
                    return false;
                stream.seekg(-1, std::ios::end);
                char value{};
                stream.read(&value, 1);
                if (!stream)
                    return false;
                value ^= static_cast<char>(0x5a);
                stream.seekp(-1, std::ios::end);
                stream.write(&value, 1);
                stream.flush();
                return static_cast<bool>(stream);
            }
            catch (...)
            {
                return false;
            }
        }
    }

    ForestAssetLibrary::ForestAssetLibrary(
        std::string projectId,
        std::filesystem::path projectRoot,
        ForestLibraryLimits limits) noexcept
        : project_id_(std::move(projectId)),
          project_root_(std::move(projectRoot)),
          limits_(limits)
    {
        if (!limits_.valid() || project_root_.empty()
            || project_root_.generic_string().size()
                > limits_.maximum_project_root_bytes)
        {
            return;
        }
        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0u)
            return;
        try
        {
            std::error_code error{};
            if (project_root_.is_relative())
                project_root_ = fs::absolute(project_root_, error);
            if (error)
            {
                project_root_.clear();
                project_key_ = 0u;
                return;
            }
            project_root_ = project_root_.lexically_normal();
        }
        catch (...)
        {
            project_root_.clear();
            project_key_ = 0u;
        }
    }

    bool ForestAssetLibrary::valid() const noexcept
    {
        return limits_.valid() && project_key_ != 0u && !project_root_.empty();
    }

    std::string_view ForestAssetLibrary::project_id() const noexcept
    {
        return project_id_;
    }

    const std::filesystem::path& ForestAssetLibrary::project_root() const noexcept
    {
        return project_root_;
    }

    std::uint64_t ForestAssetLibrary::project_key() const noexcept
    {
        return project_key_;
    }

    const ForestLibraryLimits& ForestAssetLibrary::limits() const noexcept
    {
        return limits_;
    }

    ForestLibraryMetrics ForestAssetLibrary::metrics() const noexcept
    {
        return metrics_;
    }

    PublishedForestAsset ForestAssetLibrary::publish(
        std::string_view logicalPath,
        const forest::ForestAssetDocument& document) noexcept
    {
        return publish_impl(logicalPath, document, false);
    }

    PublishedForestAsset ForestAssetLibrary::publish_materialized(
        std::string_view logicalPath,
        const forest::ForestAssetDocument& document) noexcept
    {
        return publish_impl(logicalPath, document, true);
    }

    PublishedForestAsset ForestAssetLibrary::publish_impl(
        std::string_view logicalPath,
        const forest::ForestAssetDocument& document,
        bool materialized) noexcept
    {
        ++metrics_.publish_requests;
        if (!valid())
            return reject_publish(ForestLibraryCode::invalid_library);
        try
        {
            ForestLibraryCode pathFailure{};
            const auto asset = canonical_asset(
                project_root_, project_key_, logicalPath, limits_, pathFailure);
            if (!asset)
                return reject_publish(pathFailure);

            SourceBytes source{};
            source.bytes = serialize_source(document, limits_, !materialized);
            if (source.bytes.empty())
                return reject_publish(ForestLibraryCode::invalid_source);
            source = deserialize_source(source.bytes, limits_, !materialized);
            if (source.code != ForestLibraryCode::ready)
                return reject_publish(source.code);

            ArtifactBytes artifact{};
            if (materialized)
            {
                // Compile at the host-owned publication boundary. The exact
                // resulting key, not the producer label, authorizes v2 reads.
                const auto compiled = forest::compile_forest_asset(source.document);
                artifact.bytes = serialize_materialized_artifact(
                    source.document, compiled, source.bytes, limits_);
            }
            else artifact.bytes = serialize_artifact(source.document, source.bytes, limits_);
            if (artifact.bytes.empty())
                return reject_publish(ForestLibraryCode::invalid_artifact);
            artifact = deserialize_artifact(artifact.bytes, limits_, materialized);
            if (artifact.code != ForestLibraryCode::ready)
                return reject_publish(artifact.code);

            std::error_code error{};
            fs::create_directories(asset->artifact_directory, error);
            if (error || !fs::is_directory(asset->artifact_directory, error)
                || error)
            {
                return reject_publish(ForestLibraryCode::directory_failure);
            }
            const fs::path artifactDestination = artifact_path(*asset, artifact.digest);
            bool artifactUnchanged{};
            if (fs::exists(artifactDestination, error) && !error)
            {
                std::vector<std::byte> existing{};
                const ForestLibraryCode read = read_file(
                    artifactDestination, limits_.maximum_artifact_bytes,
                    ForestLibraryCode::artifact_too_large, existing);
                metrics_.bytes_read += existing.size();
                const ArtifactBytes decoded = read == ForestLibraryCode::ready
                    ? deserialize_artifact(existing, limits_, materialized)
                    : ArtifactBytes{read};
                if (decoded.code != ForestLibraryCode::ready
                    || decoded.bytes != artifact.bytes
                    || decoded.digest.words != artifact.digest.words)
                {
                    return reject_publish(ForestLibraryCode::identity_collision);
                }
                artifactUnchanged = true;
            }
            else
            {
                if (error)
                    return reject_publish(ForestLibraryCode::read_failure);
                std::uint32_t artifactCount{};
                for (fs::directory_iterator iterator{
                        asset->artifact_directory, error}, end;
                    !error && iterator != end; iterator.increment(error))
                {
                    if (iterator->is_regular_file(error) && !error
                        && artifact_file(iterator->path()))
                    {
                        ++artifactCount;
                    }
                }
                if (error)
                    return reject_publish(ForestLibraryCode::directory_failure);
                if (artifactCount >= limits_.maximum_artifacts_per_asset)
                {
                    return reject_publish(
                        ForestLibraryCode::artifact_limit_exceeded);
                }
                fs::path temporary = artifactDestination;
                temporary += ".pending." + hex_u64(next_temporary_id());
                const ForestLibraryCode written = write_file(
                    temporary, artifact.bytes);
                if (written != ForestLibraryCode::ready)
                {
                    fs::remove(temporary, error);
                    return reject_publish(written);
                }
                std::vector<std::byte> verifiedBytes{};
                const ForestLibraryCode verifiedRead = read_file(
                    temporary, limits_.maximum_artifact_bytes,
                    ForestLibraryCode::artifact_too_large, verifiedBytes);
                const ArtifactBytes verified =
                    verifiedRead == ForestLibraryCode::ready
                    ? deserialize_artifact(verifiedBytes, limits_, materialized)
                    : ArtifactBytes{verifiedRead};
                metrics_.bytes_read += verifiedBytes.size();
                if (verified.code != ForestLibraryCode::ready
                    || verified.bytes != artifact.bytes
                    || verified.digest.words != artifact.digest.words)
                {
                    fs::remove(temporary, error);
                    return reject_publish(ForestLibraryCode::integrity_failure);
                }
                fs::rename(temporary, artifactDestination, error);
                if (error)
                {
                    std::error_code cleanup{};
                    fs::remove(temporary, cleanup);
                    return reject_publish(ForestLibraryCode::write_failure);
                }
                metrics_.bytes_written += artifact.bytes.size();
            }

            fs::create_directories(asset->source_path.parent_path(), error);
            if (error || !fs::is_directory(asset->source_path.parent_path(), error)
                || error)
            {
                return reject_publish(ForestLibraryCode::directory_failure);
            }
            bool sourceUnchanged{};
            std::vector<std::byte> existingSource{};
            const ForestLibraryCode existingRead = read_file(
                asset->source_path, limits_.maximum_source_bytes,
                ForestLibraryCode::source_too_large, existingSource);
            if (existingRead == ForestLibraryCode::ready)
            {
                metrics_.bytes_read += existingSource.size();
                if (existingSource == source.bytes)
                    sourceUnchanged = true;
            }
            else if (existingRead != ForestLibraryCode::not_found)
            {
                return reject_publish(existingRead);
            }
            if (!sourceUnchanged)
            {
                fs::path temporary = asset->source_path;
                temporary += ".pending." + hex_u64(next_temporary_id());
                const ForestLibraryCode written = write_file(temporary, source.bytes);
                if (written != ForestLibraryCode::ready)
                {
                    fs::remove(temporary, error);
                    return reject_publish(written);
                }
                std::vector<std::byte> verifiedBytes{};
                const ForestLibraryCode verifiedRead = read_file(
                    temporary, limits_.maximum_source_bytes,
                    ForestLibraryCode::source_too_large, verifiedBytes);
                const SourceBytes verified = verifiedRead == ForestLibraryCode::ready
                    ? deserialize_source(verifiedBytes, limits_, !materialized)
                    : SourceBytes{verifiedRead};
                metrics_.bytes_read += verifiedBytes.size();
                if (verified.code != ForestLibraryCode::ready
                    || verified.bytes != source.bytes
                    || verified.digest.words != source.digest.words)
                {
                    fs::remove(temporary, error);
                    return reject_publish(ForestLibraryCode::integrity_failure);
                }
                if (!platform::filesystem::atomic_replace_same_filesystem(
                        temporary, asset->source_path, error))
                {
                    std::error_code cleanup{};
                    fs::remove(temporary, cleanup);
                    return reject_publish(
                        ForestLibraryCode::atomic_replace_failure);
                }
                metrics_.bytes_written += source.bytes.size();
            }

            const bool unchanged = artifactUnchanged && sourceUnchanged;
            if (unchanged)
                ++metrics_.unchanged_publications;
            else
                ++metrics_.publications;
            const ForestLibraryCode resultCode = unchanged
                ? ForestLibraryCode::unchanged
                : ForestLibraryCode::ready;
            return {
                resultCode,
                source_locator(resultCode, *asset, project_key_, source),
                artifact_locator(
                    artifactUnchanged ? ForestLibraryCode::unchanged
                                      : ForestLibraryCode::ready,
                    *asset,
                    artifactDestination,
                    project_key_,
                    artifact)};
        }
        catch (...)
        {
            return reject_publish(ForestLibraryCode::allocation_failure);
        }
    }

    LoadedForestSource ForestAssetLibrary::load_source(
        std::string_view logicalPath) noexcept
    {
        ++metrics_.source_load_requests;
        if (!valid())
            return reject_source(ForestLibraryCode::invalid_library);
        try
        {
            ForestLibraryCode pathFailure{};
            const auto asset = canonical_asset(
                project_root_, project_key_, logicalPath, limits_, pathFailure);
            if (!asset)
                return reject_source(pathFailure);
            std::vector<std::byte> bytes{};
            const ForestLibraryCode read = read_file(
                asset->source_path, limits_.maximum_source_bytes,
                ForestLibraryCode::source_too_large, bytes);
            metrics_.bytes_read += bytes.size();
            if (read != ForestLibraryCode::ready)
                return reject_source(read);
            SourceBytes source = deserialize_source(bytes, limits_);
            if (source.code != ForestLibraryCode::ready)
                return reject_source(source.code);
            ++metrics_.source_loads;
            return {
                ForestLibraryCode::ready,
                source_locator(
                    ForestLibraryCode::ready, *asset, project_key_, source),
                std::move(source.document)};
        }
        catch (...)
        {
            return reject_source(ForestLibraryCode::allocation_failure);
        }
    }

    LoadedForestArtifact ForestAssetLibrary::load_exact(
        std::string_view logicalPath,
        const project_assets::AssetContentHash& artifactKey) noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid())
            return reject_artifact(ForestLibraryCode::invalid_library);
        if (artifactKey.empty())
            return reject_artifact(ForestLibraryCode::invalid_artifact);
        try
        {
            ForestLibraryCode pathFailure{};
            const auto asset = canonical_asset(
                project_root_, project_key_, logicalPath, limits_, pathFailure);
            if (!asset)
                return reject_artifact(pathFailure);
            const fs::path path = artifact_path(*asset, artifactKey);
            std::vector<std::byte> bytes{};
            const ForestLibraryCode read = read_file(
                path, limits_.maximum_artifact_bytes,
                ForestLibraryCode::artifact_too_large, bytes);
            metrics_.bytes_read += bytes.size();
            if (read != ForestLibraryCode::ready)
                return reject_artifact(read);
            ArtifactBytes artifact = deserialize_artifact(bytes, limits_);
            if (artifact.code == ForestLibraryCode::verified_key_required)
                return reject_artifact(artifact.code);
            if (artifact.code == ForestLibraryCode::rebuild_required
                && artifact.digest.words == artifactKey.words)
                return reject_artifact(artifact.code);
            if (artifact.code != ForestLibraryCode::ready
                || artifact.digest.words != artifactKey.words)
            {
                return reject_artifact(ForestLibraryCode::integrity_failure);
            }
            ++metrics_.artifact_loads;
            return {
                ForestLibraryCode::ready,
                artifact_locator(
                    ForestLibraryCode::ready, *asset, path, project_key_, artifact),
                std::move(artifact.artifact)};
        }
        catch (...)
        {
            return reject_artifact(ForestLibraryCode::allocation_failure);
        }
    }

    LoadedForestArtifact ForestAssetLibrary::load_materialized_exact(
        std::string_view logicalPath,
        const ForestArtifactLocator& approvedPublication) noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid()) return reject_artifact(ForestLibraryCode::invalid_library);
        if (!approvedPublication || approvedPublication.project_key != project_key_)
            return reject_artifact(ForestLibraryCode::verified_key_required);
        try
        {
            ForestLibraryCode failure{};
            const auto asset = canonical_asset(project_root_, project_key_, logicalPath, limits_, failure);
            if (!asset) return reject_artifact(failure);
            const auto path = artifact_path(*asset, approvedPublication.artifact_key);
            if (asset->asset_key != approvedPublication.asset_key
                || asset->logical_path != approvedPublication.canonical_logical_path
                || path.lexically_normal() != approvedPublication.storage_path.lexically_normal())
                return reject_artifact(ForestLibraryCode::verified_key_required);
            std::vector<std::byte> bytes;
            const auto read = read_file(path, limits_.maximum_artifact_bytes,
                ForestLibraryCode::artifact_too_large, bytes);
            metrics_.bytes_read += bytes.size();
            if (read != ForestLibraryCode::ready) return reject_artifact(read);
            const auto body = integrity_verified_body(bytes);
            if (!body || asset_hash(digest_bytes(*body)).words != approvedPublication.artifact_key.words
                || bytes.size() != approvedPublication.serialized_bytes)
                return reject_artifact(ForestLibraryCode::integrity_failure);
            // Only the exact host-issued key reaches structural materialization.
            auto decoded = deserialize_materialized_artifact(bytes, limits_);
            if (decoded.code != ForestLibraryCode::ready) return reject_artifact(decoded.code);
            if (decoded.artifact.sourceRevision != approvedPublication.source_revision
                || decoded.artifact.sourceContentHash != approvedPublication.source_content_hash)
                return reject_artifact(ForestLibraryCode::integrity_failure);
            ++metrics_.artifact_loads;
            return {ForestLibraryCode::ready,
                artifact_locator(ForestLibraryCode::ready, *asset, path, project_key_, decoded),
                std::move(decoded.artifact)};
        }
        catch (...) { return reject_artifact(ForestLibraryCode::allocation_failure); }
    }

    ForestLibraryCode ForestAssetLibrary::select_materialized(
        std::string_view logicalPath, const ForestArtifactLocator& publishedArtifact) noexcept
    {
        if (!valid()) return ForestLibraryCode::invalid_library;
        if (!publishedArtifact) return ForestLibraryCode::verified_key_required;
        try
        {
            auto state = verify_selected_materialized(project_root_, project_key_, logicalPath, limits_, metrics_.bytes_read);
            if (state.code != ForestLibraryCode::ready) return state.code;
            const auto& expected = state.locator;
            if (publishedArtifact.project_key != expected.project_key
                || publishedArtifact.asset_key != expected.asset_key
                || publishedArtifact.canonical_logical_path != expected.canonical_logical_path
                || publishedArtifact.artifact_key.words != expected.artifact_key.words
                || publishedArtifact.source_revision != expected.source_revision
                || publishedArtifact.source_content_hash != expected.source_content_hash
                || publishedArtifact.serialized_bytes != expected.serialized_bytes
                || !ordinary_selected_path(publishedArtifact.storage_path, false)
                || fs::canonical(publishedArtifact.storage_path) != expected.storage_path)
                return ForestLibraryCode::verified_key_required;

            std::error_code error;
            const bool existed = fs::exists(fs::symlink_status(state.reference_path, error));
            if (error && error != std::errc::no_such_file_or_directory) return ForestLibraryCode::read_failure;
            std::vector<std::byte> previous;
            if (existed)
            {
                if (!ordinary_selected_path(state.reference_path, false)) return ForestLibraryCode::invalid_path;
                const auto code = read_file(state.reference_path, kMaximumSelectedReferenceBytes,
                    ForestLibraryCode::artifact_too_large, previous);
                metrics_.bytes_read += previous.size();
                if (code != ForestLibraryCode::ready) return code;
                if (!valid_selected_reference(previous, state.root, project_key_, expected.asset_key,
                        expected.canonical_logical_path, limits_)) return ForestLibraryCode::integrity_failure;
            }
            if (!selected_source_is_current(state, limits_, metrics_.bytes_read)) return ForestLibraryCode::integrity_failure;
            if (previous == state.reference)
            {
                std::vector<std::byte> current;
                if (!ordinary_selected_path(state.reference_path, false)
                    || read_file(state.reference_path, kMaximumSelectedReferenceBytes,
                        ForestLibraryCode::artifact_too_large, current) != ForestLibraryCode::ready || current != previous)
                    return ForestLibraryCode::integrity_failure;
                return ForestLibraryCode::unchanged;
            }

            auto temporary = state.reference_path;
            temporary += ".pending." + hex_u64(next_temporary_id());
            error.clear();
            if (!platform::filesystem::exclusive_create_and_write(temporary, state.reference, error))
                return ForestLibraryCode::write_failure;
            const auto discard = [&] {
                std::error_code cleanup;
                fs::remove(temporary, cleanup);
            };
            std::vector<std::byte> verified;
            if (!ordinary_selected_path(temporary, false)
                || read_file(temporary, kMaximumSelectedReferenceBytes,
                    ForestLibraryCode::artifact_too_large, verified) != ForestLibraryCode::ready
                || verified != state.reference
                || !selected_source_is_current(state, limits_, metrics_.bytes_read))
            { discard(); return ForestLibraryCode::integrity_failure; }
            std::vector<std::byte> current;
            const bool stillExists = fs::exists(fs::symlink_status(state.reference_path, error));
            if ((error && error != std::errc::no_such_file_or_directory) || stillExists != existed
                || (existed && (!ordinary_selected_path(state.reference_path, false)
                    || read_file(state.reference_path, kMaximumSelectedReferenceBytes,
                        ForestLibraryCode::artifact_too_large, current) != ForestLibraryCode::ready || current != previous)))
            { discard(); return ForestLibraryCode::integrity_failure; }
            error.clear();
            if (!platform::filesystem::atomic_replace_same_filesystem(temporary, state.reference_path, error))
            { discard(); return ForestLibraryCode::atomic_replace_failure; }
            metrics_.bytes_written += state.reference.size();
            return ForestLibraryCode::ready;
        }
        catch (...) { return ForestLibraryCode::allocation_failure; }
    }

    LoadedForestArtifact ForestAssetLibrary::load_selected_materialized(std::string_view logicalPath) noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid()) return reject_artifact(ForestLibraryCode::invalid_library);
        try
        {
            if (!ordinary_selected_path(project_root_, true)) return reject_artifact(ForestLibraryCode::invalid_path);
            const auto root = fs::canonical(project_root_);
            ForestLibraryCode failure{};
            const auto asset = canonical_asset(root, project_key_, logicalPath, limits_, failure);
            if (!asset) return reject_artifact(failure);
            if (!ordinary_selected_path(root / "project.epoch.json", false)
                || !ordinary_selected_path(asset->source_path, false)
                || !ordinary_selected_path(asset->artifact_directory, true))
                return reject_artifact(ForestLibraryCode::invalid_path);
            const auto path = asset->artifact_directory / kSelectedReferenceName;
            std::error_code statusError;
            const auto status = fs::symlink_status(path, statusError);
            if (status.type() == fs::file_type::not_found
                && (!statusError || statusError == std::errc::no_such_file_or_directory))
                return reject_artifact(ForestLibraryCode::not_found);
            if (statusError) return reject_artifact(ForestLibraryCode::read_failure);
            if (!ordinary_selected_path(path, false)) return reject_artifact(ForestLibraryCode::invalid_path);
            std::vector<std::byte> reference;
            const auto code = read_file(path, kMaximumSelectedReferenceBytes,
                ForestLibraryCode::artifact_too_large, reference);
            metrics_.bytes_read += reference.size();
            if (code != ForestLibraryCode::ready) return reject_artifact(code);
            if (!valid_selected_reference(reference, root, project_key_, asset->asset_key, asset->logical_path, limits_))
                return reject_artifact(ForestLibraryCode::integrity_failure);
            auto state = verify_selected_materialized(root, project_key_, logicalPath, limits_, metrics_.bytes_read);
            if (state.code != ForestLibraryCode::ready) return reject_artifact(state.code);
            // Full canonical comparison includes the manifest, complete source
            // envelope and expected compiled key. A rehashed hint has no authority.
            if (reference != state.reference || !selected_source_is_current(state, limits_, metrics_.bytes_read))
                return reject_artifact(ForestLibraryCode::integrity_failure);
            std::vector<std::byte> current;
            if (!ordinary_selected_path(path, false)
                || read_file(path, kMaximumSelectedReferenceBytes, ForestLibraryCode::artifact_too_large, current)
                    != ForestLibraryCode::ready || current != reference)
                return reject_artifact(ForestLibraryCode::integrity_failure);
            ++metrics_.artifact_loads;
            return {ForestLibraryCode::ready, std::move(state.locator), std::move(state.artifact.artifact)};
        }
        catch (...) { return reject_artifact(ForestLibraryCode::allocation_failure); }
    }

    LoadedForestArtifact ForestAssetLibrary::load_latest(
        std::string_view logicalPath) noexcept
    {
        ++metrics_.artifact_load_requests;
        if (!valid())
            return reject_artifact(ForestLibraryCode::invalid_library);
        try
        {
            ForestLibraryCode pathFailure{};
            const auto asset = canonical_asset(
                project_root_, project_key_, logicalPath, limits_, pathFailure);
            if (!asset)
                return reject_artifact(pathFailure);
            std::error_code error{};
            if (!fs::is_directory(asset->artifact_directory, error) || error)
                return reject_artifact(ForestLibraryCode::not_found);

            std::vector<fs::path> candidates{};
            for (fs::directory_iterator iterator{
                    asset->artifact_directory, error}, end;
                !error && iterator != end; iterator.increment(error))
            {
                if (iterator->is_regular_file(error) && !error
                    && artifact_file(iterator->path()))
                {
                    candidates.push_back(iterator->path());
                    if (candidates.size() > limits_.maximum_artifacts_per_asset)
                    {
                        return reject_artifact(
                            ForestLibraryCode::artifact_limit_exceeded);
                    }
                }
            }
            if (error)
                return reject_artifact(ForestLibraryCode::directory_failure);
            std::sort(candidates.begin(), candidates.end());

            ArtifactBytes selected{};
            fs::path selectedPath{};
            bool found{};
            bool staleCompiler{};
            std::uint64_t staleRevision{};
            std::uint64_t staleContentHash{};
            for (const fs::path& candidate : candidates)
            {
                std::vector<std::byte> bytes{};
                const ForestLibraryCode read = read_file(
                    candidate, limits_.maximum_artifact_bytes,
                    ForestLibraryCode::artifact_too_large, bytes);
                metrics_.bytes_read += bytes.size();
                if (read != ForestLibraryCode::ready)
                    return reject_artifact(read);
                ArtifactBytes decoded = deserialize_artifact(bytes, limits_);
                if (decoded.code == ForestLibraryCode::verified_key_required)
                    return reject_artifact(decoded.code);
                if (decoded.code == ForestLibraryCode::rebuild_required)
                {
                    if (candidate.filename().generic_string() != hex_hash(decoded.digest)
                        + std::string{kArtifactExtension})
                        return reject_artifact(ForestLibraryCode::integrity_failure);
                    staleCompiler = true;
                    if (decoded.artifact.sourceRevision > staleRevision)
                    {
                        staleRevision = decoded.artifact.sourceRevision;
                        staleContentHash = decoded.artifact.sourceContentHash;
                    }
                    else if (decoded.artifact.sourceRevision == staleRevision
                        && decoded.artifact.sourceContentHash != staleContentHash)
                        return reject_artifact(ForestLibraryCode::ambiguous_artifact);
                    continue;
                }
                if (decoded.code != ForestLibraryCode::ready
                    || candidate.filename().generic_string()
                        != hex_hash(decoded.digest)
                            + std::string{kArtifactExtension})
                {
                    return reject_artifact(ForestLibraryCode::integrity_failure);
                }
                if (!found
                    || decoded.artifact.sourceRevision
                        > selected.artifact.sourceRevision)
                {
                    selected = std::move(decoded);
                    selectedPath = candidate;
                    found = true;
                }
                else if (decoded.artifact.sourceRevision
                        == selected.artifact.sourceRevision
                    && decoded.digest.words != selected.digest.words)
                {
                    return reject_artifact(
                        ForestLibraryCode::ambiguous_artifact);
                }
            }
            if (!found)
                return reject_artifact(staleCompiler ? ForestLibraryCode::rebuild_required : ForestLibraryCode::not_found);
            if (staleCompiler && staleRevision > selected.artifact.sourceRevision)
                return reject_artifact(ForestLibraryCode::rebuild_required);
            if (staleCompiler && staleRevision == selected.artifact.sourceRevision
                && staleContentHash != selected.artifact.sourceContentHash)
                return reject_artifact(ForestLibraryCode::ambiguous_artifact);
            ++metrics_.artifact_loads;
            return {
                ForestLibraryCode::ready,
                artifact_locator(
                    ForestLibraryCode::ready,
                    *asset,
                    selectedPath,
                    project_key_,
                    selected),
                std::move(selected.artifact)};
        }
        catch (...)
        {
            return reject_artifact(ForestLibraryCode::allocation_failure);
        }
    }

    PublishedForestAsset ForestAssetLibrary::reject_publish(
        ForestLibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedForestSource ForestAssetLibrary::reject_source(
        ForestLibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedForestArtifact ForestAssetLibrary::reject_artifact(
        ForestLibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    ForestLibraryContractFailure
        project_forest_library_contract_failure() noexcept
    {
        ContractRoot root{};
        if (!root.valid())
            return ForestLibraryContractFailure::temporary_root;
        ForestAssetLibrary library{"forest-contract", root.path};
        if (!library.valid())
            return ForestLibraryContractFailure::construction;
        forest::ForestAssetDocument document =
            forest::make_default_plant_lab_document();
        constexpr std::string_view logicalPath{
            "Assets/Forest/default.epoch_forest"};
        if (library.publish("Assets/default.epoch_forest", document).code
                != ForestLibraryCode::outside_forest_assets
            || library.publish("Assets/Forest/default.json", document).code
                != ForestLibraryCode::unsupported_extension)
        {
            return ForestLibraryContractFailure::invalid_path;
        }

        const PublishedForestAsset first = library.publish(logicalPath, document);
        if (!first || first.code != ForestLibraryCode::ready
            || first.artifact.source_revision != document.revision
            || first.artifact.source_content_hash != document.contentHash)
        {
            return ForestLibraryContractFailure::first_publication;
        }
        const PublishedForestAsset unchanged = library.publish(logicalPath, document);
        if (!unchanged || unchanged.code != ForestLibraryCode::unchanged
            || unchanged.source.revision != first.source.revision
            || unchanged.artifact.artifact_key.words
                != first.artifact.artifact_key.words)
        {
            return ForestLibraryContractFailure::unchanged_publication;
        }

        const LoadedForestSource source = library.load_source(logicalPath);
        if (!source || source.document.revision != document.revision
            || source.document.contentHash != document.contentHash
            || source.document.journal.size() != document.journal.size()
            || source.document.historyCursor != document.historyCursor)
        {
            return ForestLibraryContractFailure::source_reopen;
        }
        const LoadedForestArtifact exact = library.load_exact(
            logicalPath, first.artifact.artifact_key);
        if (!exact || exact.artifact.sourceRevision != document.revision
            || exact.artifact.sourceContentHash != document.contentHash
            || exact.artifact.preview.segmentCount
                != forest::compile_forest_asset(document).preview.segmentCount)
        {
            return ForestLibraryContractFailure::artifact_reopen;
        }

        ForestAssetLibrary restarted{"forest-contract", root.path};
        const LoadedForestSource restartedSource = restarted.load_source(logicalPath);
        const LoadedForestArtifact restartedArtifact = restarted.load_latest(logicalPath);
        if (!restartedSource || !restartedArtifact
            || restartedSource.document.contentHash != document.contentHash
            || restartedArtifact.locator.artifact_key.words
                != first.artifact.artifact_key.words)
        {
            return ForestLibraryContractFailure::restart_reopen;
        }

        if (!forest::apply_forest_profile_edit(
                document,
                forest::ForestProfileProperty::TargetHeightMeters,
                document.profile.config.targetHeightMeters + 0.5f))
        {
            return ForestLibraryContractFailure::second_revision;
        }
        const PublishedForestAsset second = restarted.publish(logicalPath, document);
        if (!second || second.code != ForestLibraryCode::ready
            || second.artifact.source_revision <= first.artifact.source_revision
            || second.artifact.artifact_key.words
                == first.artifact.artifact_key.words)
        {
            return ForestLibraryContractFailure::second_revision;
        }
        const LoadedForestArtifact oldExact = restarted.load_exact(
            logicalPath, first.artifact.artifact_key);
        if (!oldExact
            || oldExact.artifact.sourceRevision != first.artifact.source_revision)
        {
            return ForestLibraryContractFailure::exact_old_revision;
        }
        const LoadedForestArtifact latest = restarted.load_latest(logicalPath);
        if (!latest
            || latest.locator.artifact_key.words
                != second.artifact.artifact_key.words
            || latest.artifact.sourceRevision != document.revision)
        {
            return ForestLibraryContractFailure::latest_revision;
        }

        if (!corrupt_last_byte(second.source.storage_path)
            || restarted.load_source(logicalPath).code
                != ForestLibraryCode::integrity_failure)
        {
            return ForestLibraryContractFailure::malformed_source;
        }
        if (!corrupt_last_byte(first.artifact.storage_path)
            || restarted.load_exact(logicalPath, first.artifact.artifact_key).code
                != ForestLibraryCode::integrity_failure)
        {
            return ForestLibraryContractFailure::malformed_artifact;
        }

        const ForestLibraryMetrics metrics = restarted.metrics();
        if (metrics.publish_requests != 1u || metrics.publications != 1u
            || metrics.source_load_requests < 2u || metrics.source_loads != 1u
            || metrics.artifact_load_requests < 4u
            || metrics.artifact_loads < 3u
            || metrics.bytes_written == 0u || metrics.bytes_read == 0u
            || metrics.rejected_operations < 2u)
        {
            return ForestLibraryContractFailure::metrics;
        }
        return ForestLibraryContractFailure::none;
    }
}
