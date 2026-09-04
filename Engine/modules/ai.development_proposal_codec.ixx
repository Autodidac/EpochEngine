/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module ai.development_proposal_codec;

export namespace epochengine::ai::development_proposal_codec
{
    inline constexpr std::uint32_t schema_version = 1u;

    enum class SourceArea : std::uint8_t
    {
        engine,
        project
    };

    [[nodiscard]] constexpr std::string_view area_name(
        SourceArea area) noexcept
    {
        switch (area)
        {
        case SourceArea::engine: return "engine";
        case SourceArea::project: return "project";
        }
        return "unknown";
    }

    enum class SourceEditKind : std::uint8_t
    {
        replace_file,
        replace_exact_block
    };

    [[nodiscard]] constexpr std::string_view edit_kind_name(
        SourceEditKind kind) noexcept
    {
        switch (kind)
        {
        case SourceEditKind::replace_file: return "replace_file";
        case SourceEditKind::replace_exact_block: return "replace_exact_block";
        }
        return "unknown";
    }

    struct SourceChange final
    {
        SourceArea area{SourceArea::project};
        std::string relative_path{};
        std::string summary{};
        SourceEditKind edit_kind{SourceEditKind::replace_file};
        std::string match_bytes{};
        std::string replacement_bytes{};
    };

    struct Proposal final
    {
        std::string title{};
        std::string rationale{};
        std::uint64_t lifetime_seconds{900u};
        std::vector<SourceChange> changes{};
    };

    struct DecodeLimits final
    {
        std::size_t maximum_reply_bytes{1024u * 1024u};
        std::size_t maximum_operations{12u};
        std::size_t maximum_metadata_bytes{4096u};
        std::size_t maximum_path_bytes{1024u};
        std::size_t maximum_file_bytes{256u * 1024u};
        std::size_t maximum_total_replacement_bytes{1024u * 1024u};
        std::uint64_t maximum_lifetime_seconds{60u * 60u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_reply_bytes >= 256u
                && maximum_operations > 0u
                && maximum_operations <= 64u
                && maximum_metadata_bytes > 0u
                && maximum_path_bytes > 0u
                && maximum_file_bytes > 0u
                && maximum_total_replacement_bytes >= maximum_file_bytes
                && maximum_total_replacement_bytes <= maximum_reply_bytes
                && maximum_lifetime_seconds >= 60u;
        }
    };

    enum class DecodeCode : std::uint8_t
    {
        none,
        invalid_limits,
        empty_input,
        size_limit_exceeded,
        invalid_utf8,
        invalid_header,
        missing_field,
        invalid_field,
        operation_limit_exceeded,
        invalid_area,
        invalid_path,
        duplicate_path,
        malformed_content,
        trailing_data
    };

    [[nodiscard]] constexpr std::string_view code_name(
        DecodeCode code) noexcept
    {
        switch (code)
        {
        case DecodeCode::none: return "none";
        case DecodeCode::invalid_limits: return "invalid_limits";
        case DecodeCode::empty_input: return "empty_input";
        case DecodeCode::size_limit_exceeded: return "size_limit_exceeded";
        case DecodeCode::invalid_utf8: return "invalid_utf8";
        case DecodeCode::invalid_header: return "invalid_header";
        case DecodeCode::missing_field: return "missing_field";
        case DecodeCode::invalid_field: return "invalid_field";
        case DecodeCode::operation_limit_exceeded:
            return "operation_limit_exceeded";
        case DecodeCode::invalid_area: return "invalid_area";
        case DecodeCode::invalid_path: return "invalid_path";
        case DecodeCode::duplicate_path: return "duplicate_path";
        case DecodeCode::malformed_content: return "malformed_content";
        case DecodeCode::trailing_data: return "trailing_data";
        }
        return "unknown";
    }

    struct DecodeResult final
    {
        DecodeCode code{DecodeCode::empty_input};
        std::string status{};
        std::size_t line{};
        Proposal proposal{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == DecodeCode::none;
        }
    };

    struct ContextRequest final
    {
        std::string reason{};
        std::vector<std::string> paths{};
    };

    struct ContextRequestDecodeResult final
    {
        bool recognized{};
        DecodeCode code{DecodeCode::empty_input};
        std::string status{};
        std::size_t line{};
        ContextRequest request{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return recognized && code == DecodeCode::none;
        }
    };

    struct QualityResult final
    {
        bool accepted{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
    };

    [[nodiscard]] ContextRequestDecodeResult decode_context_request(
        std::string_view reply,
        SourceArea area,
        std::size_t maximum_paths = 12u,
        std::size_t maximum_reason_bytes = 512u,
        std::size_t maximum_path_bytes = 1024u);

    [[nodiscard]] std::string context_request_prompt(
        SourceArea area,
        std::string_view objective,
        std::string_view architecture_evidence);

    [[nodiscard]] bool valid_context_text(
        std::string_view text) noexcept;

    [[nodiscard]] DecodeResult decode(
        std::string_view reply,
        DecodeLimits limits = {});

    [[nodiscard]] QualityResult validate_quality(
        const Proposal& proposal,
        std::string_view objective,
        std::string_view exact_source_evidence);

    [[nodiscard]] std::string protocol_prompt(SourceArea area);

    [[nodiscard]] std::string protocol_prompt(
        SourceArea area,
        std::string_view objective,
        std::string_view architecture_evidence);

    [[nodiscard]] bool run_contract();
}
