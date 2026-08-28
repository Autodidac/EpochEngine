/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_patch_bundle;

import core.sha256;

namespace epochengine::ai::source_patch_bundle
{
    namespace
    {
        struct TextLines final
        {
            std::vector<std::string> values{};
            LineEnding ending{LineEnding::none};
            bool final_newline{};
        };

        [[nodiscard]] bool valid_limits(const Limits& limits) noexcept
        {
            return limits.maximum_patch_bytes >= 256u
                && limits.maximum_files > 0u && limits.maximum_files <= 128u
                && limits.maximum_hunks_per_file > 0u
                && limits.maximum_hunks_per_file <= 1024u
                && limits.maximum_lines_per_hunk > 0u
                && limits.maximum_path_bytes > 0u
                && limits.maximum_file_bytes > 0u
                && limits.maximum_total_postimage_bytes
                    >= limits.maximum_file_bytes;
        }

        [[nodiscard]] bool valid_utf8(std::string_view value) noexcept
        {
            std::size_t index = 0u;
            while (index < value.size())
            {
                const std::uint8_t lead = static_cast<std::uint8_t>(value[index]);
                if (lead == 0u) return false;
                if (lead < 0x80u) { ++index; continue; }
                std::size_t continuation{};
                std::uint32_t codepoint{};
                if ((lead & 0xe0u) == 0xc0u)
                {
                    continuation = 1u;
                    codepoint = lead & 0x1fu;
                    if (codepoint < 2u) return false;
                }
                else if ((lead & 0xf0u) == 0xe0u)
                {
                    continuation = 2u;
                    codepoint = lead & 0x0fu;
                }
                else if ((lead & 0xf8u) == 0xf0u)
                {
                    continuation = 3u;
                    codepoint = lead & 0x07u;
                }
                else return false;
                if (index + continuation >= value.size()) return false;
                for (std::size_t offset = 1u; offset <= continuation; ++offset)
                {
                    const std::uint8_t byte =
                        static_cast<std::uint8_t>(value[index + offset]);
                    if ((byte & 0xc0u) != 0x80u) return false;
                    codepoint = (codepoint << 6u) | (byte & 0x3fu);
                }
                if ((continuation == 2u && codepoint < 0x800u)
                    || (continuation == 3u && codepoint < 0x10000u)
                    || (codepoint >= 0xd800u && codepoint <= 0xdfffu)
                    || codepoint > 0x10ffffu)
                {
                    return false;
                }
                index += continuation + 1u;
            }
            return true;
        }

        [[nodiscard]] bool valid_identifier(std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 128u) return false;
            for (const char character : value)
            {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (!((byte >= 'a' && byte <= 'z')
                    || (byte >= 'A' && byte <= 'Z')
                    || (byte >= '0' && byte <= '9')
                    || character == '.' || character == '_'
                    || character == '-')) return false;
            }
            return true;
        }

        [[nodiscard]] bool valid_path(
            std::string_view path,
            std::size_t maximum) noexcept
        {
            if (path.empty() || path.size() > maximum || path.front() == '/'
                || path.back() == '/' || path.find('\\') != path.npos
                || path.find(':') != path.npos) return false;
            std::size_t begin = 0u;
            while (begin < path.size())
            {
                const std::size_t end = path.find('/', begin);
                const std::string_view part = path.substr(
                    begin, end == path.npos ? path.size() - begin : end - begin);
                if (part.empty() || part == "." || part == ".."
                    || part.back() == '.' || part.back() == ' ') return false;
                for (const char character : part)
                {
                    const unsigned char byte = static_cast<unsigned char>(character);
                    if (byte < 0x21u || byte > 0x7eu
                        || character == '<' || character == '>'
                        || character == '|' || character == '"'
                        || character == '*' || character == '?') return false;
                }
                if (end == path.npos) break;
                begin = end + 1u;
            }
            return true;
        }

        [[nodiscard]] std::string folded_path(std::string_view path)
        {
            std::string result{path};
            for (char& character : result)
                if (character >= 'A' && character <= 'Z')
                    character = static_cast<char>(character - 'A' + 'a');
            return result;
        }

