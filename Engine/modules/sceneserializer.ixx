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
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

export module sceneserializer;

import scenesnapshot;

export namespace epochengine::scene
{
    struct SnapshotTextLimits final
    {
        SceneDocumentLimits document{};
        std::size_t maximum_text_bytes{256u * 1024u * 1024u};
        std::size_t maximum_legacy_metadata_lines{1'024u};
    };

    struct SnapshotParseResult final
    {
        bool ok{};
        SceneSnapshot snapshot{};
        std::string error{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return ok;
        }
    };
}

namespace epochengine::scene::detail
{
    inline constexpr std::size_t kAbsoluteMaximumTextBytes =
        512u * 1024u * 1024u;
    inline constexpr std::size_t kMaximumNumericTextBytes = 128u;

    [[nodiscard]] constexpr bool valid_limits(
        const SnapshotTextLimits& limits) noexcept
    {
        constexpr SceneDocumentLimits hard{};
        return limits.maximum_text_bytes != 0
            && limits.maximum_text_bytes <= kAbsoluteMaximumTextBytes
            && limits.maximum_legacy_metadata_lines != 0
            && limits.maximum_legacy_metadata_lines <= 65'536u
            && limits.document.maximum_objects <= hard.maximum_objects
            && limits.document.maximum_timeline_keys
                <= hard.maximum_timeline_keys
            && limits.document.maximum_packages <= hard.maximum_packages
            && limits.document.maximum_string_bytes != 0
            && limits.document.maximum_string_bytes
                <= hard.maximum_string_bytes;
    }

