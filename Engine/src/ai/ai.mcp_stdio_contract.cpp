/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <string>
#include <string_view>

module ai.mcp_stdio;

namespace epochengine::ai::mcp_stdio
{
    namespace
    {
        struct HookEvidence final
        {
            std::size_t capability_checks{};
            std::size_t approval_checks{};
        };

        [[nodiscard]] bool capability_hook(
            void* context,
            const McpToolDescriptor& descriptor,
            const McpSessionAuthority&) noexcept
        {
            auto& evidence = *static_cast<HookEvidence*>(context);
            ++evidence.capability_checks;
            return descriptor.name != "diagnostics.hidden";
        }

        [[nodiscard]] bool approval_hook(
            void* context,
            std::string_view callId,
            const McpToolDescriptor& descriptor,
            const McpSessionAuthority&) noexcept
        {
            auto& evidence = *static_cast<HookEvidence*>(context);
            ++evidence.approval_checks;
            return callId == "rpc:s:call-save"
                && descriptor.name == "project.save";
        }

        [[nodiscard]] std::string initialize_message(std::string_view id)
        {
            return encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"" + std::string{id}
                + "\",\"method\":\"initialize\",\"params\":{"
                  "\"protocolVersion\":\"2025-11-25\","
                  "\"capabilities\":{},"
                  "\"clientInfo\":{\"name\":\"Epoch contract\","
                  "\"version\":\"1\",\"title\":\"Contract client\","
                  "\"description\":\"Bounded MCP interop fixture\","
                  "\"websiteUrl\":\"https://example.invalid/epoch\","
                  "\"icons\":[{\"src\":\"data:image/png;base64,AA==\","
                  "\"mimeType\":\"image/png\",\"sizes\":[\"16x16\"],"
                  "\"theme\":\"dark\"}]},"
                  "\"_meta\":{\"fixture\":true}}}");
        }

        [[nodiscard]] std::string initialized_notification()
        {
            return encode_frame(
                "{\"jsonrpc\":\"2.0\","
                "\"method\":\"notifications/initialized\"}");
        }

