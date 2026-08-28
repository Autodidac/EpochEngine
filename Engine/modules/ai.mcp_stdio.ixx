/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.mcp_stdio;

export import ai.mcp;

export namespace epochengine::ai::mcp_stdio
{
    struct ProtocolLimits final
    {
        std::size_t maximum_line_bytes{256u * 1024u};
        std::size_t maximum_payload_bytes{256u * 1024u};
        std::size_t maximum_json_depth{16u};
        std::size_t maximum_json_fields{256u};
        std::size_t maximum_collection_items{256u};
        std::size_t maximum_string_bytes{64u * 1024u};
        std::size_t maximum_identifier_bytes{96u};
        std::size_t maximum_tools{64u};
        std::size_t maximum_requests{128u};
        std::size_t maximum_calls{32u};
        std::uint32_t maximum_hops{4u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_line_bytes >= 128u
                && maximum_payload_bytes >= 128u
                && maximum_payload_bytes <= maximum_line_bytes
                && maximum_json_depth >= 2u
                && maximum_json_fields >= 8u
                && maximum_collection_items >= 8u
                && maximum_string_bytes >= 32u
                && maximum_string_bytes <= maximum_payload_bytes
                && maximum_identifier_bytes >= 8u
                && maximum_identifier_bytes <= maximum_string_bytes
                && maximum_tools > 0u
                && maximum_requests > 0u
                && maximum_calls > 0u
                && maximum_hops > 0u;
        }
    };

    enum class FrameCode : std::uint8_t
    {
        ready,
        need_more,
        invalid_limits,
        line_too_large,
        embedded_newline,
        payload_too_large
    };

    [[nodiscard]] constexpr std::string_view frame_code_name(
        FrameCode code) noexcept
    {
        switch (code)
        {
        case FrameCode::ready: return "ready";
        case FrameCode::need_more: return "need_more";
        case FrameCode::invalid_limits: return "invalid_limits";
        case FrameCode::line_too_large: return "line_too_large";
        case FrameCode::embedded_newline: return "embedded_newline";
        case FrameCode::payload_too_large: return "payload_too_large";
        }
        return "unknown";
    }