        [[nodiscard]] bool parse_u64(
            std::string_view text,
            std::uint64_t& value) noexcept
        {
            if (text.empty()) return false;
            const char* first = text.data();
            const char* last = first + text.size();
            const auto result = std::from_chars(first, last, value);
            return result.ec == std::errc{} && result.ptr == last;
        }

        [[nodiscard]] bool parse_size(
            std::string_view text,
            std::size_t& value) noexcept
        {
            std::uint64_t parsed{};
            if (!parse_u64(text, parsed)
                || parsed > (std::numeric_limits<std::size_t>::max)())
                return false;
            value = static_cast<std::size_t>(parsed);
            return true;
        }

        [[nodiscard]] std::uint8_t hex_nibble(char character) noexcept
        {
            if (character >= '0' && character <= '9')
                return static_cast<std::uint8_t>(character - '0');
            if (character >= 'a' && character <= 'f')
                return static_cast<std::uint8_t>(character - 'a' + 10);
            return 0xffu;
        }

        [[nodiscard]] bool parse_digest(
            std::string_view text,
            Digest& value) noexcept
        {
            if (text.size() != 64u) return false;
            for (std::size_t index = 0u; index < value.bytes.size(); ++index)
            {
                const std::uint8_t high = hex_nibble(text[index * 2u]);
                const std::uint8_t low = hex_nibble(text[index * 2u + 1u]);
                if (high == 0xffu || low == 0xffu) return false;
                value.bytes[index] = static_cast<std::uint8_t>((high << 4u) | low);
            }
            return true;
        }

        [[nodiscard]] std::string_view operation_name(Operation value) noexcept
        {
            switch (value)
            {
            case Operation::add: return "add";
            case Operation::update: return "update";
            case Operation::remove: return "delete";
            }
            return "unknown";
        }

        [[nodiscard]] bool parse_operation(
            std::string_view text,
            Operation& value) noexcept
        {
            if (text == "add") value = Operation::add;
            else if (text == "update") value = Operation::update;
            else if (text == "delete") value = Operation::remove;
            else return false;
            return true;
        }

        [[nodiscard]] std::string_view ending_name(LineEnding value) noexcept
        {
            switch (value)
            {
            case LineEnding::none: return "none";
            case LineEnding::lf: return "lf";
            case LineEnding::crlf: return "crlf";
            }
            return "unknown";
        }

        [[nodiscard]] bool parse_ending(
            std::string_view text,
            LineEnding& value) noexcept
        {
            if (text == "none") value = LineEnding::none;
            else if (text == "lf") value = LineEnding::lf;
            else if (text == "crlf") value = LineEnding::crlf;
            else return false;
            return true;
        }

        [[nodiscard]] bool analyze_text(
            std::string_view bytes,
            TextLines& result) noexcept
        {
            if (!valid_utf8(bytes)) return false;
            bool saw_lf = false;
            bool saw_crlf = false;
            for (std::size_t index = 0u; index < bytes.size(); ++index)
            {
                if (bytes[index] == '\r')
                {
                    if (index + 1u >= bytes.size() || bytes[index + 1u] != '\n')
                        return false;
                    saw_crlf = true;
                    ++index;
                }
                else if (bytes[index] == '\n') saw_lf = true;
            }
            if (saw_lf && saw_crlf) return false;
            result.ending = saw_crlf ? LineEnding::crlf
                : saw_lf ? LineEnding::lf : LineEnding::none;
            const std::string_view separator = saw_crlf ? "\r\n" : "\n";
            if (!saw_lf && !saw_crlf)
            {
                if (!bytes.empty()) result.values.emplace_back(bytes);
                return true;
            }
            result.final_newline = bytes.ends_with(separator);
            std::size_t begin = 0u;
            while (begin < bytes.size())
            {
                const std::size_t end = bytes.find(separator, begin);
                if (end == bytes.npos)
                {
                    result.values.emplace_back(bytes.substr(begin));
                    break;
                }
                result.values.emplace_back(bytes.substr(begin, end - begin));
                begin = end + separator.size();
                if (begin == bytes.size()) break;
            }
            return true;
        }

