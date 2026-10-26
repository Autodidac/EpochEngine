// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::optional<EditorEntity> make_editor_entity(
            EditorState& state, std::string_view archetype, std::size_t ordinalOffset = 0u)
        {
            EditorEntity entity{};
            if (archetype == "cube")
            {
                entity.name = make_entity_name(state, "StaticMesh", ordinalOffset);
                entity.type = "StaticMesh";
                entity.category = "Gameplay";
                entity.position = next_entity_position(
                    state,
                    primary_ground_top_y(state), ordinalOffset);
                if (!align_editor_entity_bottom_to_surface(
                        entity,
                        primary_ground_top_y(state)))
                {
                    state.projectStatus =
                        "Static mesh creation rejected invalid surface bounds.";
                    return std::nullopt;
                }
            }
            else if (archetype == "ground")
            {
                entity.name = make_entity_name(state, "GroundPlatform", ordinalOffset);
                entity.type = "Ground";
                entity.category = "World";
                entity.position = { 0.0f, -0.25f, 0.0f };
                entity.scale = { 12.0f, 0.5f, 12.0f };
            }
            else if (archetype == "light")
            {
                entity.name = make_entity_name(state, "PointLight", ordinalOffset);
                entity.type = "Light";
                entity.category = "Lighting";
                entity.position = next_entity_position(
                    state,
                    primary_ground_top_y(state) + 2.4f, ordinalOffset);
            }
            else if (archetype == "spawn")
            {
                entity.name = make_entity_name(state, "PlayerStart", ordinalOffset);
                entity.type = "Spawn";
                entity.category = "Gameplay";
                entity.position = next_entity_position(
                    state,
                    primary_ground_top_y(state) + 0.05f, ordinalOffset);
            }
            else if (archetype == "camera")
            {
                entity.name = make_entity_name(state, "PreviewCamera", ordinalOffset);
                entity.type = "Camera";
                entity.category = "Gameplay";
                entity.position = next_entity_position(
                    state,
                    primary_ground_top_y(state) + 1.8f, ordinalOffset);
                entity.rotation = { -18.0f, 0.0f, 0.0f };
            }
            else
            {
                const std::size_t guiWidgetCount = ordinalOffset + static_cast<std::size_t>(
                    std::count_if(
                        state.entities.begin(),
                        state.entities.end(),
                        [](const EditorEntity& candidate)
                        {
                            return candidate.category == "UI"
                                && candidate.type.starts_with("Gui");
                        }));
                const float stagger =
                    static_cast<float>(guiWidgetCount % 6u) * 0.16f;

                entity.category = "UI";
                entity.position = {
                    -0.40f + stagger,
                    0.32f - stagger,
                    0.20f + static_cast<float>(guiWidgetCount) * 0.001f
                };

                if (archetype == "gui_panel")
                {
                    entity.name = make_entity_name(state, "Panel", ordinalOffset);
                    entity.type = "GuiPanel";
                    entity.scale = { 4.8f, 2.8f, 1.0f };
                }
                else if (archetype == "gui_button")
                {
                    entity.name = make_entity_name(state, "Button", ordinalOffset);
                    entity.type = "GuiButton";
                    entity.position[1] -= 0.72f;
                    entity.scale = { 2.2f, 0.64f, 1.0f };
                }
                else if (archetype == "gui_text")
                {
                    entity.name = make_entity_name(state, "Text", ordinalOffset);
                    entity.type = "GuiText";
                    entity.position[1] += 0.78f;
                    entity.scale = { 3.4f, 0.42f, 1.0f };
                }
                else if (archetype == "gui_image")
                {
                    entity.name = make_entity_name(state, "Image", ordinalOffset);
                    entity.type = "GuiImage";
                    entity.position[0] -= 1.15f;
                    entity.scale = { 1.5f, 1.5f, 1.0f };
                }
                else if (archetype == "gui_image_button")
                {
                    entity.name = make_entity_name(state, "ImageButton", ordinalOffset);
                    entity.type = "GuiImageButton";
                    entity.position[0] -= 1.15f;
                    entity.position[1] -= 0.82f;
                    entity.scale = { 1.6f, 0.72f, 1.0f };
                }
                else if (archetype == "gui_tabs")
                {
                    entity.name = make_entity_name(state, "TabSet", ordinalOffset);
                    entity.type = "GuiTabSet";
                    entity.position[1] += 1.16f;
                    entity.scale = { 4.0f, 0.56f, 1.0f };
                }
                else if (archetype == "gui_input")
                {
                    entity.name = make_entity_name(state, "TextInput", ordinalOffset);
                    entity.type = "GuiTextInput";
                    entity.scale = { 3.2f, 0.58f, 1.0f };
                }
                else if (archetype == "gui_slider")
                {
                    entity.name = make_entity_name(state, "Slider", ordinalOffset);
                    entity.type = "GuiSlider";
                    entity.position[1] -= 1.08f;
                    entity.scale = { 3.0f, 0.32f, 1.0f };
                }
                else if (archetype == "gui_scroll")
                {
                    entity.name = make_entity_name(state, "ScrollArea", ordinalOffset);
                    entity.type = "GuiScrollArea";
                    entity.position[0] += 1.15f;
                    entity.scale = { 3.4f, 2.6f, 1.0f };
                }
                else
                {
                    return std::nullopt;
                }
            }

            return entity;
        }

        bool add_entity(EditorState& state, std::string_view archetype)
        {
            auto entity = make_editor_entity(state, archetype);
            if (!entity) return false;
            const std::string addedName = entity->name;
            const std::string addedType = entity->type;
            if (!create_editor_scene_entity(state, std::move(*entity), "Create " + addedName))
            {
                push_editor_log(state, "[entity] Could not create " + addedName + ".");
                return false;
            }
            push_editor_log(
                state,
                std::string("[entity] Added ")
                + addedName
                + " ["
                + addedType
                + "].");
            return true;
        }

        bool add_cube_batch(EditorState& state, std::size_t count)
        {
            if (count == 0u)
                return false;
            count = (std::min)(count, std::size_t{64u});
            if (!state.sceneDocument.initialized()
                && !rebuild_editor_scene_document(
                    state, "Initialize scene before batch object creation"))
            {
                return false;
            }

            const std::size_t existingStaticMeshes = static_cast<std::size_t>(
                std::count_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [](const EditorEntity& entity)
                    {
                        return entity.name.starts_with("StaticMesh");
                    }));
            const std::size_t worldEntityCount = static_cast<std::size_t>(
                std::count_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [](const EditorEntity& entity)
                    {
                        return entity.category != "UI"
                            && !entity.type.starts_with("Gui");
                    }));

            std::vector<authoring::scene::SceneOperation> operations{};
            std::vector<scene::SceneObjectId> createdIds{};
            std::unordered_set<scene::SceneObjectId> reservedIds{};
            operations.reserve(count);
            createdIds.reserve(count);
            reservedIds.reserve(count);

            const float groundTop = primary_ground_top_y(state);
            for (std::size_t i = 0u; i < count; ++i)
            {
                EditorEntity entity{};
                entity.name = epochengine::format_text(
                    "StaticMesh_{:02}", existingStaticMeshes + i + 1u);
                entity.type = "StaticMesh";
                entity.category = "Gameplay";
                const std::size_t ordinal = worldEntityCount + i;
                entity.position = {
                    -2.7f + static_cast<float>(ordinal % 5u) * 1.35f,
                    groundTop,
                    1.6f - static_cast<float>((ordinal / 5u) % 4u) * 1.15f
                };
                if (!align_editor_entity_bottom_to_surface(entity, groundTop))
                    return false;

                entity.sceneObjectId = allocate_editor_scene_object_id(
                    state, entity.name);
                std::uint64_t salt = static_cast<std::uint64_t>(
                    state.sceneDocument.objects().size() + i + 1u);
                while (entity.sceneObjectId == scene::kInvalidSceneObjectId
                    || reservedIds.contains(entity.sceneObjectId))
                {
                    entity.sceneObjectId ^= 0x9e3779b97f4a7c15ull
                        + salt
                        + (entity.sceneObjectId << 6u)
                        + (entity.sceneObjectId >> 2u);
                    ++salt;
                }
                reservedIds.insert(entity.sceneObjectId);

                const auto converted =
                    authoring::scene::descriptor_from_snapshot_object(
                        editor_entity_snapshot(entity),
                        static_cast<std::uint32_t>(
                            state.sceneDocument.objects().size() + i),
                        state.sceneDocument.limits());
                if (!converted)
                    return false;
                operations.emplace_back(
                    authoring::scene::ObjectCreatedOperation{
                        .object = *converted });
                createdIds.push_back(entity.sceneObjectId);
            }

            const scene::SceneObjectId primary = createdIds.back();
            if (!apply_editor_scene_transaction(
                    state,
                    operations,
                    epochengine::format_text("Create {} cubes", count),
                    primary))
            {
                return false;
            }

            state.selectedEntityIds = createdIds;
            state.selectedEntityId = primary;
            synchronize_editor_selection(state);
            push_editor_log(
                state,
                epochengine::format_text(
                    "[entity] Created {} cubes in one transaction and selected the group.",
                    count));
            return true;
        }

        [[nodiscard]] std::optional<std::string_view>
        ai_authoring_scene_archetype(const EditorEntity& entity) noexcept
        {
            if (entity.type == "StaticMesh")
                return "cube";
            if (entity.type == "Ground")
                return "ground";
            if (entity.type == "Light")
                return "light";
            if (entity.type == "Spawn")
                return "spawn";
            if (entity.type == "Camera")
                return "camera";
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::string_view>
        ai_authoring_gui_widget(const EditorEntity& entity) noexcept
        {
            if (entity.type == "GuiPanel")
                return "panel";
            if (entity.type == "GuiButton")
                return "button";
            if (entity.type == "GuiText")
                return "text";
            if (entity.type == "GuiImage")
                return "image";
            if (entity.type == "GuiImageButton")
                return "image_button";
            if (entity.type == "GuiTabSet")
                return "tabs";
            if (entity.type == "GuiTextInput")
                return "input";
            if (entity.type == "GuiSlider")
                return "slider";
            if (entity.type == "GuiScrollArea")
                return "scroll";
            return std::nullopt;
        }

        [[nodiscard]] const epochengine::ai::McpArgument*
        ai_authoring_argument(
            const epochengine::ai::McpToolCall& call,
            std::string_view name) noexcept
        {
            const auto found = std::find_if(
                call.arguments.begin(),
                call.arguments.end(),
                [name](const epochengine::ai::McpArgument& argument)
                {
                    return argument.name == name;
                });
            return found == call.arguments.end() ? nullptr : &*found;
        }

        [[nodiscard]] unsigned ai_authoring_count(
            const epochengine::ai::McpToolCall& call) noexcept
        {
            const auto* count = ai_authoring_argument(call, "count");
            if (!count)
                return 1u;
            unsigned value{};
            const auto converted = std::from_chars(count->value.data(),
                count->value.data() + count->value.size(), value);
            if (converted.ec != std::errc{}
                || converted.ptr != count->value.data() + count->value.size()) return 0u;
            return value;
        }


        [[nodiscard]] std::optional<std::string>
            recover_bare_ai_authoring_call(std::string_view response)
        {
            constexpr std::size_t maximumResponseBytes = 32u * 1024u;
            if (response.empty() || response.size() > maximumResponseBytes)
                return std::nullopt;

            const auto trim = [](std::string_view value) noexcept
            {
                while (!value.empty()
                    && (value.front() == ' ' || value.front() == '\t'
                        || value.front() == '\r' || value.front() == '\n'))
                {
                    value.remove_prefix(1u);
                }
                while (!value.empty()
                    && (value.back() == ' ' || value.back() == '\t'
                        || value.back() == '\r' || value.back() == '\n'))
                {
                    value.remove_suffix(1u);
                }
                return value;
            };

            // Local models occasionally wrap the one requested CALL in a short
            // explanation or Markdown. Recover only a unique CALL line. The
            // strict parser below still validates its tool/arguments, and the
            // existing operator-approval gate remains authoritative.
            std::string_view recoveredCall{};
            std::size_t callCount{};
            std::size_t cursor{};
            while (cursor <= response.size())
            {
                const std::size_t newline = response.find('\n', cursor);
                std::string_view line = response.substr(
                    cursor,
                    newline == std::string_view::npos
                        ? response.size() - cursor
                        : newline - cursor);
                cursor = newline == std::string_view::npos
                    ? response.size() + 1u : newline + 1u;
                line = trim(line);
                if (line.starts_with("CALL "))
                {
                    ++callCount;
                    recoveredCall = line;
                    if (callCount > 1u)
                        return std::nullopt;
                }
            }
            if (callCount != 1u)
                return std::nullopt;

            std::string envelope =
                "EPOCH_AUTHORING_PLAN_V1\n"
                "TITLE Bounded AI authoring change\n"
                "SUMMARY Validate and stage the single bounded call returned by the selected model.\n";
            envelope.append(recoveredCall.data(), recoveredCall.size());
            envelope += "\nEND";
            return envelope;
        }

        [[nodiscard]] std::optional<std::string>
            parse_ai_blocking_question(
                std::string_view response,
                std::string_view expectedHeader)
        {
            constexpr std::size_t maximumQuestionBytes = 2u * 1024u;
            if (response.empty() || response.size() > maximumQuestionBytes
                || ai::authoring_plan_detail::has_forbidden_controls(response))
                return std::nullopt;

            const auto trim = [](std::string_view value) noexcept
            {
                while (!value.empty()
                    && (value.front() == ' ' || value.front() == '\t'
                        || value.front() == '\r' || value.front() == '\n'))
                {
                    value.remove_prefix(1u);
                }
                while (!value.empty()
                    && (value.back() == ' ' || value.back() == '\t'
                        || value.back() == '\r' || value.back() == '\n'))
                {
                    value.remove_suffix(1u);
                }
                return value;
            };

            response = trim(response);
            const std::size_t firstNewline = response.find('\n');
            if (firstNewline == std::string_view::npos
                || trim(response.substr(0u, firstNewline)).compare(expectedHeader) != 0)
            {
                return std::nullopt;
            }
            response.remove_prefix(firstNewline + 1u);
            const std::size_t secondNewline = response.find('\n');
            if (secondNewline == std::string_view::npos)
                return std::nullopt;
            const std::string_view questionLine = trim(
                response.substr(0u, secondNewline));
            constexpr std::string_view prefix{"QUESTION "};
            if (!questionLine.starts_with(prefix))
                return std::nullopt;
            const std::string_view question = trim(
                questionLine.substr(prefix.size()));
            if (question.empty() || question.size() > 768u)
                return std::nullopt;
            response.remove_prefix(secondNewline + 1u);
            if (trim(response).compare("END") != 0)
                return std::nullopt;
            return std::string{question};
        }

        [[nodiscard]] std::string build_project_assistant_chat_prompt(
            const EditorState& editor,
            std::string_view request)
        {
            std::string prompt{};
            prompt.reserve(4096u + request.size());
            prompt +=
                "EPOCH_PROJECT_ASSISTANT_CONTEXT_V1\n"
                "This is ordinary Project Assistant conversation, not an engine-source "
                "iteration and not an authoring-plan execution request. Ground the answer "
                "in the bounded active-project evidence below. Never claim that you changed "
                "the project, scene, GUI, files, build, or runtime unless the host later "
                "returns explicit execution evidence.\n";
            prompt += "Project: ";
            prompt += editor.projectName.empty()
                ? std::string{"No project selected"}
                : editor.projectName;
            prompt += "\nProject ID: ";
            prompt += editor.projectId.empty()
                ? std::string{"none"}
                : editor.projectId;
            prompt += "\nProject status: ";
            prompt += editor.projectStatus.empty()
                ? std::string{"(none)"}
                : editor.projectStatus;
            prompt += "\nBuild status: ";
            prompt += editor.projectBuildStatus.empty()
                ? std::string{"(none)"}
                : editor.projectBuildStatus;
            prompt += "\nScene revision: ";
            prompt += std::to_string(editor.sceneDocumentRevision);

            std::size_t worldObjects{};
            std::size_t guiObjects{};
            for (const auto& entity : editor.entities)
            {
                if (is_screen_space_gui_entity(entity))
                    ++guiObjects;
                else
                    ++worldObjects;
            }
            prompt += "\nWorld objects: ";
            prompt += std::to_string(worldObjects);
            prompt += "\nGUI objects: ";
            prompt += std::to_string(guiObjects);
            prompt += "\nGUI document: ";
            prompt += editor.guiDocument && editor.guiDocument->valid()
                ? "loaded" : "not loaded";
            prompt += "\nActive script: ";
            prompt += editor.activeScript.empty()
                ? std::string{"(none)"}
                : editor.activeScript;

            if (editor.selectedEntityId != scene::kInvalidSceneObjectId
                && editor.selectedEntity < editor.entities.size())
            {
                const auto& selected = editor.entities[editor.selectedEntity];
                prompt += "\nSelected object: ";
                prompt += selected.name;
                prompt += " [";
                prompt += selected.type;
                prompt += "] id=";
                prompt += std::to_string(selected.sceneObjectId);
            }

            prompt +=
                "\nEpoch automatically routes explicit change requests to guarded project authoring, project tools, or Assistant. /plan, /tool, and Force Engine Next remain optional overrides. In this conversation response, answer "
                "the user's question directly and do not emit Epoch protocol packets.\n"
                "END_EPOCH_PROJECT_ASSISTANT_CONTEXT_V1\n\nUSER_REQUEST\n";
            prompt.append(request.data(), request.size());
            prompt += "\nEND_USER_REQUEST";
            return prompt;
        }

        void set_project_clarification_objective(EditorState& editor,
            std::string_view objective, bool fresh = false)
        {
            if (fresh || editor.aiProjectClarificationObjective.compare(objective) != 0)
            {
                editor.aiProjectClarifications.clear();
                editor.aiProjectClarificationObjective = objective;
            }
        }

        [[nodiscard]] std::string project_prompt_data(std::string_view value)
        {
            // Preserve the exact data while preventing a name/question from
            // injecting new host-framing lines or hidden GUI controls.
            std::string encoded{"\""};
            constexpr char hex[] = "0123456789abcdef";
            for (const unsigned char ch : value)
            {
                if (ch == '"' || ch == '\\') { encoded += '\\'; encoded += static_cast<char>(ch); }
                else if (ch < 0x20u || ch == 0x7fu)
                {
                    encoded += "\\u00"; encoded += hex[ch >> 4u]; encoded += hex[ch & 15u];
                }
                else encoded += static_cast<char>(ch);
            }
            return encoded + '"';
        }

        [[nodiscard]] bool ai_project_script_requested(std::string_view request)
        {
            std::string text{request};
            for (auto& ch : text) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            if (text.find("engine source") != std::string::npos
                || text.find("epoch source") != std::string::npos) return false;
            for (const std::string_view token : {std::string_view{"script"}, std::string_view{"scripts"}, std::string_view{"scripting"}})
            {
                for (auto at = text.find(token); at != std::string::npos; at = text.find(token, at + token.size()))
                {
                    const auto word = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_'; };
                    if ((at == 0u || !word(text[at - 1u]))
                        && (at + token.size() == text.size() || !word(text[at + token.size()]))) return true;
                }
            }
            return text.find(".ascript.cpp") != std::string::npos;
        }

        [[nodiscard]] std::optional<std::string> parse_ai_project_script(std::string_view reply)
        {
            constexpr std::string_view header{"EPOCH_PROJECT_SCRIPT_V1\n"};
            constexpr std::string_view begin{"\nSOURCE_BEGIN\n"};
            constexpr std::string_view end{"\nSOURCE_END\nEND"};
            if (!reply.starts_with(header) || reply.size() > 128u * 1024u
                || !ai::valid_project_context_text(reply)) return {};
            const auto start = reply.find(begin, header.size());
            const auto finish = reply.rfind(end);
            if (start == std::string_view::npos || finish == std::string_view::npos
                || finish <= start + begin.size()) return {};
            const auto metadata = reply.substr(header.size(), start - header.size());
            const auto summary = metadata.find("\nSUMMARY ");
            if (!metadata.starts_with("TITLE ") || summary == std::string_view::npos
                || summary <= 6u || summary > 166u
                || metadata.size() - summary > 778u
                || metadata.substr(summary + 1u).find('\n') != std::string_view::npos) return {};
            const auto trailing = reply.substr(finish + end.size());
            if (trailing.find_first_not_of("\r\n \t") != std::string_view::npos) return {};
            std::string source{reply.substr(start + begin.size(), finish - start - begin.size())};
            if (source.find("EPOCH_SCRIPT_EXPORT") == std::string::npos
                || source.find("run_script") == std::string::npos
                || source.find("EpochScriptHost") == std::string::npos) return {};
            source += '\n';
            return source;
        }

#if EPOCH_EDITION_PUBLIC
        [[nodiscard]] bool project_ai_scope_allowed(std::string_view projectRoot)
        {
            if (projectRoot.empty()) return true; // Plain chat can have no project context.
            std::error_code error;
            const auto root = std::filesystem::weakly_canonical(
                resolve_editor_path(std::filesystem::path{projectRoot}), error);
            if (error || !std::filesystem::is_directory(root, error) || error) return false;
            if (!editor_ai_file_scope::allows_text(root,
                    core::path::find_epoch_repo_root(core::path::executable_dir()))) return false;
            const auto owner = std::filesystem::weakly_canonical(
                std::filesystem::path{ai::default_evidence_paths().workspace_root}, error);
            if (error || owner.empty() || !owner.is_absolute()) return false;
            const auto contains = [](const std::filesystem::path& parent,
                const std::filesystem::path& child) {
                const auto relative = child.lexically_relative(parent);
                return !relative.empty() && *relative.begin() != "..";
            };
            // A normal manifest-backed Projects/<game> remains available. Engine
            // copies and their implementation trees are never automatic AI inputs.
            if (root == owner || contains(root, owner) || contains(owner / "Engine", root)) return false;
            for (auto cursor = root; !cursor.empty(); cursor = cursor.parent_path())
            {
                if (cursor != owner && std::filesystem::exists(cursor / "Engine.sln", error)) return false;
                if (error) return false;
                if (cursor == owner || cursor == cursor.parent_path()) break;
            }
            return std::filesystem::is_regular_file(root / "project.epoch.json", error) && !error;
        }

#endif
        [[nodiscard]] bool prepare_ai_project_script(EditorState& editor)
        {
#if EPOCH_EDITION_PUBLIC
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                editor.aiAuthoringStatus = "Choose a game or application project. This edition cannot read engine source for AI assistance.";
                return false;
            }
#endif
            editor.aiProjectScriptDraft.clear();
            if (editor.projectRoot.empty() || editor.scriptEditorDirty || editor.scriptBuildPending)
            {
                editor.aiAuthoringStatus = "Select a project and save unfinished script edits before requesting a script.";
                return false;
            }
            auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            if (!core::path::candidate_data_root().empty()) root = editor_resolve_candidate_project_path(root);
            std::error_code error{};
            root = std::filesystem::weakly_canonical(root, error);
            if (error || root.empty() || !root.is_absolute() || !std::filesystem::is_directory(root, error) || error) return false;
            const auto scripts = std::filesystem::weakly_canonical(root / "scripts", error);
            if (error || scripts.parent_path() != root) return false;
            auto target = std::filesystem::path{editor_resolve_script_source_path(editor.activeScript, editor.projectRoot)};
            target = std::filesystem::weakly_canonical(resolve_editor_path(target), error);
            if (error || target.parent_path() != scripts || !target.filename().string().ends_with(".ascript.cpp"))
            {
                error.clear();
                for (std::size_t index = 1u; ; ++index)
                {
                    target = scripts / ("ai_project_" + std::to_string(index) + ".ascript.cpp");
                    if (!std::filesystem::exists(target, error)) break;
                    if (error || index >= 10000u) return false;
                }
            }
            const bool existed = std::filesystem::exists(target, error);
#if EPOCH_EDITION_PUBLIC
            if (!editor_ai_file_scope::allows_text(target,
                    core::path::find_epoch_repo_root(core::path::executable_dir())))
            {
                editor.aiAuthoringStatus = "Project script points into engine source; choose a project-owned script.";
                return false;
            }
#endif
            if (error || (existed && (std::filesystem::is_symlink(std::filesystem::symlink_status(target, error))
                || error || std::filesystem::file_size(target, error) > 128u * 1024u || error))) return false;
            std::string prior{};
            if (existed)
            {
                std::ifstream file{target, std::ios::binary};
                prior.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
                if (!file || !ai::valid_project_context_text(prior)) return false;
            }
            editor.aiProjectScriptRoot = root.generic_string();
            editor.aiProjectScriptPath = target.generic_string();
            editor.aiProjectScriptExisted = existed;
            editor.aiProjectScriptPreimage = std::move(prior);
            editor.aiProjectScriptSceneRevision = editor.sceneDocumentRevision;
            return true;
        }

        [[nodiscard]] std::string build_ai_authoring_prompt(
            const EditorState& editor,
            std::string_view request,
            bool goalMilestone)
        {
            const bool script = editor.aiProjectScriptRequested;
            std::string prompt = script
                ? "Return one complete C++23 project script for review, using this exact envelope:\n"
                  "EPOCH_PROJECT_SCRIPT_V1\nTITLE short title\nSUMMARY one sentence\nSOURCE_BEGIN\n"
                  "#include <scripting.epoch_api.h>\nEPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host) { /* implementation */ }\n"
                  "SOURCE_END\nEND\n"
                  "Write the actual implementation. No Markdown or drafting notes. The destination is host-owned; never choose paths. "
                  "Epoch stages source for review. Saving, compilation and native execution are separate operator actions. "
                  "Use EpochScriptHost callbacks for scene work, never engine internals, shell, filesystem or network access. "
                  "Callbacks: log(user_data,message); rotate_all_entities_yaw(user_data,delta_degrees); "
                  "attach_selected_entity_oscillator(user_data,axis,amplitude,frequency_hz); "
                  "arrange_cubes(user_data,columns,spacing_x,spacing_z,origin_x,origin_y,origin_z) spreads existing cubes in XZ, support-snapped; "
                  "attach_cube_oscillators(user_data,count,axis,amplitude,frequency_hz) attaches motion to the first count existing cubes in document order. "
                  "axis is 0/1/2, amplitude is 0.01..100, frequency is 0.01..20 Hz. log and rotate_all_entities_yaw return void; the group/oscillator callbacks return int, 0 on success and -1 on failure. "
                  "Use group callbacks instead of enumerating every cube or fabricating IDs. Check host and callback pointers. "
                  "Motion follows Epoch's Play timeline; do not create a frame loop or sleep. Do not duplicate existing objects.\n"
                : epochengine::ai::authoring_plan_protocol_prompt();
            if (script)
                prompt += "\nCURRENT_PROJECT_SCRIPT_BEGIN\n" + editor.aiProjectScriptPreimage
                    + "\nCURRENT_PROJECT_SCRIPT_END\n";
            if (script && !editor.aiProjectScriptCompilerDiagnostics.empty())
                prompt += "\nVERIFIED_SCRIPT_COMPILER_DIAGNOSTICS_BEGIN\n" + editor.aiProjectScriptCompilerDiagnostics
                    + "\nVERIFIED_SCRIPT_COMPILER_DIAGNOSTICS_END\nRepair the earliest causal compiler error in the current script. Return a complete corrected script for fresh review.\n";
            prompt += "\n\nActive Epoch authoring context:\n";
            prompt += "Project: "
                + (editor.projectName.empty()
                    ? std::string{"No project selected"}
                    : editor.projectName)
                + "\n";
            prompt += "Project ID: "
                + (editor.projectId.empty()
                    ? std::string{"none"}
                    : editor.projectId)
                + "\n";
            const std::size_t worldObjectCount = static_cast<std::size_t>(
                std::count_if(
                    editor.entities.begin(),
                    editor.entities.end(),
                    [](const EditorEntity& entity)
                    {
                        return !is_screen_space_gui_entity(entity);
                    }));
            prompt += "World scene objects: "
                + std::to_string(worldObjectCount) + "\n";
            prompt += "Canonical archetype counts:\n";
            constexpr std::array sceneArchetypes{
                std::string_view{"cube"},
                std::string_view{"ground"},
                std::string_view{"light"},
                std::string_view{"spawn"},
                std::string_view{"camera"}};
            for (const std::string_view archetype : sceneArchetypes)
            {
                const std::size_t count = static_cast<std::size_t>(
                    std::count_if(
                        editor.entities.begin(),
                        editor.entities.end(),
                        [archetype](const EditorEntity& entity)
                        {
                            return !is_screen_space_gui_entity(entity)
                                && ai_authoring_scene_archetype(entity)
                                    == archetype;
                        }));
                prompt += "- " + std::string{archetype} + ": "
                    + std::to_string(count) + "\n";
            }
            std::size_t inventoryCount = 0u;
            const auto inventoryBudget = epochengine::ai::inference_budget(
                epochengine::ai::InferenceWorkload::authoring).maximum_prompt_bytes / 2u;
            if (!script) prompt += "Object records: id, name, type, p=position; omitted r=rotation means 0,0,0; omitted s=scale means 1,1,1.\n";
            for (std::size_t index = 0u; !script && index < editor.entities.size(); ++index)
            {
                const EditorEntity& entity = editor.entities[index];
                if (is_screen_space_gui_entity(entity))
                    continue;
                std::string record = epochengine::format_text(
                    "id={} {} {} p={},{},{}",
                    entity.sceneObjectId,
                    project_prompt_data(entity.name),
                    project_prompt_data(entity.type),
                    entity.position[0],
                    entity.position[1],
                    entity.position[2]);
                if (entity.rotation != std::array{0.0f, 0.0f, 0.0f})
                    record += epochengine::format_text(" r={},{},{}", entity.rotation[0], entity.rotation[1], entity.rotation[2]);
                if (entity.scale != std::array{1.0f, 1.0f, 1.0f})
                    record += epochengine::format_text(" s={},{},{}", entity.scale[0], entity.scale[1], entity.scale[2]);
                record += '\n';
                if (prompt.size() + record.size() > inventoryBudget) break;
                prompt += record;
                ++inventoryCount;
            }
            if (!script && worldObjectCount > inventoryCount)
            {
                prompt += "- ... "
                    + std::to_string(worldObjectCount - inventoryCount)
                    + " more object(s) omitted from prompt\n";
            }
            if (!script) prompt += "The inventory above is canonical. Reuse matching objects "
                "and use scene.reconcile for requested final counts; do not add "
                "duplicates merely because an archetype appears in the request. "
                "scene.create and gui.create are idempotent minimum-count requests; "
                "they never mean append another matching object. "
                "Use scene.transform with a listed id or exact unique object_name for position, rotation, "
                "or scale changes. Static meshes snap to the support surface "
                "unless placement=free is explicit. scene.reconcile changes exact counts only; "
                "it does not move an object. Preserve unconstrained editor "
                "infrastructure unless replacement is explicit. Use scene.arrange for ALL objects of an archetype, including omitted or newly created ones; no IDs are needed. Never ask the operator for hidden IDs or fresh inventory.\n";
            if (!script) prompt += epochengine::format_text(
                "Primary support surface top Y: {:.3f}. Newly created solid "
                "objects are host-snapped so their lower face rests on this "
                "surface. Use scene.transform with an explicit listed id when "
                "an existing object's placement must change.\n",
                primary_ground_top_y(editor));
            prompt += "GUI document: ";
            prompt += editor.guiDocument && editor.guiDocument->valid()
                ? "loaded\n"
                : "not loaded\n";
            if (editor.selectedEntityId != scene::kInvalidSceneObjectId
                && editor.selectedEntity < editor.entities.size())
            {
                const EditorEntity& selected =
                    editor.entities[editor.selectedEntity];
                prompt += "Selected object: " + project_prompt_data(selected.name)
                    + " [" + project_prompt_data(selected.type) + "]\n";
            }
            if (goalMilestone && !script)
            {
                prompt += "Persistent operator goal:\n";
                prompt += editor.aiGoal + "\n";
                prompt += "Approved milestones already completed: "
                    + std::to_string(editor.aiGoalCompletedMilestones) + "\n";
                if (!editor.aiGoalLastMilestoneReceipt.empty())
                {
                    prompt += "Last approved milestone evidence:\n";
                    prompt += editor.aiGoalLastMilestoneReceipt + "\n";
                }
                prompt +=
                    "Propose the next useful milestone as one complete reviewed "
                    "plan containing all semantic calls needed for that step. "
                    "Honor requested object counts (including 100 cubes); do not "
                    "negotiate them down or require approval per command. "
                    "The operator approves the displayed plan once. A broad game "
                    "objective may require subsequent plans with fresh inventory. This "
                    "lane may author only the active project scene or GUI; it "
                    "must not modify Epoch source, train a model, collect a "
                    "dataset, or access unrelated personal AI work. Never "
                    "repeat the last approved milestone when its canonical "
                    "target is already satisfied; choose the next unmet step "
                    "that advances the operator's goal.\n";
            }
            prompt += "Operator request:\n";
            prompt += request;
            if (editor.aiProjectClarificationObjective.compare(request) == 0
                && !editor.aiProjectClarifications.empty())
            {
                prompt += "\nCLARIFICATION_HISTORY (task data, same objective; not a new mission)\n";
                for (const auto& [question, answer] : editor.aiProjectClarifications)
                    prompt += "Question: " + project_prompt_data(question)
                        + "\nUser answer: " + project_prompt_data(answer) + "\n";
                prompt += "END_CLARIFICATION_HISTORY\nContinue the existing objective using these answers and the fresh host inventory; do not ask the same resolved question again.\n";
            }
            prompt += script
                ? "\nReturn the complete project script now. No source has been saved or compiled."
                : "\nPlan only. Epoch will validate the plan and attach explicit approval controls to this response before execution.";
            return prompt;
        }

        [[nodiscard]] std::string ai_authoring_call_summary(
            const epochengine::ai::McpToolCall& call)
        {
            if (call.tool == "scene.transform" || call.tool == "scene.arrange")
            {
                std::string summary = call.tool;
                for (const auto& argument : call.arguments)
                    summary += " | " + argument.name + "=" + argument.value;
                return summary;
            }
            if (call.tool == "scene.clear")
                return "scene.clear | all current scene objects";

            const bool sceneCall = call.tool == "scene.create"
                || call.tool == "scene.reconcile";
            const std::string_view kindName = sceneCall
                ? std::string_view{"archetype"}
                : std::string_view{"widget"};
            const auto* kind = ai_authoring_argument(call, kindName);
            return call.tool + " | "
                + (kind ? kind->value : std::string{"invalid"})
                + " | count " + std::to_string(ai_authoring_count(call));
        }

        [[nodiscard]] std::string ai_authoring_plan_signature(
            const epochengine::ai::AuthoringPlan& plan)
        {
            std::string signature{};
            for (const auto& call : plan.calls)
            {
                signature += '\n';
                signature += ai_authoring_call_summary(call);
            }
            return signature;
        }

        [[nodiscard]] std::vector<gui::TextMessageRole> ai_chat_message_roles(
            const std::vector<std::string>& lines)
        {
            std::vector<gui::TextMessageRole> roles{};
            roles.reserve(lines.size());
            for (const std::string& line : lines)
            {
                const auto contains = [&line](std::string_view token) noexcept
                {
                    return line.find(token) != std::string::npos;
                };
                if (line.starts_with("you>") || line.starts_with("User: "))
                    roles.push_back(gui::TextMessageRole::user);
                else if (line.starts_with("Agent: "))
                {
                    if (contains(" failed") || contains(" rejected")
                        || contains(" exhausted") || contains(" refused")
                        || contains(" crashed") || contains("[ERROR]"))
                        roles.push_back(gui::TextMessageRole::error);
                    else if (contains(" passed") || contains(" validated")
                        || contains(" succeeded") || contains(" compiled")
                        || contains(" built"))
                        roles.push_back(gui::TextMessageRole::success);
                    else if (contains(" queued") || contains(" waiting")
                        || contains(" pending") || contains(" stopping")
                        || contains(" correction"))
                        roles.push_back(gui::TextMessageRole::warning);
                    else
                        roles.push_back(gui::TextMessageRole::engine);
                }
                else if (line.starts_with("ai>") || line.starts_with("Assistant: "))
                    roles.push_back(gui::TextMessageRole::assistant);
                else if (line.starts_with("[ERROR]") || line.starts_with("[error]")
                    || contains(" failed") || contains(" rejected")
                    || contains(" refused") || contains(" exhausted")
                    || contains(" crashed"))
                    roles.push_back(gui::TextMessageRole::error);
                else if (contains(" accepted") || contains(" passed")
                    || contains(" validated") || contains(" succeeded")
                    || contains(" ready") || contains(" built"))
                    roles.push_back(gui::TextMessageRole::success);
                else if (contains(" queued") || contains(" waiting")
                    || contains(" pending") || contains(" retained")
                    || contains(" correction") || contains(" stopping")
                    || contains(" stopped"))
                    roles.push_back(gui::TextMessageRole::warning);
                else if ((line.starts_with("[self-coding]")
                    || line.starts_with("[Assistant]"))
                    || line.starts_with("[self-iteration]")
                    || line.starts_with("[ai-source]")
                    || line.starts_with("[source]"))
                    roles.push_back(gui::TextMessageRole::engine);
                else
                    roles.push_back(gui::TextMessageRole::system);
            }
            return roles;
        }

        [[nodiscard]] bool parse_ai_authoring_vector3(
            const epochengine::ai::McpToolCall& call,
            std::string_view name,
            std::array<float, 3>& value) noexcept
        {
            const auto* argument = ai_authoring_argument(call, name);
            if (!argument)
                return false;
            std::string_view remaining = argument->value;
            for (std::size_t index = 0u; index < value.size(); ++index)
            {
                const std::size_t separator = remaining.find(',');
                const std::string_view component = separator == std::string_view::npos
                    ? remaining
                    : remaining.substr(0u, separator);
                const char* const first = component.data();
                const char* const last = first + component.size();
                const auto converted = std::from_chars(first, last, value[index]);
                if (component.empty() || converted.ec != std::errc{}
                    || converted.ptr != last || !std::isfinite(value[index]))
                    return false;
                if (index + 1u < value.size())
                {
                    if (separator == std::string_view::npos)
                        return false;
                    remaining.remove_prefix(separator + 1u);
                }
                else if (separator != std::string_view::npos)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::optional<scene::SceneObjectId>
        ai_authoring_scene_object_id(
            const epochengine::ai::McpToolCall& call) noexcept
        {
            const auto* argument = ai_authoring_argument(call, "object_id");
            if (!argument)
                return std::nullopt;
            scene::SceneObjectId value{};
            const char* const first = argument->value.data();
            const char* const last = first + argument->value.size();
            const auto converted = std::from_chars(first, last, value);
            if (converted.ec != std::errc{} || converted.ptr != last
                || value == scene::kInvalidSceneObjectId)
                return std::nullopt;
            return value;
        }

        struct AiSceneReconcileResult final
        {
            bool succeeded{};
            std::size_t created{};
            std::size_t removed{};
            std::size_t reused{};
            std::string error{};
        };

        // A task has no aggregate work-item quota. Bound each canonical
        // transaction and adapt to its measured worker cost, not an invented
        // renderer/GPU measurement. Leave capacity for settings operations.
        constexpr std::size_t kAiAuthoringMaximumBatchObjects = 128u;
        constexpr std::uint64_t kAiAuthoringTargetBatchUs = 8'000u;

        [[nodiscard]] std::size_t ai_authoring_batch_size(const EditorState& editor,
            std::size_t remaining)
        {
            const auto requested = editor.aiAuthoringApplyProgress
                ? editor.aiAuthoringApplyProgress->nextBatch.load(std::memory_order_relaxed)
                : kAiAuthoringMaximumBatchObjects;
            return (std::min)(remaining, (std::clamp)(requested,
                std::size_t{1u}, kAiAuthoringMaximumBatchObjects));
        }

        void observe_ai_authoring_batch(AiAuthoringApplyProgress& progress,
            std::size_t count, std::uint64_t elapsedUs)
        {
            progress.objects.fetch_add(count, std::memory_order_relaxed);
            progress.batches.fetch_add(1u, std::memory_order_relaxed);
            progress.lastBatchUs.store(elapsedUs, std::memory_order_relaxed);
            progress.peakBatchUs.store((std::max)(elapsedUs,
                progress.peakBatchUs.load(std::memory_order_relaxed)), std::memory_order_relaxed);
            const auto current = progress.nextBatch.load(std::memory_order_relaxed);
            // Don't learn from a short final batch. Grow gradually; a slow
            // transaction shrinks immediately, including down to one item.
            if (count >= current || elapsedUs > kAiAuthoringTargetBatchUs)
            {
                const auto estimate = (std::max)(std::uint64_t{1u},
                    count * kAiAuthoringTargetBatchUs / (std::max)(elapsedUs, std::uint64_t{1u}));
                const auto next = elapsedUs > kAiAuthoringTargetBatchUs
                    ? (std::min)(estimate, static_cast<std::uint64_t>(current))
                    : (std::min)(estimate, static_cast<std::uint64_t>(current * 2u));
                progress.nextBatch.store(static_cast<std::size_t>((std::clamp)(next,
                    std::uint64_t{1u}, static_cast<std::uint64_t>(kAiAuthoringMaximumBatchObjects))),
                    std::memory_order_relaxed);
            }
        }

        void record_ai_authoring_batch(EditorState& editor, std::size_t count,
            std::chrono::steady_clock::time_point started)
        {
            if (!editor.aiAuthoringApplyProgress) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count();
            observe_ai_authoring_batch(*editor.aiAuthoringApplyProgress, count,
                static_cast<std::uint64_t>((std::max)(elapsed, decltype(elapsed){0})));
            std::this_thread::yield();
        }

        [[nodiscard]] bool create_ai_entity_batch(EditorState& editor,
            std::string_view archetype, std::size_t count, std::string& error,
            std::stop_token cancellation = {})
        {
            const auto started = std::chrono::steady_clock::now();
            std::vector<EditorEntity> created;
            created.reserve(count);
            for (std::size_t index = 0u; index < count; ++index)
            {
                if (cancellation.stop_requested()) { error = "Authoring job cancelled."; return false; }
                auto entity = make_editor_entity(editor, archetype, index);
                if (!entity) { error = editor.projectStatus; return false; }
                created.push_back(std::move(*entity));
            }
            if (!create_editor_scene_entities(editor, std::move(created), "AI approved batch creation"))
            {
                error = editor.projectStatus;
                return false;
            }
            record_ai_authoring_batch(editor, count, started);
            return true;
        }

        [[nodiscard]] AiSceneReconcileResult reconcile_ai_scene_archetype(
            EditorState& editor,
            std::string_view archetype,
            std::size_t targetCount, std::stop_token cancellation = {})
        {
            AiSceneReconcileResult result{};
            std::vector<scene::SceneObjectId> matchingIds{};
            for (const EditorEntity& entity : editor.entities)
            {
                const auto candidate = ai_authoring_scene_archetype(entity);
                if (candidate && *candidate == archetype
                    && entity.sceneObjectId != scene::kInvalidSceneObjectId)
                {
                    matchingIds.push_back(entity.sceneObjectId);
                }
            }

            result.reused = (std::min)(matchingIds.size(), targetCount);
            for (std::size_t remaining = matchingIds.size(); remaining > targetCount;)
            {
                if (cancellation.stop_requested()) { result.error = "Authoring job cancelled."; return result; }
                const auto started = std::chrono::steady_clock::now();
                const auto batch = ai_authoring_batch_size(editor, remaining - targetCount);
                std::vector<authoring::scene::SceneOperation> operations;
                auto settings = editor.sceneDocument.project_settings();
                bool settingsChanged{};
                for (std::size_t index = 0u; index < batch; ++index)
                {
                    const auto id = matchingIds[remaining - index - 1u];
                    const auto handle = editor.sceneDocument.find(authoring::scene::ObjectId{id});
                    if (!handle) { result.error = "Reconciliation target became stale."; return result; }
                    operations.emplace_back(authoring::scene::ObjectDestroyedOperation{.object = *handle});
                    if (settings.primary_camera.value == id) { settings.primary_camera = {}; settingsChanged = true; }
                    if (settings.primary_spawn.value == id) { settings.primary_spawn = {}; settingsChanged = true; }
                }
                if (settingsChanged)
                {
                    settings.run_state = scene_tier0::ProjectRunState::blocked;
                    operations.emplace_back(authoring::scene::ProjectSettingsChangedOperation{.value = settings});
                }
                if (!apply_editor_scene_transaction(editor, operations, "AI approved batch removal"))
                {
                    result.error = editor.projectStatus;
                    return result;
                }
                result.removed += batch;
                remaining -= batch;
                record_ai_authoring_batch(editor, batch, started);
            }

            const std::size_t retainedCount = matchingIds.size() - result.removed;
            for (std::size_t index = retainedCount; index < targetCount;)
            {
                const auto batch = ai_authoring_batch_size(editor, targetCount - index);
                if (!create_ai_entity_batch(editor, archetype, batch, result.error, cancellation)) return result;
                result.created += batch;
                index += batch;
            }

            const std::size_t finalCount = static_cast<std::size_t>(
                std::count_if(
                    editor.entities.begin(),
                    editor.entities.end(),
                    [archetype](const EditorEntity& entity)
                    {
                        const auto candidate =
                            ai_authoring_scene_archetype(entity);
                        return candidate && *candidate == archetype;
                    }));
            result.succeeded = finalCount == targetCount;
            if (!result.succeeded)
            {
                result.error = epochengine::format_text(
                    "scene.reconcile expected {} {} object(s), but the canonical scene contains {}.",
                    targetCount,
                    archetype,
                    finalCount);
            }
            return result;
        }

        [[nodiscard]] bool transform_ai_scene_object(
            EditorState& editor,
            const epochengine::ai::McpToolCall& call,
            std::string& error)
        {
            const auto objectId = ai_authoring_scene_object_id(call);
            auto entityIndex = objectId
                ? editor_entity_index(editor, *objectId)
                : std::nullopt;
            if (const auto* name = ai_authoring_argument(call, "object_name"))
            {
                for (std::size_t index = 0; index < editor.entities.size(); ++index)
                {
                    const auto& entity = editor.entities[index];
                    if (is_screen_space_gui_entity(entity) || entity.name != name->value) continue;
                    if (entityIndex)
                    {
                        error = "scene.transform object_name is ambiguous; select one object or use a listed ID. Nothing was applied.";
                        return false;
                    }
                    entityIndex = index;
                }
            }
            if (!entityIndex)
            {
                error = "scene.transform could not resolve its ID or exact name in the canonical scene; refresh the staged plan, not the user's IDs.";
                return false;
            }

            const EditorEntity& current = editor.entities[*entityIndex];
            EditorEntity transformed = current;
            std::array<float, 3> value{};
            const bool positionRequested =
                ai_authoring_argument(call, "position") != nullptr;
            const bool scaleRequested =
                ai_authoring_argument(call, "scale") != nullptr;
            if (positionRequested)
            {
                if (!parse_ai_authoring_vector3(call, "position", value))
                {
                    error = "scene.transform position could not be decoded.";
                    return false;
                }
                transformed.position = value;
            }
            if (ai_authoring_argument(call, "rotation"))
            {
                if (!parse_ai_authoring_vector3(call, "rotation", value))
                {
                    error = "scene.transform rotation could not be decoded.";
                    return false;
                }
                transformed.rotation = value;
            }
            if (scaleRequested)
            {
                if (!parse_ai_authoring_vector3(call, "scale", value))
                {
                    error = "scene.transform scale could not be decoded.";
                    return false;
                }
                transformed.scale = value;
            }
            const auto* placement = ai_authoring_argument(call, "placement");
            const bool supportPlacement = !placement
                || placement->value == "support";
            if (current.type == "StaticMesh" && supportPlacement
                && (positionRequested || scaleRequested))
            {
                if (!align_editor_entity_bottom_to_surface(
                        transformed,
                        primary_ground_top_y(editor)))
                {
                    error =
                        "scene.transform could not align invalid object bounds to the support surface.";
                    return false;
                }
            }
            if (transformed.position == current.position
                && transformed.rotation == current.rotation
                && transformed.scale == current.scale)
            {
                return true; // Already satisfied, not a failed semantic transaction.
            }

            const std::uint64_t revisionBefore = editor.sceneDocumentRevision;
            if (!transform_editor_scene_entity(
                    editor,
                    transformed,
                    "AI transform " + current.name)
                || editor.sceneDocumentRevision == revisionBefore)
            {
                error = "scene.transform failed to commit through the canonical scene history.";
                return false;
            }
            return true;
        }

        [[nodiscard]] bool arrange_ai_scene_objects(EditorState& editor,
            const epochengine::ai::McpToolCall& call, std::size_t& changed,
            std::string& error, std::stop_token cancellation = {})
        {
            const auto* archetype = ai_authoring_argument(call, "archetype");
            const auto* columns = ai_authoring_argument(call, "columns");
            unsigned width{};
            std::array<float, 3> spacing{}, origin{};
            if (!archetype || !columns
                || !parse_ai_authoring_vector3(call, "spacing", spacing)
                || !parse_ai_authoring_vector3(call, "origin", origin)) return false;
            const auto converted = std::from_chars(columns->value.data(),
                columns->value.data() + columns->value.size(), width);
            if (converted.ec != std::errc{} || converted.ptr != columns->value.data() + columns->value.size()
                || width == 0u) return false;
            std::vector<EditorEntity> targets;
            for (const auto& entity : editor.entities)
                if (!is_screen_space_gui_entity(entity)
                    && ai_authoring_scene_archetype(entity) == archetype->value)
                    targets.push_back(entity);
            if (targets.empty())
            {
                error = "scene.arrange requires an existing bounded group; create/reconcile it earlier in the approved plan.";
                return false;
            }
            std::unordered_map<scene::SceneObjectId, authoring::scene::ObjectHandle> handles;
            for (const auto& object : editor.sceneDocument.objects()) handles.emplace(object.descriptor.id.value, object.handle);
            const auto* placement = ai_authoring_argument(call, "placement");
            const float groundTop = primary_ground_top_y(editor);
            auto prepared = targets;
            for (std::size_t index = 0u; index < prepared.size(); ++index)
            {
                const float row = static_cast<float>(index / width);
                auto& transformed = prepared[index];
                transformed.position = {origin[0] + static_cast<float>(index % width) * spacing[0],
                    origin[1] + row * spacing[1], origin[2] + row * spacing[2]};
                if (!handles.contains(transformed.sceneObjectId)
                    || std::ranges::any_of(transformed.position, [](float value)
                        { return !std::isfinite(value) || std::abs(value) > 100000.0f; }))
                { error = "scene.arrange grid exceeds the canonical transform bounds or contains a stale object."; return false; }
                if (transformed.type == "StaticMesh" && (!placement || placement->value == "support")
                    && !align_editor_entity_bottom_to_surface(transformed, groundTop))
                { error = "Grid support alignment failed."; return false; }
            }
            for (std::size_t first = 0u; first < targets.size();)
            {
              const auto started = std::chrono::steady_clock::now();
              if (cancellation.stop_requested()) { error = "Authoring job cancelled."; return false; }
              std::vector<authoring::scene::SceneOperation> operations;
              const auto end = first + ai_authoring_batch_size(editor, targets.size() - first);
              for (std::size_t index = first; index < end; ++index)
              {
                const auto& transformed = prepared[index];
                if (transformed.position == targets[index].position) continue;
                operations.emplace_back(authoring::scene::ObjectTransformChangedOperation{
                    .object = handles.at(transformed.sceneObjectId),
                    .value = {
                        .position = {transformed.position[0], transformed.position[1], transformed.position[2]},
                        .rotation_degrees = {transformed.rotation[0], transformed.rotation[1], transformed.rotation[2]},
                        .scale = {transformed.scale[0], transformed.scale[1], transformed.scale[2]}}});
              }
              if (!operations.empty() && !apply_editor_scene_transaction(editor, operations, "AI approved grid batch"))
              { error = editor.projectStatus; return false; }
              changed += operations.size();
              record_ai_authoring_batch(editor, end - first, started);
              first = end;
            }
            return true;
        }

        [[nodiscard]] bool clear_ai_scene_batches(EditorState& editor,
            std::string& error, std::stop_token cancellation)
        {
            while (!editor.entities.empty())
            {
                const auto started = std::chrono::steady_clock::now();
                if (cancellation.stop_requested()) { error = "Authoring job cancelled."; return false; }
                const auto objects = editor.sceneDocument.objects();
                const auto batch = ai_authoring_batch_size(editor, objects.size());
                if (batch == 0u) { error = "Scene projection does not match its document."; return false; }
                auto settings = editor.sceneDocument.project_settings();
                settings.primary_camera = {}; settings.primary_spawn = {};
                settings.run_state = scene_tier0::ProjectRunState::blocked;
                std::vector<authoring::scene::SceneOperation> operations{
                    authoring::scene::ProjectSettingsChangedOperation{.value = settings}};
                for (std::size_t index = 0; index < batch; ++index)
                    operations.emplace_back(authoring::scene::ObjectDestroyedOperation{
                        .object = objects[objects.size() - index - 1u].handle});
                if (!apply_editor_scene_transaction(editor, operations, "AI approved clear batch"))
                { error = editor.projectStatus; return false; }
                record_ai_authoring_batch(editor, batch, started);
            }
            clear_editor_selection(editor);
            return true;
        }

        [[nodiscard]] bool apply_ai_authoring_plan(EditorState& editor, bool publishTrace = true,
            std::stop_token cancellation = {})
        {
            if (!editor.aiAuthoringPlan
                || editor.aiAuthoringPlan.plan.calls.empty()
                || editor.aiAuthoringPlanApplied)
            {
                editor.aiAuthoringStatus =
                    "No unapplied validated AI authoring plan is available.";
                return false;
            }

            const auto registry =
                epochengine::ai::make_epoch_project_tool_registry({
                    .maximum_session_steps = (std::numeric_limits<std::uint32_t>::max)()});
            epochengine::ai::McpSessionAuthority authority{
                .session_id = "editor-authoring-preview",
                .granted_capabilities =
                    epochengine::ai::McpToolCapability::author};
            const auto limits = editor.sceneDocument.initialized()
                ? editor.sceneDocument.limits() : authoring::scene::DocumentLimits{};
            // Counts describe final state, not allocation authority or a sum of
            // every repeated reconcile. Admit against the actual native profile
            // before any work/allocation; never silently clamp a large request.
            std::unordered_map<std::string, std::size_t> counts;
            std::size_t totalObjects = editor.entities.size();
            for (const auto& entity : editor.entities)
            {
                if (const auto kind = ai_authoring_scene_archetype(entity)) ++counts[std::string{*kind}];
                else if (const auto kind = ai_authoring_gui_widget(entity)) ++counts["gui_" + std::string{*kind}];
            }
            for (const auto& call : editor.aiAuthoringPlan.plan.calls)
            {
                authority.approved_call_ids.push_back(call.call_id);
                if (call.tool == "scene.clear") { counts.clear(); totalObjects = 0u; continue; }
                if (call.tool == "scene.transform" || call.tool == "scene.arrange") continue;
                const auto* kind = ai_authoring_argument(call, call.tool == "gui.create" ? "widget" : "archetype");
                if (!kind) continue; // The exact argument validator below owns malformed calls.
                const std::string key = call.tool == "gui.create" ? "gui_" + kind->value : kind->value;
                auto& existing = counts[key];
                const auto requested = static_cast<std::size_t>(ai_authoring_count(call));
                const auto target = call.tool == "scene.reconcile" ? requested : (std::max)(existing, requested);
                totalObjects = totalObjects - existing + target;
                existing = target;
                if (totalObjects > limits.maximum_active_objects
                    || counts["light"] > limits.maximum_lights || counts["ground"] > limits.maximum_grounds)
                {
                    if (editor.aiAuthoringPlanForGoal)
                    {
                        editor.aiGoalRunning = false;
                        editor.aiGoalPlanNextQueued = false;
                    }
                    editor.aiAuthoringStatus = epochengine::format_text(
                        "Requested {} native objects; this scene profile admits {} objects, {} lights and {} grounds. Large populations require an instanced/streamed representation, not silently reduced counts{}.",
                        totalObjects, limits.maximum_active_objects, limits.maximum_lights, limits.maximum_grounds,
                        editor.aiAuthoringPlanForGoal
                            ? "; the goal was paused"
                            : "");
                    return false;
                }
            }
            for (const auto& call : editor.aiAuthoringPlan.plan.calls)
            {
                std::string diagnostic;
                if (!epochengine::ai::authoring_plan_detail::validate_call(call, diagnostic))
                {
                    editor.aiAuthoringStatus = "AI plan argument validation failed before apply: " + diagnostic;
                    editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                    return false;
                }
                const auto validation = registry.validate(call, authority);
                if (!validation)
                {
                    editor.aiAuthoringStatus =
                        "AI plan validation failed at apply time: "
                        + validation.message;
                    return false;
                }
            }

            std::size_t appliedCommands = 0u;
            std::size_t createdObjects = 0u;
            std::size_t removedObjects = 0u;
            std::size_t reusedObjects = 0u;
            std::size_t transformedObjects = 0u;
            bool sceneCleared = false;
            bool guiChanged = false;
            bool idempotentOnly = true;
            const std::string planSignature = ai_authoring_plan_signature(
                editor.aiAuthoringPlan.plan);
            const std::uint64_t revisionBeforePlan = editor.sceneDocumentRevision;
            // Validate the entire visible plan first; retain canonical documents
            // and projections so a later command failure cannot leave half a
            // multi-command approval applied. Successful commands retain normal
            // semantic undo history; no source/file/runtime authority is added.
            auto originalScene = editor.sceneDocument;
            auto originalGui = editor.guiDocument
                ? std::make_unique<authoring::gui::GuiDocument>(*editor.guiDocument)
                : nullptr;
            auto originalEntities = editor.entities;
            const auto originalSelectedId = editor.selectedEntityId;
            auto originalSelectedIds = editor.selectedEntityIds;
            const auto originalSelectedIndex = editor.selectedEntity;
            auto originalGuiWidgets = editor.guiDocumentWidgets;
            const auto originalGuiRoot = editor.guiDocumentRoot;
            for (const auto& call : editor.aiAuthoringPlan.plan.calls)
            {
                idempotentOnly = idempotentOnly
                    && (call.tool == "scene.reconcile"
                        || call.tool == "scene.transform"
                        || call.tool == "scene.arrange"
                        || call.tool == "scene.create"
                        || call.tool == "gui.create");
                bool callSucceeded = false;
                std::string callFailure{};
                if (cancellation.stop_requested()) callFailure = "Authoring job cancelled.";
                else if (call.tool == "scene.clear")
                {
                    if (!editor.sceneDocument.initialized())
                        (void)rebuild_editor_scene_document(editor, "Initialize AI clear batch");
                    callSucceeded = clear_ai_scene_batches(editor, callFailure, cancellation);
                    sceneCleared = sceneCleared || callSucceeded;
                }
                else if (call.tool == "scene.reconcile")
                {
                    const auto* kind =
                        ai_authoring_argument(call, "archetype");
                    if (kind)
                    {
                        AiSceneReconcileResult reconcile =
                            reconcile_ai_scene_archetype(
                                editor,
                                kind->value,
                                ai_authoring_count(call), cancellation);
                        callSucceeded = reconcile.succeeded;
                        createdObjects += reconcile.created;
                        removedObjects += reconcile.removed;
                        reusedObjects += reconcile.reused;
                        callFailure = std::move(reconcile.error);
                    }
                    else
                    {
                        callFailure =
                            "scene.reconcile is missing its archetype.";
                    }
                }
                else if (call.tool == "scene.transform")
                {
                    const auto revision = editor.sceneDocumentRevision;
                    callSucceeded = transform_ai_scene_object(
                        editor, call, callFailure);
                    transformedObjects += callSucceeded && editor.sceneDocumentRevision != revision ? 1u : 0u;
                }
                else if (call.tool == "scene.arrange")
                {
                    callSucceeded = arrange_ai_scene_objects(editor, call, transformedObjects, callFailure, cancellation);
                }
                else if (call.tool == "scene.create")
                {
                    const auto* kind =
                        ai_authoring_argument(call, "archetype");
                    if (!kind)
                    {
                        callFailure =
                            "scene.create is missing its archetype.";
                    }
                    else
                    {
                        const std::size_t existing =
                            static_cast<std::size_t>(std::count_if(
                                editor.entities.begin(),
                                editor.entities.end(),
                                [&kind](const EditorEntity& entity)
                                {
                                    const auto candidate =
                                        ai_authoring_scene_archetype(entity);
                                    return candidate
                                        && *candidate == kind->value;
                                }));
                        AiSceneReconcileResult reconcile =
                            reconcile_ai_scene_archetype(
                                editor,
                                kind->value,
                                (std::max)(
                                    existing,
                                    static_cast<std::size_t>(
                                        ai_authoring_count(call))), cancellation);
                        callSucceeded = reconcile.succeeded;
                        createdObjects += reconcile.created;
                        reusedObjects += reconcile.reused;
                        callFailure = std::move(reconcile.error);
                    }
                }
                else if (call.tool == "gui.create")
                {
                    const auto* kind =
                        ai_authoring_argument(call, "widget");
                    if (!kind)
                    {
                        callFailure =
                            "gui.create is missing its widget.";
                    }
                    else
                    {
                        const std::size_t target =
                            ai_authoring_count(call);
                        const std::size_t existing =
                            static_cast<std::size_t>(std::count_if(
                                editor.entities.begin(),
                                editor.entities.end(),
                                [&kind](const EditorEntity& entity)
                                {
                                    const auto candidate =
                                        ai_authoring_gui_widget(entity);
                                    return candidate
                                        && *candidate == kind->value;
                                }));
                        reusedObjects += (std::min)(existing, target);
                        callSucceeded = true;
                        const std::size_t createdBefore =
                            createdObjects;
                        for (std::size_t index = existing;
                            callSucceeded && index < target;
                            )
                        {
                            const auto batch = ai_authoring_batch_size(editor, target - index);
                            callSucceeded = create_ai_entity_batch(editor, "gui_" + kind->value, batch, callFailure, cancellation);
                            if (callSucceeded) createdObjects += batch;
                            index += batch;
                        }
                        guiChanged = guiChanged
                            || createdObjects > createdBefore;
                    }
                }
                else
                {
                    callFailure =
                        "The validated AI command has no authoring executor.";
                }

                if (callSucceeded)
                    ++appliedCommands;
                if (editor.aiAuthoringApplyProgress)
                    editor.aiAuthoringApplyProgress->commands.store(appliedCommands, std::memory_order_relaxed);
                if (publishTrace) epochengine::ai::append_tool_trace(
                    epochengine::ai::McpCaptureRecord{
                        .session_id = editor.projectId.empty()
                            ? std::string{"editor-authoring"}
                            : editor.projectId + "-editor-authoring",
                        .call_id = call.call_id,
                        .server = "editor",
                        .tool = call.tool,
                        .prompt = editor.aiAuthoringPlanForGoal
                            ? editor.aiGoal
                            : editor.aiAuthoringRequest,
                        .normalized_output = callSucceeded
                            ? "approved semantic authoring command applied"
                            : "semantic authoring command failed",
                        .source_path = editor.projectScenePath,
                        .state = callSucceeded
                            ? epochengine::ai::McpCallState::succeeded
                            : epochengine::ai::McpCallState::failed,
                        .error = callSucceeded
                            ? epochengine::ai::McpErrorCode::none
                            : epochengine::ai::McpErrorCode::execution_failed});
                if (!callSucceeded)
                {
                    editor.sceneDocument = std::move(originalScene);
                    editor.guiDocument = std::move(originalGui);
                    editor.entities = std::move(originalEntities);
                    editor.selectedEntityId = originalSelectedId;
                    editor.selectedEntityIds = std::move(originalSelectedIds);
                    editor.selectedEntity = originalSelectedIndex;
                    editor.guiDocumentWidgets = std::move(originalGuiWidgets);
                    editor.guiDocumentRoot = originalGuiRoot;
                    editor.sceneDocumentRevision = revisionBeforePlan;
                    editor.sceneInteractionDirty = editor.sceneLightingDirty = true;
                    editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                    editor.aiAuthoringStatus = callFailure.empty()
                        ? "AI authoring plan failed; all commands were rolled back."
                        : std::move(callFailure);
                    editor.aiAuthoringStatus += " The approved batch was rolled back; no partial scene/GUI edit remains.";
                    push_editor_log(
                        editor,
                        "[ai] " + editor.aiAuthoringStatus);
                    return false;
                }
            }

            const bool sceneChanged =
                editor.sceneDocumentRevision != revisionBeforePlan;
            if (!sceneChanged && !idempotentOnly)
            {
                editor.aiAuthoringStatus =
                    "The approved plan made no canonical scene change. The goal was paused at this milestone instead of claiming completion.";
                editor.aiGoalRunning = false;
                editor.aiGoalPlanNextQueued = false;
                push_editor_log(editor, "[ai] " + editor.aiAuthoringStatus);
                return false;
            }

            const bool attemptedMilestone =
                std::ranges::find(
                    editor.aiGoalAttemptedMilestoneSignatures,
                    planSignature)
                != editor.aiGoalAttemptedMilestoneSignatures.end();
            if (!sceneChanged
                && editor.aiAuthoringPlanForGoal
                && editor.aiGoalActive
                && attemptedMilestone)
            {
                editor.aiAuthoringPlanApplied = true;
                editor.aiGoalRunning = false;
                editor.aiGoalPlanNextQueued = false;
                editor.aiAuthoringStatus =
                    "The model repeated the last already-satisfied milestone. "
                    "Epoch paused the goal instead of looping; edit the goal or resume to request a different next step.";
                push_editor_log(editor, "[ai] " + editor.aiAuthoringStatus);
                return true;
            }

            if (!sceneChanged && idempotentOnly)
            {
                editor.aiAuthoringPlanApplied = true;
                editor.aiAuthoringSurface = EditorMainSurface::Scene;
                editor.mainSurface = EditorMainSurface::Scene;
                editor.workspaceCompanionsPending = true;
                editor.surfaceSettleFrames = (std::max)(
                    editor.surfaceSettleFrames, 2);
                editor.aiGoalAttemptedMilestoneSignatures.push_back(
                    planSignature);
                editor.aiGoalLastMilestoneSignature = planSignature;
                editor.aiGoalLastMilestoneReceipt = epochengine::format_text(
                    "The proposed exact final count was already satisfied: "
                    "{} command(s), {} object(s) reused, canonical scene revision {} unchanged.",
                    appliedCommands,
                    reusedObjects,
                    editor.sceneDocumentRevision);
                if (editor.aiAuthoringPlanForGoal && editor.aiGoalActive)
                {
                    editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                    editor.aiAuthoringStatus = "The proposed milestone was already satisfied. The goal is paused; explicitly request the next plan to continue.";
                }
                else
                {
                    editor.aiAuthoringStatus =
                        "The requested exact scene counts were already satisfied; no canonical scene revision was created.";
                }
                push_editor_log(editor, "[ai] " + editor.aiAuthoringStatus);
                return true;
            }

            if (guiChanged)
            {
                (void)synchronize_gui_document(editor);
                editor.aiAuthoringSurface = EditorMainSurface::Game2D;
                editor.mainSurface = EditorMainSurface::Game2D;
                editor.outlinerToolTab = OutlinerToolTab::Gui;
                editor.game2DGuiWorkspace = true;
                editor.viewportCameraMode = previewgrid::CameraMode::Canvas2D;
            }
            else
            {
                editor.aiAuthoringSurface = EditorMainSurface::Scene;
                editor.mainSurface = EditorMainSurface::Scene;
            }
            editor.workspaceCompanionsPending = true;
            editor.aiAuthoringPlanApplied = true;
            editor.surfaceSettleFrames = (std::max)(
                editor.surfaceSettleFrames, 2);
            std::string goalReceipt{};
            if (editor.aiAuthoringPlanForGoal && editor.aiGoalActive)
            {
                ++editor.aiGoalCompletedMilestones;
                editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                goalReceipt = epochengine::format_text(
                    " Goal milestone {} is complete; goal is paused; explicitly request the next plan",
                    editor.aiGoalCompletedMilestones);
            }
            if (editor.aiAuthoringPlanForGoal && editor.aiGoalActive)
            {
                editor.aiGoalAttemptedMilestoneSignatures.push_back(
                    planSignature);
            }
            editor.aiGoalLastMilestoneSignature = planSignature;
            editor.aiGoalLastMilestoneReceipt = epochengine::format_text(
                "{} command(s) accepted: {} created, {} removed, {} transformed, "
                "{} reused; canonical scene revision {} -> {}{}.",
                appliedCommands,
                createdObjects,
                removedObjects,
                transformedObjects,
                reusedObjects,
                revisionBeforePlan,
                editor.sceneDocumentRevision,
                sceneChanged
                    ? ""
                    : " (requested final counts were already satisfied)");
            editor.aiAuthoringStatus = epochengine::format_text(
                "Applied {} approved command(s): {} created, {} removed, {} transformed, {} reused{}{}{}.",
                appliedCommands,
                createdObjects,
                removedObjects,
                transformedObjects,
                reusedObjects,
                sceneCleared ? " after clearing the previous scene" : "",
                sceneChanged ? "" : " (canonical targets already satisfied)",
                goalReceipt);
            push_editor_log(
                editor,
                "[ai] " + editor.aiAuthoringStatus);
            return true;
        }
        [[nodiscard]] std::unique_ptr<EditorState> snapshot_ai_authoring(const EditorState& editor)
        {
            auto working = std::make_unique<EditorState>();
            working->projectId = editor.projectId;
            working->projectName = editor.projectName;
            working->projectKind = editor.projectKind;
            working->projectRoot = editor.projectRoot;
            working->projectScenePath = editor.projectScenePath;
            working->sceneDocument = editor.sceneDocument;
            working->sceneDocumentRevision = editor.sceneDocumentRevision;
            working->entities = editor.entities;
            working->selectedEntityId = editor.selectedEntityId;
            working->selectedEntityIds = editor.selectedEntityIds;
            working->selectedEntity = editor.selectedEntity;
            working->timeSnapshot = editor.timeSnapshot;
            working->mainSurface = editor.mainSurface;
            working->aiAuthoringSurface = editor.aiAuthoringSurface;
            working->outlinerToolTab = editor.outlinerToolTab;
            working->game2DGuiWorkspace = editor.game2DGuiWorkspace;
            working->viewportCameraMode = editor.viewportCameraMode;
            working->guiDocument = editor.guiDocument
                ? std::make_unique<authoring::gui::GuiDocument>(*editor.guiDocument) : nullptr;
            working->guiDocumentWidgets = editor.guiDocumentWidgets;
            working->guiDocumentRoot = editor.guiDocumentRoot;
            working->guiProjectionTombstones = editor.guiProjectionTombstones;
            working->guiDocumentStatus = editor.guiDocumentStatus;
            // Worker snapshots never load or save project files. Pure document
            // generation is allowed; persistence remains with the live owner.
            working->guiDocumentLoadAttempted = true;
            working->guiDocumentSourcePath = editor.guiDocumentSourcePath;
            working->guiDocumentSavedRevision = editor.guiDocumentSavedRevision;
            working->guiTemplatePreset = editor.guiTemplatePreset;
            working->aiAuthoringPlan = editor.aiAuthoringPlan;
            working->aiAuthoringRequest = editor.aiAuthoringRequest;
            working->aiAuthoringRequestedGeneration = editor.aiAuthoringRequestedGeneration;
            working->aiAuthoringPlanForGoal = editor.aiAuthoringPlanForGoal;
            working->aiGoal = editor.aiGoal;
            working->aiGoalActive = editor.aiGoalActive;
            working->aiGoalRunning = editor.aiGoalRunning;
            working->aiGoalCompletedMilestones = editor.aiGoalCompletedMilestones;
            working->aiGoalAttemptedMilestoneSignatures = editor.aiGoalAttemptedMilestoneSignatures;
            working->aiGoalLastMilestoneSignature = editor.aiGoalLastMilestoneSignature;
            working->aiGoalLastMilestoneReceipt = editor.aiGoalLastMilestoneReceipt;
            return working;
        }

        [[nodiscard]] bool publish_ai_authoring_snapshot(EditorState& editor, EditorState& working)
        {
            const auto guiRevision = editor.guiDocument ? editor.guiDocument->revision().sequence : 0u;
            if (editor.aiAuthoringApplyCancelled || editor.projectId != working.projectId
                || editor.projectRoot != working.projectRoot
                || editor.projectScenePath != working.projectScenePath
                || editor.sceneDocumentRevision != editor.aiAuthoringApplySceneRevision
                || guiRevision != editor.aiAuthoringApplyGuiRevision
                || editor.aiAuthoringRequestedGeneration != working.aiAuthoringRequestedGeneration
                || ai_authoring_plan_signature(editor.aiAuthoringPlan.plan) != ai_authoring_plan_signature(working.aiAuthoringPlan.plan)
                || editor.aiGoal != working.aiGoal)
            {
                editor.aiAuthoringStatus = "Authoring result discarded: cancelled or its project/scene/GUI/plan changed while the worker ran. No live edits were overwritten. Review and apply a fresh plan.";
                return false;
            }
            editor.aiAuthoringStatus = working.aiAuthoringStatus;
            if (!working.aiAuthoringPlanApplied)
            {
                editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
                return false;
            }
            // One complete publication on the owner thread. Workers never
            // mutate renderer state, editor input, the chat or live documents.
            editor.sceneDocument = std::move(working.sceneDocument);
            editor.sceneDocumentRevision = working.sceneDocumentRevision;
            editor.entities = std::move(working.entities);
            editor.guiDocument = std::move(working.guiDocument);
            editor.guiDocumentWidgets = std::move(working.guiDocumentWidgets);
            editor.guiDocumentRoot = working.guiDocumentRoot;
            editor.guiProjectionTombstones = std::move(working.guiProjectionTombstones);
            editor.guiDocumentSourcePath = std::move(working.guiDocumentSourcePath);
            editor.guiDocumentStatus = std::move(working.guiDocumentStatus);
            editor.selectedEntityId = working.selectedEntityId;
            editor.selectedEntityIds = std::move(working.selectedEntityIds);
            editor.selectedEntity = working.selectedEntity;
            editor.aiAuthoringPlanApplied = true;
            editor.aiAuthoringSurface = editor.mainSurface = working.mainSurface;
            editor.outlinerToolTab = working.outlinerToolTab;
            editor.game2DGuiWorkspace = working.game2DGuiWorkspace;
            editor.workspaceCompanionsPending = true;
            editor.viewportCameraMode = working.viewportCameraMode;
            editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
            editor.aiGoalCompletedMilestones = working.aiGoalCompletedMilestones;
            editor.aiGoalAttemptedMilestoneSignatures = std::move(working.aiGoalAttemptedMilestoneSignatures);
            editor.aiGoalLastMilestoneSignature = std::move(working.aiGoalLastMilestoneSignature);
            editor.aiGoalLastMilestoneReceipt = std::move(working.aiGoalLastMilestoneReceipt);
            editor.sceneInteractionDirty = editor.sceneLightingDirty = true;
            editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 2);
            return true;
        }

        [[nodiscard]] bool queue_ai_authoring_apply(EditorState& editor)
        {
            if (editor.aiAuthoringApplyPending || !editor.aiAuthoringPlan || editor.aiAuthoringPlanApplied) return false;
            try
            {
                auto working = snapshot_ai_authoring(editor);
                editor.aiAuthoringApplySceneRevision = editor.sceneDocumentRevision;
                editor.aiAuthoringApplyGuiRevision = editor.guiDocument ? editor.guiDocument->revision().sequence : 0u;
                editor.aiAuthoringApplyCancelled = false;
                editor.aiAuthoringApplyProgress = std::make_shared<AiAuthoringApplyProgress>();
                working->aiAuthoringApplyProgress = editor.aiAuthoringApplyProgress;
                auto submitted = schedule_editor_cancellable_task(editor, "Approved project authoring batches",
                    [working = std::move(working)](std::stop_token stop) mutable
                    {
                        (void)apply_ai_authoring_plan(*working, false, stop);
                        return std::move(working);
                    });
                editor.aiAuthoringApplyCancellation = submitted.cancellation;
                editor.aiAuthoringApplyPending.emplace(std::move(submitted.completion));
                editor.aiAuthoringStatus = "Approved authoring job queued. Bounded batches run on a private document snapshot; the live scene stays unchanged until the complete result is validated. Cancel remains available.";
                return true;
            }
            catch (const std::exception& error)
            {
                editor.aiAuthoringStatus = "Could not queue approved authoring: " + std::string{error.what()};
                editor.aiAuthoringApplyProgress.reset();
                return false;
            }
        }

        [[nodiscard]] bool poll_ai_authoring_apply(EditorState& editor)
        {
            if (!editor.aiAuthoringApplyPending) return false;
            if (editor.aiAuthoringApplyPending->wait_for(0ms) != std::future_status::ready)
            {
                const auto& progress = editor.aiAuthoringApplyProgress;
                if (progress) editor.aiAuthoringStatus = epochengine::format_text(
                    "Authoring worker: {} commands complete, {} work items in {} batches; next batch {} items, last {} us, peak {} us (8 ms worker target). Live scene unchanged; Cancel is available.",
                    progress->commands.load(std::memory_order_relaxed),
                    progress->objects.load(std::memory_order_relaxed),
                    progress->batches.load(std::memory_order_relaxed),
                    progress->nextBatch.load(std::memory_order_relaxed),
                    progress->lastBatchUs.load(std::memory_order_relaxed),
                    progress->peakBatchUs.load(std::memory_order_relaxed));
                return false;
            }
            try
            {
                auto working = editor.aiAuthoringApplyPending->get();
                if (working && publish_ai_authoring_snapshot(editor, *working))
                {
                    for (const auto& call : editor.aiAuthoringPlan.plan.calls)
                        epochengine::ai::append_tool_trace(epochengine::ai::McpCaptureRecord{
                            .session_id = editor.projectId.empty() ? "editor-authoring" : editor.projectId + "-editor-authoring",
                            .call_id = call.call_id, .server = "editor", .tool = call.tool,
                            .prompt = editor.aiAuthoringPlanForGoal ? editor.aiGoal : editor.aiAuthoringRequest,
                            .normalized_output = "approved batched authoring result published",
                            .source_path = editor.projectScenePath,
                            .state = epochengine::ai::McpCallState::succeeded,
                            .error = epochengine::ai::McpErrorCode::none});
                }
            }
            catch (const std::exception& error)
            {
                editor.aiAuthoringStatus = "Authoring job ended without publication: " + std::string{error.what()};
                editor.aiGoalRunning = editor.aiGoalPlanNextQueued = false;
            }
            editor.aiAuthoringApplyPending.reset();
            editor.aiAuthoringApplyProgress.reset();
            editor.aiAuthoringApplyCancellation = {};
            push_editor_log(editor, "[ai] " + editor.aiAuthoringStatus);
            return true;
        }