    struct FrameDecodeResult final
    {
        FrameCode code{FrameCode::need_more};
        std::string status{};
        std::string payload{};
        std::size_t consumed_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == FrameCode::ready;
        }
    };

    [[nodiscard]] FrameDecodeResult decode_frame(
        std::string_view bytes,
        ProtocolLimits limits = {});

    [[nodiscard]] std::string encode_frame(
        std::string_view payload,
        ProtocolLimits limits = {});

    struct SessionId final
    {
        std::string value{};

        [[nodiscard]] bool valid(
            const ProtocolLimits& limits = {}) const noexcept;
    };

    enum class RequestIdKind : std::uint8_t
    {
        absent,
        string,
        integer
    };

    struct RequestId final
    {
        RequestIdKind kind{RequestIdKind::absent};
        std::string string_value{};
        std::int64_t integer_value{};

        [[nodiscard]] bool valid(
            const ProtocolLimits& limits = {}) const noexcept;
        [[nodiscard]] std::string key() const;

        [[nodiscard]] friend bool operator==(
            const RequestId& left,
            const RequestId& right) noexcept
        {
            return left.kind == right.kind
                && left.string_value == right.string_value
                && left.integer_value == right.integer_value;
        }
    };

    enum class Method : std::uint8_t
    {
        initialize,
        tools_list,
        tools_call,
        initialized_notification,
        cancelled_notification,
        unknown
    };

    struct InitializeParameters final
    {
        std::string protocol_version{};
        std::string client_name{};
        std::string client_version{};
    };

    struct ToolCallParameters final
    {
        std::string tool{};
        std::vector<McpArgument> arguments{};
        std::uint32_t hop{};
    };

    struct CancellationParameters final
    {
        RequestId request_id{};
        std::string reason{};
    };

    struct Request final
    {
        RequestId id{};
        Method method{Method::unknown};
        std::string method_name{};
        InitializeParameters initialize{};
        ToolCallParameters tool_call{};
        CancellationParameters cancellation{};

        [[nodiscard]] bool notification() const noexcept
        {
            return id.kind == RequestIdKind::absent;
        }
    };

    enum class ProtocolCode : std::uint8_t
    {
        ready,
        need_more,
        invalid_limits,
        frame_error,
        parse_error,
        invalid_request,
        unknown_method,
        not_initialized,
        already_initialized,
        duplicate_request,
        budget_exhausted,
        capability_denied,
        approval_required,
        path_denied,
        invalid_arguments,
        call_pending,
        cancelled,
        replay_denied,
        result_invalid
    };

    [[nodiscard]] constexpr std::string_view protocol_code_name(
        ProtocolCode code) noexcept
    {
        switch (code)
        {
        case ProtocolCode::ready: return "ready";
        case ProtocolCode::need_more: return "need_more";
        case ProtocolCode::invalid_limits: return "invalid_limits";
        case ProtocolCode::frame_error: return "frame_error";
        case ProtocolCode::parse_error: return "parse_error";
        case ProtocolCode::invalid_request: return "invalid_request";
        case ProtocolCode::unknown_method: return "unknown_method";
        case ProtocolCode::not_initialized: return "not_initialized";
        case ProtocolCode::already_initialized: return "already_initialized";
        case ProtocolCode::duplicate_request: return "duplicate_request";
        case ProtocolCode::budget_exhausted: return "budget_exhausted";
        case ProtocolCode::capability_denied: return "capability_denied";
        case ProtocolCode::approval_required: return "approval_required";
        case ProtocolCode::path_denied: return "path_denied";
        case ProtocolCode::invalid_arguments: return "invalid_arguments";
        case ProtocolCode::call_pending: return "call_pending";
        case ProtocolCode::cancelled: return "cancelled";
        case ProtocolCode::replay_denied: return "replay_denied";
        case ProtocolCode::result_invalid: return "result_invalid";
        }
        return "unknown";
    }

    struct RequestDecodeResult final
    {
        ProtocolCode code{ProtocolCode::parse_error};
        std::string status{};
        Request request{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ProtocolCode::ready;
        }
    };

    [[nodiscard]] RequestDecodeResult decode_request(
        std::string_view payload,
        ProtocolLimits limits = {});

    using CapabilityValidationHook = bool (*)(
        void* context,
        const McpToolDescriptor& descriptor,
        const McpSessionAuthority& authority) noexcept;
    using ApprovalValidationHook = bool (*)(
        void* context,
        std::string_view call_id,
        const McpToolDescriptor& descriptor,
        const McpSessionAuthority& authority) noexcept;

    struct ValidationHooks final
    {
        void* context{};
        CapabilityValidationHook capability{};
        ApprovalValidationHook approval{};
    };

    struct DispatchResult final
    {
        ProtocolCode code{ProtocolCode::invalid_request};
        std::string status{};
        std::string response_frame{};
        std::optional<McpToolCall> pending_call{};
        RequestId request_id{};
        std::size_t consumed_bytes{};

        [[nodiscard]] bool has_response() const noexcept
        {
            return !response_frame.empty();
        }
    };

    class ProtocolSession final
    {
    public:
        ProtocolSession(
            SessionId session_id,
            McpToolRegistry registry,
            McpSessionAuthority authority,
            ProtocolLimits limits = {},
            ValidationHooks hooks = {});

        [[nodiscard]] DispatchResult dispatch_frame(std::string_view bytes);
        [[nodiscard]] DispatchResult complete_call(
            const RequestId& request_id,
            const McpToolResult& result);
        [[nodiscard]] bool initialized() const noexcept;
        [[nodiscard]] bool cancelled() const noexcept;
        void cancel_session() noexcept;

    private:
        struct PendingCall final
        {
            RequestId request_id{};
            std::string call_id{};
        };

        [[nodiscard]] DispatchResult dispatch(const Request& request);
        [[nodiscard]] DispatchResult error(
            ProtocolCode code,
            const RequestId& id,
            std::string status,
            int json_rpc_code) const;

        SessionId session_id_{};
        McpToolRegistry registry_{};
        McpSessionAuthority authority_{};
        ProtocolLimits limits_{};
        ValidationHooks hooks_{};
        bool initialized_{};
        bool initialize_accepted_{};
        bool cancelled_{};
        std::size_t call_count_{};
        std::vector<std::string> seen_request_ids_{};
        std::vector<PendingCall> pending_calls_{};
        std::vector<std::string> terminal_request_ids_{};
    };

    [[nodiscard]] bool run_contract();
}
