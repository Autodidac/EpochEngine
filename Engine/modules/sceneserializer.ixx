// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

module;

#include <charconv>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>

export module sceneserializer;

import scenesnapshot;

namespace epoch::scene::detail
{
    [[nodiscard]] inline bool consume_literal(std::string_view value, std::size_t& pos, std::string_view literal)
    {
        if (pos > value.size() || value.substr(pos, literal.size()) != literal)
            return false;

        pos += literal.size();
        return true;
    }

    inline void skip_spaces(std::string_view value, std::size_t& pos) noexcept
    {
        while (pos < value.size() && value[pos] == ' ')
            ++pos;
    }

    [[nodiscard]] inline std::string_view read_line(std::string_view text, std::size_t& pos) noexcept
    {
        const std::size_t start = pos;
        while (pos < text.size() && text[pos] != '\n')
            ++pos;

        std::size_t end = pos;
        if (end > start && text[end - 1] == '\r')
            --end;

        if (pos < text.size() && text[pos] == '\n')
            ++pos;

        return text.substr(start, end - start);
    }

    [[nodiscard]] inline bool parse_quoted_value(std::string_view line, std::size_t& pos, std::string& out)
    {
        out.clear();
        skip_spaces(line, pos);
        if (pos >= line.size() || line[pos] != '"')
            return false;

        ++pos;
        while (pos < line.size())
        {
            const char ch = line[pos++];
            if (ch == '"')
                return true;

            if (ch != '\\')
            {
                out.push_back(ch);
                continue;
            }

            if (pos >= line.size())
                return false;

            const char escaped = line[pos++];
            switch (escaped)
            {
            case '\\':
                out.push_back('\\');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '"':
                out.push_back('"');
                break;
            default:
                return false;
            }
        }

        return false;
    }

    template <typename Number>
    [[nodiscard]] inline bool parse_number_token(std::string_view line, std::size_t& pos, Number& out)
    {
        skip_spaces(line, pos);
        const std::size_t start = pos;
        while (pos < line.size() && line[pos] != ' ')
            ++pos;

        if (start == pos)
            return false;

        const std::string_view token = line.substr(start, pos - start);
        const auto result = std::from_chars(token.data(), token.data() + token.size(), out);
        return result.ec == std::errc{} && result.ptr == token.data() + token.size();
    }

    [[nodiscard]] inline bool parse_vec3_token(std::string_view line, std::size_t& pos, Vec3& out)
    {
        skip_spaces(line, pos);
        const std::size_t start = pos;
        while (pos < line.size() && line[pos] != ' ')
            ++pos;

        if (start == pos)
            return false;

        const std::string_view token = line.substr(start, pos - start);
        std::size_t tokenPos = 0;
        for (std::size_t index = 0; index < out.size(); ++index)
        {
            const std::size_t componentStart = tokenPos;
            while (tokenPos < token.size() && token[tokenPos] != ',')
                ++tokenPos;

            if (componentStart == tokenPos)
                return false;

            float component = 0.0F;
            const std::string_view componentText = token.substr(componentStart, tokenPos - componentStart);
            const auto result = std::from_chars(
                componentText.data(),
                componentText.data() + componentText.size(),
                component);
            if (result.ec != std::errc{} || result.ptr != componentText.data() + componentText.size())
                return false;

            out[index] = component;

            if (index + 1u < out.size())
            {
                if (tokenPos >= token.size() || token[tokenPos] != ',')
                    return false;
                ++tokenPos;
            }
        }

        return tokenPos == token.size();
    }

    [[nodiscard]] inline bool parse_bool01_token(std::string_view line, std::size_t& pos, bool& out)
    {
        unsigned int value = 0;
        if (!parse_number_token(line, pos, value) || value > 1u)
            return false;

        out = value != 0u;
        return true;
    }

    [[nodiscard]] inline bool parse_named_quoted_line(
        std::string_view line,
        std::string_view prefix,
        std::string& out)
    {
        std::size_t pos = 0;
        if (!consume_literal(line, pos, prefix) || !parse_quoted_value(line, pos, out))
            return false;

        skip_spaces(line, pos);
        return pos == line.size();
    }
}

export namespace epoch::scene
{
    struct SnapshotParseResult
    {
        bool ok = false;
        SceneSnapshot snapshot{};
        std::string error{};
    };

