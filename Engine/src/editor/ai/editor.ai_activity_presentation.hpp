#pragma once

#include <string_view>

namespace epochengine::editor::ai_activity_presentation
{
    // Keep transport/queue implementation details in retained raw evidence,
    // not as independent milestones before the model has produced any work.
    // This is presentation policy only: the full host evidence stays retained.
    [[nodiscard]] constexpr bool technical_setup_event(std::string_view group,
        std::string_view detail) noexcept
    {
        if (detail.starts_with("Request submitted")) return false;
        if (detail.find("failed") != std::string_view::npos || detail.find("Failed") != std::string_view::npos
            || detail.find("error") != std::string_view::npos || detail.find("Error") != std::string_view::npos
            || detail.find("blocked") != std::string_view::npos || detail.find("denied") != std::string_view::npos
            || detail.find("approval needed") != std::string_view::npos) return false;
        if (group.compare("Workflow") == 0)
        {
            // Stages and actionable failures remain visible.
            return !(detail.starts_with("Preparing candidate")
                || detail.starts_with("Building candidate")
                || detail.starts_with("Validating")
                || detail.starts_with("Running checks")
                || detail.starts_with("Release build")
                || detail.starts_with("Release checks")
                || detail.starts_with("Debug build")
                || detail.starts_with("Debug checks")
                || detail.starts_with("Headless build")
                || detail.starts_with("Headless checks")
                || detail.starts_with("Full validation")
                || detail.starts_with("Awaiting approval")
                || detail.starts_with("Stopped")
                || detail.starts_with("Failed")
                || detail.starts_with("Coding blocked"));
        }
        if (group.compare("Assistant") == 0 || group.compare("AI Engine Dev") == 0)
            return !(detail.starts_with("Coding blocked")
                || detail.starts_with("Build failed")
                || detail.starts_with("Candidate ready")
                || detail.starts_with("Changes staged")
                || detail.starts_with("Exact campaign proposal staged")
                || detail.starts_with("Checks passed")
                || detail.starts_with("Validation failed")
                || detail.starts_with("Model-generated changes staged"));
        // System 1 routing, model preparation, readiness, queue messages,
        // source recovery and transport acknowledgements are technical context.
        return group.compare("System 1") == 0 || group.compare("Runtime") == 0 || group.compare("Model") == 0
            || group.compare("System") == 0 || group.compare("Tools") == 0
            || detail.starts_with("Host source recovery")
            || detail.starts_with("Digest-bound proposal request")
            || detail.starts_with("Model request")
            || detail.starts_with("Source already available")
            || detail.starts_with("source request is queued")
            || detail.starts_with("Host started the AI Engine Dev request")
            || detail.starts_with("Host started the Assistant request");
    }
    // Only exact host tool identifiers receive friendly action names.
    [[nodiscard]] constexpr std::string_view tool_action_title(std::string_view name) noexcept
    {
        if (name == "workspace.read") return "Reading files";
        if (name == "workspace.write") return "Writing files";
        if (name == "workspace.patch") return "Applying patch";
        if (name == "process.run") return "Running process";
        return name;
    }
}
