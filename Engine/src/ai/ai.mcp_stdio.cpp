/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.mcp_stdio;

namespace epochengine::ai::mcp_stdio
{
    namespace
    {
        [[nodiscard]] bool valid_utf8(std::string_view text) noexcept
        {
            std::size_t index{};
            while (index < text.size())
            {
                const auto lead = static_cast<std::uint8_t>(text[index]);
                if (lead <= 0x7fu)
                {
                    ++index;
                    continue;
                }

                std::size_t continuationCount{};
                std::uint32_t codePoint{};
                std::uint32_t minimum{};
                if ((lead & 0xe0u) == 0xc0u)
                {
                    continuationCount = 1u;
                    codePoint = lead & 0x1fu;
                    minimum = 0x80u;
                }
                else if ((lead & 0xf0u) == 0xe0u)
                {
                    continuationCount = 2u;
                    codePoint = lead & 0x0fu;
                    minimum = 0x800u;
                }
                else if ((lead & 0xf8u) == 0xf0u)
                {
                    continuationCount = 3u;
                    codePoint = lead & 0x07u;
                    minimum = 0x10000u;
                }
                else
                {
                    return false;
                }
                if (continuationCount > text.size() - index - 1u)
                    return false;
                for (std::size_t offset = 1u; offset <= continuationCount; ++offset)
                {
                    const auto byte = static_cast<std::uint8_t>(text[index + offset]);
                    if ((byte & 0xc0u) != 0x80u)
                        return false;
                    codePoint = (codePoint << 6u) | (byte & 0x3fu);
                }
                if (codePoint < minimum || codePoint > 0x10ffffu
                    || (codePoint >= 0xd800u && codePoint <= 0xdfffu))
                {
                    return false;
                }
                index += continuationCount + 1u;
            }
            return true;
        }

        [[nodiscard]] std::string_view trim_ascii(std::string_view value) noexcept
        {
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                value.remove_prefix(1u);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
                value.remove_suffix(1u);
            return value;
        }

        [[nodiscard]] std::string lower_ascii(std::string_view value)
        {
            std::string lowered{};
            lowered.reserve(value.size());
            for (const char character : value)
            {
                lowered.push_back(static_cast<char>(std::tolower(
                    static_cast<unsigned char>(character))));
            }
            return lowered;
        }

        [[nodiscard]] bool usable_identifier(
            std::string_view value,
            std::size_t maximumBytes) noexcept
        {
            if (value.empty() || value.size() > maximumBytes)
                return false;
            return std::all_of(value.begin(), value.end(), [](char character)
            {
                const auto byte = static_cast<unsigned char>(character);
                return std::isalnum(byte) != 0
                    || character == '-' || character == '_'
                    || character == '.' || character == ':';
            });
        }

        [[nodiscard]] bool usable_text(
            std::string_view value,
            std::size_t maximumBytes,
            bool allowEmpty = false) noexcept
        {
            if ((!allowEmpty && value.empty()) || value.size() > maximumBytes
                || !valid_utf8(value))
            {
                return false;
            }
            return std::none_of(value.begin(), value.end(), [](char character)
            {
                const auto byte = static_cast<unsigned char>(character);
                return byte == 0u || byte == 0x7fu
                    || (byte < 0x20u && character != '\t' && character != '\n');
            });
        }