    [[nodiscard]] constexpr bool valid_text_value(
        std::string_view value,
        std::size_t maximumBytes) noexcept
    {
        if (value.size() > maximumBytes)
            return false;

        for (const unsigned char character : value)
        {
            if (character == 0u || character == 0x7fu)
                return false;
            if (character < 0x20u
                && character != '\n'
                && character != '\r'
                && character != '\t')
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline SnapshotParseResult failure(std::string message)
    {
        return SnapshotParseResult{
            .ok = false,
            .snapshot = {},
            .error = std::move(message)
        };
    }

    [[nodiscard]] constexpr bool consume_literal(
        std::string_view value,
        std::size_t& position,
        std::string_view literal) noexcept
    {
        if (position > value.size()
            || literal.size() > value.size() - position
            || value.substr(position, literal.size()) != literal)
        {
            return false;
        }
        position += literal.size();
        return true;
    }

    [[nodiscard]] constexpr std::string_view trim_spaces(
        std::string_view value) noexcept
    {
        while (!value.empty() && value.front() == ' ')
            value.remove_prefix(1u);
        while (!value.empty() && value.back() == ' ')
            value.remove_suffix(1u);
        return value;
    }

    [[nodiscard]] constexpr bool read_line(
        std::string_view text,
        std::size_t& position,
        std::string_view& line) noexcept
    {
        if (position >= text.size())
        {
            line = {};
            return false;
        }

        const std::size_t start = position;
        while (position < text.size() && text[position] != '\n')
            ++position;

        std::size_t end = position;
        if (end > start && text[end - 1u] == '\r')
            --end;
        if (position < text.size())
            ++position;

        line = text.substr(start, end - start);
        return true;
    }

    [[nodiscard]] inline bool only_empty_trailing_lines(
        std::string_view text,
        std::size_t position) noexcept
    {
        std::string_view line{};
        while (position < text.size())
        {
            if (!read_line(text, position, line) || !line.empty())
                return false;
        }
        return true;
    }

    [[nodiscard]] inline bool parse_quoted_value(
        std::string_view line,
        std::size_t& position,
        std::string& output,
        std::size_t maximumBytes)
    {
        output.clear();
        if (position >= line.size() || line[position] != '"')
            return false;

        const auto append = [&output, maximumBytes](char value)
        {
            if (output.size() >= maximumBytes)
                return false;
            output.push_back(value);
            return true;
        };

        ++position;
        while (position < line.size())
        {
            const unsigned char character =
                static_cast<unsigned char>(line[position++]);
            if (character == '"')
                return true;

            if (character != '\\')
            {
                if (character == 0u
                    || character == 0x7fu
                    || character < 0x20u
                    || !append(static_cast<char>(character)))
                {
                    return false;
                }
                continue;
            }

            if (position >= line.size())
                return false;
            const char escaped = line[position++];
            switch (escaped)
            {
            case '\\': if (!append('\\')) return false; break;
            case 'n': if (!append('\n')) return false; break;
            case 'r': if (!append('\r')) return false; break;
            case 't': if (!append('\t')) return false; break;
            case '"': if (!append('"')) return false; break;
            default: return false;
            }
        }
        return false;
    }

    template<typename Number>
    [[nodiscard]] inline bool parse_number_token(
        std::string_view line,
        std::size_t& position,
        Number& output)
    {
        const std::size_t start = position;
        while (position < line.size() && line[position] != ' ')
            ++position;
        if (start == position
            || position - start > kMaximumNumericTextBytes)
        {
            return false;
        }

        const std::string_view token = line.substr(start, position - start);
        const auto parsed = std::from_chars(
            token.data(),
            token.data() + token.size(),
            output);
        return parsed.ec == std::errc{}
            && parsed.ptr == token.data() + token.size();
    }

    [[nodiscard]] inline bool parse_finite_float_token(
        std::string_view token,
        float& output)
    {
        if (token.empty() || token.size() > kMaximumNumericTextBytes)
            return false;
        const auto parsed = std::from_chars(
            token.data(),
            token.data() + token.size(),
            output);
        return parsed.ec == std::errc{}
            && parsed.ptr == token.data() + token.size()
            && std::isfinite(output);
    }

    [[nodiscard]] inline bool parse_compact_vec3(
        std::string_view line,
        std::size_t& position,
        Vec3& output)
    {
        const std::size_t start = position;
        while (position < line.size() && line[position] != ' ')
            ++position;
        if (start == position)
            return false;

        const std::string_view token = line.substr(start, position - start);
        std::size_t tokenPosition = 0;
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            const std::size_t componentStart = tokenPosition;
            while (tokenPosition < token.size()
                && token[tokenPosition] != ',')
            {
                ++tokenPosition;
            }
            if (componentStart == tokenPosition
                || !parse_finite_float_token(
                    token.substr(componentStart, tokenPosition - componentStart),
                    output[index]))
            {
                return false;
            }

            if (index + 1u < output.size())
            {
                if (tokenPosition >= token.size()
                    || token[tokenPosition] != ',')
                {
                    return false;
                }
                ++tokenPosition;
            }
        }
        return tokenPosition == token.size();
    }

    [[nodiscard]] inline bool parse_spaced_vec3(
        std::string_view line,
        std::size_t& position,
        Vec3& output)
    {
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            if (!parse_number_token(line, position, output[index])
                || !std::isfinite(output[index]))
            {
                return false;
            }
            if (index + 1u < output.size()
                && !consume_literal(line, position, " "))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool parse_bool01(
        std::string_view line,
        std::size_t& position,
        bool& output)
    {
        std::uint32_t value{};
        if (!parse_number_token(line, position, value) || value > 1u)
            return false;
        output = value != 0u;
        return true;
    }

    [[nodiscard]] inline bool parse_named_quoted_line(
        std::string_view line,
        std::string_view prefix,
        std::string& output,
        std::size_t maximumBytes)
    {
        std::size_t position = 0;
        return consume_literal(line, position, prefix)
            && parse_quoted_value(line, position, output, maximumBytes)
            && position == line.size();
    }

    template<typename Number>
    [[nodiscard]] inline bool parse_named_number_line(
        std::string_view line,
        std::string_view prefix,
        Number& output)
    {
        std::size_t position = 0;
        return consume_literal(line, position, prefix)
            && parse_number_token(line, position, output)
            && position == line.size();
    }

    [[nodiscard]] inline bool parse_named_count_line(
        std::string_view line,
        std::string_view prefix,
        std::size_t maximum,
        std::size_t& output)
    {
        std::uint64_t count{};
        if (!parse_named_number_line(line, prefix, count)
            || count > static_cast<std::uint64_t>(maximum)
            || count > static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()))
        {
            return false;
        }
        output = static_cast<std::size_t>(count);
        return true;
    }

    [[nodiscard]] inline bool parse_snapshot_object_line(
        std::string_view line,
        SceneObjectSnapshot& object,
        std::size_t maximumStringBytes,
        bool hasStableId)
    {
        std::size_t position = 0;
        if (!consume_literal(line, position, "object "))
            return false;

        if (hasStableId)
        {
            if (!parse_number_token(line, position, object.id)
                || object.id == kInvalidSceneObjectId
                || !consume_literal(line, position, " "))
            {
                return false;
            }
        }

        return parse_quoted_value(
                line, position, object.name, maximumStringBytes)
            && consume_literal(line, position, " type ")
            && parse_quoted_value(
                line, position, object.type, maximumStringBytes)
            && consume_literal(line, position, " category ")
            && parse_quoted_value(
                line, position, object.category, maximumStringBytes)
            && consume_literal(line, position, " pos ")
            && parse_compact_vec3(line, position, object.position)
            && consume_literal(line, position, " rot ")
            && parse_compact_vec3(line, position, object.rotation)
            && consume_literal(line, position, " scale ")
            && parse_compact_vec3(line, position, object.scale)
            && consume_literal(line, position, " visible ")
            && parse_bool01(line, position, object.visible)
            && consume_literal(line, position, " editor_only ")
            && parse_bool01(line, position, object.editor_only)
            && position == line.size();
    }

    [[nodiscard]] inline bool parse_legacy_object_line(
        std::string_view line,
        SceneObjectSnapshot& object,
        std::size_t maximumStringBytes)
    {
        std::size_t position = 0;
        return consume_literal(line, position, "entity ")
            && parse_quoted_value(
                line, position, object.name, maximumStringBytes)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, object.type, maximumStringBytes)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, object.category, maximumStringBytes)
            && consume_literal(line, position, " pos ")
            && parse_spaced_vec3(line, position, object.position)
            && consume_literal(line, position, " rot ")
            && parse_spaced_vec3(line, position, object.rotation)
            && consume_literal(line, position, " scale ")
            && parse_spaced_vec3(line, position, object.scale)
            && consume_literal(line, position, " visible ")
            && parse_bool01(line, position, object.visible)
            && consume_literal(line, position, " editor_only ")
            && parse_bool01(line, position, object.editor_only)
            && position == line.size();
    }

