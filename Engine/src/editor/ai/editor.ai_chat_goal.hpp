// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

// Explicit, model-independent project-goal instructions for the chat composer.
// This only recognizes instructions about the persistent goal lifecycle;
// ordinary project requests must retain normal intent routing.

#include <cstddef>
#include <string_view>

namespace epochengine::editor::ai_chat_goal
{
    enum class Action : unsigned char
    {
        none,
        set,
        pause,
        resume,
        erase,
        status
    };

    struct Command
    {
        Action action{Action::none};
        std::string_view objective{};
    };

    [[nodiscard]] constexpr std::string_view trim(std::string_view text) noexcept
    {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'
            || text.front() == '\r' || text.front() == '\n'))
            text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t'
            || text.back() == '\r' || text.back() == '\n'))
            text.remove_suffix(1);
        return text;
    }

    [[nodiscard]] constexpr bool equals(std::string_view lhs,
        std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t index = 0; index != lhs.size(); ++index)
        {
            const auto ch = [](unsigned char character) constexpr -> unsigned char
            {
                return character >= 'A' && character <= 'Z'
                    ? static_cast<unsigned char>(character - 'A' + 'a')
                    : character;
            };
            if (ch(static_cast<unsigned char>(lhs[index]))
                != ch(static_cast<unsigned char>(rhs[index])))
                return false;
        }
        return true;
    }

    [[nodiscard]] constexpr bool starts_with(std::string_view text,
        std::string_view prefix) noexcept
    {
        return text.size() >= prefix.size()
            && equals(text.substr(0, prefix.size()), prefix);
    }

    // A colon-delimited instruction is unambiguous. Natural-language
    // alternatives must still explicitly refer to setting a *goal*.
    [[nodiscard]] constexpr Command parse(std::string_view input) noexcept
    {
        const auto text = trim(input);
        if (text.empty()) return {};

        constexpr std::string_view deletePhrases[] = {
            "delete goal", "delete my goal", "delete project goal",
            "remove goal", "remove my goal", "remove project goal",
            "clear goal", "clear project goal", "end goal", "stop goal",
            "stop the goal", "cancel project goal", "/goal stop", "/goal done"};
        for (const auto phrase : deletePhrases)
            if (equals(text, phrase)) return {Action::erase, {}};

        constexpr std::string_view pausePhrases[] = {
            "pause goal", "pause my goal", "pause the goal",
            "pause project goal", "pause the project goal"};
        for (const auto phrase : pausePhrases)
            if (equals(text, phrase)) return {Action::pause, {}};

        constexpr std::string_view resumePhrases[] = {
            "resume goal", "resume my goal", "resume the goal",
            "resume project goal", "continue goal", "continue my goal",
            "next goal milestone", "next project goal milestone"};
        for (const auto phrase : resumePhrases)
            if (equals(text, phrase)) return {Action::resume, {}};

        constexpr std::string_view statusPhrases[] = {
            "goal status", "project goal status", "show goal", "show project goal",
            "what is my goal?", "what is my goal", "what's my goal?",
            "what's my goal", "what is the project goal?", "current goal"};
        for (const auto phrase : statusPhrases)
            if (equals(text, phrase)) return {Action::status, {}};

        constexpr std::string_view setPrefixes[] = {
            "project goal:", "goal:",
            "set goal to ", "set a goal to ", "set my goal to ",
            "set the goal to ", "set project goal to ",
            "set the project goal to ", "set my project goal to ",
            "set a project goal to ", "create a project goal to ",
            "create a goal to ", "start a project goal to ",
            "change goal to ", "change my goal to ",
            "change project goal to ", "change the goal to ",
            "update goal to ", "update my goal to ",
            "edit goal to ", "edit project goal to ",
            "my project goal is ", "my goal is to "};
        for (const auto prefix : setPrefixes)
        {
            if (starts_with(text, prefix))
                return {Action::set, trim(text.substr(prefix.size()))};
        }
        // Legacy power-user command remains usable without any special UI.
        if (equals(text, "/goal")) return {Action::status, {}};
        if (starts_with(text, "/goal "))
            return {Action::set, trim(text.substr(6))};
        return {};
    }
}