        [[nodiscard]] std::string tools_list_message(std::string_view id)
        {
            return encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"" + std::string{id}
                + "\",\"method\":\"tools/list\",\"params\":{}}");
        }

        [[nodiscard]] std::string tools_call_message(
            std::string_view id,
            std::string_view tool,
            std::string_view arguments = "{}",
            std::string_view meta = {})
        {
            return encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"" + std::string{id}
                + "\",\"method\":\"tools/call\",\"params\":{"
                  "\"name\":\"" + std::string{tool}
                + "\",\"arguments\":" + std::string{arguments}
                + (meta.empty() ? std::string{} : ",\"_meta\":" + std::string{meta})
                + "}}");
        }

        [[nodiscard]] McpToolRegistry contract_registry()
        {
            McpToolRegistry registry{};
            const auto add = [&registry](
                std::string name,
                McpToolCapability capabilities,
                McpToolRisk risk)
            {
                return registry.register_tool({
                    .name = std::move(name),
                    .title = "Contract tool",
                    .description = "Deterministic protocol contract tool.",
                    .input_schema_json =
                        "{\"type\":\"object\",\"additionalProperties\":false}",
                    .capabilities = capabilities,
                    .risk = risk});
            };
            (void)add("project.inspect", McpToolCapability::inspect,
                McpToolRisk::read_only);
            (void)add("project.save", McpToolCapability::author,
                McpToolRisk::workspace_write);
            (void)add("project.build", McpToolCapability::build,
                McpToolRisk::child_process);
            (void)add("diagnostics.hidden", McpToolCapability::diagnose,
                McpToolRisk::read_only);
            (void)add("release.publish", McpToolCapability::author,
                McpToolRisk::workspace_write);
            return registry;
        }

        [[nodiscard]] bool framing_contract()
        {
            if (decode_frame("{}").code != FrameCode::need_more)
                return false;
            const FrameDecodeResult frame = decode_frame("{}\nnext\n");
            if (!frame || frame.payload != "{}" || frame.consumed_bytes != 3u)
                return false;
            if (!encode_frame("{}" ).ends_with('\n')
                || !encode_frame("{\n}").empty()
                || decode_frame("{}\r\n").code != FrameCode::embedded_newline)
            {
                return false;
            }

            ProtocolLimits small{};
            small.maximum_line_bytes = 128u;
            small.maximum_payload_bytes = 128u;
            small.maximum_string_bytes = 64u;
            small.maximum_identifier_bytes = 32u;
            const std::string oversized(129u, 'x');
            return small.valid()
                && decode_frame(oversized, small).code == FrameCode::line_too_large
                && encode_frame(oversized, small).empty();
        }

        [[nodiscard]] bool decoder_abuse_contract()
        {
            const std::string duplicate =
                "{\"jsonrpc\":\"2.0\",\"id\":\"one\",\"id\":\"two\","
                "\"method\":\"tools/list\",\"params\":{}}";
            const std::string path =
                "{\"jsonrpc\":\"2.0\",\"id\":\"path\","
                "\"method\":\"tools/call\",\"params\":{"
                "\"name\":\"project.inspect\",\"arguments\":{"
                "\"path\":\"../../outside\"}}}";
            const std::string nested =
                "{\"jsonrpc\":\"2.0\",\"id\":\"nested\","
                "\"method\":\"tools/call\",\"params\":{"
                "\"name\":\"project.inspect\",\"arguments\":{"
                "\"query\":{\"again\":true}}}}";
            const std::string hop =
                "{\"jsonrpc\":\"2.0\",\"id\":\"hop\","
                "\"method\":\"tools/call\",\"params\":{"
                "\"name\":\"project.inspect\",\"arguments\":{},"
                "\"_meta\":{\"epochHop\":5}}}";
            return decode_request(duplicate).code == ProtocolCode::parse_error
                && decode_request(path).code == ProtocolCode::path_denied
                && decode_request(nested).code == ProtocolCode::invalid_arguments
                && decode_request(hop).code == ProtocolCode::budget_exhausted;
        }

        [[nodiscard]] bool lifecycle_and_call_contract()
        {
            HookEvidence hooks{};
            McpSessionAuthority authority{
                .session_id = "contract-session",
                .granted_capabilities = McpToolCapability::inspect
                    | McpToolCapability::author
                    | McpToolCapability::diagnose,
                .approved_call_ids = {"rpc:s:call-save"}};
            ProtocolSession session{
                SessionId{"contract-session"},
                contract_registry(),
                authority,
                {},
                ValidationHooks{&hooks, capability_hook, approval_hook}};

            const DispatchResult beforeInitialize = session.dispatch_frame(
                tools_list_message("early"));
            if (beforeInitialize.code != ProtocolCode::not_initialized
                || !beforeInitialize.has_response())
            {
                return false;
            }

            const DispatchResult initialize = session.dispatch_frame(
                initialize_message("init"));
            if (initialize.code != ProtocolCode::ready
                || !initialize.has_response()
                || initialize.response_frame.find("2025-11-25") == std::string::npos
                || initialize.response_frame.find("\"tools\"") == std::string::npos
                || initialize.response_frame.find("prompts") != std::string::npos
                || initialize.response_frame.find("resources") != std::string::npos
                || session.initialized())
            {
                return false;
            }

            const DispatchResult beforeNotification = session.dispatch_frame(
                tools_list_message("before-ready"));
            if (beforeNotification.code != ProtocolCode::not_initialized)
                return false;
            const DispatchResult initialized = session.dispatch_frame(
                initialized_notification());
            if (initialized.code != ProtocolCode::ready
                || initialized.has_response() || !session.initialized())
            {
                return false;
            }

            const DispatchResult unknownNotification = session.dispatch_frame(
                encode_frame(
                    "{\"jsonrpc\":\"2.0\","
                    "\"method\":\"notifications/not-exposed\"}"));
            if (unknownNotification.code != ProtocolCode::unknown_method
                || unknownNotification.has_response())
            {
                return false;
            }

            const DispatchResult invalidNotification = session.dispatch_frame(
                encode_frame(
                    "{\"jsonrpc\":\"2.0\","
                    "\"method\":\"notifications/initialized\","
                    "\"params\":{\"unexpected\":true}}"));
            if (invalidNotification.code != ProtocolCode::invalid_request
                || invalidNotification.has_response())
            {
                return false;
            }

            const DispatchResult listed = session.dispatch_frame(
                tools_list_message("list"));
            if (listed.code != ProtocolCode::ready
                || listed.response_frame.find("project.inspect") == std::string::npos
                || listed.response_frame.find("project.save") == std::string::npos
                || listed.response_frame.find("release.publish") != std::string::npos
                || listed.response_frame.find("diagnostics.hidden") != std::string::npos)
            {
                return false;
            }
            if (session.dispatch_frame(tools_list_message("list")).code
                != ProtocolCode::duplicate_request)
            {
                return false;
            }

            const DispatchResult unknown = session.dispatch_frame(encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"unknown\","
                "\"method\":\"roots/list\",\"params\":{}}"));
            if (unknown.code != ProtocolCode::unknown_method)
                return false;
            const DispatchResult denied = session.dispatch_frame(
                tools_call_message("build", "project.build"));
            if (denied.code != ProtocolCode::capability_denied)
                return false;

            const DispatchResult pending = session.dispatch_frame(
                tools_call_message("call-save", "project.save"));
            if (pending.code != ProtocolCode::call_pending
                || !pending.pending_call
                || pending.pending_call->call_id != "rpc:s:call-save"
                || hooks.capability_checks == 0u || hooks.approval_checks != 1u)
            {
                return false;
            }
            const DispatchResult cancelled = session.dispatch_frame(encode_frame(
                "{\"jsonrpc\":\"2.0\","
                "\"method\":\"notifications/cancelled\","
                "\"params\":{\"requestId\":\"call-save\","
                "\"reason\":\"operator cancelled\"}}"));
            if (cancelled.code != ProtocolCode::cancelled
                || !cancelled.has_response())
            {
                return false;
            }
            const McpToolResult lateResult{
                .session_id = "contract-session",
                .call_id = "rpc:s:call-save",
                .state = McpCallState::succeeded,
                .output = "late"};
            if (session.complete_call(
                    RequestId{RequestIdKind::string, "call-save", 0},
                    lateResult).code != ProtocolCode::replay_denied)
            {
                return false;
            }

            const DispatchResult inspect = session.dispatch_frame(
                tools_call_message("call-read", "project.inspect"));
            if (inspect.code != ProtocolCode::call_pending || !inspect.pending_call)
                return false;
            const RequestId inspectId{RequestIdKind::string, "call-read", 0};
            const McpToolResult inspectResult{
                .session_id = "contract-session",
                .call_id = "rpc:s:call-read",
                .state = McpCallState::succeeded,
                .error = McpErrorCode::none,
                .output = "bounded evidence"};
            const DispatchResult completed = session.complete_call(
                inspectId, inspectResult);
            return completed.code == ProtocolCode::ready
                && completed.response_frame.find("bounded evidence")
                    != std::string::npos
                && session.complete_call(inspectId, inspectResult).code
                    == ProtocolCode::replay_denied;
        }

        [[nodiscard]] bool request_budget_contract()
        {
            ProtocolLimits limits{};
            limits.maximum_requests = 2u;
            McpSessionAuthority authority{
                .session_id = "budget-session",
                .granted_capabilities = McpToolCapability::inspect};
            ProtocolSession session{
                SessionId{"budget-session"},
                contract_registry(),
                authority,
                limits};
            if (session.dispatch_frame(initialize_message("budget-init")).code
                    != ProtocolCode::ready
                || session.dispatch_frame(initialized_notification()).code
                    != ProtocolCode::ready
                || session.dispatch_frame(tools_list_message("budget-list")).code
                    != ProtocolCode::ready)
            {
                return false;
            }
            const DispatchResult exhausted = session.dispatch_frame(
                encode_frame(
                    "{\"jsonrpc\":\"2.0\",\"id\":\"budget-third\","
                    "\"method\":\"not/exposed\",\"params\":{}}"));
            if (exhausted.code != ProtocolCode::budget_exhausted
                || !exhausted.has_response())
            {
                return false;
            }
            const DispatchResult notification = session.dispatch_frame(
                encode_frame(
                    "{\"jsonrpc\":\"2.0\","
                    "\"method\":\"notifications/still-ignored\"}"));
            return notification.code == ProtocolCode::unknown_method
                && !notification.has_response();
        }

        [[nodiscard]] int contract_failure_code()
        {
            if (!framing_contract()) return 1;
            if (!decoder_abuse_contract()) return 2;
            if (!lifecycle_and_call_contract()) return 3;
            if (!request_budget_contract()) return 4;
            return 0;
        }
    }

    bool run_contract()
    {
        return contract_failure_code() == 0;
    }
}

#if defined(EPOCH_AI_MCP_STDIO_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::mcp_stdio::contract_failure_code();
}
#endif
