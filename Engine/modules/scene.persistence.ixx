/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

export module scene.persistence;

import scenesnapshot;
import sceneserializer;
import platform.filesystem;

namespace epochengine::scene::persistence::detail
{
    namespace fs = std::filesystem;

    inline std::atomic<std::uint64_t> temporary_sequence{ 1u };

    [[nodiscard]] inline fs::path normalized_parent(const fs::path& path)
    {
        const fs::path parent = path.parent_path();
        return parent.empty() ? fs::path(".") : parent.lexically_normal();
    }

    [[nodiscard]] inline bool same_directory(const fs::path& lhs, const fs::path& rhs)
    {
        return normalized_parent(lhs) == normalized_parent(rhs);
    }

    [[nodiscard]] inline std::string validation_error(const SceneDocumentValidation& validation)
    {
        std::string error{ "scene validation failed:" };
        const auto append = [&error](bool valid, std::string_view name)
        {
            if (!valid)
            {
                error.push_back(' ');
                error.append(name);
            }
        };

        append(validation.identity_valid, "identity");
        append(validation.counts_bounded, "counts");
        append(validation.strings_bounded, "strings");
        append(validation.object_ids_valid, "object-ids");
        append(validation.object_ids_unique, "duplicate-object-ids");
        append(validation.object_names_unique, "duplicate-object-names");
        append(validation.transforms_valid, "transforms");
        append(validation.timeline_valid, "timeline");
        append(validation.packages_valid, "packages");
        append(validation.runtime_references_valid, "runtime-references");
        return error;
    }

    [[nodiscard]] inline bool equal_object(
        const SceneObjectSnapshot& lhs,
        const SceneObjectSnapshot& rhs) noexcept
    {
        const auto equal_vec3 = [](const Vec3& a, const Vec3& b) noexcept
        {
            return std::equal(a.begin(), a.end(), b.begin());
        };
        return lhs.id == rhs.id
            && lhs.name == rhs.name
            && lhs.type == rhs.type
            && lhs.category == rhs.category
            && equal_vec3(lhs.position, rhs.position)
            && equal_vec3(lhs.rotation, rhs.rotation)
            && equal_vec3(lhs.scale, rhs.scale)
            && lhs.visible == rhs.visible
            && lhs.editor_only == rhs.editor_only;
    }

    [[nodiscard]] inline bool equal_timeline_key(
        const SceneTimelineKey& lhs,
        const SceneTimelineKey& rhs) noexcept
    {
        return lhs.simulated_seconds == rhs.simulated_seconds
            && lhs.frame_index == rhs.frame_index
            && lhs.label == rhs.label
            && lhs.event_kind == rhs.event_kind
            && lhs.target_name == rhs.target_name
            && lhs.payload == rhs.payload;
    }

    [[nodiscard]] inline bool semantically_equal(
        const SceneSnapshot& lhs,
        const SceneSnapshot& rhs) noexcept
    {
        return lhs.scene_id == rhs.scene_id
            && lhs.project_id == rhs.project_id
            && lhs.world_name == rhs.world_name
            && lhs.document_kind == rhs.document_kind
            && lhs.support_tier == rhs.support_tier
            && lhs.revision == rhs.revision
            && lhs.primary_camera == rhs.primary_camera
            && lhs.primary_spawn == rhs.primary_spawn
            && lhs.captured_frame_index == rhs.captured_frame_index
            && lhs.captured_simulated_seconds == rhs.captured_simulated_seconds
            && lhs.packages == rhs.packages
            && lhs.objects.size() == rhs.objects.size()
            && std::equal(lhs.objects.begin(), lhs.objects.end(), rhs.objects.begin(), equal_object)
            && lhs.timeline_keys.size() == rhs.timeline_keys.size()
            && std::equal(
                lhs.timeline_keys.begin(),
                lhs.timeline_keys.end(),
                rhs.timeline_keys.begin(),
                equal_timeline_key);
    }

    [[nodiscard]] inline std::uint64_t next_temporary_nonce() noexcept
    {
        const auto ticks = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        return ticks ^ temporary_sequence.fetch_add(1u, std::memory_order_relaxed);
    }

    [[nodiscard]] inline fs::path generated_temporary_path(
        const fs::path& destination,
        std::uint64_t nonce)
    {
        fs::path temporary = destination;
        temporary += ".tmp.";
        temporary += std::to_string(nonce);
        return temporary;
    }

