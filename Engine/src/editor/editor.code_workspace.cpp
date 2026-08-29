/*
 * This file is part of the Epoch Project.
 * epochengine - Modular C++ Framework
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module editor.code_workspace;

import platform.filesystem;

namespace epochengine::editor_code_workspace
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::size_t kAbsoluteMaximumFileBytes = 512u * 1024u;
        constexpr std::size_t kAbsoluteMaximumTabs = 64u;
        constexpr std::size_t kMaximumFindBytes = 1'024u;
        constexpr std::size_t kMaximumFindMatches = 4'096u;
        constexpr std::size_t kMaximumDiagnostics = 512u;
        constexpr std::size_t kMaximumDiagnosticSourceBytes = 128u;
        constexpr std::size_t kMaximumDiagnosticCodeBytes = 64u;
        constexpr std::size_t kMaximumDiagnosticMessageBytes = 2'048u;
        constexpr std::size_t kMaximumSessionBytes = 64u * 1'024u;
        constexpr std::string_view kSessionHeader{
            "epoch-code-workspace-state-v1"};

        struct ByteRange final
        {
            std::size_t begin{};
            std::size_t end{};
        };

        struct LoadedFile final
        {
            fs::path canonical_path{};
            std::string relative_path{};
            std::string text{};
            bool utf8_bom{};
        };

        constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3u};

        [[nodiscard]] std::string encoded_source_bytes(
            const std::string_view text,
            const bool utf8_bom)
        {
            std::string bytes{};
            bytes.reserve(text.size() + (utf8_bom ? kUtf8Bom.size() : 0u));
            if (utf8_bom)
                bytes.append(kUtf8Bom);
            bytes.append(text);
            return bytes;
        }

        [[nodiscard]] OperationResult result(
            const ResultCode code,
            std::string reason,
            const std::optional<DocumentHandle> document = std::nullopt,
            std::string text = {})
        {
            return OperationResult{
                .code = code,
                .reason = std::move(reason),
                .document = document,
                .text = std::move(text)};
        }

        [[nodiscard]] bool authority_valid(
            const WorkspaceAuthority& authority) noexcept
        {
            return authority.surface_revision != 0u
                && authority.project_revision != 0u
                && authority.async_generation != 0u;
        }

        [[nodiscard]] bool has_parent_component(const fs::path& path)
        {
            return std::any_of(
                path.begin(), path.end(),
                [](const fs::path& component)
                {
                    return component == "." || component == "..";
                });
        }

        [[nodiscard]] bool path_is_symlink(
            const fs::path& path,
            std::error_code& error)
        {
            const fs::file_status status = fs::symlink_status(path, error);
            return !error && fs::is_symlink(status);
        }

        [[nodiscard]] ResultCode validate_root(
            const fs::path& requested,
            fs::path& canonical,
            std::string& reason)
        {
            if (requested.empty() || !requested.is_absolute())
            {
                reason = "The code-workspace root must be an absolute path.";
                return ResultCode::invalid_request;
            }

            std::error_code error{};
            if (path_is_symlink(requested, error))
            {
                reason = "The code-workspace root is a symbolic link.";
                return ResultCode::symlink_path;
            }
            if (error)
            {
                reason = "The code-workspace root could not be inspected.";
                return ResultCode::unavailable;
            }

            canonical = fs::canonical(requested, error);
            if (error || canonical.empty()
                || !fs::is_directory(canonical, error) || error)
            {
                reason = "The code-workspace root is unavailable.";
                return ResultCode::unavailable;
            }
            return ResultCode::success;
        }

        [[nodiscard]] ResultCode resolve_file(
            const fs::path& root,
            const std::string_view relative_text,
            const std::size_t maximum_bytes,
            LoadedFile& loaded,
            std::string& reason)
        {
            const fs::path relative{relative_text};
            if (relative.empty() || relative.is_absolute()
                || has_parent_component(relative))
            {
                reason = "The requested code path is not a canonical relative path.";
                return ResultCode::external_path;
            }

            fs::path inspected = root;
            std::error_code error{};
            for (const fs::path& component : relative)
            {
                inspected /= component;
                if (path_is_symlink(inspected, error))
                {
                    reason = "The requested code path crosses a symbolic link.";
                    return ResultCode::symlink_path;
                }
                if (error)
                {
                    reason = "The requested code path could not be inspected.";
                    return ResultCode::missing_file;
                }
            }

            const fs::path candidate = fs::canonical(root / relative, error);
            if (error || candidate.empty()
                || !fs::is_regular_file(candidate, error) || error)
            {
                reason = "The requested code file does not exist as a regular file.";
                return ResultCode::missing_file;
            }

            const fs::path contained = candidate.lexically_relative(root);
            if (contained.empty() || contained.is_absolute()
                || has_parent_component(contained))
            {
                reason = "The requested code file resolves outside the workspace root.";
                return ResultCode::external_path;
            }

            const std::uintmax_t size = fs::file_size(candidate, error);
            if (error)
            {
                reason = "The requested code file size could not be read.";
                return ResultCode::unavailable;
            }
            if (size > maximum_bytes
                || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                reason = "The requested code file exceeds the workspace byte limit.";
                return ResultCode::oversized_file;
            }

            std::ifstream input(candidate, std::ios::binary);
            if (!input)
            {
                reason = "The requested code file could not be opened.";
                return ResultCode::unavailable;
            }

            std::string text(static_cast<std::size_t>(size), '\0');
            if (!text.empty())
            {
                input.read(text.data(), static_cast<std::streamsize>(text.size()));
                if (!input || static_cast<std::size_t>(input.gcount()) != text.size())
                {
                    reason = "The requested code file changed while it was read.";
                    return ResultCode::stale_disk;
                }
            }
            if (text.starts_with(kUtf8Bom))
            {
                loaded.utf8_bom = true;
                text.erase(0u, kUtf8Bom.size());
            }
            else if (text.starts_with(std::string_view{"\xFF\xFE", 2u})
                || text.starts_with(std::string_view{"\xFE\xFF", 2u}))
            {
                reason = "The requested code file uses a UTF-16 BOM; convert it to UTF-8 before editing.";
                return ResultCode::invalid_utf8;
            }
            if (!valid_utf8(text))
            {
                reason = "The requested code file contains invalid UTF-8 bytes; no text was opened or replaced.";
                return ResultCode::invalid_utf8;
            }

            loaded.canonical_path = candidate;
            loaded.relative_path = contained.generic_string();
            loaded.text = std::move(text);
            return ResultCode::success;
        }

        [[nodiscard]] std::vector<std::size_t> line_starts(
            const std::string_view text)
        {
            std::vector<std::size_t> result{0u};
            result.reserve(1u + static_cast<std::size_t>(
                std::count(text.begin(), text.end(), '\n')));
            for (std::size_t index = 0u; index < text.size(); ++index)
            {
                if (text[index] == '\n')
                    result.push_back(index + 1u);
            }
            return result;
        }

        [[nodiscard]] std::size_t next_codepoint(
            const std::string_view text,
            const std::size_t index) noexcept
        {
            if (index >= text.size())
                return text.size();
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            if (lead < 0x80u)
                return index + 1u;
            if (lead < 0xe0u)
                return (std::min)(text.size(), index + 2u);
            if (lead < 0xf0u)
                return (std::min)(text.size(), index + 3u);
            return (std::min)(text.size(), index + 4u);
        }

        [[nodiscard]] std::size_t codepoint_count(
            const std::string_view text,
            std::size_t begin,
            const std::size_t end) noexcept
        {
            std::size_t count{};
            begin = (std::min)(begin, text.size());
            while (begin < (std::min)(end, text.size()))
            {
                begin = next_codepoint(text, begin);
                ++count;
            }
            return count;
        }

        [[nodiscard]] std::size_t maximum_columns(
            const std::string_view text,
            const std::vector<std::size_t>& starts) noexcept
        {
            std::size_t maximum{};
            for (std::size_t line = 0u; line < starts.size(); ++line)
            {
                const std::size_t begin = starts[line];
                std::size_t end = line + 1u < starts.size()
                    ? starts[line + 1u] - 1u
                    : text.size();
                if (end > begin && text[end - 1u] == '\r')
                    --end;
                maximum = (std::max)(
                    maximum,
                    codepoint_count(text, begin, end));
            }
            return maximum;
        }

        [[nodiscard]] Viewport clamp_viewport(
            Viewport viewport,
            const std::size_t lines,
            const std::size_t columns) noexcept
        {
            viewport.visible_lines = (std::max)(std::size_t{1u}, viewport.visible_lines);
            viewport.visible_columns = (std::max)(std::size_t{1u}, viewport.visible_columns);
            const std::size_t maximum_first_line = lines > viewport.visible_lines
                ? lines - viewport.visible_lines
                : 0u;
            const std::size_t maximum_first_column = columns > viewport.visible_columns
                ? columns - viewport.visible_columns
                : 0u;
            viewport.first_line = (std::min)(viewport.first_line, maximum_first_line);
            viewport.first_column = (std::min)(viewport.first_column, maximum_first_column);
            return viewport;
        }

        [[nodiscard]] bool read_exact(
            const fs::path& path,
            std::string& bytes)
        {
            std::error_code error{};
            const std::uintmax_t size = fs::file_size(path, error);
            if (error || size > static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;
            bytes.assign(static_cast<std::size_t>(size), '\0');
            if (!bytes.empty())
            {
                input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                return input
                    && static_cast<std::size_t>(input.gcount()) == bytes.size();
            }
            return true;
        }

        [[nodiscard]] constexpr unsigned char ascii_fold(
            const unsigned char value) noexcept
        {
            return value >= static_cast<unsigned char>('A')
                    && value <= static_cast<unsigned char>('Z')
                ? static_cast<unsigned char>(
                    value + static_cast<unsigned char>('a' - 'A'))
                : value;
        }

        [[nodiscard]] constexpr bool identifier_byte(
            const unsigned char value) noexcept
        {
            return value >= 0x80u
                || (value >= static_cast<unsigned char>('a')
                    && value <= static_cast<unsigned char>('z'))
                || (value >= static_cast<unsigned char>('A')
                    && value <= static_cast<unsigned char>('Z'))
                || (value >= static_cast<unsigned char>('0')
                    && value <= static_cast<unsigned char>('9'))
                || value == static_cast<unsigned char>('_');
        }

        [[nodiscard]] bool match_at(
            const std::string_view text,
            const std::string_view query,
            const std::size_t offset,
            const FindOptions options) noexcept
        {
            if (query.empty() || offset > text.size()
                || query.size() > text.size() - offset)
            {
                return false;
            }
            for (std::size_t index = 0u; index < query.size(); ++index)
            {
                const unsigned char left = static_cast<unsigned char>(
                    text[offset + index]);
                const unsigned char right = static_cast<unsigned char>(
                    query[index]);
                if (options.case_sensitive
                    ? left != right
                    : ascii_fold(left) != ascii_fold(right))
                {
                    return false;
                }
            }
            if (!options.whole_identifier)
                return true;
            const bool left_boundary = offset == 0u
                || !identifier_byte(static_cast<unsigned char>(text[offset - 1u]));
            const std::size_t end = offset + query.size();
            const bool right_boundary = end == text.size()
                || !identifier_byte(static_cast<unsigned char>(text[end]));
            return left_boundary && right_boundary;
        }

        [[nodiscard]] std::vector<ByteRange> find_matches(
            const std::string_view text,
            const std::string_view query,
            const FindOptions options)
        {
            std::vector<ByteRange> matches{};
            if (query.empty() || query.size() > text.size())
                return matches;
            std::size_t offset{};
            while (offset + query.size() <= text.size()
                && matches.size() < kMaximumFindMatches)
            {
                if (match_at(text, query, offset, options))
                {
                    matches.push_back(ByteRange{
                        .begin = offset,
                        .end = offset + query.size()});
                    offset += query.size();
                }
                else
                {
                    offset = next_codepoint(text, offset);
                }
            }
            return matches;
        }

        [[nodiscard]] TextRange text_range(
            const std::string_view text,
            const ByteRange bytes) noexcept
        {
            return TextRange{
                .anchor = text_position(text, bytes.begin),
                .caret = text_position(text, bytes.end)};
        }

        [[nodiscard]] bool same_range(
            const TextRange& left,
            const TextRange& right) noexcept
        {
            return left.anchor == right.anchor && left.caret == right.caret;
        }

        void reveal_range(
            Viewport& viewport,
            const TextRange range,
            const std::size_t lines,
            const std::size_t columns) noexcept
        {
            viewport.first_line = range.caret.line;
            viewport.first_column = range.caret.column;
            viewport = clamp_viewport(viewport, lines, columns);
        }

        [[nodiscard]] char hex_digit(const unsigned char value) noexcept
        {
            return static_cast<char>(value < 10u
                ? static_cast<unsigned char>('0') + value
                : static_cast<unsigned char>('a') + value - 10u);
        }

        [[nodiscard]] std::string hex_encode(const std::string_view text)
        {
            if (text.empty())
                return "-";
            std::string encoded{};
            encoded.reserve(text.size() * 2u);
            for (const unsigned char value : text)
            {
                encoded.push_back(hex_digit(value >> 4u));
                encoded.push_back(hex_digit(value & 0x0fu));
            }
            return encoded;
        }

        [[nodiscard]] std::optional<unsigned char> hex_value(
            const char value) noexcept
        {
            if (value >= '0' && value <= '9')
                return static_cast<unsigned char>(value - '0');
            if (value >= 'a' && value <= 'f')
                return static_cast<unsigned char>(value - 'a' + 10);
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> hex_decode(
            const std::string_view encoded)
        {
            if (encoded == "-")
                return std::string{};
            if (encoded.empty() || encoded.size() % 2u != 0u)
                return std::nullopt;
            std::string decoded(encoded.size() / 2u, '\0');
            for (std::size_t index = 0u; index < decoded.size(); ++index)
            {
                const auto high = hex_value(encoded[index * 2u]);
                const auto low = hex_value(encoded[index * 2u + 1u]);
                if (!high || !low)
                    return std::nullopt;
                decoded[index] = static_cast<char>((*high << 4u) | *low);
            }
            return valid_utf8(decoded)
                ? std::optional<std::string>{std::move(decoded)}
                : std::nullopt;
        }

        [[nodiscard]] std::vector<std::string_view> split_tokens(
            const std::string_view line)
        {
            std::vector<std::string_view> tokens{};
            std::size_t begin{};
            while (begin < line.size())
            {
                while (begin < line.size() && line[begin] == ' ')
                    ++begin;
                if (begin == line.size())
                    break;
                const std::size_t end = line.find(' ', begin);
                tokens.push_back(line.substr(
                    begin,
                    end == std::string_view::npos
                        ? line.size() - begin
                        : end - begin));
                if (end == std::string_view::npos)
                    break;
                begin = end + 1u;
            }
            return tokens;
        }

        [[nodiscard]] bool parse_size(
            const std::string_view token,
            std::size_t& value) noexcept
        {
            value = 0u;
            const auto parsed = std::from_chars(
                token.data(), token.data() + token.size(), value);
            return !token.empty()
                && parsed.ec == std::errc{}
                && parsed.ptr == token.data() + token.size();
        }
    }

    struct Controller::Implementation final
    {
        struct Document final
        {
            DocumentHandle handle{};
            fs::path canonical_path{};
            std::string relative_path{};
            std::string label{};
            std::string text{};
            std::string persisted_text{};
            std::vector<std::size_t> line_starts{0u};
            std::uint64_t revision{1u};
            std::uint64_t persisted_revision{1u};
            TextRange selection{};
            Viewport viewport{};
            FindSnapshot find{};
            std::vector<Diagnostic> diagnostics{};
            std::uint64_t diagnostics_revision{};
            std::size_t maximum_bytes{256u * 1024u};
            bool writable{};
            bool utf8_bom{};
        };

        WorkspaceKind kind{WorkspaceKind::project_scripts};
        std::string workspace_id{};
        fs::path canonical_root{};
        WorkspaceAuthority authority{};
        std::vector<Document> documents{};
        std::optional<DocumentHandle> active{};
        std::uint64_t revision{};
        std::uint32_t next_generation{1u};
        bool configured{};

        [[nodiscard]] Document* find(const DocumentHandle handle) noexcept
        {
            if (!handle.valid())
                return nullptr;
            const auto found = std::find_if(
                documents.begin(), documents.end(),
                [&](const Document& document)
                {
                    return document.handle == handle;
                });
            return found == documents.end() ? nullptr : &*found;
        }

        [[nodiscard]] const Document* find(
            const DocumentHandle handle) const noexcept
        {
            if (!handle.valid())
                return nullptr;
            const auto found = std::find_if(
                documents.begin(), documents.end(),
                [&](const Document& document)
                {
                    return document.handle == handle;
                });
            return found == documents.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool dirty() const noexcept
        {
            return std::any_of(
                documents.begin(), documents.end(),
                [](const Document& document)
                {
                    return document.text != document.persisted_text;
                });
        }
    };

    namespace
    {
        [[nodiscard]] OperationResult validate_operation(
            const Controller::Implementation& state,
            const WorkspaceAuthority& expected)
        {
            if (!state.configured)
                return result(ResultCode::unavailable, "No code workspace is configured.");
            if (expected.surface_revision != state.authority.surface_revision)
                return result(ResultCode::stale_surface, "The editor surface revision changed.");
            if (expected.project_revision != state.authority.project_revision)
                return result(ResultCode::stale_project, "The project/source authority revision changed.");
            if (expected.async_generation != state.authority.async_generation)
                return result(ResultCode::stale_async_generation, "The asynchronous workspace generation changed.");
            return result(ResultCode::success, "Workspace authority is current.");
        }

        [[nodiscard]] OperationResult validate_document_operation(
            const Controller::Implementation& state,
            const DocumentHandle handle,
            const WorkspaceAuthority& expected,
            const std::uint64_t expected_revision,
            const Controller::Implementation::Document*& document)
        {
            if (const OperationResult authority = validate_operation(state, expected);
                !authority)
            {
                return authority;
            }
            document = state.find(handle);
            if (!document)
                return result(ResultCode::stale_document, "The code document handle is stale.");
            if (document->revision != expected_revision)
                return result(ResultCode::stale_document, "The code document revision changed.", handle);
            return result(ResultCode::success, "Code document authority is current.", handle);
        }

        [[nodiscard]] DocumentSnapshot make_snapshot(
            const Controller::Implementation::Document& document,
            const std::optional<DocumentHandle> active)
        {
            return DocumentSnapshot{
                .handle = document.handle,
                .relative_path = document.relative_path,
                .label = document.label,
                .text = document.text,
                .revision = document.revision,
                .persisted_revision = document.persisted_revision,
                .byte_count = document.text.size(),
                .line_count = document.line_starts.size(),
                .maximum_line_columns = maximum_columns(
                    document.text, document.line_starts),
                .selection = document.selection,
                .viewport = document.viewport,
                .find = document.find,
                .diagnostics = document.diagnostics,
                .diagnostics_revision = document.diagnostics_revision,
                .active = active && *active == document.handle,
                .dirty = document.text != document.persisted_text,
                .diagnostics_current = document.diagnostics_revision != 0u
                    && document.diagnostics_revision == document.revision,
                .writable = document.writable,
                .utf8_bom = document.utf8_bom};
        }
    }

    std::string_view to_string(const ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::invalid_request: return "invalid_request";
        case ResultCode::unavailable: return "unavailable";
        case ResultCode::external_path: return "external_path";
        case ResultCode::symlink_path: return "symlink_path";
        case ResultCode::missing_file: return "missing_file";
        case ResultCode::oversized_file: return "oversized_file";
        case ResultCode::invalid_utf8: return "invalid_utf8";
        case ResultCode::duplicate_path: return "duplicate_path";
        case ResultCode::tab_limit: return "tab_limit";
        case ResultCode::stale_surface: return "stale_surface";
        case ResultCode::stale_project: return "stale_project";
        case ResultCode::stale_document: return "stale_document";
        case ResultCode::stale_async_generation: return "stale_async_generation";
        case ResultCode::stale_disk: return "stale_disk";
        case ResultCode::dirty_workspace: return "dirty_workspace";
        case ResultCode::dirty_document: return "dirty_document";
        case ResultCode::not_found: return "not_found";
        case ResultCode::stale_diagnostics: return "stale_diagnostics";
        case ResultCode::read_only: return "read_only";
        case ResultCode::write_failed: return "write_failed";
        case ResultCode::verification_failed: return "verification_failed";
        }
        return "unknown";
    }

    bool valid_utf8(const std::string_view text) noexcept
    {
        std::size_t index{};
        while (index < text.size())
        {
            const unsigned char lead = static_cast<unsigned char>(text[index]);
            if (lead == 0u)
                return false;
            if (lead < 0x80u)
            {
                ++index;
                continue;
            }

            std::size_t count{};
            std::uint32_t value{};
            std::uint32_t minimum{};
            if (lead >= 0xc2u && lead <= 0xdfu)
            {
                count = 2u;
                value = lead & 0x1fu;
                minimum = 0x80u;
            }
            else if (lead >= 0xe0u && lead <= 0xefu)
            {
                count = 3u;
                value = lead & 0x0fu;
                minimum = 0x800u;
            }
            else if (lead >= 0xf0u && lead <= 0xf4u)
            {
                count = 4u;
                value = lead & 0x07u;
                minimum = 0x1'0000u;
            }
            else
            {
                return false;
            }
            if (index + count > text.size())
                return false;
            for (std::size_t offset = 1u; offset < count; ++offset)
            {
                const unsigned char continuation =
                    static_cast<unsigned char>(text[index + offset]);
                if ((continuation & 0xc0u) != 0x80u)
                    return false;
                value = (value << 6u) | (continuation & 0x3fu);
            }
            if (value < minimum || value > 0x10'ffffu
                || (value >= 0xd800u && value <= 0xdfffu))
            {
                return false;
            }
            index += count;
        }
        return true;
    }

    std::optional<std::size_t> byte_offset(
        const std::string_view text,
        const TextPosition position) noexcept
    {
        if (!valid_utf8(text))
            return std::nullopt;
        const std::vector<std::size_t> starts = line_starts(text);
        if (position.line >= starts.size())
            return std::nullopt;
        const std::size_t begin = starts[position.line];
        std::size_t end = position.line + 1u < starts.size()
            ? starts[position.line + 1u] - 1u
            : text.size();
        if (end > begin && text[end - 1u] == '\r')
            --end;
        std::size_t cursor = begin;
        for (std::size_t column = 0u; column < position.column; ++column)
        {
            if (cursor >= end)
                return std::nullopt;
            cursor = next_codepoint(text, cursor);
        }
        return cursor <= end ? std::optional<std::size_t>{cursor} : std::nullopt;
    }

    TextPosition text_position(
        const std::string_view text,
        const std::size_t requested_offset) noexcept
    {
        if (!valid_utf8(text))
            return {};
        const std::size_t offset = (std::min)(requested_offset, text.size());
        const std::vector<std::size_t> starts = line_starts(text);
        const auto upper = std::upper_bound(starts.begin(), starts.end(), offset);
        const std::size_t line = upper == starts.begin()
            ? 0u
            : static_cast<std::size_t>(std::distance(starts.begin(), upper) - 1);
        return TextPosition{
            .line = line,
            .column = codepoint_count(text, starts[line], offset)};
    }

    Controller::Controller()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    Controller::~Controller() = default;
    Controller::Controller(Controller&&) noexcept = default;
    Controller& Controller::operator=(Controller&&) noexcept = default;

    OperationResult Controller::open(const OpenRequest& request)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (request.workspace_id.empty() || !authority_valid(request.authority)
            || request.paths.empty() || request.maximum_file_bytes == 0u
            || request.maximum_file_bytes > kAbsoluteMaximumFileBytes
            || request.maximum_tabs == 0u
            || request.maximum_tabs > kAbsoluteMaximumTabs)
        {
            return result(ResultCode::invalid_request, "The code-workspace request is incomplete or exceeds a hard bound.");
        }
        if (request.paths.size() > request.maximum_tabs)
            return result(ResultCode::tab_limit, "The code-workspace request exceeds its tab limit.");

        fs::path root{};
        std::string reason{};
        if (const ResultCode root_code = validate_root(request.root, root, reason);
            root_code != ResultCode::success)
        {
            return result(root_code, std::move(reason));
        }

        Implementation& current = *implementation_;
        const bool same_identity = current.configured
            && current.kind == request.kind
            && current.workspace_id == request.workspace_id
            && current.canonical_root == root
            && current.authority == request.authority;
        if (current.dirty() && !same_identity)
            return result(ResultCode::dirty_workspace, "The current code workspace has unsaved documents.");

        std::vector<LoadedFile> loaded{};
        loaded.reserve(request.paths.size());
        for (const PathRequest& path : request.paths)
        {
            LoadedFile file{};
            const ResultCode code = resolve_file(
                root, path.relative_path, request.maximum_file_bytes, file, reason);
            if (code != ResultCode::success)
                return result(code, std::move(reason));
            if (std::any_of(
                    loaded.begin(), loaded.end(),
                    [&](const LoadedFile& existing)
                    {
                        return existing.canonical_path == file.canonical_path;
                    }))
            {
                return result(ResultCode::duplicate_path, "The code-workspace request contains the same file more than once.");
            }
            loaded.push_back(std::move(file));
        }

        if (same_identity && loaded.size() == current.documents.size())
        {
            bool same_paths = true;
            for (std::size_t index = 0u; index < loaded.size(); ++index)
            {
                const auto existing = std::find_if(
                    current.documents.begin(), current.documents.end(),
                    [&](const Implementation::Document& document)
                    {
                        return document.canonical_path
                                == loaded[index].canonical_path
                            && document.writable
                                == request.paths[index].writable;
                    });
                same_paths = same_paths
                    && existing != current.documents.end();
            }
            if (same_paths)
            {
                return result(
                    ResultCode::success,
                    "The requested code workspace is already open.",
                    current.active);
            }
        }
        if (current.dirty())
            return result(ResultCode::dirty_workspace, "The current code workspace has unsaved documents.");

        Implementation replacement{};
        replacement.kind = request.kind;
        replacement.workspace_id = request.workspace_id;
        replacement.canonical_root = std::move(root);
        replacement.authority = request.authority;
        replacement.revision = current.revision + 1u;
        replacement.next_generation = current.next_generation;
        replacement.configured = true;
        replacement.documents.reserve(loaded.size());
        for (std::size_t index = 0u; index < loaded.size(); ++index)
        {
            const PathRequest& path_request = request.paths[index];
            LoadedFile& file = loaded[index];
            const DocumentHandle handle{
                .index = static_cast<std::uint32_t>(index),
                .generation = replacement.next_generation++};
            if (replacement.next_generation == 0u)
                replacement.next_generation = 1u;
            std::vector<std::size_t> starts = line_starts(file.text);
            replacement.documents.push_back(Implementation::Document{
                .handle = handle,
                .canonical_path = std::move(file.canonical_path),
                .relative_path = std::move(file.relative_path),
                .label = fs::path{path_request.relative_path}.filename().string(),
                .text = file.text,
                .persisted_text = std::move(file.text),
                .line_starts = std::move(starts),
                .revision = 1u,
                .persisted_revision = 1u,
                .selection = {},
                .viewport = {},
                .maximum_bytes = request.maximum_file_bytes,
                .writable = path_request.writable,
                .utf8_bom = file.utf8_bom});
        }
        replacement.active = replacement.documents.front().handle;
        *implementation_ = std::move(replacement);
        return result(
            ResultCode::success,
            "Opened the bounded code workspace.",
            implementation_->active);
    }

    OperationResult Controller::activate(
        const DocumentHandle document,
        const WorkspaceAuthority& expected)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (const OperationResult authority = validate_operation(*implementation_, expected);
            !authority)
        {
            return authority;
        }
        if (!implementation_->find(document))
            return result(ResultCode::stale_document, "The requested code tab is stale.");
        if (implementation_->active && *implementation_->active == document)
            return result(ResultCode::success, "The requested code tab is already active.", document);
        implementation_->active = document;
        ++implementation_->revision;
        return result(ResultCode::success, "Activated the requested code tab.", document);
    }

    OperationResult Controller::move_tab(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::size_t destination_index)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (const OperationResult authority = validate_operation(*implementation_, expected);
            !authority)
        {
            return authority;
        }
        const auto source = std::find_if(
            implementation_->documents.begin(), implementation_->documents.end(),
            [&](const Implementation::Document& document)
            {
                return document.handle == handle;
            });
        if (source == implementation_->documents.end())
            return result(ResultCode::stale_document, "The requested code tab is stale.");
        if (destination_index >= implementation_->documents.size())
            return result(ResultCode::invalid_request, "The code-tab destination is outside the open tab order.", handle);
        const std::size_t source_index = static_cast<std::size_t>(
            std::distance(implementation_->documents.begin(), source));
        if (source_index == destination_index)
            return result(ResultCode::success, "The code tab is already at the requested position.", handle);
        if (source_index < destination_index)
        {
            std::rotate(
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(source_index),
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(source_index + 1u),
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(destination_index + 1u));
        }
        else
        {
            std::rotate(
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(destination_index),
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(source_index),
                implementation_->documents.begin()
                    + static_cast<std::ptrdiff_t>(source_index + 1u));
        }
        ++implementation_->revision;
        return result(ResultCode::success, "Moved the code tab without changing its document handle.", handle);
    }

    OperationResult Controller::close(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const bool discard_dirty)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (checked->text != checked->persisted_text && !discard_dirty)
            return result(ResultCode::dirty_document, "Closing a modified code document requires explicit discard permission.", handle);

        const auto closing = std::find_if(
            implementation_->documents.begin(), implementation_->documents.end(),
            [&](const Implementation::Document& document)
            {
                return document.handle == handle;
            });
        const std::size_t closing_index = static_cast<std::size_t>(
            std::distance(implementation_->documents.begin(), closing));
        const bool closing_active = implementation_->active
            && *implementation_->active == handle;
        implementation_->documents.erase(closing);
        if (closing_active)
        {
            implementation_->active.reset();
        }
        if (closing_active && !implementation_->documents.empty())
        {
            const std::size_t next = (std::min)(
                closing_index,
                implementation_->documents.size() - 1u);
            implementation_->active = implementation_->documents[next].handle;
        }
        ++implementation_->revision;
        return result(ResultCode::success, "Closed the code document.", handle);
    }

    OperationResult Controller::replace_text(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::string text)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (text.size() + (checked->utf8_bom ? kUtf8Bom.size() : 0u)
            > checked->maximum_bytes)
            return result(ResultCode::oversized_file, "The edited code exceeds the workspace byte limit.", handle);
        if (!valid_utf8(text))
            return result(ResultCode::invalid_utf8, "The edited code is not valid UTF-8 text.", handle);

        Implementation::Document& document = *implementation_->find(handle);
        if (text == document.text)
            return result(ResultCode::success, "The code document is unchanged.", handle);
        document.text = std::move(text);
        document.line_starts = line_starts(document.text);
        ++document.revision;
        document.selection = {};
        document.find.current_match.reset();
        document.find.current_index = 0u;
        document.find.match_count = 0u;
        document.viewport = clamp_viewport(
            document.viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        return result(ResultCode::success, "Updated the in-memory code document.", handle);
    }

    OperationResult Controller::replace_range(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const TextRange range,
        std::string replacement)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (!valid_utf8(replacement))
            return result(ResultCode::invalid_utf8, "The replacement is not valid UTF-8 text.", handle);
        const auto anchor = byte_offset(checked->text, range.anchor);
        const auto caret = byte_offset(checked->text, range.caret);
        if (!anchor || !caret)
            return result(ResultCode::invalid_request, "The UTF-8 replacement range is outside the code document.", handle);
        const std::size_t begin = (std::min)(*anchor, *caret);
        const std::size_t end = (std::max)(*anchor, *caret);
        const std::size_t retained_size = checked->text.size()
            - (end - begin);
        const std::size_t maximum_text = checked->maximum_bytes
            - (checked->utf8_bom ? kUtf8Bom.size() : 0u);
        if (replacement.size() > maximum_text - retained_size)
        {
            return result(ResultCode::oversized_file, "The edited code exceeds the workspace byte limit.", handle);
        }
        const std::size_t new_size = retained_size + replacement.size();

        Implementation::Document& document = *implementation_->find(handle);
        std::string updated{};
        updated.reserve(new_size);
        updated.append(document.text, 0u, begin);
        updated.append(replacement);
        updated.append(document.text, end, std::string::npos);
        if (updated == document.text)
            return result(ResultCode::success, "The UTF-8 code range is unchanged.", handle);
        const std::size_t caret_byte = begin + replacement.size();
        document.text = std::move(updated);
        document.line_starts = line_starts(document.text);
        ++document.revision;
        const TextPosition position = text_position(document.text, caret_byte);
        document.selection = TextRange{.anchor = position, .caret = position};
        document.find.current_match.reset();
        document.find.current_index = 0u;
        document.find.match_count = 0u;
        reveal_range(
            document.viewport,
            document.selection,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Replaced the selected UTF-8 code range.",
            handle);
        output.range = document.selection;
        output.affected_count = 1u;
        return output;
    }

    OperationResult Controller::set_selection(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const TextRange selection)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!byte_offset(checked->text, selection.anchor)
            || !byte_offset(checked->text, selection.caret))
        {
            return result(ResultCode::invalid_request, "The UTF-8 selection is outside the code document.", handle);
        }
        Implementation::Document& document = *implementation_->find(handle);
        document.selection = selection;
        if (document.find.current_match
            && !same_range(*document.find.current_match, selection))
        {
            document.find.current_match.reset();
            document.find.current_index = 0u;
        }
        return result(ResultCode::success, "Updated the code selection.", handle);
    }

    OperationResult Controller::copy_selection(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision) const
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* document{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, document);
            !validation)
        {
            return validation;
        }
        const auto anchor = byte_offset(document->text, document->selection.anchor);
        const auto caret = byte_offset(document->text, document->selection.caret);
        if (!anchor || !caret)
            return result(ResultCode::invalid_request, "The UTF-8 selection is outside the code document.", handle);
        const std::size_t begin = (std::min)(*anchor, *caret);
        const std::size_t end = (std::max)(*anchor, *caret);
        return result(
            ResultCode::success,
            "Copied the selected UTF-8 code range.",
            handle,
            document->text.substr(begin, end - begin));
    }

    OperationResult Controller::set_viewport(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const Viewport viewport)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        Implementation::Document& document = *implementation_->find(handle);
        document.viewport = clamp_viewport(
            viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        return result(ResultCode::success, "Clamped the independent code viewport.", handle);
    }

    OperationResult Controller::goto_line(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const std::size_t one_based_line)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (one_based_line == 0u)
            return result(ResultCode::invalid_request, "Go To Line uses one-based line numbers.", handle);
        Implementation::Document& document = *implementation_->find(handle);
        const std::size_t target = (std::min)(
            one_based_line - 1u,
            document.line_starts.size() - 1u);
        document.selection = TextRange{
            .anchor = {.line = target, .column = 0u},
            .caret = {.line = target, .column = 0u}};
        document.find.current_match.reset();
        document.find.current_index = 0u;
        document.viewport.first_line = target;
        document.viewport.first_column = 0u;
        document.viewport = clamp_viewport(
            document.viewport,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        return result(ResultCode::success, "Moved to the requested code line.", handle);
    }

    OperationResult Controller::find(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::string query,
        const FindOptions options,
        const FindDirection direction)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (query.empty() || query.size() > kMaximumFindBytes)
            return result(ResultCode::invalid_request, "Find requires a non-empty bounded UTF-8 query.", handle);
        if (!valid_utf8(query))
            return result(ResultCode::invalid_utf8, "The find query is not valid UTF-8 text.", handle);

        const std::vector<ByteRange> matches = find_matches(
            checked->text, query, options);
        Implementation::Document& document = *implementation_->find(handle);
        const bool continuing = document.find.query == query
            && document.find.options == options
            && document.find.current_match
            && same_range(*document.find.current_match, document.selection);
        document.find.query = std::move(query);
        document.find.options = options;
        document.find.match_count = matches.size();
        document.find.current_match.reset();
        document.find.current_index = 0u;
        if (matches.empty())
        {
            ++implementation_->revision;
            return result(ResultCode::not_found, "The bounded code document contains no matching text.", handle);
        }

        const auto selection_anchor = byte_offset(
            document.text, document.selection.anchor);
        const auto selection_caret = byte_offset(
            document.text, document.selection.caret);
        if (!selection_anchor || !selection_caret)
            return result(ResultCode::invalid_request, "The current UTF-8 selection is outside the code document.", handle);
        const std::size_t selection_begin = (std::min)(
            *selection_anchor, *selection_caret);
        const std::size_t selection_end = (std::max)(
            *selection_anchor, *selection_caret);

        std::optional<std::size_t> selected_index{};
        bool wrapped{};
        if (direction == FindDirection::forward)
        {
            const std::size_t threshold = continuing
                ? selection_end
                : *selection_caret;
            for (std::size_t index = 0u; index < matches.size(); ++index)
            {
                if (matches[index].begin >= threshold)
                {
                    selected_index = index;
                    break;
                }
            }
            if (!selected_index && options.wrap)
            {
                selected_index = 0u;
                wrapped = true;
            }
        }
        else
        {
            const std::size_t threshold = continuing
                ? selection_begin
                : *selection_caret;
            for (std::size_t index = matches.size(); index > 0u; --index)
            {
                if (matches[index - 1u].end <= threshold)
                {
                    selected_index = index - 1u;
                    break;
                }
            }
            if (!selected_index && options.wrap)
            {
                selected_index = matches.size() - 1u;
                wrapped = true;
            }
        }
        if (!selected_index)
        {
            ++implementation_->revision;
            return result(ResultCode::not_found, "No further match exists in the requested direction.", handle);
        }

        const TextRange selected = text_range(
            document.text, matches[*selected_index]);
        document.selection = selected;
        document.find.current_match = selected;
        document.find.current_index = *selected_index + 1u;
        reveal_range(
            document.viewport,
            selected,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Selected a verified UTF-8 find match.",
            handle);
        output.range = selected;
        output.match_index = document.find.current_index;
        output.match_count = document.find.match_count;
        output.wrapped = wrapped;
        return output;
    }

    OperationResult Controller::replace_current(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::string replacement)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (!valid_utf8(replacement))
            return result(ResultCode::invalid_utf8, "The replacement is not valid UTF-8 text.", handle);
        if (checked->find.query.empty() || !checked->find.current_match
            || !same_range(*checked->find.current_match, checked->selection))
        {
            return result(ResultCode::not_found, "Replace Current requires the active verified find match.", handle);
        }
        const auto anchor = byte_offset(
            checked->text, checked->find.current_match->anchor);
        const auto caret = byte_offset(
            checked->text, checked->find.current_match->caret);
        if (!anchor || !caret)
            return result(ResultCode::stale_document, "The active find match is stale.", handle);
        const std::size_t begin = (std::min)(*anchor, *caret);
        const std::size_t end = (std::max)(*anchor, *caret);
        if (end - begin != checked->find.query.size()
            || !match_at(
                checked->text,
                checked->find.query,
                begin,
                checked->find.options))
        {
            return result(ResultCode::stale_document, "The active find match no longer identifies the expected text.", handle);
        }
        const std::size_t retained_size = checked->text.size() - (end - begin);
        if (replacement.size() > checked->maximum_bytes -
                (std::min)(checked->maximum_bytes, retained_size
                    + (checked->utf8_bom ? kUtf8Bom.size() : 0u)))
        {
            return result(ResultCode::oversized_file, "The replacement exceeds the workspace byte limit.", handle);
        }

        Implementation::Document& document = *implementation_->find(handle);
        const std::string query = document.find.query;
        const FindOptions find_options = document.find.options;
        std::string updated{};
        updated.reserve(retained_size + replacement.size());
        updated.append(document.text, 0u, begin);
        updated.append(replacement);
        updated.append(document.text, end, std::string::npos);
        const std::size_t caret_byte = begin + replacement.size();
        document.text = std::move(updated);
        document.line_starts = line_starts(document.text);
        ++document.revision;
        const TextPosition position = text_position(document.text, caret_byte);
        document.selection = TextRange{.anchor = position, .caret = position};
        document.find = FindSnapshot{
            .query = query,
            .options = find_options,
            .current_match = std::nullopt,
            .current_index = 0u,
            .match_count = find_matches(
                document.text, query, find_options).size()};
        reveal_range(
            document.viewport,
            document.selection,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Replaced the active verified find match.",
            handle);
        output.range = document.selection;
        output.affected_count = 1u;
        output.match_count = document.find.match_count;
        return output;
    }

    OperationResult Controller::replace_all(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::string query,
        const FindOptions options,
        std::string replacement)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (query.empty() || query.size() > kMaximumFindBytes)
            return result(ResultCode::invalid_request, "Replace All requires a non-empty bounded UTF-8 query.", handle);
        if (!valid_utf8(query) || !valid_utf8(replacement))
            return result(ResultCode::invalid_utf8, "Replace All requires valid UTF-8 query and replacement text.", handle);
        const std::vector<ByteRange> matches = find_matches(
            checked->text, query, options);
        if (matches.empty())
            return result(ResultCode::not_found, "The bounded code document contains no matching text.", handle);
        if (matches.size() == kMaximumFindMatches)
        {
            std::size_t offset = matches.back().end;
            while (offset + query.size() <= checked->text.size())
            {
                if (match_at(checked->text, query, offset, options))
                    return result(ResultCode::invalid_request, "Replace All exceeds the bounded match count; no text was changed.", handle);
                offset = next_codepoint(checked->text, offset);
            }
        }
        const std::size_t removed = matches.size() * query.size();
        const std::size_t retained = checked->text.size() - removed;
        const std::size_t maximum_text = checked->maximum_bytes
            - (checked->utf8_bom ? kUtf8Bom.size() : 0u);
        if (replacement.size() != 0u
            && matches.size() > (maximum_text - (std::min)(maximum_text, retained))
                / replacement.size())
        {
            return result(ResultCode::oversized_file, "Replace All exceeds the workspace byte limit; no text was changed.", handle);
        }
        const std::size_t updated_size = retained
            + matches.size() * replacement.size();
        if (updated_size > maximum_text)
            return result(ResultCode::oversized_file, "Replace All exceeds the workspace byte limit; no text was changed.", handle);

        std::string updated{};
        updated.reserve(updated_size);
        std::size_t copied{};
        std::size_t final_caret{};
        for (const ByteRange match : matches)
        {
            updated.append(checked->text, copied, match.begin - copied);
            updated.append(replacement);
            final_caret = updated.size();
            copied = match.end;
        }
        updated.append(checked->text, copied, std::string::npos);

        Implementation::Document& document = *implementation_->find(handle);
        document.text = std::move(updated);
        document.line_starts = line_starts(document.text);
        ++document.revision;
        const TextPosition position = text_position(document.text, final_caret);
        document.selection = TextRange{.anchor = position, .caret = position};
        document.find = FindSnapshot{
            .query = std::move(query),
            .options = options,
            .current_match = std::nullopt,
            .current_index = 0u,
            .match_count = 0u};
        document.find.match_count = find_matches(
            document.text, document.find.query, options).size();
        reveal_range(
            document.viewport,
            document.selection,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Replaced every bounded verified UTF-8 match.",
            handle);
        output.range = document.selection;
        output.affected_count = matches.size();
        output.match_count = document.find.match_count;
        return output;
    }

    OperationResult Controller::revert(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (checked->text == checked->persisted_text)
            return result(ResultCode::success, "The code document has no in-memory edits to revert.", handle);
        Implementation::Document& document = *implementation_->find(handle);
        document.text = document.persisted_text;
        document.line_starts = line_starts(document.text);
        ++document.revision;
        document.selection = {};
        document.viewport = {};
        document.find.current_match.reset();
        document.find.current_index = 0u;
        document.find.match_count = document.find.query.empty()
            ? 0u
            : find_matches(
                document.text,
                document.find.query,
                document.find.options).size();
        ++implementation_->revision;
        return result(ResultCode::success, "Reverted the in-memory code document to its last verified saved bytes.", handle);
    }

    OperationResult Controller::reload(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const bool discard_dirty)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (checked->text != checked->persisted_text && !discard_dirty)
            return result(ResultCode::dirty_document, "Reload requires explicit permission to discard local edits.", handle);

        LoadedFile loaded{};
        std::string reason{};
        const ResultCode code = resolve_file(
            implementation_->canonical_root,
            checked->relative_path,
            checked->maximum_bytes,
            loaded,
            reason);
        if (code != ResultCode::success)
            return result(code, std::move(reason), handle);

        Implementation::Document& document = *implementation_->find(handle);
        if (loaded.canonical_path != document.canonical_path)
            return result(ResultCode::stale_disk, "The code file identity changed before reload.", handle);
        document.text = std::move(loaded.text);
        document.persisted_text = document.text;
        document.utf8_bom = loaded.utf8_bom;
        document.line_starts = line_starts(document.text);
        ++document.revision;
        document.persisted_revision = document.revision;
        document.selection = {};
        document.viewport = {};
        document.find.current_match.reset();
        document.find.current_index = 0u;
        document.find.match_count = document.find.query.empty()
            ? 0u
            : find_matches(
                document.text,
                document.find.query,
                document.find.options).size();
        ++implementation_->revision;
        return result(ResultCode::success, "Reloaded the verified code document.", handle);
    }

    OperationResult Controller::save(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (!checked->writable)
            return result(ResultCode::read_only, "The reviewed code document is read-only.", handle);
        if (checked->text == checked->persisted_text)
            return result(ResultCode::success, "The code document is already saved.", handle);

        LoadedFile current{};
        std::string reason{};
        ResultCode code = resolve_file(
            implementation_->canonical_root,
            checked->relative_path,
            checked->maximum_bytes,
            current,
            reason);
        if (code != ResultCode::success)
            return result(code, std::move(reason), handle);
        if (current.canonical_path != checked->canonical_path
            || current.text != checked->persisted_text
            || current.utf8_bom != checked->utf8_bom)
        {
            return result(ResultCode::stale_disk, "The code file changed on disk after it was opened.", handle);
        }

        const fs::path temporary = checked->canonical_path.parent_path()
            / (checked->canonical_path.filename().string()
                + ".epoch-workspace-"
                + std::to_string(handle.generation)
                + "-" + std::to_string(checked->revision) + ".tmp");
        std::error_code error{};
        fs::remove(temporary, error);
        error.clear();
        const std::string encoded = encoded_source_bytes(
            checked->text, checked->utf8_bom);
        const std::span<const char> characters{
            encoded.data(), encoded.size()};
        if (!platform::filesystem::exclusive_create_and_write(
                temporary, std::as_bytes(characters), error))
        {
            return result(ResultCode::write_failed, "The verified temporary code file could not be written.", handle);
        }

        std::string temporary_bytes{};
        if (!read_exact(temporary, temporary_bytes)
            || temporary_bytes != encoded)
        {
            fs::remove(temporary, error);
            return result(ResultCode::verification_failed, "The temporary code file failed byte verification.", handle);
        }

        std::string precommit_bytes{};
        const std::string expected_persisted = encoded_source_bytes(
            checked->persisted_text, checked->utf8_bom);
        if (!read_exact(checked->canonical_path, precommit_bytes)
            || precommit_bytes != expected_persisted)
        {
            fs::remove(temporary, error);
            return result(ResultCode::stale_disk, "The code file changed immediately before atomic replacement.", handle);
        }

        if (!platform::filesystem::atomic_replace_same_filesystem(
                temporary, checked->canonical_path, error))
        {
            fs::remove(temporary, error);
            return result(ResultCode::write_failed, "Atomic code-file replacement failed.", handle);
        }

        std::string committed{};
        if (!read_exact(checked->canonical_path, committed)
            || committed != encoded)
        {
            return result(ResultCode::verification_failed, "The committed code file failed byte verification.", handle);
        }

        Implementation::Document& document = *implementation_->find(handle);
        document.persisted_text = document.text;
        document.persisted_revision = document.revision;
        ++implementation_->revision;
        return result(ResultCode::success, "Saved the code document by verified atomic replacement.", handle);
    }

    OperationResult Controller::publish_diagnostics(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        std::vector<Diagnostic> diagnostics)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (diagnostics.size() > kMaximumDiagnostics)
            return result(ResultCode::invalid_request, "The diagnostic publication exceeds its bounded entry count.", handle);
        for (const Diagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.source.size() > kMaximumDiagnosticSourceBytes
                || diagnostic.code.size() > kMaximumDiagnosticCodeBytes
                || diagnostic.message.empty()
                || diagnostic.message.size() > kMaximumDiagnosticMessageBytes
                || !valid_utf8(diagnostic.source)
                || !valid_utf8(diagnostic.code)
                || !valid_utf8(diagnostic.message)
                || !byte_offset(checked->text, diagnostic.range.anchor)
                || !byte_offset(checked->text, diagnostic.range.caret)
                || diagnostic.severity < DiagnosticSeverity::information
                || diagnostic.severity > DiagnosticSeverity::error)
            {
                return result(ResultCode::invalid_request, "A diagnostic entry is malformed or outside the exact document revision.", handle);
            }
        }
        std::stable_sort(
            diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& left, const Diagnostic& right)
            {
                const auto position_less = [](
                    const TextPosition first,
                    const TextPosition second) noexcept
                {
                    return first.line != second.line
                        ? first.line < second.line
                        : first.column < second.column;
                };
                const TextPosition left_begin = position_less(
                    left.range.caret, left.range.anchor)
                    ? left.range.caret : left.range.anchor;
                const TextPosition right_begin = position_less(
                    right.range.caret, right.range.anchor)
                    ? right.range.caret : right.range.anchor;
                if (left_begin != right_begin)
                    return position_less(left_begin, right_begin);
                const TextPosition left_end = position_less(
                    left.range.anchor, left.range.caret)
                    ? left.range.caret : left.range.anchor;
                const TextPosition right_end = position_less(
                    right.range.anchor, right.range.caret)
                    ? right.range.caret : right.range.anchor;
                if (left_end != right_end)
                    return position_less(left_end, right_end);
                if (left.severity != right.severity)
                    return left.severity > right.severity;
                if (left.source != right.source)
                    return left.source < right.source;
                if (left.code != right.code)
                    return left.code < right.code;
                return left.message < right.message;
            });

        Implementation::Document& document = *implementation_->find(handle);
        document.diagnostics = std::move(diagnostics);
        document.diagnostics_revision = document.revision;
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Published externally supplied diagnostics for the exact code revision.",
            handle);
        output.affected_count = document.diagnostics.size();
        return output;
    }

    OperationResult Controller::navigate_diagnostic(
        const DocumentHandle handle,
        const WorkspaceAuthority& expected,
        const std::uint64_t expected_revision,
        const std::size_t diagnostic_index)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        const Implementation::Document* checked{};
        if (const OperationResult validation = validate_document_operation(
                *implementation_, handle, expected, expected_revision, checked);
            !validation)
        {
            return validation;
        }
        if (checked->diagnostics_revision != checked->revision)
            return result(ResultCode::stale_diagnostics, "The published diagnostics do not match the active code revision.", handle);
        if (diagnostic_index >= checked->diagnostics.size())
            return result(ResultCode::invalid_request, "The diagnostic index is outside the published evidence set.", handle);
        Implementation::Document& document = *implementation_->find(handle);
        document.selection = document.diagnostics[diagnostic_index].range;
        reveal_range(
            document.viewport,
            document.selection,
            document.line_starts.size(),
            maximum_columns(document.text, document.line_starts));
        ++implementation_->revision;
        OperationResult output = result(
            ResultCode::success,
            "Navigated to an externally supplied diagnostic.",
            handle);
        output.range = document.selection;
        output.affected_count = 1u;
        return output;
    }

    std::string Controller::serialize_session() const
    {
        if (!implementation_ || !implementation_->configured)
            return {};
        std::string active_path{};
        if (implementation_->active)
        {
            if (const Implementation::Document* active =
                    implementation_->find(*implementation_->active))
            {
                active_path = active->relative_path;
            }
        }
        std::string encoded{};
        encoded.reserve(256u + implementation_->documents.size() * 160u);
        encoded.append(kSessionHeader);
        encoded.append("\nworkspace ");
        encoded.append(hex_encode(implementation_->workspace_id));
        encoded.append("\nactive ");
        encoded.append(hex_encode(active_path));
        encoded.append("\ndocuments ");
        encoded.append(std::to_string(implementation_->documents.size()));
        encoded.push_back('\n');
        for (const Implementation::Document& document :
             implementation_->documents)
        {
            encoded.append("document ");
            encoded.append(hex_encode(document.relative_path));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.selection.anchor.line));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.selection.anchor.column));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.selection.caret.line));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.selection.caret.column));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.viewport.first_line));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.viewport.first_column));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.viewport.visible_lines));
            encoded.push_back(' ');
            encoded.append(std::to_string(document.viewport.visible_columns));
            encoded.push_back(' ');
            encoded.append(hex_encode(document.find.query));
            encoded.push_back(' ');
            encoded.push_back(document.find.options.case_sensitive ? '1' : '0');
            encoded.push_back(' ');
            encoded.push_back(document.find.options.whole_identifier ? '1' : '0');
            encoded.push_back(' ');
            encoded.push_back(document.find.options.wrap ? '1' : '0');
            encoded.push_back('\n');
        }
        return encoded.size() <= kMaximumSessionBytes
            ? encoded
            : std::string{};
    }

    OperationResult Controller::restore_session(
        const WorkspaceAuthority& expected,
        const std::string_view encoded_session)
    {
        if (!implementation_)
            return result(ResultCode::unavailable, "The code-workspace controller is unavailable.");
        if (const OperationResult authority = validate_operation(
                *implementation_, expected);
            !authority)
        {
            return authority;
        }
        if (encoded_session.empty()
            || encoded_session.size() > kMaximumSessionBytes
            || encoded_session.find('\r') != std::string_view::npos)
        {
            return result(ResultCode::invalid_request, "The persisted code-workspace session is empty, oversized, or noncanonical.");
        }
        std::vector<std::string_view> lines{};
        std::size_t offset{};
        while (offset < encoded_session.size())
        {
            const std::size_t end = encoded_session.find('\n', offset);
            lines.push_back(encoded_session.substr(
                offset,
                end == std::string_view::npos
                    ? encoded_session.size() - offset
                    : end - offset));
            if (end == std::string_view::npos)
                break;
            offset = end + 1u;
        }
        if (!encoded_session.empty() && encoded_session.back() == '\n'
            && !lines.empty() && lines.back().empty())
        {
            lines.pop_back();
        }
        if (lines.size() < 4u || lines[0] != kSessionHeader)
            return result(ResultCode::invalid_request, "The persisted code-workspace session header is invalid.");
        const std::vector<std::string_view> workspace_tokens =
            split_tokens(lines[1]);
        const std::vector<std::string_view> active_tokens =
            split_tokens(lines[2]);
        const std::vector<std::string_view> count_tokens =
            split_tokens(lines[3]);
        if (workspace_tokens.size() != 2u
            || workspace_tokens[0] != "workspace"
            || active_tokens.size() != 2u
            || active_tokens[0] != "active"
            || count_tokens.size() != 2u
            || count_tokens[0] != "documents")
        {
            return result(ResultCode::invalid_request, "The persisted code-workspace session preamble is invalid.");
        }
        const auto workspace_id = hex_decode(workspace_tokens[1]);
        const auto active_path = hex_decode(active_tokens[1]);
        std::size_t document_count{};
        if (!workspace_id || !active_path
            || *workspace_id != implementation_->workspace_id
            || !parse_size(count_tokens[1], document_count)
            || document_count != implementation_->documents.size()
            || document_count > kAbsoluteMaximumTabs
            || lines.size() != 4u + document_count)
        {
            return result(ResultCode::invalid_request, "The persisted code-workspace identity or document catalog is stale.");
        }

        struct RestoredDocument final
        {
            std::size_t source_index{};
            TextRange selection{};
            Viewport viewport{};
            std::string query{};
            FindOptions options{};
        };
        std::vector<RestoredDocument> restored{};
        restored.reserve(document_count);
        std::vector<bool> seen(document_count, false);
        for (std::size_t line_index = 0u;
             line_index < document_count;
             ++line_index)
        {
            const std::vector<std::string_view> tokens = split_tokens(
                lines[4u + line_index]);
            if (tokens.size() != 14u || tokens[0] != "document")
                return result(ResultCode::invalid_request, "A persisted code-workspace document row is malformed.");
            const auto path = hex_decode(tokens[1]);
            const auto query = hex_decode(tokens[10]);
            std::size_t values[11]{};
            bool values_valid = path && query;
            for (std::size_t index = 0u; index < 8u && values_valid; ++index)
                values_valid = parse_size(tokens[2u + index], values[index]);
            for (std::size_t index = 0u; index < 3u && values_valid; ++index)
            {
                values_valid = parse_size(tokens[11u + index], values[8u + index])
                    && values[8u + index] <= 1u;
            }
            if (!values_valid || query->size() > kMaximumFindBytes)
                return result(ResultCode::invalid_request, "A persisted code-workspace document state is invalid.");
            const auto existing = std::find_if(
                implementation_->documents.begin(),
                implementation_->documents.end(),
                [&](const Implementation::Document& document)
                {
                    return document.relative_path == *path;
                });
            if (existing == implementation_->documents.end())
                return result(ResultCode::invalid_request, "A persisted code-workspace tab no longer exists in the admitted catalog.");
            const std::size_t source_index = static_cast<std::size_t>(
                std::distance(implementation_->documents.begin(), existing));
            if (seen[source_index])
                return result(ResultCode::duplicate_path, "The persisted code-workspace tab order contains a duplicate path.");
            seen[source_index] = true;
            const TextRange selection{
                .anchor = {.line = values[0], .column = values[1]},
                .caret = {.line = values[2], .column = values[3]}};
            if (!byte_offset(existing->text, selection.anchor)
                || !byte_offset(existing->text, selection.caret))
            {
                return result(ResultCode::invalid_request, "A persisted UTF-8 code selection is outside its current document.");
            }
            restored.push_back(RestoredDocument{
                .source_index = source_index,
                .selection = selection,
                .viewport = {
                    .first_line = values[4],
                    .first_column = values[5],
                    .visible_lines = values[6],
                    .visible_columns = values[7]},
                .query = std::move(*query),
                .options = {
                    .case_sensitive = values[8] != 0u,
                    .whole_identifier = values[9] != 0u,
                    .wrap = values[10] != 0u}});
        }
        if (std::any_of(seen.begin(), seen.end(), [](const bool value)
            {
                return !value;
            }))
        {
            return result(ResultCode::invalid_request, "The persisted code-workspace tab order is incomplete.");
        }
        if (!active_path->empty()
            && std::none_of(
                implementation_->documents.begin(),
                implementation_->documents.end(),
                [&](const Implementation::Document& document)
                {
                    return document.relative_path == *active_path;
                }))
        {
            return result(ResultCode::invalid_request, "The persisted active code tab is outside the admitted catalog.");
        }
        if (active_path->empty() && document_count != 0u)
            return result(ResultCode::invalid_request, "A non-empty persisted tab set requires one active code tab.");

        std::vector<Implementation::Document> reordered{};
        reordered.reserve(document_count);
        for (const RestoredDocument& state : restored)
        {
            Implementation::Document document = std::move(
                implementation_->documents[state.source_index]);
            document.selection = state.selection;
            document.viewport = clamp_viewport(
                state.viewport,
                document.line_starts.size(),
                maximum_columns(document.text, document.line_starts));
            document.find = FindSnapshot{
                .query = state.query,
                .options = state.options,
                .current_match = std::nullopt,
                .current_index = 0u,
                .match_count = state.query.empty()
                    ? 0u
                    : find_matches(
                        document.text,
                        state.query,
                        state.options).size()};
            reordered.push_back(std::move(document));
        }
        implementation_->documents = std::move(reordered);
        implementation_->active.reset();
        if (!active_path->empty())
        {
            const auto active = std::find_if(
                implementation_->documents.begin(),
                implementation_->documents.end(),
                [&](const Implementation::Document& document)
                {
                    return document.relative_path == *active_path;
                });
            implementation_->active = active->handle;
        }
        ++implementation_->revision;
        return result(
            ResultCode::success,
            "Restored deterministic tab, selection, viewport, and find state without restoring source bytes.",
            implementation_->active);
    }

    WorkspaceSnapshot Controller::snapshot() const
    {
        WorkspaceSnapshot output{};
        if (!implementation_)
            return output;
        output.kind = implementation_->kind;
        output.workspace_id = implementation_->workspace_id;
        output.canonical_root = implementation_->canonical_root.generic_string();
        output.authority = implementation_->authority;
        output.active_document = implementation_->active;
        output.revision = implementation_->revision;
        output.configured = implementation_->configured;
        output.dirty = implementation_->dirty();
        output.documents.reserve(implementation_->documents.size());
        for (const Implementation::Document& document : implementation_->documents)
            output.documents.push_back(make_snapshot(document, implementation_->active));
        return output;
    }

    std::optional<DocumentSnapshot> Controller::document(
        const DocumentHandle handle) const
    {
        if (!implementation_)
            return std::nullopt;
        const Implementation::Document* document = implementation_->find(handle);
        if (!document)
            return std::nullopt;
        return make_snapshot(*document, implementation_->active);
    }

    std::optional<DocumentSnapshot> Controller::active_document() const
    {
        if (!implementation_ || !implementation_->active)
            return std::nullopt;
        return document(*implementation_->active);
    }

    bool Controller::has_dirty_documents() const noexcept
    {
        return implementation_ && implementation_->dirty();
    }
}
