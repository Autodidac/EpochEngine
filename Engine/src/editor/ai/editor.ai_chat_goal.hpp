// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once

// Explicit project-goal instructions and reviewed host-tool sequences for
// the chat composer. Ordinary requests retain their normal intent routing.

#include <cstddef>
#include <string_view>
#include <string>
#include <cstdint>
#include <initializer_list>

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
    enum class RoundTripRequest : unsigned char { none, reopen, build_and_run };

    enum class ForestAssetKind : unsigned char { none, beacon, tree };

    struct ForestAssetRequest
    {
        ForestAssetKind kind{};
        std::string_view image{}, foliage{};
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return kind != ForestAssetKind::none; }
    };

    [[nodiscard]] constexpr ForestAssetRequest forest_asset_request(std::string_view text) noexcept
    {
        text = trim(text);
        if (starts_with(text, "please ")) text.remove_prefix(7u);
        constexpr std::string_view beacon = "Generate and place a landing beacon near the selected forest:";
        if (starts_with(text, beacon)) return {ForestAssetKind::beacon, trim(text.substr(beacon.size()))};
        constexpr std::string_view tree = "Add a custom textured tree to the selected forest:";
        if (!starts_with(text, tree)) return {};
        const auto prompts = trim(text.substr(tree.size()));
        const auto split = prompts.find('\n');
        if (!starts_with(prompts, "Bark:") || split == std::string_view::npos) return {ForestAssetKind::tree};
        const auto foliage = trim(prompts.substr(split + 1u));
        if (!starts_with(foliage, "Foliage:")) return {ForestAssetKind::tree};
        return {ForestAssetKind::tree, trim(prompts.substr(5u, split - 5u)), trim(foliage.substr(8u))};
    }

    [[nodiscard]] constexpr RoundTripRequest round_trip_request(std::string_view text) noexcept
    {
        text = trim(text);
        if (starts_with(text,"please ")) text.remove_prefix(7u);
        if (text.ends_with('.')) text.remove_suffix(1u);
        for (const auto phrase : {"save, close and reopen ", "save, close, and reopen "})
            if (starts_with(text,phrase))
            {
                const auto target = text.substr(std::string_view{phrase}.size());
                for (const std::string_view project : {"the project", "this project", "project"})
                {
                    if (!starts_with(target, project)) continue;
                    const auto tail = target.substr(project.size());
                    if (tail.empty()) return RoundTripRequest::reopen;
                    if (equals(tail, ", then build and run") || equals(tail, ", then build and run it"))
                        return RoundTripRequest::build_and_run;
                }
            }
        return RoundTripRequest::none;
    }

    [[nodiscard]] inline bool requests_project_run(std::string_view request)
    {
        // Mask quoted labels and examples while preserving command boundaries.
        std::string text;
        text.reserve(request.size());
        char quote{};
        bool escaped{};
        for (const char c : request)
        {
            if (quote)
            {
                if (c == quote && !escaped) quote = 0;
                escaped = !escaped && c == '\\';
                text += ' ';
                continue;
            }
            const char previous = text.empty() ? '\0' : text.back();
            const bool apostrophe = c == '\'' && ((previous >= 'a' && previous <= 'z')
                || (previous >= '0' && previous <= '9') || previous == '_');
            if (c == '"' || c == '`' || (c == '\'' && !apostrophe))
            {
                quote = c;
                text += ' ';
            }
            else text += c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        bool requested{};
        for (auto remaining = std::string_view{text}; !remaining.empty();)
        {
            const auto end = remaining.find_first_of(".;!?\r\n");
            auto clause = trim(remaining.substr(0u, end));
            const bool question = end != std::string_view::npos && remaining[end] == '?';
            remaining = end == std::string_view::npos ? std::string_view{} : remaining.substr(end + 1u);
            if (clause.starts_with("attachment") || clause.starts_with("file content")
                || clause.starts_with("reference text") || clause.starts_with("quoted text")) break;
            if (clause != "run" && !clause.starts_with("run ") && !clause.ends_with(" run")
                && clause.find(" run ") == std::string_view::npos && clause.find(" running") == std::string_view::npos) continue;
            if (clause.find("do not ") != std::string_view::npos || clause.find("don't ") != std::string_view::npos
                || clause.find("never ") != std::string_view::npos || clause.find("without ") != std::string_view::npos)
                return false;
            if (question || clause.starts_with("example") || clause.starts_with("if ") || clause.starts_with("when ")) continue;

            for (const std::string_view lead : {", then ", ", save and ", ", save, and ", ", build and "})
                if (const auto at = clause.rfind(lead); at != std::string_view::npos)
                    clause = trim(clause.substr(at + lead.size()));
            if (clause.starts_with("please ")) clause.remove_prefix(7u);
            if (clause.starts_with("then ")) clause.remove_prefix(5u);
            for (const std::string_view lead : {"save and ", "save, and ", "build and "})
                if (clause.starts_with(lead)) { clause.remove_prefix(lead.size()); break; }
            for (const std::string_view command : {"run", "run it", "run project", "run the project", "run in the editor",
                "run it in the editor", "run the project in the editor", "run in a separate window"})
                requested = requested || clause == command;
        }
        return requested;
    }

    struct ToolSequence
    {
        unsigned stage{};
        bool enabled{}, waiting{}, stopped{}, configure_plant{}, run_after_save{}, initial_landing_forest{};
        std::string project_id{}, project_root{};
        std::uint64_t project_generation{};
        std::string objective{}, origin_project_id{}, origin_project_root{};
        std::uint64_t origin_project_generation{};
        std::string step_request{};
        unsigned request_stage{};
        bool round_trip{}, build_and_run{}, texture_update{};
        std::string reopen_manifest{};
        std::uint64_t saved_scene_revision{}, accepted_build_generation{};
        std::uint64_t import_scene_revision{}, imported_recipe_revision{}, imported_recipe_hash{};
        std::uint64_t forest_asset_target{}, forest_asset_scene_revision{};
        ForestAssetKind forest_asset_kind{ForestAssetKind::beacon};
        std::uint64_t forest_asset_recipe_revision{}, forest_asset_recipe_hash{};
        std::string forest_asset_output{}, beacon_reference_sha256{};
        [[nodiscard]] bool generates_foliage_cutout() const noexcept
        {
            return enabled && !stopped && forest_asset_target && forest_asset_kind == ForestAssetKind::tree && stage == 2u;
        }
        void bind_request(std::string_view request, std::string_view id,
            std::string_view root, std::uint64_t generation, bool update_texture = false)
        {
            objective = request;
            origin_project_id = id;
            origin_project_root = root;
            origin_project_generation = generation;
            run_after_save = requests_project_run(request);
            texture_update = update_texture;
            if (texture_update)
            {
                enabled = true;
                project_id = origin_project_id;
                project_root = origin_project_root;
                project_generation = generation;
            }
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
            constexpr std::string_view roundTripOperations[]{"project.save", "project.close", "project.open", "project.build", "project.run"};
            constexpr std::string_view beaconOperations[]{"asset.image.generate", "asset.reference.approve",
                "asset.model.generate", "asset.model.setup_landing", "project.save"};
            constexpr std::string_view treeOperations[]{"asset.image.generate", "forest.material.import",
                "asset.image.generate", "forest.material.import", "forest.species.add"};
            if (!enabled || stage >= operation_count()) return {};
            if (forest_asset_target) return forest_asset_kind == ForestAssetKind::tree ? treeOperations[stage] : beaconOperations[stage];
            if (texture_update) return stage == 0u ? "forest.material.import" : "forest.update";
            if (round_trip) return roundTripOperations[stage];
            return operations[stage + (!configure_plant && stage > 0u ? 1u : 0u)];
        }
        [[nodiscard]] unsigned operation_count() const noexcept
        {
            if (forest_asset_target) return 5u;
            if (texture_update) return 2u;
            return round_trip ? (build_and_run ? 5u : 3u) : 3u + unsigned(configure_plant) + unsigned(run_after_save);
        }
        [[nodiscard]] std::string next_request(std::string_view original) const
        {
            const auto tool = operation();
            if (tool.empty()) return {};
            if (forest_asset_target)
            {
                if (tool == "asset.image.generate")
                {
                    const auto prompts = forest_asset_request(objective);
                    if (forest_asset_kind == ForestAssetKind::tree && stage == 2u)
                        return "Generate an image with " + std::string{prompts.foliage}
                            + ". Isolate the foliage and its downward stem against a flat vivid pure magenta (#FF00FF) background. "
                              "Leave a clear magenta margin on all four edges. No magenta on the plant, no shadows, text or border.";
                    return "Generate an image with " + std::string{prompts.image};
                }
                if (tool == "forest.material.import")
                    return stage == 1u ? "Import and apply the current PNG as bark." : "Import and apply the current PNG as foliage.";
                if (tool == "forest.species.add") return "Add the current Plant Lab tree to the selected forest.";
                if (tool == "asset.reference.approve") return "Approve the current PNG as the 3D reference.";
                if (tool == "asset.model.generate") return "Generate 3D from the approved reference.";
                if (tool == "asset.model.setup_landing") return "Set up landing near the selected forest with the current generated 3D model.";
            }
            if (!step_request.empty() && request_stage == stage) return step_request;
            if (tool == "project.create") return std::string{original};
            if (tool == "forest.material.import") return std::string{original};
            if (tool == "forest.update") return "Update Painted Trees.";
            if (tool == "project.save") return "save project";
            if (tool == "project.close") return "close project";
            if (tool == "project.open") return "reopen project=" + origin_project_id;
            if (tool == "project.build") return "build project";
            if (tool == "project.run") return "run project";
            if (tool == "forest.paint" && initial_landing_forest)
                return "Paint a forest near the landing area using the current Plant Lab recipe. Keep the separately requested custom tree out of this step. Ask for any missing radius or density.";
            const std::string prefix = tool == "forest.configure"
                ? "Configure the Plant Lab recipe for the original project goal. Preserve every requested design choice; ask about missing or unsupported species/foliage instead of substituting."
                : "Paint forest for the original project goal; use its approved plant recipe and specified settings or explicitly requested defaults, and ask for missing radius or density.";
            return prefix + "\nOriginal project goal: " + std::string{original};
        }
        [[nodiscard]] bool matches(std::string_view id, std::string_view root, std::uint64_t generation) const noexcept
        {
            return (!round_trip && !texture_update && stage == 0u && project_id.empty())
                || (id == project_id && root == project_root && generation == project_generation);
        }
        [[nodiscard]] bool matches_imported_recipe(std::uint64_t scene_revision,
            std::uint64_t recipe_revision, std::uint64_t recipe_hash) const noexcept
        {
            return texture_update && stage == 1u && scene_revision == import_scene_revision
                && recipe_revision == imported_recipe_revision && recipe_hash == imported_recipe_hash;
        }
        // Explicit resume keeps the next uncompleted stage. A created shell
        // whose later generation failed must not create another project.
        [[nodiscard]] bool resume(std::string_view request, std::string_view id,
            std::string_view root, std::uint64_t generation) noexcept
        {
            if (!enabled || waiting || operation().empty() || objective.empty() || request != objective)
                return false;
            const bool current = stage == 0u && !round_trip
                ? id == origin_project_id && root == origin_project_root && generation == origin_project_generation
                : id == project_id && root == project_root && generation == project_generation;
            if (!current) return false;
            stopped = false;
            return true;
        }
        [[nodiscard]] std::string local_review_plan() const
        {
            if (!enabled || stopped || waiting || texture_update) return {};
            if (round_trip)
            {
                if (operation() == "project.save")
                    return "EPOCH_TOOL_PLAN_V1\nTITLE Save the current project\n"
                        "SUMMARY Save the current scene and GUI before closing.\nCALL project.save\nEND\n";
                if (operation() == "project.close")
                    return "EPOCH_TOOL_PLAN_V1\nTITLE Close the saved project\n"
                        "SUMMARY Close the current saved project.\nCALL project.close\nEND\n";
                if (operation() == "project.open")
                    return "EPOCH_TOOL_PLAN_V1\nTITLE Reopen the saved project\n"
                        "SUMMARY Restore the verified saved scene in the same project.\nCALL project.open project="
                        + origin_project_id + "\nEND\n";
                if (operation() == "project.build")
                    return "EPOCH_TOOL_PLAN_V1\nTITLE Build the reopened project\n"
                        "SUMMARY Build the verified saved project. Runtime launch requires separate approval.\nCALL project.build\nEND\n";
                if (operation() == "project.run" && accepted_build_generation != 0u)
                    return "EPOCH_TOOL_PLAN_V1\nTITLE Run the accepted build\n"
                        "SUMMARY Launch the verified executable from the approved build.\nCALL project.run\nEND\n";
                return {};
            }
            if (operation() == "project.save")
                return forest_asset_target
                    ? "EPOCH_TOOL_PLAN_V1\nTITLE Save the landing area\nSUMMARY Save the reviewed beacon, PlayerStart and reach objective.\nCALL project.save\nEND\n"
                    : "EPOCH_TOOL_PLAN_V1\nTITLE Save the created project\n"
                    "SUMMARY Save the verified planet and painted forest in the current project.\n"
                    "CALL project.save\nEND\n";
            if (operation() == "project.run")
                return "EPOCH_TOOL_PLAN_V1\nTITLE Run the saved project\n"
                    "SUMMARY Build if needed and run the saved planet and forest through the normal project controls.\n"
                    "CALL project.run\nEND\n";
            return {};
        }
        bool receipt(std::string_view operation_name, bool success, std::string_view id,
            std::string_view root, std::uint64_t generation, std::uint64_t scene_revision = 0u,
            std::uint64_t build_generation = 0u, std::uint64_t recipe_revision = 0u,
            std::uint64_t recipe_hash = 0u)
        {
            if (!enabled || stopped || !waiting || operation_name != operation()) return false;
            waiting = false;
            if (forest_asset_target)
            {
                const bool tree = forest_asset_kind == ForestAssetKind::tree;
                const bool imported = operation_name == "forest.material.import";
                const bool placed = operation_name == "asset.model.setup_landing" || operation_name == "forest.species.add";
                if (!success || !matches(id, root, generation) || scene_revision == 0u
                    || (placed ? scene_revision == forest_asset_scene_revision : scene_revision != forest_asset_scene_revision)
                    || (operation_name != "project.save" && operation_name != "forest.species.add" && forest_asset_output.empty())
                    || (!tree && operation_name != "project.save" && beacon_reference_sha256.empty())
                    || (tree && (recipe_revision == 0u || (!imported && (recipe_revision != forest_asset_recipe_revision
                        || recipe_hash != forest_asset_recipe_hash)))))
                { stopped = true; return false; }
                if (placed) forest_asset_scene_revision = scene_revision;
                if (imported)
                {
                    forest_asset_recipe_revision = recipe_revision;
                    forest_asset_recipe_hash = recipe_hash;
                    forest_asset_output.clear();
                }
                ++stage;
                return stage < operation_count();
            }
            if (texture_update)
            {
                if (!success || id.empty() || root.empty() || !matches(id, root, generation)
                    || (stage == 0u && (scene_revision == 0u || recipe_revision == 0u))
                    || (stage == 1u && (recipe_revision != imported_recipe_revision
                        || recipe_hash != imported_recipe_hash)))
                { stopped = true; return false; }
                if (stage == 0u)
                {
                    import_scene_revision = scene_revision;
                    imported_recipe_revision = recipe_revision;
                    imported_recipe_hash = recipe_hash;
                }
                ++stage;
                return stage < operation_count();
            }
            if (round_trip)
            {
                bool accepted = false;
                if (operation_name == "project.save")
                    accepted = matches(id, root, generation) && scene_revision != 0u;
                else if (operation_name == "project.close")
                    accepted = generation != project_generation && id.empty() && root.empty() && saved_scene_revision != 0u;
                else if (operation_name == "project.open")
                    accepted = generation != project_generation && id == origin_project_id && root == origin_project_root
                        && scene_revision == saved_scene_revision;
                else if (operation_name == "project.build")
                    accepted = matches(id, root, generation) && scene_revision == saved_scene_revision && build_generation != 0u;
                else if (operation_name == "project.run")
                    accepted = matches(id, root, generation) && scene_revision == saved_scene_revision
                        && accepted_build_generation != 0u && build_generation == accepted_build_generation;
                accepted = success && accepted;
                if (!accepted) { stopped = true; return false; }
                if (operation_name == "project.save") saved_scene_revision = scene_revision;
                if (operation_name == "project.build") accepted_build_generation = build_generation;
                project_id = id; project_root = root; project_generation = generation;
                ++stage;
                return stage < operation_count();
            }
            if (!success || id.empty() || root.empty() || !matches(id, root, generation))
            { stopped = true; return false; }
            if (stage == 0u)
            { project_id = id; project_root = root; project_generation = generation; }
            ++stage;
            return stage < operation_count();
        }
    };

}