    inline void remove_owned_temporary(const fs::path& temporary) noexcept
    {
        if (temporary.empty())
            return;
        std::error_code ignored;
        fs::remove(temporary, ignored);
    }
}

export namespace epochengine::scene::persistence
{
    namespace fs = std::filesystem;

    inline constexpr std::uint64_t kDefaultMaximumSceneBytes = 64u * 1024u * 1024u;
    inline constexpr std::size_t kDefaultMaximumPathCodeUnits = 32'768u;
    inline constexpr std::uint32_t kTemporaryPathAttempts = 32u;

    enum class ScenePersistenceStatus : std::uint8_t
    {
        planned,
        saved,
        loaded,
        invalid_path,
        invalid_limits,
        invalid_scene,
        serialized_payload_too_large,
        serialization_round_trip_failed,
        source_missing,
        source_not_regular,
        source_size_unavailable,
        source_payload_too_large,
        source_open_failed,
        source_read_failed,
        source_parse_failed,
        parent_missing,
        parent_not_directory,
        destination_not_regular,
        temporary_path_invalid,
        temporary_path_occupied,
        temporary_path_unavailable,
        temporary_open_failed,
        temporary_write_failed,
        temporary_flush_failed,
        temporary_verification_failed,
        atomic_replace_failed,
        filesystem_failure,
        unexpected_failure
    };

    [[nodiscard]] constexpr std::string_view scene_persistence_status_name(
        ScenePersistenceStatus status) noexcept
    {
        switch (status)
        {
        case ScenePersistenceStatus::planned: return "planned";
        case ScenePersistenceStatus::saved: return "saved";
        case ScenePersistenceStatus::loaded: return "loaded";
        case ScenePersistenceStatus::invalid_path: return "invalid-path";
        case ScenePersistenceStatus::invalid_limits: return "invalid-limits";
        case ScenePersistenceStatus::invalid_scene: return "invalid-scene";
        case ScenePersistenceStatus::serialized_payload_too_large: return "serialized-payload-too-large";
        case ScenePersistenceStatus::serialization_round_trip_failed: return "serialization-round-trip-failed";
        case ScenePersistenceStatus::source_missing: return "source-missing";
        case ScenePersistenceStatus::source_not_regular: return "source-not-regular";
        case ScenePersistenceStatus::source_size_unavailable: return "source-size-unavailable";
        case ScenePersistenceStatus::source_payload_too_large: return "source-payload-too-large";
        case ScenePersistenceStatus::source_open_failed: return "source-open-failed";
        case ScenePersistenceStatus::source_read_failed: return "source-read-failed";
        case ScenePersistenceStatus::source_parse_failed: return "source-parse-failed";
        case ScenePersistenceStatus::parent_missing: return "parent-missing";
        case ScenePersistenceStatus::parent_not_directory: return "parent-not-directory";
        case ScenePersistenceStatus::destination_not_regular: return "destination-not-regular";
        case ScenePersistenceStatus::temporary_path_invalid: return "temporary-path-invalid";
        case ScenePersistenceStatus::temporary_path_occupied: return "temporary-path-occupied";
        case ScenePersistenceStatus::temporary_path_unavailable: return "temporary-path-unavailable";
        case ScenePersistenceStatus::temporary_open_failed: return "temporary-open-failed";
        case ScenePersistenceStatus::temporary_write_failed: return "temporary-write-failed";
        case ScenePersistenceStatus::temporary_flush_failed: return "temporary-flush-failed";
        case ScenePersistenceStatus::temporary_verification_failed: return "temporary-verification-failed";
        case ScenePersistenceStatus::atomic_replace_failed: return "atomic-replace-failed";
        case ScenePersistenceStatus::filesystem_failure: return "filesystem-failure";
        case ScenePersistenceStatus::unexpected_failure: return "unexpected-failure";
        }
        return "unknown";
    }

    struct ScenePersistenceOptions final
    {
        SceneDocumentLimits document_limits{};
        SceneDocumentRequirement requirement{ SceneDocumentRequirement::authoring };
        std::uint64_t maximum_serialized_bytes{ kDefaultMaximumSceneBytes };
        std::size_t maximum_path_code_units{ kDefaultMaximumPathCodeUnits };
    };

