// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

// Explicit, model-independent project-goal instructions for the chat composer.
// This only recognizes instructions about the persistent goal lifecycle;
// ordinary project requests must retain normal intent routing.

#include <cstddef>
#include <string_view>
#include <string>
#include <cstdint>

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
    // A bounded composition of existing GUI actions. This state grants no
    // execution authority: every stage still needs its own exact-call approval.
    struct ToolSequence
    {
        unsigned stage{};
        bool enabled{}, waiting{}, stopped{}, configure_plant{};
        std::string project_id{}, project_root{};
        std::uint64_t project_generation{};
        std::string objective{}, origin_project_id{}, origin_project_root{};
        std::uint64_t origin_project_generation{};
        std::string step_request{};
        unsigned request_stage{};
        void bind_request(std::string_view request, std::string_view id,
            std::string_view root, std::uint64_t generation)
        {
            objective = request;
            origin_project_id = id;
            origin_project_root = root;
            origin_project_generation = generation;
        }
        bool remember_request(std::string_view operation_name, std::string_view request)
        {
            if (!enabled || waiting || stopped || request.empty() || operation_name != operation())
                return false;
            step_request = request;
            request_stage = stage;
            return true;
        }
        [[nodiscard]] std::string_view operation() const noexcept
        {
            constexpr std::string_view operations[]{"project.create", "forest.configure", "forest.paint", "project.save", "project.run"};
            if (!enabled || stage >= operation_count()) return {};
            return operations[stage + (!configure_plant && stage > 0u ? 1u : 0u)];
        }
        [[nodiscard]] unsigned operation_count() const noexcept { return configure_plant ? 5u : 4u; }
        [[nodiscard]] std::string next_request(std::string_view original) const
        {
            const auto tool = operation();
            if (tool.empty()) return {};
            if (!step_request.empty() && request_stage == stage) return step_request;
            if (tool == "project.create") return std::string{original};
            if (tool == "project.save") return "save project";
            if (tool == "project.run") return "run project";
            const std::string prefix = tool == "forest.configure"
                ? "Configure the Plant Lab recipe for the original project goal. Preserve every requested design choice; ask about missing or unsupported species/foliage instead of substituting."
                : "Paint forest for the original project goal; use its approved plant recipe and specified settings or explicitly requested defaults, and ask for missing radius or density.";
            return prefix + "\nOriginal project goal: " + std::string{original};
        }
        [[nodiscard]] bool matches(std::string_view id, std::string_view root, std::uint64_t generation) const noexcept
        {
            return (stage == 0u && project_id.empty()) || (id == project_id && root == project_root && generation == project_generation);
        }
        // Explicit resume keeps the next uncompleted stage. A created shell
        // whose later generation failed must not create another project.
        [[nodiscard]] bool resume(std::string_view request, std::string_view id,
            std::string_view root, std::uint64_t generation) noexcept
        {
            if (!enabled || waiting || operation().empty() || objective.empty() || request != objective)
                return false;
            const bool current = stage == 0u
                ? id == origin_project_id && root == origin_project_root && generation == origin_project_generation
                : id == project_id && root == project_root && generation == project_generation;
            if (!current) return false;
            stopped = false;
            return true;
        }
        [[nodiscard]] std::string_view local_review_plan() const noexcept
        {
            if (!enabled || stopped || waiting) return {};
            if (operation() == "project.save")
                return "EPOCH_TOOL_PLAN_V1\nTITLE Save the created project\n"
                    "SUMMARY Save the verified planet and painted forest in the current project.\n"
                    "CALL project.save\nEND\n";
            if (operation() == "project.run")
                return "EPOCH_TOOL_PLAN_V1\nTITLE Run the saved project\n"
                    "SUMMARY Build if needed and run the saved planet and forest through the normal project controls.\n"
                    "CALL project.run\nEND\n";
            return {};
        }
        bool receipt(std::string_view operation_name, bool success, std::string_view id,
            std::string_view root, std::uint64_t generation)
        {
            if (!enabled || stopped || !waiting || operation_name != operation()) return false;
            waiting = false;
            if (!success || id.empty() || root.empty() || !matches(id, root, generation))
            { stopped = true; return false; }
            if (stage == 0u)
            { project_id = id; project_root = root; project_generation = generation; }
            ++stage;
            return stage < operation_count();
        }
    };

}
