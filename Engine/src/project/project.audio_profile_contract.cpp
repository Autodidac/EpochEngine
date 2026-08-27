/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

module project.audio_profile;

namespace epochengine::project_audio
{
    namespace
    {
        struct ContractRoot final
        {
            std::filesystem::path path{};

            ContractRoot()
            {
                std::error_code error{};
                const auto stamp = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                path = std::filesystem::temp_directory_path(error)
                    / ("epoch_project_audio_contract_"
                        + std::to_string(stamp));
                if (error)
                {
                    path.clear();
                    return;
                }
                std::filesystem::create_directories(
                    path / "Assets" / "Audio", error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        };

        void append_u16(
            std::vector<std::uint8_t>& bytes,
            std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
        }

        void append_u32(
            std::vector<std::uint8_t>& bytes,
            std::uint32_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
            bytes.push_back(static_cast<std::uint8_t>(value >> 16u));
            bytes.push_back(static_cast<std::uint8_t>(value >> 24u));
        }

        void append_tag(
            std::vector<std::uint8_t>& bytes,
            std::string_view tag)
        {
            for (const char character : tag)
                bytes.push_back(static_cast<std::uint8_t>(character));
        }

        [[nodiscard]] std::vector<std::uint8_t> pcm16_wave(
            std::int16_t first,
            std::int16_t second)
        {
            std::vector<std::uint8_t> bytes{};
            append_tag(bytes, "RIFF");
            append_u32(bytes, 40u);
            append_tag(bytes, "WAVE");
            append_tag(bytes, "fmt ");
            append_u32(bytes, 16u);
            append_u16(bytes, 1u);
            append_u16(bytes, 1u);
            append_u32(bytes, 48'000u);
            append_u32(bytes, 96'000u);
            append_u16(bytes, 2u);
            append_u16(bytes, 16u);
            append_tag(bytes, "data");
            append_u32(bytes, 4u);
            append_u16(bytes, static_cast<std::uint16_t>(first));
            append_u16(bytes, static_cast<std::uint16_t>(second));
            return bytes;
        }

        [[nodiscard]] bool write_bytes(
            const std::filesystem::path& path,
            std::span<const std::uint8_t> bytes)
        {
            std::ofstream output(path, std::ios::binary);
            return output
                && static_cast<bool>(output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())));
        }

        [[nodiscard]] ProfileSource sample_profile()
        {
            return {
                .profile_id = 0x4550'4f43'485f'4155ull,
                .sequence = 7u,
                .buses = {
                    {
                        .id = {10u},
                        .name = "Effects",
                        .gain = 0.75f
                    },
                    {
                        .id = {20u},
                        .name = "Music",
                        .gain = 0.5f
                    },
                    {
                        .id = {30u},
                        .parent = {20u},
                        .name = "Ambient",
                        .gain = 0.8f
                    }
                },
                .cues = {
                    {
                        .id = {100u},
                        .semantic = CueSemantic::jump,
                        .name = "Actor Jump",
                        .logical_audio_path = "Assets/Audio/jump.wav",
                        .bus = {10u},
                        .gain = 0.9f
                    },
                    {
                        .id = {200u},
                        .semantic = CueSemantic::music,
                        .name = "World Music",
                        .logical_audio_path = "Assets/Audio/music.wav",
                        .bus = {30u},
                        .gain = 0.6f,
                        .looping = true,
                        .autoplay = true
                    }
                }
            };
        }
    }