    struct ScenePersistenceEvidence final
    {
        fs::path path{};
        fs::path temporary_path{};
        std::uint64_t revision{};
        std::uint64_t bytes{};
        bool destination_existed{};
        bool destination_preserved{};
        bool document_validated{};
        bool serialization_round_trip_verified{};
        bool temporary_is_same_directory{};
        bool temporary_bytes_verified{};
        bool atomic_replace_committed{};
    };

    struct ScenePersistenceResult final
    {
        ScenePersistenceStatus status{ ScenePersistenceStatus::unexpected_failure };
        ScenePersistenceEvidence evidence{};
        std::string error{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == ScenePersistenceStatus::planned
                || status == ScenePersistenceStatus::saved
                || status == ScenePersistenceStatus::loaded;
        }
    };

    struct SceneSavePlan final
    {
        ScenePersistenceResult result{};
        SceneSnapshot snapshot{};
        std::string serialized_text{};
    };

    struct SceneLoadResult final
    {
        ScenePersistenceResult result{};
        SceneSnapshot snapshot{};
    };

    [[nodiscard]] inline bool valid_scene_persistence_options(
        const ScenePersistenceOptions& options) noexcept
    {
        const SceneDocumentLimits& limits = options.document_limits;
        return options.maximum_serialized_bytes > 0u
            && options.maximum_serialized_bytes
                <= static_cast<std::uint64_t>((std::numeric_limits<std::streamsize>::max)())
            && options.maximum_path_code_units > 0u
            && limits.maximum_objects > 0u
            && limits.maximum_timeline_keys > 0u
            && limits.maximum_packages > 0u
            && limits.maximum_string_bytes > 0u;
    }

    [[nodiscard]] inline bool valid_scene_persistence_path(
        const fs::path& path,
        const ScenePersistenceOptions& options) noexcept
    {
        return !path.empty()
            && path.has_filename()
            && path.native().size() <= options.maximum_path_code_units;
    }

    [[nodiscard]] inline bool scene_snapshot_semantically_equal(
        const SceneSnapshot& lhs,
        const SceneSnapshot& rhs) noexcept
    {
        return detail::semantically_equal(lhs, rhs);
    }

    [[nodiscard]] inline SceneLoadResult parse_scene_snapshot_payload(
        const fs::path& source,
        std::string_view payload,
        const ScenePersistenceOptions& options = {})
    {
        SceneLoadResult load{};
        load.result.evidence.path = source;
        load.result.evidence.bytes = static_cast<std::uint64_t>(payload.size());

        if (!valid_scene_persistence_options(options))
        {
            load.result.status = ScenePersistenceStatus::invalid_limits;
            load.result.error = "scene persistence limits are invalid";
            return load;
        }
        if (!valid_scene_persistence_path(source, options))
        {
            load.result.status = ScenePersistenceStatus::invalid_path;
            load.result.error = "scene source path is empty, lacks a filename, or exceeds the path bound";
            return load;
        }
        if (payload.size() > options.maximum_serialized_bytes)
        {
            load.result.status = ScenePersistenceStatus::source_payload_too_large;
            load.result.error = "scene payload exceeds the configured byte bound";
            return load;
        }

        try
        {
            SnapshotParseResult parsed = parse_snapshot_text(payload);
            if (!parsed.ok)
                parsed = parse_legacy_editor_scene_text(payload);
            if (!parsed.ok)
            {
                load.result.status = ScenePersistenceStatus::source_parse_failed;
                load.result.error = std::move(parsed.error);
                return load;
            }

            normalize_scene_document(parsed.snapshot);
            const SceneDocumentValidation validation = validate_scene_document(
                parsed.snapshot,
                options.requirement,
                options.document_limits);
            if (!static_cast<bool>(validation))
            {
                load.result.status = ScenePersistenceStatus::invalid_scene;
                load.result.error = detail::validation_error(validation);
                return load;
            }

            load.snapshot = std::move(parsed.snapshot);
            load.result.evidence.revision = load.snapshot.revision;
            load.result.evidence.document_validated = true;
            load.result.status = ScenePersistenceStatus::loaded;
            return load;
        }
        catch (const std::exception& exception)
        {
            load.result.status = ScenePersistenceStatus::unexpected_failure;
            load.result.error = exception.what();
            return load;
        }
        catch (...)
        {
            load.result.status = ScenePersistenceStatus::unexpected_failure;
            load.result.error = "unexpected exception while parsing the scene payload";
            return load;
        }
    }

