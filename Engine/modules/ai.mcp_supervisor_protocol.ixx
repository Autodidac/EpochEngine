/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module ai.mcp_supervisor_protocol;

export namespace epochengine::ai::mcp_supervisor_protocol
{
    inline constexpr std::string_view schema{
        "epoch.ai.mcp_supervisor_adapter.request/v1"};

    struct WireRequest final
    {
        std::string id{}, method{}, session_id{}, actor_sha256{}, campaign_id{},
            project_id{}, objective_id{}, operation_id{};
        std::uint64_t curated_session_id{}, expected_adapter_generation{};
        std::string expected_adapter_state_sha256{};
        std::uint64_t expected_control_generation{};
        std::string expected_control_state_sha256{};
        std::uint64_t expected_queue_generation{};
        std::string expected_queue_state_sha256{};
        std::uint64_t expected_scheduler_generation{};
        std::string expected_scheduler_state_sha256{}, curated_bundle_sha256{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};

        friend bool operator==(const WireRequest&, const WireRequest&) = default;
    };

    enum class ParseCode : std::uint8_t
    {
        ready,
        malformed,
        noncanonical
    };

    struct ParseResult final
    {
        ParseCode code{ParseCode::malformed};
        WireRequest request{};
    };

    [[nodiscard]] inline std::string json_string(std::string_view value)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string out{"\""};
        out.reserve(value.size() + 2u);
        for (const unsigned char c : value)
        {
            switch (c)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u)
                {
                    out += "\\u00";
                    out.push_back(digits[c >> 4u]);
                    out.push_back(digits[c & 0x0fu]);
                }
                else out.push_back(static_cast<char>(c));
                break;
            }
        }
        out.push_back('"');
        return out;
    }

    [[nodiscard]] inline std::string canonical_text(const WireRequest& r)
    {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + json_string(r.id)
            + ",\"method\":" + json_string(r.method)
            + ",\"params\":{\"schema\":" + json_string(schema)
            + ",\"session_id\":" + json_string(r.session_id)
            + ",\"actor_sha256\":" + json_string(r.actor_sha256)
            + ",\"campaign_id\":" + json_string(r.campaign_id)
            + ",\"project_id\":" + json_string(r.project_id)
            + ",\"objective_id\":" + json_string(r.objective_id)
            + ",\"operation_id\":" + json_string(r.operation_id)
            + ",\"curated_session_id\":"
            + std::to_string(r.curated_session_id)
            + ",\"expected_adapter_generation\":"
            + std::to_string(r.expected_adapter_generation)
            + ",\"expected_adapter_state_sha256\":"
            + json_string(r.expected_adapter_state_sha256)
            + ",\"expected_control_generation\":"
            + std::to_string(r.expected_control_generation)
            + ",\"expected_control_state_sha256\":"
            + json_string(r.expected_control_state_sha256)
            + ",\"expected_queue_generation\":"
            + std::to_string(r.expected_queue_generation)
            + ",\"expected_queue_state_sha256\":"
            + json_string(r.expected_queue_state_sha256)
            + ",\"expected_scheduler_generation\":"
            + std::to_string(r.expected_scheduler_generation)
            + ",\"expected_scheduler_state_sha256\":"
            + json_string(r.expected_scheduler_state_sha256)
            + ",\"curated_bundle_sha256\":"
            + json_string(r.curated_bundle_sha256)
            + ",\"now_unix_seconds\":" + std::to_string(r.now_unix_seconds)
            + ",\"operator_approved\":"
            + std::string{r.operator_approved ? "true" : "false"} + "}}";
    }

    [[nodiscard]] inline std::vector<std::uint8_t> canonical_bytes(
        const WireRequest& request)
    {
        const std::string text = canonical_text(request);
        return {text.begin(), text.end()};
    }

    namespace detail
    {
        class Cursor final
        {
        public:
            explicit Cursor(std::string_view value) : value_{value} {}
            [[nodiscard]] bool literal(std::string_view expected) noexcept
            {
                whitespace();
                if (!value_.substr(at_).starts_with(expected)) return false;
                at_ += expected.size();
                return true;
            }
            [[nodiscard]] bool key(std::string_view expected)
            {
                std::string parsed{};
                return string(parsed, 128u) && parsed == expected && literal(":");
            }
            [[nodiscard]] bool string(std::string& out, std::size_t maximum)
            {
                whitespace();
                if (at_ >= value_.size() || value_[at_] != '"') return false;
                ++at_; out.clear();
                while (at_ < value_.size())
                {
                    const unsigned char c =
                        static_cast<unsigned char>(value_[at_++]);
                    if (c == '"') return out.size() <= maximum;
                    if (c == '\\' || c < 0x20u || out.size() >= maximum)
                        return false;
                    out.push_back(static_cast<char>(c));
                }
                return false;
            }
            [[nodiscard]] bool number(std::uint64_t& out) noexcept
            {
                whitespace();
                const char* first = value_.data() + at_;
                const auto converted = std::from_chars(
                    first, value_.data() + value_.size(), out);
                if (converted.ec != std::errc{} || converted.ptr == first)
                    return false;
                at_ = static_cast<std::size_t>(converted.ptr - value_.data());
                return true;
            }
            [[nodiscard]] bool boolean(bool& out) noexcept
            {
                if (literal("true")) { out = true; return true; }
                if (literal("false")) { out = false; return true; }
                return false;
            }
            [[nodiscard]] bool complete() noexcept
            {
                whitespace();
                return at_ == value_.size();
            }
        private:
            void whitespace() noexcept
            {
                while (at_ < value_.size()
                    && (value_[at_] == ' ' || value_[at_] == '\t'
                        || value_[at_] == '\r' || value_[at_] == '\n')) ++at_;
            }
            std::string_view value_{};
            std::size_t at_{};
        };
    }

    [[nodiscard]] inline ParseResult parse(
        std::span<const std::uint8_t> input)
    {
        const std::string_view text{
            reinterpret_cast<const char*>(input.data()), input.size()};
        detail::Cursor c{text};
        WireRequest r{};
        std::string jsonrpc{}, parsed_schema{};
        if (!c.literal("{")
            || !c.key("jsonrpc") || !c.string(jsonrpc, 8u)
            || !c.literal(",") || !c.key("id") || !c.string(r.id, 128u)
            || !c.literal(",") || !c.key("method")
            || !c.string(r.method, 128u)
            || !c.literal(",") || !c.key("params") || !c.literal("{")
            || !c.key("schema") || !c.string(parsed_schema, 128u)
            || !c.literal(",") || !c.key("session_id")
            || !c.string(r.session_id, 128u)
            || !c.literal(",") || !c.key("actor_sha256")
            || !c.string(r.actor_sha256, 64u)
            || !c.literal(",") || !c.key("campaign_id")
            || !c.string(r.campaign_id, 128u)
            || !c.literal(",") || !c.key("project_id")
            || !c.string(r.project_id, 128u)
            || !c.literal(",") || !c.key("objective_id")
            || !c.string(r.objective_id, 128u)
            || !c.literal(",") || !c.key("operation_id")
            || !c.string(r.operation_id, 128u)
            || !c.literal(",") || !c.key("curated_session_id")
            || !c.number(r.curated_session_id)
            || !c.literal(",") || !c.key("expected_adapter_generation")
            || !c.number(r.expected_adapter_generation)
            || !c.literal(",") || !c.key("expected_adapter_state_sha256")
            || !c.string(r.expected_adapter_state_sha256, 64u)
            || !c.literal(",") || !c.key("expected_control_generation")
            || !c.number(r.expected_control_generation)
            || !c.literal(",") || !c.key("expected_control_state_sha256")
            || !c.string(r.expected_control_state_sha256, 64u)
            || !c.literal(",") || !c.key("expected_queue_generation")
            || !c.number(r.expected_queue_generation)
            || !c.literal(",") || !c.key("expected_queue_state_sha256")
            || !c.string(r.expected_queue_state_sha256, 64u)
            || !c.literal(",") || !c.key("expected_scheduler_generation")
            || !c.number(r.expected_scheduler_generation)
            || !c.literal(",") || !c.key("expected_scheduler_state_sha256")
            || !c.string(r.expected_scheduler_state_sha256, 64u)
            || !c.literal(",") || !c.key("curated_bundle_sha256")
            || !c.string(r.curated_bundle_sha256, 64u)
            || !c.literal(",") || !c.key("now_unix_seconds")
            || !c.number(r.now_unix_seconds)
            || !c.literal(",") || !c.key("operator_approved")
            || !c.boolean(r.operator_approved)
            || !c.literal("}") || !c.literal("}") || !c.complete()
            || jsonrpc != "2.0" || parsed_schema != schema)
            return {};
        return {canonical_text(r) == text
            ? ParseCode::ready : ParseCode::noncanonical, std::move(r)};
    }

    [[nodiscard]] inline bool run_contract()
    {
        WireRequest request{
            "request-1", "epoch.campaign.query", "session-1",
            std::string(64u, 'a'), "campaign-1", "project-1",
            "objective-1", "operation-1", 7u, 1u,
            std::string(64u, 'b'), 2u, std::string(64u, 'c'), 3u,
            std::string(64u, 'd'), 4u, std::string(64u, 'e'), {}, 9u, false};
        const auto canonical = canonical_bytes(request);
        const auto parsed = parse(canonical);
        if (parsed.code != ParseCode::ready || parsed.request != request)
            return false;
        std::vector<std::uint8_t> spaced = canonical;
        spaced.insert(spaced.begin() + 1u, static_cast<std::uint8_t>(' '));
        if (parse(spaced).code != ParseCode::noncanonical) return false;
        spaced.push_back(static_cast<std::uint8_t>('x'));
        return parse(spaced).code == ParseCode::malformed;
    }
}
