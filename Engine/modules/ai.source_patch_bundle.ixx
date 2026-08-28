/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.source_patch_bundle;

export namespace epochengine::ai::source_patch_bundle
{
    inline constexpr std::uint32_t schema_version = 1u;

    struct Digest final
    {
        std::array<std::uint8_t, 32u> bytes{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            for (const std::uint8_t byte : bytes)
                if (byte != 0u) return true;
            return false;
        }

        friend constexpr bool operator==(
            const Digest& left,
            const Digest& right) noexcept
        {
            for (std::size_t index = 0u; index < left.bytes.size(); ++index)
                if (left.bytes[index] != right.bytes[index]) return false;
            return true;
        }
    };

    enum class Operation : std::uint8_t { add, update, remove };
    enum class Encoding : std::uint8_t { utf8 };
    enum class LineEnding : std::uint8_t { none, lf, crlf };
    enum class EntryKind : std::uint8_t { missing, regular, symlink, other };

    struct FileState final
    {
        bool exists{};
        Digest digest{};
        std::uint64_t byte_count{};
        Encoding encoding{Encoding::utf8};
        LineEnding line_ending{LineEnding::none};
        bool final_newline{};

        friend constexpr bool operator==(const FileState&, const FileState&) = default;
    };

    struct CuratedFile final
    {
        std::string relative_path{};
        FileState preimage{};
        std::string preimage_bytes{};
        bool allow_create{};
        bool allow_remove{};
    };

    enum class HunkLineKind : std::uint8_t { context, remove, add };

    struct HunkLine final
    {
        HunkLineKind kind{HunkLineKind::context};
        std::string bytes{};
    };

    struct Hunk final
    {
        std::uint64_t old_start{};
        std::uint64_t old_count{};
        std::uint64_t new_start{};
        std::uint64_t new_count{};
        std::vector<HunkLine> lines{};
        Digest digest{};
    };

    struct FilePatch final
    {
        Operation operation{Operation::update};
        std::string relative_path{};
        FileState preimage{};
        FileState postimage{};
        std::string postimage_bytes{};
        std::vector<Hunk> hunks{};
        Digest evidence_digest{};
    };

    struct Bundle final
    {
        std::string bundle_id{};
        Digest authority_digest{};
        Digest digest{};
        std::string canonical_diff{};
        std::vector<FilePatch> files{};
    };

    struct Limits final
    {
        std::size_t maximum_patch_bytes{1024u * 1024u};
        std::size_t maximum_files{16u};
        std::size_t maximum_hunks_per_file{64u};
        std::size_t maximum_lines_per_hunk{4096u};
        std::size_t maximum_path_bytes{1024u};
        std::uint64_t maximum_file_bytes{16u * 1024u * 1024u};
        std::uint64_t maximum_total_postimage_bytes{64u * 1024u * 1024u};
    };

    enum class DecodeCode : std::uint8_t
    {
        none,
        invalid_limits,
        empty_input,
        size_limit_exceeded,
        invalid_utf8,
        invalid_header,
        invalid_metadata,
        invalid_path,
        uncurated_path,
        duplicate_path,
        unsupported_operation,
        binary_rejected,
        link_rejected,
        preimage_mismatch,
        malformed_hunk,
        overlapping_hunk,
        ambiguous_hunk,
        content_mismatch,
        trailing_data
    };

    struct DecodeResult final
    {
        DecodeCode code{DecodeCode::empty_input};
        std::string status{};
        std::size_t line{};
        Bundle bundle{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == DecodeCode::none;
        }
    };

    struct PortRead final
    {
        EntryKind kind{EntryKind::missing};
        std::string bytes{};
    };

    class SandboxFilePort
    {
    public:
        virtual ~SandboxFilePort() = default;
        [[nodiscard]] virtual PortRead read_live(std::string_view relative_path) = 0;
        [[nodiscard]] virtual bool begin(std::string_view transaction_id, Digest bundle_digest) = 0;
        [[nodiscard]] virtual bool stage_write(std::string_view relative_path, std::string_view bytes) = 0;
        [[nodiscard]] virtual bool stage_remove(std::string_view relative_path) = 0;
        [[nodiscard]] virtual PortRead read_staged(std::string_view relative_path) = 0;
        [[nodiscard]] virtual bool commit(Digest expected_manifest_digest) = 0;
        virtual void rollback() noexcept = 0;
    };

    struct ApplyPermit final
    {
        std::string receipt_id{};
        Digest bundle_digest{};
        Digest authority_digest{};
        bool operator_approved{};
    };

    enum class ApplyCode : std::uint8_t
    {
        none,
        invalid_permit,
        replay_rejected,
        stale_preimage,
        destination_rejected,
        transaction_rejected,
        stage_failed,
        stage_verification_failed,
        commit_failed,
        postimage_verification_failed
    };

    struct HunkEvidence final
    {
        std::uint64_t old_start{};
        std::uint64_t old_count{};
        std::uint64_t new_start{};
        std::uint64_t new_count{};
        Digest digest{};
    };

    struct FileEvidence final
    {
        std::string relative_path{};
        Operation operation{Operation::update};
        FileState before{};
        FileState after{};
        std::vector<HunkEvidence> hunks{};
        bool preimage_verified{};
        bool staged_verified{};
        bool committed_verified{};
    };

    struct ApplyResult final
    {
        ApplyCode code{ApplyCode::invalid_permit};
        std::string status{};
        Digest evidence_digest{};
        std::vector<FileEvidence> files{};
        bool rolled_back{};
        bool committed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ApplyCode::none && committed;
        }
    };

    [[nodiscard]] Digest digest(std::string_view bytes) noexcept;
    [[nodiscard]] std::string digest_hex(Digest value);
    [[nodiscard]] DecodeResult decode(
        std::string_view canonical_diff,
        std::span<const CuratedFile> curated,
        Limits limits = {});
    [[nodiscard]] std::string encode(const Bundle& bundle);

    class Executor final
    {
    public:
        [[nodiscard]] ApplyResult apply(
            const Bundle& bundle,
            const ApplyPermit& permit,
            SandboxFilePort& port);

    private:
        std::vector<std::string> consumed_receipts_{};
        std::vector<Digest> committed_bundles_{};
    };

    [[nodiscard]] bool run_contract();
}
