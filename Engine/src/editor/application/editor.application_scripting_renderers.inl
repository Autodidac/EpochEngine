// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::string build_ai_scene_prompt(const EditorState& state)
        {
            if (state.entities.empty())
                return std::string("Summarize the current Epoch editor scene and suggest one useful next step.");

            const std::size_t selectedIndex = (std::min)(state.selectedEntity, state.entities.size() - 1u);
            const auto& entity = state.entities[selectedIndex];
            const std::string positionText = epochengine::format_text(
                "({:.1f}, {:.1f}, {:.1f})",
                entity.position[0],
                entity.position[1],
                entity.position[2]);
            return epochengine::format_text(
                "In Epoch editor, project '{}' has {} entities. Selected entity is '{}' of type '{}' at {}. Suggest one concrete next edit and one gameplay follow-up.",
                state.projectName,
                state.entities.size(),
                entity.name,
                entity.type,
                positionText);
        }

        void handle_scene_tool(
            EditorState& state,
            const core::Context* ctx,
            std::string_view toolId)
        {
            if (toolId == "focus_selection")
            {
                if (!editor_has_selection(state) || !ctx)
                {
                    push_editor_log(state, "[scene] Nothing selected to focus.");
                    return;
                }

                if (!ensure_editor_scene_interaction(state))
                {
                    push_editor_log(state, "[scene] Selection bounds are invalid; Focus was not applied.");
                    return;
                }

                const auto focus = state.sceneInteraction.focus_target(
                    scene_interaction::SceneObjectId{ state.selectedEntityId, 1u });
                const auto index = editor_entity_index(state, state.selectedEntityId);
                if (!focus || !index)
                {
                    push_editor_log(state, "[scene] Selected object no longer resolves; Focus was not applied.");
                    return;
                }

                const EditorEntity& entity = state.entities[*index];
                bool focused = epochengine::previewgrid::focus_camera(
                    ctx,
                    { focus->target.x, focus->target.y, focus->target.z },
                    focus->radius);
#if defined(_WIN32)
                if (auto* manager = core::GetActiveMultiContextManager())
                {
                    for (const auto& window : manager->GetWindows())
                    {
                        if (!window || !window->context || window->context.get() == ctx)
                            continue;
                        focused = epochengine::previewgrid::focus_camera(
                            window->context.get(),
                            { focus->target.x, focus->target.y, focus->target.z },
                            focus->radius) || focused;
                    }
                }
#endif
                push_editor_log(
                    state,
                    focused
                        ? std::string("[scene] Focused ") + entity.name + "."
                        : std::string("[scene] Could not focus ") + entity.name + ".");
                return;
            }

            if (toolId == "reset_camera")
            {
                std::vector<authoring::scene::SceneOperation> operations{};
                for (const auto& entity : state.entities)
                {
                    if (entity.type != "Camera")
                        continue;
                    const auto handle = state.sceneDocument.find(
                        authoring::scene::ObjectId{entity.sceneObjectId});
                    if (!handle)
                        continue;
                    operations.emplace_back(
                        authoring::scene::ObjectTransformChangedOperation{
                            .object = *handle,
                            .value = {
                                .position = {0.0f, 1.5f, 5.0f},
                                .rotation_degrees = {0.0f, 0.0f, 0.0f},
                                .scale = {
                                    entity.scale[0],
                                    entity.scale[1],
                                    entity.scale[2]}
                            }
                        });
                }
                const bool committed = operations.empty() ||
                    apply_editor_scene_transaction(state, operations, "Reset camera rigs");
                if (committed && ctx)
                    epochengine::previewgrid::reset_camera(ctx);
                push_editor_log(
                    state,
                    committed
                        ? "[scene] Camera rigs reset."
                        : "[scene] Camera reset was rejected.");
                return;
            }

            if (toolId == "toggle_helpers")
            {
                const bool nextVisibility = !state.helpersVisible;
                std::vector<authoring::scene::SceneOperation> operations{};
                for (const auto& entity : state.entities)
                {
                    if (!entity.editorOnly && entity.category != "Editor")
                        continue;
                    const auto handle = state.sceneDocument.find(
                        authoring::scene::ObjectId{entity.sceneObjectId});
                    if (!handle)
                        continue;
                    operations.emplace_back(
                        authoring::scene::ObjectVisibilityChangedOperation{
                            .object = *handle,
                            .runtime_visible = nextVisibility && !entity.editorOnly,
                            .editor_visible = nextVisibility,
                            .editor_only = entity.editorOnly
                        });
                }
                const bool committed = operations.empty() ||
                    apply_editor_scene_transaction(state, operations, "Toggle editor helpers");
                if (committed)
                    state.helpersVisible = nextVisibility;
                push_editor_log(
                    state,
                    committed
                        ? std::string("[scene] Helpers ") +
                            (state.helpersVisible ? "shown." : "hidden.")
                        : "[scene] Helper visibility change was rejected.");
                return;
            }
        }

        std::size_t rotate_script_target_entities(EditorState& state, float deltaDegrees)
        {
            std::vector<authoring::scene::SceneOperation> operations{};
            for (const auto& entity : state.entities)
            {
                if (entity.editorOnly || entity.type == "Level" || entity.type == "Camera")
                    continue;

                const auto handle = state.sceneDocument.find(
                    authoring::scene::ObjectId{entity.sceneObjectId});
                if (!handle)
                    continue;
                float yaw = entity.rotation[1] + deltaDegrees;
                if (yaw > 180.0f)
                    yaw -= 360.0f;
                if (yaw < -180.0f)
                    yaw += 360.0f;
                operations.emplace_back(
                    authoring::scene::ObjectTransformChangedOperation{
                        .object = *handle,
                        .value = {
                            .position = {
                                entity.position[0],
                                entity.position[1],
                                entity.position[2]},
                            .rotation_degrees = {
                                entity.rotation[0],
                                yaw,
                                entity.rotation[2]},
                            .scale = {
                                entity.scale[0],
                                entity.scale[1],
                                entity.scale[2]}
                        }
                    });
            }
            if (operations.empty())
                return 0u;
            return apply_editor_scene_transaction(
                state,
                operations,
                "Script rotate scene objects")
                ? operations.size()
                : 0u;
        }

        [[nodiscard]] std::string editor_tooling_state_summary(const EditorState& state)
        {
            std::size_t mutableEntities = 0;
            float yawSum = 0.0f;
            std::string firstMutable = "(none)";
            for (const auto& entity : state.entities)
            {
                if (entity.editorOnly || entity.type == "Level" || entity.type == "Camera")
                    continue;

                if (mutableEntities == 0)
                    firstMutable = entity.name + " yaw=" + epochengine::format_text("{:.1f}", entity.rotation[1]);
                yawSum += entity.rotation[1];
                ++mutableEntities;
            }

            return epochengine::format_text(
                "project={} script={} entities={} mutable={} first_mutable={} yaw_sum={:.1f}",
                state.projectId,
                state.activeScript,
                state.entities.size(),
                mutableEntities,
                firstMutable,
                yawSum);
        }

        void script_log_callback(void* userData, const char* message)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx)
                return;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return;

            push_editor_log(it->second, std::string("[script] ") + (message ? message : "(null)"));
        }

        void script_rotate_all_entities_yaw_callback(void* userData, float deltaDegrees)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx)
                return;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return;

            const std::size_t rotated = rotate_script_target_entities(it->second, deltaDegrees);
            push_editor_log(
                it->second,
                epochengine::format_text("[script] Rotated {} scene entities by {:.1f} degrees.", rotated, deltaDegrees));
        }

        [[nodiscard]] bool attach_script_cube_oscillators(EditorState& state, unsigned count,
            int axis, float amplitude, float frequency)
        {
            if (axis < 0 || axis > 2 || !std::isfinite(amplitude) || !std::isfinite(frequency)
                || amplitude < 0.01f || amplitude > 100.0f || frequency < 0.01f || frequency > 20.0f)
                return false;
            const auto available = std::ranges::count_if(state.entities, [](const auto& entity)
                { return entity.type == "StaticMesh" && !entity.editorOnly; });
            if (count > static_cast<std::size_t>(available)) return false;
            std::unordered_map<scene::SceneObjectId, ScriptOscillator> motions{};
            unsigned attached{};
            for (const auto& entity : state.entities)
            {
                if (entity.type != "StaticMesh" || entity.editorOnly || attached == count) continue;
                motions.emplace(entity.sceneObjectId, ScriptOscillator{
                    .enabled = true, .entity_id = entity.sceneObjectId, .base_position = entity.position,
                    .axis = static_cast<std::uint8_t>(axis), .amplitude = amplitude, .frequency_hz = frequency,
                    .phase_radians = 6.283185307179586 * attached / count});
                ++attached;
            }
            state.scriptCubeOscillators = std::move(motions);
            return true;
        }

        int script_attach_cube_oscillators_callback(void* userData, unsigned count,
            int axis, float amplitude, float frequency)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx) return -1;
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.states.find(ctx);
            if (found == storage.states.end()) return -1;
            const bool accepted = attach_script_cube_oscillators(found->second, count, axis, amplitude, frequency);
            push_editor_log(found->second, accepted
                ? epochengine::format_text("[script] Attached motion to {} existing cubes. Press Play to animate.", count)
                : "[script] Cube motion rejected: count or parameters exceed the active scene.");
            return accepted ? 0 : -1;
        }

        int script_arrange_cubes_callback(void* userData, unsigned columns,
            float spacingX, float spacingZ, float originX, float originY, float originZ)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx) return -1;
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.states.find(ctx);
            if (found == storage.states.end()) return -1;
            const ai::McpToolCall call{.tool = "scene.arrange", .arguments = {
                {"archetype", "cube"}, {"columns", std::to_string(columns)},
                {"spacing", epochengine::format_text("{},0,{}", spacingX, spacingZ)},
                {"origin", epochengine::format_text("{},{},{}", originX, originY, originZ)},
                {"placement", "support"}}};
            std::string diagnostic{};
            std::size_t changed{};
            if (!ai::authoring_plan_detail::validate_call(call, diagnostic)
                || !arrange_ai_scene_objects(found->second, call, changed, diagnostic))
            {
                push_editor_log(found->second, "[script] Cube arrangement rejected: " + diagnostic);
                return -1;
            }
            push_editor_log(found->second, epochengine::format_text("[script] Arranged {} existing cubes.", changed));
            return 0;
        }

        int script_attach_selected_entity_oscillator_callback(
            void* userData,
            int axis,
            float amplitude,
            float frequencyHz)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx || axis < 0 || axis > 2
                || !std::isfinite(amplitude) || !std::isfinite(frequencyHz))
            {
                return -1;
            }

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return -1;

            auto& state = it->second;
            const auto selected = editor_entity_index(state, state.selectedEntityId);
            if (!selected)
            {
                push_editor_log(state, "[script] Oscillator attach failed: select a scene entity first.");
                return -1;
            }

            const auto& entity = state.entities[*selected];
            if (entity.editorOnly || entity.type == "Level" || entity.type == "Camera")
            {
                push_editor_log(state, "[script] Oscillator attach failed: select a movable scene object such as a Cube.");
                return -1;
            }

            state.scriptOscillator.enabled = true;
            state.scriptCubeOscillators.erase(entity.sceneObjectId);
            state.scriptOscillator.entity_id = entity.sceneObjectId;
            state.scriptOscillator.base_position = entity.position;
            state.scriptOscillator.axis = static_cast<std::uint8_t>(axis);
            state.scriptOscillator.amplitude = (std::clamp)(std::abs(amplitude), 0.01f, 100.0f);
            state.scriptOscillator.frequency_hz = (std::clamp)(std::abs(frequencyHz), 0.01f, 20.0f);

            static constexpr std::array<std::string_view, 3> axisNames{ "X", "Y", "Z" };
            push_editor_log(
                state,
                epochengine::format_text(
                    "[script] Attached oscillator to '{}' on {}: +/-{:.2f} units at {:.2f} Hz. Press Play to animate.",
                    entity.name,
                    axisNames[state.scriptOscillator.axis],
                    state.scriptOscillator.amplitude,
                    state.scriptOscillator.frequency_hz));
            return 0;
        }

        int script_queue_model_load_callback(void* userData, const char* debugName, const char* modelPath)
        {
            auto* ctx = static_cast<core::Context*>(userData);
            if (!ctx || !modelPath || modelPath[0] == '\0')
                return -1;
            if (!ctx->add_model)
                return -1;

            const std::string path{ modelPath };
            const std::string label =
                (debugName && debugName[0] != '\0')
                ? std::string(debugName)
                : std::filesystem::path{ path }.stem().string();

            const auto invokeLoad = [ctx, label, path]() noexcept
                {
                    return ctx->add_model_safe(label.c_str(), path.c_str());
                };

            if (auto current = core::get_current_render_context(); current && current.get() == ctx)
                return invokeLoad();

            if (!ctx->windowData)
                return -1;

            const core::RenderPath renderPath =
                (ctx->type == core::ContextType::OpenGL) ? core::RenderPath::OpenGL
                : (ctx->type == core::ContextType::SFML) ? core::RenderPath::SFML
                : (ctx->type == core::ContextType::Vulkan) ? core::RenderPath::Vulkan
                : (ctx->type == core::ContextType::DirectX) ? core::RenderPath::DirectX
                : core::RenderPath::Unknown;

            ctx->windowData->commandQueue.enqueue(
                [ctx, label, path]()
                {
                    (void)ctx->add_model_safe(label.c_str(), path.c_str());
                },
                renderPath);
            return 1;
        }

        [[nodiscard]] bool is_engine_arcade_scene_id(std::string_view sceneId) noexcept
        {
            constexpr std::array<std::string_view, 11> kSceneIds{{
                "snake",
                "tetris",
                "pacman",
                "frogger",
                "sokoban",
                "bejeweled",
                "match3",
                "puzzle",
                "sliding",
                "minesweep",
                "minesweeper"
            }};

            if (sceneId == "fourty" || sceneId == "2048" || sceneId == "sandsim"
                || sceneId == "sand" || sceneId == "cellular" || sceneId == "cell")
            {
                return true;
            }

            return std::find(kSceneIds.begin(), kSceneIds.end(), sceneId) != kSceneIds.end();
        }

        int script_request_engine_scene_callback(void* userData, const char* sceneId)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx || !sceneId || sceneId[0] == '\0')
                return -1;

            const std::string requestedScene{ sceneId };
            if (!is_engine_arcade_scene_id(requestedScene))
                return -1;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return -1;

            it->second.activeRuntimeScene = requestedScene;
            it->second.projectStatus = std::string("Script selected built-in engine scene '") + requestedScene + "' for the Run button.";
            push_editor_log(
                it->second,
                std::string("[script] Built-in engine scene selected: ") + requestedScene + ".");
            push_editor_log(
                it->second,
                "[script] Press the centered Run button to launch it inside the engine runtime.");
            return 1;
        }

        [[nodiscard]] std::string renderer_name(core::ContextType type)
        {
            switch (type)
            {
            case core::ContextType::OpenGL: return "OpenGL";
            case core::ContextType::Vulkan: return "Vulkan";
            case core::ContextType::RayLib: return "Raylib";
            case core::ContextType::SDL: return "SDL";
            case core::ContextType::SFML: return "SFML";
            case core::ContextType::DirectX: return "DirectX";
            case core::ContextType::Software: return "Software";
            default: return "Unknown";
            }
        }

        [[nodiscard]] std::string renderer_name(const std::shared_ptr<core::Context>& ctx)
        {
            return ctx ? renderer_name(ctx->type) : std::string{ "Unknown" };
        }

        [[nodiscard]] bool is_context_driver_candidate(core::ContextType type) noexcept
        {
            switch (type)
            {
            case core::ContextType::OpenGL:
            case core::ContextType::SDL:
            case core::ContextType::SFML:
            case core::ContextType::RayLib:
            case core::ContextType::Vulkan:
            case core::ContextType::DirectX:
            case core::ContextType::Software:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] bool editor_context_replacement_in_progress() noexcept
        {
#if defined(_WIN32)
            if (const auto* manager = core::GetActiveMultiContextManager())
                return manager->ContextReplacementInProgress();
#endif
            return false;
        }

        [[nodiscard]] int context_driver_sort_key(core::ContextType type) noexcept
        {
            switch (type)
            {
            case core::ContextType::DirectX: return 0;
            case core::ContextType::OpenGL: return 1;
            case core::ContextType::SDL: return 2;
            case core::ContextType::SFML: return 3;
            case core::ContextType::RayLib: return 4;
            case core::ContextType::Vulkan: return 5;
            case core::ContextType::Software: return 6;
            default: return 100;
            }
        }

        struct ContextBackendOption
        {
            core::ContextType type{ core::ContextType::None };
            std::string label{};
            bool compiled{};
            bool selectable{};
            std::string unavailable_reason{};
        };

        [[nodiscard]] std::vector<ContextBackendOption> available_context_backend_options()
        {
            core::InitializeAllContexts();

            static constexpr std::array kKnownBackends{
                core::ContextType::DirectX,
                core::ContextType::OpenGL,
                core::ContextType::SDL,
                core::ContextType::SFML,
                core::ContextType::RayLib,
                core::ContextType::Vulkan,
                core::ContextType::Software
            };

            std::vector<ContextBackendOption> options;
            options.reserve(kKnownBackends.size());
            {
                std::shared_lock lock(core::g_backendsMutex);
                for (const auto type : kKnownBackends)
                {
                    const auto backend = core::g_backends.find(type);
                    const bool compiled = backend != core::g_backends.end() && backend->second.master;
#if defined(_WIN32)
                    const bool hostSupportsReplacement = true;
#else
                    const bool hostSupportsReplacement = false;
#endif

                    options.push_back(ContextBackendOption{
                        .type = type,
                        .label = renderer_name(type),
                        .compiled = compiled,
                        .selectable = compiled && hostSupportsReplacement,
                        .unavailable_reason = !compiled
                            ? std::string{ "not compiled" }
                            : (!hostSupportsReplacement
                                ? std::string{ "whole-editor replacement is unavailable on this host" }
                                : std::string{})
                    });
                }
            }

            std::sort(options.begin(), options.end(), [](const auto& a, const auto& b)
            {
                const int ar = context_driver_sort_key(a.type);
                const int br = context_driver_sort_key(b.type);
                if (ar != br)
                    return ar < br;
                return a.label < b.label;
            });
            return options;
        }

        [[nodiscard]] std::string context_backend_summary(const std::vector<ContextBackendOption>& options)
        {
            if (options.empty())
                return "none registered";

            std::string summary;
            for (const auto& option : options)
            {
                if (!option.compiled)
                    continue;
                if (!summary.empty())
                    summary += " | ";
                summary += option.label;
            }
            return summary.empty() ? std::string{ "none registered" } : summary;
        }

        [[nodiscard]] std::string disabled_context_backend_summary(
            const std::vector<ContextBackendOption>& options)
        {
            std::string summary;
            for (const auto& option : options)
            {
                if (option.selectable)
                    continue;
                if (!summary.empty())
                    summary += " | ";
                summary += option.label;
                summary += " (";
                summary += option.unavailable_reason.empty()
                    ? std::string{ "unavailable" }
                    : option.unavailable_reason;
                summary += ")";
            }
            return summary.empty() ? std::string{ "none" } : summary;
        }

        struct PassiveContextObservation
        {
            std::size_t liveWindowCount{ 0 };
            std::size_t routedPanelCount{ 0 };
            bool currentContextLive{ false };
            bool currentContextRouted{ false };
        };

        [[nodiscard]] epochengine::context::PassiveContextBackend passive_context_backend(core::ContextType type) noexcept
        {
            switch (type)
            {
            case core::ContextType::OpenGL: return epochengine::context::PassiveContextBackend::OpenGL;
            case core::ContextType::SDL: return epochengine::context::PassiveContextBackend::SDL;
            case core::ContextType::SFML: return epochengine::context::PassiveContextBackend::SFML;
            case core::ContextType::RayLib: return epochengine::context::PassiveContextBackend::RayLib;
            case core::ContextType::Vulkan: return epochengine::context::PassiveContextBackend::Vulkan;
            case core::ContextType::DirectX: return epochengine::context::PassiveContextBackend::DirectX;
            case core::ContextType::Software: return epochengine::context::PassiveContextBackend::Software;
            case core::ContextType::Noop: return epochengine::context::PassiveContextBackend::Noop;
            case core::ContextType::Custom: return epochengine::context::PassiveContextBackend::Custom;
            default: return epochengine::context::PassiveContextBackend::None;
            }
        }

        [[nodiscard]] PassiveContextObservation passive_context_observation_for(const core::Context* activeContext)
        {
            PassiveContextObservation observation{};
            std::shared_lock lock(core::g_backendsMutex);
            for (const auto& [_, backend] : core::g_backends)
            {
                const auto visit = [&](const std::shared_ptr<core::Context>& candidate)
                {
                    if (!candidate || !candidate->windowData || !candidate->windowData->running)
                        return;

                    ++observation.liveWindowCount;
                    if (!candidate->windowData->guiRoute.empty() || candidate->windowData->isFloating)
                        ++observation.routedPanelCount;

                    if (candidate.get() == activeContext)
                    {
                        observation.currentContextLive = true;
                        observation.currentContextRouted =
                            !candidate->windowData->guiRoute.empty() || candidate->windowData->isFloating;
                    }
                };

                visit(backend.master);
                for (const auto& duplicate : backend.duplicates)
                    visit(duplicate);
            }
            return observation;
        }

        [[nodiscard]] epochengine::context::PassiveSignalStatus passive_stability_status(
            double frameMs,
            double jitterMs,
            const EditorTimeSnapshot& snapshot) noexcept
        {
            if (!std::isfinite(frameMs) || frameMs <= 0.0)
                return epochengine::context::PassiveSignalStatus::NotSampled;

            const double targetMs = snapshot.fixed_dt_seconds > 0.0
                ? snapshot.fixed_dt_seconds * 1000.0
                : 16.6667;

            if (frameMs <= targetMs * 1.25 && jitterMs <= 4.0)
                return epochengine::context::PassiveSignalStatus::Healthy;
            if (frameMs <= targetMs * 2.0 && jitterMs <= 12.0)
                return epochengine::context::PassiveSignalStatus::Watch;
            return epochengine::context::PassiveSignalStatus::Unstable;
        }

        [[nodiscard]] std::string passive_context_recommendation_text(
            const epochengine::context::PassiveContextScoreboard& scoreboard)
        {
            const auto recommendation = scoreboard.best_recommendation();
            if (!recommendation.available)
            {
                return epochengine::format_text(
                    "No recommendation yet ({} accepted / {} rejected samples).",
                    scoreboard.accepted_sample_count(),
                    scoreboard.rejected_sample_count());
            }

            return epochengine::format_text(
                "{} recommended for editor default: avg {:.1f}, recent {:.1f}, {} samples ({} accepted / {} rejected).",
                std::string{ epochengine::context::backend_name(recommendation.backend) },
                recommendation.average_score,
                recommendation.recent_score,
                recommendation.sample_count,
                scoreboard.accepted_sample_count(),
                scoreboard.rejected_sample_count());
        }

        void update_passive_context_scoring(
            EditorStorage& storage,
            EditorState& editor,
            const core::Context* ctx,
            const EditorTimeSnapshot& snapshot,
            const PassiveContextObservation& observation)
        {
            if (!ctx)
                return;

            const bool validFrameTime = std::isfinite(snapshot.real_dt_seconds)
                && snapshot.real_dt_seconds > 0.0;
            const double frameMs = validFrameTime ? snapshot.real_dt_seconds * 1000.0 : 0.0;
            const double jitterMs = editor.passiveContextHasLastFrame
                ? std::fabs(frameMs - editor.passiveContextLastFrameMs)
                : 0.0;
            if (validFrameTime)
            {
                editor.passiveContextLastFrameMs = frameMs;
                editor.passiveContextHasLastFrame = true;
            }

            auto kind = epochengine::context::PassiveContextSampleKind::SingleContextBackend;
            if (!observation.currentContextLive || !validFrameTime)
                kind = epochengine::context::PassiveContextSampleKind::Unknown;
            else if (observation.currentContextRouted)
                kind = epochengine::context::PassiveContextSampleKind::DiagnosticGrid;
            else if (observation.liveWindowCount != 1U || observation.routedPanelCount != 0U)
                kind = epochengine::context::PassiveContextSampleKind::MultiContextBackend;

            const epochengine::context::PassiveContextEvidence evidence{
                .source = epochengine::context::PassiveEvidenceSource::PassiveHostObservation,
                .fps = validFrameTime ? std::optional<double>{ 1.0 / snapshot.real_dt_seconds } : std::optional<double>{},
                .frame_time_ms = validFrameTime ? std::optional<double>{ frameMs } : std::optional<double>{},
                .frame_time_jitter_ms = validFrameTime ? std::optional<double>{ jitterMs } : std::optional<double>{},
                .stability = passive_stability_status(frameMs, jitterMs, snapshot),
                .input_latency_ms = {},
                .input_latency_status = epochengine::context::PassiveSignalStatus::NotSampled,
                .runtime_launched = false
            };
            const epochengine::context::PassiveContextSample sample{
                .backend = passive_context_backend(ctx->type),
                .kind = kind,
                .evidence = evidence,
                .label = "editor.passive-context"
            };
            const auto score = epochengine::context::score_passive_context_sample(sample);
            (void)storage.passiveContextScores.record(score);
            editor.passiveContextRecommendation = passive_context_recommendation_text(storage.passiveContextScores);

            if (score.accepted)
            {
                const auto summary = storage.passiveContextScores.summary_for(score.backend);
                editor.passiveContextScoreStatus = epochengine::format_text(
                    "{} single-context score {:.1f} (avg {:.1f}, recent {:.1f}, {} samples, {:.1f} FPS, {:.2f} ms jitter).",
                    renderer_name(ctx->type),
                    score.score,
                    summary.average_score,
                    summary.recent_score,
                    summary.sample_count,
                    evidence.fps.value_or(0.0),
                    evidence.frame_time_jitter_ms.value_or(0.0));
            }
            else
            {
                editor.passiveContextScoreStatus = epochengine::format_text(
                    "Paused: {} ({} live windows, {} routed/floating panels).",
                    std::string{ epochengine::context::rejection_name(score.rejection) },
                    observation.liveWindowCount,
                    observation.routedPanelCount);
            }
        }

        [[nodiscard]] epochengine::RendererBackendKind renderer_backend_kind(const std::shared_ptr<core::Context>& ctx) noexcept
        {
            if (!ctx)
                return epochengine::RendererBackendKind::null;

            switch (ctx->type)
            {
            case core::ContextType::OpenGL: return epochengine::RendererBackendKind::opengl;
            case core::ContextType::SDL: return epochengine::RendererBackendKind::sdl3;
            case core::ContextType::SFML: return epochengine::RendererBackendKind::sfml3;
            case core::ContextType::RayLib: return epochengine::RendererBackendKind::raylib3;
            case core::ContextType::Vulkan: return epochengine::RendererBackendKind::vulkan;
            case core::ContextType::DirectX: return epochengine::RendererBackendKind::directx;
            case core::ContextType::Software: return epochengine::RendererBackendKind::software;
            default: return epochengine::RendererBackendKind::null;
            }
        }

        [[nodiscard]] constexpr epochengine::RendererBackendKind renderer_backend_kind(
            const std::string_view argument) noexcept
        {
            if (argument == "directx") return epochengine::RendererBackendKind::directx;
            if (argument == "vulkan") return epochengine::RendererBackendKind::vulkan;
            if (argument == "raylib") return epochengine::RendererBackendKind::raylib3;
            if (argument == "sdl") return epochengine::RendererBackendKind::sdl3;
            if (argument == "sfml") return epochengine::RendererBackendKind::sfml3;
            if (argument == "software") return epochengine::RendererBackendKind::software;
            if (argument == "headless") return epochengine::RendererBackendKind::null;
            return epochengine::RendererBackendKind::opengl;
        }

        [[nodiscard]] epochengine::capability::Profile renderer_capability_profile(
            const std::shared_ptr<core::Context>& ctx) noexcept
        {
            return epochengine::capability::renderer_profile_for(
                renderer_backend_kind(ctx));
        }

        [[nodiscard]] const EditorProjectProfile& active_project_profile(
            const EditorState& editor) noexcept
        {
            if (const auto* profile = editor_find_project_profile(editor.projectId))
                return *profile;
            return editor_default_project_profile();
        }

        [[nodiscard]] bool has_open_project(
            const EditorState& editor) noexcept
        {
            return !editor.projectId.empty()
                && editor.projectLifecycle.evidence().project_selected;
        }

        [[nodiscard]] bool open_project_manifest_from_dialog(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx)
        {
            std::filesystem::path initialDirectory{};
            if (!editor.projectRoot.empty())
                initialDirectory =
                    resolve_editor_path(editor.projectRoot).parent_path();
            if (initialDirectory.empty())
            {
                std::error_code ignored{};
                initialDirectory = std::filesystem::current_path(ignored);
            }

            const auto selected =
                platform::filesystem::open_file_dialog(
                    L"Open Epoch Project",
                    L"Epoch project manifest",
                    L"project.epoch.json",
                    initialDirectory);
            if (!selected.accepted)
            {
                editor.projectStatus = selected.diagnostic;
                if (!selected.cancelled)
                {
                    push_editor_log(
                        editor,
                        "[project] " + editor.projectStatus);
                }
                return false;
            }

            const auto admitted = editor_admit_project_manifest(
                selected.path.generic_string());
            editor.projectStatus = admitted.summary;
            if (!admitted.succeeded)
            {
                push_editor_log(
                    editor,
                    "[project] " + editor.projectStatus);
                return false;
            }
            const EditorApplicationKind requestedApplication =
                editor.applicationKind;
            if (!editor_open_project(ctx, admitted.project_id))
            {
                editor.projectStatus =
                    "The project manifest was admitted, but its authoring session could not be opened.";
                push_editor_log(
                    editor,
                    "[project] " + editor.projectStatus);
                return false;
            }

            if (requestedApplication
                == EditorApplicationKind::GuiEditor)
            {
                apply_application_profile(
                    editor,
                    gui_editor_application());
                epochengine::previewgrid::set_camera_mode(
                    ctx.get(),
                    editor.viewportCameraMode);
            }

            editor.projectPickerId = admitted.project_id;
            editor.projectStatus =
                "Opened project from " + admitted.manifest_path + ".";
            push_editor_log(
                editor,
                "[project] " + editor.projectStatus);
            return true;
        }

        [[nodiscard]] epochengine::capability::RequirementAdmission renderer_admission(
            const epochengine::RendererBackendKind kind,
            const EditorProjectCapabilityPolicy& policy) noexcept
        {
            return epochengine::capability::assess_requirement(
                epochengine::capability::renderer_subsystem_profile_for(kind),
                policy.renderer_requirement,
                policy.allow_experimental,
                policy.allow_software_fallback);
        }

        [[nodiscard]] std::string renderer_admission_summary(
            const epochengine::RendererBackendKind kind,
            const EditorProjectCapabilityPolicy& policy)
        {
            const auto candidate =
                epochengine::capability::renderer_subsystem_profile_for(kind);
            const auto admission = renderer_admission(kind, policy);
            std::string summary = epochengine::format_text(
                "{} | {} {}-{}",
                policy.id,
                epochengine::capability::admission_status_label(admission.status),
                epochengine::capability::tier_code(candidate.capability.tier),
                epochengine::capability::backend_code(candidate.capability.backend));
            if (admission.status == epochengine::capability::AdmissionStatus::rejected)
            {
                summary += " | ";
                summary += epochengine::capability::match_failure_label(admission.failure);
            }
            return summary;
        }

        [[nodiscard]] std::string renderer_budget_summary(
            const epochengine::RendererBackendKind kind)
        {
            const auto budgets = epochengine::capability::renderer_profile_for(kind)
                .recommended_budgets;
            return epochengine::format_text(
                "{}x{} | CPU {:.1f} ms | GPU {:.1f} ms | {} MiB local",
                budgets.max_w,
                budgets.max_h,
                budgets.cpu_ms,
                budgets.gpu_ms,
                budgets.vram_budget_bytes / (1024ull * 1024ull));
        }

        [[nodiscard]] std::string renderer_capability_profile_summary(
            const std::shared_ptr<core::Context>& ctx)
        {
            const auto profile = renderer_capability_profile(ctx);
            const auto report = epochengine::renderer_capability_report_for(
                renderer_backend_kind(ctx));
            return epochengine::format_text(
                "{}-{} | profile {} | presentation {}",
                epochengine::capability::tier_code(profile.tier),
                epochengine::capability::backend_code(profile.backend),
                epochengine::renderer_capability_status_label(profile.status),
                epochengine::renderer_capability_status_label(report.presentation_proof));
        }

        [[nodiscard]] std::string renderer_resource_spine_summary(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            const auto caps = epochengine::renderer_capabilities_for(kind);

            if (kind == epochengine::RendererBackendKind::software)
                return "Software remains debug/safe-launch fallback; production renderer parity excludes it.";

            std::string summary;
            const auto append = [&summary](bool enabled, std::string_view name)
            {
                if (!enabled)
                    return;

                if (!summary.empty())
                    summary += " | ";
                summary += name;
            };

            append(caps.buffers, "buffers");
            append(caps.textures, "textures");
            append(caps.samplers, "samplers");
            append(caps.shaders, "shaders");
            append(caps.pipelines, "pipelines");
            append(caps.materials, "materials");
            append(caps.render_targets, "targets");
            append(caps.command_lists, "commands");
            append(caps.frame_graph, "graph");
            append(caps.render_to_texture, "RTT");
            append(caps.sampled_render_targets, "sampled RTT contract");
            append(caps.binding_sets, "binding sets");
            append(caps.mesh_resources, "meshes");
            append(caps.model_resources, "models");

            if (summary.empty())
                return "No renderer-resource spine is active for this backend yet.";

            return summary;
        }

        [[nodiscard]] std::string renderer_declared_descriptor_status()
        {
            return "buffers | textures | materials | targets | command lists | mesh descriptors | model descriptors";
        }

        [[nodiscard]] std::string renderer_capability_proof_stage_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            const auto report = epochengine::renderer_capability_report_for(kind);

            return epochengine::format_text(
                "desc {} | graph {} | hook {} | live {} | presentation {} | sampled {} | scene surface {}",
                epochengine::renderer_capability_status_label(report.descriptor_contract),
                epochengine::renderer_capability_status_label(report.build_graph_proof),
                epochengine::renderer_capability_status_label(report.hook_readiness),
                epochengine::renderer_capability_status_label(report.live_native_allocation),
                epochengine::renderer_capability_status_label(report.presentation_proof),
                epochengine::renderer_capability_status_label(report.sampled_render_targets),
                epochengine::renderer_capability_status_label(report.scene_sampled_surface));
        }

        [[nodiscard]] std::string renderer_sampled_rtt_descriptor_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.descriptor_contract);
        }

        [[nodiscard]] std::string renderer_sampled_rtt_graph_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.build_graph_proof);
        }

        [[nodiscard]] std::string renderer_sampled_rtt_hook_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.hook_readiness);
        }

        [[nodiscard]] std::string renderer_sampled_rtt_live_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.live_native_allocation);
        }

        [[nodiscard]] std::string renderer_sampled_rtt_presentation_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.presentation_proof);
        }

        [[nodiscard]] std::string renderer_sampled_rtt_rollup_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.sampled_render_targets);
        }
        [[nodiscard]] std::string renderer_scene_sampled_surface_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto report = epochengine::renderer_capability_report_for(renderer_backend_kind(ctx));
            return epochengine::renderer_capability_status_label(report.scene_sampled_surface);
        }


        [[nodiscard]] std::string renderer_native_mesh_model_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            if (kind == epochengine::RendererBackendKind::software)
                return "debug fallback; production native mesh/model allocation is out of scope";

            const auto caps = epochengine::renderer_capabilities_for(kind);
            const auto report = epochengine::renderer_capability_report_for(kind);
            const bool meshNative = epochengine::renderer_supports_mesh_resources(caps);
            const bool modelNative = epochengine::renderer_supports_model_resources(caps);
            if (meshNative && modelNative)
                return "mesh/model native allocation active";
            if (meshNative)
                return "mesh native, model allocation pending";
            if (modelNative)
                return "model native, mesh allocation pending";
            if (report.mesh_model_resources == epochengine::RendererCapabilityStatus::partial)
                return "Partial: descriptors/graph path exists; backend-native mesh/model allocation pending";
            return "descriptors compile through graph; backend-native allocation pending";
        }

        [[nodiscard]] std::string renderer_native_sampled_rtt_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            const auto caps = epochengine::renderer_capabilities_for(kind);
            const auto report = epochengine::renderer_capability_report_for(kind);
            const std::string status = epochengine::renderer_capability_status_label(report.sampled_render_targets);
            const std::string sceneStatus = epochengine::renderer_capability_status_label(report.scene_sampled_surface);
            const std::string liveStatus = epochengine::renderer_capability_status_label(report.live_native_allocation);
            const std::string presentationStatus = epochengine::renderer_capability_status_label(report.presentation_proof);
            if (kind == epochengine::RendererBackendKind::software)
                return sceneStatus + ": bounded CPU sampled scene surface; generic device-spine promotion deferred";

            if (epochengine::renderer_supports_native_sampled_render_targets(caps))
                return status + ": native sampled RTT allocation active; live " + liveStatus +
                    " | presentation " + presentationStatus;
            if (epochengine::renderer_supports_sampled_render_targets(caps))
            {
                switch (kind)
                {
                case epochengine::RendererBackendKind::opengl:
                case epochengine::RendererBackendKind::sdl3:
                    return sceneStatus + ": context sampled surface active; device spine " + status +
                        " | presentation " + presentationStatus;
                case epochengine::RendererBackendKind::sfml3:
                case epochengine::RendererBackendKind::raylib3:
                    return sceneStatus + ": context-native sampled surface active; device hooks " +
                        epochengine::renderer_capability_status_label(report.hook_readiness);
                case epochengine::RendererBackendKind::vulkan:
                case epochengine::RendererBackendKind::directx:
                    return sceneStatus + ": context-native sampled surface active; device graph " +
                        epochengine::renderer_capability_status_label(report.build_graph_proof);
                default:
                    return sceneStatus + ": sampled scene surface path active; device spine " + status;
                }
            }
            return status + ": sampled RTT unavailable";
        }

        [[nodiscard]] std::string renderer_next_feature_gate(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "Select a renderer before promoting backend features.";

            if (renderer_backend_kind(ctx) == epochengine::RendererBackendKind::software)
                return "Keep software as fallback; prove the six production contexts through the shared spine.";

            return "Backend-native allocation behind handles, then materials, model import, normal maps, skybox, instancing, shadows, RTT, G-buffer, SSAO.";
        }

        struct ProjectRunBackendChoice
        {
            std::string_view label{};
            std::string_view argument{};
        };

        [[nodiscard]] std::span<const ProjectRunBackendChoice> project_run_backend_choices() noexcept
        {
#if defined(_WIN32)
            static constexpr std::array<ProjectRunBackendChoice, 6> kChoices{ {
                { "OpenGL single context", "opengl" },
                { "DirectX single context", "directx" },
                { "Vulkan single context", "vulkan" },
                { "Raylib single context", "raylib" },
                { "SDL single context", "sdl" },
                { "SFML single context", "sfml" }
            } };
#elif defined(__linux__)
            static constexpr std::array<ProjectRunBackendChoice, 1> kChoices{ {
                { "OpenGL single context", "opengl" }
            } };
#else
            static constexpr std::array<ProjectRunBackendChoice, 5> kChoices{ {
                { "OpenGL single context", "opengl" },
                { "Vulkan single context", "vulkan" },
                { "Raylib single context", "raylib" },
                { "SDL single context", "sdl" },
                { "SFML single context", "sfml" }
            } };
#endif
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view project_run_backend_label(std::string_view argument) noexcept
        {
            for (const auto& choice : project_run_backend_choices())
            {
                if (choice.argument == argument)
                    return choice.label;
            }
            return "OpenGL single context";
        }

        struct Canvas2DResolutionChoice
        {
            std::string_view label{};
            std::uint32_t width{};
            std::uint32_t height{};
        };

        struct Canvas2DBudgetChoice
        {
            std::string_view label{};
            std::uint32_t value{};
        };

        [[nodiscard]] std::span<const Canvas2DResolutionChoice> canvas2d_resolution_choices() noexcept
        {
            static constexpr std::array<Canvas2DResolutionChoice, 4> kChoices{ {
                { "320 x 180", 320u, 180u },
                { "640 x 360", 640u, 360u },
                { "1280 x 720", 1280u, 720u },
                { "1920 x 1080", 1920u, 1080u }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view canvas2d_resolution_label(
            const EditorCanvas2DProjectSnapshot& settings) noexcept
        {
            for (const auto& choice : canvas2d_resolution_choices())
            {
                if (choice.width == settings.logical_width && choice.height == settings.logical_height)
                    return choice.label;
            }
            return "Custom logical extent";
        }

        [[nodiscard]] constexpr std::string_view canvas2d_scale_policy_label(std::uint8_t policy) noexcept
        {
            switch (policy)
            {
            case 1u: return "Fractional fit";
            case 2u: return "Fill and crop";
            case 3u: return "Stretch";
            case 0u:
            default: return "Integer fit";
            }
        }

        [[nodiscard]] constexpr std::string_view canvas2d_sampling_policy_label(std::uint8_t policy) noexcept
        {
            return policy == 1u ? "Linear" : "Nearest";
        }

        [[nodiscard]] std::span<const Canvas2DBudgetChoice> canvas2d_sprite_budget_choices() noexcept
        {
            static constexpr std::array<Canvas2DBudgetChoice, 4> kChoices{ {
                { "512 sprites per batch", 512u },
                { "2048 sprites per batch", 2048u },
                { "8192 sprites per batch", 8192u },
                { "16384 sprites per batch", 16'384u }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view canvas2d_sprite_budget_label(std::uint32_t value) noexcept
        {
            for (const auto& choice : canvas2d_sprite_budget_choices())
            {
                if (choice.value == value)
                    return choice.label;
            }
            return "Custom sprite batch budget";
        }

        [[nodiscard]] std::span<const Canvas2DBudgetChoice> canvas2d_chunk_extent_choices() noexcept
        {
            static constexpr std::array<Canvas2DBudgetChoice, 5> kChoices{ {
                { "8 x 8 tiles", 8u },
                { "16 x 16 tiles", 16u },
                { "32 x 32 tiles", 32u },
                { "64 x 64 tiles", 64u },
                { "128 x 128 tiles", 128u }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view canvas2d_chunk_extent_label(std::uint32_t value) noexcept
        {
            for (const auto& choice : canvas2d_chunk_extent_choices())
            {
                if (choice.value == value)
                    return choice.label;
            }
            return "Custom tile chunk";
        }

        struct FrameLimitChoice
        {
            std::string_view label{};
            std::string_view argument{};
            double fps{};
        };

        struct ProjectCameraChoice
        {
            std::string_view label{};
            previewgrid::CameraMode mode{ previewgrid::CameraMode::Editor };
        };

        struct InputProfileChoice
        {
            std::string_view label{};
            input::ProfilePreset preset{ input::ProfilePreset::EditorDefault };
        };

        [[nodiscard]] std::span<const epochengine::project_input::KeyCode>
            project_key_choices() noexcept
        {
            using Key = epochengine::project_input::KeyCode;
            static constexpr std::array<Key, 43> kChoices{
                Key::a, Key::b, Key::c, Key::d, Key::e, Key::f, Key::g,
                Key::h, Key::i, Key::j, Key::k, Key::l, Key::m, Key::n,
                Key::o, Key::p, Key::q, Key::r, Key::s, Key::t, Key::u,
                Key::v, Key::w, Key::x, Key::y, Key::z,
                Key::space, Key::enter, Key::escape, Key::tab,
                Key::backspace, Key::left, Key::right, Key::up, Key::down,
                Key::left_control, Key::left_shift, Key::left_alt,
                Key::left_super, Key::right_control, Key::right_shift,
                Key::right_alt, Key::right_super};
            return {kChoices.data(), kChoices.size()};
        }

        [[nodiscard]] std::string_view project_key_label(
            std::uint16_t code) noexcept
        {
            return epochengine::project_input::key_code_name(
                static_cast<epochengine::project_input::KeyCode>(code));
        }

        [[nodiscard]] std::span<
            const epochengine::project_input::ControllerButton>
            project_controller_button_choices() noexcept
        {
            using Button =
                epochengine::project_input::ControllerButton;
            static constexpr std::array<Button, 15> kChoices{
                Button::south,
                Button::east,
                Button::west,
                Button::north,
                Button::back,
                Button::guide,
                Button::start,
                Button::left_stick,
                Button::right_stick,
                Button::left_shoulder,
                Button::right_shoulder,
                Button::dpad_up,
                Button::dpad_down,
                Button::dpad_left,
                Button::dpad_right};
            return {kChoices.data(), kChoices.size()};
        }

        [[nodiscard]] std::span<
            const epochengine::project_input::ControllerAxis>
            project_controller_axis_choices() noexcept
        {
            using Axis = epochengine::project_input::ControllerAxis;
            static constexpr std::array<Axis, 6> kChoices{
                Axis::left_x,
                Axis::left_y,
                Axis::right_x,
                Axis::right_y,
                Axis::left_trigger,
                Axis::right_trigger};
            return {kChoices.data(), kChoices.size()};
        }

        [[nodiscard]] std::span<const std::string_view>
            project_controller_slot_labels() noexcept
        {
            static constexpr std::array<std::string_view, 4> kLabels{
                "Controller 1",
                "Controller 2",
                "Controller 3",
                "Controller 4"};
            return {kLabels.data(), kLabels.size()};
        }

        [[nodiscard]] std::string_view project_controller_slot_label(
            std::uint8_t slot) noexcept
        {
            const auto labels = project_controller_slot_labels();
            return slot < labels.size() ? labels[slot] : "Unavailable slot";
        }

        struct ProjectControllerDeadZoneChoice final
        {
            std::string_view label{};
            std::uint16_t value_q15{};
        };

        [[nodiscard]] std::span<const ProjectControllerDeadZoneChoice>
            project_controller_dead_zone_choices() noexcept
        {
            static constexpr std::array<ProjectControllerDeadZoneChoice, 8>
                kChoices{{
                    {"Off", 0u},
                    {"5%", 1'638u},
                    {"10%", 3'277u},
                    {"12.5%", 4'096u},
                    {"15%", 4'915u},
                    {"20%", 6'553u},
                    {"25%", 8'192u},
                    {"30%", 9'830u}}};
            return {kChoices.data(), kChoices.size()};
        }

        [[nodiscard]] std::string_view project_controller_dead_zone_label(
            std::uint16_t value,
            bool uniform) noexcept
        {
            if (!uniform)
                return "Mixed values";
            for (const auto& choice : project_controller_dead_zone_choices())
            {
                if (choice.value_q15 == value)
                    return choice.label;
            }
            return "Custom";
        }

        [[nodiscard]] std::string project_controller_dead_zone_evidence(
            std::uint16_t value,
            bool uniform)
        {
            const std::string_view label =
                project_controller_dead_zone_label(value, uniform);
            if (label != "Custom")
            {
                return epochengine::format_text(
                    "{} (controller source)",
                    label);
            }

            const std::uint32_t roundedPercent =
                (static_cast<std::uint32_t>(value) * 100u + 16'383u)
                / 32'767u;
            return epochengine::format_text(
                "{}% custom (controller source)",
                roundedPercent);
        }

        [[nodiscard]] std::string project_input_settings_evidence(
            const editor_project_input_settings::SettingsEvidence& evidence)
        {
            if (!evidence.conflicts.empty())
            {
                const auto& conflict = evidence.conflicts.front();
                return epochengine::format_text(
                    "Binding conflict: {} and {} use the same physical input.",
                    conflict.binding.value,
                    conflict.conflicting_binding.value);
            }
            return epochengine::format_text(
                "{} | validation {} | edit {}",
                editor_project_input_settings::settings_code_name(evidence.code),
                project_input::validation_code_name(evidence.validation),
                project_input::profile_edit_code_name(evidence.edit));
        }

        [[nodiscard]] bool load_project_input_settings(
            EditorState& editor,
            bool forceReload = false)
        {
            if (editor.projectId.empty() || editor.projectRoot.empty())
            {
                editor.projectInputSettings.reset();
                editor.projectInputSettingsStatus =
                    "Open a project before editing project input.";
                return false;
            }
            if (!forceReload && editor.projectInputSettings)
            {
                const auto snapshot = editor.projectInputSettings->snapshot();
                if (snapshot.loaded && snapshot.project_id == editor.projectId)
                    return true;
            }

            project_input::ProjectInputProfileStore store{
                editor.projectId,
                std::filesystem::path{editor.projectRoot}};
            if (!store.valid())
            {
                editor.projectInputSettings.reset();
                editor.projectInputSettingsStatus =
                    "Project input store is invalid.";
                return false;
            }

            project_input::ProfileSource source{};
            const auto loaded = store.load_source();
            if (loaded)
            {
                source = loaded.source;
            }
            else if (loaded.code == project_input::StoreCode::not_found)
            {
                const auto artifact = store.load_artifact();
                if (artifact)
                {
                    source = {
                        .id = artifact.artifact.profile_id,
                        .display_name = artifact.artifact.display_name,
                        .revision = artifact.artifact.source_revision,
                        .actions = artifact.artifact.actions,
                        .bindings = artifact.artifact.bindings};
                }
                else if (artifact.code == project_input::StoreCode::not_found)
                {
                    source = project_input::make_legacy_default_profile();
                }
                else
                {
                    editor.projectInputSettings.reset();
                    editor.projectInputSettingsStatus = std::string{
                        "Input artifact "}
                        + std::string{project_input::store_code_name(artifact.code)};
                    return false;
                }
            }
            else
            {
                editor.projectInputSettings.reset();
                editor.projectInputSettingsStatus = std::string{"Input source "}
                    + std::string{project_input::store_code_name(loaded.code)};
                return false;
            }

            auto controller = std::make_unique<
                editor_project_input_settings::Controller>();
            const auto evidence = controller->load(
                editor.projectId, std::move(source));
            editor.projectInputSettingsStatus =
                project_input_settings_evidence(evidence);
            if (!evidence)
            {
                editor.projectInputSettings.reset();
                return false;
            }
            editor.projectInputSettings = std::move(controller);
            return true;
        }

        [[nodiscard]] EditorProjectInputUpdateResult
            apply_project_input_settings(EditorState& editor)
        {
            if (!editor.projectInputSettings)
                return {false, "Project input settings are not loaded."};
            const auto plan = editor.projectInputSettings->prepare_save();
            if (!plan)
            {
                editor.projectInputSettingsStatus = epochengine::format_text(
                    "Apply blocked: {} / validation {}.",
                    editor_project_input_settings::settings_code_name(plan.code),
                    project_input::validation_code_name(plan.validation));
                return {false, editor.projectInputSettingsStatus};
            }

            project_input::ProjectInputProfileStore store{
                editor.projectId,
                std::filesystem::path{editor.projectRoot}};
            if (!store.valid())
                return {false, "Project input store is invalid."};

            const auto current = store.load_source();
            if (current)
            {
                if (current.source.revision != plan.expected_base_revision)
                {
                    editor.projectInputSettingsStatus =
                        "Apply blocked: project input changed on disk. Discard or reload before editing again.";
                    return {false, editor.projectInputSettingsStatus};
                }
            }
            else if (current.code == project_input::StoreCode::not_found)
            {
                const auto artifact = store.load_artifact();
                if (artifact
                    && artifact.artifact.source_revision
                        != plan.expected_base_revision)
                {
                    editor.projectInputSettingsStatus =
                        "Apply blocked: compiled project input changed on disk.";
                    return {false, editor.projectInputSettingsStatus};
                }
                if (!artifact
                    && artifact.code != project_input::StoreCode::not_found)
                {
                    editor.projectInputSettingsStatus = std::string{
                        "Apply blocked: input artifact "}
                        + std::string{project_input::store_code_name(artifact.code)};
                    return {false, editor.projectInputSettingsStatus};
                }
            }
            else
            {
                editor.projectInputSettingsStatus = std::string{
                    "Apply blocked: input source "}
                    + std::string{project_input::store_code_name(current.code)};
                return {false, editor.projectInputSettingsStatus};
            }

            const auto published = store.publish_source_and_artifact(
                plan.source, plan.artifact);
            if (!published)
            {
                if (published.source)
                {
                    const auto confirmed =
                        editor.projectInputSettings->confirm_saved(
                            plan.revision);
                    editor.projectInputSettingsStatus =
                        epochengine::format_text(
                            "Project input source saved at revision {}; "
                            "artifact publish {}. Build or Run will regenerate it.",
                            plan.revision.sequence,
                            project_input::store_code_name(published.code));
                    return {
                        static_cast<bool>(confirmed),
                        editor.projectInputSettingsStatus};
                }
                editor.projectInputSettingsStatus = std::string{
                    "Input source/artifact publish "}
                    + std::string{project_input::store_code_name(published.code)}
                    + ". No source change was committed.";
                return {false, editor.projectInputSettingsStatus};
            }
            const auto confirmed =
                editor.projectInputSettings->confirm_saved(plan.revision);
            editor.projectInputSettingsStatus =
                project_input_settings_evidence(confirmed);
            return {
                static_cast<bool>(confirmed),
                confirmed
                    ? epochengine::format_text(
                        "Project input source and artifact saved at revision {}.",
                        plan.revision.sequence)
                    : editor.projectInputSettingsStatus};
        }

        [[nodiscard]] std::span<const FrameLimitChoice> frame_limit_choices() noexcept
        {
            static constexpr std::array<FrameLimitChoice, 5> kChoices{ {
                { "30 FPS", "30", 30.0 },
                { "60 FPS", "60", 60.0 },
                { "120 FPS", "120", 120.0 },
                { "0 (Unlimited)", "0", 0.0 },
                { "Manual", "1500", 1500.0 }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view frame_limit_label(double fps) noexcept
        {
            return epochengine::perf::label_for_frame_limit(fps);
        }

        [[nodiscard]] std::string frame_limit_argument(double fps)
        {
            for (const auto& choice : frame_limit_choices())
            {
                if (choice.fps == fps)
                    return std::string{choice.argument};
            }
            return std::isfinite(fps) && fps > 0.0
                ? epochengine::format_text("{}", fps) : "0";
        }

        [[nodiscard]] bool frame_limit_control(std::string_view id, double& fps,
            EditorState::FrameLimitEditState& edit, float width, bool vsync = false)
        {
            const auto start = gui::cursor_position();
            const auto choices = frame_limit_choices();
            if (!edit.initialized)
            {
                edit.initialized = true;
                edit.manual = fps != 0.0 && fps != 30.0 && fps != 60.0 && fps != 120.0;
                if (edit.manual) edit.text = frame_limit_argument(fps);
            }
            std::array<std::string_view, 5> labels{};
            for (std::size_t n = 0; n < choices.size(); ++n) labels[n] = choices[n].label;
            const float rowWidth = (std::max)(1.0f, width);
            const bool stacked = rowWidth < 280.0f;
            const float selectWidth = stacked ? rowWidth : (std::min)(250.0f, rowWidth - 160.0f);
            const auto selected = gui::select_box(gui::SelectBoxOptions{
                .id = id, .placeholder = "Frame limit",
                .selected = edit.manual ? "Manual" : vsync ? "VSync"
                    : fps == 0.0 ? "0 (Unlimited)" : frame_limit_label(fps),
                .options = labels, .size = {selectWidth, 30.0f},
                .row_height = 28.0f, .max_visible_options = 5});
            bool changed = false;
            if (selected.changed && selected.selected_index && *selected.selected_index < choices.size())
            {
                edit.manual = *selected.selected_index == choices.size() - 1u;
                edit.error.clear();
                if (edit.manual)
                {
                    double parsed{};
                    if (perf::parse_frame_limit_value(edit.text, parsed)) { fps = parsed; changed = true; }
                    else edit.error = "Enter 0 or a positive FPS.";
                }
                else { fps = choices[*selected.selected_index].fps; changed = true; }
            }
            if (edit.manual)
            {
                const gui::Vec2 manualPos{stacked ? start.x : start.x + selectWidth + 8.0f,
                    start.y + (stacked ? 34.0f : 0.0f)};
                const float inputWidth = stacked ? (std::min)(88.0f, (std::max)(32.0f, rowWidth - 64.0f)) : 88.0f;
                gui::set_cursor(manualPos);
                const auto input = gui::edit_box(edit.text, {inputWidth, 30.0f}, 24u, false,
                    gui::TextInputKeyboardPolicy::focused, true, "1500");
                gui::set_cursor({manualPos.x + inputWidth + 8.0f, manualPos.y});
                const bool apply = gui::button("Apply", {56.0f, 30.0f});
                if (input.submitted || apply)
                {
                    double parsed{};
                    if (perf::parse_frame_limit_value(edit.text, parsed))
                    { fps = parsed; changed = true; edit.error.clear(); }
                    else edit.error = "Enter 0 or a positive FPS.";
                }
            }
            gui::set_cursor({start.x, start.y + (stacked && edit.manual ? 66.0f : 32.0f)});
            if (!edit.error.empty()) gui::label(edit.error);
            return changed;
        }

        [[nodiscard]] std::span<const ProjectCameraChoice> project_camera_choices() noexcept
        {
            static constexpr std::array<ProjectCameraChoice, 3> kChoices{ {
                { "Editor orbit camera", previewgrid::CameraMode::Editor },
                { "First-person runtime camera", previewgrid::CameraMode::FPS },
                { "Locked 2D canvas camera", previewgrid::CameraMode::Canvas2D }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::span<const InputProfileChoice> input_profile_choices() noexcept
        {
            static constexpr std::array<InputProfileChoice, 4> kChoices{ {
                { "Editor Default", input::ProfilePreset::EditorDefault },
                { "Runtime WASD", input::ProfilePreset::RuntimeWASD },
                { "Arrow Pilot", input::ProfilePreset::ArrowPilot },
                { "Left-Handed IJKL", input::ProfilePreset::LeftHanded }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view input_profile_label(input::ProfilePreset preset) noexcept
        {
            return input::profile_preset_label(preset);
        }

        [[nodiscard]] std::string_view input_profile_argument(input::ProfilePreset preset) noexcept
        {
            return input::profile_preset_id(preset);
        }

        [[nodiscard]] std::string_view project_camera_label(previewgrid::CameraMode mode) noexcept
        {
            for (const auto& choice : project_camera_choices())
            {
                if (choice.mode == mode)
                    return choice.label;
            }
            return "Editor orbit camera";
        }

        [[nodiscard]] std::string_view project_camera_argument(previewgrid::CameraMode mode) noexcept
        {
            switch (mode)
            {
            case previewgrid::CameraMode::FPS:
                return "fps";
            case previewgrid::CameraMode::Canvas2D:
                return "canvas2d";
            case previewgrid::CameraMode::Editor:
            default:
                return "editor";
            }
        }

        [[nodiscard]] std::string vec3_text(const std::array<float, 3>& value)
        {
            return epochengine::format_text("({:.1f}, {:.1f}, {:.1f})", value[0], value[1], value[2]);
        }

        [[nodiscard]] std::string ellipsize(std::string_view text, std::size_t max_chars)
        {
            if (text.size() <= max_chars)
                return std::string(text);

            if (max_chars <= 3)
                return std::string(text.substr(0, max_chars));

            return std::string(text.substr(0, max_chars - 3)) + "...";
        }

        [[nodiscard]] std::string_view preview_mode_name(core::ScenePreviewMode mode) noexcept
        {
            switch (mode)
            {
            case core::ScenePreviewMode::Editor: return "Editor";
            case core::ScenePreviewMode::None: return "None";
            default: return "Editor";
            }
        }

        [[nodiscard]] std::string preview_camera_name(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "Editor";
            return std::string(epochengine::previewgrid::camera_mode_name(
                epochengine::previewgrid::camera_mode_for(ctx.get())));
        }

        [[nodiscard]] std::string preview_zoom_text(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "13.5";

            return epochengine::format_text("{:.1f}", epochengine::previewgrid::camera_distance_for(ctx.get()));
        }

        [[nodiscard]] std::string backend_ownership_model(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "No active backend context.";

            switch (ctx->type)
            {
            case core::ContextType::SDL:
                return "Visible proxy shell owns the dock slot; SDL_app stays nested inside it during undock/redock.";
            case core::ContextType::SFML:
                return "Visible proxy shell owns the dock slot; SFML_Window stays nested inside it during undock/redock.";
            case core::ContextType::RayLib:
                return "Real child pane with a parked helper host; the child surface is the visible docked backend.";
            case core::ContextType::OpenGL:
            case core::ContextType::Vulkan:
            case core::ContextType::DirectX:
            case core::ContextType::Software:
                return "Direct child/editor-owned pane with no proxy-shell handoff in normal docked use.";
            default:
                return "Backend ownership model not classified yet.";
            }
        }

        [[nodiscard]] constexpr std::string_view backend_lifecycle_policy() noexcept
        {
            return "Switch deliberately; inactive backends should be torn down and recreated, not parked invisibly.";
        }

