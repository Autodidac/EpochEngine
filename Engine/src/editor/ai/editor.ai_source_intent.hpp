/************************************************
 * EpochEngine - deterministic engine-owned editor intent
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace epochengine::editor_ai_source_intent
{
    // The Project Assistant can mutate scene objects and author project GUI
    // widgets; it cannot modify the C++ editor theme registry or its saved
    // preference schema. Select the guarded Engine Source lane *before*
    // prompting a model with tools it cannot use. Never ask an operator for
    // internal selector IDs, code paths, or configuration field names.
    [[nodiscard]] inline bool requires_engine_source(std::string_view request)
    {
        std::string lower{};
        lower.reserve(request.size());
        for (const unsigned char c : request)
            lower += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);

        const auto has_phrase = [&lower](std::string_view phrase)
        {
            return lower.find(phrase) != std::string::npos;
        };
        const auto is_word_character = [](char c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        };
        const auto has_word = [&](std::string_view word)
        {
            for (std::size_t at = lower.find(word); at != std::string::npos;
                at = lower.find(word, at + 1u))
            {
                const std::size_t end = at + word.size();
                if ((at == 0u || !is_word_character(lower[at - 1u]))
                    && (end == lower.size() || !is_word_character(lower[end])))
                    return true;
            }
            return false;
        };
        constexpr std::array<std::string_view, 24> edit_verbs{
            "add", "create", "make", "change", "modify", "edit", "implement",
            "update", "set", "select", "fix", "repair", "replace", "rename",
            "redesign", "register", "introduce", "wire", "enable", "disable",
            "integrate", "remove", "delete", "build"};
        bool changing = false;
        for (const auto verb : edit_verbs)
            if (has_word(verb)) { changing = true; break; }
        if (!changing) return false;

        const bool theme = has_word("theme") || has_word("themes");
        const bool editor_preferences =
            has_phrase("editor settings") || has_phrase("editor preference")
            || has_phrase("global settings") || has_phrase("application settings");
        const bool source_theme_registry =
            has_phrase("theme selector") || has_phrase("theme selection")
            || has_phrase("theme preference") || has_phrase("theme registry")
            || has_phrase("theme choices") || has_phrase("theme list")
            || has_phrase("theme cycle") || has_phrase("current theme")
            || has_phrase("active theme") || has_phrase("saved theme");
        // A new *editor* theme is a source change, but a visual theme for a
        // particular game/level/project is not evidence of a source target.
        const bool project_specific =
            has_phrase("project theme") || has_phrase("game theme")
            || has_phrase("scene theme") || has_phrase("theme for my game")
            || has_phrase("theme for the game")
            || has_phrase("theme for my project")
            || has_phrase("theme for the scene");
        const bool new_editor_theme = theme && !project_specific
            && (has_phrase("new theme") || has_phrase("another theme")
                || has_phrase("unique theme") || has_phrase("theme like the others"));
        return source_theme_registry
            || (theme && (editor_preferences
                || has_phrase("configuration") || has_phrase("theme config")
                || has_phrase("editor palette")))
            || new_editor_theme;
    }
}