    [[nodiscard]] inline SceneSavePlan plan_scene_snapshot_save(
        const fs::path& destination,
        const SceneSnapshot& source,
        const ScenePersistenceOptions& options = {})
    {
        SceneSavePlan plan{};
        plan.result.evidence.path = destination;

        if (!valid_scene_persistence_options(options))
        {
            plan.result.status = ScenePersistenceStatus::invalid_limits;
            plan.result.error = "scene persistence limits are invalid";
            return plan;
        }
        if (!valid_scene_persistence_path(destination, options))
        {
            plan.result.status = ScenePersistenceStatus::invalid_path;
            plan.result.error = "scene destination path is empty, lacks a filename, or exceeds the path bound";
            return plan;
        }

        try
        {
            plan.snapshot = source;
            normalize_scene_document(plan.snapshot);
            const SceneDocumentValidation validation = validate_scene_document(
                plan.snapshot,
                options.requirement,
                options.document_limits);
            if (!static_cast<bool>(validation))
            {
                plan.result.status = ScenePersistenceStatus::invalid_scene;
                plan.result.error = detail::validation_error(validation);
                return plan;
            }
            plan.result.evidence.document_validated = true;
            plan.result.evidence.revision = plan.snapshot.revision;

            plan.serialized_text = serialize_snapshot_text(plan.snapshot);
            plan.result.evidence.bytes = static_cast<std::uint64_t>(plan.serialized_text.size());
            if (plan.serialized_text.size() > options.maximum_serialized_bytes)
            {
                plan.result.status = ScenePersistenceStatus::serialized_payload_too_large;
                plan.result.error = "serialized scene exceeds the configured byte bound";
                plan.serialized_text.clear();
                return plan;
            }

            SceneLoadResult round_trip = parse_scene_snapshot_payload(
                destination,
                plan.serialized_text,
                options);
            if (!round_trip.result || !scene_snapshot_semantically_equal(plan.snapshot, round_trip.snapshot))
            {
                plan.result.status = ScenePersistenceStatus::serialization_round_trip_failed;
                plan.result.error = round_trip.result
                    ? "scene serialization changed canonical snapshot meaning"
                    : "scene serialization could not be parsed and validated: " + round_trip.result.error;
                return plan;
            }

            plan.result.evidence.serialization_round_trip_verified = true;
            plan.result.status = ScenePersistenceStatus::planned;
            return plan;
        }
        catch (const std::exception& exception)
        {
            plan.result.status = ScenePersistenceStatus::unexpected_failure;
            plan.result.error = exception.what();
            return plan;
        }
        catch (...)
        {
            plan.result.status = ScenePersistenceStatus::unexpected_failure;
            plan.result.error = "unexpected exception while planning the scene save";
            return plan;
        }
    }