    [[nodiscard]] inline bool parse_timeline_key_line(
        std::string_view line,
        SceneTimelineKey& key,
        std::size_t maximumStringBytes)
    {
        std::size_t position = 0;
        return consume_literal(line, position, "key ")
            && parse_number_token(line, position, key.simulated_seconds)
            && std::isfinite(key.simulated_seconds)
            && key.simulated_seconds >= 0.0
            && consume_literal(line, position, " ")
            && parse_number_token(line, position, key.frame_index)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, key.label, maximumStringBytes)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, key.event_kind, maximumStringBytes)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, key.target_name, maximumStringBytes)
            && consume_literal(line, position, " ")
            && parse_quoted_value(
                line, position, key.payload, maximumStringBytes)
            && position == line.size();
    }

    [[nodiscard]] inline bool packages_canonical(
        const SceneSnapshot& snapshot) noexcept
    {
        for (std::size_t index = 1; index < snapshot.packages.size(); ++index)
        {
            if (snapshot.packages[index - 1u] >= snapshot.packages[index])
                return false;
        }
        return true;
    }

    [[nodiscard]] inline bool timeline_canonical(
        const SceneSnapshot& snapshot) noexcept
    {
        for (std::size_t index = 1; index < snapshot.timeline_keys.size(); ++index)
        {
            const SceneTimelineKey& previous =
                snapshot.timeline_keys[index - 1u];
            const SceneTimelineKey& current = snapshot.timeline_keys[index];
            if (current.simulated_seconds < previous.simulated_seconds
                || (current.simulated_seconds == previous.simulated_seconds
                    && current.frame_index < previous.frame_index))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline bool strict_snapshot_valid(
        const SceneSnapshot& snapshot,
        const SnapshotTextLimits& limits,
        bool requireCanonicalOrder)
    {
        if (!valid_limits(limits)
            || !std::isfinite(snapshot.captured_simulated_seconds)
            || snapshot.captured_simulated_seconds < 0.0
            || snapshot.document_kind.empty()
            || snapshot.support_tier.empty()
            || !validate_scene_document(
                snapshot,
                SceneDocumentRequirement::authoring,
                limits.document))
        {
            return false;
        }

        const auto validText = [&limits](std::string_view value)
        {
            return valid_text_value(
                value,
                limits.document.maximum_string_bytes);
        };
        if (!validText(snapshot.scene_id)
            || !validText(snapshot.project_id)
            || !validText(snapshot.world_name)
            || !validText(snapshot.document_kind)
            || !validText(snapshot.support_tier))
        {
            return false;
        }

        for (const std::string& package : snapshot.packages)
        {
            if (!validText(package))
                return false;
        }
        for (const SceneObjectSnapshot& object : snapshot.objects)
        {
            if (!validText(object.name)
                || !validText(object.type)
                || !validText(object.category))
            {
                return false;
            }
        }
        for (const SceneTimelineKey& key : snapshot.timeline_keys)
        {
            if (!validText(key.label)
                || !validText(key.event_kind)
                || !validText(key.target_name)
                || !validText(key.payload))
            {
                return false;
            }
        }

        if (snapshot.primary_camera != kInvalidSceneObjectId)
        {
            const SceneObjectSnapshot* camera =
                find_object_by_id(snapshot, snapshot.primary_camera);
            if (camera == nullptr || camera->type != "Camera")
                return false;
        }
        if (snapshot.primary_spawn != kInvalidSceneObjectId)
        {
            const SceneObjectSnapshot* spawn =
                find_object_by_id(snapshot, snapshot.primary_spawn);
            if (spawn == nullptr || spawn->type != "Spawn")
                return false;
        }

        return !requireCanonicalOrder
            || (packages_canonical(snapshot) && timeline_canonical(snapshot));
    }

    inline void normalize_compatible_snapshot(SceneSnapshot& snapshot)
    {
        normalize_scene_document(snapshot);
        std::sort(snapshot.packages.begin(), snapshot.packages.end());
        sort_timeline_keys(snapshot);
    }

    class BoundedTextWriter final
    {
    public:
        explicit BoundedTextWriter(std::size_t maximumBytes)
            : maximumBytes_(maximumBytes)
        {
        }

        [[nodiscard]] bool append(std::string_view value)
        {
            if (!valid_
                || value.size() > maximumBytes_
                || text_.size() > maximumBytes_ - value.size())
            {
                valid_ = false;
                return false;
            }
            text_.append(value);
            return true;
        }

        template<typename Integer>
            requires std::is_integral_v<Integer>
        [[nodiscard]] bool append_integer(Integer value)
        {
            std::array<char, kMaximumNumericTextBytes> buffer{};
            const auto converted = std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                value);
            return converted.ec == std::errc{}
                && append(std::string_view{
                    buffer.data(),
                    static_cast<std::size_t>(
                        converted.ptr - buffer.data())});
        }

        template<typename Floating>
            requires std::is_floating_point_v<Floating>
        [[nodiscard]] bool append_floating(Floating value)
        {
            if (!std::isfinite(value))
                return false;
            std::array<char, kMaximumNumericTextBytes> buffer{};
            const auto converted = std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                value,
                std::chars_format::general,
                std::numeric_limits<Floating>::max_digits10);
            return converted.ec == std::errc{}
                && append(std::string_view{
                    buffer.data(),
                    static_cast<std::size_t>(
                        converted.ptr - buffer.data())});
        }

        [[nodiscard]] bool append_quoted(std::string_view value)
        {
            if (!append("\""))
                return false;
            for (const char character : value)
            {
                switch (character)
                {
                case '\\': if (!append("\\\\")) return false; break;
                case '\n': if (!append("\\n")) return false; break;
                case '\r': if (!append("\\r")) return false; break;
                case '\t': if (!append("\\t")) return false; break;
                case '"': if (!append("\\\"")) return false; break;
                default:
                    if (!append(std::string_view{&character, 1u}))
                        return false;
                    break;
                }
            }
            return append("\"");
        }

        [[nodiscard]] bool append_vec3(const Vec3& value)
        {
            return append_floating(value[0])
                && append(",")
                && append_floating(value[1])
                && append(",")
                && append_floating(value[2]);
        }

        [[nodiscard]] std::string take()
        {
            return valid_ ? std::move(text_) : std::string{};
        }

    private:
        std::string text_{};
        std::size_t maximumBytes_{};
        bool valid_{true};
    };

    [[nodiscard]] inline bool append_quoted_line(
        BoundedTextWriter& writer,
        std::string_view name,
        std::string_view value)
    {
        return writer.append(name)
            && writer.append(" ")
            && writer.append_quoted(value)
            && writer.append("\n");
    }

    template<typename Number>
    [[nodiscard]] inline bool append_number_line(
        BoundedTextWriter& writer,
        std::string_view name,
        Number value)
    {
        if (!writer.append(name) || !writer.append(" "))
            return false;
        if constexpr (std::is_floating_point_v<Number>)
        {
            if (!writer.append_floating(value))
                return false;
        }
        else if (!writer.append_integer(value))
        {
            return false;
        }
        return writer.append("\n");
    }

    [[nodiscard]] inline SnapshotParseResult finish_parse(
        SceneSnapshot snapshot,
        const SnapshotTextLimits& limits,
        bool normalizeCompatibility,
        bool requireCanonicalOrder)
    {
        if (normalizeCompatibility)
            normalize_compatible_snapshot(snapshot);
        if (!strict_snapshot_valid(snapshot, limits, requireCanonicalOrder))
            return failure("snapshot failed bounded document validation");
        return SnapshotParseResult{
            .ok = true,
            .snapshot = std::move(snapshot),
            .error = {}
        };
    }

    [[nodiscard]] inline SnapshotParseResult parse_v2_body(
        std::string_view text,
        std::size_t position,
        const SnapshotTextLimits& limits)
    {
        SceneSnapshot snapshot{};
        std::string_view line{};
        const std::size_t maximumStringBytes =
            limits.document.maximum_string_bytes;

        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line, "scene ", snapshot.scene_id, maximumStringBytes))
            return failure("invalid v2 scene line");
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line, "project ", snapshot.project_id, maximumStringBytes))
            return failure("invalid v2 project line");
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line, "world ", snapshot.world_name, maximumStringBytes))
            return failure("invalid v2 world line");
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line,
                "document_kind ",
                snapshot.document_kind,
                maximumStringBytes))
            return failure("invalid v2 document_kind line");
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line,
                "support_tier ",
                snapshot.support_tier,
                maximumStringBytes))
            return failure("invalid v2 support_tier line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "revision ", snapshot.revision)
            || snapshot.revision == 0u)
            return failure("invalid v2 revision line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "primary_camera ", snapshot.primary_camera))
            return failure("invalid v2 primary_camera line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "primary_spawn ", snapshot.primary_spawn))
            return failure("invalid v2 primary_spawn line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "frame ", snapshot.captured_frame_index))
            return failure("invalid v2 frame line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "time ", snapshot.captured_simulated_seconds)
            || !std::isfinite(snapshot.captured_simulated_seconds)
            || snapshot.captured_simulated_seconds < 0.0)
            return failure("invalid v2 time line");

        std::size_t packageCount{};
        if (!read_line(text, position, line)
            || !parse_named_count_line(
                line,
                "packages ",
                limits.document.maximum_packages,
                packageCount))
            return failure("invalid or excessive v2 package count");
        snapshot.packages.reserve(packageCount);
        for (std::size_t index = 0; index < packageCount; ++index)
        {
            std::string package{};
            if (!read_line(text, position, line)
                || !parse_named_quoted_line(
                    line, "package ", package, maximumStringBytes)
                || package.empty())
            {
                return failure(
                    "invalid v2 package line " + std::to_string(index));
            }
            snapshot.packages.push_back(std::move(package));
        }

        std::size_t objectCount{};
        if (!read_line(text, position, line)
            || !parse_named_count_line(
                line,
                "objects ",
                limits.document.maximum_objects,
                objectCount))
            return failure("invalid or excessive v2 object count");
        snapshot.objects.reserve(objectCount);
        for (std::size_t index = 0; index < objectCount; ++index)
        {
            SceneObjectSnapshot object{};
            if (!read_line(text, position, line)
                || !parse_snapshot_object_line(
                    line, object, maximumStringBytes, true))
            {
                return failure(
                    "invalid v2 object line " + std::to_string(index));
            }
            snapshot.objects.push_back(std::move(object));
        }

        std::size_t keyCount{};
        if (!read_line(text, position, line)
            || !parse_named_count_line(
                line,
                "timeline_keys ",
                limits.document.maximum_timeline_keys,
                keyCount))
            return failure("invalid or excessive v2 timeline key count");
        snapshot.timeline_keys.reserve(keyCount);
        for (std::size_t index = 0; index < keyCount; ++index)
        {
            SceneTimelineKey key{};
            if (!read_line(text, position, line)
                || !parse_timeline_key_line(
                    line, key, maximumStringBytes))
            {
                return failure(
                    "invalid v2 timeline key line " + std::to_string(index));
            }
            snapshot.timeline_keys.push_back(std::move(key));
        }

        if (position != text.size())
            return failure("unexpected trailing v2 snapshot text");
        return finish_parse(std::move(snapshot), limits, false, true);
    }

    [[nodiscard]] inline SnapshotParseResult parse_v1_body(
        std::string_view text,
        std::size_t position,
        const SnapshotTextLimits& limits)
    {
        SceneSnapshot snapshot{};
        snapshot.document_kind = "game";
        snapshot.support_tier = "baseline";
        snapshot.revision = 1u;

        std::string_view line{};
        const std::size_t maximumStringBytes =
            limits.document.maximum_string_bytes;
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line, "scene ", snapshot.scene_id, maximumStringBytes))
            return failure("invalid v1 scene line");
        if (!read_line(text, position, line)
            || !parse_named_quoted_line(
                line, "world ", snapshot.world_name, maximumStringBytes))
            return failure("invalid v1 world line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "frame ", snapshot.captured_frame_index))
            return failure("invalid v1 frame line");
        if (!read_line(text, position, line)
            || !parse_named_number_line(
                line, "time ", snapshot.captured_simulated_seconds)
            || !std::isfinite(snapshot.captured_simulated_seconds)
            || snapshot.captured_simulated_seconds < 0.0)
            return failure("invalid v1 time line");

        std::size_t objectCount{};
        if (!read_line(text, position, line)
            || !parse_named_count_line(
                line,
                "objects ",
                limits.document.maximum_objects,
                objectCount))
            return failure("invalid or excessive v1 object count");
        snapshot.objects.reserve(objectCount);
        for (std::size_t index = 0; index < objectCount; ++index)
        {
            SceneObjectSnapshot object{};
            if (!read_line(text, position, line)
                || !parse_snapshot_object_line(
                    line, object, maximumStringBytes, false))
            {
                return failure(
                    "invalid v1 object line " + std::to_string(index));
            }
            snapshot.objects.push_back(std::move(object));
        }

        std::size_t keyCount{};
        if (!read_line(text, position, line)
            || !parse_named_count_line(
                line,
                "timeline_keys ",
                limits.document.maximum_timeline_keys,
                keyCount))
            return failure("invalid or excessive v1 timeline key count");
        snapshot.timeline_keys.reserve(keyCount);
        for (std::size_t index = 0; index < keyCount; ++index)
        {
            SceneTimelineKey key{};
            if (!read_line(text, position, line)
                || !parse_timeline_key_line(
                    line, key, maximumStringBytes))
            {
                return failure(
                    "invalid v1 timeline key line " + std::to_string(index));
            }
            snapshot.timeline_keys.push_back(std::move(key));
        }

        if (!only_empty_trailing_lines(text, position))
            return failure("unexpected trailing v1 snapshot text");
        return finish_parse(std::move(snapshot), limits, true, true);
    }
}

