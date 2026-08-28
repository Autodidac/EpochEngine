/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

module ai.project_profile;

import core.sha256;

namespace epochengine::ai::project_profile
{
    namespace
    {
        enum class ValueKind : std::uint8_t
        {
            text,
            boolean
        };

        struct JsonValue final
        {
            ValueKind kind{ValueKind::text};
            std::string text{};
            bool boolean{};
        };

        class JsonReader final
        {
        public:
            explicit JsonReader(const std::string_view bytes) : bytes_{bytes} {}

            [[nodiscard]] bool parse(std::map<std::string, JsonValue>& fields)
            {
                skip_space();
                if (!take('{'))
                    return false;
                skip_space();
                if (take('}'))
                    return offset_ == bytes_.size();
                while (offset_ < bytes_.size())
                {
                    std::string key{};
                    if (!parse_string(key))
                        return false;
                    skip_space();
                    if (!take(':'))
                        return false;
                    skip_space();
                    JsonValue value{};
                    if (peek() == '"')
                    {
                        value.kind = ValueKind::text;
                        if (!parse_string(value.text))
                            return false;
                    }
                    else if (take_literal("true"))
                    {
                        value.kind = ValueKind::boolean;
                        value.boolean = true;
                    }
                    else if (take_literal("false"))
                    {
                        value.kind = ValueKind::boolean;
                        value.boolean = false;
                    }
                    else
                    {
                        return false;
                    }
                    if (!fields.emplace(std::move(key), std::move(value)).second)
                        return false;
                    skip_space();
                    if (take('}'))
                    {
                        skip_space();
                        return offset_ == bytes_.size();
                    }
                    if (!take(','))
                        return false;
                    skip_space();
                }
                return false;
            }

        private:
            [[nodiscard]] char peek() const noexcept
            {
                return offset_ < bytes_.size() ? bytes_[offset_] : '\0';
            }

            void skip_space() noexcept
            {
                while (offset_ < bytes_.size()
                    && (bytes_[offset_] == ' ' || bytes_[offset_] == '\t'
                        || bytes_[offset_] == '\r' || bytes_[offset_] == '\n'))
                    ++offset_;
            }

            [[nodiscard]] bool take(const char expected) noexcept
            {
                if (peek() != expected)
                    return false;
                ++offset_;
                return true;
            }

            [[nodiscard]] bool take_literal(const std::string_view literal) noexcept
            {
                if (bytes_.substr(offset_, literal.size()) != literal)
                    return false;
                offset_ += literal.size();
                return true;
            }

            [[nodiscard]] bool parse_string(std::string& result)
            {
                if (!take('"'))
                    return false;
                while (offset_ < bytes_.size())
                {
                    const unsigned char value = static_cast<unsigned char>(bytes_[offset_++]);
                    if (value == '"')
                        return true;
                    if (value < 0x20u)
                        return false;
                    if (value != '\\')
                    {
                        result.push_back(static_cast<char>(value));
                        continue;
                    }
                    if (offset_ >= bytes_.size())
                        return false;
                    const char escaped = bytes_[offset_++];
                    switch (escaped)
                    {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default: return false;
                    }
                }
                return false;
            }

            std::string_view bytes_{};
            std::size_t offset_{};
        };

        [[nodiscard]] std::string_view provider_name(const Provider provider) noexcept
        {
            switch (provider)
            {
            case Provider::disabled: return "disabled";
            case Provider::epoch_local_qwen38: return "epoch_local_qwen38";
            case Provider::external_mcp: return "external_mcp";
            case Provider::engine_selected: return "engine_selected";
            }
            return "disabled";
        }

        [[nodiscard]] std::optional<Provider> parse_provider(const std::string_view name)
        {
            if (name == "disabled")
                return Provider::disabled;
            if (name == "epoch_local_qwen38")
                return Provider::epoch_local_qwen38;
            if (name == "external_mcp")
                return Provider::external_mcp;
            if (name == "engine_selected")
                return Provider::engine_selected;
            return std::nullopt;
        }

        [[nodiscard]] const JsonValue* field(
            const std::map<std::string, JsonValue>& fields,
            const std::string_view name,
            const ValueKind kind)
        {
            const auto found = fields.find(std::string{name});
            return found != fields.end() && found->second.kind == kind
                ? &found->second : nullptr;
        }

        [[nodiscard]] bool boolean_field(
            const std::map<std::string, JsonValue>& fields,
            const std::string_view name,
            const bool expected)
        {
            const auto* value = field(fields, name, ValueKind::boolean);
            return value && value->boolean == expected;
        }

        [[nodiscard]] bool text_field(
            const std::map<std::string, JsonValue>& fields,
            const std::string_view name,
            const std::string_view expected)
        {
            const auto* value = field(fields, name, ValueKind::text);
            return value && value->text == expected;
        }

        [[nodiscard]] CodecResult reject(std::string status)
        {
            return CodecResult{.status = std::move(status)};
        }
    }