    [[nodiscard]] inline std::string escape_snapshot_value(std::string_view value)
    {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '"':
                escaped += "\\\"";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    [[nodiscard]] inline std::string vec3_snapshot_text(const Vec3& value)
    {
        return std::format("{:.4f},{:.4f},{:.4f}", value[0], value[1], value[2]);
    }

    [[nodiscard]] inline std::string snapshot_summary(const SceneSnapshot& snapshot)
    {
        return std::format(
            "{} | world {} | frame {} | time {:.3f}s | objects {} | keys {}",
            snapshot.scene_id.empty() ? std::string("(unnamed scene)") : snapshot.scene_id,
            snapshot.world_name.empty() ? std::string("(unnamed world)") : snapshot.world_name,
            snapshot.captured_frame_index,
            snapshot.captured_simulated_seconds,
            snapshot.objects.size(),
            snapshot.timeline_keys.size());
    }

    [[nodiscard]] inline std::string serialize_snapshot_text(const SceneSnapshot& snapshot)
    {
        std::string text;
        text += "epoch_snapshot 1\n";
        text += "scene \"" + escape_snapshot_value(snapshot.scene_id) + "\"\n";
        text += "world \"" + escape_snapshot_value(snapshot.world_name) + "\"\n";
        text += std::format("frame {}\n", snapshot.captured_frame_index);
        text += std::format("time {:.6f}\n", snapshot.captured_simulated_seconds);
        text += std::format("objects {}\n", snapshot.objects.size());

        for (const auto& object : snapshot.objects)
        {
            text += "object \"";
            text += escape_snapshot_value(object.name);
            text += "\" type \"";
            text += escape_snapshot_value(object.type);
            text += "\" category \"";
            text += escape_snapshot_value(object.category);
            text += "\" pos ";
            text += vec3_snapshot_text(object.position);
            text += " rot ";
            text += vec3_snapshot_text(object.rotation);
            text += " scale ";
            text += vec3_snapshot_text(object.scale);
            text += std::format(" visible {} editor_only {}\n", object.visible ? 1 : 0, object.editor_only ? 1 : 0);
        }

        text += std::format("timeline_keys {}\n", snapshot.timeline_keys.size());
        for (const auto& key : snapshot.timeline_keys)
        {
            text += std::format(
                "key {:.6f} {} \"{}\" \"{}\" \"{}\" \"{}\"\n",
                key.simulated_seconds,
                key.frame_index,
                escape_snapshot_value(key.label),
                escape_snapshot_value(key.event_kind),
                escape_snapshot_value(key.target_name),
                escape_snapshot_value(key.payload));
        }

        return text;
    }

    [[nodiscard]] inline SnapshotParseResult parse_snapshot_text(std::string_view text)
    {
        SnapshotParseResult result{};
        std::size_t textPos = 0;
        const auto fail = [&result](std::string message)
        {
            result.ok = false;
            result.error = std::move(message);
            return result;
        };

        if (detail::read_line(text, textPos) != "epoch_snapshot 1")
            return fail("missing epoch_snapshot 1 header");

        if (!detail::parse_named_quoted_line(detail::read_line(text, textPos), "scene ", result.snapshot.scene_id))
            return fail("invalid scene line");

        if (!detail::parse_named_quoted_line(detail::read_line(text, textPos), "world ", result.snapshot.world_name))
            return fail("invalid world line");

        {
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "frame ")
                || !detail::parse_number_token(line, pos, result.snapshot.captured_frame_index))
            {
                return fail("invalid frame line");
            }
            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail("invalid frame line suffix");
        }

        {
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "time ")
                || !detail::parse_number_token(line, pos, result.snapshot.captured_simulated_seconds))
            {
                return fail("invalid time line");
            }
            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail("invalid time line suffix");
        }

        std::size_t objectCount = 0;
        {
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "objects ")
                || !detail::parse_number_token(line, pos, objectCount))
            {
                return fail("invalid objects line");
            }
            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail("invalid objects line suffix");
        }

        result.snapshot.objects.reserve(objectCount);
        for (std::size_t index = 0; index < objectCount; ++index)
        {
            SceneObjectSnapshot object{};
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "object ")
                || !detail::parse_quoted_value(line, pos, object.name)
                || !detail::consume_literal(line, pos, " type ")
                || !detail::parse_quoted_value(line, pos, object.type)
                || !detail::consume_literal(line, pos, " category ")
                || !detail::parse_quoted_value(line, pos, object.category)
                || !detail::consume_literal(line, pos, " pos ")
                || !detail::parse_vec3_token(line, pos, object.position)
                || !detail::consume_literal(line, pos, " rot ")
                || !detail::parse_vec3_token(line, pos, object.rotation)
                || !detail::consume_literal(line, pos, " scale ")
                || !detail::parse_vec3_token(line, pos, object.scale)
                || !detail::consume_literal(line, pos, " visible ")
                || !detail::parse_bool01_token(line, pos, object.visible)
                || !detail::consume_literal(line, pos, " editor_only ")
                || !detail::parse_bool01_token(line, pos, object.editor_only))
            {
                return fail(std::format("invalid object line {}", index));
            }

            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail(std::format("invalid object line {} suffix", index));

            result.snapshot.objects.push_back(std::move(object));
        }

        std::size_t keyCount = 0;
        {
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "timeline_keys ")
                || !detail::parse_number_token(line, pos, keyCount))
            {
                return fail("invalid timeline_keys line");
            }
            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail("invalid timeline_keys line suffix");
        }

        result.snapshot.timeline_keys.reserve(keyCount);
        for (std::size_t index = 0; index < keyCount; ++index)
        {
            SceneTimelineKey key{};
            std::size_t pos = 0;
            const std::string_view line = detail::read_line(text, textPos);
            if (!detail::consume_literal(line, pos, "key ")
                || !detail::parse_number_token(line, pos, key.simulated_seconds)
                || !detail::parse_number_token(line, pos, key.frame_index)
                || !detail::parse_quoted_value(line, pos, key.label)
                || !detail::consume_literal(line, pos, " ")
                || !detail::parse_quoted_value(line, pos, key.event_kind)
                || !detail::consume_literal(line, pos, " ")
                || !detail::parse_quoted_value(line, pos, key.target_name)
                || !detail::consume_literal(line, pos, " ")
                || !detail::parse_quoted_value(line, pos, key.payload))
            {
                return fail(std::format("invalid timeline key line {}", index));
            }

            detail::skip_spaces(line, pos);
            if (pos != line.size())
                return fail(std::format("invalid timeline key line {} suffix", index));

            result.snapshot.timeline_keys.push_back(std::move(key));
        }

        while (textPos < text.size())
        {
            const std::string_view line = detail::read_line(text, textPos);
            if (!line.empty())
                return fail("unexpected trailing snapshot text");
        }

        result.ok = true;
        result.error.clear();
        return result;
    }
}