export namespace epochengine::scene
{
    [[nodiscard]] inline std::string escape_snapshot_value(
        std::string_view value)
    {
        std::string escaped{};
        if (value.size() <= (std::numeric_limits<std::size_t>::max)() / 2u)
            escaped.reserve(value.size() * 2u);
        for (const char character : value)
        {
            switch (character)
            {
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '"': escaped += "\\\""; break;
            default: escaped.push_back(character); break;
            }
        }
        return escaped;
    }

    [[nodiscard]] inline std::string vec3_snapshot_text(const Vec3& value)
    {
        detail::BoundedTextWriter writer{512u};
        if (!writer.append_vec3(value))
            return {};
        return writer.take();
    }

    [[nodiscard]] inline std::string snapshot_summary(
        const SceneSnapshot& snapshot)
    {
        return snapshot.scene_id
            + " | project "
            + (snapshot.project_id.empty() ? "(unbound)" : snapshot.project_id)
            + " | world "
            + (snapshot.world_name.empty() ? "(unnamed)" : snapshot.world_name)
            + " | revision "
            + std::to_string(snapshot.revision)
            + " | frame "
            + std::to_string(snapshot.captured_frame_index)
            + " | objects "
            + std::to_string(snapshot.objects.size())
            + " | keys "
            + std::to_string(snapshot.timeline_keys.size());
    }