        [[nodiscard]] bool build_text(
            const std::vector<std::string>& lines,
            LineEnding ending,
            bool final_newline,
            std::string& bytes)
        {
            if (ending == LineEnding::none)
            {
                if (lines.size() > 1u || final_newline) return false;
                bytes = lines.empty() ? std::string{} : lines.front();
                return true;
            }
            const std::string_view separator =
                ending == LineEnding::lf ? "\n" : "\r\n";
            for (std::size_t index = 0u; index < lines.size(); ++index)
            {
                if (index != 0u) bytes.append(separator);
                bytes += lines[index];
            }
            if (final_newline) bytes.append(separator);
            return true;
        }

        [[nodiscard]] FileState make_state(
            bool exists,
            std::string_view bytes,
            LineEnding ending,
            bool final_newline) noexcept
        {
            return {
                .exists = exists,
                .digest = digest(bytes),
                .byte_count = static_cast<std::uint64_t>(bytes.size()),
                .encoding = Encoding::utf8,
                .line_ending = ending,
                .final_newline = final_newline};
        }

        [[nodiscard]] bool state_matches_bytes(
            const FileState& state,
            std::string_view bytes) noexcept
        {
            if (!state.exists)
                return bytes.empty() && state.byte_count == 0u
                    && state.digest == digest({})
                    && state.line_ending == LineEnding::none
                    && !state.final_newline;
            TextLines text{};
            return analyze_text(bytes, text)
                && state.digest == digest(bytes)
                && state.byte_count == bytes.size()
                && state.encoding == Encoding::utf8
                && state.line_ending == text.ending
                && state.final_newline == text.final_newline;
        }

        [[nodiscard]] bool read_field(
            std::span<const std::string_view> lines,
            std::size_t& cursor,
            std::string_view key,
            std::string_view& value) noexcept
        {
            if (cursor >= lines.size() || !lines[cursor].starts_with(key)
                || lines[cursor].size() <= key.size()
                || lines[cursor][key.size()] != ' ') return false;
            value = lines[cursor].substr(key.size() + 1u);
            ++cursor;
            return !value.empty();
        }

        [[nodiscard]] bool parse_hunk_header(
            std::string_view line,
            Hunk& hunk) noexcept
        {
            if (!line.starts_with("@@ -") || !line.ends_with(" @@")) return false;
            const std::size_t comma_old = line.find(',', 4u);
            const std::size_t plus = line.find(" +", comma_old);
            const std::size_t comma_new = line.find(',', plus == line.npos ? 0u : plus + 2u);
            if (comma_old == line.npos || plus == line.npos
                || comma_new == line.npos) return false;
            return parse_u64(line.substr(4u, comma_old - 4u), hunk.old_start)
                && parse_u64(line.substr(comma_old + 1u, plus - comma_old - 1u), hunk.old_count)
                && parse_u64(line.substr(plus + 2u, comma_new - plus - 2u), hunk.new_start)
                && parse_u64(line.substr(comma_new + 1u,
                    line.size() - comma_new - 4u), hunk.new_count)
                && ((hunk.old_count == 0u && hunk.old_start == 0u)
                    || (hunk.old_count != 0u && hunk.old_start != 0u))
                && ((hunk.new_count == 0u && hunk.new_start == 0u)
                    || (hunk.new_count != 0u && hunk.new_start != 0u));
        }

        [[nodiscard]] std::string encode_hunk(const Hunk& hunk)
        {
            std::string result = "@@ -" + std::to_string(hunk.old_start)
                + "," + std::to_string(hunk.old_count) + " +"
                + std::to_string(hunk.new_start) + ","
                + std::to_string(hunk.new_count) + " @@\n";
            for (const HunkLine& line : hunk.lines)
            {
                result.push_back(line.kind == HunkLineKind::context ? ' '
                    : line.kind == HunkLineKind::remove ? '-' : '+');
                result += line.bytes;
                result.push_back('\n');
            }
            return result;
        }

        [[nodiscard]] const CuratedFile* find_curated(
            std::span<const CuratedFile> curated,
            std::string_view path) noexcept
        {
            for (const CuratedFile& file : curated)
                if (file.relative_path == path) return &file;
            return nullptr;
        }