        void append_utf8(std::string& output, std::uint32_t codePoint)
        {
            if (codePoint <= 0x7fu)
            {
                output.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7ffu)
            {
                output.push_back(static_cast<char>(0xc0u | (codePoint >> 6u)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else if (codePoint <= 0xffffu)
            {
                output.push_back(static_cast<char>(0xe0u | (codePoint >> 12u)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else
            {
                output.push_back(static_cast<char>(0xf0u | (codePoint >> 18u)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 12u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
        }

        [[nodiscard]] std::string json_string(std::string_view value)
        {
            constexpr char hex[] = "0123456789abcdef";
            std::string encoded{"\""};
            encoded.reserve(value.size() + 2u);
            for (const char character : value)
            {
                const auto byte = static_cast<unsigned char>(character);
                switch (character)
                {
                case '"': encoded += "\\\""; break;
                case '\\': encoded += "\\\\"; break;
                case '\b': encoded += "\\b"; break;
                case '\f': encoded += "\\f"; break;
                case '\n': encoded += "\\n"; break;
                case '\r': encoded += "\\r"; break;
                case '\t': encoded += "\\t"; break;
                default:
                    if (byte < 0x20u)
                    {
                        encoded += "\\u00";
                        encoded.push_back(hex[(byte >> 4u) & 0x0fu]);
                        encoded.push_back(hex[byte & 0x0fu]);
                    }
                    else
                    {
                        encoded.push_back(character);
                    }
                    break;
                }
            }
            encoded.push_back('"');
            return encoded;
        }

        enum class JsonKind : std::uint8_t
        {
            null_value,
            boolean,
            number,
            string,
            array,
            object
        };

        struct JsonValue final
        {
            JsonKind kind{JsonKind::null_value};
            bool boolean{};
            std::string scalar{};
            std::vector<JsonValue> array{};
            std::vector<std::pair<std::string, JsonValue>> object{};
        };

        class JsonReader final
        {
        public:
            JsonReader(std::string_view source, ProtocolLimits limits) noexcept
                : source_(source), limits_(limits)
            {
            }

            [[nodiscard]] bool parse(JsonValue& value)
            {
                if (!limits_.valid())
                    return fail("The JSON limits are invalid.");
                if (!valid_utf8(source_))
                    return fail("The JSON payload is not valid UTF-8.");
                skip_space();
                if (!parse_value(value, 1u))
                    return false;
                skip_space();
                if (position_ != source_.size())
                    return fail("The JSON payload contains trailing bytes.");
                return true;
            }

            [[nodiscard]] const std::string& status() const noexcept
            {
                return status_;
            }

        private:
            [[nodiscard]] bool fail(std::string status)
            {
                if (status_.empty())
                    status_ = std::move(status);
                return false;
            }

            void skip_space() noexcept
            {
                while (position_ < source_.size())
                {
                    const char character = source_[position_];
                    if (character != ' ' && character != '\t'
                        && character != '\r' && character != '\n')
                    {
                        break;
                    }
                    ++position_;
                }
            }

            [[nodiscard]] bool consume(std::string_view text) noexcept
            {
                if (!source_.substr(position_).starts_with(text))
                    return false;
                position_ += text.size();
                return true;
            }

            [[nodiscard]] static std::optional<std::uint32_t> hex_quad(
                std::string_view text) noexcept
            {
                if (text.size() != 4u)
                    return std::nullopt;
                std::uint32_t value{};
                for (const char character : text)
                {
                    value <<= 4u;
                    if (character >= '0' && character <= '9')
                        value |= static_cast<std::uint32_t>(character - '0');
                    else if (character >= 'a' && character <= 'f')
                        value |= static_cast<std::uint32_t>(character - 'a' + 10);
                    else if (character >= 'A' && character <= 'F')
                        value |= static_cast<std::uint32_t>(character - 'A' + 10);
                    else
                        return std::nullopt;
                }
                return value;
            }

            [[nodiscard]] bool parse_string(std::string& output)
            {
                if (position_ >= source_.size() || source_[position_] != '"')
                    return fail("Expected a JSON string.");
                ++position_;
                while (position_ < source_.size())
                {
                    const char character = source_[position_++];
                    if (character == '"')
                        return true;
                    if (static_cast<unsigned char>(character) < 0x20u)
                        return fail("A JSON string contains a control byte.");
                    if (character != '\\')
                    {
                        output.push_back(character);
                    }
                    else
                    {
                        if (position_ >= source_.size())
                            return fail("A JSON string ends inside an escape.");
                        const char escaped = source_[position_++];
                        switch (escaped)
                        {
                        case '"': output.push_back('"'); break;
                        case '\\': output.push_back('\\'); break;
                        case '/': output.push_back('/'); break;
                        case 'b': output.push_back('\b'); break;
                        case 'f': output.push_back('\f'); break;
                        case 'n': output.push_back('\n'); break;
                        case 'r': output.push_back('\r'); break;
                        case 't': output.push_back('\t'); break;
                        case 'u':
                        {
                            if (source_.size() - position_ < 4u)
                                return fail("A Unicode escape is incomplete.");
                            const auto first = hex_quad(source_.substr(position_, 4u));
                            if (!first)
                                return fail("A Unicode escape is malformed.");
                            position_ += 4u;
                            std::uint32_t codePoint = *first;
                            if (codePoint >= 0xd800u && codePoint <= 0xdbffu)
                            {
                                if (source_.size() - position_ < 6u
                                    || source_[position_] != '\\'
                                    || source_[position_ + 1u] != 'u')
                                {
                                    return fail("A Unicode surrogate pair is incomplete.");
                                }
                                const auto second = hex_quad(
                                    source_.substr(position_ + 2u, 4u));
                                if (!second || *second < 0xdc00u || *second > 0xdfffu)
                                    return fail("A Unicode surrogate pair is malformed.");
                                position_ += 6u;
                                codePoint = 0x10000u
                                    + ((codePoint - 0xd800u) << 10u)
                                    + (*second - 0xdc00u);
                            }
                            else if (codePoint >= 0xdc00u && codePoint <= 0xdfffu)
                            {
                                return fail("A low Unicode surrogate has no leading surrogate.");
                            }
                            append_utf8(output, codePoint);
                            break;
                        }
                        default: return fail("A JSON string escape is unsupported.");
                        }
                    }
                    if (output.size() > limits_.maximum_string_bytes)
                        return fail("A JSON string exceeds the byte budget.");
                }
                return fail("A JSON string is unterminated.");
            }

            [[nodiscard]] bool parse_number(std::string& output)
            {
                const std::size_t begin = position_;
                if (position_ < source_.size() && source_[position_] == '-')
                    ++position_;
                if (position_ >= source_.size())
                    return fail("A JSON number is incomplete.");
                if (source_[position_] == '0')
                {
                    ++position_;
                    if (position_ < source_.size()
                        && source_[position_] >= '0' && source_[position_] <= '9')
                    {
                        return fail("A JSON number has a leading zero.");
                    }
                }
                else if (source_[position_] >= '1' && source_[position_] <= '9')
                {
                    while (position_ < source_.size()
                        && source_[position_] >= '0' && source_[position_] <= '9')
                    {
                        ++position_;
                    }
                }
                else
                {
                    return fail("A JSON number is malformed.");
                }
                if (position_ < source_.size() && source_[position_] == '.')
                {
                    ++position_;
                    const std::size_t fraction = position_;
                    while (position_ < source_.size()
                        && source_[position_] >= '0' && source_[position_] <= '9')
                    {
                        ++position_;
                    }
                    if (position_ == fraction)
                        return fail("A JSON fraction is malformed.");
                }
                if (position_ < source_.size()
                    && (source_[position_] == 'e' || source_[position_] == 'E'))
                {
                    ++position_;
                    if (position_ < source_.size()
                        && (source_[position_] == '+' || source_[position_] == '-'))
                    {
                        ++position_;
                    }
                    const std::size_t exponent = position_;
                    while (position_ < source_.size()
                        && source_[position_] >= '0' && source_[position_] <= '9')
                    {
                        ++position_;
                    }
                    if (position_ == exponent)
                        return fail("A JSON exponent is malformed.");
                }
                output = std::string{source_.substr(begin, position_ - begin)};
                return true;
            }

            [[nodiscard]] bool parse_value(JsonValue& value, std::size_t depth)
            {
                if (depth > limits_.maximum_json_depth)
                    return fail("The JSON nesting depth exceeds the protocol budget.");
                skip_space();
                if (position_ >= source_.size())
                    return fail("The JSON value is missing.");
                const char character = source_[position_];
                if (character == '"')
                {
                    value.kind = JsonKind::string;
                    return parse_string(value.scalar);
                }
                if (character == '{')
                {
                    value.kind = JsonKind::object;
                    ++position_;
                    skip_space();
                    if (position_ < source_.size() && source_[position_] == '}')
                    {
                        ++position_;
                        return true;
                    }
                    for (;;)
                    {
                        if (++field_count_ > limits_.maximum_json_fields
                            || value.object.size() >= limits_.maximum_collection_items)
                        {
                            return fail("The JSON object exceeds the field budget.");
                        }
                        std::string name{};
                        if (!parse_string(name))
                            return false;
                        if (std::any_of(value.object.begin(), value.object.end(),
                            [&name](const auto& field) { return field.first == name; }))
                        {
                            return fail("The JSON object contains a duplicate field.");
                        }
                        skip_space();
                        if (position_ >= source_.size() || source_[position_] != ':')
                            return fail("A JSON object field is missing ':'.");
                        ++position_;
                        JsonValue child{};
                        if (!parse_value(child, depth + 1u))
                            return false;
                        value.object.emplace_back(std::move(name), std::move(child));
                        skip_space();
                        if (position_ >= source_.size())
                            return fail("A JSON object is unterminated.");
                        if (source_[position_] == '}')
                        {
                            ++position_;
                            return true;
                        }
                        if (source_[position_] != ',')
                            return fail("A JSON object is missing ','.");
                        ++position_;
                        skip_space();
                    }
                }
                if (character == '[')
                {
                    value.kind = JsonKind::array;
                    ++position_;
                    skip_space();
                    if (position_ < source_.size() && source_[position_] == ']')
                    {
                        ++position_;
                        return true;
                    }
                    for (;;)
                    {
                        if (value.array.size() >= limits_.maximum_collection_items)
                            return fail("The JSON array exceeds the item budget.");
                        JsonValue child{};
                        if (!parse_value(child, depth + 1u))
                            return false;
                        value.array.push_back(std::move(child));
                        skip_space();
                        if (position_ >= source_.size())
                            return fail("A JSON array is unterminated.");
                        if (source_[position_] == ']')
                        {
                            ++position_;
                            return true;
                        }
                        if (source_[position_] != ',')
                            return fail("A JSON array is missing ','.");
                        ++position_;
                        skip_space();
                    }
                }
                if (consume("true"))
                {
                    value.kind = JsonKind::boolean;
                    value.boolean = true;
                    return true;
                }
                if (consume("false"))
                {
                    value.kind = JsonKind::boolean;
                    value.boolean = false;
                    return true;
                }
                if (consume("null"))
                {
                    value.kind = JsonKind::null_value;
                    return true;
                }
                value.kind = JsonKind::number;
                return parse_number(value.scalar);
            }

            std::string_view source_{};
            ProtocolLimits limits_{};
            std::size_t position_{};
            std::size_t field_count_{};
            std::string status_{};
        };

        [[nodiscard]] const JsonValue* member(
            const JsonValue& value,
            std::string_view name) noexcept
        {
            if (value.kind != JsonKind::object)
                return nullptr;
            const auto found = std::find_if(
                value.object.begin(), value.object.end(),
                [name](const auto& field) { return field.first == name; });
            return found == value.object.end() ? nullptr : &found->second;
        }

        [[nodiscard]] bool only_fields(
            const JsonValue& value,
            std::initializer_list<std::string_view> allowed) noexcept
        {
            if (value.kind != JsonKind::object)
                return false;
            return std::all_of(value.object.begin(), value.object.end(),
                [allowed](const auto& field)
                {
                    return std::find(allowed.begin(), allowed.end(), field.first)
                        != allowed.end();
                });
        }

        [[nodiscard]] std::optional<RequestId> request_id_from_json(
            const JsonValue* value,
            const ProtocolLimits& limits) noexcept
        {
            if (value == nullptr)
                return RequestId{};
            if (value->kind == JsonKind::string)
            {
                RequestId id{RequestIdKind::string, value->scalar, 0};
                return id.valid(limits) ? std::optional<RequestId>{std::move(id)}
                                        : std::nullopt;
            }
            if (value->kind == JsonKind::number
                && value->scalar.find_first_of(".eE") == std::string::npos)
            {
                std::int64_t integer{};
                const auto parsed = std::from_chars(
                    value->scalar.data(),
                    value->scalar.data() + value->scalar.size(), integer);
                if (parsed.ec == std::errc{}
                    && parsed.ptr == value->scalar.data() + value->scalar.size())
                {
                    return RequestId{RequestIdKind::integer, {}, integer};
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] bool notification_payload(
            std::string_view payload,
            ProtocolLimits limits)
        {
            JsonReader reader{payload, limits};
            JsonValue root{};
            if (!reader.parse(root) || root.kind != JsonKind::object
                || member(root, "id") != nullptr)
            {
                return false;
            }
            const JsonValue* method = member(root, "method");
            return method != nullptr && method->kind == JsonKind::string
                && !method->scalar.empty();
        }

        [[nodiscard]] std::string request_id_json(const RequestId& id)
        {
            if (id.kind == RequestIdKind::string)
                return json_string(id.string_value);
            if (id.kind == RequestIdKind::integer)
                return std::to_string(id.integer_value);
            return "null";
        }

        [[nodiscard]] bool supported_protocol(std::string_view version) noexcept
        {
            return version == "2025-11-25";
        }

        [[nodiscard]] bool forbidden_tool_name(std::string_view name)
        {
            const std::string lowered = lower_ascii(name);
            constexpr std::array forbidden{
                std::string_view{"approve"}, std::string_view{"promote"},
                std::string_view{"release"}, std::string_view{"git"}};
            return std::any_of(forbidden.begin(), forbidden.end(),
                [&lowered](std::string_view token)
                {
                    return lowered.find(token) != std::string::npos;
                });
        }

        [[nodiscard]] bool forbidden_argument_name(std::string_view name)
        {
            const std::string lowered = lower_ascii(name);
            constexpr std::array forbidden{
                std::string_view{"path"}, std::string_view{"file"},
                std::string_view{"root"}, std::string_view{"cwd"},
                std::string_view{"command"}, std::string_view{"argv"},
                std::string_view{"executable"}, std::string_view{"url"},
                std::string_view{"uri"}, std::string_view{"git"},
                std::string_view{"approve"}, std::string_view{"promote"},
                std::string_view{"release"}};
            return std::any_of(forbidden.begin(), forbidden.end(),
                [&lowered](std::string_view token)
                {
                    return lowered.find(token) != std::string::npos;
                });
        }

        [[nodiscard]] bool path_like_abuse(std::string_view value) noexcept
        {
            return value.starts_with('/') || value.starts_with('\\')
                || value.find("../") != std::string_view::npos
                || value.find("..\\") != std::string_view::npos
                || (value.size() >= 2u
                    && std::isalpha(static_cast<unsigned char>(value[0u])) != 0
                    && value[1u] == ':');
        }

        [[nodiscard]] std::optional<std::string> scalar_argument(
            const JsonValue& value)
        {
            switch (value.kind)
            {
            case JsonKind::string: return value.scalar;
            case JsonKind::number: return value.scalar;
            case JsonKind::boolean: return value.boolean ? "true" : "false";
            default: return std::nullopt;
            }
        }

        [[nodiscard]] std::string error_payload(
            const RequestId& id,
            int code,
            std::string_view status,
            ProtocolCode protocolCode)
        {
            return "{\"jsonrpc\":\"2.0\",\"id\":" + request_id_json(id)
                + ",\"error\":{\"code\":" + std::to_string(code)
                + ",\"message\":" + json_string(status)
                + ",\"data\":{\"epochCode\":"
                + json_string(protocol_code_name(protocolCode)) + "}}}";
        }

        [[nodiscard]] std::string call_id_for(const RequestId& id)
        {
            return "rpc:" + id.key();
        }

        [[nodiscard]] bool terminal_state(McpCallState state) noexcept
        {
            return state == McpCallState::succeeded
                || state == McpCallState::failed
                || state == McpCallState::cancelled
                || state == McpCallState::rejected;
        }

        [[nodiscard]] std::string_view call_state_name(McpCallState state) noexcept
        {
            switch (state)
            {
            case McpCallState::proposed: return "proposed";
            case McpCallState::awaiting_approval: return "awaiting_approval";
            case McpCallState::running: return "running";
            case McpCallState::succeeded: return "succeeded";
            case McpCallState::failed: return "failed";
            case McpCallState::cancelled: return "cancelled";
            case McpCallState::rejected: return "rejected";
            }
            return "unknown";
        }

        [[nodiscard]] std::string_view mcp_error_name(McpErrorCode error) noexcept
        {
            switch (error)
            {
            case McpErrorCode::none: return "none";
            case McpErrorCode::invalid_request: return "invalid_request";
            case McpErrorCode::unknown_tool: return "unknown_tool";
            case McpErrorCode::capability_denied: return "capability_denied";
            case McpErrorCode::approval_required: return "approval_required";
            case McpErrorCode::invalid_argument: return "invalid_argument";
            case McpErrorCode::path_denied: return "path_denied";
            case McpErrorCode::budget_exhausted: return "budget_exhausted";
            case McpErrorCode::cancelled: return "cancelled";
            case McpErrorCode::execution_failed: return "execution_failed";
            case McpErrorCode::evidence_missing: return "evidence_missing";
            }
            return "unknown";
        }
    }

    FrameDecodeResult decode_frame(
        std::string_view bytes,
        ProtocolLimits limits)
    {
        if (!limits.valid())
            return {FrameCode::invalid_limits, "The framing limits are invalid."};
        const std::size_t newline = bytes.find('\n');
        if (newline == std::string_view::npos)
        {
            if (bytes.size() > limits.maximum_line_bytes)
                return {FrameCode::line_too_large, "The MCP stdio line exceeds its byte budget."};
            return {FrameCode::need_more, "The MCP stdio line is incomplete."};
        }
        if (newline > limits.maximum_line_bytes)
            return {FrameCode::line_too_large, "The MCP stdio line exceeds its byte budget."};
        const std::string_view payload = bytes.substr(0u, newline);
        if (payload.find('\r') != std::string_view::npos)
            return {FrameCode::embedded_newline, "MCP stdio JSON must not contain carriage-return or embedded newline bytes."};
        if (payload.size() > limits.maximum_payload_bytes)
            return {FrameCode::payload_too_large, "The MCP payload exceeds its byte budget."};
        return {
            FrameCode::ready,
            "One bounded newline-delimited MCP stdio message is ready.",
            std::string{payload},
            newline + 1u};
    }

    std::string encode_frame(std::string_view payload, ProtocolLimits limits)
    {
        if (!limits.valid() || payload.size() > limits.maximum_payload_bytes
            || payload.size() > limits.maximum_line_bytes
            || payload.find_first_of("\r\n") != std::string_view::npos)
            return {};
        return std::string{payload} + "\n";
    }

    bool SessionId::valid(const ProtocolLimits& limits) const noexcept
    {
        return limits.valid()
            && usable_identifier(value, limits.maximum_identifier_bytes);
    }

    bool RequestId::valid(const ProtocolLimits& limits) const noexcept
    {
        if (!limits.valid())
            return false;
        if (kind == RequestIdKind::integer)
            return true;
        if (kind == RequestIdKind::string)
            return usable_identifier(string_value, limits.maximum_identifier_bytes);
        return kind == RequestIdKind::absent;
    }

    std::string RequestId::key() const
    {
        if (kind == RequestIdKind::string)
            return "s:" + string_value;
        if (kind == RequestIdKind::integer)
            return "i:" + std::to_string(integer_value);
        return "absent";
    }

    RequestDecodeResult decode_request(
        std::string_view payload,
        ProtocolLimits limits)
    {
        if (!limits.valid())
            return {ProtocolCode::invalid_limits, "The MCP protocol limits are invalid."};
        if (payload.empty() || payload.size() > limits.maximum_payload_bytes)
            return {ProtocolCode::parse_error, "The JSON-RPC payload is empty or oversized."};

        JsonReader reader{payload, limits};
        JsonValue root{};
        if (!reader.parse(root))
            return {ProtocolCode::parse_error, reader.status()};
        if (root.kind != JsonKind::object
            || !only_fields(root, {"jsonrpc", "id", "method", "params"}))
        {
            return {ProtocolCode::invalid_request, "The JSON-RPC request envelope is not exact."};
        }
        const JsonValue* jsonrpc = member(root, "jsonrpc");
        const JsonValue* methodValue = member(root, "method");
        if (jsonrpc == nullptr || jsonrpc->kind != JsonKind::string
            || jsonrpc->scalar != "2.0" || methodValue == nullptr
            || methodValue->kind != JsonKind::string
            || !usable_text(methodValue->scalar, 64u))
        {
            return {ProtocolCode::invalid_request, "The JSON-RPC version or method is invalid."};
        }
        const auto id = request_id_from_json(member(root, "id"), limits);
        if (!id)
            return {ProtocolCode::invalid_request, "The JSON-RPC request ID is invalid."};

        Request request{};
        request.id = *id;
        request.method_name = methodValue->scalar;
        if (request.method_name == "initialize")
            request.method = Method::initialize;
        else if (request.method_name == "tools/list")
            request.method = Method::tools_list;
        else if (request.method_name == "tools/call")
            request.method = Method::tools_call;
        else if (request.method_name == "notifications/initialized")
            request.method = Method::initialized_notification;
        else if (request.method_name == "notifications/cancelled")
            request.method = Method::cancelled_notification;
        else
            request.method = Method::unknown;

        const JsonValue* params = member(root, "params");
        if (request.method == Method::unknown)
            return {ProtocolCode::ready, "The unknown method is well-framed and will fail closed.", std::move(request)};

        if (request.method == Method::initialized_notification)
        {
            if (!request.notification()
                || (params != nullptr
                    && (params->kind != JsonKind::object
                        || !params->object.empty())))
            {
                return {ProtocolCode::invalid_request, "Initialized must be an empty JSON-RPC notification."};
            }
            return {ProtocolCode::ready, "The initialized notification is valid.", std::move(request)};
        }

        if (request.method == Method::cancelled_notification)
        {
            if (!request.notification() || params == nullptr
                || params->kind != JsonKind::object
                || !only_fields(*params, {"requestId", "reason"}))
            {
                return {ProtocolCode::invalid_request, "Cancellation must be an exact notification."};
            }
            const auto target = request_id_from_json(member(*params, "requestId"), limits);
            if (!target || target->kind == RequestIdKind::absent)
                return {ProtocolCode::invalid_request, "Cancellation requires a valid target request ID."};
            request.cancellation.request_id = *target;
            if (const JsonValue* reason = member(*params, "reason"))
            {
                if (reason->kind != JsonKind::string
                    || !usable_text(reason->scalar, 512u, true))
                {
                    return {ProtocolCode::invalid_request, "The cancellation reason is invalid."};
                }
                request.cancellation.reason = reason->scalar;
            }
            return {ProtocolCode::ready, "The cancellation notification is valid.", std::move(request)};
        }

        if (request.notification())
            return {ProtocolCode::invalid_request, "This MCP request requires a typed request ID."};
        if (params == nullptr)
        {
            static const JsonValue emptyObject{.kind = JsonKind::object};
            params = &emptyObject;
        }
        if (params->kind != JsonKind::object)
            return {ProtocolCode::invalid_request, "MCP params must be a JSON object."};

        if (request.method == Method::initialize)
        {
            if (!only_fields(*params, {"protocolVersion", "capabilities", "clientInfo", "_meta"}))
                return {ProtocolCode::invalid_request, "Initialize params contain unsupported fields."};
            const JsonValue* protocolVersion = member(*params, "protocolVersion");
            const JsonValue* capabilities = member(*params, "capabilities");
            const JsonValue* clientInfo = member(*params, "clientInfo");
            if (protocolVersion == nullptr || protocolVersion->kind != JsonKind::string
                || !supported_protocol(protocolVersion->scalar)
                || capabilities == nullptr || capabilities->kind != JsonKind::object
                || clientInfo == nullptr || clientInfo->kind != JsonKind::object
                || !only_fields(*clientInfo, {"name", "version", "title", "description", "icons", "websiteUrl"}))
            {
                return {ProtocolCode::invalid_request, "Initialize params do not match the supported contract."};
            }
            if (const JsonValue* meta = member(*params, "_meta");
                meta != nullptr && meta->kind != JsonKind::object)
            {
                return {ProtocolCode::invalid_request, "Initialize metadata must be a bounded object."};
            }
            const JsonValue* clientName = member(*clientInfo, "name");
            const JsonValue* clientVersion = member(*clientInfo, "version");
            if (clientName == nullptr || clientName->kind != JsonKind::string
                || clientVersion == nullptr || clientVersion->kind != JsonKind::string
                || !usable_text(clientName->scalar, 128u)
                || !usable_text(clientVersion->scalar, 64u))
            {
                return {ProtocolCode::invalid_request, "Initialize client identity is invalid."};
            }
            for (const auto [field, maximumBytes] : {
                std::pair{std::string_view{"title"}, std::size_t{256u}},
                std::pair{std::string_view{"description"}, std::size_t{1024u}},
                std::pair{std::string_view{"websiteUrl"}, std::size_t{4096u}}})
            {
                if (const JsonValue* optional = member(*clientInfo, field);
                    optional != nullptr
                    && (optional->kind != JsonKind::string
                        || !usable_text(optional->scalar, maximumBytes, true)))
                {
                    return {ProtocolCode::invalid_request, "Initialize client metadata is invalid."};
                }
            }
            if (const JsonValue* icons = member(*clientInfo, "icons"))
            {
                if (icons->kind != JsonKind::array || icons->array.size() > 8u)
                    return {ProtocolCode::invalid_request, "Initialize client icons exceed the bounded contract."};
                for (const JsonValue& icon : icons->array)
                {
                    if (icon.kind != JsonKind::object
                        || !only_fields(icon, {"src", "mimeType", "sizes", "theme"}))
                    {
                        return {ProtocolCode::invalid_request, "An initialize client icon is invalid."};
                    }
                    const JsonValue* src = member(icon, "src");
                    if (src == nullptr || src->kind != JsonKind::string
                        || !usable_text(src->scalar, 4096u))
                    {
                        return {ProtocolCode::invalid_request, "An initialize client icon source is invalid."};
                    }
                    if (const JsonValue* mime = member(icon, "mimeType");
                        mime != nullptr && (mime->kind != JsonKind::string
                            || !usable_text(mime->scalar, 128u)))
                    {
                        return {ProtocolCode::invalid_request, "An initialize client icon MIME type is invalid."};
                    }
                    if (const JsonValue* theme = member(icon, "theme");
                        theme != nullptr && (theme->kind != JsonKind::string
                            || (theme->scalar != "light" && theme->scalar != "dark")))
                    {
                        return {ProtocolCode::invalid_request, "An initialize client icon theme is invalid."};
                    }
                    if (const JsonValue* sizes = member(icon, "sizes"))
                    {
                        if (sizes->kind != JsonKind::array || sizes->array.size() > 16u)
                            return {ProtocolCode::invalid_request, "Initialize client icon sizes exceed the bounded contract."};
                        for (const JsonValue& size : sizes->array)
                        {
                            if (size.kind != JsonKind::string
                                || !usable_text(size.scalar, 64u))
                            {
                                return {ProtocolCode::invalid_request, "An initialize client icon size is invalid."};
                            }
                        }
                    }
                }
            }
            request.initialize = {
                protocolVersion->scalar,
                clientName->scalar,
                clientVersion->scalar};
            return {ProtocolCode::ready, "The initialize request is valid.", std::move(request)};
        }

        if (request.method == Method::tools_list)
        {
            if (!params->object.empty())
                return {ProtocolCode::invalid_request, "This bounded tools/list contract does not accept cursor fields."};
            return {ProtocolCode::ready, "The tools/list request is valid.", std::move(request)};
        }

        if (!only_fields(*params, {"name", "arguments", "_meta"}))
            return {ProtocolCode::invalid_arguments, "tools/call params contain unsupported fields."};
        const JsonValue* tool = member(*params, "name");
        const JsonValue* arguments = member(*params, "arguments");
        if (tool == nullptr || tool->kind != JsonKind::string
            || !usable_identifier(tool->scalar, limits.maximum_identifier_bytes)
            || forbidden_tool_name(tool->scalar)
            || arguments == nullptr || arguments->kind != JsonKind::object)
        {
            return {ProtocolCode::invalid_arguments, "The tools/call name or argument object is invalid."};
        }
        request.tool_call.tool = tool->scalar;
        if (arguments->object.size() > 32u)
            return {ProtocolCode::budget_exhausted, "The tools/call argument count exceeds the protocol budget."};
        for (const auto& [name, value] : arguments->object)
        {
            if (!usable_identifier(name, 64u))
                return {ProtocolCode::invalid_arguments, "A tool argument name is invalid."};
            const auto scalar = scalar_argument(value);
            if (!scalar)
                return {ProtocolCode::invalid_arguments, "Nested tool arguments are forbidden."};
            if (forbidden_argument_name(name) || path_like_abuse(*scalar))
                return {ProtocolCode::path_denied, "Model-selected paths, commands, URLs, release, approval, promotion, and Git arguments are forbidden."};
            if (!usable_text(*scalar, limits.maximum_string_bytes, true))
                return {ProtocolCode::invalid_arguments, "A tool argument value is invalid."};
            request.tool_call.arguments.push_back({name, *scalar});
        }
        if (const JsonValue* meta = member(*params, "_meta"))
        {
            if (meta->kind != JsonKind::object || !only_fields(*meta, {"epochHop"}))
                return {ProtocolCode::invalid_arguments, "The tools/call metadata is invalid."};
            if (const JsonValue* hop = member(*meta, "epochHop"))
            {
                if (hop->kind != JsonKind::number
                    || hop->scalar.find_first_of("-.eE") != std::string::npos)
                {
                    return {ProtocolCode::invalid_arguments, "The MCP hop count must be an unsigned integer."};
                }
                std::uint32_t parsedHop{};
                const auto parsed = std::from_chars(
                    hop->scalar.data(), hop->scalar.data() + hop->scalar.size(), parsedHop);
                if (parsed.ec != std::errc{}
                    || parsed.ptr != hop->scalar.data() + hop->scalar.size())
                {
                    return {ProtocolCode::invalid_arguments, "The MCP hop count is invalid."};
                }
                request.tool_call.hop = parsedHop;
            }
        }
        if (request.tool_call.hop > limits.maximum_hops)
            return {ProtocolCode::budget_exhausted, "The MCP hop budget is exhausted."};
        return {ProtocolCode::ready, "The tools/call request is valid.", std::move(request)};
    }

    ProtocolSession::ProtocolSession(
        SessionId sessionId,
        McpToolRegistry registry,
        McpSessionAuthority authority,
        ProtocolLimits limits,
        ValidationHooks hooks)
        : session_id_(std::move(sessionId)),
          registry_(std::move(registry)),
          authority_(std::move(authority)),
          limits_(limits),
          hooks_(hooks)
    {
    }

    DispatchResult ProtocolSession::error(
        ProtocolCode code,
        const RequestId& id,
        std::string status,
        int jsonRpcCode) const
    {
        return {
            .code = code,
            .status = status,
            .response_frame = encode_frame(
                error_payload(id, jsonRpcCode, status, code), limits_),
            .request_id = id};
    }

    DispatchResult ProtocolSession::dispatch_frame(std::string_view bytes)
    {
        const FrameDecodeResult framed = decode_frame(bytes, limits_);
        if (!framed)
        {
            if (framed.code == FrameCode::need_more)
                return {.code = ProtocolCode::need_more, .status = framed.status};
            DispatchResult failed = error(
                framed.code == FrameCode::invalid_limits
                    ? ProtocolCode::invalid_limits
                    : ProtocolCode::frame_error,
                {}, framed.status, -32700);
            return failed;
        }

        const RequestDecodeResult decoded = decode_request(framed.payload, limits_);
        if (!decoded)
        {
            if (notification_payload(framed.payload, limits_))
            {
                return {
                    .code = decoded.code,
                    .status = decoded.status,
                    .consumed_bytes = framed.consumed_bytes};
            }
            const int rpcCode = decoded.code == ProtocolCode::parse_error
                ? -32700 : -32600;
            DispatchResult failed = error(decoded.code, {}, decoded.status, rpcCode);
            failed.consumed_bytes = framed.consumed_bytes;
            return failed;
        }

        if (!session_id_.valid(limits_) || !limits_.valid()
            || authority_.session_id != session_id_.value)
        {
            DispatchResult failed = error(
                ProtocolCode::capability_denied,
                decoded.request.id,
                "The host session authority is invalid.", -32001);
            failed.consumed_bytes = framed.consumed_bytes;
            return failed;
        }

        if (!decoded.request.notification())
        {
            const std::string key = decoded.request.id.key();
            if (std::find(seen_request_ids_.begin(), seen_request_ids_.end(), key)
                != seen_request_ids_.end())
            {
                DispatchResult failed = error(
                    ProtocolCode::duplicate_request,
                    decoded.request.id,
                    "The JSON-RPC request ID was already used in this session.",
                    -32600);
                failed.consumed_bytes = framed.consumed_bytes;
                return failed;
            }
            if (seen_request_ids_.size() >= limits_.maximum_requests)
            {
                DispatchResult failed = error(
                    ProtocolCode::budget_exhausted,
                    decoded.request.id,
                    "The JSON-RPC request budget is exhausted.",
                    -32003);
                failed.consumed_bytes = framed.consumed_bytes;
                return failed;
            }
            seen_request_ids_.push_back(key);
        }

        DispatchResult result = dispatch(decoded.request);
        result.consumed_bytes = framed.consumed_bytes;
        return result;
    }

    DispatchResult ProtocolSession::dispatch(const Request& request)
    {
        if (cancelled_ && request.method != Method::cancelled_notification
            && request.method != Method::initialized_notification)
            return error(ProtocolCode::cancelled, request.id, "The MCP session is cancelled.", -32800);

        if (request.method == Method::unknown)
        {
            if (request.notification())
            {
                return {
                    .code = ProtocolCode::unknown_method,
                    .status = "The unknown JSON-RPC notification was ignored without a response."};
            }
            return error(ProtocolCode::unknown_method, request.id, "The JSON-RPC method is not exposed.", -32601);
        }

        if (request.method == Method::initialize)
        {
            if (initialize_accepted_)
                return error(ProtocolCode::already_initialized, request.id, "The MCP session is already initialized.", -32600);
            initialize_accepted_ = true;
            const std::string payload =
                "{\"jsonrpc\":\"2.0\",\"id\":" + request_id_json(request.id)
                + ",\"result\":{\"protocolVersion\":"
                + json_string(request.initialize.protocol_version)
                + ",\"capabilities\":{\"tools\":{\"listChanged\":false}},"
                  "\"serverInfo\":{\"name\":\"Epoch MCP codec\",\"version\":\"1\"}}}";
            return {
                .code = ProtocolCode::ready,
                .status = "The initialize response is ready; operations remain gated until notifications/initialized.",
                .response_frame = encode_frame(payload, limits_),
                .request_id = request.id};
        }

        if (request.method == Method::initialized_notification)
        {
            if (!initialize_accepted_ || initialized_)
            {
                return {
                    .code = ProtocolCode::invalid_request,
                    .status = "The initialized notification is out of lifecycle order."};
            }
            initialized_ = true;
            return {
                .code = ProtocolCode::ready,
                .status = "The MCP lifecycle is initialized.",
                .request_id = request.id};
        }

        if (request.method == Method::cancelled_notification)
        {
            const std::string targetKey = request.cancellation.request_id.key();
            const auto pending = std::find_if(
                pending_calls_.begin(), pending_calls_.end(),
                [&targetKey](const PendingCall& call)
                {
                    return call.request_id.key() == targetKey;
                });
            if (pending == pending_calls_.end())
            {
                return {
                    .code = ProtocolCode::replay_denied,
                    .status = "The cancellation target is not pending.",
                    .request_id = request.cancellation.request_id};
            }
            const RequestId target = pending->request_id;
            pending_calls_.erase(pending);
            terminal_request_ids_.push_back(targetKey);
            return error(
                ProtocolCode::cancelled,
                target,
                request.cancellation.reason.empty()
                    ? "The tool call was cancelled."
                    : request.cancellation.reason,
                -32800);
        }

        if (!initialized_)
            return error(ProtocolCode::not_initialized, request.id, "Initialize and notifications/initialized must complete before tool requests.", -32002);

        if (request.method == Method::tools_list)
        {
            std::string toolsJson{};
            std::size_t count{};
            for (const McpToolDescriptor& descriptor : registry_.tools())
            {
                if (!descriptor.available || forbidden_tool_name(descriptor.name)
                    || !has_capability(authority_.granted_capabilities, descriptor.capabilities))
                {
                    continue;
                }
                if (hooks_.capability != nullptr
                    && !hooks_.capability(hooks_.context, descriptor, authority_))
                {
                    continue;
                }
                if (++count > limits_.maximum_tools)
                    return error(ProtocolCode::budget_exhausted, request.id, "The tools/list result exceeds the tool budget.", -32003);
                if (!toolsJson.empty())
                    toolsJson.push_back(',');
                const std::string_view schema = descriptor.input_schema_json.empty()
                    ? std::string_view{"{\"type\":\"object\",\"additionalProperties\":false}"}
                    : std::string_view{descriptor.input_schema_json};
                toolsJson += "{\"name\":" + json_string(descriptor.name)
                    + ",\"title\":" + json_string(descriptor.title)
                    + ",\"description\":" + json_string(descriptor.description)
                    + ",\"inputSchema\":" + std::string{schema} + "}";
            }
            const std::string payload =
                "{\"jsonrpc\":\"2.0\",\"id\":" + request_id_json(request.id)
                + ",\"result\":{\"tools\":[" + toolsJson + "]}}";
            return {
                .code = ProtocolCode::ready,
                .status = "The capability-filtered tool list is ready.",
                .response_frame = encode_frame(payload, limits_),
                .request_id = request.id};
        }

        if (call_count_ >= limits_.maximum_calls)
            return error(ProtocolCode::budget_exhausted, request.id, "The MCP call budget is exhausted.", -32003);
        const McpToolDescriptor* descriptor = registry_.find(request.tool_call.tool);
        if (descriptor == nullptr || forbidden_tool_name(request.tool_call.tool))
            return error(ProtocolCode::invalid_arguments, request.id, "The requested tool is unavailable.", -32602);

        const std::string callId = call_id_for(request.id);
        McpToolCall call{
            .session_id = session_id_.value,
            .call_id = callId,
            .tool = request.tool_call.tool,
            .arguments = request.tool_call.arguments,
            .step = static_cast<std::uint32_t>(call_count_)};
        const McpValidation validation = registry_.validate(call, authority_);
        if (validation.error == McpErrorCode::capability_denied)
            return error(ProtocolCode::capability_denied, request.id, validation.message, -32001);
        if (validation.error == McpErrorCode::approval_required)
            return error(ProtocolCode::approval_required, request.id, validation.message, -32004);
        if (validation.error == McpErrorCode::cancelled)
            return error(ProtocolCode::cancelled, request.id, validation.message, -32800);
        if (!validation)
            return error(ProtocolCode::invalid_arguments, request.id, validation.message, -32602);
        if (hooks_.capability != nullptr
            && !hooks_.capability(hooks_.context, *descriptor, authority_))
        {
            return error(ProtocolCode::capability_denied, request.id, "The host capability hook denied the exact tool.", -32001);
        }
        if (McpToolRegistry::requires_operator_approval(descriptor->risk)
            && hooks_.approval != nullptr
            && !hooks_.approval(hooks_.context, callId, *descriptor, authority_))
        {
            return error(ProtocolCode::approval_required, request.id, "The host approval hook denied the exact call.", -32004);
        }

        ++call_count_;
        pending_calls_.push_back({request.id, callId});
        return {
            .code = ProtocolCode::call_pending,
            .status = "The bounded tool call is validated and awaits host dispatch.",
            .pending_call = std::move(call),
            .request_id = request.id};
    }

    DispatchResult ProtocolSession::complete_call(
        const RequestId& requestId,
        const McpToolResult& result)
    {
        const std::string key = requestId.key();
        if (std::find(terminal_request_ids_.begin(), terminal_request_ids_.end(), key)
            != terminal_request_ids_.end())
        {
            return error(ProtocolCode::replay_denied, requestId, "The tool result was already completed or cancelled.", -32600);
        }
        const auto pending = std::find_if(
            pending_calls_.begin(), pending_calls_.end(),
            [&key](const PendingCall& call) { return call.request_id.key() == key; });
        if (pending == pending_calls_.end())
            return error(ProtocolCode::replay_denied, requestId, "No pending tool call matches this result.", -32600);
        if (result.session_id != session_id_.value || result.call_id != pending->call_id
            || !terminal_state(result.state)
            || result.output.size() > registry_.limits().maximum_result_bytes
            || result.evidence.size() > registry_.limits().maximum_evidence_items)
        {
            return error(ProtocolCode::result_invalid, requestId, "The host result does not match the pending bounded call.", -32603);
        }
        std::string evidenceJson{};
        for (const McpEvidenceAttachment& evidence : result.evidence)
        {
            if (!usable_text(evidence.kind, 64u)
                || !usable_text(evidence.path, 4096u)
                || !usable_text(evidence.content_hash, 256u, true)
                || !usable_text(evidence.summary, 4096u, true))
            {
                return error(ProtocolCode::result_invalid, requestId, "A host evidence attachment is invalid.", -32603);
            }
            if (!evidenceJson.empty())
                evidenceJson.push_back(',');
            evidenceJson += "{\"kind\":" + json_string(evidence.kind)
                + ",\"path\":" + json_string(evidence.path)
                + ",\"contentHash\":" + json_string(evidence.content_hash)
                + ",\"summary\":" + json_string(evidence.summary) + "}";
        }
        const bool isError = result.state != McpCallState::succeeded;
        const std::string payload =
            "{\"jsonrpc\":\"2.0\",\"id\":" + request_id_json(requestId)
            + ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":"
            + json_string(result.output) + "}],\"isError\":"
            + (isError ? "true" : "false")
            + ",\"_meta\":{\"state\":" + json_string(call_state_name(result.state))
            + ",\"error\":" + json_string(mcp_error_name(result.error))
            + ",\"elapsedMilliseconds\":" + std::to_string(result.elapsed_milliseconds)
            + ",\"outputTruncated\":" + (result.output_truncated ? "true" : "false")
            + ",\"evidence\":[" + evidenceJson + "]}}}";
        pending_calls_.erase(pending);
        terminal_request_ids_.push_back(key);
        return {
            .code = ProtocolCode::ready,
            .status = "The bounded host result response is ready.",
            .response_frame = encode_frame(payload, limits_),
            .request_id = requestId};
    }

    bool ProtocolSession::initialized() const noexcept
    {
        return initialized_;
    }

    bool ProtocolSession::cancelled() const noexcept
    {
        return cancelled_;
    }

    void ProtocolSession::cancel_session() noexcept
    {
        cancelled_ = true;
        authority_.cancellation_requested = true;
    }
}