    [[nodiscard]] inline std::string serialize_snapshot_text(
        const SceneSnapshot& source,
        const SnapshotTextLimits& limits = {})
    {
        try
        {
            if (!detail::valid_limits(limits))
                return {};

            SceneSnapshot snapshot = source;
            detail::normalize_compatible_snapshot(snapshot);
            if (!detail::strict_snapshot_valid(snapshot, limits, true))
                return {};

            detail::BoundedTextWriter writer{limits.maximum_text_bytes};
            if (!writer.append("epoch_snapshot 2\n")
                || !detail::append_quoted_line(
                    writer, "scene", snapshot.scene_id)
                || !detail::append_quoted_line(
                    writer, "project", snapshot.project_id)
                || !detail::append_quoted_line(
                    writer, "world", snapshot.world_name)
                || !detail::append_quoted_line(
                    writer, "document_kind", snapshot.document_kind)
                || !detail::append_quoted_line(
                    writer, "support_tier", snapshot.support_tier)
                || !detail::append_number_line(
                    writer, "revision", snapshot.revision)
                || !detail::append_number_line(
                    writer, "primary_camera", snapshot.primary_camera)
                || !detail::append_number_line(
                    writer, "primary_spawn", snapshot.primary_spawn)
                || !detail::append_number_line(
                    writer, "frame", snapshot.captured_frame_index)
                || !detail::append_number_line(
                    writer, "time", snapshot.captured_simulated_seconds)
                || !detail::append_number_line(
                    writer, "packages", snapshot.packages.size()))
            {
                return {};
            }

            for (const std::string& package : snapshot.packages)
            {
                if (!detail::append_quoted_line(writer, "package", package))
                    return {};
            }

            if (!detail::append_number_line(
                    writer, "objects", snapshot.objects.size()))
                return {};
            for (const SceneObjectSnapshot& object : snapshot.objects)
            {
                if (!writer.append("object ")
                    || !writer.append_integer(object.id)
                    || !writer.append(" ")
                    || !writer.append_quoted(object.name)
                    || !writer.append(" type ")
                    || !writer.append_quoted(object.type)
                    || !writer.append(" category ")
                    || !writer.append_quoted(object.category)
                    || !writer.append(" pos ")
                    || !writer.append_vec3(object.position)
                    || !writer.append(" rot ")
                    || !writer.append_vec3(object.rotation)
                    || !writer.append(" scale ")
                    || !writer.append_vec3(object.scale)
                    || !writer.append(" visible ")
                    || !writer.append_integer(object.visible ? 1u : 0u)
                    || !writer.append(" editor_only ")
                    || !writer.append_integer(object.editor_only ? 1u : 0u)
                    || !writer.append("\n"))
                {
                    return {};
                }
            }

            if (!detail::append_number_line(
                    writer,
                    "timeline_keys",
                    snapshot.timeline_keys.size()))
                return {};
            for (const SceneTimelineKey& key : snapshot.timeline_keys)
            {
                if (!writer.append("key ")
                    || !writer.append_floating(key.simulated_seconds)
                    || !writer.append(" ")
                    || !writer.append_integer(key.frame_index)
                    || !writer.append(" ")
                    || !writer.append_quoted(key.label)
                    || !writer.append(" ")
                    || !writer.append_quoted(key.event_kind)
                    || !writer.append(" ")
                    || !writer.append_quoted(key.target_name)
                    || !writer.append(" ")
                    || !writer.append_quoted(key.payload)
                    || !writer.append("\n"))
                {
                    return {};
                }
            }
            return writer.take();
        }
        catch (...)
        {
            return {};
        }
    }