    ContractFailure project_audio_profile_contract_failure() noexcept
    {
        ContractRoot root{};
        if (root.path.empty())
            return ContractFailure::temporary_root;

        const ProfileSource source = sample_profile();
        if (validate_profile(source) != ValidationCode::ready)
            return ContractFailure::validation;

        const SerializedProfile encoded = serialize_profile(source);
        const DeserializedProfile decoded =
            deserialize_profile(encoded.bytes);
        if (!encoded || !decoded || decoded.source != source
            || decoded.digest != encoded.digest)
            return ContractFailure::source_roundtrip;

        std::vector<std::byte> malformed = encoded.bytes;
        malformed[20u] ^= std::byte{0x7fu};
        if (deserialize_profile(malformed).code
            != CodecCode::integrity_failure)
            return ContractFailure::malformed_rejection;

        ProfileSource duplicateSemantic = source;
        duplicateSemantic.cues.back().semantic = CueSemantic::jump;
        if (validate_profile(duplicateSemantic)
            != ValidationCode::duplicate_semantic)
        {
            return ContractFailure::duplicate_semantic_rejection;
        }

        const auto busEdited = set_bus_mix(
            source, BusId{10u}, 0.25f, true);
        if (!busEdited
            || busEdited.code != ProfileEditCode::ready
            || busEdited.source.sequence != source.sequence + 1u
            || busEdited.source.buses.front().gain != 0.25f
            || !busEdited.source.buses.front().muted)
        {
            return ContractFailure::profile_edits;
        }
        const auto busUnchanged = set_bus_mix(
            busEdited.source, BusId{10u}, 0.25f, true);
        if (!busUnchanged
            || busUnchanged.code != ProfileEditCode::unchanged
            || busUnchanged.source.sequence != busEdited.source.sequence)
        {
            return ContractFailure::profile_edits;
        }
        const auto cueEdited = configure_cue(
            source,
            CueId{200u},
            CueSemantic::ambient,
            BusId{30u},
            0.4f,
            true,
            true);
        if (!cueEdited
            || cueEdited.source.sequence != source.sequence + 1u
            || cueEdited.source.cues.back().semantic
                != CueSemantic::ambient
            || cueEdited.source.cues.back().gain != 0.4f
            || !cueEdited.source.cues.back().looping
            || !cueEdited.source.cues.back().autoplay)
        {
            return ContractFailure::profile_edits;
        }
        const auto duplicateBinding = configure_cue(
            source,
            CueId{200u},
            CueSemantic::jump,
            BusId{30u},
            0.4f,
            true,
            true);
        if (duplicateBinding.code != ProfileEditCode::validation_failed
            || duplicateBinding.validation
                != ValidationCode::duplicate_semantic)
        {
            return ContractFailure::duplicate_semantic_rejection;
        }

        const auto jump = pcm16_wave(-16'384, 16'384);
        const auto music = pcm16_wave(8'192, -8'192);
        if (!write_bytes(root.path / "Assets/Audio/jump.wav", jump)
            || !write_bytes(root.path / "Assets/Audio/music.wav", music)
            || !write_bytes(root.path / "external_import.wav", jump))
            return ContractFailure::temporary_root;

        ProjectAudioProfileStore store{"contract_game", root.path};
        const ImportedAudioSource imported = store.import_source(
            root.path / "external_import.wav");
        if (!imported
            || imported.code != SourceImportCode::ready
            || !imported.created
            || imported.logical_path.rfind(
                    "Assets/Audio/imported/", 0u) != 0u
            || imported.source_bytes != jump.size()
            || imported.decoded_bytes != 2u * sizeof(float)
            || imported.frame_count != 2u
            || imported.sample_rate != 48'000u
            || imported.channel_count != 1u
            || !std::filesystem::is_regular_file(
                imported.destination_path))
        {
            return ContractFailure::source_import;
        }
        const ImportedAudioSource reused = store.import_source(
            root.path / "external_import.wav");
        if (!reused
            || reused.code != SourceImportCode::unchanged
            || reused.created
            || reused.logical_path != imported.logical_path
            || reused.source_digest != imported.source_digest)
        {
            return ContractFailure::source_import_reuse;
        }

        const auto cueAdded = add_cue(
            source,
            CueSource{
                .semantic = CueSemantic::custom,
                .name = "Imported Effect",
                .logical_audio_path = imported.logical_path,
                .bus = {10u},
                .gain = 0.7f});
        if (!cueAdded
            || cueAdded.source.cues.size() != source.cues.size() + 1u
            || !cueAdded.source.cues.back().id)
        {
            return ContractFailure::profile_edits;
        }
        const CueId importedCue = cueAdded.source.cues.back().id;
        const auto importedCompiled = store.compile(cueAdded.source, false);
        if (!importedCompiled
            || importedCompiled.request.cues.size() != 3u
            || importedCompiled.decoded_bytes
                != 6u * sizeof(float))
        {
            return ContractFailure::source_import;
        }
        const auto cueRemoved = remove_cue(
            cueAdded.source, importedCue);
        if (!cueRemoved
            || cueRemoved.source.cues.size() != source.cues.size()
            || std::any_of(
                cueRemoved.source.cues.begin(),
                cueRemoved.source.cues.end(),
                [importedCue](const CueSource& cue) noexcept
                {
                    return cue.id == importedCue;
                }))
        {
            return ContractFailure::profile_edits;
        }

        const StoredProfile saved = store.save(source);
        const LoadedProfile loaded = store.load();
        if (!saved || !loaded || loaded.source != source
            || loaded.digest != saved.digest)
            return ContractFailure::save_reopen;
        const StoredProfile unchanged = store.save(source);
        if (!unchanged || unchanged.code != StoreCode::unchanged)
            return ContractFailure::unchanged_save;

        CompiledSession compiled = store.compile(source, false);
        if (!compiled || compiled.request.buses.size() != 3u
            || compiled.request.cues.size() != 2u
            || compiled.bindings.size() != 2u
            || compiled.decoded_bytes != 4u * sizeof(float)
            || !compiled.find(CueSemantic::jump)
            || !compiled.find(CueId{200u}))
            return ContractFailure::session_compile;

        const SerializedAudioArtifact encodedArtifact =
            serialize_audio_artifact(compiled);
        const DeserializedAudioArtifact decodedArtifact =
            deserialize_audio_artifact(
                encodedArtifact.bytes, "contract_game", true);
        if (!encodedArtifact || !decodedArtifact
            || decodedArtifact.digest != encodedArtifact.digest
            || decodedArtifact.artifact.project_key != store.project_key()
            || decodedArtifact.artifact.artifact_key
                != compiled.artifact_key
            || decodedArtifact.artifact.profile_id != source.profile_id
            || decodedArtifact.artifact.source_sequence != source.sequence
            || decodedArtifact.artifact.source_digest
                != compiled.source_digest
            || !decodedArtifact.artifact.request.request_physical_output
            || decodedArtifact.artifact.request.cues.size() != 2u
            || decodedArtifact.artifact.request.cues.front()
                    .clip.interleaved_samples
                != compiled.request.cues.front().clip.interleaved_samples
            || decodedArtifact.artifact.bindings != compiled.bindings)
        {
            return ContractFailure::artifact_roundtrip;
        }
        std::vector<std::byte> malformedArtifact =
            encodedArtifact.bytes;
        malformedArtifact[48u] ^= std::byte{0x55u};
        if (deserialize_audio_artifact(
                malformedArtifact, "contract_game", false).code
            != CodecCode::integrity_failure)
        {
            return ContractFailure::artifact_integrity;
        }

        const StoredAudioArtifact published =
            store.publish_artifact(compiled);
        const StoredAudioArtifact artifactUnchanged =
            store.publish_artifact(compiled);
        LoadedAudioArtifact restored = store.load_artifact(false);
        if (!published || !artifactUnchanged || !restored
            || published.code != StoreCode::ready
            || artifactUnchanged.code != StoreCode::unchanged
            || restored.digest != published.digest
            || restored.artifact.source_digest != compiled.source_digest
            || restored.artifact.request.request_physical_output
            || restored.artifact.bindings != compiled.bindings)
        {
            return ContractFailure::artifact_roundtrip;
        }

        if (!write_bytes(
                store.source_path(),
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(malformed.data()),
                    malformed.size()}))
            return ContractFailure::stale_artifact_refusal;
        ProjectAudioProfileStore refusalStore{"contract_game", root.path};
        const PreparedAudioSession refused = refusalStore.prepare(false);
        if (refused
            || refused.code != PreparationCode::source_failure
            || refused.compiled_from_source
            || refused.restored_from_artifact)
        {
            return ContractFailure::stale_artifact_refusal;
        }
        if (!write_bytes(
                store.source_path(),
                std::span<const std::uint8_t>{
                    reinterpret_cast<const std::uint8_t*>(
                        encoded.bytes.data()),
                    encoded.bytes.size()}))
            return ContractFailure::stale_artifact_refusal;

