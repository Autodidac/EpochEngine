/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.audio_profile;

export import audio.playback_runtime;
export import asset.audio_import;
export import core.sha256;
export import project.asset_registry;

export namespace epochengine::project_audio
{
    inline constexpr std::string_view canonical_source_path{
        "Assets/Audio/project_audio.epochaudio"};
    inline constexpr std::string_view canonical_artifact_path{
        "Library/Audio/project_audio.epochaudioc"};
    inline constexpr std::uint32_t artifact_schema_version{1u};

    struct BusId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(BusId, BusId) noexcept = default;
    };

    struct CueId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u;
        }

        friend constexpr auto operator<=>(CueId, CueId) noexcept = default;
    };

    enum class CueSemantic : std::uint8_t
    {
        custom,
        jump,
        land,
        music,
        ambient,
        user_interface
    };

    [[nodiscard]] constexpr std::string_view cue_semantic_name(
        CueSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case CueSemantic::custom: return "custom";
        case CueSemantic::jump: return "jump";
        case CueSemantic::land: return "land";
        case CueSemantic::music: return "music";
        case CueSemantic::ambient: return "ambient";
        case CueSemantic::user_interface: return "user_interface";
        }
        return "unknown";
    }

    struct BusSource final
    {
        BusId id{};
        BusId parent{};
        std::string name{};
        float gain{1.0f};
        bool muted{};

        friend bool operator==(const BusSource&, const BusSource&) = default;
    };

    struct CueSource final
    {
        CueId id{};
        CueSemantic semantic{CueSemantic::custom};
        std::string name{};
        std::string logical_audio_path{};
        BusId bus{};
        float gain{1.0f};
        bool looping{};
        bool autoplay{};

        friend bool operator==(const CueSource&, const CueSource&) = default;
    };

    struct ProfileSource final
    {
        std::uint64_t profile_id{};
        std::uint64_t sequence{1u};
        std::vector<BusSource> buses{};
        std::vector<CueSource> cues{};

        friend bool operator==(const ProfileSource&, const ProfileSource&) = default;
    };

    struct ProfileLimits final
    {
        std::uint32_t maximum_buses{64u};
        std::uint32_t maximum_cues{2'048u};
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_path_bytes{1'024u};
        std::uint64_t maximum_serialized_bytes{4ull * 1024ull * 1024ull};
        std::uint64_t maximum_artifact_bytes{544ull * 1024ull * 1024ull};
        std::uint64_t maximum_total_decoded_bytes{512ull * 1024ull * 1024ull};
        asset::audio::AudioImportLimits import{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_buses != 0u
                && maximum_cues != 0u
                && maximum_name_bytes != 0u
                && maximum_path_bytes != 0u
                && maximum_serialized_bytes >= 64u
                && maximum_artifact_bytes >= maximum_total_decoded_bytes
                && maximum_total_decoded_bytes >= sizeof(float)
                && import.valid();
        }
    };

    enum class ValidationCode : std::uint8_t
    {
        ready,
        invalid_limits,
        invalid_profile,
        bus_limit_exceeded,
        cue_limit_exceeded,
        invalid_bus,
        duplicate_bus,
        missing_parent,
        bus_cycle,
        invalid_cue,
        duplicate_cue,
        duplicate_semantic,
        duplicate_audio_path,
        missing_bus,
        invalid_path
    };

    [[nodiscard]] constexpr std::string_view validation_code_name(
        ValidationCode code) noexcept
    {
        switch (code)
        {
        case ValidationCode::ready: return "ready";
        case ValidationCode::invalid_limits: return "invalid_limits";
        case ValidationCode::invalid_profile: return "invalid_profile";
        case ValidationCode::bus_limit_exceeded: return "bus_limit_exceeded";
        case ValidationCode::cue_limit_exceeded: return "cue_limit_exceeded";
        case ValidationCode::invalid_bus: return "invalid_bus";
        case ValidationCode::duplicate_bus: return "duplicate_bus";
        case ValidationCode::missing_parent: return "missing_parent";
        case ValidationCode::bus_cycle: return "bus_cycle";
        case ValidationCode::invalid_cue: return "invalid_cue";
        case ValidationCode::duplicate_cue: return "duplicate_cue";
        case ValidationCode::duplicate_semantic: return "duplicate_semantic";
        case ValidationCode::duplicate_audio_path: return "duplicate_audio_path";
        case ValidationCode::missing_bus: return "missing_bus";
        case ValidationCode::invalid_path: return "invalid_path";
        }
        return "unknown";
    }

    [[nodiscard]] ProfileSource make_default_2d_profile() noexcept;

    [[nodiscard]] ValidationCode validate_profile(
        const ProfileSource& source,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    enum class ProfileEditCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_source,
        invalid_bus,
        invalid_cue,
        invalid_gain,
        id_exhausted,
        revision_exhausted,
        validation_failed,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view profile_edit_code_name(
        ProfileEditCode code) noexcept
    {
        switch (code)
        {
        case ProfileEditCode::ready: return "ready";
        case ProfileEditCode::unchanged: return "unchanged";
        case ProfileEditCode::invalid_source: return "invalid_source";
        case ProfileEditCode::invalid_bus: return "invalid_bus";
        case ProfileEditCode::invalid_cue: return "invalid_cue";
        case ProfileEditCode::invalid_gain: return "invalid_gain";
        case ProfileEditCode::id_exhausted: return "id_exhausted";
        case ProfileEditCode::revision_exhausted:
            return "revision_exhausted";
        case ProfileEditCode::validation_failed: return "validation_failed";
        case ProfileEditCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    struct ProfileEditResult final
    {
        ProfileEditCode code{ProfileEditCode::invalid_source};
        ValidationCode validation{ValidationCode::invalid_profile};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ProfileEditCode::ready
                || code == ProfileEditCode::unchanged;
        }
    };

    [[nodiscard]] ProfileEditResult set_bus_mix(
        ProfileSource source,
        BusId bus,
        float gain,
        bool muted,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] ProfileEditResult configure_cue(
        ProfileSource source,
        CueId cue,
        CueSemantic semantic,
        BusId bus,
        float gain,
        bool looping,
        bool autoplay,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] ProfileEditResult add_cue(
        ProfileSource source,
        CueSource cue,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] ProfileEditResult remove_cue(
        ProfileSource source,
        CueId cue,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    [[nodiscard]] core::sha256::Digest profile_digest(
        const ProfileSource& source,
        const ProfileLimits& limits = {}) noexcept;

    enum class CodecCode : std::uint8_t
    {
        ready,
        invalid_source,
        malformed_data,
        version_unsupported,
        size_limit_exceeded,
        integrity_failure,
        allocation_failure
    };

    struct SerializedProfile final
    {
        CodecCode code{CodecCode::invalid_source};
        core::sha256::Digest digest{};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready
                && digest.valid()
                && !bytes.empty();
        }
    };

    struct DeserializedProfile final
    {
        CodecCode code{CodecCode::malformed_data};
        core::sha256::Digest digest{};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready && digest.valid();
        }
    };

    [[nodiscard]] SerializedProfile serialize_profile(
        const ProfileSource& source,
        const ProfileLimits& limits = {}) noexcept;
    [[nodiscard]] DeserializedProfile deserialize_profile(
        std::span<const std::byte> bytes,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    struct CueBinding final
    {
        CueId source_id{};
        CueSemantic semantic{CueSemantic::custom};
        std::string logical_audio_path{};
        std::uint64_t audio_asset_key{};
        audio::ClipId runtime_clip{};
        bool autoplay{};
        bool looping{};

        friend bool operator==(const CueBinding&, const CueBinding&) = default;
    };

    enum class CompileCode : std::uint8_t
    {
        ready,
        invalid_project,
        invalid_source,
        source_not_found,
        source_outside_project,
        import_failed,
        decoded_budget_exceeded,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view compile_code_name(
        CompileCode code) noexcept
    {
        switch (code)
        {
        case CompileCode::ready: return "ready";
        case CompileCode::invalid_project: return "invalid_project";
        case CompileCode::invalid_source: return "invalid_source";
        case CompileCode::source_not_found: return "source_not_found";
        case CompileCode::source_outside_project: return "source_outside_project";
        case CompileCode::import_failed: return "import_failed";
        case CompileCode::decoded_budget_exceeded:
            return "decoded_budget_exceeded";
        case CompileCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct CompiledSession final
    {
        CompileCode code{CompileCode::invalid_source};
        std::uint64_t project_key{};
        std::uint64_t artifact_key{};
        std::uint64_t profile_id{};
        std::uint64_t source_sequence{};
        core::sha256::Digest source_digest{};
        audio::PlaybackSessionRequest request{};
        std::vector<CueBinding> bindings{};
        std::uint64_t decoded_bytes{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CompileCode::ready
                && project_key != 0u
                && artifact_key != 0u
                && profile_id != 0u
                && source_sequence != 0u
                && source_digest.valid()
                && request.stable_session_id == artifact_key
                && !request.cues.empty()
                && request.cues.size() == bindings.size();
        }

        [[nodiscard]] const CueBinding* find(CueId id) const noexcept;
        [[nodiscard]] const CueBinding* find(CueSemantic semantic) const noexcept;
        [[nodiscard]] std::vector<audio::ClipId> autoplay_clips() const;
    };

    struct SerializedAudioArtifact final
    {
        CodecCode code{CodecCode::invalid_source};
        core::sha256::Digest digest{};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready
                && digest.valid()
                && !bytes.empty();
        }
    };

    struct DeserializedAudioArtifact final
    {
        CodecCode code{CodecCode::malformed_data};
        core::sha256::Digest digest{};
        CompiledSession artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CodecCode::ready
                && digest.valid()
                && static_cast<bool>(artifact);
        }
    };

    [[nodiscard]] SerializedAudioArtifact serialize_audio_artifact(
        const CompiledSession& artifact,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;
    [[nodiscard]] DeserializedAudioArtifact deserialize_audio_artifact(
        std::span<const std::byte> bytes,
        std::string_view projectId,
        bool requestPhysicalOutput,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    [[nodiscard]] CompiledSession compile_profile(
        std::string_view projectId,
        const std::filesystem::path& projectRoot,
        const ProfileSource& source,
        bool requestPhysicalOutput,
        const ProfileLimits& limits = {},
        const project_assets::RegistryLimits& registryLimits = {}) noexcept;

    enum class StoreCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_store,
        invalid_source,
        not_found,
        directory_failure,
        read_failure,
        write_failure,
        integrity_failure,
        atomic_replace_failure,
        size_limit_exceeded,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view store_code_name(
        StoreCode code) noexcept
    {
        switch (code)
        {
        case StoreCode::ready: return "ready";
        case StoreCode::unchanged: return "unchanged";
        case StoreCode::invalid_store: return "invalid_store";
        case StoreCode::invalid_source: return "invalid_source";
        case StoreCode::not_found: return "not_found";
        case StoreCode::directory_failure: return "directory_failure";
        case StoreCode::read_failure: return "read_failure";
        case StoreCode::write_failure: return "write_failure";
        case StoreCode::integrity_failure: return "integrity_failure";
        case StoreCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case StoreCode::size_limit_exceeded: return "size_limit_exceeded";
        case StoreCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct StoreMetrics final
    {
        std::uint64_t save_requests{};
        std::uint64_t saves{};
        std::uint64_t load_requests{};
        std::uint64_t loads{};
        std::uint64_t unchanged_writes{};
        std::uint64_t rejected_operations{};
        std::uint64_t bytes_read{};
        std::uint64_t bytes_written{};
        std::uint64_t artifact_save_requests{};
        std::uint64_t artifact_saves{};
        std::uint64_t artifact_load_requests{};
        std::uint64_t artifact_loads{};
        std::uint64_t artifact_unchanged_writes{};
        std::uint64_t artifact_bytes_read{};
        std::uint64_t artifact_bytes_written{};

        friend constexpr bool operator==(
            const StoreMetrics&, const StoreMetrics&) noexcept = default;
    };

    struct StoredProfile final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path path{};
        core::sha256::Digest digest{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready || code == StoreCode::unchanged)
                && !path.empty() && digest.valid() && serialized_bytes != 0u;
        }
    };

    struct LoadedProfile final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path path{};
        core::sha256::Digest digest{};
        ProfileSource source{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready
                && !path.empty() && digest.valid();
        }
    };

    struct StoredAudioArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path path{};
        core::sha256::Digest digest{};
        core::sha256::Digest source_digest{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == StoreCode::ready || code == StoreCode::unchanged)
                && !path.empty()
                && digest.valid()
                && source_digest.valid()
                && serialized_bytes != 0u;
        }
    };

    struct LoadedAudioArtifact final
    {
        StoreCode code{StoreCode::invalid_store};
        std::filesystem::path path{};
        core::sha256::Digest digest{};
        CompiledSession artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == StoreCode::ready
                && !path.empty()
                && digest.valid()
                && static_cast<bool>(artifact);
        }
    };

    enum class PreparationCode : std::uint8_t
    {
        ready,
        invalid_store,
        source_failure,
        compile_failure,
        artifact_failure
    };

    [[nodiscard]] constexpr std::string_view preparation_code_name(
        PreparationCode code) noexcept
    {
        switch (code)
        {
        case PreparationCode::ready: return "ready";
        case PreparationCode::invalid_store: return "invalid_store";
        case PreparationCode::source_failure: return "source_failure";
        case PreparationCode::compile_failure: return "compile_failure";
        case PreparationCode::artifact_failure: return "artifact_failure";
        }
        return "unknown";
    }

    struct PreparedAudioSession final
    {
        PreparationCode code{PreparationCode::invalid_store};
        CompiledSession session{};
        bool compiled_from_source{};
        bool restored_from_artifact{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == PreparationCode::ready
                && static_cast<bool>(session)
                && (compiled_from_source != restored_from_artifact);
        }
    };

    enum class SourceImportCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_store,
        invalid_source,
        unsupported_source,
        source_too_large,
        decode_failed,
        directory_failure,
        read_failure,
        write_failure,
        integrity_failure,
        atomic_replace_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view source_import_code_name(
        SourceImportCode code) noexcept
    {
        switch (code)
        {
        case SourceImportCode::ready: return "ready";
        case SourceImportCode::unchanged: return "unchanged";
        case SourceImportCode::invalid_store: return "invalid_store";
        case SourceImportCode::invalid_source: return "invalid_source";
        case SourceImportCode::unsupported_source: return "unsupported_source";
        case SourceImportCode::source_too_large: return "source_too_large";
        case SourceImportCode::decode_failed: return "decode_failed";
        case SourceImportCode::directory_failure: return "directory_failure";
        case SourceImportCode::read_failure: return "read_failure";
        case SourceImportCode::write_failure: return "write_failure";
        case SourceImportCode::integrity_failure: return "integrity_failure";
        case SourceImportCode::atomic_replace_failure:
            return "atomic_replace_failure";
        case SourceImportCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    struct ImportedAudioSource final
    {
        SourceImportCode code{SourceImportCode::invalid_source};
        bool created{};
        std::string logical_path{};
        std::filesystem::path destination_path{};
        core::sha256::Digest source_digest{};
        std::uint64_t source_bytes{};
        std::uint64_t decoded_bytes{};
        std::uint64_t frame_count{};
        std::uint32_t sample_rate{};
        std::uint16_t channel_count{};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == SourceImportCode::ready
                    || code == SourceImportCode::unchanged)
                && !logical_path.empty()
                && !destination_path.empty()
                && source_digest.valid()
                && frame_count != 0u;
        }
    };

    class ProjectAudioProfileStore final
    {
    public:
        ProjectAudioProfileStore(
            std::string projectId,
            std::filesystem::path projectRoot,
            ProfileLimits limits = {},
            project_assets::RegistryLimits registryLimits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const std::filesystem::path& project_root() const noexcept;
        [[nodiscard]] std::filesystem::path source_path() const;
        [[nodiscard]] std::filesystem::path artifact_path() const;
        [[nodiscard]] const ProfileLimits& limits() const noexcept;
        [[nodiscard]] StoreMetrics metrics() const noexcept;

        [[nodiscard]] StoredProfile save(const ProfileSource& source) noexcept;
        [[nodiscard]] LoadedProfile load() noexcept;
        [[nodiscard]] StoredAudioArtifact publish_artifact(
            const CompiledSession& artifact) noexcept;
        [[nodiscard]] LoadedAudioArtifact load_artifact(
            bool requestPhysicalOutput) noexcept;
        [[nodiscard]] PreparedAudioSession prepare(
            bool requestPhysicalOutput) noexcept;
        [[nodiscard]] ImportedAudioSource import_source(
            const std::filesystem::path& externalSource) const noexcept;
        [[nodiscard]] CompiledSession compile(
            const ProfileSource& source,
            bool requestPhysicalOutput) const noexcept;

    private:
        [[nodiscard]] StoredProfile reject_store(StoreCode code) noexcept;
        [[nodiscard]] LoadedProfile reject_load(StoreCode code) noexcept;
        [[nodiscard]] StoredAudioArtifact reject_artifact(
            StoreCode code) noexcept;
        [[nodiscard]] LoadedAudioArtifact reject_artifact_load(
            StoreCode code) noexcept;

        std::string project_id_{};
        std::filesystem::path project_root_{};
        ProfileLimits limits_{};
        project_assets::RegistryLimits registry_limits_{};
        std::uint64_t project_key_{};
        StoreMetrics metrics_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        temporary_root,
        validation,
        source_roundtrip,
        malformed_rejection,
        save_reopen,
        unchanged_save,
        session_compile,
        stable_identity,
        playback_open,
        bus_controls,
        autoplay,
        profile_edits,
        duplicate_semantic_rejection,
        source_import,
        source_import_reuse,
        artifact_roundtrip,
        artifact_integrity,
        stale_artifact_refusal,
        artifact_only_prepare,
        missing_source_rejection,
        metrics
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::temporary_root: return "temporary_root";
        case ContractFailure::validation: return "validation";
        case ContractFailure::source_roundtrip: return "source_roundtrip";
        case ContractFailure::malformed_rejection: return "malformed_rejection";
        case ContractFailure::save_reopen: return "save_reopen";
        case ContractFailure::unchanged_save: return "unchanged_save";
        case ContractFailure::session_compile: return "session_compile";
        case ContractFailure::stable_identity: return "stable_identity";
        case ContractFailure::playback_open: return "playback_open";
        case ContractFailure::bus_controls: return "bus_controls";
        case ContractFailure::autoplay: return "autoplay";
        case ContractFailure::profile_edits: return "profile_edits";
        case ContractFailure::duplicate_semantic_rejection:
            return "duplicate_semantic_rejection";
        case ContractFailure::source_import: return "source_import";
        case ContractFailure::source_import_reuse:
            return "source_import_reuse";
        case ContractFailure::artifact_roundtrip:
            return "artifact_roundtrip";
        case ContractFailure::artifact_integrity:
            return "artifact_integrity";
        case ContractFailure::stale_artifact_refusal:
            return "stale_artifact_refusal";
        case ContractFailure::artifact_only_prepare:
            return "artifact_only_prepare";
        case ContractFailure::missing_source_rejection:
            return "missing_source_rejection";
        case ContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure
        project_audio_profile_contract_failure() noexcept;
}