    [[nodiscard]] inline SceneLoadResult load_scene_snapshot(
        const fs::path& source,
        const ScenePersistenceOptions& options = {})
    {
        SceneLoadResult load{};
        load.result.evidence.path = source;

        if (!valid_scene_persistence_options(options))
        {
            load.result.status = ScenePersistenceStatus::invalid_limits;
            load.result.error = "scene persistence limits are invalid";
            return load;
        }
        if (!valid_scene_persistence_path(source, options))
        {
            load.result.status = ScenePersistenceStatus::invalid_path;
            load.result.error = "scene source path is empty, lacks a filename, or exceeds the path bound";
            return load;
        }

        try
        {
            std::error_code error;
            const fs::file_status status = fs::symlink_status(source, error);
            if (error)
            {
                load.result.status = ScenePersistenceStatus::filesystem_failure;
                load.result.error = error.message();
                return load;
            }
            if (!fs::exists(status))
            {
                load.result.status = ScenePersistenceStatus::source_missing;
                load.result.error = "scene source does not exist";
                return load;
            }
            if (!fs::is_regular_file(status))
            {
                load.result.status = ScenePersistenceStatus::source_not_regular;
                load.result.error = "scene source is not a regular file";
                return load;
            }

            const std::uintmax_t file_bytes = fs::file_size(source, error);
            if (error)
            {
                load.result.status = ScenePersistenceStatus::source_size_unavailable;
                load.result.error = error.message();
                return load;
            }
            load.result.evidence.bytes = static_cast<std::uint64_t>(file_bytes);
            if (file_bytes > options.maximum_serialized_bytes
                || file_bytes > static_cast<std::uintmax_t>((std::numeric_limits<std::size_t>::max)()))
            {
                load.result.status = ScenePersistenceStatus::source_payload_too_large;
                load.result.error = "scene source exceeds the configured byte bound";
                return load;
            }

            std::ifstream input(source, std::ios::binary);
            if (!input.is_open())
            {
                load.result.status = ScenePersistenceStatus::source_open_failed;
                load.result.error = "scene source could not be opened";
                return load;
            }

            std::string payload(static_cast<std::size_t>(file_bytes), '\0');
            if (!payload.empty())
            {
                input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
                if (input.gcount() != static_cast<std::streamsize>(payload.size()) || input.bad())
                {
                    load.result.status = ScenePersistenceStatus::source_read_failed;
                    load.result.error = "scene source could not be read completely";
                    return load;
                }
            }

            load = parse_scene_snapshot_payload(source, payload, options);
            return load;
        }
        catch (const std::exception& exception)
        {
            load.result.status = ScenePersistenceStatus::unexpected_failure;
            load.result.error = exception.what();
            return load;
        }
        catch (...)
        {
            load.result.status = ScenePersistenceStatus::unexpected_failure;
            load.result.error = "unexpected exception while loading the scene";
            return load;
        }
    }

