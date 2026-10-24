        class ProjectPlayScene final : public epochengine::scene::Scene
        {
        public:
            explicit ProjectPlayScene(std::string_view project_payload)
            {
                const auto launch = parse_project_scene_launch(project_payload);
                m_cameraMode = launch.camera_mode;
                m_inputProfile = launch.input_profile;
                input::set_active_profile(m_inputProfile);

                const auto* profile = epochengine::editor_find_project_profile(launch.project_id);
                if (!profile)
                    profile = &epochengine::editor_default_project_profile();

                m_projectId = std::string(profile->id);
                m_projectName = std::string(profile->display_name);
                const std::filesystem::path runtimeRoot =
                    project_runtime_root(*profile);
                m_projectRoot = runtimeRoot.generic_string();
                m_scenePath = project_profile_path(
                    *profile, runtimeRoot, profile->scene_path).generic_string();
                m_tileMapPath = std::string(profile->tilemap_path);
                m_inputProfilePath = std::string(profile->input_profile_path);
                m_spriteAnimationPath =
                    std::string(profile->sprite_animation_path);
                m_audioProfilePath =
                    std::string(profile->audio_profile_path);
                m_worldName = std::string(profile->world_name);
                m_scriptName = std::string(profile->default_script);
                m_description = std::string(profile->description);
                m_runtimeBudgets =
                    epochengine::platform::recommended_budgets_for_tier(
                        epochengine::perf::tier::mobile_30);
                m_runtimeBudgetProfile = "T1-GLES/mobile_30";
                m_modelSummary = epochengine::editor_project_model_summary(m_projectId);
                prepare_project_gui();
                const auto seedEntities = epochengine::editor_seed_entities_for_project(m_projectId);
                const auto sourceSnapshot = load_project_runtime_snapshot(
                    m_scenePath,
                    profile->id,
                    profile->runtime_scene_id,
                    profile->world_name,
                    std::span<const epochengine::EditorSceneSeedEntity>{ seedEntities.data(), seedEntities.size() });
                const auto compileReport = m_sceneRuntime.replace(
                    sourceSnapshot,
                    epochengine::scene_runtime::RuntimeScenePolicy{
                        .include_hidden_objects = false,
                        .include_editor_only_objects = false
                    });
                if (compileReport.committed())
                {
                    const auto compiled = m_sceneRuntime.projection()->entities();
                    m_entities.assign(compiled.begin(), compiled.end());
                }
                m_lightingFrame = build_project_play_lighting(
                    std::span<const ProjectRuntimeEntity>{ m_entities.data(), m_entities.size() });
            }

            void load() override
            {
                Scene::load();
                prepare_gameplay();
            }

            void unload() override
            {
                shutdown_gameplay();
                retire_canvas2d_scenes();
                m_cameraApplied.clear();
                m_hasLastFrame = false;
                Scene::unload();
            }

            ~ProjectPlayScene() override
            {
                shutdown_gameplay();
                retire_canvas2d_scenes();
            }

            bool frame(std::shared_ptr<epochengine::core::Context> ctx, epochengine::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                // In-editor play always returns on physical Escape, even if
                // the project remaps its Cancel action to another key.
                if (ctx->action_pressed_safe(input::Action::Cancel)
                    || ctx->is_key_down_safe(input::Key::Escape))
                {
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
                    (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    m_canvas2dPublished.erase(ctx.get());
                    return false;
                }

                const auto now = timing::Clock::now();
                float dt = 0.0f;
                if (m_hasLastFrame)
                    dt = std::chrono::duration<float>(now - m_lastFrame).count();
                m_lastFrame = now;
                m_hasLastFrame = true;

                int mx = 0;
                int my = 0;
                ctx->get_mouse_position_safe(mx, my);
                const gui::Vec2 mouse_pos{
                    static_cast<float>(mx),
                    static_cast<float>(my)
                };

                const bool mouse_left_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                const bool mouse_right_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseRight);
                const bool mouse_middle_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseMiddle);

                const int width = (std::max)(1, ctx->width > 0 ? ctx->width : ctx->get_width_safe());
                const int height = (std::max)(1, ctx->height > 0 ? ctx->height : ctx->get_height_safe());
                bool projectGuiCapturesPointer{};
                bool projectGuiCapturesKeyboard{};
                if (m_projectGuiRuntime)
                {
                    (void)m_projectGuiRuntime->build_frame({
                        static_cast<float>(width),
                        static_cast<float>(height)});
                    projectGuiCapturesPointer =
                        m_projectGuiRuntime->pointer_captured({
                            mouse_pos.x, mouse_pos.y});
                    projectGuiCapturesKeyboard =
                        m_projectGuiRuntime->keyboard_captured();
                }
                advance_gameplay(
                    ctx.get(),
                    dt,
                    projectGuiCapturesKeyboard || !ctx->has_input_focus_safe());

                const bool backendOwnsFrameClear =
                    ctx->type == core::ContextType::OpenGL;
                if (!backendOwnsFrameClear)
                    ctx->clear_safe();
                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                if (!m_cameraApplied.contains(ctx.get()))
                {
                    epochengine::previewgrid::set_camera_mode(ctx.get(), m_cameraMode);
                    if (m_cameraMode == epochengine::previewgrid::CameraMode::FPS)
                    {
                        if (const auto spawn = project_spawn_position(
                            std::span<const ProjectRuntimeEntity>{ m_entities.data(), m_entities.size() }))
                        {
                            (void)epochengine::previewgrid::focus_camera(ctx.get(), *spawn, 0.0f);
                        }
                    }
                    m_cameraApplied[ctx.get()] = true;
                }
                ctx->set_scene_viewport({ 0, 0, width, height });
                if (m_cameraMode == epochengine::previewgrid::CameraMode::Canvas2D)
                {
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
                    const auto published = m_canvas2dPublished.find(ctx.get());
                    const auto acquired =
                        epochengine::canvas2d::scene_content::acquire(ctx.get());
                    const auto* desiredScene = m_gameplayRuntime
                        ? m_gameplayRuntime->scene()
                        : (m_staticCanvasScene
                            ? &*m_staticCanvasScene : nullptr);
                    const bool needsPublication = desiredScene
                        && (published == m_canvas2dPublished.end()
                            || !acquired
                            || acquired.generation != published->second
                            || !acquired.content
                            || acquired.content->source_revision
                                != desiredScene->source_revision);
                    if (needsPublication)
                    {
                        const auto publication =
                            epochengine::canvas2d::scene_content::publish(
                                ctx.get(), *desiredScene);
                        if (publication)
                            m_canvas2dPublished[ctx.get()] = publication.generation;
                        else
                        {
                            m_tileMapStatus =
                                std::string("scene publication ")
                                + std::string(
                                    epochengine::canvas2d::scene_content::scene_code_name(
                                        publication.code));
                        }
                    }
                    else if (!desiredScene)
                    {
                        (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    }
                }
                else
                {
                    (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    m_canvas2dPublished.erase(ctx.get());
                    publish_project_play_markers(ctx.get(), std::span<const ProjectRuntimeEntity>{
                        m_entities.data(),
                        m_entities.size()
                    });
                    epochengine::previewgrid::set_lighting_frame(
                        ctx.get(), m_lightingFrame);
                }

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                const int wheelDelta = epochengine::gui::consume_mouse_wheel_delta();
                if (ctx->action_pressed_safe(input::Action::ResetCamera))
                    epochengine::previewgrid::reset_camera(ctx.get());

                const bool altHeld = ctx->is_key_held_safe(input::Key::LeftAlt)
                    || ctx->is_key_held_safe(input::Key::RightAlt);
                const bool shiftHeld = ctx->is_key_held_safe(input::Key::LeftShift)
                    || ctx->is_key_held_safe(input::Key::RightShift);
                const bool controlHeld = ctx->is_key_held_safe(input::Key::LeftControl)
                    || ctx->is_key_held_safe(input::Key::RightControl);
                const auto navigationGestures =
                    epochengine::previewgrid::resolve_camera_navigation_gestures(
                        altHeld, mouse_left_down, mouse_middle_down, mouse_right_down);
                const bool orbiting = !projectGuiCapturesPointer && navigationGestures.orbiting;
                const bool panning = !projectGuiCapturesPointer && navigationGestures.panning;
                const bool dollying = !projectGuiCapturesPointer && navigationGestures.dollying;
                const bool flying = !projectGuiCapturesPointer && navigationGestures.flying;
                const bool keyboardNavigation = !projectGuiCapturesKeyboard && flying;
                const float navigationMultiplier = shiftHeld ? 4.0f : (controlHeld ? 0.25f : 1.0f);

                const float forwardInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::MoveForward) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::MoveBackward) ? 1.0f : 0.0f);
                const float rightInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::MoveRight) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::MoveLeft) ? 1.0f : 0.0f);
                const float upInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::MoveUp) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::MoveDown) ? 1.0f : 0.0f);
                const float yawInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::LookRight) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::LookLeft) ? 1.0f : 0.0f);
                const float pitchInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::LookUp) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::LookDown) ? 1.0f : 0.0f);


                if ((flying && m_lookState.flying) || (orbiting && m_lookState.orbiting))
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    const float sensitivity = input::mouse_look_sensitivity();
                    epochengine::previewgrid::look_camera(
                        ctx.get(),
                        mouseDeltaX * sensitivity,
                        -mouseDeltaY * sensitivity);
                }

                else if (panning && m_lookState.panning)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochengine::previewgrid::pan_camera_drag(
                        ctx.get(),
                        mouseDeltaX,
                        -mouseDeltaY);
                }

                else if (dollying && m_lookState.dollying)
                {
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochengine::previewgrid::dolly_camera_drag(ctx.get(), mouseDeltaY);
                }


                if (wheelDelta != 0 && !projectGuiCapturesPointer)
                {
                    const float wheelSteps = static_cast<float>(wheelDelta) / 120.0f;
                    if (flying)
                        epochengine::previewgrid::adjust_fly_speed(ctx.get(), wheelSteps);
                    else
                        epochengine::previewgrid::zoom_camera(
                            ctx.get(), wheelSteps * input::wheel_zoom_step());
                }

                epochengine::previewgrid::step_camera(
                    ctx.get(),
                    dt,
                    forwardInput,
                    rightInput,
                    upInput,
                    yawInput,
                    pitchInput,
                    navigationMultiplier);

                m_lookState.last_mouse = mouse_pos;
                m_lookState.orbiting = orbiting;
                m_lookState.panning = panning;
                m_lookState.dollying = dollying;
                m_lookState.flying = flying;

                bool returnToEditor = false;
                if (m_projectGuiRuntime && m_projectGuiAdapter)
                {
                    auto guiResult = m_projectGuiAdapter->render(
                        *m_projectGuiRuntime,
                        {
                            static_cast<float>(width),
                            static_cast<float>(height)});
                    if (guiResult)
                    {
                        for (const auto& event : guiResult.events)
                        {
                            if (event.action.empty())
                                continue;
                            if (event.action == "editor.return")
                            {
                                if (event.kind
                                    == epochengine::project_gui_runtime::EventKind::activated)
                                {
                                    returnToEditor = true;
                                    m_projectGuiStatus =
                                        "GUI host command: return to editor.";
                                }
                                else
                                {
                                    m_projectGuiStatus =
                                        "GUI host command rejected for this event.";
                                }
                                continue;
                            }
                            queue_project_gui_action(event);
                        }
                    }
                    else
                    {
                        m_projectGuiStatus = std::string{"GUI runtime: "}
                            + std::string{
                                epochengine::project_gui_runtime::runtime_code_name(
                                    guiResult.code)};
                    }
                }
                gui::begin_top_layer();
                gui::begin_window(
                    "Project Runtime Preview",
                    {24.0f, 24.0f},
                    {430.0f, m_gameplayRuntime
                            && m_gameplayRuntime->active()
                        ? 724.0f
                        : (m_tileMapPath.empty() ? 210.0f : 306.0f)});
                gui::label(std::string("Project: ") + m_projectName);
                gui::label(std::string("World: ") + m_worldName);
                gui::label(std::string("Scene: ") + m_scenePath);
                gui::label(std::string("Script: ") + m_scriptName);
                gui::wrapped_label(m_projectGuiStatus, 390.0f);
                gui::label(std::string("Preview Objects: ") + std::to_string(visible_runtime_entity_count(m_entities)));
                if (!m_tileMapPath.empty())
                {
                    gui::label(std::string("Tile Map: ") + m_tileMapPath);
                    gui::wrapped_label(
                        std::string("Tile Runtime: ")
                            + (m_tileMapStatus.empty()
                                ? std::string("not prepared")
                                : m_tileMapStatus),
                        390.0f);
                    if (!m_inputProfilePath.empty())
                    {
                        gui::label(std::string("Input Profile: ")
                            + m_inputProfilePath);
                        gui::wrapped_label(
                            std::string("Input Runtime: ")
                                + (m_projectInputStatus.empty()
                                    ? std::string("not prepared")
                                    : m_projectInputStatus),
                            390.0f);
                    }
                    if (!m_spriteAnimationPath.empty())
                    {
                        gui::label(std::string("Sprite Animations: ")
                            + m_spriteAnimationPath);
                        gui::wrapped_label(
                            std::string("Animation Runtime: ")
                                + (m_spriteAnimationStatus.empty()
                                    ? std::string("not prepared")
                                    : m_spriteAnimationStatus),
                            390.0f);
                    }
                    gui::wrapped_label(
                        std::string{"Gameplay: "}
                            + (m_actorStatus.empty()
                                ? std::string{"not prepared"}
                                : m_actorStatus),
                        390.0f);
                    if (m_gameplayRuntime && m_gameplayRuntime->active())
                    {
                        const auto snapshot = m_gameplayRuntime->snapshot();
                        const auto& actor = snapshot.actor;
                        const auto metrics = snapshot.metrics;
                        gui::label(
                            std::string("Position: ")
                            + std::to_string(actor.x) + ", "
                            + std::to_string(actor.y));
                        gui::label(
                            std::string("Physics Tick: ")
                            + std::to_string(actor.fixed_tick));
                        gui::label(
                            std::string("Fixed Steps: ")
                            + std::to_string(metrics.fixed_steps));
                        gui::label(
                            std::string("Actor Events: ")
                            + std::to_string(metrics.actor_events));
                        gui::label(
                            std::string("Audio Triggers: ")
                            + std::to_string(metrics.audio_triggers));
                        if (!m_runtimeCosts
                            || m_costFramesUntilRefresh == 0u)
                        {
                            m_runtimeCosts =
                                m_gameplayRuntime->cost_snapshot();
                            m_runtimeBudgetAssessment =
                                epochengine::project_gameplay2d::
                                    assess_runtime_costs(
                                        *m_runtimeCosts, m_runtimeBudgets);
                            m_costFramesUntilRefresh = 15u;
                        }
                        else
                            --m_costFramesUntilRefresh;
                        if (m_runtimeCosts)
                        {
                            const auto& costs = *m_runtimeCosts;
                            gui::label(
                                std::string{"Canvas Cost: "}
                                + std::to_string(costs.emitted_sprites)
                                + " sprites / "
                                + std::to_string(costs.emitted_batches)
                                + " batches / "
                                + std::to_string(costs.emitted_vertices)
                                + " vertices");
                            gui::label(
                                std::string{"Texture Cost: "}
                                + std::to_string(
                                    costs.source_texture_views)
                                + " views / "
                                + std::to_string(
                                    costs.source_texture_bytes / 1024u)
                                + " KiB logical");
                            gui::label(
                                std::string{"Physics Cost: "}
                                + std::to_string(
                                    costs.collision_surfaces)
                                + " surfaces / "
                                + std::to_string(
                                    costs.peak_contacts_per_step)
                                + " peak contacts");
                            gui::label(
                                std::string{"Audio Cost: "}
                                + std::to_string(
                                    costs.audio_resident_bytes / 1024u)
                                + " KiB / "
                                + std::to_string(
                                    costs.audio_frames_mixed)
                                + " mixed frames");
                            gui::wrapped_label(
                                std::string{"Cost Status: "}
                                    + costs.diagnostic,
                                390.0f);
                            if (m_runtimeBudgetAssessment)
                            {
                                gui::wrapped_label(
                                    std::string{"Tier Budget ("}
                                        + m_runtimeBudgetProfile + "): "
                                        + m_runtimeBudgetAssessment->diagnostic,
                                    390.0f);
                            }
                        }
                        gui::wrapped_label(
                            std::string("Audio: ")
                                + (m_audioStatus.empty()
                                    ? std::string("not active")
                                    : m_audioStatus),
                            390.0f);
                        if (gui::button(
                            actor.paused ? "Resume" : "Pause",
                            {190.0f, 30.0f}))
                        {
                            const auto code = m_gameplayRuntime->set_paused(
                                !actor.paused);
                            m_actorStatus = std::string{
                                epochengine::project_gameplay2d::
                                    session_code_name(code)}
                                + ": " + std::string{
                                    m_gameplayRuntime->diagnostic()};
                        }
                        if (gui::button("Reset Actor", {190.0f, 30.0f}))
                        {
                            const auto code = m_gameplayRuntime->reset();
                            m_actorStatus = std::string{
                                epochengine::project_gameplay2d::
                                    session_code_name(code)}
                                + ": " + std::string{
                                    m_gameplayRuntime->diagnostic()};
                        }
                    }
                    if (gui::button(
                            "Return to Editor", {190.0f, 30.0f}))
                    {
                        returnToEditor = true;
                    }
                }
                gui::wrapped_label(
                    std::string("Demo model: ")
                    + (m_modelSummary.asset_path.empty() ? std::string("(none)") : m_modelSummary.asset_path),
                    390.0f);
                gui::wrapped_label(
                    std::string("Model summary: ")
                    + (m_modelSummary.summary.empty() ? std::string("(unavailable)") : m_modelSummary.summary),
                    390.0f);
                gui::wrapped_label(m_description, 390.0f);
                gui::end_window();
                gui::end_top_layer();

                gui::end_frame();
                ctx->present_safe();
                return !returnToEditor;
            }

        private:
            void prepare_gameplay()
            {
                shutdown_gameplay();
                m_staticCanvasScene.reset();
                m_projectControllerSampler.reset();
                m_inputFrameIndex = 0u;
                m_runtimeCosts.reset();
                m_runtimeBudgetAssessment.reset();
                m_costFramesUntilRefresh = 0u;
                for (auto& pending : m_pendingGuiActions)
                    pending.reset();

                if (m_cameraMode
                        != epochengine::previewgrid::CameraMode::Canvas2D
                    || m_tileMapPath.empty() || m_projectRoot.empty())
                {
                    return;
                }

                auto prepared =
                    epochengine::project_gameplay2d::prepare_project({
                        .project_id = m_projectId,
                        .project_root =
                            std::filesystem::path{m_projectRoot},
                        .tilemap = {
                            .logical_path = m_tileMapPath,
                            .source_policy = epochengine::
                                project_tilemap_runtime::SourcePolicy::
                                    prefer_source},
                        .input_profile_path = m_inputProfilePath,
                        .sprite_animation_path = m_spriteAnimationPath,
                        .audio_profile_path = m_audioProfilePath,
                        .input_fallback = epochengine::project_gameplay2d::
                            InputFallbackPolicy::allow_legacy_default,
                        .request_physical_audio =
                            EPOCH_ENABLE_PHYSICAL_AUDIO != 0,
                        .allow_compatibility_audio = true,
                        .require_sprite_animation = false,
                        .require_audio = false});

                if (!prepared)
                {
                    m_actorStatus = std::string{"gameplay preparation "}
                        + std::string{epochengine::project_gameplay2d::
                            preparation_code_name(prepared.code)}
                        + ": " + prepared.diagnostic;
                    m_tileMapStatus = m_actorStatus;

                    epochengine::project_tilemap_runtime::
                        ProjectTileMapRuntime fallback{
                            m_projectId, m_projectRoot};
                    auto staticMap = fallback.prepare({
                        .logical_path = m_tileMapPath,
                        .source_policy = epochengine::
                            project_tilemap_runtime::SourcePolicy::
                                prefer_source});
                    if (staticMap)
                    {
                        m_staticCanvasScene = std::move(staticMap.scene);
                        m_tileMapStatus = std::move(staticMap.diagnostic);
                    }
                    return;
                }

                const auto& evidence = prepared.project.evidence;
                m_tileMapStatus = evidence.tilemap_diagnostic;
                m_projectInputStatus = evidence.input_diagnostic;
                m_spriteAnimationStatus =
                    m_spriteAnimationPath.empty()
                    ? "Project does not declare sprite animations."
                    : evidence.animation_diagnostic;
                m_audioStatus = evidence.audio_diagnostic.empty()
                    ? "Project audio is not active."
                    : evidence.audio_diagnostic;
                m_staticCanvasScene = prepared.project.base_scene;

                if (!g_processAudioRuntime && prepared.project.audio_program)
                {
                    prepared.project.audio_program.reset();
                    m_audioStatus =
                        "Process audio runtime unavailable; gameplay audio disabled.";
                }

                auto gameplay = std::make_unique<
                    epochengine::project_gameplay2d::Gameplay2DRuntime>();
                const auto opened = gameplay->open(
                    std::move(prepared.project), g_processAudioRuntime);
                m_actorStatus =
                    std::string{epochengine::project_gameplay2d::
                        session_code_name(opened)}
                    + ": " + std::string{gameplay->diagnostic()};
                if (opened
                        == epochengine::project_gameplay2d::SessionCode::ready
                    || opened
                        == epochengine::project_gameplay2d::SessionCode::degraded)
                {
                    m_gameplayRuntime = std::move(gameplay);
                    m_staticCanvasScene.reset();
                }
            }

            void shutdown_gameplay() noexcept
            {
                if (m_gameplayRuntime)
                    (void)m_gameplayRuntime->close();
                m_gameplayRuntime.reset();
                m_staticCanvasScene.reset();
                m_projectControllerSampler.reset();
                m_inputFrameIndex = 0u;
                m_runtimeCosts.reset();
                m_runtimeBudgetAssessment.reset();
                m_costFramesUntilRefresh = 0u;
                for (auto& pending : m_pendingGuiActions)
                    pending.reset();
            }

            void prepare_project_gui()
            {
                epochengine::project_gui::ArtifactLibrary library{
                    m_projectId, m_projectRoot};
                auto loaded = library.load_latest(
                    project_gui::canonical_source_path);
                if (!loaded)
                {
                    m_projectGuiStatus = loaded.code
                            == epochengine::project_gui::LibraryCode::not_found
                        ? "Project GUI: no compiled GUI artifact."
                        : std::string{"Project GUI load failed: "}
                            + std::string{
                                epochengine::project_gui::library_code_name(
                                    loaded.code)};
                    return;
                }

                auto runtime = std::make_unique<
                    epochengine::project_gui_runtime::RuntimeSession>(
                        std::move(loaded.artifact));
                if (!runtime->valid())
                {
                    m_projectGuiStatus =
                        "Project GUI runtime rejected the compiled artifact.";
                    return;
                }
                m_projectGuiRuntime = std::move(runtime);
                m_projectGuiAdapter = std::make_unique<
                    epochengine::project_gui::EpochGuiAdapter>(
                        m_projectId, m_projectRoot);
                m_projectGuiStatus = "Project GUI runtime ready.";
            }

            void queue_project_gui_action(
                const epochengine::project_gui_runtime::RuntimeEvent& event)
            {
                namespace project_input = epochengine::project_input;
                const project_input::ActionSemantic semantic =
                    project_input::action_semantic_from_name(event.action);
                if (semantic == project_input::ActionSemantic::invalid)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: unknown project action '"
                        + event.action + "'.";
                    return;
                }
                const auto* projectInput = m_gameplayRuntime
                    ? m_gameplayRuntime->input_profile() : nullptr;
                if (!projectInput)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: no compiled project input profile.";
                    return;
                }
                const auto definition = std::find_if(
                    projectInput->actions.begin(),
                    projectInput->actions.end(),
                    [&](const project_input::ActionDefinition& action)
                    {
                        return action.semantic == semantic
                            && action.id
                                == project_input::stable_action_id(semantic);
                    });
                if (definition == projectInput->actions.end())
                {
                    m_projectGuiStatus =
                        "GUI action rejected: action is absent from the "
                        "compiled project profile.";
                    return;
                }

                project_input::ActionImpulse impulse{
                    .semantic = semantic,
                    .value_q15 = project_input::normalized_unit,
                    .pressed = true};
                if (event.kind
                    == epochengine::project_gui_runtime::EventKind::slider_changed)
                {
                    if (definition->value_kind
                        != project_input::ActionValueKind::axis)
                    {
                        m_projectGuiStatus =
                            "GUI action rejected: slider requires an axis action.";
                        return;
                    }
                    const double normalized = std::clamp(event.value, -1.0, 1.0);
                    impulse.value_q15 = static_cast<std::int32_t>(
                        std::llround(
                            normalized
                            * static_cast<double>(
                                project_input::normalized_unit)));
                    impulse.pressed = false;
                }
                else if (event.kind
                    != epochengine::project_gui_runtime::EventKind::activated)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: event does not produce "
                        "project input.";
                    return;
                }

                const std::size_t index =
                    static_cast<std::size_t>(semantic) - 1u;
                m_pendingGuiActions[index] = impulse;
                m_projectGuiStatus =
                    "GUI action queued: " + event.action + ".";
            }

            void advance_gameplay(
                const epochengine::core::Context* ctx,
                float frameSeconds,
                bool suppressPhysicalInput)
            {
                constexpr std::size_t actionCount =
                    static_cast<std::size_t>(
                        epochengine::project_input::ActionSemantic::count) - 1u;
                std::array<epochengine::project_input::ActionImpulse, actionCount>
                    guiImpulses{};
                std::size_t guiImpulseCount{};
                for (auto& pending : m_pendingGuiActions)
                {
                    if (pending)
                        guiImpulses[guiImpulseCount++] = *pending;
                    pending.reset();
                }

                if (!m_gameplayRuntime || !m_gameplayRuntime->active())
                    return;
                const auto* projectInput =
                    m_gameplayRuntime->input_profile();
                if (!projectInput)
                {
                    m_actorStatus =
                        "gameplay runtime has no admitted input profile";
                    return;
                }
                if (m_inputFrameIndex
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    m_actorStatus = "input sequence exhausted";
                    return;
                }

                ++m_inputFrameIndex;
                auto snapshot = project_input_snapshot(
                    *projectInput,
                    m_inputFrameIndex,
                    ctx,
                    !suppressPhysicalInput);
                const auto physicalControllers =
                    epochengine::controller_input::snapshot();
                if (suppressPhysicalInput)
                {
                    epochengine::project_input::InputSnapshot discarded{
                        .frame_index = m_inputFrameIndex};
                    const auto consumed = m_projectControllerSampler.sample(
                        *projectInput,
                        physicalControllers,
                        discarded);
                    if (!consumed)
                    {
                        m_actorStatus = std::string{"controller input "}
                            + std::string{epochengine::
                                project_input_controller::
                                    sample_code_name(consumed.code)};
                        return;
                    }
                }
                else
                {
                    const auto sampled = m_projectControllerSampler.sample(
                        *projectInput,
                        physicalControllers,
                        snapshot);
                    if (!sampled)
                    {
                        m_actorStatus = std::string{"controller input "}
                            + std::string{epochengine::
                                project_input_controller::
                                    sample_code_name(sampled.code)};
                        return;
                    }
                }

                const auto result = m_gameplayRuntime->advance(
                    snapshot,
                    std::span<
                        const epochengine::project_input::ActionImpulse>{
                            guiImpulses.data(), guiImpulseCount},
                    static_cast<double>(frameSeconds));
                m_actorStatus = std::string{epochengine::
                    project_gameplay2d::session_code_name(result.code)}
                    + ": " + std::string{m_gameplayRuntime->diagnostic()};
                if (result.audio_code == epochengine::audio::
                        PlaybackRuntimeCode::physical_queue_saturated)
                {
                    m_audioStatus =
                        "audio ready; physical queue applying backpressure";
                }
                else if (result.audio_code
                    != epochengine::audio::PlaybackRuntimeCode::success)
                {
                    m_audioStatus = std::string{"audio frame "}
                        + std::string{epochengine::audio::
                            playback_runtime_code_name(result.audio_code)};
                }
            }

            void retire_canvas2d_scenes() noexcept
            {
                for (const auto& [owner, generation] : m_canvas2dPublished)
                {
                    (void)generation;
                    (void)epochengine::canvas2d::scene_content::retire(owner);
                }
                m_canvas2dPublished.clear();
            }

            std::string m_projectId{};
            std::string m_projectName{};
            std::string m_projectRoot{};
            std::string m_tileMapPath{};
            std::string m_tileMapStatus{};
            std::string m_inputProfilePath{};
            std::string m_projectInputStatus{};
            std::string m_spriteAnimationPath{};
            std::string m_spriteAnimationStatus{};
            std::string m_audioProfilePath{};
            std::string m_audioStatus{};
            std::string m_actorStatus{};
            std::string m_projectGuiStatus{"Project GUI not prepared."};
            std::unique_ptr<epochengine::project_gui_runtime::RuntimeSession>
                m_projectGuiRuntime{};
            std::unique_ptr<epochengine::project_gui::EpochGuiAdapter>
                m_projectGuiAdapter{};
            std::string m_scenePath{};
            std::string m_worldName{};
            std::string m_scriptName{};
            std::string m_description{};
            epochengine::EditorProjectModelSummary m_modelSummary{};
            epochengine::scene_runtime::SceneRuntime m_sceneRuntime{};
            std::unique_ptr<
                epochengine::project_gameplay2d::Gameplay2DRuntime>
                m_gameplayRuntime{};
            std::optional<
                epochengine::canvas2d::scene_content::SceneContent>
                m_staticCanvasScene{};
            epochengine::project_input_controller::SnapshotSampler
                m_projectControllerSampler{};
            std::uint64_t m_inputFrameIndex{};
            std::optional<
                epochengine::project_gameplay2d::RuntimeCostSnapshot>
                m_runtimeCosts{};
            std::optional<
                epochengine::project_gameplay2d::RuntimeBudgetAssessment>
                m_runtimeBudgetAssessment{};
            epochengine::Budgets m_runtimeBudgets{};
            std::string m_runtimeBudgetProfile{"portable"};
            std::uint32_t m_costFramesUntilRefresh{};
            std::array<
                std::optional<epochengine::project_input::ActionImpulse>,
                static_cast<std::size_t>(
                    epochengine::project_input::ActionSemantic::count) - 1u>
                m_pendingGuiActions{};
            std::vector<ProjectRuntimeEntity> m_entities{};
            epochengine::lighting::LightingFrame m_lightingFrame{};
            timing::Clock::time_point m_lastFrame{};
            bool m_hasLastFrame{ false };
            epochengine::previewgrid::CameraMode m_cameraMode{ epochengine::previewgrid::CameraMode::Editor };
            input::ProfilePreset m_inputProfile{ input::ProfilePreset::EditorDefault };
            std::unordered_map<const void*, bool> m_cameraApplied{};
            std::unordered_map<const void*, std::uint64_t> m_canvas2dPublished{};
            PreviewLookState m_lookState{};
        };