        const CueBinding* jumpBinding =
            compiled.find(CueSemantic::jump);
        const CueBinding* musicBinding =
            compiled.find(CueSemantic::music);
        if (!jumpBinding || !musicBinding
            || jumpBinding->runtime_clip == musicBinding->runtime_clip
            || jumpBinding->audio_asset_key == 0u
            || musicBinding->audio_asset_key == 0u
            || compiled.autoplay_clips()
                != std::vector<audio::ClipId>{musicBinding->runtime_clip})
            return ContractFailure::stable_identity;

        const audio::LogicalResourceId effects{
            store.project_key(), 10u};
        const audio::LogicalResourceId musicBus{
            store.project_key(), 20u};
        audio::PlaybackRuntime runtime{};
        const audio::PlaybackSessionResult opened =
            runtime.open_session(std::move(restored.artifact.request));
        if (!opened)
            return ContractFailure::playback_open;

        if (runtime.set_bus_gain(opened.handle, effects, 0.25f)
                != audio::PlaybackRuntimeCode::success
            || runtime.set_bus_muted(opened.handle, musicBus, true)
                != audio::PlaybackRuntimeCode::success
            || runtime.set_bus_muted(opened.handle, musicBus, false)
                != audio::PlaybackRuntimeCode::success)
            return ContractFailure::bus_controls;