        [[nodiscard]] DecodeResult failure(
            DecodeCode code,
            std::size_t line,
            std::string status)
        {
            return {.code = code, .status = std::move(status), .line = line};
        }
    }

    Digest digest(std::string_view bytes) noexcept
    {
        return {core::sha256::hash(bytes).bytes};
    }

    std::string digest_hex(Digest value)
    {
        static constexpr char symbols[] = "0123456789abcdef";
        std::string result(value.bytes.size() * 2u, '0');
        for (std::size_t index = 0u; index < value.bytes.size(); ++index)
        {
            result[index * 2u] = symbols[value.bytes[index] >> 4u];
            result[index * 2u + 1u] = symbols[value.bytes[index] & 0x0fu];
        }
        return result;
    }

    DecodeResult decode(
        std::string_view canonical_diff,
        std::span<const CuratedFile> curated,
        Limits limits)
    {
        if (!valid_limits(limits))
            return failure(DecodeCode::invalid_limits, 0u, "Invalid patch limits.");
        if (canonical_diff.empty())
            return failure(DecodeCode::empty_input, 0u, "Patch bundle is empty.");
        if (canonical_diff.size() > limits.maximum_patch_bytes)
            return failure(DecodeCode::size_limit_exceeded, 0u, "Patch bundle exceeds its byte budget.");
        if (!valid_utf8(canonical_diff))
            return failure(DecodeCode::invalid_utf8, 0u, "Patch bundle is not canonical UTF-8 text.");
        if (!canonical_diff.ends_with('\n') || canonical_diff.find('\r') != canonical_diff.npos)
            return failure(DecodeCode::invalid_header, 1u, "Patch transport must use canonical LF lines and a final newline.");

        std::vector<std::string_view> lines{};
        std::size_t begin = 0u;
        while (begin < canonical_diff.size())
        {
            const std::size_t end = canonical_diff.find('\n', begin);
            lines.push_back(canonical_diff.substr(begin, end - begin));
            begin = end + 1u;
        }
        std::size_t cursor{};
        if (lines.empty() || lines[cursor++] != "EPOCH_SOURCE_PATCH_BUNDLE_V1")
            return failure(DecodeCode::invalid_header, 1u, "Missing patch bundle schema header.");

        Bundle bundle{};
        bundle.canonical_diff = std::string{canonical_diff};
        std::string_view field{};
        if (!read_field(lines, cursor, "bundle-id", field)
            || !valid_identifier(field))
            return failure(DecodeCode::invalid_metadata, cursor, "Invalid bundle identifier.");
        bundle.bundle_id = field;
        if (!read_field(lines, cursor, "authority-sha256", field)
            || !parse_digest(field, bundle.authority_digest)
            || !bundle.authority_digest.valid())
            return failure(DecodeCode::invalid_metadata, cursor, "Invalid authority digest.");
        std::size_t file_count{};
        if (!read_field(lines, cursor, "file-count", field)
            || !parse_size(field, file_count) || file_count == 0u
            || file_count > limits.maximum_files)
            return failure(DecodeCode::size_limit_exceeded, cursor, "Invalid file count.");

        std::vector<std::string> seen_paths{};
        std::uint64_t total_postimage{};
        for (std::size_t file_index = 0u; file_index < file_count; ++file_index)
        {
            const std::size_t first_line = cursor + 1u;
            if (cursor >= lines.size() || !lines[cursor].starts_with("diff --epoch "))
                return failure(DecodeCode::invalid_header, first_line, "Missing strict file diff header.");
            const std::string_view pair = lines[cursor++].substr(13u);
            const std::size_t separator = pair.find(' ');
            if (separator == pair.npos || pair.find(' ', separator + 1u) != pair.npos)
                return failure(DecodeCode::invalid_path, first_line, "Diff paths are malformed.");
            const std::string_view old_token = pair.substr(0u, separator);
            const std::string_view new_token = pair.substr(separator + 1u);

            FilePatch file{};
            if (!read_field(lines, cursor, "operation", field)
                || !parse_operation(field, file.operation))
                return failure(DecodeCode::unsupported_operation, cursor, "Only add, update, and delete are supported; rename is rejected.");
            const std::string_view path_token = file.operation == Operation::add
                ? new_token : old_token;
            if (path_token.size() <= 2u || !path_token.starts_with(file.operation == Operation::add ? "b/" : "a/"))
                return failure(DecodeCode::invalid_path, first_line, "Diff path prefix is invalid.");
            file.relative_path = path_token.substr(2u);
            if (!valid_path(file.relative_path, limits.maximum_path_bytes))
                return failure(DecodeCode::invalid_path, first_line, "Source path is not a canonical curated relative path.");
            if ((file.operation == Operation::add && old_token != "/dev/null")
                || (file.operation == Operation::remove && new_token != "/dev/null")
                || (file.operation == Operation::update
                    && (old_token != "a/" + file.relative_path
                        || new_token != "b/" + file.relative_path)))
                return failure(DecodeCode::unsupported_operation, first_line, "Rename or cross-path patch rejected.");
            const std::string folded = folded_path(file.relative_path);
            if (std::find(seen_paths.begin(), seen_paths.end(), folded) != seen_paths.end())
                return failure(DecodeCode::duplicate_path, first_line, "Duplicate case-folded source path.");
            seen_paths.push_back(folded);

            const CuratedFile* admitted = find_curated(curated, file.relative_path);
            if (admitted == nullptr)
                return failure(DecodeCode::uncurated_path, first_line, "Patch path is outside curated authority.");
            if (!state_matches_bytes(admitted->preimage, admitted->preimage_bytes))
                return failure(DecodeCode::preimage_mismatch, first_line, "Curated preimage metadata does not match its exact bytes.");
            if ((file.operation == Operation::add
                    && (admitted->preimage.exists || !admitted->allow_create))
                || (file.operation != Operation::add && !admitted->preimage.exists)
                || (file.operation == Operation::remove && !admitted->allow_remove))
                return failure(DecodeCode::uncurated_path, first_line, "Operation is not admitted by curated authority.");

            Digest declared_digest{};
            std::uint64_t declared_bytes{};
            LineEnding declared_ending{};
            bool declared_final{};
            LineEnding target_ending{};
            bool target_final{};
            if (!read_field(lines, cursor, "preimage-sha256", field)
                || !parse_digest(field, declared_digest)
                || !read_field(lines, cursor, "preimage-bytes", field)
                || !parse_u64(field, declared_bytes)
                || !read_field(lines, cursor, "preimage-encoding", field)
                || field != "utf-8"
                || !read_field(lines, cursor, "preimage-line-ending", field)
                || !parse_ending(field, declared_ending)
                || !read_field(lines, cursor, "preimage-final-newline", field)
                || (field != "0" && field != "1"))
                return failure(DecodeCode::invalid_metadata, cursor, "Invalid preimage binding metadata.");
            declared_final = field == "1";
            if (!read_field(lines, cursor, "postimage-line-ending", field)
                || !parse_ending(field, target_ending)
                || !read_field(lines, cursor, "postimage-final-newline", field)
                || (field != "0" && field != "1"))
                return failure(DecodeCode::invalid_metadata, cursor, "Invalid postimage text metadata.");
            target_final = field == "1";
            if (declared_digest != admitted->preimage.digest
                || declared_bytes != admitted->preimage.byte_count
                || declared_ending != admitted->preimage.line_ending
                || declared_final != admitted->preimage.final_newline)
                return failure(DecodeCode::preimage_mismatch, first_line, "Patch preimage binding is stale or forged.");
            if (file.operation == Operation::remove
                    ? (target_ending != LineEnding::none || target_final)
                    : false)
                return failure(DecodeCode::invalid_metadata, cursor, "Deleted postimage must be empty.");

            if (cursor + 1u >= lines.size())
                return failure(DecodeCode::invalid_header, cursor + 1u, "Missing unified file markers.");
            const std::string expected_old = file.operation == Operation::add
                ? "--- /dev/null" : "--- a/" + file.relative_path;
            const std::string expected_new_marker = file.operation == Operation::remove
                ? "+++ /dev/null" : "+++ b/" + file.relative_path;
            if (lines[cursor++] != expected_old
                || lines[cursor++] != expected_new_marker)
                return failure(DecodeCode::invalid_header, cursor - 1u, "Unified file markers do not match curated path.");

            TextLines old_text{};
            if (!analyze_text(admitted->preimage_bytes, old_text))
                return failure(DecodeCode::binary_rejected, first_line, "Binary, NUL, invalid UTF-8, or mixed line endings rejected.");
            std::vector<std::string> output{};
            std::size_t old_cursor{};
            std::uint64_t expected_new_start = 1u;
            std::uint64_t prior_zero_start = (std::numeric_limits<std::uint64_t>::max)();
            while (cursor < lines.size() && lines[cursor].starts_with("@@ "))
            {
                if (file.hunks.size() >= limits.maximum_hunks_per_file)
                    return failure(DecodeCode::size_limit_exceeded, cursor + 1u, "Hunk budget exceeded.");
                Hunk hunk{};
                if (!parse_hunk_header(lines[cursor++], hunk))
                    return failure(DecodeCode::malformed_hunk, cursor, "Malformed strict hunk header.");
                const std::size_t hunk_old = hunk.old_start == 0u
                    ? 0u : static_cast<std::size_t>(hunk.old_start - 1u);
                if (hunk_old < old_cursor || hunk_old > old_text.values.size()
                    || (hunk.old_count == 0u && prior_zero_start == hunk.old_start))
                    return failure(DecodeCode::overlapping_hunk, cursor, "Overlapping or duplicate hunk rejected.");
                if (hunk.old_count == 0u) prior_zero_start = hunk.old_start;
                output.insert(output.end(), old_text.values.begin() + old_cursor,
                    old_text.values.begin() + hunk_old);
                old_cursor = hunk_old;
                const std::size_t expected_new = output.size() + 1u;
                if (hunk.new_count != 0u && hunk.new_start != expected_new)
                    return failure(DecodeCode::ambiguous_hunk, cursor, "Hunk new coordinate is not deterministic.");
                std::uint64_t old_seen{};
                std::uint64_t new_seen{};
                bool changed{};
                while (cursor < lines.size() && !lines[cursor].starts_with("@@ ")
                    && !lines[cursor].starts_with("diff --epoch "))
                {
                    if (hunk.lines.size() >= limits.maximum_lines_per_hunk || lines[cursor].empty())
                        return failure(lines[cursor].empty() ? DecodeCode::malformed_hunk : DecodeCode::size_limit_exceeded,
                            cursor + 1u, "Invalid or oversized hunk body.");
                    const char prefix = lines[cursor].front();
                    if (prefix != ' ' && prefix != '-' && prefix != '+')
                        return failure(DecodeCode::malformed_hunk, cursor + 1u, "Unsupported unified-diff line marker.");
                    HunkLine parsed{.kind = prefix == ' ' ? HunkLineKind::context
                        : prefix == '-' ? HunkLineKind::remove : HunkLineKind::add,
                        .bytes = std::string{lines[cursor].substr(1u)}};
                    if (!valid_utf8(parsed.bytes))
                        return failure(DecodeCode::invalid_utf8, cursor + 1u, "Hunk line is not valid UTF-8.");
                    if (parsed.kind != HunkLineKind::add)
                    {
                        if (old_cursor >= old_text.values.size()
                            || old_text.values[old_cursor] != parsed.bytes)
                            return failure(DecodeCode::content_mismatch, cursor + 1u, "Hunk context/removal does not match exact preimage.");
                        ++old_cursor;
                        ++old_seen;
                    }
                    if (parsed.kind != HunkLineKind::remove)
                    {
                        output.push_back(parsed.bytes);
                        ++new_seen;
                    }
                    changed = changed || parsed.kind != HunkLineKind::context;
                    hunk.lines.push_back(std::move(parsed));
                    ++cursor;
                }
                if (!changed || old_seen != hunk.old_count || new_seen != hunk.new_count)
                    return failure(DecodeCode::malformed_hunk, cursor, "Hunk counts or change body are invalid.");
                hunk.digest = digest(encode_hunk(hunk));
                file.hunks.push_back(std::move(hunk));
                expected_new_start = static_cast<std::uint64_t>(output.size() + 1u);
                (void)expected_new_start;
            }
            if (file.hunks.empty())
                return failure(DecodeCode::malformed_hunk, cursor + 1u, "Each file requires at least one hunk.");
            output.insert(output.end(), old_text.values.begin() + old_cursor, old_text.values.end());
            if (!build_text(output, target_ending, target_final, file.postimage_bytes))
                return failure(DecodeCode::invalid_metadata, first_line, "Postimage line-ending metadata cannot represent hunk output.");
            if (file.operation == Operation::add && file.postimage_bytes.empty())
                return failure(DecodeCode::ambiguous_hunk, first_line, "Empty-file add is intentionally unsupported.");
            if (file.operation == Operation::remove && !file.postimage_bytes.empty())
                return failure(DecodeCode::content_mismatch, first_line, "Delete hunk did not remove the complete file.");
            if (file.operation == Operation::update
                && file.postimage_bytes == admitted->preimage_bytes)
                return failure(DecodeCode::ambiguous_hunk, first_line, "No-op update rejected.");
            if (file.postimage_bytes.size() > limits.maximum_file_bytes
                || total_postimage > limits.maximum_total_postimage_bytes
                    - file.postimage_bytes.size())
                return failure(DecodeCode::size_limit_exceeded, first_line, "Postimage byte budget exceeded.");
            total_postimage += file.postimage_bytes.size();
            file.preimage = admitted->preimage;
            file.postimage = make_state(file.operation != Operation::remove,
                file.postimage_bytes, target_ending, target_final);
            std::string evidence = file.relative_path + "\n" + operation_name(file.operation).data()
                + "\n" + digest_hex(file.preimage.digest) + "\n"
                + digest_hex(file.postimage.digest);
            for (const Hunk& hunk : file.hunks) evidence += "\n" + digest_hex(hunk.digest);
            file.evidence_digest = digest(evidence);
            bundle.files.push_back(std::move(file));
        }
        if (cursor != lines.size())
            return failure(DecodeCode::trailing_data, cursor + 1u, "Unexpected data after declared files.");
        bundle.digest = digest(canonical_diff);
        return {.code = DecodeCode::none,
            .status = "Strict curated patch bundle decoded and postimages verified.",
            .bundle = std::move(bundle)};
    }

    std::string encode(const Bundle& bundle)
    {
        std::string result = "EPOCH_SOURCE_PATCH_BUNDLE_V1\nbundle-id "
            + bundle.bundle_id + "\nauthority-sha256 "
            + digest_hex(bundle.authority_digest) + "\nfile-count "
            + std::to_string(bundle.files.size()) + "\n";
        for (const FilePatch& file : bundle.files)
        {
            const std::string old_path = file.operation == Operation::add
                ? "/dev/null" : "a/" + file.relative_path;
            const std::string new_path = file.operation == Operation::remove
                ? "/dev/null" : "b/" + file.relative_path;
            result += "diff --epoch " + old_path + " " + new_path
                + "\noperation " + std::string{operation_name(file.operation)}
                + "\npreimage-sha256 " + digest_hex(file.preimage.digest)
                + "\npreimage-bytes " + std::to_string(file.preimage.byte_count)
                + "\npreimage-encoding utf-8\npreimage-line-ending "
                + std::string{ending_name(file.preimage.line_ending)}
                + "\npreimage-final-newline " + (file.preimage.final_newline ? "1" : "0")
                + "\npostimage-line-ending "
                + std::string{ending_name(file.postimage.line_ending)}
                + "\npostimage-final-newline " + (file.postimage.final_newline ? "1" : "0")
                + "\n--- " + old_path + "\n+++ " + new_path + "\n";
            for (const Hunk& hunk : file.hunks) result += encode_hunk(hunk);
        }
        return result;
    }

    ApplyResult Executor::apply(
        const Bundle& bundle,
        const ApplyPermit& permit,
        SandboxFilePort& port)
    {
        ApplyResult result{};
        if (!permit.operator_approved || !valid_identifier(permit.receipt_id)
            || permit.bundle_digest != bundle.digest
            || permit.authority_digest != bundle.authority_digest
            || bundle.digest != digest(encode(bundle)))
        {
            result.status = "Host approval does not bind this exact canonical bundle.";
            return result;
        }
        if (std::find(consumed_receipts_.begin(), consumed_receipts_.end(), permit.receipt_id)
                != consumed_receipts_.end()
            || std::find(committed_bundles_.begin(), committed_bundles_.end(), bundle.digest)
                != committed_bundles_.end())
        {
            result.code = ApplyCode::replay_rejected;
            result.status = "Patch receipt or bundle was already consumed.";
            return result;
        }
        consumed_receipts_.push_back(permit.receipt_id);

        for (const FilePatch& file : bundle.files)
        {
            FileEvidence evidence{
                .relative_path = file.relative_path,
                .operation = file.operation,
                .before = file.preimage,
                .after = file.postimage};
            for (const Hunk& hunk : file.hunks)
                evidence.hunks.push_back({hunk.old_start, hunk.old_count,
                    hunk.new_start, hunk.new_count, hunk.digest});
            const PortRead current = port.read_live(file.relative_path);
            if (current.kind == EntryKind::symlink || current.kind == EntryKind::other)
            {
                result.code = ApplyCode::destination_rejected;
                result.status = "Sandbox destination is not a regular file or missing entry.";
                result.files.push_back(std::move(evidence));
                return result;
            }
            const bool expected_missing = !file.preimage.exists;
            if ((expected_missing && current.kind != EntryKind::missing)
                || (!expected_missing && (current.kind != EntryKind::regular
                    || !state_matches_bytes(file.preimage, current.bytes))))
            {
                result.code = ApplyCode::stale_preimage;
                result.status = "Sandbox preimage changed after review.";
                result.files.push_back(std::move(evidence));
                return result;
            }
            evidence.preimage_verified = true;
            result.files.push_back(std::move(evidence));
        }

        const std::string transaction_id = bundle.bundle_id + "." + permit.receipt_id;
        if (!port.begin(transaction_id, bundle.digest))
        {
            result.code = ApplyCode::transaction_rejected;
            result.status = "Disposable sandbox rejected transaction start.";
            return result;
        }
        for (std::size_t index = 0u; index < bundle.files.size(); ++index)
        {
            const FilePatch& file = bundle.files[index];
            const bool staged = file.operation == Operation::remove
                ? port.stage_remove(file.relative_path)
                : port.stage_write(file.relative_path, file.postimage_bytes);
            if (!staged)
            {
                port.rollback();
                result.code = ApplyCode::stage_failed;
                result.status = "Disposable sandbox staging failed; transaction rolled back.";
                result.rolled_back = true;
                return result;
            }
            const PortRead staged_read = port.read_staged(file.relative_path);
            const bool verified = file.operation == Operation::remove
                ? staged_read.kind == EntryKind::missing
                : staged_read.kind == EntryKind::regular
                    && state_matches_bytes(file.postimage, staged_read.bytes);
            if (!verified)
            {
                port.rollback();
                result.code = ApplyCode::stage_verification_failed;
                result.status = "Staged postimage verification failed; transaction rolled back.";
                result.rolled_back = true;
                return result;
            }
            result.files[index].staged_verified = true;
        }

        std::string manifest = "EPOCH_SOURCE_PATCH_COMMIT_V1\n" + digest_hex(bundle.digest) + "\n";
        for (const FilePatch& file : bundle.files)
            manifest += file.relative_path + " " + digest_hex(file.postimage.digest)
                + " " + std::to_string(file.postimage.byte_count) + "\n";
        const Digest manifest_digest = digest(manifest);
        if (!port.commit(manifest_digest))
        {
            port.rollback();
            result.code = ApplyCode::commit_failed;
            result.status = "Atomic sandbox commit failed and staging was rolled back.";
            result.rolled_back = true;
            return result;
        }
        for (std::size_t index = 0u; index < bundle.files.size(); ++index)
        {
            const FilePatch& file = bundle.files[index];
            const PortRead current = port.read_live(file.relative_path);
            const bool verified = file.operation == Operation::remove
                ? current.kind == EntryKind::missing
                : current.kind == EntryKind::regular
                    && state_matches_bytes(file.postimage, current.bytes);
            if (!verified)
            {
                result.code = ApplyCode::postimage_verification_failed;
                result.status = "Sandbox violated its atomic commit/read contract.";
                return result;
            }
            result.files[index].committed_verified = true;
        }
        std::string evidence = permit.receipt_id + "\n" + digest_hex(bundle.digest)
            + "\n" + digest_hex(manifest_digest);
        for (const FileEvidence& file : result.files)
        {
            evidence += "\n" + file.relative_path + "\n"
                + digest_hex(file.before.digest) + "\n"
                + digest_hex(file.after.digest);
            for (const HunkEvidence& hunk : file.hunks)
                evidence += "\n" + digest_hex(hunk.digest);
        }
        result.evidence_digest = digest(evidence);
        result.code = ApplyCode::none;
        result.status = "Approved patch committed atomically inside the disposable sandbox.";
        result.committed = true;
        committed_bundles_.push_back(bundle.digest);
        return result;
    }
}