    Profile make_profile(const Provider provider)
    {
        const bool enabled = provider != Provider::disabled;
        const bool local = provider == Provider::epoch_local_qwen38;
        const bool external = provider == Provider::external_mcp;
        const bool selected = provider == Provider::engine_selected;
        return Profile{
            .provider = provider,
            .enabled = enabled,
            .model_binding = local ? "os_model_qwen_3_8_27b"
                : (external ? "operator_selected_external"
                    : (selected ? "engine_selected_runtime_model" : "none")),
            .runtime_binding = local ? "epoch_shared"
                : (external ? "external_mcp"
                    : (selected ? "epoch_engine_selected" : "none")),
            .inference_transport = local ? "llama_cpp_cli_child_process"
                : (external ? "operator_managed_openai_compatible"
                    : (selected ? "engine_selected_existing" : "none")),
            .tool_protocol = "epoch_mcp_v1",
            .endpoint_binding = external
                ? "operator_selected_runtime_endpoint"
                : (selected ? "engine_selected_runtime" : "none"),
            .self_iteration = enabled
                ? "operator_approved_project_source_only" : "disabled",
            .project_source_write = enabled,
            .engine_source_write = false,
            .operator_approval_per_iteration = true,
            .auto_start = false,
            .server_or_listener = false,
            .weights_bundled = false,
            .external_provider_preserved = true};
    }

    CodecResult serialize_profile(const Profile& profile)
    {
        if (profile != make_profile(profile.provider))
            return reject("Project AI profile fields are inconsistent with the selected provider or safety policy.");

        std::ostringstream out{};
        out << "{\n"
            << "  \"schema\": \"epoch.project.ai.v1\",\n"
            << "  \"provider\": \"" << provider_name(profile.provider) << "\",\n"
            << "  \"enabled\": " << (profile.enabled ? "true" : "false") << ",\n"
            << "  \"model_binding\": \"" << profile.model_binding << "\",\n"
            << "  \"runtime_binding\": \"" << profile.runtime_binding << "\",\n"
            << "  \"inference_transport\": \"" << profile.inference_transport << "\",\n"
            << "  \"tool_protocol\": \"" << profile.tool_protocol << "\",\n"
            << "  \"endpoint_binding\": \"" << profile.endpoint_binding << "\",\n"
            << "  \"self_iteration\": \"" << profile.self_iteration << "\",\n"
            << "  \"project_source_write\": " << (profile.project_source_write ? "true" : "false") << ",\n"
            << "  \"engine_source_write\": false,\n"
            << "  \"operator_approval_per_iteration\": true,\n"
            << "  \"auto_start\": false,\n"
            << "  \"server_or_listener\": false,\n"
            << "  \"weights_bundled\": false,\n"
            << "  \"external_provider_preserved\": true\n"
            << "}\n";
        CodecResult result{
            .accepted = true,
            .profile = profile,
            .canonical_bytes = out.str(),
            .status = "Project AI profile is canonical and bounded."};
        result.sha256 = core::sha256::hex(
            core::sha256::hash(result.canonical_bytes));
        return result;
    }

    CodecResult parse_profile(const std::string_view bytes)
    {
        if (bytes.empty() || bytes.size() > 32u * 1024u
            || bytes.find('\0') != std::string_view::npos)
            return reject("Project AI profile is empty or exceeds the 32 KiB bound.");

        std::map<std::string, JsonValue> fields{};
        JsonReader reader{bytes};
        if (!reader.parse(fields))
            return reject("Project AI profile is not one strict flat JSON object with unique fields.");

        const bool legacy = fields.size() == 10u
            && text_field(fields, "schema", "epoch.project.ai.v1")
            && text_field(fields, "provider", "disabled")
            && text_field(fields, "inference_transport", "none")
            && text_field(fields, "tool_protocol", "epoch_mcp_v1")
            && text_field(fields, "self_iteration_scope", "project_source_only")
            && boolean_field(fields, "engine_source_write", false)
            && boolean_field(fields, "operator_approval_per_iteration", true)
            && boolean_field(fields, "auto_start", false)
            && boolean_field(fields, "server_or_listener", false)
            && boolean_field(fields, "weights_bundled", false);
        if (legacy)
        {
            CodecResult result = serialize_profile(make_profile(Provider::disabled));
            result.migrated_legacy = result.accepted;
            result.status = "Safe legacy project AI profile accepted and normalized to the canonical disabled profile.";
            return result;
        }

        if (fields.size() != 16u
            || !text_field(fields, "schema", "epoch.project.ai.v1"))
            return reject("Project AI profile has missing, unknown, or unsupported schema fields.");
        const auto* provider_value = field(fields, "provider", ValueKind::text);
        const auto provider = provider_value
            ? parse_provider(provider_value->text) : std::nullopt;
        if (!provider)
            return reject("Project AI profile provider is not supported.");

        const Profile expected = make_profile(*provider);
        if (!boolean_field(fields, "enabled", expected.enabled)
            || !text_field(fields, "model_binding", expected.model_binding)
            || !text_field(fields, "runtime_binding", expected.runtime_binding)
            || !text_field(fields, "inference_transport", expected.inference_transport)
            || !text_field(fields, "tool_protocol", expected.tool_protocol)
            || !text_field(fields, "endpoint_binding", expected.endpoint_binding)
            || !text_field(fields, "self_iteration", expected.self_iteration)
            || !boolean_field(fields, "project_source_write", expected.project_source_write)
            || !boolean_field(fields, "engine_source_write", false)
            || !boolean_field(fields, "operator_approval_per_iteration", true)
            || !boolean_field(fields, "auto_start", false)
            || !boolean_field(fields, "server_or_listener", false)
            || !boolean_field(fields, "weights_bundled", false)
            || !boolean_field(fields, "external_provider_preserved", true))
            return reject("Project AI profile conflicts with provider bindings or guarded source policy.");

        CodecResult result = serialize_profile(expected);
        if (!result)
            return result;
        result.status = "Project AI profile parsed and normalized to canonical bytes.";
        return result;
    }
}