    [[nodiscard]] inline ScenePersistenceResult save_scene_snapshot_atomic(
        const fs::path& destination,
        const SceneSnapshot& snapshot,
        const ScenePersistenceOptions& options = {},
        const fs::path& supplied_temporary_path = {})
    {
        SceneSavePlan plan = plan_scene_snapshot_save(destination, snapshot, options);
        ScenePersistenceResult result = plan.result;
        if (!plan.result)
            return result;

        fs::path temporary;
        bool owns_temporary = false;
        try
        {
            std::error_code error;
            const fs::path parent = detail::normalized_parent(destination);
            const fs::file_status parent_status = fs::status(parent, error);
            if (error || !fs::exists(parent_status))
            {
                result.status = ScenePersistenceStatus::parent_missing;
                result.error = error ? error.message() : "scene destination parent does not exist";
                return result;
            }
            if (!fs::is_directory(parent_status))
            {
                result.status = ScenePersistenceStatus::parent_not_directory;
                result.error = "scene destination parent is not a directory";
                return result;
            }

            const fs::file_status destination_status = fs::symlink_status(destination, error);
            if (error && error != std::errc::no_such_file_or_directory)
            {
                result.status = ScenePersistenceStatus::filesystem_failure;
                result.error = error.message();
                return result;
            }
            error.clear();
            result.evidence.destination_existed = fs::exists(destination_status);
            result.evidence.destination_preserved = result.evidence.destination_existed;
            if (result.evidence.destination_existed && !fs::is_regular_file(destination_status))
            {
                result.status = ScenePersistenceStatus::destination_not_regular;
                result.error = "scene destination exists but is not a regular file";
                return result;
            }

            if (!supplied_temporary_path.empty())
            {
                temporary = supplied_temporary_path;
                if (!valid_scene_persistence_path(temporary, options)
                    || temporary == destination
                    || !detail::same_directory(destination, temporary))
                {
                    result.status = ScenePersistenceStatus::temporary_path_invalid;
                    result.error = "supplied temporary path must be a distinct same-directory file path";
                    return result;
                }

                const fs::file_status temporary_status = fs::symlink_status(temporary, error);
                if (!error && fs::exists(temporary_status))
                {
                    result.status = ScenePersistenceStatus::temporary_path_occupied;
                    result.error = "supplied temporary path already exists";
                    return result;
                }
                if (error && error != std::errc::no_such_file_or_directory)
                {
                    result.status = ScenePersistenceStatus::filesystem_failure;
                    result.error = error.message();
                    return result;
                }
                error.clear();
            }
            else
            {
                const std::uint64_t base_nonce = detail::next_temporary_nonce();
                for (std::uint32_t attempt = 0u; attempt < kTemporaryPathAttempts; ++attempt)
                {
                    fs::path candidate = detail::generated_temporary_path(destination, base_nonce + attempt);
                    const fs::file_status candidate_status = fs::symlink_status(candidate, error);
                    if (error == std::errc::no_such_file_or_directory || (!error && !fs::exists(candidate_status)))
                    {
                        temporary = std::move(candidate);
                        error.clear();
                        break;
                    }
                    if (error)
                    {
                        result.status = ScenePersistenceStatus::filesystem_failure;
                        result.error = error.message();
                        return result;
                    }
                }
                if (temporary.empty())
                {
                    result.status = ScenePersistenceStatus::temporary_path_unavailable;
                    result.error = "no unused same-directory temporary path was available";
                    return result;
                }
            }

            owns_temporary = true;
            result.evidence.temporary_path = temporary;
            result.evidence.temporary_is_same_directory = detail::same_directory(destination, temporary);

            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                result.status = ScenePersistenceStatus::temporary_open_failed;
                result.error = "scene temporary file could not be opened";
                detail::remove_owned_temporary(temporary);
                return result;
            }

            if (!plan.serialized_text.empty())
            {
                output.write(
                    plan.serialized_text.data(),
                    static_cast<std::streamsize>(plan.serialized_text.size()));
            }
            if (!output.good())
            {
                output.close();
                result.status = ScenePersistenceStatus::temporary_write_failed;
                result.error = "scene temporary file could not be written completely";
                detail::remove_owned_temporary(temporary);
                return result;
            }

            output.flush();
            if (!output.good())
            {
                output.close();
                result.status = ScenePersistenceStatus::temporary_flush_failed;
                result.error = "scene temporary file could not be flushed";
                detail::remove_owned_temporary(temporary);
                return result;
            }
            output.close();
            if (output.fail())
            {
                result.status = ScenePersistenceStatus::temporary_flush_failed;
                result.error = "scene temporary file could not be closed cleanly";
                detail::remove_owned_temporary(temporary);
                return result;
            }

            const std::uintmax_t temporary_bytes = fs::file_size(temporary, error);
            if (error || temporary_bytes != plan.serialized_text.size())
            {
                result.status = ScenePersistenceStatus::temporary_verification_failed;
                result.error = error ? error.message() : "scene temporary file size does not match the save plan";
                detail::remove_owned_temporary(temporary);
                return result;
            }

            std::ifstream verification(temporary, std::ios::binary);
            std::string verified(plan.serialized_text.size(), '\0');
            if (!verification.is_open())
            {
                result.status = ScenePersistenceStatus::temporary_verification_failed;
                result.error = "scene temporary file could not be reopened for verification";
                detail::remove_owned_temporary(temporary);
                return result;
            }
            if (!verified.empty())
                verification.read(verified.data(), static_cast<std::streamsize>(verified.size()));
            const bool verified_bytes =
                verification.gcount() == static_cast<std::streamsize>(verified.size())
                && !verification.bad()
                && verified == plan.serialized_text;
            verification.close();
            if (!verified_bytes
                || verification.bad()
                || verification.fail())
            {
                result.status = ScenePersistenceStatus::temporary_verification_failed;
                result.error = "scene temporary bytes do not match the validated save plan";
                detail::remove_owned_temporary(temporary);
                return result;
            }
            result.evidence.temporary_bytes_verified = true;

            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary,
                    destination,
                    error))
            {
                result.status = ScenePersistenceStatus::atomic_replace_failed;
                result.error = error.message();
                detail::remove_owned_temporary(temporary);
                return result;
            }

            owns_temporary = false;
            result.status = ScenePersistenceStatus::saved;
            result.error.clear();
            result.evidence.atomic_replace_committed = true;
            result.evidence.destination_preserved = false;
            return result;
        }
        catch (const fs::filesystem_error& exception)
        {
            if (owns_temporary)
                detail::remove_owned_temporary(temporary);
            result.status = ScenePersistenceStatus::filesystem_failure;
            result.error = exception.what();
            return result;
        }
        catch (const std::exception& exception)
        {
            if (owns_temporary)
                detail::remove_owned_temporary(temporary);
            result.status = ScenePersistenceStatus::unexpected_failure;
            result.error = exception.what();
            return result;
        }
        catch (...)
        {
            if (owns_temporary)
                detail::remove_owned_temporary(temporary);
            result.status = ScenePersistenceStatus::unexpected_failure;
            result.error = "unexpected exception while saving the scene";
            return result;
        }
    }

    struct ScenePersistenceContractReport final
    {
        bool valid_plan{};
        bool invalid_scene_rejected{};
        bool oversized_payload_rejected{};
        bool same_directory_temp_accepted{};
        bool foreign_directory_temp_rejected{};
        bool semantic_round_trip_preserved{};
        bool extended_metadata_preserved_or_rejected{};

        [[nodiscard]] constexpr bool all_passed() const noexcept
        {
            return valid_plan
                && invalid_scene_rejected
                && oversized_payload_rejected
                && same_directory_temp_accepted
                && foreign_directory_temp_rejected
                && semantic_round_trip_preserved
                && extended_metadata_preserved_or_rejected;
        }
    };

    [[nodiscard]] inline ScenePersistenceContractReport scene_persistence_contract_checks()
    {
        SceneSnapshot snapshot{};
        snapshot.scene_id = "contract-scene";
        snapshot.world_name = "Contract World";
        snapshot.revision = 1u;

        SceneObjectSnapshot camera{};
        camera.id = stable_scene_object_id(snapshot.scene_id, "Camera");
        camera.name = "Camera";
        camera.type = "Camera";
        camera.category = "Editor";
        camera.position = { 0.0F, 4.0F, 8.0F };

        SceneObjectSnapshot spawn{};
        spawn.id = stable_scene_object_id(snapshot.scene_id, "Spawn");
        spawn.name = "Spawn";
        spawn.type = "Spawn";
        spawn.category = "Gameplay";

        snapshot.primary_camera = camera.id;
        snapshot.primary_spawn = spawn.id;
        snapshot.objects.push_back(camera);
        snapshot.objects.push_back(spawn);

        const fs::path destination = fs::path("contract") / "scene.epoch";
        const fs::path same_directory_temp = fs::path("contract") / "scene.epoch.tmp.test";
        const fs::path foreign_directory_temp = fs::path("other") / "scene.epoch.tmp.test";

        const SceneSavePlan plan = plan_scene_snapshot_save(destination, snapshot);
        const SceneLoadResult parsed = plan.result
            ? parse_scene_snapshot_payload(destination, plan.serialized_text)
            : SceneLoadResult{};

        SceneSnapshot invalid = snapshot;
        invalid.objects[1].id = invalid.objects[0].id;
        const SceneSavePlan invalid_plan = plan_scene_snapshot_save(destination, invalid);

        ScenePersistenceOptions tiny{};
        tiny.maximum_serialized_bytes = 8u;
        const SceneSavePlan oversized = plan_scene_snapshot_save(destination, snapshot, tiny);

        SceneSnapshot extended = snapshot;
        extended.project_id = "contract-project";
        extended.revision = 7u;
        const SceneSavePlan extended_plan = plan_scene_snapshot_save(destination, extended);

        ScenePersistenceContractReport report{};
        report.valid_plan = static_cast<bool>(plan.result)
            && plan.result.evidence.document_validated
            && plan.result.evidence.serialization_round_trip_verified
            && plan.result.evidence.revision == snapshot.revision;
        report.invalid_scene_rejected = invalid_plan.result.status == ScenePersistenceStatus::invalid_scene;
        report.oversized_payload_rejected =
            oversized.result.status == ScenePersistenceStatus::serialized_payload_too_large;
        report.same_directory_temp_accepted = detail::same_directory(destination, same_directory_temp)
            && same_directory_temp != destination;
        report.foreign_directory_temp_rejected = !detail::same_directory(destination, foreign_directory_temp);
        report.semantic_round_trip_preserved = parsed.result
            && scene_snapshot_semantically_equal(plan.snapshot, parsed.snapshot);
        report.extended_metadata_preserved_or_rejected =
            (extended_plan.result
                && extended_plan.result.evidence.revision == extended.revision
                && extended_plan.snapshot.project_id == extended.project_id)
            || extended_plan.result.status == ScenePersistenceStatus::serialization_round_trip_failed;
        return report;
    }
}
