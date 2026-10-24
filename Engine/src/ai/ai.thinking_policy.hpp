// Epoch Engine AI - portable, request-time model thinking policy.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
#include <string>
#include <string_view>

namespace epochengine::ai::thinking {
enum class Mode : unsigned char { automatic, off, low, high };

// Normalize display names, HF IDs and GGUF basenames without imposing a
// specific punctuation convention on independently managed local endpoints.
[[nodiscard]] inline std::string folded_model_id(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value) {
        if (c >= 'A' && c <= 'Z') result += static_cast<char>(c + ('a' - 'A'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            result += static_cast<char>(c);
    }
    return result;
}
[[nodiscard]] inline bool is_qwen38(std::string_view id) {
    return folded_model_id(id).find("qwen38") != std::string::npos;
}
[[nodiscard]] inline bool is_flash_next(std::string_view id) {
    const auto folded = folded_model_id(id);
    return folded.find("qwen38") != std::string::npos &&
        (folded.find("flashnext") != std::string::npos ||
         folded.find("nextflash") != std::string::npos);
}
[[nodiscard]] inline Mode model_default(std::string_view id) {
    return is_qwen38(id) && !is_flash_next(id) ? Mode::off : Mode::low;
}
[[nodiscard]] inline Mode effective(Mode overrideMode, std::string_view id) {
    return overrideMode == Mode::automatic ? model_default(id) : overrideMode;
}
[[nodiscard]] inline constexpr std::string_view name(Mode mode) {
    switch (mode) {
        case Mode::off: return "Off";
        case Mode::low: return "Low";
        case Mode::high: return "High";
        default: return "Auto";
    }
}
// Qwen's chat template exposes an on/off flag, not a bounded low-reasoning
// budget. Map Low to direct-output mode rather than silently enabling unlimited
// thinking. High is the explicit opt-in for the reasoning channel. This is
// deliberately independent of the historical Flash Next default display.
[[nodiscard]] inline constexpr bool template_thinking_enabled(Mode mode) noexcept {
    return mode == Mode::high;
}
// Every supported explicit mode must be reachable from the chat control.
// In particular, Qwen Low is direct output; High enables its reasoning channel.
[[nodiscard]] inline constexpr Mode next_mode(Mode mode) noexcept {
    switch (mode) {
        case Mode::off: return Mode::low;
        case Mode::low: return Mode::high;
        case Mode::high: return Mode::off;
        default: return Mode::low;
    }
}
// The enable_thinking extension is Qwen-template-specific; never send it to
// arbitrary OpenAI-compatible providers where it may be unsupported.
[[nodiscard]] inline bool uses_qwen_template_flag(std::string_view model) {
    // The installed 9B identifies as Qwen3.5, not Qwen3.8. Its actual GGUF
    // template also consumes enable_thinking; omitting it leaves reasoning on
    // even when the operator selects Off/Low.
    return is_qwen38(model)
        || folded_model_id(model).find("qwen35") != std::string::npos;
}
// Request text uses the runtime vocabulary (deep/fast/auto), while the UI
// stores High/Low/Off. Keep the Qwen template decision identical for the
// initial request, resident-model fallback and direct host completions.
// Unknown modes use the family default, never an unbounded implicit mode.
[[nodiscard]] inline bool qwen_thinking_enabled(
    std::string_view requestedMode, std::string_view model) {
    // This request boundary accepts std::string_view because the native transport
    // stores mode/model IDs as std::string. Epoch also defines its own distinct
    // epochengine::string_view; callers with Epoch views cross this boundary
    // explicitly via core.stl_types.hpp to_std(). Member comparison avoids
    // ambiguous operator== resolution with the engine string overload set.
    if (requestedMode.compare("deep") == 0 || requestedMode.compare("high") == 0)
        return true;
    if (requestedMode.compare("off") == 0 || requestedMode.compare("low") == 0 ||
        requestedMode.compare("fast") == 0)
        return false;
    return template_thinking_enabled(model_default(model));
}
} // namespace epochengine::ai::thinking