        if (runtime.trigger(
                opened.handle, musicBinding->runtime_clip)
                != audio::PlaybackRuntimeCode::success
            || !runtime.advance(opened.handle, 1.0 / 48'000.0)
            || runtime.close_session(opened.handle)
                != audio::PlaybackRuntimeCode::success)
            return ContractFailure::autoplay;

        ProfileSource missing = source;
        missing.cues.front().logical_audio_path =
            "Assets/Audio/missing.wav";
        if (store.compile(missing, false).code
            != CompileCode::source_not_found)
            return ContractFailure::missing_source_rejection;

        std::error_code removeError{};
        std::filesystem::remove(store.source_path(), removeError);
        if (removeError)
            return ContractFailure::artifact_only_prepare;
        std::filesystem::remove(
            root.path / "Assets/Audio/jump.wav", removeError);
        if (removeError)
            return ContractFailure::artifact_only_prepare;
        std::filesystem::remove(
            root.path / "Assets/Audio/music.wav", removeError);
        if (removeError)
            return ContractFailure::artifact_only_prepare;

        PreparedAudioSession prepared = store.prepare(false);
        if (!prepared
            || prepared.compiled_from_source
            || !prepared.restored_from_artifact
            || prepared.session.source_digest != compiled.source_digest
            || prepared.session.request.cues.size() != 2u
            || prepared.session.bindings != compiled.bindings)
        {
            return ContractFailure::artifact_only_prepare;
        }

        const StoreMetrics metrics = store.metrics();
        if (metrics.save_requests != 2u
            || metrics.saves != 1u
            || metrics.load_requests != 2u
            || metrics.loads != 1u
            || metrics.unchanged_writes != 1u
            || metrics.artifact_save_requests != 2u
            || metrics.artifact_saves != 1u
            || metrics.artifact_load_requests != 2u
            || metrics.artifact_loads != 2u
            || metrics.artifact_unchanged_writes != 1u
            || metrics.rejected_operations != 1u
            || metrics.bytes_read == 0u
            || metrics.bytes_written == 0u
            || metrics.artifact_bytes_read == 0u
            || metrics.artifact_bytes_written == 0u)
            return ContractFailure::metrics;

        return ContractFailure::none;
    }
}

#if defined(EPOCH_PROJECT_AUDIO_PROFILE_CONTRACT_MAIN)
extern "C++" int main()
{
    return epochengine::project_audio::
        project_audio_profile_contract_failure()
            == epochengine::project_audio::ContractFailure::none
        ? 0
        : 1;
}
#endif
