/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module project.asset.registry;

export namespace epochengine::project_assets
{
    struct AssetTag final {};

    struct AssetHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        friend constexpr auto operator<=>(AssetHandle, AssetHandle) noexcept = default;
    };

    [[nodiscard]] constexpr std::uint32_t next_generation(
        std::uint32_t generation) noexcept
    {
        ++generation;
        return generation == 0 ? 1u : generation;
    }

    enum class AssetKind : std::uint8_t
    {
        invalid,
        texture,
        tileset,
        sprite_animation,
        scene,
        audio,
        model,
        material,
        script,
        binary
    };

    [[nodiscard]] constexpr bool valid(AssetKind kind) noexcept
    {
        return kind > AssetKind::invalid && kind <= AssetKind::binary;
    }

    [[nodiscard]] constexpr std::string_view asset_kind_name(
        AssetKind kind) noexcept
    {
        switch (kind)
        {
        case AssetKind::texture: return "texture";
        case AssetKind::tileset: return "tileset";
        case AssetKind::sprite_animation: return "sprite_animation";
        case AssetKind::scene: return "scene";
        case AssetKind::audio: return "audio";
        case AssetKind::model: return "model";
        case AssetKind::material: return "material";
        case AssetKind::script: return "script";
        case AssetKind::binary: return "binary";
        case AssetKind::invalid: break;
        }
        return "invalid";
    }

    struct AssetContentHash final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        friend constexpr bool operator==(
            const AssetContentHash& left,
            const AssetContentHash& right) noexcept
        {
            return left.words == right.words;
        }
    };

    struct AssetRevision final
    {
        AssetContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !content.empty() && sequence != 0;
        }

        friend constexpr bool operator==(
            const AssetRevision& left,
            const AssetRevision& right) noexcept
        {
            return left.content.words == right.content.words
                && left.sequence == right.sequence;
        }
    };

    struct AssetIdentity final
    {
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        AssetKind kind{AssetKind::invalid};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return project_key != 0 && asset_key != 0 && valid(kind);
        }

        friend constexpr bool operator==(
            const AssetIdentity&,
            const AssetIdentity&) noexcept = default;
    };

    struct AssetRecord final
    {
        AssetHandle handle{};
        AssetIdentity identity{};
        AssetRevision revision{};
        std::string canonical_path{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return handle && identity && revision
                && !canonical_path.empty();
        }
    };

    struct RegistryLimits final
    {
        std::uint32_t maximum_assets{65'536};
        std::uint32_t maximum_path_bytes{1'024};
        std::uint32_t maximum_project_id_bytes{256};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_assets != 0 && maximum_path_bytes != 0
                && maximum_project_id_bytes != 0;
        }
    };

    enum class RegistryCode : std::uint8_t
    {
        ready,
        already_registered,
        unchanged,
        invalid_registry,
        invalid_project,
        invalid_kind,
        invalid_path,
        invalid_revision,
        path_collision,
        identity_collision,
        revision_conflict,
        stale_revision,
        capacity_exceeded,
        allocation_failure,
        invalid_handle,
        stale_handle,
        not_found
    };

    [[nodiscard]] constexpr std::string_view registry_code_name(
        RegistryCode code) noexcept
    {
        switch (code)
        {
        case RegistryCode::ready: return "ready";
        case RegistryCode::already_registered: return "already_registered";
        case RegistryCode::unchanged: return "unchanged";
        case RegistryCode::invalid_registry: return "invalid_registry";
        case RegistryCode::invalid_project: return "invalid_project";
        case RegistryCode::invalid_kind: return "invalid_kind";
        case RegistryCode::invalid_path: return "invalid_path";
        case RegistryCode::invalid_revision: return "invalid_revision";
        case RegistryCode::path_collision: return "path_collision";
        case RegistryCode::identity_collision: return "identity_collision";
        case RegistryCode::revision_conflict: return "revision_conflict";
        case RegistryCode::stale_revision: return "stale_revision";
        case RegistryCode::capacity_exceeded: return "capacity_exceeded";
        case RegistryCode::allocation_failure: return "allocation_failure";
        case RegistryCode::invalid_handle: return "invalid_handle";
        case RegistryCode::stale_handle: return "stale_handle";
        case RegistryCode::not_found: return "not_found";
        }
        return "unknown";
    }

    struct AssetDeclaration final
    {
        std::string_view logical_path{};
        AssetKind kind{AssetKind::invalid};
        AssetRevision revision{};
    };

    struct RegistrationResult final
    {
        RegistryCode code{RegistryCode::invalid_registry};
        AssetHandle handle{};
        std::uint64_t asset_key{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return (code == RegistryCode::ready
                    || code == RegistryCode::already_registered)
                && handle && asset_key != 0;
        }
    };

    struct RegistryMetrics final
    {
        std::uint32_t active_assets{};
        std::uint32_t retired_slots{};
        std::uint64_t registrations{};
        std::uint64_t idempotent_registrations{};
        std::uint64_t revision_updates{};
        std::uint64_t retirements{};
        std::uint64_t rejected_operations{};
        std::uint64_t path_bytes{};
    };

    namespace detail
    {
        [[nodiscard]] constexpr bool ascii_control(unsigned char value) noexcept
        {
            return value < 0x20u || value == 0x7fu;
        }

        [[nodiscard]] constexpr char ascii_fold(char value) noexcept
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        }

        [[nodiscard]] inline bool portable_path_equal(
            std::string_view left,
            std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                if (ascii_fold(left[index]) != ascii_fold(right[index]))
                    return false;
            }
            return true;
        }

        [[nodiscard]] inline std::optional<std::string> canonical_path(
            std::string_view source,
            std::uint32_t maximumBytes)
        {
            if (source.empty() || source.size() > maximumBytes)
                return std::nullopt;
            if (source.front() == '/' || source.front() == '\\')
                return std::nullopt;

            std::string result{};
            result.reserve(source.size());
            std::string segment{};
            segment.reserve(source.size());
            const auto flush = [&]() -> bool
            {
                if (segment.empty())
                    return true;
                if (segment == "." || segment == "..")
                    return false;
                if (!result.empty())
                    result.push_back('/');
                result.append(segment);
                segment.clear();
                return true;
            };

            for (char character : source)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (ascii_control(byte) || character == ':' || character == '*'
                    || character == '?' || character == '"' || character == '<'
                    || character == '>' || character == '|')
                {
                    return std::nullopt;
                }
                if (character == '/' || character == '\\')
                {
                    if (!flush())
                        return std::nullopt;
                    continue;
                }
                segment.push_back(character);
            }
            if (!flush() || result.empty() || result.size() > maximumBytes)
                return std::nullopt;
            return result;
        }

        [[nodiscard]] constexpr std::uint64_t hash_append(
            std::uint64_t hash,
            std::string_view value) noexcept
        {
            for (char character : value)
            {
                hash ^= static_cast<unsigned char>(character);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        [[nodiscard]] constexpr std::uint64_t stable_project_key(
            std::string_view projectId) noexcept
        {
            std::uint64_t hash = hash_append(
                14695981039346656037ull,
                "epoch.project.asset-registry.v1");
            hash = hash_append(hash, projectId);
            return hash == 0 ? 1u : hash;
        }

        [[nodiscard]] constexpr std::uint64_t stable_asset_key(
            std::uint64_t projectKey,
            AssetKind kind,
            std::string_view path) noexcept
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (std::uint32_t byte = 0; byte < 8; ++byte)
            {
                hash ^= static_cast<std::uint8_t>(projectKey >> (byte * 8u));
                hash *= 1099511628211ull;
            }
            hash ^= static_cast<std::uint8_t>(kind);
            hash *= 1099511628211ull;
            hash = hash_append(hash, path);
            return hash == 0 ? 1u : hash;
        }

        [[nodiscard]] inline bool valid_project_id(
            std::string_view projectId,
            const RegistryLimits& limits) noexcept
        {
            if (projectId.empty()
                || projectId.size() > limits.maximum_project_id_bytes)
            {
                return false;
            }
            for (char character : projectId)
            {
                if (ascii_control(static_cast<unsigned char>(character)))
                    return false;
            }
            return true;
        }
    }

    class AssetRegistry final
    {
    public:
        explicit AssetRegistry(
            std::string projectId,
            RegistryLimits limits = {}) noexcept
            : project_id_(std::move(projectId)), limits_(limits)
        {
            if (limits_.valid()
                && detail::valid_project_id(project_id_, limits_))
            {
                project_key_ = detail::stable_project_key(project_id_);
            }
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return project_key_ != 0 && limits_.valid();
        }

        [[nodiscard]] std::string_view project_id() const noexcept
        {
            return project_id_;
        }

        [[nodiscard]] std::uint64_t project_key() const noexcept
        {
            return project_key_;
        }

        [[nodiscard]] const RegistryLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] RegistryMetrics metrics() const noexcept
        {
            RegistryMetrics result = metrics_;
            result.active_assets = active_assets_;
            result.retired_slots = static_cast<std::uint32_t>(free_slots_.size());
            return result;
        }

        [[nodiscard]] RegistrationResult register_asset(
            const AssetDeclaration& declaration) noexcept
        {
            if (!valid())
                return reject(RegistryCode::invalid_registry);
            if (!project_key_)
                return reject(RegistryCode::invalid_project);
            if (!project_assets::valid(declaration.kind))
                return reject(RegistryCode::invalid_kind);
            if (!declaration.revision)
                return reject(RegistryCode::invalid_revision);

            std::optional<std::string> canonical{};
            try
            {
                canonical = detail::canonical_path(
                    declaration.logical_path,
                    limits_.maximum_path_bytes);
            }
            catch (...)
            {
                return reject(RegistryCode::allocation_failure);
            }
            if (!canonical)
                return reject(RegistryCode::invalid_path);

            const std::uint64_t assetKey = detail::stable_asset_key(
                project_key_, declaration.kind, *canonical);
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& slot = slots_[index];
                if (!slot.active)
                    continue;
                if (detail::portable_path_equal(
                    slot.record.canonical_path, *canonical))
                {
                    if (slot.record.canonical_path != *canonical
                        || slot.record.identity.kind != declaration.kind)
                    {
                        return reject(RegistryCode::path_collision);
                    }
                    if (slot.record.revision != declaration.revision)
                        return reject(RegistryCode::revision_conflict);
                    ++metrics_.idempotent_registrations;
                    return {
                        RegistryCode::already_registered,
                        slot.record.handle,
                        slot.record.identity.asset_key};
                }
                if (slot.record.identity.asset_key == assetKey)
                    return reject(RegistryCode::identity_collision);
            }
            if (active_assets_ >= limits_.maximum_assets)
                return reject(RegistryCode::capacity_exceeded);

            try
            {
                std::uint32_t index{};
                if (!free_slots_.empty())
                {
                    index = free_slots_.back();
                    free_slots_.pop_back();
                }
                else
                {
                    if (slots_.size() >= AssetHandle::invalid_index)
                        return reject(RegistryCode::capacity_exceeded);
                    index = static_cast<std::uint32_t>(slots_.size());
                    slots_.push_back({});
                }

                Slot& slot = slots_[index];
                if (slot.generation == 0)
                    slot.generation = 1;
                slot.active = true;
                slot.record = AssetRecord{
                    AssetHandle{index, slot.generation},
                    AssetIdentity{project_key_, assetKey, declaration.kind},
                    declaration.revision,
                    std::move(*canonical)};
                ++active_assets_;
                ++metrics_.registrations;
                metrics_.path_bytes += slot.record.canonical_path.size();
                return {RegistryCode::ready, slot.record.handle, assetKey};
            }
            catch (...)
            {
                return reject(RegistryCode::allocation_failure);
            }
        }

        [[nodiscard]] RegistryCode update_revision(
            AssetHandle handle,
            AssetRevision revision) noexcept
        {
            Slot* const slot = resolve_slot(handle);
            if (!slot)
                return reject_code(handle ? RegistryCode::stale_handle
                                          : RegistryCode::invalid_handle);
            if (!revision)
                return reject_code(RegistryCode::invalid_revision);
            if (revision == slot->record.revision)
                return RegistryCode::unchanged;
            if (revision.sequence <= slot->record.revision.sequence)
                return reject_code(RegistryCode::stale_revision);
            slot->record.revision = revision;
            ++metrics_.revision_updates;
            return RegistryCode::ready;
        }

        [[nodiscard]] RegistryCode retire(AssetHandle handle) noexcept
        {
            Slot* const slot = resolve_slot(handle);
            if (!slot)
                return reject_code(handle ? RegistryCode::stale_handle
                                          : RegistryCode::invalid_handle);
            try
            {
                free_slots_.push_back(handle.index);
            }
            catch (...)
            {
                return reject_code(RegistryCode::allocation_failure);
            }
            metrics_.path_bytes -= slot->record.canonical_path.size();
            slot->active = false;
            slot->generation = next_generation(slot->generation);
            slot->record = {};
            --active_assets_;
            ++metrics_.retirements;
            return RegistryCode::ready;
        }

        [[nodiscard]] const AssetRecord* resolve(
            AssetHandle handle) const noexcept
        {
            const Slot* const slot = resolve_slot(handle);
            return slot ? &slot->record : nullptr;
        }

        [[nodiscard]] const AssetRecord* find_by_key(
            std::uint64_t assetKey) const noexcept
        {
            if (assetKey == 0)
                return nullptr;
            for (const Slot& slot : slots_)
            {
                if (slot.active && slot.record.identity.asset_key == assetKey)
                    return &slot.record;
            }
            return nullptr;
        }

        [[nodiscard]] const AssetRecord* find_by_path(
            std::string_view logicalPath) const noexcept
        {
            std::optional<std::string> canonical{};
            try
            {
                canonical = detail::canonical_path(
                    logicalPath, limits_.maximum_path_bytes);
            }
            catch (...)
            {
                return nullptr;
            }
            if (!canonical)
                return nullptr;
            for (const Slot& slot : slots_)
            {
                if (slot.active && slot.record.canonical_path == *canonical)
                    return &slot.record;
            }
            return nullptr;
        }

    private:
        struct Slot final
        {
            std::uint32_t generation{1};
            bool active{};
            AssetRecord record{};
        };

        [[nodiscard]] Slot* resolve_slot(AssetHandle handle) noexcept
        {
            if (!handle || handle.index >= slots_.size())
                return nullptr;
            Slot& slot = slots_[handle.index];
            return slot.active && slot.generation == handle.generation
                ? &slot : nullptr;
        }

        [[nodiscard]] const Slot* resolve_slot(AssetHandle handle) const noexcept
        {
            if (!handle || handle.index >= slots_.size())
                return nullptr;
            const Slot& slot = slots_[handle.index];
            return slot.active && slot.generation == handle.generation
                ? &slot : nullptr;
        }

        [[nodiscard]] RegistrationResult reject(RegistryCode code) noexcept
        {
            ++metrics_.rejected_operations;
            return {code, {}, 0};
        }

        [[nodiscard]] RegistryCode reject_code(RegistryCode code) noexcept
        {
            ++metrics_.rejected_operations;
            return code;
        }

        std::string project_id_{};
        RegistryLimits limits_{};
        std::uint64_t project_key_{};
        std::vector<Slot> slots_{};
        std::vector<std::uint32_t> free_slots_{};
        RegistryMetrics metrics_{};
        std::uint32_t active_assets_{};
    };

    enum class RegistryContractFailure : std::uint8_t
    {
        none,
        invalid_project,
        traversal_rejection,
        registration,
        path_normalization,
        duplicate_identity,
        case_collision,
        revision_update,
        stale_revision,
        retirement,
        stale_handle,
        stable_reregistration,
        metrics
    };

    [[nodiscard]] constexpr std::string_view registry_contract_failure_name(
        RegistryContractFailure failure) noexcept
    {
        switch (failure)
        {
        case RegistryContractFailure::none: return "pass";
        case RegistryContractFailure::invalid_project: return "invalid_project";
        case RegistryContractFailure::traversal_rejection: return "traversal_rejection";
        case RegistryContractFailure::registration: return "registration";
        case RegistryContractFailure::path_normalization: return "path_normalization";
        case RegistryContractFailure::duplicate_identity: return "duplicate_identity";
        case RegistryContractFailure::case_collision: return "case_collision";
        case RegistryContractFailure::revision_update: return "revision_update";
        case RegistryContractFailure::stale_revision: return "stale_revision";
        case RegistryContractFailure::retirement: return "retirement";
        case RegistryContractFailure::stale_handle: return "stale_handle";
        case RegistryContractFailure::stable_reregistration: return "stable_reregistration";
        case RegistryContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] RegistryContractFailure
        project_asset_registry_runtime_contract_failure();
}