    [[nodiscard]] inline SnapshotParseResult parse_snapshot_text(
        std::string_view text,
        const SnapshotTextLimits& limits = {})
    {
        try
        {
            if (!detail::valid_limits(limits))
                return detail::failure("invalid snapshot parser limits");
            if (text.empty() || text.size() > limits.maximum_text_bytes)
                return detail::failure("empty or excessive snapshot text");

            std::size_t position = 0;
            std::string_view headerLine{};
            if (!detail::read_line(text, position, headerLine))
                return detail::failure("missing snapshot header");
            if (headerLine == "epoch_snapshot 2")
                return detail::parse_v2_body(text, position, limits);
            if (headerLine == "epoch_snapshot 1")
                return detail::parse_v1_body(text, position, limits);
            return detail::failure("unsupported snapshot header");
        }
        catch (const std::bad_alloc&)
        {
            return {};
        }
        catch (...)
        {
            return detail::failure("snapshot parser failed safely");
        }
    }

    [[nodiscard]] inline SnapshotParseResult parse_legacy_editor_scene_text(
        std::string_view text,
        const SnapshotTextLimits& limits = {})
    {
        try
        {
            if (!detail::valid_limits(limits))
                return detail::failure("invalid legacy parser limits");
            if (text.empty() || text.size() > limits.maximum_text_bytes)
                return detail::failure(
                    "empty or excessive legacy scene text");

            SceneSnapshot snapshot{};
            snapshot.document_kind = "game";
            snapshot.support_tier = "baseline";
            snapshot.revision = 1u;

            std::size_t position = 0;
            std::string_view line{};
            const std::size_t maximumStringBytes =
                limits.document.maximum_string_bytes;
            if (!detail::read_line(text, position, line)
                || !detail::parse_named_quoted_line(
                    line,
                    "scene ",
                    snapshot.scene_id,
                    maximumStringBytes))
            {
                return detail::failure("invalid legacy scene line");
            }
            snapshot.world_name = snapshot.scene_id;

            if (!detail::read_line(text, position, line)
                || !detail::parse_named_quoted_line(
                    line,
                    "project ",
                    snapshot.project_id,
                    maximumStringBytes))
            {
                return detail::failure("invalid legacy project line");
            }

            if (!detail::read_line(text, position, line))
                return detail::failure("missing legacy entity header");
            if (line == "{")
            {
                bool kindSeen = false;
                bool supportTierSeen = false;
                std::size_t metadataLines = 0;
                for (;;)
                {
                    if (!detail::read_line(text, position, line))
                        return detail::failure(
                            "unterminated legacy metadata");
                    const std::string_view metadata =
                        detail::trim_spaces(line);
                    if (metadata == "}")
                        break;
                    if (metadata.empty()
                        || ++metadataLines
                            > limits.maximum_legacy_metadata_lines)
                    {
                        return detail::failure(
                            "invalid or excessive legacy metadata");
                    }

                    if (metadata.starts_with("kind "))
                    {
                        if (kindSeen
                            || !detail::parse_named_quoted_line(
                                metadata,
                                "kind ",
                                snapshot.document_kind,
                                maximumStringBytes)
                            || snapshot.document_kind.empty())
                        {
                            return detail::failure(
                                "invalid duplicate legacy kind metadata");
                        }
                        kindSeen = true;
                    }
                    else if (metadata.starts_with("support_tier "))
                    {
                        if (supportTierSeen
                            || !detail::parse_named_quoted_line(
                                metadata,
                                "support_tier ",
                                snapshot.support_tier,
                                maximumStringBytes)
                            || snapshot.support_tier.empty())
                        {
                            return detail::failure(
                                "invalid duplicate legacy support metadata");
                        }
                        supportTierSeen = true;
                    }
                    else if (metadata.starts_with(
                        "engine_asset_package "))
                    {
                        std::string package{};
                        if (!detail::parse_named_quoted_line(
                                metadata,
                                "engine_asset_package ",
                                package,
                                maximumStringBytes)
                            || package.empty()
                            || snapshot.packages.size()
                                >= limits.document.maximum_packages)
                        {
                            return detail::failure(
                                "invalid or excessive legacy package metadata");
                        }
                        snapshot.packages.push_back(std::move(package));
                    }
                    else
                    {
                        return detail::failure(
                            "unsupported legacy metadata field");
                    }
                }

                if (!detail::read_line(text, position, line))
                    return detail::failure(
                        "missing legacy entity header");
            }

            if (line != "epoch_editor_entities 1")
                return detail::failure(
                    "missing epoch_editor_entities 1 header");

            while (position < text.size())
            {
                if (!detail::read_line(text, position, line)
                    || line.empty())
                {
                    return detail::failure(
                        "invalid trailing legacy entity text");
                }
                if (snapshot.objects.size()
                    >= limits.document.maximum_objects)
                {
                    return detail::failure(
                        "excessive legacy entity count");
                }

                SceneObjectSnapshot object{};
                if (!detail::parse_legacy_object_line(
                        line, object, maximumStringBytes))
                {
                    return detail::failure(
                        "invalid legacy entity line "
                        + std::to_string(snapshot.objects.size()));
                }
                snapshot.objects.push_back(std::move(object));
            }

            return detail::finish_parse(
                std::move(snapshot),
                limits,
                true,
                true);
        }
        catch (const std::bad_alloc&)
        {
            return {};
        }
        catch (...)
        {
            return detail::failure(
                "legacy scene parser failed safely");
        }
    }
}
