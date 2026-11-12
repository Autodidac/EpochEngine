        class ProjectPlayScene final : public epochengine::scene::Scene
        {
        public:
            explicit ProjectPlayScene(std::string_view project_payload)
            {
                const auto launch = parse_project_scene_launch(project_payload);
                m_cameraMode = launch.camera_mode;
                m_inputProfile = launch.input_profile;
                m_runtimeInputProfile = input::make_profile(m_inputProfile);

                const auto* profile = epochengine::editor_find_project_profile(launch.project_id);
                if (!profile)
                    profile = &epochengine::editor_default_project_profile();

                m_projectId = std::string(profile->id);
                m_editorRuntimeControlled=platform::child_process::claim_inherited_runtime_control(
                    m_projectId,profile->runtime_scene_id);
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
                std::vector<EditorSceneSeedEntity> sourceCameras;
                for(const auto& object:sourceSnapshot.objects)
                    if(object.type=="Camera")sourceCameras.push_back({object.name,object.type,object.category,
                        object.position,object.rotation,object.scale,object.visible,object.editor_only});
                m_seedCamerasUnchanged=editor_seed_cameras_unchanged(sourceCameras,seedEntities);
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
                    std::span<const ProjectRuntimeEntity>{ m_entities.data(), m_entities.size() },
                    sourceSnapshot.ambient,&m_lightingManager);
            }

            void load() override
            {
                Scene::load();
                m_runtimeCloudsVisible=m_runtimeWaterVisible=true;
                prepare_project_meshes();
                prepare_planetary_runtime();
                prepare_gameplay();
                prepare_runtime_input();
                const auto* projection = m_sceneRuntime.projection();
                m_reachObjective.reset(projection ? projection->source().reach_objective : std::nullopt);
                m_reachTargetName.clear();
                if (const auto* target = reach_objective_target())
                    m_reachTargetName = target->name;
            }

            void unload() override
            {
                shutdown_planetary_runtime();
                m_runtimeInspectionLight=false;
                shutdown_gameplay();
                m_reachObjective.reset();
                m_reachTargetName.clear();
                retire_canvas2d_scenes();
                m_cameraApplied.clear();
                m_meshes.clear();
                m_meshBaseColorTextures.clear();
                m_waterCaches.clear();
                m_runtimeFrameLimitEdits.clear();
                m_hasLastFrame = false;
                Scene::unload();
            }

            ~ProjectPlayScene() override
            {
                shutdown_planetary_runtime();
                shutdown_gameplay();
                retire_canvas2d_scenes();
            }

            bool frame(std::shared_ptr<epochengine::core::Context> ctx, epochengine::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                const bool measureFrame=m_runtimeFrameEvidence.reports<24u;
                const auto frameStarted=measureFrame?timing::Clock::now():timing::Clock::time_point{};
                const double frameIntervalMilliseconds=measureFrame&&m_hasLastFrame
                    ?std::chrono::duration<double,std::milli>(frameStarted-m_lastFrameEntry).count():0.0;
                if(measureFrame)m_lastFrameEntry=frameStarted;
                // In-editor play always returns on physical Escape, even if
                // the project remaps its Cancel action to another key.
                if (ctx->action_pressed_safe(input::Action::Cancel, m_runtimeInputProfile)
                    || ctx->is_key_down_safe(input::Key::Escape))
                {
                    shutdown_planetary_runtime();
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
                    (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    m_canvas2dPublished.erase(ctx.get());
                    m_waterCaches.erase(ctx.get());
                    return false;
                }

                const auto now = timing::Clock::now();
                std::array<double,6> framePhaseMilliseconds{};
                auto phaseStarted=frameStarted;
                const auto finishPhase=[&](std::size_t phase) {
                    if(!measureFrame)return;
                    const auto finished=timing::Clock::now();
                    framePhaseMilliseconds[phase]+=std::chrono::duration<double,std::milli>(finished-phaseStarted).count();
                    phaseStarted=finished;
                };
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
                m_runtimeViewportHeight=static_cast<float>(height);
                m_runtimeViewportWidth=static_cast<float>(width);
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
                update_runtime_view_shortcuts(ctx.get(), projectGuiCapturesKeyboard);
                if(m_editorRuntimeControlled&&!m_ownedRuntimeMeshCommand)
                    if(const auto command=platform::child_process::poll_runtime_mesh_command())
                    {m_ownedRuntimeMeshCommand=*command;m_requestedMeshWireframe=command->mode==platform::child_process::MeshDrawMode::wireframe;}
                if(m_editorRuntimeControlled)
                {
                    if(const auto command=platform::child_process::poll_runtime_presentation_command())
                    {
                        const bool accepted=apply_runtime_presentation(command->sample,ctx.get());
                        platform::child_process::acknowledge_runtime_presentation_command(*command,accepted);
                    }
                    advance_runtime_presentation(ctx.get());
                }
                sample_runtime_input(ctx.get(), projectGuiCapturesKeyboard
                    || gui::keyboard_input_captured() || !ctx->has_input_focus_safe()
                    || runtime_panel_captures_pointer(mouse_pos));
                advance_gameplay(dt);

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
                        if (const auto* projection = m_sceneRuntime.projection())
                            focus_project_spawn(ctx.get(), *projection);
                    }
                    else if(m_cameraMode==previewgrid::CameraMode::Editor&&m_seedCamerasUnchanged&&m_hasPlanetRecipe)
                    {
                        const ProjectRuntimeEntity* planet{};
                        std::size_t planets{};
                        for(const auto& entity:m_entities)
                            if(entity.visible()&&entity.category=="Planetary Terrain"&&entity.mesh_asset)
                            {++planets;planet=&entity;}
                        if(planets==1u)
                        {
                            const auto mesh=m_meshes.find(planet->source_id);
                            if(mesh!=m_meshes.end()&&mesh->second)
                            {
                                const auto& transform=planet->transform;
                                const auto meshScale=scene::mesh_render_scale(transform.scale,planet->mesh_asset);
                                if(!meshScale) return false;
                                (void)previewgrid::frame_initial_orbit_mesh(ctx.get(),
                                    previewgrid::ObjectMarker{
                                        .position={transform.position[0],transform.position[1],transform.position[2]},
                                        .scale={(*meshScale)[0],(*meshScale)[1],(*meshScale)[2]},
                                        .rotationDegrees={transform.rotation[0],transform.rotation[1],transform.rotation[2]},
                                        .primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=mesh->second},
                                    static_cast<float>(width)/static_cast<float>(height));
                            }
                        }
                    }
                    m_cameraApplied[ctx.get()] = true;
                }
                ctx->set_scene_viewport({ 0, 0, width, height });
                finishPhase(0u); // Input, gameplay and frame setup.
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
                    finishPhase(0u);
                    advance_planetary_exploration(ctx.get(),dt,mouse_pos,mouse_right_down,
                        projectGuiCapturesPointer,projectGuiCapturesKeyboard);
                    finishPhase(1u);
                    pump_planetary_runtime(ctx.get(), width, height);
                    pump_planetary_water();
                    finishPhase(2u);
                    auto planetaryMarkers=planetary_exploration_markers();
                    const bool requestedWireframe=m_requestedMeshWireframe.value_or(m_meshWireframe);
                    auto sceneOrigin=planetary_scene_origin_offset();
                    const bool rebased=m_explorationRenderCamera.has_value();
                    bool completeManagedFrame=!rebased||(m_explorationPreview
                        &&m_explorationPreview->body_count>0u&&planetaryMarkers.size()==m_explorationPreview->body_count);
                    std::vector<previewgrid::CloudVolume> cloudVolumes;
                    if(completeManagedFrame)
                        completeManagedFrame=append_planetary_cloud_markers(planetaryMarkers,cloudVolumes,
                            ctx->type==core::ContextType::OpenGL,
                            ctx->simulation_sample_time().value_or(0.0));
                    std::vector<std::uint64_t> managedTerrainAttachments;
                    if(rebased&&completeManagedFrame)
                        m_terrainAttachmentStatus=append_planetary_attached_markers(planetaryMarkers,managedTerrainAttachments,
                            ctx->type == core::ContextType::OpenGL);
                    else m_terrainAttachmentStatus.clear();
                    if(rebased)for(std::size_t axis=0;axis<3;++axis)sceneOrigin[axis]-=m_explorationRenderOrigin[axis];
                    update_planetary_sunlight();
                    auto frameLighting=project_play_lighting_for_origin(m_lightingFrame,sceneOrigin);
                    if(frameLighting&&m_runtimeInspectionLight)
                        apply_runtime_inspection_light(*frameLighting);
                    auto& waterCache=m_waterCaches[ctx.get()];
                    if(m_runtimeWaterVisible)
                        (void)sample_project_play_water(m_entities,ctx->simulation_sample_time(),waterCache);
                    else waterCache={};
                    const bool published=completeManagedFrame&&frameLighting&&publish_project_play_markers(ctx.get(), std::span<const ProjectRuntimeEntity>{
                        m_entities.data(),
                        m_entities.size()
                    }, &m_meshes, m_meshFrameLimits, planetaryMarkers,
                        rebased&&!planetaryMarkers.empty() ? m_planetaryEntity : 0u, requestedWireframe,
                        sceneOrigin, &m_meshBaseColorTextures, waterCache.mesh,
                        rebased?&*m_explorationRenderCamera:nullptr,&*frameLighting,
                        m_explorationRenderOrigin,managedTerrainAttachments,cloudVolumes);
                    const bool preparing=rebased&&planetary_frame_preparing(ctx.get());
                    if(published||!preparing)finish_runtime_mesh_display(published,requestedWireframe);
                    if (published)
                        update_reach_objective(ctx.get(), sceneOrigin, planetaryMarkers, managedTerrainAttachments);
                    if(m_runtimeCloudBackendUnavailable)
                        m_runtimeViewStatus="Volumetric clouds currently require OpenGL; this backend has no density pass.";
                    if(!published&&rebased&&previewgrid::advance_scene_frame_camera(ctx.get(),
                        *m_explorationRenderCamera,m_explorationRenderOrigin,std::span<const previewgrid::CloudVolume>{cloudVolumes}))
                        m_runtimeViewStatus=preparing
                            ?"Preparing terrain display; camera controls remain available."
                            :"New geometry was not admitted; the previous terrain remains visible with live camera controls.";
                    if(!completeManagedFrame)
                        m_runtimeViewStatus="Planet exploration frame is incomplete; the previous view is retained.";
                    else if (published && ctx->type != core::ContextType::OpenGL
                        && std::any_of(m_entities.begin(),m_entities.end(),[](const auto& entity)
                        {
                            return project_play_entity_has_geometry(entity,0u) && entity.mesh_asset
                                && entity.texture_material && entity.type != "ForestTrunk";
                        }))
                        m_runtimeViewStatus = "Textured objects are hidden on this backend. Run with OpenGL to view them.";
                    else if (!published && ctx->type == core::ContextType::OpenGL)
                    {
                        const auto needsTexture = [](const auto& entity)
                        { return entity.visible() && entity.mesh_asset && entity.texture_material; };
                        if (std::any_of(m_entities.begin(),m_entities.end(),[&](const auto& entity)
                            { return needsTexture(entity) && !m_meshBaseColorTextures.contains(entity.source_id); }))
                            m_runtimeViewStatus = "Textured mesh drawing refused: its exact opaque/cutout base-color texture is unavailable.";
                    }
                    if(!rebased&&frameLighting)
                        previewgrid::set_lighting_frame(ctx.get(),std::move(*frameLighting));
                }
                finishPhase(3u);

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                // The controls' scroll area owns wheel input over its panel.
                // Draining it here made lower planet/flight controls unreachable.
                const bool navigationPointerCaptured = projectGuiCapturesPointer
                    || runtime_panel_captures_pointer(mouse_pos);
                const int wheelDelta = consume_runtime_navigation_wheel(
                    mouse_pos, projectGuiCapturesPointer);
                if (!m_explorationController)
                {
                if (ctx->action_pressed_safe(input::Action::ResetCamera, m_runtimeInputProfile))
                    epochengine::previewgrid::reset_camera(ctx.get());

                const bool altHeld = ctx->is_key_held_safe(input::Key::LeftAlt)
                    || ctx->is_key_held_safe(input::Key::RightAlt);
                const bool shiftHeld = ctx->is_key_held_safe(input::Key::LeftShift)
                    || ctx->is_key_held_safe(input::Key::RightShift);
                const auto navigationGestures =
                    epochengine::previewgrid::resolve_camera_navigation_gestures(
                        altHeld, mouse_left_down, mouse_middle_down, mouse_right_down);
                const bool orbiting = !navigationPointerCaptured && navigationGestures.orbiting;
                const bool panning = !navigationPointerCaptured && navigationGestures.panning;
                const bool dollying = !navigationPointerCaptured && navigationGestures.dollying;
                const bool flying = !navigationPointerCaptured && navigationGestures.flying;
                const bool keyboardNavigation = !projectGuiCapturesKeyboard
                    && !gui::keyboard_input_captured() && ctx->has_input_focus_safe()
                    && !navigationPointerCaptured && runtime_input_ready()
                    && (m_cameraMode == previewgrid::CameraMode::FPS || flying);
                const float navigationMultiplier = shiftHeld ? 4.0f : 1.0f;

                const float forwardInput = !keyboardNavigation ? 0.0f :
                    runtime_planar_axis(project_input::ActionSemantic::move_y,
                        (ctx->action_held_safe(input::Action::MoveForward, m_runtimeInputProfile) ? 1.0f : 0.0f)
                        - (ctx->action_held_safe(input::Action::MoveBackward, m_runtimeInputProfile) ? 1.0f : 0.0f));
                const float rightInput = !keyboardNavigation ? 0.0f :
                    runtime_planar_axis(project_input::ActionSemantic::move_x,
                        (ctx->action_held_safe(input::Action::MoveRight, m_runtimeInputProfile) ? 1.0f : 0.0f)
                        - (ctx->action_held_safe(input::Action::MoveLeft, m_runtimeInputProfile) ? 1.0f : 0.0f));
                const float upInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::MoveUp, m_runtimeInputProfile) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::MoveDown, m_runtimeInputProfile) ? 1.0f : 0.0f);
                const float yawInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::LookRight, m_runtimeInputProfile) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::LookLeft, m_runtimeInputProfile) ? 1.0f : 0.0f);
                const float pitchInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::LookUp, m_runtimeInputProfile) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::LookDown, m_runtimeInputProfile) ? 1.0f : 0.0f);
                const float rollInput = !keyboardNavigation ? 0.0f :
                    (ctx->action_held_safe(input::Action::RollRight, m_runtimeInputProfile) ? 1.0f : 0.0f)
                    - (ctx->action_held_safe(input::Action::RollLeft, m_runtimeInputProfile) ? 1.0f : 0.0f);


                if ((flying && m_lookState.flying) || (orbiting && m_lookState.orbiting))
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    const float sensitivity = m_runtimeInputProfile.mouse_look_sensitivity;
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


                if (wheelDelta != 0)
                {
                    const float wheelSteps = static_cast<float>(wheelDelta) / 120.0f;
                    if (flying)
                        epochengine::previewgrid::adjust_fly_speed(ctx.get(), wheelSteps);
                    else
                        epochengine::previewgrid::zoom_camera(
                            ctx.get(), wheelSteps * m_runtimeInputProfile.wheel_zoom_step);
                }

                epochengine::previewgrid::step_camera(
                    ctx.get(),
                    dt,
                    forwardInput,
                    rightInput,
                    upInput,
                    yawInput,
                    pitchInput,
                    navigationMultiplier, rollInput);

                m_lookState.last_mouse = mouse_pos;
                m_lookState.orbiting = orbiting;
                m_lookState.panning = panning;
                m_lookState.dollying = dollying;
                m_lookState.flying = flying;
                }

                bool returnToEditor = false;
                if (m_projectGuiRuntime && m_projectGuiAdapter)
                {
                    // The visible controls are above the project's own GUI.
                    // Paint both, but let only the top surface consume input.
                    gui::ScopedInputSuppression controlsOwnInput{
                        runtime_panel_captures_pointer(mouse_pos)};
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
                if (!m_runtimeControlsVisible)
                    draw_runtime_controls_restore();
                else
                {
                    const auto controls = runtime_panel_bounds();
                    const float controlsContentWidth = (std::max)(1.0f, controls.size.x - 52.0f);
                    gui::begin_window(
                        "Project Controls",
                        controls.position,
                        controls.size);
                    {
                        gui::ContentClipScope controlsClip(controls.position,
                            {controls.position.x+controls.size.x,controls.position.y+controls.size.y});
                        draw_runtime_controls_hide(controlsContentWidth);
                        draw_runtime_frame_limit(ctx, controlsContentWidth);
                        const float controlsBodyHeight=(std::max)(1.0f,
                            controls.position.y+controls.size.y-gui::cursor_position().y-8.0f);
                        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                            .id="project-runtime-controls", .size={controlsContentWidth+12.0f,controlsBodyHeight},
                            .content_height=0.0f,.show_scrollbar=true});
                        const auto runtimeLabel=[](std::string_view text,float width,std::string_view lineBuffer={})
                        {
                            const auto start=gui::cursor_position();
                            gui::wrapped_label(text,width);
                            // Clipped drawing can stop before the last line. Keep
                            // the full label in the shared scroll area's extent.
                            // Reserve measured multiline space for changing values,
                            // including empty statuses. Longer diagnostics still fit.
                            const float textHeight=gui::measure_wrapped_label_height(text,width);
                            const float bufferHeight=lineBuffer.empty()?0.0f:gui::measure_wrapped_label_height(lineBuffer,width);
                            gui::set_cursor({start.x,start.y+(std::max)(textHeight,bufferHeight)});
                        };
                        draw_runtime_environment_controls(controlsContentWidth);
                        draw_planetary_exploration_controls(controlsContentWidth);
                        const std::array lightingActions{
                            gui::InlineButtonSpec{.label=m_runtimeInspectionLight?"Inspection Light: On":"Inspection Light: Off",.width=0.0f}};
                        if(gui::inline_button_row(lightingActions,24.0f,6.0f,
                            {.sizing=gui::InlineButtonSizing::FitContent,.alignment=gui::InlineButtonAlignment::Center,
                             .wrap=true,.available_width=controlsContentWidth}))
                        {
                            m_runtimeInspectionLight=!m_runtimeInspectionLight;
                        }
                        const std::array viewActions{
                            gui::InlineButtonSpec{.label="Copy Screenshot [F12]", .width=0.0f}};
                        if(gui::inline_button_row(viewActions,28.0f,6.0f,
                            {.sizing=gui::InlineButtonSizing::FitContent,
                             .alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=controlsContentWidth}))
                            m_screenshotAfterFrames=2;
                        if (m_planetarySettings)
                        {
                            const std::array actions{
                                gui::InlineButtonSpec{.label=m_planetaryPaused ? "Resume Terrain LOD" : "Pause Terrain LOD",
                                    .width=180.0f, .enabled=m_planetaryEntity!=0u},
                                gui::InlineButtonSpec{.label="Retry Terrain LOD", .width=180.0f,
                                    .enabled=m_planetaryFailed && m_planetaryEntity!=0u}};
                            if(const auto action=gui::inline_button_row(actions,28.0f,6.0f,
                                {.sizing=gui::InlineButtonSizing::FitContent,
                                 .alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=controlsContentWidth}))
                            {
                                if(*action==0u) {m_planetaryPaused=!m_planetaryPaused;if(!m_planetaryPaused)m_planetaryFailed=false;}
                                else {m_planetaryFailed=false;m_planetaryPaused=false;}
                                if(m_planetaryPaused && m_planetaryScheduler && m_planetaryPending)
                                    (void)m_planetaryScheduler->cancel(m_planetaryCancellation);
                                m_planetaryContinue=true;
                                m_planetaryNextTick={};
                                m_planetaryStatus=m_planetaryPaused
                                    ? "Terrain LOD paused. The current terrain stays visible."
                                    : "Terrain LOD queued for the current camera.";
                            }
                        }
                        // Live diagnostics follow the actions: wrapping timing or
                        // progress text must not move the controls under a pointer.
                        gui::section_heading(m_projectName);
                        if(m_hasPlanetRecipe||!m_explorationAdvanceStatus.empty()||!m_explorationStatus.empty())
                        {
                            runtimeLabel(m_explorationAdvanceStatus,controlsContentWidth,
                                "Planet exploration provider is unavailable; the last camera is retained.");
                            runtimeLabel(m_explorationStatus,controlsContentWidth,
                                "Planet exploration needs object Scale 1, 1, 1. Use World Size / Radius to resize it while preserving walking speed and distances.");
                        }
                        if(m_explorationController)
                        {
                            std::string anchorName,targetName;
                            for(std::uint32_t i=0;i<m_explorationFrame.body_count;++i)
                            {const auto& body=m_explorationFrame.bodies[i];if(body.id==m_explorationFrame.anchor)anchorName=body.name;if(body.id==m_explorationFrame.target)targetName=body.name;}
                            const auto mode=m_explorationFrame.mode==project_planetary_runtime::TravelMode::walk?"Walk":
                                m_explorationFrame.mode==project_planetary_runtime::TravelMode::cruise?"Cruise":"Flight";
                            runtimeLabel("Anchor: "+anchorName+" | Target: "+targetName+" | "+mode,controlsContentWidth,
                                "Anchor: "+anchorName+" | Target: "+targetName+" | Flight");
                            runtimeLabel(epochengine::format_text("Altitude {:.1f} m  |  Speed {:.1f} m/s",
                                m_explorationFrame.altitude,m_explorationFrame.speed),controlsContentWidth,
                                "Altitude -999999999999.9 m  |  Speed 999999999999.9 m/s");
                            runtimeLabel(epochengine::format_text("Time {:.1f} s  /  {:g}x",
                                m_explorationFrame.seconds,m_explorationFrame.time_scale),controlsContentWidth,
                                "Time 999999999999.9 s  /  999999999999x");
                            runtimeLabel("Flight: WASD | Roll: Q/E | Up/down: Space/Ctrl | Shift: boost. Walk keeps ground alignment.",controlsContentWidth);
                        }
                        runtimeLabel(m_runtimeViewStatus,controlsContentWidth,
                            "Scene geometry was not admitted. The previous mesh display is retained; check the terrain and asset diagnostics.");
                        if(m_hasPlanetRecipe||!m_terrainAttachmentStatus.empty())runtimeLabel(m_terrainAttachmentStatus,controlsContentWidth,
                            "Forest area no longer matches terrain; regenerate/repaint to attach it. Authored poses are retained.");
                        constexpr std::string_view inspectionStatus=
                            "Inspection Light brightens this view only. Scene lights and planetary time are unchanged.";
                        runtimeLabel(m_runtimeInspectionLight?inspectionStatus:std::string_view{},controlsContentWidth,inspectionStatus);
                        runtimeLabel(m_screenshotStatus,controlsContentWidth,
                            "The screenshot could not be copied. The clipboard is busy; try Copy Screenshot again.");
                        const auto water=m_waterCaches.find(ctx.get());
                        runtimeLabel(water!=m_waterCaches.end()?std::string_view{water->second.diagnostic}:std::string_view{},controlsContentWidth,
                            "Water could not update. The previous surface is retained; check the terrain and asset diagnostics.");
                        if(m_planetarySettings)
                        {
                            runtimeLabel(runtime_terrain_activity(),controlsContentWidth,"Terrain: refining (request in progress)");
                            runtimeLabel(m_planetaryStatus,controlsContentWidth,
                                "Planet LOD 0–24 | 999999 visible patches | 999999 loading | patch cache 999999.9 / 999999 MiB\nLast LOD build: 4/4 workers, 3600000 ms provider");
                            runtimeLabel(m_planetaryWaterStatus,controlsContentWidth,
                                "Ocean: waiting for updated terrain. Review the Planetary source update, then Build & Generate.");
                        }
                        if (!m_inputProfilePath.empty())
                        {
                            gui::label(std::string("Input Profile: ")
                                + m_inputProfilePath);
                            runtimeLabel(
                                std::string("Input Runtime: ")
                                    + (m_projectInputStatus.empty()
                                        ? std::string("not prepared")
                                        : m_projectInputStatus),
                                controlsContentWidth);
                        }
                        if (!m_tileMapPath.empty())
                        {
                            gui::label(std::string("Tile Map: ") + m_tileMapPath);
                            runtimeLabel(
                                std::string("Tile Runtime: ")
                                    + (m_tileMapStatus.empty()
                                        ? std::string("not prepared")
                                        : m_tileMapStatus),
                                controlsContentWidth);
                            if (!m_spriteAnimationPath.empty())
                            {
                                gui::label(std::string("Sprite Animations: ")
                                    + m_spriteAnimationPath);
                                runtimeLabel(
                                    std::string("Animation Runtime: ")
                                        + (m_spriteAnimationStatus.empty()
                                            ? std::string("not prepared")
                                            : m_spriteAnimationStatus),
                                    controlsContentWidth);
                            }
                            runtimeLabel(
                                std::string{"Gameplay: "}
                                    + (m_actorStatus.empty()
                                        ? std::string{"not prepared"}
                                        : m_actorStatus),
                                controlsContentWidth);
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
                                    runtimeLabel(
                                        std::string{"Cost Status: "}
                                            + costs.diagnostic,
                                        controlsContentWidth);
                                    if (m_runtimeBudgetAssessment)
                                    {
                                        runtimeLabel(
                                            std::string{"Tier Budget ("}
                                                + m_runtimeBudgetProfile + "): "
                                                + m_runtimeBudgetAssessment->diagnostic,
                                            controlsContentWidth);
                                    }
                                }
                                runtimeLabel(
                                    std::string("Audio: ")
                                        + (m_audioStatus.empty()
                                            ? std::string("not active")
                                            : m_audioStatus),
                                    controlsContentWidth);
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
                        runtimeLabel(
                            std::string("Demo model: ")
                            + (m_modelSummary.asset_path.empty() ? std::string("(none)") : m_modelSummary.asset_path),
                            controlsContentWidth);
                        runtimeLabel(
                            std::string("Model summary: ")
                            + (m_modelSummary.summary.empty() ? std::string("(unavailable)") : m_modelSummary.summary),
                            controlsContentWidth);
                        runtimeLabel(m_description, controlsContentWidth);
                        // Paths and loader evidence remain available after the actual
                        // play/terrain controls instead of pushing those off-screen.
                        gui::section_heading("Project Details");
                        runtimeLabel(std::string("World: ") + m_worldName,controlsContentWidth);
                        runtimeLabel(std::string("Scene: ") + m_scenePath,controlsContentWidth);
                        runtimeLabel(std::string("Script: ") + m_scriptName,controlsContentWidth);
                        runtimeLabel(m_projectGuiStatus,controlsContentWidth);
                        gui::label(std::string("Objects: ") + std::to_string(visible_runtime_entity_count(m_entities)));
                        gui::end_scroll_area();
                    }
                    gui::end_window();
                }
                draw_reach_objective();
                gui::end_top_layer();

                gui::end_frame();
                finishPhase(4u);
                ctx->present_safe();
                finish_runtime_screenshot(ctx.get());
                finishPhase(5u);
                if(measureFrame)
                    record_runtime_frame_evidence(frameStarted,frameIntervalMilliseconds,framePhaseMilliseconds,mouse_pos,
                        m_explorationController?m_explorationMouseHeld:
                            (m_lookState.orbiting||m_lookState.panning||m_lookState.dollying||m_lookState.flying),
                        m_planetaryPending.has_value(),planetary_frame_preparing(ctx.get()));
                if(returnToEditor) shutdown_planetary_runtime();
                return !returnToEditor;
            }

        private:
            struct RuntimeFrameEvidence final
            {
                timing::Clock::time_point began{};
                std::array<double,6> phase_maximum{};
                double interval_sum{},interval_maximum{},work_sum{},work_maximum{},mouse_delta_maximum{};
                double previous_log_milliseconds{};
                std::uint32_t frames{},intervals{},moving_frames{},looking_frames{},lod_pending_frames{},display_pending_frames{},reports{};
                gui::Vec2 last_mouse{};
                bool has_mouse{},slowest_moving{},slowest_looking{},longest_interval_moving{},longest_interval_looking{};
            };
            void record_runtime_frame_evidence(timing::Clock::time_point started,double interval,
                const std::array<double,6>& phases,gui::Vec2 mouse,bool looking,bool lodPending,bool displayPending) noexcept
            {
                auto& evidence=m_runtimeFrameEvidence;
                if(evidence.reports>=24u)return;
                if(evidence.frames==0u)evidence.began=started;
                const double delta=evidence.has_mouse?std::hypot(double(mouse.x-evidence.last_mouse.x),
                    double(mouse.y-evidence.last_mouse.y)):0.0;
                evidence.last_mouse=mouse;evidence.has_mouse=true;
                const bool moving=delta>0.0;
                ++evidence.frames;
                if(interval>0.0&&std::isfinite(interval))
                {
                    ++evidence.intervals;evidence.interval_sum+=interval;
                    if(interval>evidence.interval_maximum)
                    {evidence.interval_maximum=interval;evidence.longest_interval_moving=moving;evidence.longest_interval_looking=looking;}
                }
                evidence.moving_frames+=moving;evidence.looking_frames+=looking;
                evidence.lod_pending_frames+=lodPending;evidence.display_pending_frames+=displayPending;
                evidence.mouse_delta_maximum=(std::max)(evidence.mouse_delta_maximum,delta);
                double work{};
                for(std::size_t phase=0u;phase<phases.size();++phase)
                {work+=phases[phase];evidence.phase_maximum[phase]=(std::max)(evidence.phase_maximum[phase],phases[phase]);}
                evidence.work_sum+=work;
                if(work>evidence.work_maximum)
                {evidence.work_maximum=work;evidence.slowest_moving=moving;evidence.slowest_looking=looking;}
                const auto finished=timing::Clock::now();
                if(finished-evidence.began<std::chrono::seconds{5})return;
                // Keep the bounded evidence budget for interaction or stalls;
                // an idle project must not exhaust it before the flight test.
                const bool report=evidence.reports==0u||evidence.moving_frames||evidence.looking_frames
                    ||evidence.interval_maximum>=50.0||evidence.work_maximum>=50.0;
                if(report)try
                {
                    // Owner work is elapsed time inside this update, including any
                    // waits. It is distinct from renderer FPS and OS CPU accounting.
                    auto& log=logger::get("Engine.Project");
                    log.logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene owner #{}: {} turns; interval avg/max {:.2f}/{:.2f} ms; work avg/max {:.2f}/{:.2f} ms; prior log {:.2f} ms.",
                        evidence.reports+1u,evidence.frames,evidence.interval_sum/(std::max)(1u,evidence.intervals),
                        evidence.interval_maximum,evidence.work_sum/evidence.frames,evidence.work_maximum,evidence.previous_log_milliseconds);
                    log.logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene owner phase max ms: input/gameplay {:.2f}, exploration {:.2f}, LOD/water {:.2f}, display {:.2f}, GUI/navigation {:.2f}, present {:.2f}.",
                        evidence.phase_maximum[0],evidence.phase_maximum[1],evidence.phase_maximum[2],
                        evidence.phase_maximum[3],evidence.phase_maximum[4],evidence.phase_maximum[5]);
                    log.logf(logger::LogLevel::INFO,std::source_location::current(),
                        "Scene owner input: moved/look {}/{}; max delta {:.1f}px; peak work moved/look={}/{}; peak interval moved/look={}/{}; pending LOD/display={}/{}.",
                        evidence.moving_frames,evidence.looking_frames,evidence.mouse_delta_maximum,
                        evidence.slowest_moving,evidence.slowest_looking,evidence.longest_interval_moving,
                        evidence.longest_interval_looking,evidence.lod_pending_frames,evidence.display_pending_frames);
                }
                catch(...){} // Evidence must not interrupt input or publication.
                const double logMilliseconds=report
                    ?std::chrono::duration<double,std::milli>(timing::Clock::now()-finished).count():evidence.previous_log_milliseconds;
                const auto reports=evidence.reports+static_cast<std::uint32_t>(report);
                evidence={};evidence.reports=reports;evidence.began=finished;
                evidence.previous_log_milliseconds=logMilliseconds;
                evidence.last_mouse=mouse;evidence.has_mouse=true;
            }
            static void apply_runtime_inspection_light(lighting::LightingFrame& frame) noexcept
            {
                // This is a transient inspection override, never authored light
                // or simulation state. The next normal frame restores the sun.
                auto& ambient=frame.environment.ambient;
                ambient.r=(std::max)(ambient.r,0.6f);
                ambient.g=(std::max)(ambient.g,0.6f);
                ambient.b=(std::max)(ambient.b,0.6f);
            }
            [[nodiscard]] std::string_view runtime_terrain_activity() const noexcept
            {
                if(m_planetaryFailed)return "Terrain: needs attention";
                if(m_planetaryPaused)return m_planetaryPending?"Terrain: stopping refinement":"Terrain: paused";
                if(m_planetaryPending)return "Terrain: refining (request in progress)";
                if(m_planetaryContinue)return "Terrain: refinement queued";
                return "Terrain: ready";
            }
            [[nodiscard]] const ProjectRuntimeEntity* reach_objective_target() const
            {
                const auto& objective = m_reachObjective.objective();
                const auto* projection = m_sceneRuntime.projection();
                const auto* entity = objective && projection ? projection->find(objective->target) : nullptr;
                return entity && entity->mesh_asset && project_play_entity_has_geometry(*entity, 0u) ? entity : nullptr;
            }

            void update_reach_objective(const void* context, const std::array<double, 3>& sceneOrigin,
                std::span<const previewgrid::ObjectMarker> planetaryMarkers,
                std::span<const std::uint64_t> managed)
            {
                if (!m_reachObjective.objective() || m_reachObjective.completed()) return;
                const auto* target = reach_objective_target();
                std::optional<std::array<double, 3>> player{}, point{};
                if (target && m_meshes.contains(target->source_id))
                {
                    if (m_explorationController)
                    {
                        if (m_explorationRenderCamera && m_explorationAdvanceStatus.empty()
                            && m_explorationFrame.mode == project_planetary_runtime::TravelMode::walk)
                        {
                            player = previewgrid::camera_world_eye(*m_explorationRenderCamera);
                            if (target->terrain_attachment)
                            {
                                // The managed objects occupy the tail in managed-ID order.
                                // Use the exact published rebased transform for the objective.
                                const auto found = std::find(managed.begin(), managed.end(), target->source_id);
                                if (found != managed.end() && managed.size() <= planetaryMarkers.size())
                                {
                                    const auto& marker = planetaryMarkers[planetaryMarkers.size() - managed.size()
                                        + static_cast<std::size_t>(found - managed.begin())];
                                    if (marker.preciseTransform)
                                    {
                                        const auto& transform = *marker.preciseTransform;
                                        point = transform.position;
                                        if (player && target->terrain_attachment->solid_walk_box && marker.mesh)
                                        {
                                            const auto low = marker.mesh->minimum(), high = marker.mesh->maximum();
                                            std::array<double, 3> closest{};
                                            for (std::size_t axis = 0; axis < closest.size(); ++axis)
                                            {
                                                double projected{};
                                                for (std::size_t row = 0; row < closest.size(); ++row)
                                                    projected += transform.rotation[row * 3 + axis] * ((*player)[row] - transform.position[row]);
                                                closest[axis] = std::clamp(projected / transform.scale[axis], double(low[axis]), double(high[axis]));
                                            }
                                            point = transform.apply(closest);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                point = scene::world_position(target->transform.position, target->position_anchor);
                                if (point)
                                    for (std::size_t axis = 0; axis < point->size(); ++axis)
                                        (*point)[axis] += sceneOrigin[axis];
                            }
                        }
                    }
                    else if (m_cameraMode == previewgrid::CameraMode::FPS && (!target->terrain_attachment || !m_hasPlanetRecipe))
                    {
                        player = previewgrid::camera_world_eye(previewgrid::camera_for(context));
                        point = scene::world_position(target->transform.position, target->position_anchor);
                    }
                }
                if (m_reachObjective.sample(player, point))
                {
                    const auto message = "Reach objective complete: " + m_reachTargetName;
                    core::log::info("Engine.Project.Play", epochengine::string_view{message.data(), message.size()});
                }
            }

            void draw_reach_objective() const
            {
                if (!m_reachObjective.objective()) return;
                std::string text;
                if (m_reachTargetName.empty())
                    text = "Objective target unavailable";
                else if (m_reachObjective.completed())
                    text = "Objective complete · " + m_reachTargetName;
                else
                {
                    text = "Reach " + m_reachTargetName;
                    if (const auto distance = m_reachObjective.distance(); distance)
                        text += epochengine::format_text("\n{:.1f} m away", *distance);
                    else if (m_explorationController && m_explorationFrame.mode != project_planetary_runtime::TravelMode::walk)
                        text += "\nLand and switch to Walk";
                }
                const float width = (std::max)(1.0f, (std::min)(400.0f, m_runtimeViewportWidth - 32.0f));
                const float textWidth = (std::max)(1.0f, width - 20.0f);
                const float height = gui::measure_wrapped_label_height(text, textWidth) + 16.0f;
                const gui::Vec2 origin{(std::max)(0.0f, m_runtimeViewportWidth - width - 16.0f),
                    (std::max)(0.0f, m_runtimeViewportHeight - height - 16.0f)};
                const auto cursor = gui::cursor_position();
                gui::panel_rect(origin, {width, height});
                gui::set_cursor({origin.x + 10.0f, origin.y + 8.0f});
                gui::wrapped_label(text, textWidth);
                gui::set_cursor(cursor);
            }

            static void focus_project_spawn(const void* context,
                const epochengine::scene_runtime::CompiledSceneProjection& projection) noexcept
            {
                const auto* spawn = projection.resolve(projection.primary().spawn);
                if (!spawn)
                    return;

                constexpr float eyeHeight = 1.65f;
                const auto& position = spawn->transform.position;
                if (spawn->position_anchor)
                {
                    if (auto eye = scene::world_position(position, spawn->position_anchor))
                    {
                        (*eye)[1] += static_cast<double>(eyeHeight);
                        (void)previewgrid::focus_camera_precise(context, *eye, 0.0f);
                    }
                    return;
                }
                (void)previewgrid::focus_camera(context,
                    {position[0], position[1] + eyeHeight, position[2]}, 0.0f);
            }
            [[nodiscard]] static std::optional<epochengine::lighting::LightingFrame> project_play_lighting_for_origin(
                epochengine::lighting::LightingFrame frame,const std::array<double,3>& origin_offset)
            {
                for(auto& light:frame.lights)
                {
                    if(light.desc.kind==lighting::LightKind::Directional)continue;
                    const auto position=project_play_position_for_origin(
                        {light.desc.position.x,light.desc.position.y,light.desc.position.z},origin_offset);
                    if(!position)return {};
                    light.desc.position={position->x,position->y,position->z};
                }
                return frame;
            }
            void finish_runtime_mesh_display(bool published,bool requestedWireframe)
            {
                if(published)
                {
                    m_meshWireframe=requestedWireframe;
                    m_runtimeViewStatus.clear();
                }
                else
                    m_runtimeViewStatus="Scene geometry was not admitted. The previous mesh display is retained; check the terrain and asset diagnostics.";
                m_requestedMeshWireframe.reset();
                if(m_ownedRuntimeMeshCommand)
                {
                    platform::child_process::acknowledge_runtime_mesh_command(*m_ownedRuntimeMeshCommand,published,
                        m_meshWireframe?platform::child_process::MeshDrawMode::wireframe:platform::child_process::MeshDrawMode::solid);
                    m_ownedRuntimeMeshCommand.reset();
                }
            }
            void update_runtime_view_shortcuts(core::Context* context,bool keyboardCaptured)
            {
                if(!context || !context->has_input_focus_safe())
                {m_screenshotHeld=false;m_wireframeHeld=false;return;}
                const bool modified=context->is_key_held_safe(input::Key::LeftControl)
                    ||context->is_key_held_safe(input::Key::RightControl)
                    ||context->is_key_held_safe(input::Key::LeftAlt)
                    ||context->is_key_held_safe(input::Key::RightAlt)
                    ||context->is_key_held_safe(input::Key::LeftSuper)
                    ||context->is_key_held_safe(input::Key::RightSuper);
                const bool screenshot=context->is_key_held_safe(input::Key::F12)
                    ||context->is_key_held_safe(input::Key::PrintScreen);
                const bool wireframe=context->is_key_held_safe(input::Key::F2);
                const bool screenshotPressed=context->is_key_down_safe(input::Key::F12)
                    ||context->is_key_down_safe(input::Key::PrintScreen)
                    ||(screenshot&&!m_screenshotHeld);
                const bool wireframePressed=context->is_key_down_safe(input::Key::F2)
                    ||(wireframe&&!m_wireframeHeld);
                if(!modified)
                {
                    if(screenshotPressed&&m_screenshotAfterFrames==0) m_screenshotAfterFrames=2;
                    if(wireframePressed&&!m_editorRuntimeControlled&&!keyboardCaptured&&!gui::keyboard_input_captured())
                        m_requestedMeshWireframe=!m_requestedMeshWireframe.value_or(m_meshWireframe);
                }
                m_screenshotHeld=screenshot;
                m_wireframeHeld=wireframe;
            }
            void finish_runtime_screenshot(core::Context* context)
            {
                if(m_screenshotAfterFrames==0) return;
                if(!context||!context->has_input_focus_safe()) {m_screenshotAfterFrames=0;return;}
                if(--m_screenshotAfterFrames!=0) return;
                const auto result=editor_clipboard_screenshot::copy_window(
                    reinterpret_cast<std::uintptr_t>(context->get_hwnd()));
                m_screenshotStatus=result.message;
                logger::get("Engine.Runtime").log(result.ok?logger::LogLevel::INFO:logger::LogLevel::WARN,
                    result.message,std::source_location::current());
            }
            [[nodiscard]] float runtime_panel_height() const noexcept
            {
                const float content=(m_gameplayRuntime&&m_gameplayRuntime->active()?724.0f:(m_tileMapPath.empty()?210.0f:306.0f))
                    +70.0f+(m_planetarySettings?100.0f:0.0f)+(m_explorationController?300.0f:0.0f);
                return (std::min)(content,(std::max)(1.0f,m_runtimeViewportHeight-48.0f));
            }
            [[nodiscard]] bool runtime_panel_captures_pointer(gui::Vec2 mouse) const noexcept
            {
                const auto bounds = runtime_panel_bounds();
                return std::isfinite(mouse.x)&&std::isfinite(mouse.y)
                    &&mouse.x>=bounds.position.x&&mouse.x<=bounds.position.x+bounds.size.x
                    &&mouse.y>=bounds.position.y&&mouse.y<=bounds.position.y+bounds.size.y;
            }
            [[nodiscard]] gui::WidgetBounds runtime_panel_bounds() const noexcept
            {
                const float width = std::isfinite(m_runtimeViewportWidth)
                    ? (std::max)(1.0f,m_runtimeViewportWidth) : 1.0f;
                const float height = std::isfinite(m_runtimeViewportHeight)
                    ? (std::max)(1.0f,m_runtimeViewportHeight) : 1.0f;
                const float inset = (std::min)(24.0f,((std::min)(width,height)-1.0f)*0.5f);
                const float availableWidth = (std::max)(1.0f,width-2.0f*inset);
                const float availableHeight = (std::max)(1.0f,height-2.0f*inset);
                if (m_runtimeControlsVisible)
                    return {{inset,inset},{(std::min)(430.0f,availableWidth),
                        (std::min)(runtime_panel_height(),availableHeight)}};
                const float toggleWidth = (std::min)(160.0f,availableWidth);
                return {{width-inset-toggleWidth,inset},
                    {toggleWidth,(std::min)(32.0f,availableHeight)}};
            }
            void draw_runtime_controls_restore()
            {
                const auto bounds = runtime_panel_bounds();
                gui::begin_window("",bounds.position,bounds.size,false,false);
                {
                    gui::ContentClipScope clip(bounds.position,
                        {bounds.position.x+bounds.size.x,bounds.position.y+bounds.size.y});
                    gui::set_cursor(bounds.position);
                    if (gui::button("Controls",bounds.size))
                        m_runtimeControlsVisible = true;
                }
                gui::end_window();
            }
            void draw_runtime_controls_hide(float contentWidth)
            {
                if (gui::button("Hide Controls", {(std::min)(120.0f,contentWidth),24.0f}))
                    m_runtimeControlsVisible = false;
            }
            void draw_runtime_frame_limit(const std::shared_ptr<core::Context>& ctx, float contentWidth)
            {
                const auto requestedMode = cli::frame_vsync_requested ? perf::frame_pacing_mode::vsync
                    : cli::frame_limit_fps > 0.0 ? perf::frame_pacing_mode::target_hz
                    : perf::frame_pacing_mode::uncapped;
                const auto startup = perf::select_frame_pacing_policy(requestedMode,
                    cli::frame_limit_fps, cli::frame_limit_explicit,
                    !cli::parented_mode && !cli::editor_requested && !cli::run_menu_loop);
                auto* pacing = ctx && ctx->windowData ? &ctx->windowData->editorFramePacingOverride : nullptr;
                gui::label("Foreground FPS (this run)");
                (void)gui::live_frame_limit_control("project-runtime-frame-limit", startup, pacing,
                    m_runtimeFrameLimitEdits[ctx.get()], contentWidth);
            }
            [[nodiscard]] bool runtime_water_visibility_pending() const noexcept
            {
                return m_planetarySettings
                    &&m_planetaryPublishedOcean!=m_planetarySettings->ocean_surface;
            }
            void set_runtime_water_visible(bool visible)
            {
                if(visible==m_runtimeWaterVisible)return;
                std::optional<project_planetary::Settings> authored;
                if(m_planetarySettings&&m_meshPlanetarySettings)
                {
                    authored=project_planetary::active_body_stream_settings(
                        *m_meshPlanetarySettings,m_planetarySettings->body_index);
                    if(!authored||m_planetarySettings->mode!="planet")
                    {m_runtimeViewStatus="Water view could not be changed for this body's terrain settings.";return;}
                }
                m_runtimeWaterVisible=visible;
                m_waterCaches.clear();
                if(!authored)return;
                const bool ocean=visible&&authored->ocean_surface;
                if(m_planetarySettings->ocean_surface==ocean)return;
                if(m_planetaryScheduler)
                {
                    if(m_planetaryPending)(void)m_planetaryScheduler->cancel(m_planetaryCancellation);
                    if(m_planetaryWaterPending)(void)m_planetaryScheduler->cancel(m_planetaryWaterCancellation);
                }
                // Keep every future until it retires. Its settings/clock binding
                // prevents a late wet/dry result from replacing the newer choice.
                if(m_planetaryWaterClockGeneration!=std::numeric_limits<std::uint64_t>::max())
                    ++m_planetaryWaterClockGeneration;
                m_planetarySettings->ocean_surface=ocean;
                m_planetaryContinue=true;m_planetaryFailed=false;m_planetaryNextTick={};
                m_planetaryWaterPublishedSource.reset();m_planetaryWaterPublishedSeconds.reset();
                m_planetaryWaterNextTick={};m_planetaryWaterSourceRefused=false;
                m_runtimeViewStatus=ocean?"Water on requested; updating the terrain view."
                    :"Water off requested; updating the terrain view.";
            }
            void draw_runtime_environment_controls(float contentWidth)
            {
                const bool ordinaryWater=std::any_of(m_entities.begin(),m_entities.end(),
                    [](const auto& entity){return entity.visible()&&entity.type=="Water";});
                const bool planetWater=m_meshPlanetarySettings&&m_meshPlanetarySettings->ocean_surface
                    &&m_planetarySettings&&m_planetarySettings->mode=="planet";
                const bool pending=runtime_water_visibility_pending();
                const std::string waterLabel=std::string{planetWater?"Planet Water: ":"Water: "}
                    +(!(ordinaryWater||planetWater)?"Not included":pending
                        ?(m_planetaryFailed?"update failed":m_runtimeWaterVisible?"turning on":"turning off")
                        :(m_runtimeWaterVisible?"On":"Off"));
                const std::array environment{
                    gui::InlineButtonSpec{.label=!m_hasPlanetRecipe?"Clouds: Not included":m_runtimeCloudsVisible?"Clouds: On":"Clouds: Off",
                        .width=0.0f,.enabled=m_hasPlanetRecipe},
                    gui::InlineButtonSpec{.label=waterLabel,
                        .width=0.0f,.enabled=ordinaryWater||planetWater}};
                if(const auto action=gui::inline_button_row(environment,28.0f,6.0f,
                    {.sizing=gui::InlineButtonSizing::FitContent,.alignment=gui::InlineButtonAlignment::Center,
                     .wrap=true,.available_width=contentWidth}))
                {
                    if(*action==0u)m_runtimeCloudsVisible=!m_runtimeCloudsVisible;
                    else set_runtime_water_visible(!m_runtimeWaterVisible);
                }
            }
            [[nodiscard]] int consume_runtime_navigation_wheel(gui::Vec2 mouse,bool projectCaptured) const noexcept
            {
                if(m_explorationController||projectCaptured||runtime_panel_captures_pointer(mouse)
                    ||gui::keyboard_input_captured())return 0;
                return gui::consume_mouse_wheel_delta();
            }
            void prepare_planetary_exploration()
            {
                if(!m_meshPlanetarySource||!m_planetarySettings)return;
                const auto binding=project_planetary_runtime::admitted_project_binding();
                if(!binding)return; // Ordinary projects need no optional controller.
                if(const auto* entity=planetary_entity();entity&&entity->mesh_asset&&entity->mesh_asset->coordinates)
                    for(const auto scale:entity->transform.scale)
                        if(!std::isfinite(scale)||std::abs(scale-1.0f)>0.000001f)
                        {
                            m_explorationStatus="Planet exploration needs object Scale 1, 1, 1. Use World Size / Radius to resize it while preserving walking speed and distances.";
                            return;
                        }
                std::error_code error;
                const bool sameRoot=std::filesystem::equivalent(binding->project_root,m_projectRoot,error);
                const auto& settings=*m_planetarySettings;
                if(error||!sameRoot||binding->project_id!=m_projectId
                    ||binding->source_hash!=m_meshPlanetarySource->content_hash
                    ||binding->seed!=settings.seed||binding->initial_body!=settings.body_index
                    ||binding->ocean_surface!=settings.ocean_surface
                    ||binding->size_reference_body!=settings.size_reference_body
                    ||binding->reference_radius_meters!=settings.reference_radius_meters
                    ||binding->original_sunlight!=settings.original_sunlight
                    ||!project_planetary::source_generation_is_current(m_projectRoot,*m_meshPlanetarySource))
                {m_explorationStatus="Planet exploration was not started: the linked provider belongs to different project source or settings.";return;}
                auto controller=project_planetary_runtime::create_controller(*binding);
                if(!controller){m_explorationStatus="Planet exploration provider is unavailable. The saved terrain remains visible.";return;}
                const auto* terrain = planetary_entity();
                std::vector<project_planetary_runtime::WalkBox> boxes;
                bool refusedBox{};
                for (const auto& entity : m_entities)
                {
                    if (!entity.terrain_attachment || !entity.terrain_attachment->solid_walk_box
                        || !project_play_entity_has_geometry(entity, m_planetaryEntity)) continue;
                    const auto mesh = m_meshes.find(entity.source_id);
                    const auto box = terrain && mesh != m_meshes.end() && mesh->second
                        ? project_play_walk_box(entity, *terrain, *mesh->second) : std::nullopt;
                    if (box && boxes.size() < project_planetary_runtime::maximum_walk_boxes) boxes.push_back(*box);
                    else refusedBox = true;
                }
                if (!boxes.empty() && !controller->set_walk_boxes(boxes)) refusedBox = true;
                auto session=project_planetary::load_exploration_session(m_projectRoot);
                if(!session.ok||(session.present&&!controller->restore_session(session.bytes)))
                {m_explorationStatus=session.ok?"The saved planet exploration state is invalid; it was preserved.":session.message;return;}
                auto captured=controller->capture();
                std::string startStatus;
                const auto* projection = m_sceneRuntime.projection();
                const auto* spawn = projection ? projection->resolve(projection->primary().spawn) : nullptr;
                if (!session.present && spawn && spawn->terrain_attachment)
                {
                    const auto start = terrain ? project_play_surface_start(*spawn, *terrain) : std::nullopt;
                    const auto placed = start ? controller->start_on_surface(*start) : project_planetary_runtime::Result{};
                    if (placed.code == project_planetary_runtime::Code::ready)
                    {
                        captured = placed;
                        startStatus = "Started at " + spawn->name + " in Walk mode.";
                    }
                    else startStatus = "Player start unavailable. Verify the saved terrain and move PlayerStart onto dry ground.";
                }
                if(captured.code!=project_planetary_runtime::Code::ready||!project_planetary_runtime::valid_frame(captured.frame)
                    ||(settings.original_sunlight&&!captured.frame.sunlight))
                {m_explorationStatus="Planet exploration returned an invalid initial camera.";return;}
                m_explorationController=std::move(controller);m_explorationSession=std::move(session);
                m_explorationFrame=captured.frame;m_explorationPreview=project_planetary_runtime::preview_frame(captured.frame);
                configure_exploration_stream(captured.frame.anchor);
                m_explorationStatus=startStatus.empty()
                    ? "Right-drag to look. Move with the project movement keys. Choose a body, then Orbit, Land or Cruise."
                    : std::move(startStatus);
                if (refusedBox) m_explorationStatus += " Some saved walking colliders could not be loaded.";
            }
            void configure_exploration_stream(std::uint32_t body)
            {
                const auto settings=project_planetary::active_body_stream_settings(*m_meshPlanetarySettings,body);
                if(!settings)
                {
                    m_planetaryFailed=true;
                    m_planetaryStatus="Terrain detail paused: the selected body's settings exceed the terrain budget. Exploration remains available.";
                    return;
                }
                if(m_planetarySettings&&m_planetarySettings->body_index!=body)
                    reset_planetary_water();
                m_planetarySettings=*settings;
                m_planetarySettings->ocean_surface=m_runtimeWaterVisible&&settings->ocean_surface;
                m_planetaryContinue=true;m_planetaryFailed=false;m_planetaryNextTick={};
            }
            [[nodiscard]] const ProjectRuntimeEntity* planetary_entity() const noexcept
            {
                const auto entity=std::find_if(m_entities.begin(),m_entities.end(),
                    [&](const auto& candidate){return candidate.source_id==m_planetaryEntity;});
                return entity!=m_entities.end()&&entity->visible()?&*entity:nullptr;
            }
            void update_planetary_sunlight()
            {
                if(!m_explorationController)return;
                const auto* planet=planetary_entity();if(!planet)return;
                const auto sunlight=m_meshPlanetarySettings&&m_meshPlanetarySettings->original_sunlight
                    ?m_explorationFrame.sunlight:std::nullopt;
                if(update_project_play_sunlight(m_lightingManager,m_sunlightHandle,m_lightingFrame,
                    sunlight,planet->transform.rotation)==ProjectSunlightUpdate::refused)
                    m_explorationStatus="Planetary sunlight could not fit the lighting budget or returned invalid data. Previous lighting is retained.";
            }
            [[nodiscard]] static std::optional<previewgrid::Vec3> planetary_preview_vector(
                const std::array<double,3>& value,const ProjectRuntimeEntity& entity,bool point,
                double point_units=1.0) noexcept
            {
                std::array<double,3> scaled{};
                for(std::size_t axis=0;axis<3;++axis)
                {
                    const double scale=entity.transform.scale[axis];
                    if(!std::isfinite(scale)||std::abs(scale)<1e-5)return {};
                    scaled[axis]=value[axis]*scale*(point?point_units:1.0);
                    if(!std::isfinite(scaled[axis])||std::abs(scaled[axis])>(std::numeric_limits<float>::max)())return {};
                }
                auto result=previewgrid::rotate_euler_degrees(
                    {static_cast<float>(scaled[0]),static_cast<float>(scaled[1]),static_cast<float>(scaled[2])},
                    {entity.transform.rotation[0],entity.transform.rotation[1],entity.transform.rotation[2]});
                if(point)result=previewgrid::add(result,{entity.transform.position[0],entity.transform.position[1],entity.transform.position[2]});
                if(!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.z))return {};
                return result;
            }
            [[nodiscard]] double planetary_scene_units() const noexcept
            {
                const auto* entity=planetary_entity();
                if(!entity||!entity->mesh_asset||!entity->mesh_asset->coordinates||!m_explorationPreview)return 1.0;
                return 1.0/m_explorationPreview->meters_to_preview;
            }
            [[nodiscard]] std::array<double,3> planetary_scene_origin_offset(
                const project_planetary_runtime::Frame* current_frame=nullptr) const noexcept
            {
                std::array<double,3> offset{};
                const auto* entity=planetary_entity();
                if(!entity||!entity->mesh_asset||!entity->mesh_asset->coordinates||!m_meshPlanetarySettings)return offset;
                const auto& frame=current_frame?*current_frame:m_explorationFrame;
                const project_planetary_runtime::BodyFrame* anchor{};
                const project_planetary_runtime::BodyFrame* reference{};
                for(std::uint32_t i=0;i<frame.body_count;++i)
                {
                    const auto& body=frame.bodies[i];
                    if(body.id==frame.anchor)anchor=&body;
                    if(body.id==m_meshPlanetarySettings->body_index)reference=&body;
                }
                if(!anchor)return offset;
                auto origin=entity->mesh_asset->coordinates->source_origin_meters;
                if(m_meshPlanetarySettings->mode=="planet"&&reference)origin=reference->center;
                const auto basis=previewgrid::make_precise_mesh_transform({},
                    {entity->transform.scale[0],entity->transform.scale[1],entity->transform.scale[2]},
                    {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]});
                if(!basis)return offset;
                offset=basis->apply({origin[0]-anchor->center[0],origin[1]-anchor->center[1],origin[2]-anchor->center[2]});
                return offset;
            }
            void advance_planetary_exploration(core::Context* context,float dt,gui::Vec2 mouse,bool looking,
                bool pointerCaptured,bool keyboardCaptured)
            {
                if(!m_explorationController||!context||!m_planetarySettings)return;
                const bool panel=runtime_panel_captures_pointer(mouse);
                const bool focused=context->has_input_focus_safe();
                const bool keyboard=focused&&!keyboardCaptured&&!gui::keyboard_input_captured()&&!panel&&runtime_input_ready();
                const bool pointer=focused&&!pointerCaptured&&!panel&&!gui::keyboard_input_captured();
                auto controls=m_explorationInput;m_explorationInput={};
                controls.elapsed_seconds=std::isfinite(dt)?std::clamp(double(dt),0.0,0.25):0.0;
                const auto axis=[&](input::Action positive,input::Action negative){return keyboard
                    ?double(context->action_held_safe(positive, m_runtimeInputProfile))-double(context->action_held_safe(negative, m_runtimeInputProfile)):0.0;};
                controls.forward=keyboard?runtime_planar_axis(project_input::ActionSemantic::move_y,
                    static_cast<float>(axis(input::Action::MoveForward,input::Action::MoveBackward))):0.0;
                controls.right=keyboard?runtime_planar_axis(project_input::ActionSemantic::move_x,
                    static_cast<float>(axis(input::Action::MoveRight,input::Action::MoveLeft))):0.0;
                controls.rise=axis(input::Action::MoveUp,input::Action::MoveDown);
                controls.yaw=-axis(input::Action::LookRight,input::Action::LookLeft)*controls.elapsed_seconds;
                controls.pitch=axis(input::Action::LookUp,input::Action::LookDown)*controls.elapsed_seconds;
                controls.roll=axis(input::Action::RollRight,input::Action::RollLeft)*controls.elapsed_seconds*1.5707963267948966;
                controls.boost=keyboard&&(context->is_key_held_safe(input::Key::LeftShift)||context->is_key_held_safe(input::Key::RightShift));
                if(pointer&&looking&&m_explorationMouseHeld)
                {
                    constexpr double radians=0.017453292519943295;
                    controls.yaw-=(mouse.x-m_explorationLastMouse.x)*m_runtimeInputProfile.mouse_look_sensitivity*radians;
                    controls.pitch-=(mouse.y-m_explorationLastMouse.y)*m_runtimeInputProfile.mouse_look_sensitivity*radians;
                }
                m_explorationLastMouse=mouse;m_explorationMouseHeld=pointer&&looking;
                const auto result=m_explorationController->advance(controls);
                const auto projected=result.code==project_planetary_runtime::Code::ready
                    &&(!m_meshPlanetarySettings->original_sunlight||result.frame.sunlight)
                    ?project_planetary_runtime::preview_frame(result.frame):std::nullopt;
                if(!projected)
                {
                    using Code=project_planetary_runtime::Code;
                    switch(result.code)
                    {
                    case Code::invalid_input:
                        m_explorationAdvanceStatus="Planet exploration input was rejected; the last camera is retained.";break;
                    case Code::cancelled:
                        m_explorationAdvanceStatus="Planet exploration advance was cancelled; the last camera is retained.";break;
                    case Code::unavailable:
                        m_explorationAdvanceStatus="Planet exploration provider is unavailable; the last camera is retained.";break;
                    case Code::provider_failure:
                        m_explorationAdvanceStatus="Planet exploration provider failed; the last camera is retained.";break;
                    case Code::ready:
                        m_explorationAdvanceStatus=m_meshPlanetarySettings->original_sunlight&&!result.frame.sunlight
                            ?"Planet exploration returned no required sunlight; the last camera is retained."
                            :"Planet exploration returned an invalid camera frame; the last camera is retained.";break;
                    case Code::invalid_frame:
                        m_explorationAdvanceStatus="Planet exploration returned an invalid camera frame; the last camera is retained.";break;
                    }
                    return;
                }
                const auto* entity=planetary_entity();if(!entity)return;
                const double sceneUnits=entity->mesh_asset&&entity->mesh_asset->coordinates
                    ?1.0/projected->meters_to_preview:1.0;
                const auto pivot=scene::world_position(entity->transform.position,entity->position_anchor);
                const auto authored=pivot?previewgrid::make_precise_mesh_transform(*pivot,
                    {entity->transform.scale[0],entity->transform.scale[1],entity->transform.scale[2]},
                    {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]}):std::nullopt;
                if(!authored){m_explorationAdvanceStatus="Planet exploration needs a finite nonzero authored transform.";return;}
                std::array<double,3> metricCamera{};
                for(std::size_t axis=0;axis<3;++axis)metricCamera[axis]=projected->camera[axis]*sceneUnits;
                const auto renderOrigin=authored->apply(metricCamera);
                const auto forward=planetary_preview_vector(projected->forward,*entity,false);
                const auto up=planetary_preview_vector(projected->up,*entity,false);
                if(!forward||!up){m_explorationAdvanceStatus="Planet exploration needs a finite nonzero authored transform.";return;}
                const auto direction=previewgrid::normalize(*forward);
                const auto scale=(std::max)({std::abs(entity->transform.scale[0]),std::abs(entity->transform.scale[1]),std::abs(entity->transform.scale[2])});
                const auto clearanceScale=(std::min)({std::abs(entity->transform.scale[0]),std::abs(entity->transform.scale[1]),std::abs(entity->transform.scale[2])});
                auto camera=previewgrid::camera_for(context);
                camera.eye={};camera.target=direction;camera.up=previewgrid::normalize(*up);
                // This simulation supplies a new rebased eye and basis. The
                // copied editor rig's precise navigation frame belongs to its
                // original origin and must not override the pilot's direction.
                camera.navigation={};camera.navigationValid=false;
                camera.farPlane=static_cast<float>((std::min)(1e12,projected->far_distance*sceneUnits*double(scale)*1.05+64.0));
                auto sceneOffset=planetary_scene_origin_offset(&result.frame);
                for(std::size_t axis=0;axis<3;++axis)sceneOffset[axis]-=renderOrigin[axis];
                camera.nearPlane=project_play_adaptive_near_plane(camera,
                    projected->eye_clearance*sceneUnits*double(clearanceScale),
                    projected->meters_to_preview*sceneUnits*double(clearanceScale),m_entities,m_meshes,
                    m_planetaryEntity,sceneOffset,true);
                camera.projection=render_camera::ProjectionKind::perspective;
                const auto validated=previewgrid::validate_camera_frame(camera);
                if(!validated)
                {m_explorationAdvanceStatus="Planet exploration camera was rejected; the last view is retained.";return;}
                m_explorationRenderOrigin=renderOrigin;m_explorationRenderCamera=*validated;
                if(m_explorationFrame.anchor!=result.frame.anchor)
                {
                    if(m_planetaryPending&&m_planetaryScheduler)(void)m_planetaryScheduler->cancel(m_planetaryCancellation);
                    configure_exploration_stream(result.frame.anchor);
                }
                m_explorationFrame=result.frame;m_explorationPreview=projected;
                m_explorationAdvanceStatus.clear();
            }
            [[nodiscard]] std::vector<previewgrid::ObjectMarker> planetary_exploration_markers() const
            {
                std::vector<previewgrid::ObjectMarker> markers;
                if(!m_explorationPreview)return markers;
                const auto* entity=planetary_entity();if(!entity)return markers;
                for(std::uint32_t i=0;i<m_explorationPreview->body_count;++i)
                {
                    const auto& body=m_explorationPreview->bodies[i];
                    const double sceneUnits=planetary_scene_units();
                    const auto pivot=scene::world_position(entity->transform.position,entity->position_anchor);
                    const auto authored=pivot?previewgrid::make_precise_mesh_transform(*pivot,
                        {entity->transform.scale[0],entity->transform.scale[1],entity->transform.scale[2]},
                        {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]}):std::nullopt;
                    if(!authored)return {};
                    std::array<double,3> local{};
                    for(std::size_t axis=0;axis<3;++axis)local[axis]=body.center[axis]*sceneUnits;
                    auto center=authored->apply(local);
                    for(std::size_t axis=0;axis<3;++axis)center[axis]-=m_explorationRenderOrigin[axis];
                    double preciseRatio=body.radius*sceneUnits/10.0;
                    auto mesh=body.mesh;
                    if(body.id==m_explorationPreview->anchor&&m_planetaryLodBody==body.id)
                    {const auto detailed=m_meshes.find(m_planetaryEntity);if(detailed!=m_meshes.end())
                        {mesh=detailed->second;if(m_planetaryMeshFrame)
                            {
                                preciseRatio=m_planetaryMeshFrame->meters_per_unit*m_explorationPreview->meters_to_preview*sceneUnits;
                                auto localOrigin=m_planetaryMeshFrame->source_origin_meters;
                                for(auto& value:localOrigin)value*=m_explorationPreview->meters_to_preview*sceneUnits;
                                auto translated=authored->apply(localOrigin);
                                for(std::size_t axis=0;axis<3u;++axis)center[axis]+=translated[axis]-authored->position[axis];
                            }}}
                    const auto ratio=static_cast<float>(preciseRatio);
                    auto precise=previewgrid::make_precise_mesh_transform(center,
                        {double(entity->transform.scale[0])*preciseRatio,double(entity->transform.scale[1])*preciseRatio,
                         double(entity->transform.scale[2])*preciseRatio},
                        {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]});
                    if(!precise)return {};
                    markers.push_back({.position={float(center[0]),float(center[1]),float(center[2])},.color={1,1,1},
                        .scale={entity->transform.scale[0]*ratio,entity->transform.scale[1]*ratio,entity->transform.scale[2]*ratio},
                        .rotationDegrees={entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]},
                        .radius=static_cast<float>(body.radius),.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,
                        .mesh=std::move(mesh),.preciseTransform=std::move(precise)});
                }
                return markers;
            }
            [[nodiscard]] bool append_planetary_cloud_markers(std::vector<previewgrid::ObjectMarker>& markers,
                std::vector<previewgrid::CloudVolume>& volumes,bool densityBackend,
                double sample_seconds=0.0)
            {
                m_runtimeCloudBackendUnavailable=false;
                if(!m_runtimeCloudsVisible)return true;
                const auto* entity=planetary_entity();if(!entity)return !m_explorationPreview;
                const double sceneUnits=planetary_scene_units();
                const auto pivot=scene::world_position(entity->transform.position,entity->position_anchor);
                const auto authored=pivot?previewgrid::make_precise_mesh_transform(*pivot,
                    {entity->transform.scale[0],entity->transform.scale[1],entity->transform.scale[2]},
                    {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]}):std::nullopt;
                if(!authored)return false;
                const auto append=[&](const std::shared_ptr<const asset::mesh::Artifact>& mesh,
                    std::array<double,3> center,double ratio,double wind_degrees)
                {
                    for(std::size_t axis=0;axis<3u;++axis)center[axis]-=m_explorationRenderOrigin[axis];
                    auto precise=previewgrid::make_precise_mesh_transform(center,
                        {double(entity->transform.scale[0])*ratio,double(entity->transform.scale[1])*ratio,
                         double(entity->transform.scale[2])*ratio},
                        {entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]});
                    if(!precise)return false;
                    // Compose provider wind with the authored orientation rather
                    // than adding Euler angles, which loses rolled planet axes.
                    const auto wind=previewgrid::make_precise_mesh_transform({0,0,0},{1,1,1},
                        {0,static_cast<float>(wind_degrees),0});
                    if(!wind)return false;
                    const auto rotation=precise->rotation;
                    for(unsigned row=0;row<3u;++row)for(unsigned column=0;column<3u;++column)
                    {
                        double value{};
                        for(unsigned axis=0;axis<3u;++axis)value+=rotation[row*3u+axis]*wind->rotation[axis*3u+column];
                        precise->rotation[row*3u+column]=value;
                    }
                    if(!previewgrid::valid_precise_mesh_transform(*precise))return false;
                    markers.push_back({.position={float(center[0]),float(center[1]),float(center[2])},.color={1,1,1},
                        .scale={static_cast<float>(entity->transform.scale[0]*ratio),static_cast<float>(entity->transform.scale[1]*ratio),
                            static_cast<float>(entity->transform.scale[2]*ratio)},
                        .primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,
                        .mesh=mesh,.preciseTransform=std::move(precise)});
                    return true;
                };
                const auto appendDensity=[&](const asset::cloud_volume::Recipe& recipe,
                    std::array<double,3> center,double radius,double metersPerUnit,double seconds) {
                    const double scale=entity->transform.scale[0];
                    if(scale<=0.0||std::abs(entity->transform.scale[1]-scale)>scale*1e-6
                        ||std::abs(entity->transform.scale[2]-scale)>scale*1e-6)return false;
                    if(!densityBackend) {
                        m_runtimeCloudBackendUnavailable=true;
                        return true;
                    }
                    for(unsigned axis=0;axis<3u;++axis)center[axis]-=m_explorationRenderOrigin[axis];
                    previewgrid::CloudVolume volume{.center=center,.radius=radius*scale,
                        .meters_per_world_unit=metersPerUnit/scale,.time_seconds=seconds,
                        .rotation=authored->rotation,.recipe=recipe};
                    if(!previewgrid::valid_cloud_volume(volume))return false;
                    volumes.push_back(std::move(volume));return true;
                };
                if(m_explorationPreview)
                {
                    for(std::uint32_t i=0;i<m_explorationPreview->body_count;++i)
                    {
                        const auto& body=m_explorationPreview->bodies[i];if(!body.cloud_mesh&&!body.cloud_density)continue;
                        std::array<double,3> local{};
                        for(std::size_t axis=0;axis<3u;++axis)local[axis]=body.center[axis]*sceneUnits;
                        if(body.cloud_density) {
                            if(!appendDensity(*body.cloud_density,authored->apply(local),body.radius*sceneUnits,
                                1.0/(m_explorationPreview->meters_to_preview*sceneUnits),m_explorationFrame.seconds))return false;
                        }
                        else if(!append(body.cloud_mesh,authored->apply(local),body.radius*sceneUnits/10.0,
                            body.cloud_wind_degrees))return false;
                    }
                }
                else if(m_planetaryWaterSource&&m_planetaryWaterSource->metadata.cloud&&m_planetarySettings)
                {
                    // The generic project runtime consumes the same admitted
                    // package layer without requiring the flight controller.
                    const auto& layer=*m_planetaryWaterSource->metadata.cloud;
                    if(layer.body!=m_planetarySettings->body_index||(!layer.artifact&&!layer.density))return true;
                    const double seconds=m_runtimePresentation?m_runtimePresentation->playhead_seconds:sample_seconds;
                    const double wind=std::isfinite(seconds)
                        ?std::fmod((std::max)(0.0,seconds),layer.wind_period_seconds)/layer.wind_period_seconds*360.0:0.0;
                    if(layer.density) {
                        if(!appendDensity(*layer.density,authored->apply(layer.center_meters),layer.radius_meters,1.0,
                            std::isfinite(seconds)?(std::max)(0.0,seconds):0.0))return false;
                    }
                    else if(!append(layer.artifact,authored->apply(layer.center_meters),layer.radius_meters/10.0,wind))return false;
                }
                return true;
            }
            [[nodiscard]] std::string append_planetary_attached_markers(
                std::vector<previewgrid::ObjectMarker>& markers,std::vector<std::uint64_t>& managed,
                bool texturedMeshes) const
            {
                if(!m_explorationController||!m_explorationPreview||!m_explorationRenderCamera)return {};
                const auto* terrain=planetary_entity();
                bool forestRefused{}, modelRefused{};
                std::string_view providerHash=m_meshPlanetarySource?m_meshPlanetarySource->content_hash:std::string_view{};
                if(providerHash.size()==71u&&providerHash.starts_with("sha256:"))providerHash.remove_prefix(7u);
                else providerHash={};
                const auto recipe=m_meshPlanetarySettings?project_planetary::encode_settings(*m_meshPlanetarySettings):std::string{};
                for(const auto& entity:m_entities)
                {
                    const auto& forest = entity.forest_region;
                    const auto& attachment = entity.terrain_attachment;
                    if ((!forest && !attachment) || !project_play_entity_has_geometry(entity, 0u)) continue;
                    auto& refused = forest ? forestRefused : modelRefused;
                    // The immutable authored mesh reference is the provenance
                    // key; a streamed LOD artifact legitimately has another key.
                    if (!terrain || !m_meshPlanetarySource || !m_meshPlanetarySettings
                        || (forest && (!forest->valid() || forest->provider_content_hash != providerHash || forest->recipe != recipe)))
                    {refused=true;continue;}
                    const auto bodyIndex = forest ? forest->body_index : attachment->body_index;
                    const project_planetary_runtime::PreviewBody* body{};
                    for(std::uint32_t i=0;i<m_explorationPreview->body_count&&i<m_explorationPreview->bodies.size();++i)
                        if(m_explorationPreview->bodies[i].id==bodyIndex)body=&m_explorationPreview->bodies[i];
                    const auto mesh=m_meshes.find(entity.source_id);
                    if(!body||mesh==m_meshes.end()){refused=true;continue;}
                    auto marker = forest
                        ? project_play_forest_marker_for_body(entity,*terrain,*body,planetary_scene_units(),m_explorationRenderOrigin,mesh->second)
                        : project_play_model_marker_for_body(entity,*terrain,*body,planetary_scene_units(),m_explorationRenderOrigin,mesh->second);
                    if(!marker){refused=true;continue;}
                    if(entity.texture_material && (texturedMeshes || !marker->forestLighting))
                    {
                        if(!texturedMeshes)
                        {
                            managed.push_back(entity.source_id);
                            continue;
                        }
                        const auto binding=m_meshBaseColorTextures.find(entity.source_id);
                        if(binding==m_meshBaseColorTextures.end()||!binding->second){refused=true;continue;}
                        marker->baseColorTexture=binding->second;
                    }
                    markers.push_back(std::move(*marker));
                    managed.push_back(entity.source_id);
                }
                if (forestRefused && modelRefused) return "Some terrain attachments are unavailable. Authored poses are retained.";
                if (forestRefused) return "Forest area no longer matches terrain; regenerate/repaint to attach it. Authored poses are retained.";
                if (modelRefused) return "A model attachment no longer matches terrain. Its authored pose is retained.";
                return {};
            }
            void draw_planetary_exploration_controls(float contentWidth)
            {
                if(!m_explorationController)return;
                using Command=project_planetary_runtime::Command;
                const auto row=[&](auto&& buttons){return gui::inline_button_row(buttons,28.0f,6.0f,
                    {.sizing=gui::InlineButtonSizing::FitContent,.alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=contentWidth});};
                std::vector<gui::InlineButtonSpec> bodies; bodies.reserve(m_explorationFrame.body_count);
                for(std::uint32_t i=0;i<m_explorationFrame.body_count;++i)
                    bodies.push_back({.label=m_explorationFrame.bodies[i].name,.width=0.0f});
                if(const auto selected=row(bodies))m_explorationInput.selected_body=m_explorationFrame.bodies[*selected].id;
                const std::array essential{gui::InlineButtonSpec{.label="Orbit",.width=0.0f},
                    gui::InlineButtonSpec{.label="Land",.width=0.0f},
                    gui::InlineButtonSpec{.label="Cruise",.width=0.0f},
                    gui::InlineButtonSpec{.label="Walk / Fly",.width=0.0f}};
                if(const auto action=row(essential))m_explorationInput.command=
                    std::array{Command::orbit,Command::land,Command::toggle_cruise,Command::toggle_walk}[*action];
                const std::array options{gui::InlineButtonSpec{.label=m_explorationFrame.gravity_enabled?"Gravity On":"Gravity Off",.width=0.0f},
                    gui::InlineButtonSpec{.label=m_explorationFrame.level_assist?"Level Assist On":"Level Assist Off",.width=0.0f}};
                if(const auto action=row(options))m_explorationInput.command=*action==0u?Command::toggle_gravity:Command::toggle_level_assist;
                const std::array clock{gui::InlineButtonSpec{.label=m_explorationFrame.time_scale==0.0?"Resume Time":"Pause Time",.width=0.0f},
                    gui::InlineButtonSpec{.label="Time Speed",.width=0.0f},gui::InlineButtonSpec{.label="Save Exploration",.width=0.0f}};
                if(const auto action=row(clock))
                {
                    if(*action<2u)m_explorationInput.command=*action==0u?Command::toggle_time_pause:Command::cycle_time_rate;
                    else if(m_meshPlanetarySource)
                    {
                        const auto bytes=m_explorationController->encode_session();
                        auto saved=project_planetary::save_exploration_session(m_projectRoot,*m_meshPlanetarySource,bytes,m_explorationSession);
                        m_explorationStatus=saved.message;if(saved.ok)m_explorationSession=std::move(saved);
                    }
                }
                const std::array speed{gui::InlineButtonSpec{.label="Slower",.width=0.0f},gui::InlineButtonSpec{.label="Faster",.width=0.0f}};
                if(const auto action=row(speed))m_explorationInput.speed_multiplier=*action==0u?0.5:2.0;
            }

            void prepare_planetary_runtime()
            {
                shutdown_planetary_runtime();
                if(m_cameraMode==previewgrid::CameraMode::Canvas2D || m_projectRoot.empty()) return;
                const auto settings=m_meshPlanetarySettings;
                m_hasPlanetRecipe=settings&&project_planetary::supports_exploration(*settings);
                if(!m_hasPlanetRecipe) return;
                m_planetarySettings=settings;
                m_planetaryPublishedOcean=settings->ocean_surface;
                m_planetarySettings->ocean_surface=m_runtimeWaterVisible&&settings->ocean_surface;
                std::size_t owners{};
                for(const auto& entity:m_entities)
                    if(entity.category=="Planetary Terrain" && entity.visible() && entity.mesh_asset)
                    {++owners;m_planetaryEntity=entity.source_id;}
                if(owners!=1u || !m_meshes.contains(m_planetaryEntity))
                {
                    m_planetaryEntity=0u;m_planetaryFailed=true;
                    m_planetaryStatus=owners>1u
                        ? "Terrain LOD supports one active planet. Hide the other planets in the Editor."
                        : "Terrain LOD needs a visible, validated planet mesh.";
                    return;
                }
                if(settings->mode=="planet")m_planetaryLodBody=settings->body_index;
                prepare_planetary_exploration();
                m_planetaryContinue=true;
                if(!m_planetaryFailed)m_planetaryStatus="Terrain LOD queued for the current camera.";
            }

            void shutdown_planetary_runtime() noexcept
            {
                // Submitted operations capture immutable values, never this Scene.
                // Retire the owned child before its scheduler and scene disappear.
                try
                {
                    if(m_planetaryScheduler)
                    {
                        if(m_planetaryPending) (void)m_planetaryScheduler->cancel(m_planetaryCancellation);
                        if(m_planetaryWaterPending) (void)m_planetaryScheduler->cancel(m_planetaryWaterCancellation);
                        for(const auto& [context,work]:m_planetaryFramePublications)
                            if(work.pending)(void)m_planetaryScheduler->cancel(work.cancellation);
                        m_planetaryScheduler->graph().Shutdown();
                        m_planetaryScheduler.reset();
                    }
                    m_planetaryPending.reset();
                    m_planetaryWaterPending.reset();
                    m_planetaryFramePublications.clear();
                }
                catch(...)
                {
                    logger::get("Engine.Planet").log(logger::LogLevel::Error,
                        "Planetary runtime job retirement raised an exception.",std::source_location::current());
                }
                for(const auto& [context,applied]:m_cameraApplied)
                    (void)previewgrid::clear_external_camera_frame(context);
                m_explorationController.reset();m_explorationPreview.reset();m_explorationFrame={};m_runtimePresentation.reset();
                m_explorationRenderCamera.reset();m_explorationRenderOrigin={};
                m_explorationInput={};m_explorationSession={};m_explorationStatus.clear();m_explorationAdvanceStatus.clear();
                m_terrainAttachmentStatus.clear();
                m_explorationMouseHeld=false;m_planetaryLodBody.reset();
                m_planetaryMeshFrame.reset();
                m_planetarySettings.reset();m_planetaryRequestSettings.reset();m_planetaryEntity=0u;
                m_hasPlanetRecipe=false;
                m_planetaryPaused=false;m_planetaryFailed=false;m_planetaryContinue=true;
                m_planetaryNextTick={};
                reset_planetary_water();
            }

            struct PlanetaryWaterResult final
            {
                std::shared_ptr<const project_planetary_snapshot::Snapshot> source{};
                std::shared_ptr<const asset::mesh::Artifact> mesh{};
                std::uint32_t body{};
                std::uint64_t clock_generation{};
                double seconds{};
                double worker_seconds{};
                bool source_current{},cancelled{};
            };
            void reset_planetary_water()
            {
                if(m_planetaryScheduler&&m_planetaryWaterPending)
                    (void)m_planetaryScheduler->cancel(m_planetaryWaterCancellation);
                // An in-flight provider must retire before another evaluation
                // starts. Clearing its base invalidates completion, not ownership.
                m_planetaryWaterSource.reset();m_planetaryWaterPublishedSource.reset();
                m_planetaryWaterPublishedSeconds.reset();m_planetaryWaterNextTick={};
                m_planetaryWaterObservedSeconds.reset();m_planetaryWaterObservedScale.reset();
                m_planetaryWaterClockDirection=0;
                m_planetaryWaterEvidenceTick={};
                m_planetaryWaterStatus.clear();m_planetaryWaterSourceRefused=false;
            }
            void ensure_planetary_scheduler()
            {
                if(!m_planetaryScheduler)m_planetaryScheduler=std::make_unique<editor_tasks::Scheduler>(
                    editor_tasks::SchedulerLimits{.worker_count=2u,.retained_finished_nodes=64u});
                (void)m_planetaryScheduler->maintain();
            }
            bool publish_project_play_markers(
                const epochengine::core::Context* ctx,
                std::span<const ProjectRuntimeEntity> entities,
                const std::unordered_map<std::uint64_t, std::shared_ptr<const asset::mesh::Artifact>>* meshes = nullptr,
                epochengine::previewgrid::MeshFrameLimits mesh_limits = epochengine::previewgrid::MeshFrameLimits(),
                std::span<const epochengine::previewgrid::ObjectMarker> additional = {},
                std::uint64_t suppressed_entity = 0u,
                bool mesh_wireframe = false,
                const std::array<double,3>& origin_offset = {},
                const std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>>* base_colors = nullptr,
                const std::shared_ptr<const asset::mesh::Artifact>& water_surface = {},
                const previewgrid::Camera* frame_camera = nullptr,
                const lighting::LightingFrame* frame_lighting = nullptr,
                const std::array<double,3>& render_origin = {},
                std::span<const std::uint64_t> managed_forest_entities = {},
                std::span<const previewgrid::CloudVolume> clouds = {})
            {
                if (!ctx)
                    return false;

                std::vector<epochengine::previewgrid::ObjectMarker> markers{};
                markers.reserve(entities.size()+additional.size());
                for (const auto& entity : entities)
                {
                    if(!project_play_entity_has_geometry(entity,suppressed_entity))continue;
                    if(std::find(managed_forest_entities.begin(),managed_forest_entities.end(),entity.source_id)
                        !=managed_forest_entities.end())continue;

                    std::shared_ptr<const asset::mesh::Artifact> mesh{};
                    if (entity.type == "Water")
                    {
                        if (!water_surface) continue;
                        mesh = water_surface;
                    }
                    else if (entity.mesh_asset)
                    {
                        if (!meshes) continue;
                        const auto found = meshes->find(entity.source_id);
                        if (found == meshes->end()) continue;
                        mesh = found->second;
                    }
                    auto marker=project_play_marker_for_entity(entity,std::move(mesh),origin_offset,frame_camera!=nullptr);
                    if(!marker)return false;
                    if (entity.mesh_asset && entity.texture_material
                        && (ctx->type == core::ContextType::OpenGL || !marker->forestLighting))
                    {
                        if (ctx->type != core::ContextType::OpenGL) continue;
                        if (!base_colors) return false;
                        const auto binding = base_colors->find(entity.source_id);
                        if (binding == base_colors->end() || !binding->second) return false;
                        marker->baseColorTexture = binding->second;
                    }
                    markers.push_back(std::move(*marker));
                }

                for(const auto& marker:additional)
                    if(ctx->type==core::ContextType::OpenGL||!marker.baseColorTexture)markers.push_back(marker);
                for (auto& marker : markers)
                    marker.wireframe = mesh_wireframe;

                const std::span<const previewgrid::ObjectMarker> published{markers.data(),markers.size()};
                if(frame_camera)
                    return frame_lighting&&pump_runtime_planetary_frame(ctx,
                        previewgrid::SceneFrameRequest{std::move(markers),*frame_camera,*frame_lighting,
                            mesh_limits,render_origin,{clouds.begin(),clouds.end()},
                            double(m_runtimeViewportWidth)/(std::max)(1.0,double(m_runtimeViewportHeight))},mesh_wireframe);
                if(!clouds.empty())
                    return previewgrid::publish_scene_frame(ctx,published,previewgrid::camera_for(ctx),
                        frame_lighting?*frame_lighting:previewgrid::lighting_frame_for(ctx),mesh_limits,render_origin,clouds);
                return previewgrid::set_object_markers(ctx,published,mesh_limits);
            }
            struct RuntimeSceneFrameResult final
            {
                std::optional<previewgrid::PreparedSceneFrame> prepared{};
                double queue_seconds{},prepare_seconds{};
                bool cancelled{};
            };
            struct PlanetaryFramePublication final
            {
                std::optional<std::future<RuntimeSceneFrameResult>> pending{};
                editor_tasks::CancellationTicket cancellation{};
                std::shared_ptr<const previewgrid::SceneFrameRequest> request{};
                timing::Clock::time_point submitted{},next_evidence{};
                std::uint32_t body{},evidence_count{};
                bool wireframe{},water_visible{},clouds_visible{},admitted{};
            };
            [[nodiscard]] bool planetary_frame_preparing(const void* context) const noexcept
            {
                const auto found=m_planetaryFramePublications.find(context);
                return found!=m_planetaryFramePublications.end()&&found->second.pending.has_value();
            }
            // The project uses the same immutable prepare/commit boundary as
            // the Editor. Expanding a new LOD, moving its origin and baking
            // lighting must not run on the frame/input owner.
            [[nodiscard]] bool pump_runtime_planetary_frame(const void* context,
                previewgrid::SceneFrameRequest request,bool wireframe)
            {
                auto& work=m_planetaryFramePublications[context];
                const auto current=[&]{return work.body==m_explorationFrame.anchor
                    &&work.wireframe==wireframe&&work.water_visible==m_runtimeWaterVisible
                    &&work.clouds_visible==m_runtimeCloudsVisible&&work.request
                    &&work.request->depth_fit_aspect==request.depth_fit_aspect;};
                if(work.pending&&!current()&&m_planetaryScheduler&&work.cancellation.valid())
                    (void)m_planetaryScheduler->cancel(work.cancellation);
                bool published{};
                if(work.pending&&work.pending->wait_for(std::chrono::milliseconds{0})==std::future_status::ready)
                {
                    RuntimeSceneFrameResult result;
                    auto commit_started=timing::Clock::now();
                    std::string_view outcome{"preparation refused"};
                    try
                    {
                        result=work.pending->get();work.pending.reset();
                        commit_started=timing::Clock::now();
                        if(result.cancelled)
                        {work.request.reset();work.admitted=false;outcome="cancelled";}
                        else if(!current())
                        {work.request.reset();work.admitted=false;outcome="superseded";}
                        else if(result.prepared)
                        {
                            // The render thread must never see the worker's
                            // captured camera between geometry and live-view updates.
                            published=previewgrid::commit_scene_frame(context,std::move(*result.prepared),&request);
                            work.admitted=published;
                            outcome=published?"committed":"commit refused";
                            if(!published)work.request.reset();
                        }
                        else work.admitted=false;
                    }
                    catch(const editor_tasks::TaskCancelled&)
                    {work.pending.reset();work.request.reset();work.admitted=false;outcome="cancelled";}
                    catch(...)
                    {
                        work.pending.reset();work.admitted=false;
                        if(!current()){work.request.reset();outcome="superseded";}
                    }
                    work.cancellation={};
                    const auto now=timing::Clock::now();
                    try
                    {
                        if(work.evidence_count<24u&&now>=work.next_evidence)
                        {
                            logger::get("Engine.Planet").logf(logger::LogLevel::INFO,std::source_location::current(),
                                "Terrain display: queue {:.3f} ms, CPU prepare {:.3f} ms, owner commit {:.3f} ms, total {:.3f} ms; committed={}; outcome={}",
                                result.queue_seconds*1000.0,result.prepare_seconds*1000.0,
                                std::chrono::duration<double,std::milli>(now-commit_started).count(),
                                std::chrono::duration<double,std::milli>(now-work.submitted).count(),published,outcome);
                            ++work.evidence_count;work.next_evidence=now+std::chrono::seconds{5};
                        }
                    }
                    catch(...){}
                }
                const auto camera=request.camera;
                const auto origin=request.render_origin;
                const auto clouds=request.clouds;
                if(!work.pending&&(!work.request||!previewgrid::same_scene_frame_request(request,*work.request)))
                {
                    try
                    {
                        if(auto source=previewgrid::capture_scene_frame_source(context))
                        {
                            ensure_planetary_scheduler();
                            auto owned=std::make_shared<const previewgrid::SceneFrameRequest>(std::move(request));
                            const auto queued=timing::Clock::now();
                            auto submitted=m_planetaryScheduler->submit_cancellable("Prepare project terrain display",
                                [source=std::move(*source),owned,queued](std::stop_token stop){
                                    const auto started=timing::Clock::now();
                                    RuntimeSceneFrameResult result;
                                    result.queue_seconds=std::chrono::duration<double>(started-queued).count();
                                    if(!stop.stop_requested())
                                        result.prepared=previewgrid::prepare_scene_frame(source,owned->markers,
                                            previewgrid::fit_visible_depth_range(owned->camera,owned->markers,owned->depth_fit_aspect),
                                            owned->lighting,owned->limits,owned->render_origin,stop,owned->clouds);
                                    result.cancelled=stop.stop_requested();
                                    result.prepare_seconds=std::chrono::duration<double>(timing::Clock::now()-started).count();
                                    return result;
                                });
                            work.cancellation=submitted.cancellation;work.pending.emplace(std::move(submitted.completion));
                            work.request=std::move(owned);work.submitted=queued;
                            work.body=m_explorationFrame.anchor;work.wireframe=wireframe;
                            work.water_visible=m_runtimeWaterVisible;work.clouds_visible=m_runtimeCloudsVisible;
                            work.admitted=false;
                        }
                    }
                    catch(...){work.admitted=false;}
                }
                // Keep navigating the last complete packet while a replacement
                // is queued; the camera and clouds retain that packet's origin.
                if(const auto previous=previewgrid::scene_frame_for(context))
                {
                    auto retained=camera;retained.nearPlane=previous->camera.nearPlane;
                    retained.farPlane=previous->camera.farPlane;
                    (void)previewgrid::advance_scene_frame_camera(context,retained,origin,
                        std::span<const previewgrid::CloudVolume>{clouds});
                }
                return published||(!work.pending&&work.admitted);
            }
            [[nodiscard]] bool planetary_mesh_frame_fits(
                const std::shared_ptr<const asset::mesh::Artifact>& replacement) const noexcept
            {
                const auto* entity=planetary_entity();
                if(!replacement||!entity||!m_planetarySettings)return false;
                const auto scale=scene::mesh_render_scale(entity->transform.scale,entity->mesh_asset);
                if(!scale)return false;
                const auto budget=project_planetary::scene_mesh_budget(*m_planetarySettings);
                std::uint64_t bytes{},indices{};
                const auto account=[&](const auto& mesh) {
                    if(mesh){bytes+=mesh->vertices().size()*sizeof(asset::mesh::Vertex)
                        +mesh->indices().size()*sizeof(std::uint32_t)+64u;indices+=mesh->indices().size();}
                };
                for(const auto& [id,mesh]:m_meshes)if(id!=m_planetaryEntity)account(mesh);
                if(m_explorationPreview)
                    for(std::uint32_t i=0;i<m_explorationPreview->body_count;++i)
                        if(m_explorationPreview->bodies[i].id!=m_explorationPreview->anchor)
                            account(m_explorationPreview->bodies[i].mesh);
                account(replacement);
                const previewgrid::ObjectMarker marker{
                    .position{entity->transform.position[0],entity->transform.position[1],entity->transform.position[2]},
                    .scale{(*scale)[0],(*scale)[1],(*scale)[2]},
                    .rotationDegrees{entity->transform.rotation[0],entity->transform.rotation[1],entity->transform.rotation[2]},
                    .primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=replacement};
                const std::array frame{marker};
                return replacement->vertices().size()<=budget.artifact.maximum_vertices
                    &&replacement->indices().size()<=budget.artifact.maximum_indices
                    &&48u+replacement->vertices().size()*44u+replacement->indices().size()*4u<=budget.artifact.maximum_bytes
                    &&bytes<=budget.cache_bytes&&indices<=budget.frame_indices
                    &&previewgrid::mesh_frame_admitted(frame,m_meshFrameLimits);
            }
            [[nodiscard]] static PlanetaryWaterResult sample_planetary_water(
                const std::filesystem::path& root,const project_planetary::SourceGeneration& identity,
                std::shared_ptr<const project_planetary_snapshot::Snapshot> source,
                const std::shared_ptr<water::wave::Provider>& provider,
                const project_planetary::Settings& settings,double seconds,
                std::string_view water_source_hash,std::uint64_t clock_generation,
                std::uint64_t retained_display_bytes,std::stop_token stop)
            {
                const auto started=timing::Clock::now();
                PlanetaryWaterResult result{.source=std::move(source),.body=settings.body_index,
                    .clock_generation=clock_generation,.seconds=seconds};
                if(stop.stop_requested()){result.cancelled=true;return result;}
                const auto current_source=[&] {
                    if(!project_planetary::source_generation_is_current(root,identity,stop))return false;
                    const auto water=package_source::load_project_attachment(root,"fft_ocean",stop);
                    return water.source_attached()&&water.content_hash==water_source_hash;
                };
                if(!result.source||!provider||!std::isfinite(seconds)
                    ||!current_source())return result;
                result.source_current=true;
                auto limits=project_planetary::snapshot_transition_limits(settings);
                if(retained_display_bytes>=limits.maximum_retained_bytes)return result;
                limits.maximum_retained_bytes-=retained_display_bytes;
                const auto wave_budget=(std::min<std::uint64_t>)(limits.maximum_transient_bytes,64ull*1024u*1024u);
                const auto wave=provider->evaluate(seconds,wave_budget,stop);
                if(wave&&wave.tile&&wave.tile->seconds()==seconds)
                    result.mesh=project_planetary_snapshot::resample_water(*result.source,*wave.tile,limits,stop);
                result.cancelled=stop.stop_requested();
                result.source_current=!result.cancelled&&current_source();
                if(!result.source_current)result.mesh.reset();
                result.worker_seconds=std::chrono::duration<double>(timing::Clock::now()-started).count();
                return result;
            }
            bool apply_runtime_presentation(const platform::child_process::RuntimePresentation& sample,
                core::Context* context)
            {
                if(!platform::child_process::valid_runtime_presentation(sample)||!context)return false;
                const auto clock=context->simulation_sample_time();
                if(!clock||*clock<0.0)return false;
                epochengine::timeline::TimelineState state;
                state.playhead_seconds=sample.seconds;state.duration_seconds=sample.duration_seconds;
                state.playback_rate=sample.rate;state.playing=sample.playing;
                state.direction=sample.reverse?epochengine::timeline::PlaybackDirection::reverse
                    :epochengine::timeline::PlaybackDirection::forward;
                m_runtimePresentation=state;
                advance_runtime_presentation(context);
                // A seek or direction command starts a new presentation branch;
                // a completed wave from the previous branch cannot publish later.
                if(m_planetaryWaterClockGeneration!=std::numeric_limits<std::uint64_t>::max())
                    ++m_planetaryWaterClockGeneration;
                if(m_planetaryScheduler&&m_planetaryWaterPending)
                    (void)m_planetaryScheduler->cancel(m_planetaryWaterCancellation);
                m_planetaryWaterPublishedSource.reset();m_planetaryWaterNextTick={};
                return true;
            }
            void advance_runtime_presentation(core::Context* context)
            {
                if(!m_runtimePresentation||!context)return;
                const auto seconds=context->simulation_sample_time();
                if(!seconds)return;
                core::time::simulation_stats stats;
                stats.simulated_seconds=*seconds;stats.fixed_dt_seconds=m_runtimePresentation->fixed_dt_seconds;
                epochengine::timeline::sync_to_simulation(*m_runtimePresentation,stats);
            }
            void observe_planetary_water_clock(timing::Clock::time_point now,double seconds,double scale)
            {
                if(m_explorationController&&std::isfinite(seconds))
                {
                    const double delta=m_planetaryWaterObservedSeconds
                        ?seconds-*m_planetaryWaterObservedSeconds:0.0;
                    const int direction=std::abs(delta)>1e-9?(delta>0.0?1:-1):m_planetaryWaterClockDirection;
                    const double elapsed=m_planetaryWaterObservedSeconds
                        ?std::chrono::duration<double>(now-m_planetaryWaterObservedTick).count():0.0;
                    const double expected=std::abs(m_planetaryWaterObservedScale.value_or(0.0))*elapsed+0.25;
                    const bool clock_changed=m_planetaryWaterObservedSeconds
                        &&(m_planetaryWaterObservedScale!=scale
                            ||(m_planetaryWaterClockDirection&&direction!=m_planetaryWaterClockDirection)
                            ||std::abs(delta)>expected);
                    if(clock_changed)
                    {
                        if(m_planetaryWaterClockGeneration==std::numeric_limits<std::uint64_t>::max())return;
                        ++m_planetaryWaterClockGeneration;
                        if(m_planetaryScheduler&&m_planetaryWaterPending)
                            (void)m_planetaryScheduler->cancel(m_planetaryWaterCancellation);
                        m_planetaryWaterPublishedSource.reset();m_planetaryWaterNextTick={};
                    }
                    m_planetaryWaterObservedSeconds=seconds;
                    m_planetaryWaterObservedScale=scale;
                    m_planetaryWaterObservedTick=now;m_planetaryWaterClockDirection=direction;
                }
            }
            void pump_planetary_water()
            {
                const auto now=timing::Clock::now();
                const double sample_seconds=m_runtimePresentation?m_runtimePresentation->playhead_seconds:m_explorationFrame.seconds;
                const double sample_scale=m_runtimePresentation
                    ?(m_runtimePresentation->playing?m_runtimePresentation->playback_rate
                        *(m_runtimePresentation->direction==epochengine::timeline::PlaybackDirection::reverse?-1.0:1.0):0.0)
                    :m_explorationFrame.time_scale;
                observe_planetary_water_clock(now,sample_seconds,sample_scale);
                if(m_planetaryWaterPending
                    &&m_planetaryWaterPending->wait_for(std::chrono::milliseconds{0})==std::future_status::ready)
                {
                    try
                    {
                        auto result=m_planetaryWaterPending->get();m_planetaryWaterPending.reset();
                        if(result.source==m_planetaryWaterSource&&m_planetarySettings
                            &&result.clock_generation==m_planetaryWaterClockGeneration
                            &&result.body==m_planetarySettings->body_index&&m_explorationController
                            &&result.body==m_explorationFrame.anchor)
                        {
                            if(result.mesh&&result.source_current&&!result.cancelled&&planetary_mesh_frame_fits(result.mesh))
                            {
                                m_meshes[m_planetaryEntity]=std::move(result.mesh);
                                m_planetaryWaterPublishedSource=result.source;
                                m_planetaryWaterPublishedSeconds=result.seconds;
                                m_planetaryWaterStatus=m_runtimePresentation
                                    ?(sample_scale==0.0?"Ocean: Editor timeline paused. Flight remains live."
                                        :"Ocean: following Editor timeline. Flight remains live.")
                                    :(sample_scale==0.0?"Ocean: paused.":"Ocean: animating.");
                                m_planetaryWaterNextTick=now;
                                if(now>=m_planetaryWaterEvidenceTick)
                                {
                                    logger::get("Engine.Water").log(logger::LogLevel::INFO,
                                        "Planetary ocean sample: time="+std::to_string(result.seconds)
                                            +"s worker="+std::to_string(result.worker_seconds)
                                            +"s body="+std::to_string(result.body)
                                            +" terrain_request="+std::to_string(result.source->metadata.identity.request_id),
                                        std::source_location::current());
                                    m_planetaryWaterEvidenceTick=now+std::chrono::seconds{5};
                                }
                            }
                            else if(!result.cancelled)
                            {
                                m_planetaryWaterSourceRefused=!result.source_current;
                                m_planetaryWaterStatus=result.source_current
                                    ?"Ocean animation could not update within its geometry or memory limits. The previous surface is retained."
                                    :"Ocean animation stopped because its approved source changed. Rebuild the project to use the new source.";
                                m_planetaryWaterNextTick=now+std::chrono::seconds{1};
                            }
                        }
                    }
                    catch(...)
                    {
                        m_planetaryWaterPending.reset();m_planetaryWaterNextTick=now+std::chrono::seconds{1};
                        m_planetaryWaterStatus="Ocean animation could not update. The previous surface is retained.";
                    }
                }
                if(m_planetaryWaterPending||!m_planetarySettings||!m_planetarySettings->ocean_surface
                    ||!m_explorationController||!m_meshPlanetarySource||!m_planetaryWaterSource
                    ||m_planetarySettings->body_index!=m_explorationFrame.anchor
                    ||m_planetaryWaterSourceRefused||now<m_planetaryWaterNextTick)return;
                if(m_planetaryWaterSource->metadata.water_radius_meters<=0.0)
                {m_planetaryWaterStatus="Ocean: waiting for updated terrain. Review the Planetary source update, then Build & Generate.";return;}
                if(m_planetaryWaterSource->metadata.water_vertices.empty())
                {m_planetaryWaterStatus="Ocean: animation follows nearby water detail.";return;}
                // Coalesce render turns onto absolute simulation time. Pausing
                // evaluates once and then does no work; reversing requests the
                // earlier time directly. A completed tile is never relabelled.
                const double seconds=std::floor(sample_seconds*20.0)/20.0;
                if(!std::isfinite(seconds))return;
                if(m_planetaryWaterPublishedSource==m_planetaryWaterSource
                    &&m_planetaryWaterPublishedSeconds==seconds)return;
                auto provider=m_explorationController->water_provider();
                if(!provider){m_planetaryWaterStatus="Ocean animation needs the project's linked FFT Water package.";return;}
                const auto binding=project_planetary_runtime::admitted_project_binding();
                if(!binding||binding->water_source_hash.empty())
                {m_planetaryWaterStatus="Ocean animation needs an approved Water source binding.";return;}
                try
                {
                    ensure_planetary_scheduler();
                    const auto root=std::filesystem::path{m_projectRoot};const auto identity=*m_meshPlanetarySource;
                    const auto source=m_planetaryWaterSource;const auto settings=*m_planetarySettings;
                    const auto water_source_hash=binding->water_source_hash;
                    const auto clock_generation=m_planetaryWaterClockGeneration;
                    std::uint64_t retained_display_bytes{};
                    for(const auto& [id,mesh]:m_meshes)
                        if(mesh&&mesh!=source->artifact)
                            retained_display_bytes+=mesh->vertices().size()*sizeof(asset::mesh::Vertex)
                                +mesh->indices().size()*sizeof(std::uint32_t)+64u;
                    auto submission=m_planetaryScheduler->submit_cancellable("Animate project FFT ocean",
                        [root,identity,source,provider,settings,seconds,water_source_hash,clock_generation,retained_display_bytes](std::stop_token stop) {
                            return sample_planetary_water(root,identity,source,provider,settings,seconds,
                                water_source_hash,clock_generation,retained_display_bytes,stop);
                        });
                    m_planetaryWaterCancellation=submission.cancellation;
                    m_planetaryWaterPending.emplace(std::move(submission.completion));
                    m_planetaryWaterNextTick=now+std::chrono::milliseconds{50};
                }
                catch(...)
                {m_planetaryWaterNextTick=now+std::chrono::seconds{1};m_planetaryWaterStatus="Ocean animation could not be queued. The previous surface is retained.";}
            }

            [[nodiscard]] static bool planetary_view_changed(const project_planetary::StreamView& current,
                const project_planetary::StreamView& previous) noexcept
            {
                const double radius=std::sqrt(current.camera[0]*current.camera[0]
                    +current.camera[1]*current.camera[1]+current.camera[2]*current.camera[2]);
                const double epsilon=std::clamp(std::abs(radius-10.0)*0.05,0.00001,0.025);
                if(current.viewport_height!=previous.viewport_height
                    ||std::abs(current.vertical_fov-previous.vertical_fov)>0.001
                    ||current.forward.has_value()!=previous.forward.has_value()
                    ||current.up.has_value()!=previous.up.has_value()
                    ||std::abs(current.aspect_ratio-previous.aspect_ratio)>0.001) return true;
                if(current.forward&&previous.forward)
                    for(std::size_t axis=0;axis<3u;++axis)
                        if(std::abs((*current.forward)[axis]-(*previous.forward)[axis])>0.01)return true;
                if(current.up&&previous.up)
                    for(std::size_t axis=0;axis<3u;++axis)
                        if(std::abs((*current.up)[axis]-(*previous.up)[axis])>0.01)return true;
                for(std::size_t axis=0;axis<3u;++axis)
                    if(std::abs(current.camera[axis]-previous.camera[axis])>epsilon) return true;
                return false;
            }

            void pump_planetary_runtime(const core::Context* context,int width,int height)
            {
                if(!m_planetarySettings || (!m_planetarySettings->stream_lod&&!runtime_water_visibility_pending())
                    || !m_planetaryEntity || !context) return;
                const auto entity=std::find_if(m_entities.begin(),m_entities.end(),
                    [&](const auto& candidate){return candidate.source_id==m_planetaryEntity;});
                if(entity==m_entities.end() || !entity->visible()) return;
                const auto camera=previewgrid::camera_for(context);
                const auto& transform=entity->transform;
                const auto meshScale=scene::mesh_render_scale(transform.scale,entity->mesh_asset);
                if(!meshScale)return;
                project_planetary::StreamView view;
                if(m_explorationPreview)
                {view.camera=m_explorationPreview->camera;view.simulation_seconds=m_explorationFrame.seconds;
                 view.forward=m_explorationPreview->forward;view.up=m_explorationPreview->up;}
                else
                {
                    const auto eye=previewgrid::camera_world_eye(camera);
                    const std::array<double,3> relative{eye[0]-double(transform.position[0]),
                        eye[1]-double(transform.position[1]),eye[2]-double(transform.position[2])};
                    const previewgrid::Vec3 rotation{transform.rotation[0],transform.rotation[1],transform.rotation[2]};
                    const std::array axes{previewgrid::rotate_euler_degrees({1,0,0},rotation),
                        previewgrid::rotate_euler_degrees({0,1,0},rotation),previewgrid::rotate_euler_degrees({0,0,1},rotation)};
                    const auto direction=previewgrid::normalize(previewgrid::subtract(camera.target,camera.eye));
                    std::array<double,3> localForward{},localUp{};
                    for(std::size_t axis=0;axis<3u;++axis)
                    {
                        if(!std::isfinite((*meshScale)[axis])||std::abs((*meshScale)[axis])<0.00001f)return;
                        view.camera[axis]=(relative[0]*axes[axis].x+relative[1]*axes[axis].y+relative[2]*axes[axis].z)/(*meshScale)[axis];
                        localForward[axis]=(double(direction.x)*axes[axis].x+double(direction.y)*axes[axis].y
                            +double(direction.z)*axes[axis].z)/(*meshScale)[axis];
                        localUp[axis]=(double(camera.up.x)*axes[axis].x+double(camera.up.y)*axes[axis].y
                            +double(camera.up.z)*axes[axis].z)/(*meshScale)[axis];
                    }
                    const double length=std::hypot(localForward[0],localForward[1],localForward[2]);
                    if(!std::isfinite(length)||length<1e-12)return;
                    for(auto& component:localForward)component/=length;
                    view.forward=localForward;
                    view.up=localUp;
                    if(!project_planetary::valid_stream_up(view))return;
                }
                view.viewport_height=static_cast<std::uint32_t>(std::clamp(height,64,16384));
                view.aspect_ratio=std::clamp(double(width)/double((std::max)(1,height)),0.1,10.0);
                view.vertical_fov=camera.fovRadians;
                const auto now=timing::Clock::now();
                if(m_planetaryPending &&m_planetaryPending->wait_for(std::chrono::milliseconds{0})==std::future_status::ready)
                {
                    m_planetaryNextTick=now+std::chrono::milliseconds{250};
                    try
                    {
                        auto result=m_planetaryPending->get();m_planetaryPending.reset();
                        if((!m_planetaryPaused||runtime_water_visibility_pending()) && m_planetaryRequestBody==m_planetarySettings->body_index
                            && (!m_explorationController || m_planetaryRequestBody==m_explorationFrame.anchor)
                            && m_planetaryRequestSettings==m_planetarySettings
                            && project_planetary::stream_view_reusable(view,m_planetaryView))
                        {
                            if(result.operation.ok &&!result.operation.cancelled
                                &&result.request_id==m_planetaryView.request_id&&result.request_id==m_planetarySequence)
                            {
                                if(result.snapshot_ready && result.snapshot)
                                {
                                    const auto budget=project_planetary::scene_mesh_budget(*m_planetarySettings);
                                    if(m_meshPlanetarySource
                                        &&result.verified_source
                                        &&result.verified_source->project_root==m_meshPlanetarySource->project_root
                                        &&result.verified_source->project_manifest==m_meshPlanetarySource->project_manifest
                                        &&result.verified_source->content_hash==m_meshPlanetarySource->content_hash
                                        &&result.snapshot->vertices().size()<=budget.artifact.maximum_vertices
                                        &&planetary_mesh_frame_fits(result.snapshot))
                                    {m_meshes[m_planetaryEntity]=std::move(result.snapshot);
                                     m_planetaryPublishedOcean=m_planetaryRequestSettings->ocean_surface;
                                     if(!runtime_water_visibility_pending())m_runtimeViewStatus="Water view updated. Terrain settings are unchanged.";
                                     m_planetaryMeshFrame=result.operation.mesh_frame;
                                     m_planetaryLodBody=m_planetaryRequestBody;
                                     reset_planetary_water();
                                     m_planetaryWaterSource=std::move(result.patch_snapshot);}
                                    else
                                    {
                                        m_planetaryFailed=true;
                                        m_planetaryStatus=!m_meshPlanetarySource||!result.verified_source
                                            ||result.verified_source->project_root!=m_meshPlanetarySource->project_root
                                            ||result.verified_source->project_manifest!=m_meshPlanetarySource->project_manifest
                                            ||result.verified_source->content_hash!=m_meshPlanetarySource->content_hash
                                            ?"Terrain LOD paused: its verified source no longer matches this running project. Rebuild to use changed source."
                                            :"Terrain LOD paused: the scene geometry or memory budget is full.";
                                    }
                                }
                                if(!m_planetaryFailed)m_planetaryStatus=std::move(result.operation.message);
                                m_planetaryContinue=result.pending!=0u||planetary_view_changed(view,m_planetaryView);
                                if(!m_planetaryFailed&&(result.pending!=0u||planetary_view_changed(view,m_planetaryView)))
                                    m_planetaryNextTick=now;
                            }
                            else if(result.waiting_for_worker ||result.operation.cancelled)
                            {m_planetaryContinue=true;m_planetaryStatus=std::move(result.operation.message);}
                            else
                            {
                                m_planetaryFailed=true;m_planetaryStatus=result.operation.ok
                                    ? "Terrain LOD returned evidence for another request." : std::move(result.operation.message);
                                logger::get("Engine.Planet").log(logger::LogLevel::WARN,m_planetaryStatus,std::source_location::current());
                            }
                        }
                        else if(!m_planetaryPaused)
                        {
                            m_planetaryContinue=true;
                            // A successful old-camera step is already retired.
                            // Discard its snapshot, then retarget immediately;
                            // worker failures and cancellation keep their backoff.
                            if(result.operation.ok&&!result.operation.cancelled
                                &&result.request_id==m_planetaryView.request_id
                                &&result.request_id==m_planetarySequence)
                                m_planetaryNextTick=now;
                        }
                    }
                    catch(const editor_tasks::TaskCancelled&)
                    {
                        m_planetaryPending.reset();
                        m_planetaryContinue=true;
                        m_planetaryStatus="Terrain detail cancelled. The previous mesh is retained.";
                    }
                    catch(...)
                    {m_planetaryPending.reset();m_planetaryFailed=true;m_planetaryStatus="Terrain LOD stopped after a worker failure. The saved terrain remains available.";}
                }
                if(m_planetaryPending ||((m_planetaryPaused||!m_planetarySettings->stream_lod)&&!runtime_water_visibility_pending())
                    ||m_planetaryFailed ||now<m_planetaryNextTick) return;
                if(!m_planetaryContinue &&!planetary_view_changed(view,m_planetaryView)) return;
                if(m_planetarySequence==std::numeric_limits<std::uint64_t>::max()) return;
                view.request_id=++m_planetarySequence;m_planetaryView=view;
                m_planetaryRequestBody=m_planetarySettings->body_index;
                m_planetaryRequestSettings=m_planetarySettings;
                try
                {
                    ensure_planetary_scheduler();
                    const auto root=std::filesystem::path{m_projectRoot};auto settings=*m_planetarySettings;
                    // A visibility change needs one admitted cut even when
                    // continuous LOD is disabled; it does not enable LOD in the saved recipe.
                    settings.stream_lod=true;
                    auto submission=m_planetaryScheduler->submit_cancellable("Stream project planetary terrain",
                        [root,settings,view](std::stop_token stop){return project_planetary::stream_step(root,settings,view,stop);});
                    m_planetaryCancellation=submission.cancellation;
                    m_planetaryPending.emplace(std::move(submission.completion));
                    m_planetaryNextTick=now+std::chrono::milliseconds{500};
                }
                catch(...){m_planetaryFailed=true;m_planetaryStatus="Could not queue project terrain detail.";}
            }

            void prepare_runtime_input()
            {
                m_projectInput.reset();
                m_projectInputSnapshot = {};
                m_projectActionFrame = {};
                m_projectControllerSampler.reset();
                m_inputFrameIndex = 0u;
                if (m_inputProfilePath.empty())
                {
                    m_projectInputStatus = "No project bindings declared; using the runtime camera preset.";
                    return;
                }
                if (m_gameplayRuntime && m_gameplayRuntime->input_profile())
                {
                    m_projectInput = *m_gameplayRuntime->input_profile();
                    return; // The 2D admission already validated this exact profile.
                }
                auto prepared = project_input::prepare_runtime_profile({
                    .project_id = m_projectId,
                    .project_root = std::filesystem::path{m_projectRoot},
                    .logical_path = m_inputProfilePath});
                m_projectInputStatus = std::move(prepared.diagnostic);
                m_projectInput = std::move(prepared.artifact);
            }

            [[nodiscard]] bool runtime_input_ready() const noexcept
            {
                return m_inputProfilePath.empty()
                    || (m_projectInput && m_projectActionFrame);
            }

            [[nodiscard]] float runtime_planar_axis(
                project_input::ActionSemantic semantic, float presetValue) const noexcept
            {
                if (m_inputProfilePath.empty())
                    return presetValue;
                if (!m_projectInput || !m_projectActionFrame)
                    return 0.0f;
                const auto* action = project_input::find_action(
                    m_projectActionFrame, semantic, *m_projectInput);
                return action ? static_cast<float>(action->value_q15)
                    / static_cast<float>(project_input::normalized_unit) : 0.0f;
            }

            void sample_runtime_input(const core::Context* ctx, bool suppressPhysicalInput)
            {
                m_projectInputSnapshot = {};
                m_projectActionFrame = {};
                if (!m_projectInput)
                    return;
                if (m_inputFrameIndex == (std::numeric_limits<std::uint64_t>::max)())
                {
                    m_projectInputStatus = "Project input sequence exhausted.";
                    return;
                }
                ++m_inputFrameIndex;
                auto snapshot = project_input_snapshot(*m_projectInput,
                    m_inputFrameIndex, ctx, !suppressPhysicalInput);
                const auto physicalControllers = controller_input::snapshot();
                project_input::InputSnapshot discarded{.frame_index = m_inputFrameIndex};
                const auto sampled = m_projectControllerSampler.sample(*m_projectInput,
                    physicalControllers, suppressPhysicalInput ? discarded : snapshot);
                if (!sampled)
                {
                    m_projectInputStatus = std::string{"Project controller input "}
                        + std::string{project_input_controller::sample_code_name(sampled.code)};
                    return;
                }
                auto evaluated = project_input::evaluate_action_frame(*m_projectInput, snapshot);
                if (!evaluated)
                {
                    m_projectInputStatus = "Project input evaluation was rejected.";
                    return;
                }
                m_projectInputSnapshot = std::move(snapshot);
                m_projectActionFrame = std::move(evaluated);
            }

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
                m_projectInput.reset();
                m_projectInputSnapshot = {};
                m_projectActionFrame = {};
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
                m_runtimeControlsVisible = false;
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
                float frameSeconds)
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

                if (!m_gameplayRuntime || !m_gameplayRuntime->active()
                    || !m_projectActionFrame)
                    return;

                const auto result = m_gameplayRuntime->advance(
                    m_projectInputSnapshot,
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

            void prepare_project_meshes()
            {
                m_meshes.clear();
                m_meshBaseColorTextures.clear();
                m_meshPlanetarySettings.reset();m_meshPlanetarySource.reset();m_meshFrameLimits={};
                const std::filesystem::path root{m_projectRoot};
                const auto settings=project_planetary::load_settings(root);
                if(settings)
                {
                    const auto attached=package_source::load_project_attachment(root,"planetary_terrain");
                    if(attached.source_attached())
                    {
                        const auto source=project_planetary::capture_source_generation(root,attached.content_hash);
                        if(source&&project_planetary::source_generation_is_current(root,*source))
                        {m_meshPlanetarySettings=settings;m_meshPlanetarySource=source;}
                    }
                }
                const auto budget=m_meshPlanetarySettings
                    ?project_planetary::scene_mesh_budget(*m_meshPlanetarySettings):project_planetary::SceneMeshBudget{};
                m_meshFrameLimits={budget.frame_indices,budget.frame_bytes};
                std::uint64_t mesh_bytes{}, mesh_indices{},ordinary_bytes{},ordinary_indices{};
                std::optional<editor_project_textures::ProjectTextureController> textures;
                for (const auto& entity : m_entities)
                {
                    if (!entity.mesh_asset) continue;
                    const bool planetary=entity.category=="Planetary Terrain"&&m_meshPlanetarySettings.has_value();
                    const auto& reference = *entity.mesh_asset;
                    const std::uint64_t bytes = 64u + std::uint64_t(reference.vertex_count) * sizeof(asset::mesh::Vertex)
                        + std::uint64_t(reference.index_count) * sizeof(std::uint32_t);
                    if (m_meshes.size() >= 64u || bytes>budget.cache_bytes-(std::min)(budget.cache_bytes,mesh_bytes)
                        || reference.index_count>budget.frame_indices-(std::min)(budget.frame_indices,mesh_indices)
                        || (!planetary&&(bytes>std::uint64_t{16u}*1024u*1024u-(std::min)(std::uint64_t{16u}*1024u*1024u,ordinary_bytes)
                            ||reference.index_count>196'608u-(std::min)(std::uint64_t{196'608u},ordinary_indices))))
                    {
                        logger::get("Engine.Assets").log(logger::LogLevel::WARN, "Project mesh residency or geometry budget exceeded; mesh was not loaded.", std::source_location::current());
                        continue;
                    }
                    auto limits=planetary?budget.artifact:asset::mesh::Limits{};
                    limits.maximum_bytes=(std::min)(limits.maximum_bytes,budget.cache_bytes-mesh_bytes);
                    auto loaded = asset::mesh::read_project_artifact(root, reference.logical_path, {reference.artifact_key},limits);
                    if (!loaded || loaded.artifact->vertices().size() != reference.vertex_count
                        || loaded.artifact->indices().size() != reference.index_count)
                    {
                        logger::get("Engine.Assets").log(logger::LogLevel::WARN, "Project mesh failed file/content admission: " + reference.logical_path, std::source_location::current());
                        continue;
                    }
                    m_meshes.emplace(entity.source_id, std::move(loaded.artifact));
                    if (entity.texture_material)
                    {
                        if (!textures) textures.emplace(m_projectId, root);
                        const auto& material = *entity.texture_material;
                        auto resolved = (material.alpha == scene::SceneTextureAlphaMode::opaque
                            || material.alpha == scene::SceneTextureAlphaMode::cutout)
                            ? textures->resolve_scene_material(material) : editor_project_textures::ResolvedTextureMaterial{};
                        if (resolved && resolved.artifact)
                        {
                            auto binding = std::make_shared<previewgrid::MeshBaseColorBinding>(
                                previewgrid::MeshBaseColorBinding{std::move(resolved.artifact),resolved.logical,resolved.sampler,
                                    material.alpha == scene::SceneTextureAlphaMode::cutout,material.alpha_cutoff,
                                    !material.logical_path.starts_with("Textures/Forest/reference_bark_foliage_v1")
                                        && !material.logical_path.starts_with("Textures/Forest/project_bark_foliage_")});
                            if (previewgrid::valid_mesh_base_color_binding(*binding))
                                m_meshBaseColorTextures.emplace(entity.source_id,std::move(binding));
                        }
                        if (!m_meshBaseColorTextures.contains(entity.source_id))
                        {
                            m_runtimeViewStatus = "Textured mesh drawing refused: its exact opaque/cutout base-color texture is unavailable.";
                            logger::get("Engine.Assets").log(logger::LogLevel::WARN,
                                m_runtimeViewStatus + " " + material.logical_path,std::source_location::current());
                        }
                    }
                    mesh_bytes += bytes;
                    mesh_indices += reference.index_count;
                    if(!planetary){ordinary_bytes+=bytes;ordinary_indices+=reference.index_count;}
                }
            }
            std::unordered_map<std::uint64_t, std::shared_ptr<const asset::mesh::Artifact>> m_meshes{};
            std::unordered_map<const void*,ProjectPlayWaterCache> m_waterCaches{};
            std::unordered_map<std::uint64_t,std::shared_ptr<const previewgrid::MeshBaseColorBinding>> m_meshBaseColorTextures{};
            std::optional<project_planetary::Settings> m_meshPlanetarySettings{};
            std::optional<project_planetary::SourceGeneration> m_meshPlanetarySource{};
            bool m_meshWireframe{},m_screenshotHeld{},m_wireframeHeld{};
            bool m_editorRuntimeControlled{};
            std::optional<platform::child_process::RuntimeMeshCommand> m_ownedRuntimeMeshCommand{};
            std::optional<bool> m_requestedMeshWireframe{};
            float m_runtimeViewportHeight{720.0f},m_runtimeViewportWidth{1280.0f};
            bool m_runtimeControlsVisible{true};
            std::unordered_map<const void*, gui::FrameLimitEditState> m_runtimeFrameLimitEdits{};
            bool m_runtimeInspectionLight{};
            bool m_runtimeCloudsVisible{true},m_runtimeWaterVisible{true},m_planetaryPublishedOcean{};
            bool m_runtimeCloudBackendUnavailable{};
            std::string m_runtimeViewStatus{};
            std::string m_terrainAttachmentStatus{};
            scene_runtime::ReachObjectiveSession m_reachObjective{};
            std::string m_reachTargetName{};
            int m_screenshotAfterFrames{};
            std::string m_screenshotStatus{};
            previewgrid::MeshFrameLimits m_meshFrameLimits{};
            std::unique_ptr<project_planetary_runtime::Controller> m_explorationController{};
            std::array<double,3> m_explorationRenderOrigin{};
            std::optional<previewgrid::Camera> m_explorationRenderCamera{};
            project_planetary_runtime::Frame m_explorationFrame{};
            std::optional<epochengine::timeline::TimelineState> m_runtimePresentation{};
            std::optional<project_planetary_runtime::PreviewFrame> m_explorationPreview{};
            project_planetary_runtime::Input m_explorationInput{};
            project_planetary::SessionFile m_explorationSession{};
            std::string m_explorationStatus{};
            std::string m_explorationAdvanceStatus{};
            gui::Vec2 m_explorationLastMouse{};
            bool m_explorationMouseHeld{};
            std::optional<std::uint32_t> m_planetaryLodBody{};
            std::optional<project_planetary::GeneratedMeshFrame> m_planetaryMeshFrame{};
            std::uint32_t m_planetaryRequestBody{};
            std::optional<project_planetary::Settings> m_planetarySettings{};
            std::optional<project_planetary::Settings> m_planetaryRequestSettings{};
            std::unique_ptr<editor_tasks::Scheduler> m_planetaryScheduler{};
            std::unordered_map<const void*,PlanetaryFramePublication> m_planetaryFramePublications{};
            std::optional<std::future<project_planetary::StreamResult>> m_planetaryPending{};
            editor_tasks::CancellationTicket m_planetaryCancellation{};
            std::shared_ptr<const project_planetary_snapshot::Snapshot> m_planetaryWaterSource{};
            std::shared_ptr<const project_planetary_snapshot::Snapshot> m_planetaryWaterPublishedSource{};
            std::optional<double> m_planetaryWaterPublishedSeconds{};
            std::optional<double> m_planetaryWaterObservedSeconds{},m_planetaryWaterObservedScale{};
            std::uint64_t m_planetaryWaterClockGeneration{};
            timing::Clock::time_point m_planetaryWaterObservedTick{};
            int m_planetaryWaterClockDirection{};
            std::optional<std::future<PlanetaryWaterResult>> m_planetaryWaterPending{};
            editor_tasks::CancellationTicket m_planetaryWaterCancellation{};
            timing::Clock::time_point m_planetaryWaterNextTick{};
            timing::Clock::time_point m_planetaryWaterEvidenceTick{};
            std::string m_planetaryWaterStatus{};
            bool m_planetaryWaterSourceRefused{};
            project_planetary::StreamView m_planetaryView{};
            std::uint64_t m_planetaryEntity{},m_planetarySequence{};
            timing::Clock::time_point m_planetaryNextTick{};
            std::string m_planetaryStatus{};
            bool m_planetaryPaused{},m_planetaryFailed{},m_planetaryContinue{true};
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
            std::optional<project_input::CompiledInputProfile> m_projectInput{};
            project_input::InputSnapshot m_projectInputSnapshot{};
            project_input::ActionFrame m_projectActionFrame{};
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
            epochengine::lighting::LightManager m_lightingManager{128};
            epochengine::lighting::LightHandle m_sunlightHandle{};
            timing::Clock::time_point m_lastFrame{};
            timing::Clock::time_point m_lastFrameEntry{};
            RuntimeFrameEvidence m_runtimeFrameEvidence{};
            bool m_hasLastFrame{ false };
            epochengine::previewgrid::CameraMode m_cameraMode{ epochengine::previewgrid::CameraMode::Editor };
            input::ProfilePreset m_inputProfile{ input::ProfilePreset::EditorDefault };
            input::InputProfile m_runtimeInputProfile{input::make_profile(m_inputProfile)};
            std::unordered_map<const void*, bool> m_cameraApplied{};
            bool m_seedCamerasUnchanged{};
            bool m_hasPlanetRecipe{};
            std::unordered_map<const void*, std::uint64_t> m_canvas2dPublished{};
            PreviewLookState m_lookState{};
        };

