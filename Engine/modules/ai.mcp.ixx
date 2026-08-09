module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module ai.mcp;

export namespace epochengine::ai
{
    enum class McpToolCapability : std::uint32_t
    {
        none = 0u,
        inspect = 1u << 0u,
        author = 1u << 1u,
        build = 1u << 2u,
        execute = 1u << 3u,
        capture = 1u << 4u,
        diagnose = 1u << 5u,
        engine_source = 1u << 6u,
        network = 1u << 7u
    };

    [[nodiscard]] constexpr McpToolCapability operator|(
        McpToolCapability left, McpToolCapability right) noexcept
    {
        return static_cast<McpToolCapability>(
            static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr bool has_capability(
        McpToolCapability value, McpToolCapability requested) noexcept
    {
        return (static_cast<std::uint32_t>(value) &
                static_cast<std::uint32_t>(requested)) ==
               static_cast<std::uint32_t>(requested);
    }

    enum class McpToolRisk : std::uint8_t
    {
        read_only,
        workspace_write,
        child_process,
        engine_source_write,
        network_service
    };

    enum class McpCallState : std::uint8_t
    {
        proposed,
        awaiting_approval,
        running,
        succeeded,
        failed,
        cancelled,
        rejected
    };

    enum class McpErrorCode : std::uint8_t
    {
        none,
        invalid_request,
        unknown_tool,
        capability_denied,
        approval_required,
        invalid_argument,
        path_denied,
        budget_exhausted,
        cancelled,
        execution_failed,
        evidence_missing
    };

    struct McpLimits final
    {
        std::size_t maximum_tools{64u};
        std::size_t maximum_arguments{32u};
        std::size_t maximum_argument_bytes{64u * 1024u};
        std::size_t maximum_result_bytes{16u * 1024u * 1024u};
        std::size_t maximum_evidence_items{64u};
        std::uint32_t maximum_session_steps{32u};
        std::chrono::seconds maximum_call_time{120};
    };

    struct McpToolDescriptor final
    {
        std::string name{};
        std::string title{};
        std::string description{};
        std::string input_schema_json{};
        McpToolCapability capabilities{McpToolCapability::none};
        McpToolRisk risk{McpToolRisk::read_only};
        bool cancellable{true};
        bool available{true};
    };

    struct McpArgument final
    {
        std::string name{};
        std::string value{};
    };

    struct McpEvidenceAttachment final
    {
        std::string kind{};
        std::string path{};
        std::string content_hash{};
        std::string summary{};
    };

    struct McpToolCall final
    {
        std::string session_id{};
        std::string call_id{};
        std::string tool{};
        std::vector<McpArgument> arguments{};
        McpToolCapability granted_capabilities{McpToolCapability::none};
        std::uint32_t step{};
        bool operator_approved{};
        bool cancellation_requested{};
    };

    struct McpToolResult final
    {
        std::string session_id{};
        std::string call_id{};
        McpCallState state{McpCallState::failed};
        McpErrorCode error{McpErrorCode::none};
        std::string output{};
        std::vector<McpEvidenceAttachment> evidence{};
        std::uint64_t elapsed_milliseconds{};
        bool output_truncated{};
    };

    struct McpValidation final
    {
        McpErrorCode error{McpErrorCode::none};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return error == McpErrorCode::none;
        }
    };

    class McpToolRegistry final
    {
    public:
        explicit McpToolRegistry(McpLimits limits = {}) : limits_(limits) {}

        [[nodiscard]] bool register_tool(McpToolDescriptor descriptor)
        {
            if (descriptor.name.empty() ||
                descriptor.name.size() > 96u ||
                tools_.size() >= limits_.maximum_tools ||
                find(descriptor.name) != nullptr)
                return false;
            tools_.push_back(std::move(descriptor));
            return true;
        }

        [[nodiscard]] const McpToolDescriptor* find(std::string_view name) const noexcept
        {
            const auto found = std::find_if(
                tools_.begin(), tools_.end(),
                [name](const McpToolDescriptor& tool) { return tool.name == name; });
            return found == tools_.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::span<const McpToolDescriptor> tools() const noexcept
        {
            return tools_;
        }

        [[nodiscard]] const McpLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] McpValidation validate(const McpToolCall& call) const
        {
            if (call.session_id.empty() || call.call_id.empty() || call.tool.empty())
                return {McpErrorCode::invalid_request, "Session, call, and tool identity are required."};
            if (call.step >= limits_.maximum_session_steps)
                return {McpErrorCode::budget_exhausted, "Session step budget is exhausted."};
            if (call.arguments.size() > limits_.maximum_arguments)
                return {McpErrorCode::invalid_argument, "Tool argument count exceeds the session limit."};

            std::size_t argumentBytes = 0u;
            for (const auto& argument : call.arguments)
            {
                if (argument.name.empty())
                    return {McpErrorCode::invalid_argument, "Tool argument names cannot be empty."};
                const std::size_t next = argument.name.size() + argument.value.size();
                if (argumentBytes > (std::numeric_limits<std::size_t>::max)() - next)
                    return {McpErrorCode::invalid_argument, "Tool argument size overflowed."};
                argumentBytes += next;
            }
            if (argumentBytes > limits_.maximum_argument_bytes)
                return {McpErrorCode::invalid_argument, "Tool arguments exceed the byte budget."};

            const McpToolDescriptor* descriptor = find(call.tool);
            if (descriptor == nullptr || !descriptor->available)
                return {McpErrorCode::unknown_tool, "Requested tool is unavailable."};
            if (!has_capability(call.granted_capabilities, descriptor->capabilities))
                return {McpErrorCode::capability_denied, "The session did not grant the tool capability."};
            if (requires_operator_approval(descriptor->risk) && !call.operator_approved)
                return {McpErrorCode::approval_required, "The tool requires visible operator approval."};
            if (call.cancellation_requested)
                return {McpErrorCode::cancelled, "The tool call was cancelled before execution."};
            return {};
        }

        [[nodiscard]] static constexpr bool requires_operator_approval(
            McpToolRisk risk) noexcept
        {
            return risk != McpToolRisk::read_only;
        }

    private:
        McpLimits limits_{};
        std::vector<McpToolDescriptor> tools_{};
    };

    [[nodiscard]] inline McpToolRegistry make_epoch_project_tool_registry()
    {
        McpToolRegistry registry{};
        const auto add = [&registry](
            std::string name,
            std::string title,
            std::string description,
            McpToolCapability capabilities,
            McpToolRisk risk) {
            (void)registry.register_tool(McpToolDescriptor{
                .name = std::move(name),
                .title = std::move(title),
                .description = std::move(description),
                .input_schema_json = "{\"type\":\"object\",\"additionalProperties\":false}",
                .capabilities = capabilities,
                .risk = risk});
        };

        add("project.inspect", "Inspect Project", "Read active project, scene, script, and build evidence.",
            McpToolCapability::inspect, McpToolRisk::read_only);
        add("project.create", "Create Project", "Create a project through Epoch's validated project shell.",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add("project.save", "Save Project", "Persist the active project and authoring documents.",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add("document.edit", "Edit Document", "Apply a validated semantic authoring command.",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add("script.edit", "Edit Script", "Edit an allowlisted project script with a reviewable patch.",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add("project.build", "Build Project", "Build the active project and retain compiler evidence.",
            McpToolCapability::build | McpToolCapability::capture, McpToolRisk::child_process);
        add("project.run", "Run Project", "Run an approved built project and retain runtime evidence.",
            McpToolCapability::execute | McpToolCapability::capture, McpToolRisk::child_process);
        add("script.run", "Run Script", "Build and run an approved script through the editor harness.",
            McpToolCapability::build | McpToolCapability::execute | McpToolCapability::capture,
            McpToolRisk::child_process);
        add("project.test", "Test Project", "Run approved project contract tests and collect results.",
            McpToolCapability::build | McpToolCapability::execute | McpToolCapability::capture,
            McpToolRisk::child_process);
        add("editor.capture", "Capture Editor", "Capture visible editor evidence for the active request.",
            McpToolCapability::capture, McpToolRisk::read_only);
        add("diagnostics.read", "Read Diagnostics", "Read bounded build, runtime, and editor diagnostics.",
            McpToolCapability::inspect | McpToolCapability::diagnose, McpToolRisk::read_only);
        return registry;
    }

    struct McpCaptureRecord
    {
        std::string session_id{};
        std::string call_id{};
        std::string server{};
        std::string tool{};
        std::string prompt{};
        std::string normalized_output{};
        std::string source_path{};
        McpCallState state{McpCallState::succeeded};
        McpErrorCode error{McpErrorCode::none};
    };

    [[nodiscard]] inline bool run_mcp_contract()
    {
        const McpToolRegistry registry = make_epoch_project_tool_registry();
        if (registry.find("project.create") == nullptr ||
            registry.find("project.build") == nullptr ||
            registry.find("project.run") == nullptr ||
            registry.find("diagnostics.read") == nullptr)
            return false;

        McpToolCall inspect{
            .session_id = "contract-session",
            .call_id = "inspect-1",
            .tool = "project.inspect",
            .granted_capabilities = McpToolCapability::inspect};
        if (!registry.validate(inspect))
            return false;

        McpToolCall build{
            .session_id = "contract-session",
            .call_id = "build-1",
            .tool = "project.build",
            .granted_capabilities = McpToolCapability::build | McpToolCapability::capture};
        if (registry.validate(build).error != McpErrorCode::approval_required)
            return false;
        build.operator_approved = true;
        return static_cast<bool>(registry.validate(build));
    }
}