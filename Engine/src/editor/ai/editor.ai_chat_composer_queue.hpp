// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

namespace epochengine::editor::ai_chat_queue
{
    // Pure, provider-independent readiness gate. The chat submitter captures
    // its transport/model before checking this gate; workers are never started
    // just because a loading operation was initiated.
    enum class Transport : unsigned char
    {
        none,
        native,
        local_api,
        direct_gguf,
        external_mcp,
        codex
    };

    struct Status final
    {
        Transport transport{Transport::none};
        bool selected{};
        bool loaded{};
        bool busy{};
        bool confirmed{};
        bool inventory_pending{};
        bool provider_ready{};
        bool positive_loaded_signal{};
        std::chrono::steady_clock::time_point confirmed_at{};
    };

    // Upper bound for waiting on provider/model readiness. This does not
    // unload a resident model and does not limit generation or downloads.
    inline constexpr auto kQueuedLoadTtl = std::chrono::hours{1};

    // Route-internal model changes do not invalidate a user's queued turn.
    // Increment the live revision only for explicit user model selection.
    [[nodiscard]] constexpr bool user_selection_unchanged(
        std::uint64_t queued_revision, std::uint64_t current_revision) noexcept
    {
        return queued_revision == current_revision;
    }

    // Visible reason for a staged message, not a guess at GPU or provider
    // activity. This is intentionally independent of the native model count.
    [[nodiscard]] constexpr std::string_view dispatch_block_reason(
        const Status& status, bool method_matches, bool model_matches,
        std::chrono::steady_clock::time_point now) noexcept;

    [[nodiscard]] constexpr bool queued_load_expired(
        std::chrono::steady_clock::time_point queued_at,
        std::chrono::steady_clock::time_point now) noexcept
    {
        return queued_at != std::chrono::steady_clock::time_point{}
            && now >= queued_at && now - queued_at >= kQueuedLoadTtl;
    }

    [[nodiscard]] constexpr bool ready_to_dispatch(
        const Status& status,
        std::chrono::steady_clock::time_point now) noexcept
    {
        using enum Transport;
        switch (status.transport)
        {
            case native:
                return status.selected && status.loaded && !status.busy;
            case local_api:
                if (!status.confirmed || status.inventory_pending) return false;
                // If a provider can attest residency, trust it, not the clock.
                if (status.positive_loaded_signal)
                    return status.loaded && !status.busy;
                // The API model list/selection is not a residency signal.
                // Only a newly initiated load gets a six-second grace period.
                return status.confirmed_at == std::chrono::steady_clock::time_point{}
                    || now - status.confirmed_at >= std::chrono::seconds{6};
            case direct_gguf:
            case external_mcp:
                return status.provider_ready && !status.busy;
            case codex:
                return status.provider_ready && !status.busy;
            case none:
                return false;
        }
        return false;
    }

    [[nodiscard]] constexpr std::string_view dispatch_block_reason(
        const Status& status, bool method_matches, bool model_matches,
        std::chrono::steady_clock::time_point now) noexcept
    {
        if (!method_matches) return "The selected AI transport changed. Re-send on the chosen transport.";
        if (!model_matches) return "The selected model changed. Re-send to avoid silently changing models.";
        if (ready_to_dispatch(status, now)) return {};
        switch (status.transport)
        {
            case Transport::native:
                if (!status.loaded) return status.busy
                    ? "Native model is still loading." : "No native foundation is loaded. Load & Use a model.";
                if (!status.selected) return "A native model is loaded but not selected for chat. Use Load & Use.";
                return "Native model preparation is still running.";
            case Transport::local_api:
                if (!status.confirmed) return "Confirm the chosen local API model in Model Manager.";
                if (status.inventory_pending) return "Waiting for local endpoint inventory scan.";
                if (status.positive_loaded_signal) return status.loaded
                    ? "Local API model is busy." : "Local API model is not resident.";
                return "Waiting for the newly selected local API model to become ready.";
            case Transport::direct_gguf:
                return "Direct GGUF runtime is not ready.";
            case Transport::external_mcp:
                return "External MCP transport is not ready.";
            case Transport::codex:
                return "Codex is unavailable or busy.";
            case Transport::none:
                return "No AI transport is selected. Select one in Model Manager.";
        }
        return "AI transport is not ready.";
    }
}
