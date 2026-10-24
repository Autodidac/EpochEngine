// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace epoch_ai::controller {

// Safety-first routing policy: only explicitly simple requests are eligible for
// a smaller responder. Ambiguous, code and tool-heavy work goes to Foundation.
// Epoch's project-assistant evidence is context, not part of the user's
// complexity signal. Keep authorization in the editor: this extracts only
// routing text, and never grants the model any additional permissions.
[[nodiscard]] constexpr std::string_view routing_request_text(
    std::string_view message) noexcept
{
    constexpr std::string_view prefix{"EPOCH_PROJECT_ASSISTANT_CONTEXT_V1\n"};
    constexpr std::string_view marker{
        "\nEND_EPOCH_PROJECT_ASSISTANT_CONTEXT_V1\n\nUSER_REQUEST\n"};
    constexpr std::string_view trailer{"\nEND_USER_REQUEST"};
    if (!message.starts_with(prefix) || !message.ends_with(trailer))
        return message;
    const auto position = message.find(marker);
    if (position == std::string_view::npos) return message;
    const auto start = position + marker.size();
    return message.substr(start, message.size() - trailer.size() - start);
}

enum class TaskIntent { simple = 0, complex = 1, coding = 2 };
[[nodiscard]] std::string_view to_string(TaskIntent intent) noexcept;
[[nodiscard]] TaskIntent classify_task(std::string_view text);
// Exclude quantitative and sensitive tasks from tiny-model handoff. They stay
// on KeyMaker even if the text doesn't contain an obvious "math" keyword.
[[nodiscard]] bool requires_foundation_reasoning(std::string_view text);
// Only substantial tasks benefit from extra advisory inference. Ordinary code
// examples and straightforward requests go directly to their final responder.
[[nodiscard]] bool warrants_specialist_handoff(std::string_view text);

// Native three-tier final-response policy. A reviewer is also allowed to be a
// text-only middle responder for bounded explanations; no tools are delegated.
// Uncertain, tool-dependent and safety-sensitive prompts stay on foundation.
enum class ResponseTier { small, middle, foundation };
[[nodiscard]] std::string_view to_string(ResponseTier tier) noexcept;
[[nodiscard]] ResponseTier route_response(std::string_view text, bool small_ready,
    bool middle_ready, bool auto_route = true);

// Trainable feature/prediction *stub*, not a pretrained neural JEPA model.
// Shadow learns only to imitate the conservative policy using privacy-preserving
// numeric examples, and never overrides it without future verified evaluation.
class PredictiveRouteShadow final {
public:
    explicit PredictiveRouteShadow(std::filesystem::path data_root);
    void observe(std::string_view text, TaskIntent policy_label);
    [[nodiscard]] std::pair<TaskIntent, double> predict(std::string_view text) const;
    [[nodiscard]] std::size_t examples_seen() const noexcept;
private:
    using Features = std::array<double, 9>;
    [[nodiscard]] static Features features(std::string_view text);
    void train(const Features& values, TaskIntent label);
    std::filesystem::path journal_;
    mutable std::mutex mutex_;
    std::array<Features, 3> weights_{};
    std::size_t examples_{};
};
} // namespace epoch_ai::controller
