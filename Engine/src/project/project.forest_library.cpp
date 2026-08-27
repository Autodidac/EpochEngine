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
#include <utility>
#include <vector>

module project.forest_library;

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
            const ForestLibraryLimits& limits) noexcept
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
                || document.profile.branch.levels == 0u
                || document.profile.branch.levels > 32u
                || document.profile.branch.childrenPerNode == 0u
                || document.profile.branch.childrenPerNode > 8u
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
                    || static_cast<std::uint8_t>(edit.property)
                        > static_cast<std::uint8_t>(
                            forest::ForestProfileProperty::GrowthTimeSeconds))
                {
                    return false;
                }
                previous = edit.sequence;
            }

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

        [[nodiscard]] std::vector<std::byte> serialize_source(
            const forest::ForestAssetDocument& document,
            const ForestLibraryLimits& limits) noexcept
        {
            if (!valid_source_document(document, limits))
                return {};
            Writer writer{limits.maximum_source_bytes - kDigestBytes};
            writer.raw(kSourceMagic);
            writer.integer(kSchemaVersion);
            writer.integer(document.genome.stableHash);
            writer.text(document.name);
            write_profile(writer, document.profile);
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
            const ForestLibraryLimits& limits) noexcept
        {
            if (bytes.size() > limits.maximum_source_bytes)
                return {ForestLibraryCode::source_too_large};
            const auto body = integrity_verified_body(bytes);
            if (!body)
                return {ForestLibraryCode::integrity_failure};
            Reader reader{*body};
            std::array<std::uint8_t, kSourceMagic.size()> magic{};
            if (!reader.raw(magic) || magic != kSourceMagic
                || reader.integer<std::uint16_t>() != kSchemaVersion)
            {
                return {ForestLibraryCode::invalid_source};
            }

            forest::ForestAssetDocument document{};
            document.genome.stableHash = reader.integer<std::uint64_t>();
            document.name = reader.text(limits.maximum_name_bytes);
            document.profile = read_profile(reader);
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
                }
            }
            catch (...)
            {
                return {ForestLibraryCode::allocation_failure};
            }
            if (!reader.finished() || !valid_source_document(document, limits))
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
            writer.integer(kSchemaVersion);
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
            const ForestLibraryLimits& limits) noexcept
        {
            if (bytes.size() > limits.maximum_artifact_bytes)
                return {ForestLibraryCode::artifact_too_large};
            const auto body = integrity_verified_body(bytes);
            if (!body)
                return {ForestLibraryCode::integrity_failure};
            Reader reader{*body};
            std::array<std::uint8_t, kArtifactMagic.size()> magic{};
            if (!reader.raw(magic) || magic != kArtifactMagic
                || reader.integer<std::uint16_t>() != kSchemaVersion)
            {
                return {ForestLibraryCode::invalid_artifact};
            }
            std::vector<std::byte> sourceBytes = reader.byte_vector(
                static_cast<std::size_t>(limits.maximum_source_bytes));
            if (!reader.ok())
                return {ForestLibraryCode::invalid_artifact};
            SourceBytes source = deserialize_source(sourceBytes, limits);
            if (source.code != ForestLibraryCode::ready)
                return {ForestLibraryCode::invalid_artifact};
            const forest::CompiledForestAsset artifact =
                forest::compile_forest_asset(source.document);
            const std::vector<std::byte> expected = compiled_descriptor(
                artifact, limits.maximum_artifact_bytes);
            if (expected.empty() || reader.remaining() != expected.size())
                return {ForestLibraryCode::invalid_artifact};
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
                    return {ForestLibraryCode::integrity_failure};
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
            source.bytes = serialize_source(document, limits_);
            if (source.bytes.empty())
                return reject_publish(ForestLibraryCode::invalid_source);
            source = deserialize_source(source.bytes, limits_);
            if (source.code != ForestLibraryCode::ready)
                return reject_publish(source.code);

            ArtifactBytes artifact{};
            artifact.bytes = serialize_artifact(
                source.document, source.bytes, limits_);
            if (artifact.bytes.empty())
                return reject_publish(ForestLibraryCode::invalid_artifact);
            artifact = deserialize_artifact(artifact.bytes, limits_);
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
                    ? deserialize_artifact(existing, limits_)
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
                    ? deserialize_artifact(verifiedBytes, limits_)
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
                    ? deserialize_source(verifiedBytes, limits_)
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
                return reject_artifact(ForestLibraryCode::not_found);
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
