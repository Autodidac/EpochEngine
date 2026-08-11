/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

export module authoring.document;

export namespace epochengine::authoring
{
    template<typename Tag, std::unsigned_integral Index = std::uint32_t>
        requires (!std::same_as<std::remove_cv_t<Index>, bool>)
    struct Handle final
    {
        using tag_type = Tag;
        using index_type = Index;
        using generation_type = std::uint32_t;

        static constexpr index_type invalid_index =
            (std::numeric_limits<index_type>::max)();

        index_type index{invalid_index};
        generation_type generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const Handle&,
            const Handle&) noexcept = default;
    };

    template<typename Tag, std::unsigned_integral Index>
        requires (!std::same_as<std::remove_cv_t<Index>, bool>)
    [[nodiscard]] constexpr Handle<Tag, Index> make_handle(
        Index index,
        std::uint32_t generation) noexcept
    {
        const Handle<Tag, Index> candidate{index, generation};
        return candidate.valid() ? candidate : Handle<Tag, Index>{};
    }

    [[nodiscard]] constexpr std::uint32_t next_generation(
        std::uint32_t generation) noexcept
    {
        ++generation;
        return generation == 0 ? 1u : generation;
    }

    struct DocumentTag final {};
    struct CheckpointTag final {};

    using DocumentHandle = Handle<DocumentTag, std::uint32_t>;
    using CheckpointHandle = Handle<CheckpointTag, std::uint32_t>;

    enum class DocumentKind : std::uint8_t
    {
        invalid,
        texture,
        material,
        model,
        animation,
        scene,
        particle_effect,
        audio_graph,
        simulation_graph,
        generic_node_graph,
        tilemap
    };

    [[nodiscard]] constexpr bool valid(DocumentKind kind) noexcept
    {
        switch (kind)
        {
        case DocumentKind::texture:
        case DocumentKind::material:
        case DocumentKind::model:
        case DocumentKind::animation:
        case DocumentKind::scene:
        case DocumentKind::particle_effect:
        case DocumentKind::audio_graph:
        case DocumentKind::simulation_graph:
        case DocumentKind::generic_node_graph:
        case DocumentKind::tilemap:
            return true;
        case DocumentKind::invalid:
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr std::string_view document_kind_name(
        DocumentKind kind) noexcept
    {
        switch (kind)
        {
        case DocumentKind::texture: return "texture";
        case DocumentKind::material: return "material";
        case DocumentKind::model: return "model";
        case DocumentKind::animation: return "animation";
        case DocumentKind::scene: return "scene";
        case DocumentKind::particle_effect: return "particle_effect";
        case DocumentKind::audio_graph: return "audio_graph";
        case DocumentKind::simulation_graph: return "simulation_graph";
        case DocumentKind::generic_node_graph: return "generic_node_graph";
        case DocumentKind::tilemap: return "tilemap";
        case DocumentKind::invalid:
        default:
            return "invalid";
        }
    }

    struct ContentHash final
    {
        std::array<std::uint64_t, 4> words{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return words == std::array<std::uint64_t, 4>{};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !empty();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const ContentHash&,
            const ContentHash&) noexcept = default;
    };

    struct DocumentRevision final
    {
        ContentHash content{};
        std::uint64_t sequence{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return static_cast<bool>(content) && sequence != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const DocumentRevision&,
            const DocumentRevision&) noexcept = default;
    };

    struct BranchIdentity final
    {
        std::uint64_t timeline{};
        std::uint64_t branch{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return timeline != 0 && branch != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const BranchIdentity&,
            const BranchIdentity&) noexcept = default;
    };

    struct TemporalPoint final
    {
        BranchIdentity stream{};
        std::int64_t tick{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return stream.valid() && tick >= 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const TemporalPoint&,
            const TemporalPoint&) noexcept = default;
    };

    struct OperationId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const OperationId&,
            const OperationId&) noexcept = default;
    };

    enum class HistoryMode : std::uint8_t
    {
        disabled,
        semantic_operations,
        checkpointed_operations,
        lossless_deltas,
        lossy_deltas,
        full_snapshot_debug
    };

    [[nodiscard]] constexpr bool valid(HistoryMode mode) noexcept
    {
        switch (mode)
        {
        case HistoryMode::disabled:
        case HistoryMode::semantic_operations:
        case HistoryMode::checkpointed_operations:
        case HistoryMode::lossless_deltas:
        case HistoryMode::lossy_deltas:
        case HistoryMode::full_snapshot_debug:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr bool history_mode_uses_checkpoints(
        HistoryMode mode) noexcept
    {
        return mode == HistoryMode::checkpointed_operations
            || mode == HistoryMode::lossless_deltas
            || mode == HistoryMode::lossy_deltas
            || mode == HistoryMode::full_snapshot_debug;
    }

    [[nodiscard]] constexpr std::string_view history_mode_name(
        HistoryMode mode) noexcept
    {
        switch (mode)
        {
        case HistoryMode::disabled: return "disabled";
        case HistoryMode::semantic_operations: return "semantic_operations";
        case HistoryMode::checkpointed_operations: return "checkpointed_operations";
        case HistoryMode::lossless_deltas: return "lossless_deltas";
        case HistoryMode::lossy_deltas: return "lossy_deltas";
        case HistoryMode::full_snapshot_debug: return "full_snapshot_debug";
        default: return "invalid";
        }
    }

    struct HistoryPolicy final
    {
        HistoryMode mode{HistoryMode::checkpointed_operations};
        std::uint32_t checkpoint_interval_operations{64};
        std::uint64_t maximum_operations{65'536};
        std::uint32_t maximum_checkpoints{1'024};
        std::uint64_t maximum_operation_bytes{8ull * 1024ull * 1024ull};
        std::uint64_t maximum_checkpoint_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_memory_bytes{512ull * 1024ull * 1024ull};
        std::uint64_t maximum_disk_bytes{8ull * 1024ull * 1024ull * 1024ull};
        bool compress{true};
        bool deduplicate{true};
        bool allow_branching{true};

        [[nodiscard]] friend constexpr auto operator<=>(
            const HistoryPolicy&,
            const HistoryPolicy&) noexcept = default;
    };

    struct HistoryPolicyBounds final
    {
        std::uint32_t maximum_checkpoint_interval_operations{1'048'576};
        std::uint64_t maximum_operations{16'777'216};
        std::uint32_t maximum_checkpoints{1'048'576};
        std::uint64_t maximum_operation_bytes{1ull * 1024ull * 1024ull * 1024ull};
        std::uint64_t maximum_checkpoint_bytes{16ull * 1024ull * 1024ull * 1024ull};
        std::uint64_t maximum_memory_bytes{64ull * 1024ull * 1024ull * 1024ull};
        std::uint64_t maximum_disk_bytes{4ull * 1024ull * 1024ull * 1024ull * 1024ull};

        [[nodiscard]] friend constexpr auto operator<=>(
            const HistoryPolicyBounds&,
            const HistoryPolicyBounds&) noexcept = default;
    };

    enum class ValidationCode : std::uint8_t
    {
        valid,
        invalid_handle,
        invalid_document_kind,
        invalid_content_hash,
        invalid_revision,
        invalid_branch,
        invalid_temporal_point,
        invalid_operation_id,
        invalid_history_mode,
        invalid_history_bounds,
        history_limit_required,
        history_limit_exceeded,
        checkpoint_policy_required,
        inconsistent_storage_budget
    };

    struct ValidationResult final
    {
        ValidationCode code{ValidationCode::valid};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ValidationCode::valid;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const ValidationResult&,
            const ValidationResult&) noexcept = default;
    };

    [[nodiscard]] constexpr std::string_view validation_code_name(
        ValidationCode code) noexcept
    {
        switch (code)
        {
        case ValidationCode::valid: return "valid";
        case ValidationCode::invalid_handle: return "invalid_handle";
        case ValidationCode::invalid_document_kind: return "invalid_document_kind";
        case ValidationCode::invalid_content_hash: return "invalid_content_hash";
        case ValidationCode::invalid_revision: return "invalid_revision";
        case ValidationCode::invalid_branch: return "invalid_branch";
        case ValidationCode::invalid_temporal_point: return "invalid_temporal_point";
        case ValidationCode::invalid_operation_id: return "invalid_operation_id";
        case ValidationCode::invalid_history_mode: return "invalid_history_mode";
        case ValidationCode::invalid_history_bounds: return "invalid_history_bounds";
        case ValidationCode::history_limit_required: return "history_limit_required";
        case ValidationCode::history_limit_exceeded: return "history_limit_exceeded";
        case ValidationCode::checkpoint_policy_required: return "checkpoint_policy_required";
        case ValidationCode::inconsistent_storage_budget: return "inconsistent_storage_budget";
        default: return "unknown";
        }
    }

    template<typename Tag, std::unsigned_integral Index>
        requires (!std::same_as<std::remove_cv_t<Index>, bool>)
    [[nodiscard]] constexpr ValidationResult validate(
        Handle<Tag, Index> handle) noexcept
    {
        return {handle.valid()
            ? ValidationCode::valid
            : ValidationCode::invalid_handle};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        DocumentKind kind) noexcept
    {
        return {valid(kind)
            ? ValidationCode::valid
            : ValidationCode::invalid_document_kind};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        const ContentHash& hash) noexcept
    {
        return {hash.empty()
            ? ValidationCode::invalid_content_hash
            : ValidationCode::valid};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        const DocumentRevision& revision) noexcept
    {
        if (revision.content.empty())
            return {ValidationCode::invalid_content_hash};
        return {revision.sequence == 0
            ? ValidationCode::invalid_revision
            : ValidationCode::valid};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        BranchIdentity branch) noexcept
    {
        return {branch.valid()
            ? ValidationCode::valid
            : ValidationCode::invalid_branch};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        TemporalPoint point) noexcept
    {
        if (!point.stream.valid())
            return {ValidationCode::invalid_branch};
        return {point.tick >= 0
            ? ValidationCode::valid
            : ValidationCode::invalid_temporal_point};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        OperationId operation) noexcept
    {
        return {operation.valid()
            ? ValidationCode::valid
            : ValidationCode::invalid_operation_id};
    }

    [[nodiscard]] constexpr ValidationResult validate(
        const HistoryPolicy& policy,
        const HistoryPolicyBounds& bounds = {}) noexcept
    {
        const bool validBounds =
            bounds.maximum_checkpoint_interval_operations != 0
            && bounds.maximum_operations != 0
            && bounds.maximum_checkpoints != 0
            && bounds.maximum_operation_bytes != 0
            && bounds.maximum_checkpoint_bytes != 0
            && bounds.maximum_memory_bytes != 0
            && bounds.maximum_disk_bytes != 0;
        if (!validBounds)
            return {ValidationCode::invalid_history_bounds};
        if (!valid(policy.mode))
            return {ValidationCode::invalid_history_mode};
        if (policy.mode == HistoryMode::disabled)
            return {};

        if (policy.maximum_operations == 0
            || policy.maximum_operation_bytes == 0
            || (policy.maximum_memory_bytes == 0
                && policy.maximum_disk_bytes == 0))
        {
            return {ValidationCode::history_limit_required};
        }

        if (policy.maximum_operations > bounds.maximum_operations
            || policy.maximum_checkpoints > bounds.maximum_checkpoints
            || policy.checkpoint_interval_operations
                > bounds.maximum_checkpoint_interval_operations
            || policy.maximum_operation_bytes > bounds.maximum_operation_bytes
            || policy.maximum_checkpoint_bytes > bounds.maximum_checkpoint_bytes
            || policy.maximum_memory_bytes > bounds.maximum_memory_bytes
            || policy.maximum_disk_bytes > bounds.maximum_disk_bytes)
        {
            return {ValidationCode::history_limit_exceeded};
        }

        const std::uint64_t availableStorage =
            policy.maximum_memory_bytes > policy.maximum_disk_bytes
                ? policy.maximum_memory_bytes
                : policy.maximum_disk_bytes;
        if (policy.maximum_operation_bytes > availableStorage)
            return {ValidationCode::inconsistent_storage_budget};

        if (history_mode_uses_checkpoints(policy.mode))
        {
            if (policy.checkpoint_interval_operations == 0
                || policy.maximum_checkpoints == 0
                || policy.maximum_checkpoint_bytes == 0)
            {
                return {ValidationCode::checkpoint_policy_required};
            }
            if (policy.checkpoint_interval_operations > policy.maximum_operations
                || policy.maximum_checkpoint_bytes > availableStorage)
            {
                return {ValidationCode::inconsistent_storage_budget};
            }
            if (policy.mode == HistoryMode::full_snapshot_debug
                && policy.checkpoint_interval_operations != 1)
            {
                return {ValidationCode::checkpoint_policy_required};
            }
        }

        return {};
    }

    namespace detail
    {
        [[nodiscard]] constexpr std::uint64_t avalanche64(
            std::uint64_t value) noexcept
        {
            value ^= value >> 30u;
            value *= 0xbf58476d1ce4e5b9ull;
            value ^= value >> 27u;
            value *= 0x94d049bb133111ebull;
            value ^= value >> 31u;
            return value;
        }
    }

    class DeterministicHashBuilder final
    {
    public:
        constexpr explicit DeterministicHashBuilder(
            std::string_view domain = {}) noexcept
        {
            append_string(domain);
        }

        constexpr void append_byte(std::uint8_t value) noexcept
        {
            for (std::size_t lane = 0; lane < state_.size(); ++lane)
            {
                const std::uint64_t laneInput =
                    static_cast<std::uint64_t>(value)
                    + 0x9e3779b97f4a7c15ull * (lane + 1u);
                state_[lane] ^= laneInput
                    + std::rotl(state_[(lane + 1u) % state_.size()],
                        static_cast<int>(11u + lane * 7u));
                state_[lane] *= primes_[lane];
                state_[lane] = std::rotl(
                    state_[lane],
                    static_cast<int>(17u + lane * 5u));
            }

            if (byteCount_ == (std::numeric_limits<std::uint64_t>::max)())
                overflowed_ = true;
            else
                ++byteCount_;
        }

        constexpr void append_bytes(std::span<const std::byte> bytes) noexcept
        {
            for (const std::byte value : bytes)
                append_byte(std::to_integer<std::uint8_t>(value));
        }

        constexpr void append_u8(std::uint8_t value) noexcept
        {
            append_byte(value);
        }

        constexpr void append_u16(std::uint16_t value) noexcept
        {
            append_unsigned(value);
        }

        constexpr void append_u32(std::uint32_t value) noexcept
        {
            append_unsigned(value);
        }

        constexpr void append_u64(std::uint64_t value) noexcept
        {
            append_unsigned(value);
        }

        constexpr void append_i64(std::int64_t value) noexcept
        {
            append_u64(static_cast<std::uint64_t>(value));
        }

        constexpr void append_bool(bool value) noexcept
        {
            append_u8(value ? 1u : 0u);
        }

        constexpr void append_float(float value) noexcept
        {
            std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            if ((bits & 0x7fffffffu) == 0u)
                bits = 0u;
            else if ((bits & 0x7f800000u) == 0x7f800000u
                && (bits & 0x007fffffu) != 0u)
                bits = 0x7fc00000u;
            append_u32(bits);
        }

        constexpr void append_double(double value) noexcept
        {
            std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
            if ((bits & 0x7fffffffffffffffull) == 0ull)
                bits = 0ull;
            else if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull
                && (bits & 0x000fffffffffffffull) != 0ull)
                bits = 0x7ff8000000000000ull;
            append_u64(bits);
        }

        constexpr void append_string(std::string_view value) noexcept
        {
            append_u64(static_cast<std::uint64_t>(value.size()));
            for (const unsigned char character : value)
                append_byte(character);
        }

        template<typename Enum>
            requires std::is_enum_v<Enum>
        constexpr void append_enum(Enum value) noexcept
        {
            using Underlying = std::underlying_type_t<Enum>;
            using Unsigned = std::make_unsigned_t<Underlying>;
            append_u8(static_cast<std::uint8_t>(sizeof(Underlying)));
            append_unsigned(static_cast<Unsigned>(value));
        }

        template<typename Tag, std::unsigned_integral Index>
            requires (!std::same_as<std::remove_cv_t<Index>, bool>)
        constexpr void append_handle(Handle<Tag, Index> handle) noexcept
        {
            append_u8(static_cast<std::uint8_t>(sizeof(Index)));
            append_unsigned(handle.index);
            append_u32(handle.generation);
        }

        constexpr void append_hash(const ContentHash& hash) noexcept
        {
            for (const std::uint64_t word : hash.words)
                append_u64(word);
        }

        [[nodiscard]] constexpr std::uint64_t byte_count() const noexcept
        {
            return byteCount_;
        }

        [[nodiscard]] constexpr bool overflowed() const noexcept
        {
            return overflowed_;
        }

        [[nodiscard]] constexpr ContentHash finish() const noexcept
        {
            ContentHash result{};
            for (std::size_t lane = 0; lane < state_.size(); ++lane)
            {
                const std::uint64_t finalInput = state_[lane]
                    ^ std::rotl(byteCount_, static_cast<int>(lane * 13u))
                    ^ (overflowed_ ? 0xd6e8feb86659fd93ull : 0ull)
                    ^ offsets_[(lane + 1u) % offsets_.size()];
                result.words[lane] = detail::avalanche64(finalInput);
            }
            if (result.empty())
                result.words[0] = 0x6a09e667f3bcc909ull;
            return result;
        }

    private:
        template<std::unsigned_integral Unsigned>
            requires (!std::same_as<std::remove_cv_t<Unsigned>, bool>)
        constexpr void append_unsigned(Unsigned value) noexcept
        {
            using ShiftType = std::conditional_t<(sizeof(Unsigned) < sizeof(unsigned int)),
                unsigned int, Unsigned>;
            ShiftType remaining = static_cast<ShiftType>(value);
            for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte)
            {
                append_byte(static_cast<std::uint8_t>(
                    remaining & static_cast<ShiftType>(0xffu)));
                remaining >>= 8u;
            }
        }

        static constexpr std::array<std::uint64_t, 4> offsets_{
            0x243f6a8885a308d3ull,
            0x13198a2e03707344ull,
            0xa4093822299f31d0ull,
            0x082efa98ec4e6c89ull
        };
        static constexpr std::array<std::uint64_t, 4> primes_{
            0x00000100000001b3ull,
            0x9e3779b185ebca87ull,
            0xc2b2ae3d27d4eb4full,
            0x165667b19e3779f9ull
        };

        std::array<std::uint64_t, 4> state_{offsets_};
        std::uint64_t byteCount_{};
        bool overflowed_{};
    };

    inline constexpr std::string_view kContentHashDomain{
        "epoch.authoring.content.v1"};
    inline constexpr std::string_view kIdentityHashDomain{
        "epoch.authoring.identity.v1"};

    [[nodiscard]] constexpr ContentHash deterministic_content_hash(
        std::span<const std::byte> bytes,
        std::string_view domain = kContentHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_bytes(bytes);
        return builder.finish();
    }

    [[nodiscard]] constexpr ContentHash deterministic_content_hash(
        std::string_view text,
        std::string_view domain = kContentHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_string(text);
        return builder.finish();
    }

    [[nodiscard]] constexpr std::uint64_t deterministic_hash64(
        const ContentHash& hash,
        std::string_view domain = kIdentityHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_hash(hash);
        return builder.finish().words[0];
    }

    template<typename Tag, std::unsigned_integral Index>
        requires (!std::same_as<std::remove_cv_t<Index>, bool>)
    [[nodiscard]] constexpr std::uint64_t deterministic_hash64(
        Handle<Tag, Index> handle,
        std::string_view domain = kIdentityHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_handle(handle);
        return builder.finish().words[0];
    }

    [[nodiscard]] constexpr std::uint64_t deterministic_hash64(
        BranchIdentity branch,
        std::string_view domain = kIdentityHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_u64(branch.timeline);
        builder.append_u64(branch.branch);
        return builder.finish().words[0];
    }

    [[nodiscard]] constexpr std::uint64_t deterministic_hash64(
        OperationId operation,
        std::string_view domain = kIdentityHashDomain) noexcept
    {
        DeterministicHashBuilder builder{domain};
        builder.append_u64(operation.value);
        return builder.finish().words[0];
    }

    template<typename Tag, std::unsigned_integral Index = std::uint32_t>
        requires (!std::same_as<std::remove_cv_t<Index>, bool>)
    struct HandleHash final
    {
        [[nodiscard]] constexpr std::size_t operator()(
            Handle<Tag, Index> handle) const noexcept
        {
            return static_cast<std::size_t>(deterministic_hash64(handle));
        }
    };

    using ContentHashHex = std::array<char, 65>;

    [[nodiscard]] constexpr ContentHashHex content_hash_hex(
        const ContentHash& hash) noexcept
    {
        constexpr char digits[] = "0123456789abcdef";
        ContentHashHex result{};
        std::size_t output = 0;
        for (const std::uint64_t word : hash.words)
        {
            for (int shift = 60; shift >= 0; shift -= 4)
                result[output++] = digits[(word >> shift) & 0x0full];
        }
        result[64] = '\0';
        return result;
    }

    [[nodiscard]] constexpr std::optional<ContentHash> content_hash_from_hex(
        std::string_view text) noexcept
    {
        if (text.size() != 64)
            return std::nullopt;

        ContentHash result{};
        for (std::size_t word = 0; word < result.words.size(); ++word)
        {
            std::uint64_t value = 0;
            for (std::size_t nibble = 0; nibble < 16; ++nibble)
            {
                const char character = text[word * 16u + nibble];
                std::uint8_t decoded{};
                if (character >= '0' && character <= '9')
                    decoded = static_cast<std::uint8_t>(character - '0');
                else if (character >= 'a' && character <= 'f')
                    decoded = static_cast<std::uint8_t>(character - 'a' + 10);
                else if (character >= 'A' && character <= 'F')
                    decoded = static_cast<std::uint8_t>(character - 'A' + 10);
                else
                    return std::nullopt;
                value = (value << 4u) | decoded;
            }
            result.words[word] = value;
        }

        return result.empty()
            ? std::nullopt
            : std::optional<ContentHash>{result};
    }
}
