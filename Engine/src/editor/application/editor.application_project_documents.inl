// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] EditorWorkspaceTab application_dock_tab(
            EditorApplicationDock dock) noexcept
        {
            switch (dock)
            {
            case EditorApplicationDock::Assets:
                return EditorWorkspaceTab::Assets;
            case EditorApplicationDock::Project:
                return EditorWorkspaceTab::Project;
            case EditorApplicationDock::Output:
            default:
                return EditorWorkspaceTab::Output;
            }
        }

        [[nodiscard]] previewgrid::CameraMode application_camera_mode(
            EditorCameraPolicy policy) noexcept
        {
            return policy == EditorCameraPolicy::Canvas2D
                ? previewgrid::CameraMode::Canvas2D
                : previewgrid::CameraMode::Editor;
        }

        void apply_application_profile(
            EditorState& state,
            const EditorApplicationProfile& application)
        {
            state.applicationKind = application.kind;
            state.mainSurface = application.default_surface;
            if (main_surface_is_ai_authoring(state.mainSurface))
                state.aiAuthoringSurface = state.mainSurface;
            state.aiAuthoringSurfaceRestoreQueued = false;
            state.workspaceTab = application_dock_tab(application.initial_dock);
            state.outlinerToolTab =
                application.kind == EditorApplicationKind::GuiEditor
                    ? OutlinerToolTab::Gui
                    : OutlinerToolTab::World;
            state.game2DGuiWorkspace = application.kind == EditorApplicationKind::GuiEditor;
            state.workspaceCompanionsPending = true;
            state.projectCameraMode = application_camera_mode(application.camera_policy);
            state.viewportCameraMode = application_camera_mode(application.camera_policy);
            state.showOutliner = application.panes.outliner;
            state.showInspector = application.panes.inspector;
            state.showConsoleDock = application.panes.console;
            state.showAiChat = application.panes.ai_chat;
        }

        void reset_project_bound_editor_state(
            EditorState& state,
            std::string_view guiStatus)
        {
            state.selectedProjectFile.clear();
            state.selectedAssetPath.clear();
            state.assetBrowserFolder.clear();
            state.assetWorkspaceSection = 0u;
            state.projectWorkspaceSection = 0u;
            state.guiDocumentTab = 0u;
            state.guiCanvasWidgetDrag = {};
            state.guiCanvasWidgetDragHandle = {};
            state.assetGridScroll = 0.0f;
            state.assetGridVisibleIds.clear();
            state.selectedProjectAudioCueId = 0u;
            state.projectAudioStatus =
                "Open Project Audio to manage decoded cues and buses.";
            state.projectTextures.reset();
            state.projectInputSettings.reset();
            state.projectInputSettingsStatus =
                "Open Input Manager to load the active project's bindings.";
            state.guiDocument.reset();
            state.guiDocumentRoot = {};
            state.guiDocumentWidgets.clear();
            state.guiProjectionTombstones.clear();
            state.guiDocumentStatus = std::string{guiStatus};
            state.guiDocumentSourcePath.clear();
            state.guiDocumentSavedRevision = 0u;
            state.guiDocumentLoadAttempted = false;
            state.guiContentEditorEntityId =
                scene::kInvalidSceneObjectId;
            state.guiContentPrimary.clear();
            state.guiContentSecondary.clear();
            state.guiActionEditor.clear();
            state.guiTooltipEditor.clear();
            state.guiTemplatePreset = 0u;
            state.guiDocumentReplaceArmed = false;
            state.projectTextureThumbnails.clear();
            state.projectSourceTextureThumbnails.clear();
            state.projectTextureThumbnailAtlasSignature = 0u;
            state.plantLabDocument = {};
            state.plantLabCompiled = {};
            state.plantLabPublishedArtifact = {};
            state.plantLabCompiledRevision = 0u;
            state.plantLabCompiledContentHash = 0u;
            state.plantLabDocumentInitialized = false;
            state.forestPreviewEntities.clear();
            state.forestPreviewRevision = 0u;
            state.forestPreviewContentHash = 0u;
            state.forestProjectCamera.reset();
            state.aiAuthoringPlan = {};
            state.aiProjectScriptDraft.clear();
            state.aiProjectScriptRequested = false;
            state.aiProjectScriptRunAfterBuild = state.aiProjectScriptRepairQueued = false;
            state.aiProjectScriptCompilerDiagnostics.clear();
            state.scriptCubeOscillators.clear();
            state.scriptOscillator = {};
            if (state.scriptBuildCancellation.valid() && state.taskScheduler)
                (void)state.taskScheduler->cancel(state.scriptBuildCancellation);
            if (state.aiAuthoringApplyCancellation.valid() && state.taskScheduler)
                (void)state.taskScheduler->cancel(state.aiAuthoringApplyCancellation);
            state.aiAuthoringApplyPending.reset();
            state.aiAuthoringApplyProgress.reset();
            state.aiAuthoringApplyCancellation = {};
            state.aiToolPlan = {};
            state.aiAuthoringAwaitingReply = false;
            state.aiToolAwaitingReply = false;
            state.aiAuthoringPlanApplied = false;
            state.aiToolPlanApplied = false;
            state.aiAuthoringPlanForGoal = false;
            state.aiToolStatus =
                "Project changed; request a new guarded tool plan.";
            if (!state.aiToolTestPending)
                state.aiToolTestProjectId.clear();
            state.aiGoal.clear();
            state.aiGoalDraft.clear();
            state.aiGoalActive = false;
            state.aiGoalRunning = false;
            state.aiGoalEditing = false;
            state.aiGoalPlanNextQueued = false;
            state.aiGoalCompletedMilestones = 0u;
            state.aiGoalAttemptedMilestoneSignatures.clear();
            state.aiGoalLastMilestoneSignature.clear();
            state.aiGoalLastMilestoneReceipt.clear();
            state.aiAuthoringIgnoredThroughGeneration = 0u;
            state.aiAuthoringStatus =
                "Project changed; request a new AI authoring plan.";
            state.projectTextureStatus =
                "Create an editable texture or select a BMP, TGA, PPM, or .epoch_texture source.";
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            state.tileMapWorkspace.reset();
            state.tileMapStatus =
                "Open the 2D Scene/UI workspace to create a map.";
            state.tileMapTileExtent = 32u;
#endif
            state.sceneDocument = authoring::scene::SceneDocument{};
        }

        void set_project(EditorState& state, std::string_view projectId, bool writeLog)
        {
            const auto* profile = editor_find_project_profile(projectId);
            if (!profile)
                profile = &editor_default_project_profile();
            const auto* application = editor_application_for_project(profile->id);
            apply_application_profile(
                state,
                application ? *application : standard_editor_application());

            state.helpersVisible = true;
            state.projectId = std::string(profile->id);
            state.projectPickerId = state.projectId;
            const std::uint64_t projectKey =
                project_assets::stable_project_identity(state.projectId);
            const std::uint64_t previousProjectKey =
                state.projectLifecycle.evidence()
                    .generations.selected_project.project_key;
            if (!state.projectLifecycle.select_project(projectKey))
                state.projectLifecycle.clear_project();
            if (previousProjectKey != projectKey)
                state.projectBuildAcceptedOutputDigest = {};
            state.projectName = std::string(profile->display_name);
            state.projectRoot = std::string(profile->root_path);
            reset_project_bound_editor_state(
                state,
                "GUI document will load from the selected project.");
            state.projectScenePath = std::string(profile->scene_path);
            state.projectManifest = std::string(profile->manifest_path);
            state.projectTemplate = std::string(profile->template_family);
            state.projectKind = std::string(editor_project_kind_name(profile->kind));
            state.activeWorld = std::string(profile->world_name);
            state.activeScript = std::string(profile->default_script);
            state.activeRuntimeScene = std::string(profile->runtime_scene_id);
            state.projectStatus = "Selected project profile. Use File > Save Project or the centered Run button to materialize/update generated project files.";
            state.projectBuildStatus = "Build Project creates or refreshes a repo-local child executable for the active shell after an explicit save/run.";
            state.scriptBuildStatus = "Select a script to validate or run against the active project shell.";

            state.entities.clear();
            for (const auto& seed : editor_seed_entities_for_project(profile->id))
            {
                state.entities.push_back(editor_entity_from_seed(seed));
            }

            const bool loadedScene = load_editor_scene_snapshot(state);
            if (loadedScene)
                (void)state.projectLifecycle.record_scene_commit(state.sceneDocumentRevision);
            else
                (void)rebuild_editor_scene_document(state, "Load project scene seed");

            synchronize_editor_selection(state);
            if (writeLog)
                push_editor_log(
                    state,
                    std::string("[project] Loaded ")
                    + state.projectName
                    + " -> "
                    + state.activeRuntimeScene
                    + ".");
        }

        [[nodiscard]] const EditorScriptProfile* active_script_profile(const EditorState& state) noexcept
        {
            for (const auto& script : editor_script_profiles())
            {
                if (script.id == state.activeScript)
                    return &script;
            }
            return nullptr;
        }

        [[nodiscard]] std::string make_entity_name(const EditorState& state, std::string_view base,
            std::size_t ordinalOffset = 0u)
        {
            std::size_t ordinal = 1;
            for (const auto& entity : state.entities)
            {
                if (!entity.name.starts_with(base) || entity.name.size() <= base.size()
                    || entity.name[base.size()] != '_') continue;
                const std::string_view suffix{entity.name.data() + base.size() + 1u,
                    entity.name.size() - base.size() - 1u};
                std::size_t value{};
                const auto converted = std::from_chars(suffix.data(), suffix.data() + suffix.size(), value);
                if (converted.ec == std::errc{} && converted.ptr == suffix.data() + suffix.size()
                    && value < (std::numeric_limits<std::size_t>::max)())
                    ordinal = (std::max)(ordinal, value + 1u);
            }

            return epochengine::format_text("{}_{:02}", base, ordinal + ordinalOffset);
        }

        [[nodiscard]] std::array<float, 3> next_entity_position(
            const EditorState& state,
            float baseY = 0.5f, std::size_t ordinalOffset = 0u)
        {
            const std::size_t worldEntityCount = static_cast<std::size_t>(
                std::count_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [](const EditorEntity& entity)
                    {
                        return entity.category != "UI"
                            && !entity.type.starts_with("Gui");
                    }));
            const float offset =
                static_cast<float>((worldEntityCount + ordinalOffset) % 5u) * 1.35f;
            return {
                -2.7f + offset,
                baseY,
                1.6f
                    - static_cast<float>(((worldEntityCount + ordinalOffset) / 5u) % 4u)
                        * 1.15f
            };
        }

        [[nodiscard]] bool is_gui_authoring_entity(
            const EditorEntity& entity) noexcept
        {
            return entity.type.starts_with("Gui")
                && entity.type != "GuiDocument"
                && entity.type != "GuiSafeArea";
        }

        [[nodiscard]] bool is_screen_space_gui_entity(
            const EditorEntity& entity) noexcept
        {
            return entity.category == "UI"
                || is_gui_authoring_entity(entity);
        }

        [[nodiscard]] float primary_ground_top_y(
            const EditorState& state) noexcept
        {
            for (const auto& entity : state.entities)
            {
                if (entity.visible
                    && !is_screen_space_gui_entity(entity)
                    && entity.type == "Ground")
                {
                    const auto elevation =
                        scene::surface_alignment::support_surface_elevation(
                            scene::surface_alignment::centered_unit_bounds,
                            entity.position[1],
                            entity.scale[1]);
                    if (elevation)
                        return *elevation;
                }
            }
            return 0.0f;
        }

        [[nodiscard]] bool align_editor_entity_bottom_to_surface(
            EditorEntity& entity,
            float surfaceElevation,
            scene::surface_alignment::VerticalBounds localBounds =
                scene::surface_alignment::centered_unit_bounds) noexcept
        {
            const auto aligned =
                scene::surface_alignment::align_bottom_to_surface(
                    localBounds,
                    entity.position[1],
                    entity.scale[1],
                    surfaceElevation);
            if (!aligned)
                return false;
            entity.position[1] = aligned->origin_y;
            return true;
        }

        [[nodiscard]] bool align_editor_entity_group_to_surface(
            std::vector<EditorEntity>& entities,
            float surfaceElevation) noexcept
        {
            if (entities.empty())
                return true;

            std::optional<scene::surface_alignment::VerticalBounds> groupBounds{};
            for (const EditorEntity& entity : entities)
            {
                const auto world =
                    scene::surface_alignment::transform_vertical_bounds(
                        scene::surface_alignment::centered_unit_bounds,
                        entity.position[1],
                        entity.scale[1]);
                if (!world)
                    return false;
                if (!groupBounds)
                    groupBounds = *world;
                else
                    groupBounds =
                        scene::surface_alignment::merge_vertical_bounds(
                            *groupBounds,
                            *world);
                if (!groupBounds)
                    return false;
            }
            if (!groupBounds)
                return false;

            const auto aligned =
                scene::surface_alignment::align_bottom_to_surface(
                    *groupBounds,
                    0.0f,
                    1.0f,
                    surfaceElevation);
            if (!aligned)
                return false;
            for (EditorEntity& entity : entities)
                entity.position[1] += aligned->translation_y;
            return true;
        }

        [[nodiscard]] authoring::gui::WidgetDescriptor
        gui_widget_descriptor_from_entity(const EditorEntity& entity)
        {
            using namespace authoring::gui;

            WidgetDescriptor descriptor{};
            descriptor.name = entity.name;
            descriptor.layout.flow = LayoutFlow::absolute;
            descriptor.layout.x = (entity.position[0] + 3.2f) * 100.0f;
            descriptor.layout.y = (1.8f - entity.position[1]) * 100.0f;
            descriptor.layout.width = (std::max)(20.0f, entity.scale[0] * 100.0f);
            descriptor.layout.height = (std::max)(20.0f, entity.scale[1] * 100.0f);
            descriptor.layout.minimum_width = 16.0f;
            descriptor.layout.minimum_height = 16.0f;
            descriptor.interaction.enabled = true;
            descriptor.interaction.visible = true;

            const std::string assetPath = entity.textureMaterial
                ? entity.textureMaterial->logical_path
                : "epoch://gui/unassigned";

            if (entity.type == "GuiPanel")
            {
                descriptor.kind = WidgetKind::panel;
                descriptor.content = PanelContent{entity.name};
            }
            else if (entity.type == "GuiButton")
            {
                descriptor.kind = WidgetKind::button;
                descriptor.content = ButtonContent{entity.name};
                descriptor.interaction.focusable = true;
                descriptor.interaction.accepts_pointer = true;
                descriptor.interaction.action = "interact";
            }
            else if (entity.type == "GuiText")
            {
                descriptor.kind = WidgetKind::text;
                descriptor.content = TextContent{entity.name, true};
            }
            else if (entity.type == "GuiImage")
            {
                descriptor.kind = WidgetKind::image;
                descriptor.content = ImageContent{
                    assetPath,
                    entity.name,
                    true};
                descriptor.interaction.accepts_pointer = true;
            }
            else if (entity.type == "GuiImageButton")
            {
                descriptor.kind = WidgetKind::image_button;
                descriptor.content = ImageButtonContent{
                    assetPath,
                    entity.name,
                    true};
                descriptor.interaction.focusable = true;
                descriptor.interaction.accepts_pointer = true;
                descriptor.interaction.action = "interact";
            }
            else if (entity.type == "GuiTextInput")
            {
                descriptor.kind = WidgetKind::text_input;
                descriptor.content = TextInputContent{
                    {},
                    entity.name,
                    4'096u,
                    false,
                    false};
                descriptor.interaction.focusable = true;
                descriptor.interaction.accepts_pointer = true;
            }
            else if (entity.type == "GuiSlider")
            {
                descriptor.kind = WidgetKind::slider;
                descriptor.content = SliderContent{
                    entity.name,
                    0.0,
                    1.0,
                    0.5,
                    0.01};
                descriptor.interaction.focusable = true;
                descriptor.interaction.accepts_pointer = true;
            }
            else if (entity.type == "GuiScrollArea")
            {
                descriptor.kind = WidgetKind::scroll_area;
                descriptor.content = ScrollAreaContent{
                    0.0f,
                    0.0f,
                    false,
                    true};
                descriptor.layout.clip_children = true;
                descriptor.interaction.accepts_pointer = true;
            }
            else if (entity.type == "GuiTabSet")
            {
                descriptor.kind = WidgetKind::tab_set;
                descriptor.content = TabSetContent{};
                descriptor.interaction.focusable = true;
                descriptor.interaction.accepts_pointer = true;
            }
            else
            {
                descriptor.kind = WidgetKind::panel;
                descriptor.content = PanelContent{entity.name};
            }
            return descriptor;
        }

        inline constexpr std::uint64_t kMaximumProjectGuiDocumentBytes =
            64ull * 1024ull * 1024ull;

        [[nodiscard]] std::filesystem::path project_gui_document_path(
            const EditorState& state)
        {
            if (state.projectRoot.empty())
                return {};
            const auto root = resolve_editor_path(std::filesystem::path{state.projectRoot});
            if (root.empty()) return {};
            return root / "Assets" / "Gui" / "main.epochgui";
        }

        [[nodiscard]] std::optional<std::vector<std::byte>>
        read_project_gui_document_bytes(
            const std::filesystem::path& path,
            std::string& failure)
        {
            failure.clear();
            try
            {
                std::error_code error{};
                if (!std::filesystem::exists(path, error) || error)
                {
                    failure = error
                        ? "GUI source existence check failed."
                        : "GUI source does not exist.";
                    return std::nullopt;
                }
                const std::filesystem::file_status status =
                    std::filesystem::symlink_status(path, error);
                if (error
                    || std::filesystem::is_symlink(status)
                    || !std::filesystem::is_regular_file(status))
                {
                    failure =
                        "GUI source must be a regular non-symlink file.";
                    return std::nullopt;
                }

                const std::uintmax_t fileSize =
                    std::filesystem::file_size(path, error);
                if (error
                    || fileSize == 0u
                    || fileSize > kMaximumProjectGuiDocumentBytes
                    || fileSize > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::streamsize>::max)()))
                {
                    failure =
                        "GUI source exceeds the bounded document budget.";
                    return std::nullopt;
                }

                std::vector<std::byte> bytes(
                    static_cast<std::size_t>(fileSize));
                std::ifstream stream{path, std::ios::binary};
                if (!stream)
                {
                    failure = "GUI source could not be opened.";
                    return std::nullopt;
                }
                stream.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!stream
                    || stream.gcount()
                        != static_cast<std::streamsize>(bytes.size()))
                {
                    failure = "GUI source read was incomplete.";
                    return std::nullopt;
                }
                return bytes;
            }
            catch (const std::bad_alloc&)
            {
                failure = "GUI source allocation failed.";
                return std::nullopt;
            }
            catch (...)
            {
                failure = "GUI source read failed.";
                return std::nullopt;
            }
        }

        [[nodiscard]] std::string_view gui_entity_type_for_widget(
            authoring::gui::WidgetKind kind) noexcept;

        [[nodiscard]] EditorEntity gui_projection_entity(
            const authoring::gui::WidgetView& widget,
            std::size_t order);

        [[nodiscard]] authoring::gui::TemplatePreset
            project_gui_starter_template(const EditorState& state) noexcept
        {
            const auto* projectProfile =
                editor_find_project_profile(state.projectId);
            const bool gameProject =
                (projectProfile
                    && projectProfile->kind == EditorProjectKind::Game)
                || state.projectKind
                    == editor_project_kind_name(EditorProjectKind::Game);
            return gameProject
                ? authoring::gui::TemplatePreset::game_hud
                : authoring::gui::TemplatePreset::desktop_app;
        }

        [[nodiscard]] bool load_project_gui_document(EditorState& state)
        {
            using namespace authoring::gui;

            state.guiDocumentLoadAttempted = true;
            const std::filesystem::path source =
                project_gui_document_path(state);
            state.guiDocumentSourcePath = source.empty()
                ? std::string{}
                : display_project_path(source);
            if (source.empty())
            {
                state.guiDocumentStatus =
                    "GUI source path is unavailable for this project.";
                return false;
            }

            std::error_code existenceError{};
            if (!std::filesystem::exists(source, existenceError))
            {
                state.guiDocumentStatus = existenceError
                    ? "GUI source existence check failed."
                    : "No saved GUI source exists; a new document will be created.";
                return false;
            }

            std::string failure{};
            auto bytes = read_project_gui_document_bytes(source, failure);
            if (!bytes)
            {
                state.guiDocumentStatus = std::move(failure);
                return false;
            }

            const DeserializedGuiDocument decoded =
                deserialize_gui_document(*bytes);
            if (!decoded)
            {
                state.guiDocumentStatus = epochengine::format_text(
                    "GUI source rejected: {}.",
                    snapshot_codec_code_name(decoded.code));
                return false;
            }

            const bool legacyGenerated =
                is_legacy_generated_root_only_document(*decoded.snapshot);
            std::optional<GuiDocument> migrated{};
            if (legacyGenerated)
            {
                migrated = migrate_legacy_generated_root_only_document(
                    *decoded.snapshot,
                    project_gui_starter_template(state));
                if (!migrated)
                {
                    state.guiDocumentStatus =
                        "Legacy generated GUI source migration failed.";
                    return false;
                }
            }

            std::unique_ptr<GuiDocument> document{};
            try
            {
                document = legacyGenerated
                    ? std::make_unique<GuiDocument>(std::move(*migrated))
                    : std::make_unique<GuiDocument>(*decoded.snapshot);
            }
            catch (...)
            {
                state.guiDocumentStatus =
                    "GUI source reconstruction allocation failed.";
                return false;
            }
            if (!document
                || !document->valid()
                || document->validate()
                || document->roots().size() != 1u)
            {
                state.guiDocumentStatus =
                    "GUI source reconstructed an invalid document.";
                return false;
            }

            const WidgetHandle root = document->roots().front();
            const auto rootView = document->widget(root);
            if (!rootView
                || rootView->descriptor.kind != WidgetKind::canvas)
            {
                state.guiDocumentStatus =
                    "GUI source has no canonical canvas root.";
                return false;
            }

            state.guiDocument = std::move(document);
            state.guiDocumentRoot = root;
            state.guiDocumentWidgets.clear();

            const std::vector<WidgetView> widgets =
                state.guiDocument->widgets();
            std::unordered_set<std::uint64_t> assigned{};
            const auto widgetKey = [](WidgetHandle handle) noexcept
            {
                return (static_cast<std::uint64_t>(handle.index) << 32u)
                    | handle.generation;
            };
            const scene::SceneObjectId previousSelection =
                state.selectedEntityId;

            for (const EditorEntity& entity : state.entities)
            {
                if (!is_gui_authoring_entity(entity)
                    || entity.sceneObjectId == scene::kInvalidSceneObjectId)
                {
                    continue;
                }
                const WidgetKind expected =
                    gui_widget_descriptor_from_entity(entity).kind;
                const auto match = std::find_if(
                    widgets.begin(),
                    widgets.end(),
                    [&](const WidgetView& widget)
                    {
                        return widget.descriptor.kind == expected
                            && widget.descriptor.name == entity.name
                            && !assigned.contains(widgetKey(widget.handle));
                    });
                if (match == widgets.end())
                    continue;
                state.guiDocumentWidgets.insert_or_assign(
                    entity.sceneObjectId,
                    match->handle);
                assigned.insert(widgetKey(match->handle));
            }

            // A saved GUI document is authoritative even when its disposable
            // scene projections were not present in the scene snapshot. Build
            // any missing projections now; otherwise synchronize_gui_document
            // interprets every unprojected saved widget as deleted and removes
            // the complete document on first display.
            std::size_t projectionOrder = 0u;
            for (const WidgetView& widget : widgets)
            {
                if (widget.handle == root
                    || assigned.contains(widgetKey(widget.handle))
                    || gui_entity_type_for_widget(
                        widget.descriptor.kind).empty())
                {
                    continue;
                }

                EditorEntity entity =
                    gui_projection_entity(widget, projectionOrder++);
                if (!create_editor_scene_entity(
                        state,
                        std::move(entity),
                        "Restore saved GUI widget projection"))
                {
                    state.guiDocumentStatus =
                        "GUI source loaded, but one saved widget projection could not be restored.";
                    continue;
                }
                state.guiDocumentWidgets.insert_or_assign(
                    state.selectedEntityId,
                    widget.handle);
                assigned.insert(widgetKey(widget.handle));
            }

            if (previousSelection != scene::kInvalidSceneObjectId
                && editor_entity_index(state, previousSelection))
            {
                state.selectedEntityId = previousSelection;
                synchronize_editor_selection(state);
            }
            else
            {
                clear_editor_selection(state);
            }

            state.guiDocumentSavedRevision = legacyGenerated
                ? 0u
                : state.guiDocument->revision().sequence;
            (void)project_gui_document_layout(state);
            state.guiDocumentStatus = legacyGenerated
                ? epochengine::format_text(
                    "Migrated legacy generated root-only GUI from {} to the project starter; save GUI or Project to commit it.",
                    state.guiDocumentSourcePath)
                : epochengine::format_text(
                    "Loaded GUI source revision {} from {}.",
                    state.guiDocumentSavedRevision,
                    state.guiDocumentSourcePath);
            return true;
        }

        [[nodiscard]] authoring::gui::GuiDocument*
        synchronize_gui_document(EditorState& state)
        {
            using namespace authoring::gui;

            if ((!state.guiDocument || !state.guiDocument->valid())
                && !state.guiDocumentLoadAttempted)
            {
                (void)load_project_gui_document(state);
            }

            if (!state.guiDocument || !state.guiDocument->valid())
            {
                auto starter = make_template_document(
                    project_gui_starter_template(state));
                if (!starter)
                {
                    state.guiDocumentStatus =
                        "GUI starter document initialization failed.";
                    state.guiDocument.reset();
                    state.guiDocumentRoot = {};
                    state.guiDocumentWidgets.clear();
                    return nullptr;
                }
                state.guiDocument = std::make_unique<GuiDocument>(
                    std::move(*starter));
                state.guiDocumentRoot =
                    state.guiDocument->roots().front();
                state.guiDocumentWidgets.clear();
                state.guiDocumentLoadAttempted = true;
                state.guiDocumentSavedRevision = 0u;
                state.guiDocumentSourcePath =
                    display_project_path(project_gui_document_path(state));
            }

            std::unordered_set<scene::SceneObjectId> retained{};
            for (const EditorEntity& entity : state.entities)
            {
                if (!is_gui_authoring_entity(entity)
                    || entity.sceneObjectId == scene::kInvalidSceneObjectId)
                {
                    continue;
                }

                retained.insert(entity.sceneObjectId);
                WidgetDescriptor descriptor =
                    gui_widget_descriptor_from_entity(entity);
                const auto mapped =
                    state.guiDocumentWidgets.find(entity.sceneObjectId);
                if (mapped == state.guiDocumentWidgets.end()
                    || !state.guiDocument->widget(mapped->second))
                {
                    const WidgetKind createdKind = descriptor.kind;
                    const MutationResult created =
                        state.guiDocument->create_widget(
                            std::move(descriptor),
                            state.guiDocumentRoot);
                    if (created)
                    {
                        state.guiDocumentWidgets.insert_or_assign(
                            entity.sceneObjectId,
                            created.widget);
                        if (createdKind == WidgetKind::tab_set)
                        {
                            constexpr std::array<std::string_view, 3>
                                defaultTabs{"Main", "Details", "Events"};
                            WidgetHandle selectedPage{};
                            for (const std::string_view label : defaultTabs)
                            {
                                WidgetDescriptor page{};
                                page.kind = WidgetKind::tab_page;
                                page.name = std::string{label};
                                page.layout.flow = LayoutFlow::overlay;
                                page.layout.width_rule = SizeRule::fill;
                                page.layout.height_rule = SizeRule::fill;
                                page.content = TabPageContent{
                                    std::string{label},
                                    true};
                                const MutationResult pageCreated =
                                    state.guiDocument->create_widget(
                                        std::move(page),
                                        created.widget);
                                if (pageCreated && !selectedPage)
                                    selectedPage = pageCreated.widget;
                            }
                            if (selectedPage)
                            {
                                (void)state.guiDocument->update_content(
                                    created.widget,
                                    TabSetContent{selectedPage});
                            }
                        }
                    }
                    else
                    {
                        state.guiDocumentStatus =
                            "GUI widget admission failed: "
                            + std::string(result_code_name(created.code));
                    }
                    continue;
                }

                const WidgetHandle widget = mapped->second;
                const auto existing = state.guiDocument->widget(widget);
                if (!existing)
                    continue;

                LayoutDescriptor mergedLayout =
                    existing->descriptor.layout;
                mergedLayout.x = descriptor.layout.x;
                mergedLayout.y = descriptor.layout.y;
                mergedLayout.width = descriptor.layout.width;
                mergedLayout.height = descriptor.layout.height;
                mergedLayout.minimum_width =
                    descriptor.layout.minimum_width;
                mergedLayout.minimum_height =
                    descriptor.layout.minimum_height;
                (void)state.guiDocument->update_layout(
                    widget,
                    std::move(mergedLayout));

                if ((descriptor.kind == WidgetKind::image
                        || descriptor.kind == WidgetKind::image_button)
                    && entity.textureMaterial)
                {
                    if (descriptor.kind == WidgetKind::image)
                    {
                        auto content = std::get<ImageContent>(
                            existing->descriptor.content);
                        content.asset_path =
                            entity.textureMaterial->logical_path;
                        (void)state.guiDocument->update_content(
                            widget,
                            std::move(content));
                    }
                    else
                    {
                        auto content = std::get<ImageButtonContent>(
                            existing->descriptor.content);
                        content.asset_path =
                            entity.textureMaterial->logical_path;
                        (void)state.guiDocument->update_content(
                            widget,
                            std::move(content));
                    }
                }
            }

            for (auto it = state.guiDocumentWidgets.begin();
                 it != state.guiDocumentWidgets.end();)
            {
                if (retained.contains(it->first))
                {
                    ++it;
                    continue;
                }
                (void)state.guiDocument->remove_widget(it->second);
                it = state.guiDocumentWidgets.erase(it);
            }

            const auto metrics = state.guiDocument->metrics();
            if (!state.guiDocumentReplaceArmed)
            {
                const bool dirty =
                    state.guiDocumentSavedRevision
                        != state.guiDocument->revision().sequence;
                state.guiDocumentStatus = epochengine::format_text(
                    "{}{} widgets, revision {}, {} semantic operations, {} history bytes",
                    dirty ? "* " : "",
                    metrics.active_widgets > 0u
                        ? metrics.active_widgets - 1u
                        : 0u,
                    metrics.revision_sequence,
                    metrics.applied_operations,
                    metrics.retained_history_bytes);
            }
            return state.guiDocument.get();
        }

        [[nodiscard]] bool project_gui_document_layout(EditorState& state)
        {
            if (!state.guiDocument)
                return false;

            bool projected = false;
            for (const auto& [sceneId, widget] : state.guiDocumentWidgets)
            {
                const auto view = state.guiDocument->widget(widget);
                const auto entityIndex = editor_entity_index(state, sceneId);
                if (!view || !entityIndex)
                    continue;

                EditorEntity changed = state.entities[*entityIndex];
                changed.position[0] = view->descriptor.layout.x / 100.0f - 3.2f;
                changed.position[1] = 1.8f - view->descriptor.layout.y / 100.0f;
                changed.scale[0] = view->descriptor.layout.width / 100.0f;
                changed.scale[1] = view->descriptor.layout.height / 100.0f;
                if (changed.position == state.entities[*entityIndex].position
                    && changed.scale == state.entities[*entityIndex].scale)
                {
                    continue;
                }
                projected = transform_editor_scene_entity(
                    state,
                    changed,
                    "Project GUI document layout") || projected;
            }
            return projected;
        }

        [[nodiscard]] std::string_view gui_entity_type_for_widget(
            authoring::gui::WidgetKind kind) noexcept
        {
            using authoring::gui::WidgetKind;
            switch (kind)
            {
            case WidgetKind::panel: return "GuiPanel";
            case WidgetKind::button: return "GuiButton";
            case WidgetKind::text: return "GuiText";
            case WidgetKind::image: return "GuiImage";
            case WidgetKind::image_button: return "GuiImageButton";
            case WidgetKind::text_input: return "GuiTextInput";
            case WidgetKind::slider: return "GuiSlider";
            case WidgetKind::scroll_area: return "GuiScrollArea";
            case WidgetKind::tab_set: return "GuiTabSet";
            case WidgetKind::canvas:
            case WidgetKind::tab_page:
            default:
                return {};
            }
        }

        [[nodiscard]] std::string gui_widget_display_text(
            const authoring::gui::WidgetView& widget)
        {
            using namespace authoring::gui;
            return std::visit([&](const auto& content) -> std::string
            {
                using Content = std::remove_cvref_t<decltype(content)>;
                if constexpr (std::same_as<Content, PanelContent>)
                    return content.title;
                else if constexpr (std::same_as<Content, ButtonContent>)
                    return content.label;
                else if constexpr (std::same_as<Content, TextContent>)
                    return content.text;
                else if constexpr (std::same_as<Content, ImageContent>)
                    return content.alternative_text;
                else if constexpr (std::same_as<Content, ImageButtonContent>)
                    return content.label;
                else if constexpr (std::same_as<Content, TextInputContent>)
                    return content.text.empty()
                        ? content.placeholder
                        : content.text;
                else if constexpr (std::same_as<Content, SliderContent>)
                    return content.label;
                else if constexpr (std::same_as<Content, TabPageContent>)
                    return content.label;
                return widget.descriptor.name;
            }, widget.descriptor.content);
        }

        [[nodiscard]] std::string_view gui_widget_asset_path(
            const authoring::gui::WidgetView& widget) noexcept
        {
            using namespace authoring::gui;
            if (const auto* image = std::get_if<ImageContent>(
                    &widget.descriptor.content))
                return image->asset_path;
            if (const auto* imageButton =
                    std::get_if<ImageButtonContent>(
                        &widget.descriptor.content))
                return imageButton->asset_path;
            return {};
        }

        [[nodiscard]] EditorEntity gui_projection_entity(
            const authoring::gui::WidgetView& widget,
            std::size_t order)
        {
            EditorEntity entity{};
            entity.name = widget.descriptor.name;
            entity.type = std::string{
                gui_entity_type_for_widget(widget.descriptor.kind)};
            entity.category = "UI";
            entity.position = {
                widget.descriptor.layout.x / 100.0f - 3.2f,
                1.8f - widget.descriptor.layout.y / 100.0f,
                0.20f + static_cast<float>(order) * 0.001f};
            entity.scale = {
                (std::max)(
                    0.20f,
                    widget.descriptor.layout.width / 100.0f),
                (std::max)(
                    0.20f,
                    widget.descriptor.layout.height / 100.0f),
                1.0f};
            entity.visible =
                widget.descriptor.interaction.visible;
            const std::string_view assetPath =
                gui_widget_asset_path(widget);
            if (!assetPath.empty()
                && !assetPath.starts_with("epoch://"))
            {
                entity.textureMaterial =
                    scene::SceneTextureMaterialSnapshot{
                        .logical_path = std::string{assetPath}};
            }
            return entity;
        }

        [[nodiscard]] bool install_gui_document(
            EditorState& state,
            std::unique_ptr<authoring::gui::GuiDocument> document,
            std::string source,
            bool source_is_saved,
            std::string status)
        {
            using namespace authoring::gui;
            if (!document
                || !document->valid()
                || document->validate()
                || document->roots().size() != 1u)
            {
                state.guiDocumentStatus =
                    "GUI document replacement rejected invalid source.";
                return false;
            }

            const WidgetHandle root = document->roots().front();
            const auto rootView = document->widget(root);
            if (!rootView
                || rootView->descriptor.kind != WidgetKind::canvas)
            {
                state.guiDocumentStatus =
                    "GUI document replacement requires one canvas root.";
                return false;
            }

            std::vector<scene::SceneObjectId> oldProjectionIds{};
            oldProjectionIds.reserve(
                state.guiDocumentWidgets.size());
            for (const auto& [sceneId, _] :
                 state.guiDocumentWidgets)
            {
                oldProjectionIds.push_back(sceneId);
            }
            for (const scene::SceneObjectId sceneId :
                 oldProjectionIds)
            {
                (void)destroy_editor_scene_entity(
                    state,
                    sceneId,
                    "Replace GUI document projection");
            }

            state.guiDocument = std::move(document);
            state.guiDocumentRoot = root;
            state.guiDocumentWidgets.clear();
            state.guiProjectionTombstones.clear();
            state.guiContentEditorEntityId =
                scene::kInvalidSceneObjectId;
            state.guiDocumentLoadAttempted = true;
            state.guiDocumentSourcePath = std::move(source);
            state.guiDocumentSavedRevision = source_is_saved
                ? state.guiDocument->revision().sequence
                : 0u;
            state.guiDocumentReplaceArmed = false;

            std::size_t order = 0u;
            for (const WidgetView& widget :
                 state.guiDocument->widgets())
            {
                if (widget.handle == root
                    || gui_entity_type_for_widget(
                        widget.descriptor.kind).empty())
                {
                    continue;
                }

                EditorEntity entity =
                    gui_projection_entity(widget, order++);
                if (!create_editor_scene_entity(
                        state,
                        std::move(entity),
                        "Project loaded GUI widget"))
                {
                    state.guiDocumentStatus =
                        "GUI source loaded, but one scene projection failed.";
                    continue;
                }
                state.guiDocumentWidgets.insert_or_assign(
                    state.selectedEntityId,
                    widget.handle);
            }

            clear_editor_selection(state);
            (void)project_gui_document_layout(state);
            state.guiDocumentStatus = std::move(status);
            push_editor_log(
                state,
                "[gui] " + state.guiDocumentStatus);
            return true;
        }

        [[nodiscard]] bool gui_document_has_unsaved_changes(
            const EditorState& state) noexcept
        {
            return state.guiDocument
                && state.guiDocument->valid()
                && state.guiDocument->revision().sequence
                    != state.guiDocumentSavedRevision;
        }

        [[nodiscard]] bool apply_gui_template(
            EditorState& state,
            GuiTemplatePreset preset)
        {
            if (gui_document_has_unsaved_changes(state)
                && !state.guiDocumentReplaceArmed)
            {
                state.guiDocumentReplaceArmed = true;
                state.guiDocumentStatus =
                    "Unsaved GUI changes exist. Press Apply Template again to replace them.";
                return false;
            }

            auto templateDocument =
                authoring::gui::make_template_document(preset);
            auto document = templateDocument
                ? std::make_unique<authoring::gui::GuiDocument>(
                    std::move(*templateDocument))
                : std::unique_ptr<authoring::gui::GuiDocument>{};
            const auto templateIt = std::find_if(
                kGuiTemplateCatalog.begin(),
                kGuiTemplateCatalog.end(),
                [preset](const GuiTemplateDefinition& candidate)
                {
                    return candidate.preset == preset;
                });
            const std::string label =
                templateIt == kGuiTemplateCatalog.end()
                    ? std::string{"GUI"}
                    : std::string{templateIt->label};
            return install_gui_document(
                state,
                std::move(document),
                "Unsaved " + label + " template",
                false,
                label
                    + " template created. Save GUI publishes it to the active project.");
        }

        [[nodiscard]] bool write_verified_gui_document(
            const authoring::gui::GuiDocument& document,
            std::filesystem::path destination,
            std::string& failure)
        {
            using namespace authoring::gui;
            failure.clear();
            if (destination.extension() != ".epochgui")
                destination += ".epochgui";
            if (!core::path::candidate_data_root().empty())
            {
                destination = editor_resolve_candidate_project_path(destination);
                if (destination.empty())
                {
                    failure = "Candidate GUI documents must save inside this preview's private Projects folder.";
                    return false;
                }
            }

            const SerializedGuiDocument encoded =
                serialize_gui_document(
                    document.snapshot(),
                    kMaximumProjectGuiDocumentBytes);
            if (!encoded)
            {
                failure =
                    "GUI document serialization failed: "
                    + std::string(
                        snapshot_codec_code_name(encoded.code))
                    + ".";
                return false;
            }

            try
            {
                std::error_code error{};
                const auto destinationStatus =
                    std::filesystem::symlink_status(
                        destination,
                        error);
                if (!error
                    && std::filesystem::exists(destinationStatus)
                    && (std::filesystem::is_symlink(
                            destinationStatus)
                        || !std::filesystem::is_regular_file(
                            destinationStatus)))
                {
                    failure =
                        "GUI destination must be a regular non-symlink file.";
                    return false;
                }

                error.clear();
                std::filesystem::create_directories(
                    destination.parent_path(),
                    error);
                if (error)
                {
                    failure =
                        "GUI destination directory could not be created.";
                    return false;
                }

                const std::filesystem::path temporary =
                    destination.parent_path()
                    / (destination.filename().string()
                        + ".write");
                if (std::filesystem::exists(temporary, error))
                {
                    error.clear();
                    if (!std::filesystem::remove(
                            temporary,
                            error)
                        || error)
                    {
                        failure =
                            "Stale GUI temporary source could not be retired.";
                        return false;
                    }
                }

                error.clear();
                if (!platform::filesystem::
                        exclusive_create_and_write(
                            temporary,
                            encoded.bytes,
                            error))
                {
                    failure =
                        "GUI temporary source could not be created and flushed.";
                    return false;
                }

                std::string verificationFailure{};
                const auto verifiedBytes =
                    read_project_gui_document_bytes(
                        temporary,
                        verificationFailure);
                const auto verified = verifiedBytes
                    ? deserialize_gui_document(*verifiedBytes)
                    : DeserializedGuiDocument{};
                if (!verified
                    || *verified.snapshot
                        != document.snapshot())
                {
                    std::error_code ignored{};
                    (void)std::filesystem::remove(
                        temporary,
                        ignored);
                    failure = verificationFailure.empty()
                        ? "GUI temporary source failed exact verification."
                        : std::move(verificationFailure);
                    return false;
                }

                if (!platform::filesystem::
                        atomic_replace_same_filesystem(
                            temporary,
                            destination,
                            error))
                {
                    std::error_code ignored{};
                    (void)std::filesystem::remove(
                        temporary,
                        ignored);
                    failure =
                        "GUI source could not be atomically published.";
                    return false;
                }
                return true;
            }
            catch (...)
            {
                failure =
                    "GUI source publication failed during filesystem work.";
                return false;
            }
        }

        [[nodiscard]] std::unique_ptr<
            authoring::gui::GuiDocument>
        decode_gui_document_file(
            const std::filesystem::path& source,
            std::string& failure)
        {
            using namespace authoring::gui;
            const auto bytes =
                read_project_gui_document_bytes(
                    source,
                    failure);
            if (!bytes)
                return {};

            const DeserializedGuiDocument decoded =
                deserialize_gui_document(*bytes);
            if (!decoded)
            {
                failure =
                    "GUI source rejected: "
                    + std::string(
                        snapshot_codec_code_name(decoded.code))
                    + ".";
                return {};
            }

            auto document =
                std::make_unique<GuiDocument>(
                    *decoded.snapshot);
            if (!document->valid()
                || document->validate()
                || document->roots().size() != 1u)
            {
                failure =
                    "GUI source reconstructed an invalid document.";
                return {};
            }
            return document;
        }

        [[nodiscard]] bool open_gui_document_from_dialog(
            EditorState& state)
        {
            if (gui_document_has_unsaved_changes(state)
                && !state.guiDocumentReplaceArmed)
            {
                state.guiDocumentReplaceArmed = true;
                state.guiDocumentStatus =
                    "Unsaved GUI changes exist. Press Open GUI again to replace them.";
                return false;
            }

            std::filesystem::path initialDirectory{};
            if (!state.projectRoot.empty())
            {
                initialDirectory =
                    resolve_editor_path(state.projectRoot)
                    / "Assets" / "Gui";
            }
            if (initialDirectory.empty())
            {
                std::error_code ignored{};
                initialDirectory =
                    std::filesystem::current_path(ignored);
            }

            const auto selected =
                platform::filesystem::open_file_dialog(
                    L"Open Epoch GUI Document",
                    L"Epoch GUI document",
                    L"*.epochgui",
                    initialDirectory);
            if (!selected.accepted)
            {
                state.guiDocumentReplaceArmed = false;
                if (!selected.cancelled)
                    state.guiDocumentStatus =
                        selected.diagnostic;
                return false;
            }

            std::string failure{};
            auto document =
                decode_gui_document_file(
                    selected.path,
                    failure);
            if (!document)
            {
                state.guiDocumentReplaceArmed = false;
                state.guiDocumentStatus =
                    std::move(failure);
                return false;
            }
            return install_gui_document(
                state,
                std::move(document),
                display_project_path(selected.path),
                false,
                "Opened GUI document as an unsaved project draft from "
                    + display_project_path(selected.path)
                    + ".");
        }

        [[nodiscard]] bool save_gui_template_from_dialog(
            EditorState& state)
        {
            auto* document =
                synchronize_gui_document(state);
            if (!document)
                return false;

            const std::size_t presetIndex = (std::min)(
                state.guiTemplatePreset,
                kGuiTemplateCatalog.size() - 1u);
            std::filesystem::path initialDirectory{};
            if (!state.projectRoot.empty())
            {
                auto root = resolve_editor_path(state.projectRoot);
                if (!core::path::candidate_data_root().empty())
                    root = editor_resolve_candidate_project_path(root);
                if (root.empty())
                {
                    state.guiDocumentStatus = "No writable project is admitted for this GUI template.";
                    return false;
                }
                initialDirectory = root / "Assets" / "Gui" / "Templates";
                std::error_code ignored{};
                std::filesystem::create_directories(
                    initialDirectory,
                    ignored);
            }
            if (initialDirectory.empty())
            {
                if (!core::path::candidate_data_root().empty())
                {
                    state.guiDocumentStatus = "Select a private candidate project before saving a GUI template.";
                    return false;
                }
                std::error_code ignored{};
                initialDirectory =
                    std::filesystem::current_path(ignored);
            }

            const std::wstring defaultName =
                std::filesystem::path{
                    std::string{
                        kGuiTemplateCatalog[presetIndex].id}
                    + ".epochgui"}.wstring();
            const auto destination =
                platform::filesystem::save_file_dialog(
                    L"Save Epoch GUI Template",
                    L"Epoch GUI document",
                    L"*.epochgui",
                    defaultName,
                    L"epochgui",
                    initialDirectory);
            if (!destination.accepted)
            {
                if (!destination.cancelled)
                {
                    state.guiDocumentStatus =
                        destination.diagnostic;
                }
                return false;
            }

            std::string failure{};
            if (!write_verified_gui_document(
                    *document,
                    destination.path,
                    failure))
            {
                state.guiDocumentStatus =
                    std::move(failure);
                return false;
            }
            state.guiDocumentStatus =
                "Saved reusable GUI template to "
                + display_project_path(destination.path)
                + ".";
            push_editor_log(
                state,
                "[gui] " + state.guiDocumentStatus);
            return true;
        }

        [[nodiscard]] bool gui_widget_selected(
            const EditorState& state,
            authoring::gui::WidgetHandle widget) noexcept
        {
            const auto mapped = std::ranges::find(
                state.guiDocumentWidgets,
                widget,
                &decltype(state.guiDocumentWidgets)::value_type::second);
            return mapped != state.guiDocumentWidgets.end()
                && mapped->first == state.selectedEntityId;
        }

        void select_gui_widget(
            EditorState& state,
            authoring::gui::WidgetHandle widget)
        {
            const auto mapped = std::ranges::find(
                state.guiDocumentWidgets,
                widget,
                &decltype(state.guiDocumentWidgets)::value_type::second);
            if (mapped == state.guiDocumentWidgets.end())
                return;
            const auto entityIndex =
                editor_entity_index(state, mapped->first);
            if (entityIndex)
            {
                select_editor_entity(state, *entityIndex);
                state.outlinerToolTab = OutlinerToolTab::Gui;
                state.game2DGuiWorkspace = true;
                state.guiAuthoringSection = 1u;
                state.inspectorToolTab = InspectorToolTab::Properties;
            }
        }

        [[nodiscard]] std::optional<authoring::gui::WidgetHandle>
        selected_gui_widget(const EditorState& state) noexcept
        {
            const auto mapped = state.guiDocumentWidgets.find(
                state.selectedEntityId);
            if (mapped == state.guiDocumentWidgets.end())
                return std::nullopt;
            return mapped->second;
        }

        void remember_gui_projection_tombstone(
            EditorState& state,
            authoring::gui::WidgetHandle widget,
            const EditorEntity& entity)
        {
            const auto existing = std::find_if(
                state.guiProjectionTombstones.begin(),
                state.guiProjectionTombstones.end(),
                [widget](const GuiProjectionTombstone& tombstone)
                {
                    return tombstone.widget == widget;
                });
            if (existing != state.guiProjectionTombstones.end())
            {
                existing->entity = entity;
                return;
            }
            state.guiProjectionTombstones.push_back(
                GuiProjectionTombstone{widget, entity});
        }

        void reconcile_gui_document_projections(
            EditorState& state,
            authoring::gui::GuiDocument& document)
        {
            struct RemovedProjection final
            {
                scene::SceneObjectId scene_id{};
                authoring::gui::WidgetHandle widget{};
                EditorEntity entity{};
            };
            std::vector<RemovedProjection> removed{};
            for (const auto& [sceneId, widget] : state.guiDocumentWidgets)
            {
                if (document.widget(widget))
                    continue;
                const auto entityIndex = editor_entity_index(state, sceneId);
                if (!entityIndex)
                    continue;
                removed.push_back(RemovedProjection{
                    sceneId,
                    widget,
                    state.entities[*entityIndex]});
            }
            for (const auto& projection : removed)
            {
                remember_gui_projection_tombstone(
                    state, projection.widget, projection.entity);
                state.guiDocumentWidgets.erase(projection.scene_id);
                (void)destroy_editor_scene_entity(
                    state,
                    projection.scene_id,
                    "Remove GUI widget projection");
            }

            std::vector<std::size_t> restored{};
            const scene::SceneObjectId previousSelection =
                state.selectedEntityId;
            for (std::size_t index = 0u;
                 index < state.guiProjectionTombstones.size();
                 ++index)
            {
                const auto& tombstone =
                    state.guiProjectionTombstones[index];
                if (!document.widget(tombstone.widget))
                    continue;
                const bool alreadyProjected = std::ranges::find(
                    state.guiDocumentWidgets,
                    tombstone.widget,
                    &decltype(state.guiDocumentWidgets)::value_type::second)
                        != state.guiDocumentWidgets.end();
                if (alreadyProjected)
                {
                    restored.push_back(index);
                    continue;
                }

                EditorEntity entity = tombstone.entity;
                entity.sceneObjectId = scene::kInvalidSceneObjectId;
                if (!create_editor_scene_entity(
                        state,
                        std::move(entity),
                        "Restore GUI widget projection"))
                {
                    continue;
                }
                state.guiDocumentWidgets.insert_or_assign(
                    state.selectedEntityId,
                    tombstone.widget);
                restored.push_back(index);
            }
            for (auto it = restored.rbegin(); it != restored.rend(); ++it)
            {
                state.guiProjectionTombstones.erase(
                    state.guiProjectionTombstones.begin()
                        + static_cast<std::ptrdiff_t>(*it));
            }

            if (previousSelection != scene::kInvalidSceneObjectId
                && editor_entity_index(state, previousSelection))
            {
                state.selectedEntityId = previousSelection;
                synchronize_editor_selection(state);
            }
            (void)project_gui_document_layout(state);
        }

        [[nodiscard]] bool remove_selected_gui_widget(
            EditorState& state,
            authoring::gui::GuiDocument& document)
        {
            const auto selected = selected_gui_widget(state);
            if (!selected)
            {
                state.guiDocumentStatus =
                    "Select a GUI widget before deleting it.";
                return false;
            }

            const auto removed = document.remove_widget(*selected);
            if (!removed)
            {
                state.guiDocumentStatus = epochengine::format_text(
                    "GUI delete rejected: {}.",
                    authoring::gui::result_code_name(removed.code));
                return false;
            }

            reconcile_gui_document_projections(state, document);
            state.guiContentEditorEntityId =
                scene::kInvalidSceneObjectId;
            state.guiDocumentStatus =
                "GUI widget subtree removed. Undo restores the document and canvas projection.";
            push_editor_log(state, "[gui] " + state.guiDocumentStatus);
            return true;
        }
        [[nodiscard]] bool duplicate_selected_gui_widget(
            EditorState& state,
            authoring::gui::GuiDocument& document)
        {
            const auto selected = selected_gui_widget(state);
            const auto source = selected ? document.widget(*selected) : std::nullopt;
            if (!source)
            {
                state.guiDocumentStatus =
                    "Select a GUI widget before duplicating it.";
                return false;
            }

            auto duplicate = source->descriptor;
            const std::string nameStem = duplicate.name + "_Copy";
            duplicate.name = nameStem;
            const auto widgets = document.widgets();
            for (std::uint32_t suffix = 2u;
                std::ranges::any_of(widgets, [&](const auto& widget)
                    { return widget.descriptor.name == duplicate.name; });
                ++suffix)
            {
                duplicate.name = nameStem + std::to_string(suffix);
            }
            duplicate.layout.x += 16.0f;
            duplicate.layout.y += 16.0f;
            const auto created = document.create_widget(
                std::move(duplicate), source->parent);
            if (!created)
            {
                state.guiDocumentStatus = epochengine::format_text(
                    "GUI duplicate rejected: {}.",
                    authoring::gui::result_code_name(created.code));
                return false;
            }

            reconcile_gui_document_projections(state, document);
            const auto projection = std::ranges::find_if(
                state.guiDocumentWidgets,
                [&](const auto& entry) { return entry.second == created.widget; });
            if (projection != state.guiDocumentWidgets.end())
            {
                if (const auto index = editor_entity_index(state, projection->first))
                    select_editor_entity(state, *index);
            }
            state.guiDocumentStatus =
                "GUI widget duplicated in the active GUI document.";
            push_editor_log(state, "[gui] " + state.guiDocumentStatus);
            return true;
        }


        [[nodiscard]] bool undo_gui_document(EditorState& state)
        {
            auto* document = synchronize_gui_document(state);
            if (!document)
                return false;
            const auto result = document->undo();
            if (!result)
            {
                state.guiDocumentStatus = "GUI undo unavailable: "
                    + std::string(authoring::gui::result_code_name(result.code));
                return false;
            }
            state.guiContentEditorEntityId = scene::kInvalidSceneObjectId;
            reconcile_gui_document_projections(state, *document);
            state.guiDocumentStatus = "GUI document undo committed to the shared canvas projection.";
            push_editor_log(state, "[edit] " + state.guiDocumentStatus);
            return true;
        }

        [[nodiscard]] bool redo_gui_document(EditorState& state)
        {
            auto* document = synchronize_gui_document(state);
            if (!document)
                return false;
            const auto result = document->redo();
            if (!result)
            {
                state.guiDocumentStatus = "GUI redo unavailable: "
                    + std::string(authoring::gui::result_code_name(result.code));
                return false;
            }
            state.guiContentEditorEntityId = scene::kInvalidSceneObjectId;
            reconcile_gui_document_projections(state, *document);
            state.guiDocumentStatus = "GUI document redo committed to the shared canvas projection.";
            push_editor_log(state, "[edit] " + state.guiDocumentStatus);
            return true;
        }

        [[nodiscard]] bool reload_project_gui_document(EditorState& state)
        {
            if (state.guiDocument
                && state.guiDocument->revision().sequence
                    != state.guiDocumentSavedRevision)
            {
                state.guiDocumentStatus =
                    "Reload blocked because the GUI document has unsaved changes.";
                push_editor_log(state, "[gui] " + state.guiDocumentStatus);
                return false;
            }
            state.guiDocument.reset();
            state.guiDocumentRoot = {};
            state.guiDocumentWidgets.clear();
            state.guiCanvasWidgetDrag = {};
            state.guiCanvasWidgetDragHandle = {};
            state.guiProjectionTombstones.clear();
            state.guiDocumentLoadAttempted = false;
            state.guiContentEditorEntityId = scene::kInvalidSceneObjectId;
            if (!load_project_gui_document(state))
                return synchronize_gui_document(state) != nullptr;
            return project_gui_document_layout(state);
        }

        enum class ActiveEditDocument : std::uint8_t
        {
            none,
            scene,
            gui,
            tilemap,
            plant_lab
        };

        [[nodiscard]] ActiveEditDocument active_edit_document(
            const EditorState& state) noexcept
        {
            if (state.mainSurface == EditorMainSurface::Scene)
                return ActiveEditDocument::scene;
            if (state.mainSurface == EditorMainSurface::Game2D
                && state.game2DGuiWorkspace)
                return ActiveEditDocument::gui;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (state.mainSurface == EditorMainSurface::Game2D
                && !state.game2DGuiWorkspace)
                return ActiveEditDocument::tilemap;
#endif
            if (state.mainSurface == EditorMainSurface::PlantLab
                || state.mainSurface == EditorMainSurface::ForestFactory)
                return ActiveEditDocument::plant_lab;
            return ActiveEditDocument::none;
        }

        [[nodiscard]] bool undo_active_editor_document(EditorState& state)
        {
            switch (active_edit_document(state))
            {
            case ActiveEditDocument::gui:
                return undo_gui_document(state);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            case ActiveEditDocument::tilemap:
                if (auto* tileMap = tilemap_workspace_controller(state))
                {
                    const bool changed = static_cast<bool>(tileMap->undo());
                    state.tileMapStatus = std::string(tileMap->status());
                    push_editor_log(state, "[edit] " + state.tileMapStatus);
                    return changed;
                }
                return false;
#endif
            case ActiveEditDocument::scene:
                if (undo_editor_scene(state))
                {
                    push_editor_log(state, "[edit] World undo committed.");
                    return true;
                }
                push_editor_log(state, "[edit] Nothing to undo in the active world document.");
                return false;
            case ActiveEditDocument::plant_lab:
                if (forest::undo_forest_profile_edit(state.plantLabDocument))
                {
                    state.plantLabCompiledRevision = 0u;
                    state.plantLabCompiledContentHash = 0u;
                    push_editor_log(state, "[edit] Plant Lab undo committed.");
                    return true;
                }
                push_editor_log(state, "[edit] Nothing to undo in Plant Lab.");
                return false;
            case ActiveEditDocument::none:
            default:
                push_editor_log(state,
                    "[edit] Undo is unavailable on this surface; World unchanged.");
                return false;
            }
        }

        [[nodiscard]] bool redo_active_editor_document(EditorState& state)
        {
            switch (active_edit_document(state))
            {
            case ActiveEditDocument::gui:
                return redo_gui_document(state);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            case ActiveEditDocument::tilemap:
                if (auto* tileMap = tilemap_workspace_controller(state))
                {
                    const bool changed = static_cast<bool>(tileMap->redo());
                    state.tileMapStatus = std::string(tileMap->status());
                    push_editor_log(state, "[edit] " + state.tileMapStatus);
                    return changed;
                }
                return false;
#endif
            case ActiveEditDocument::scene:
                if (redo_editor_scene(state))
                {
                    push_editor_log(state, "[edit] World redo committed.");
                    return true;
                }
                push_editor_log(state, "[edit] Nothing to redo in the active world document.");
                return false;
            case ActiveEditDocument::plant_lab:
                if (forest::redo_forest_profile_edit(state.plantLabDocument))
                {
                    state.plantLabCompiledRevision = 0u;
                    state.plantLabCompiledContentHash = 0u;
                    push_editor_log(state, "[edit] Plant Lab redo committed.");
                    return true;
                }
                push_editor_log(state, "[edit] Nothing to redo in Plant Lab.");
                return false;
            case ActiveEditDocument::none:
            default:
                push_editor_log(state,
                    "[edit] Redo is unavailable on this surface; World unchanged.");
                return false;
            }
        }
        void hydrate_gui_content_editor(
            EditorState& state,
            scene::SceneObjectId entity,
            const authoring::gui::WidgetView& widget)
        {
            if (state.guiContentEditorEntityId == entity)
                return;

            using namespace authoring::gui;
            state.guiContentEditorEntityId = entity;
            state.guiContentPrimary.clear();
            state.guiContentSecondary.clear();
            state.guiActionEditor =
                widget.descriptor.interaction.action;
            state.guiTooltipEditor =
                widget.descriptor.interaction.tooltip;

            std::visit([&](const auto& content)
            {
                using Content = std::remove_cvref_t<decltype(content)>;
                if constexpr (std::same_as<Content, PanelContent>)
                {
                    state.guiContentPrimary = content.title;
                }
                else if constexpr (std::same_as<Content, ButtonContent>)
                {
                    state.guiContentPrimary = content.label;
                }
                else if constexpr (std::same_as<Content, TextContent>)
                {
                    state.guiContentPrimary = content.text;
                }
                else if constexpr (std::same_as<Content, ImageContent>)
                {
                    state.guiContentPrimary = content.asset_path;
                    state.guiContentSecondary = content.alternative_text;
                }
                else if constexpr (
                    std::same_as<Content, ImageButtonContent>)
                {
                    state.guiContentPrimary = content.asset_path;
                    state.guiContentSecondary = content.label;
                }
                else if constexpr (
                    std::same_as<Content, TextInputContent>)
                {
                    state.guiContentPrimary = content.text;
                    state.guiContentSecondary = content.placeholder;
                }
                else if constexpr (std::same_as<Content, SliderContent>)
                {
                    state.guiContentPrimary = content.label;
                }
                else if constexpr (std::same_as<Content, TabPageContent>)
                {
                    state.guiContentPrimary = content.label;
                }
            }, widget.descriptor.content);
        }

        [[nodiscard]] bool apply_gui_content_editor(
            EditorState& state,
            authoring::gui::GuiDocument& document,
            const authoring::gui::WidgetView& widget)
        {
            using namespace authoring::gui;

            WidgetContent content = widget.descriptor.content;
            bool contentEditable = true;
            std::visit([&](auto& value)
            {
                using Content = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<Content, PanelContent>)
                {
                    value.title = state.guiContentPrimary;
                }
                else if constexpr (std::same_as<Content, ButtonContent>)
                {
                    value.label = state.guiContentPrimary;
                }
                else if constexpr (std::same_as<Content, TextContent>)
                {
                    value.text = state.guiContentPrimary;
                }
                else if constexpr (std::same_as<Content, ImageContent>)
                {
                    value.asset_path = state.guiContentPrimary;
                    value.alternative_text = state.guiContentSecondary;
                }
                else if constexpr (
                    std::same_as<Content, ImageButtonContent>)
                {
                    value.asset_path = state.guiContentPrimary;
                    value.label = state.guiContentSecondary;
                }
                else if constexpr (
                    std::same_as<Content, TextInputContent>)
                {
                    value.text = state.guiContentPrimary;
                    value.placeholder = state.guiContentSecondary;
                }
                else if constexpr (std::same_as<Content, SliderContent>)
                {
                    value.label = state.guiContentPrimary;
                }
                else if constexpr (std::same_as<Content, TabPageContent>)
                {
                    value.label = state.guiContentPrimary;
                }
                else
                {
                    contentEditable = false;
                }
            }, content);

            bool changed = false;
            if (contentEditable && content != widget.descriptor.content)
            {
                changed = static_cast<bool>(
                    document.update_content(
                        widget.handle,
                        std::move(content)));
            }

            InteractionDescriptor interaction =
                widget.descriptor.interaction;
            interaction.action = state.guiActionEditor;
            interaction.tooltip = state.guiTooltipEditor;
            if (interaction != widget.descriptor.interaction)
            {
                changed = static_cast<bool>(
                    document.update_interaction(
                        widget.handle,
                        std::move(interaction))) || changed;
            }
            return changed;
        }

        [[nodiscard]] bool save_project_gui_document(EditorState& state)
        {
            using namespace authoring::gui;

            GuiDocument* document =
                synchronize_gui_document(state);
            if (!document)
                return false;

            const std::filesystem::path destination =
                project_gui_document_path(state);
            state.guiDocumentSourcePath = destination.empty()
                ? std::string{}
                : display_project_path(destination);
            if (destination.empty())
            {
                state.guiDocumentStatus =
                    "GUI save blocked because the project root is unavailable.";
                return false;
            }

            const GuiDocumentSnapshot snapshot = document->snapshot();
            const CompileResult compiled = compile_gui_document(snapshot);
            if (!compiled)
            {
                state.guiDocumentStatus = epochengine::format_text(
                    "GUI artifact compilation failed: {} ({} widgets emitted).",
                    compile_code_name(compiled.code),
                    compiled.diagnostics.emitted_widgets);
                return false;
            }
            const SerializedGuiDocument encoded =
                serialize_gui_document(
                    snapshot,
                    kMaximumProjectGuiDocumentBytes);
            if (!encoded)
            {
                state.guiDocumentStatus = epochengine::format_text(
                    "GUI save serialization failed: {}.",
                    snapshot_codec_code_name(encoded.code));
                return false;
            }

            try
            {
                std::error_code error{};
                std::filesystem::create_directories(
                    destination.parent_path(),
                    error);
                if (error)
                {
                    state.guiDocumentStatus =
                        "GUI save could not create Assets/Gui.";
                    return false;
                }

                const std::filesystem::path temporary =
                    destination.parent_path()
                    / (destination.filename().string() + ".write");
                if (std::filesystem::exists(temporary, error))
                {
                    error.clear();
                    if (!std::filesystem::remove(temporary, error)
                        || error)
                    {
                        state.guiDocumentStatus =
                            "GUI save could not retire its stale temporary source.";
                        return false;
                    }
                }
                error.clear();
                if (!platform::filesystem::exclusive_create_and_write(
                        temporary,
                        encoded.bytes,
                        error))
                {
                    state.guiDocumentStatus =
                        "GUI save could not create and flush the temporary source.";
                    return false;
                }

                std::string failure{};
                auto verifiedBytes =
                    read_project_gui_document_bytes(temporary, failure);
                const auto verified = verifiedBytes
                    ? deserialize_gui_document(*verifiedBytes)
                    : DeserializedGuiDocument{};
                if (!verified
                    || *verified.snapshot != document->snapshot())
                {
                    std::error_code cleanupError{};
                    (void)std::filesystem::remove(
                        temporary,
                        cleanupError);
                    state.guiDocumentStatus = failure.empty()
                        ? "GUI save verification rejected the temporary source."
                        : std::move(failure);
                    return false;
                }

                if (!platform::filesystem::atomic_replace_same_filesystem(
                        temporary,
                        destination,
                        error))
                {
                    std::error_code cleanupError{};
                    (void)std::filesystem::remove(
                        temporary,
                        cleanupError);
                    state.guiDocumentStatus =
                        "GUI save could not atomically publish the source.";
                    return false;
                }

                project_gui::ArtifactLibrary library{
                    state.projectId,
                    resolve_editor_path(
                        std::filesystem::path{state.projectRoot})
                        .generic_string()};
                const project_gui::ArtifactLocator published =
                    library.persist(
                        project_gui::canonical_source_path,
                        compiled.artifact);
                if (!published)
                {
                    state.guiDocumentStatus = epochengine::format_text(
                        "GUI source saved, but runtime artifact publication "
                        "failed: {}.",
                        project_gui::library_code_name(published.code));
                    push_editor_log(
                        state,
                        "[gui] " + state.guiDocumentStatus);
                    return false;
                }
                state.guiDocumentSavedRevision =
                    document->revision().sequence;
                state.guiDocumentStatus = epochengine::format_text(
                    "Saved GUI source revision {} ({} bytes) and {} "
                    "runtime widgets to Library/Gui.",
                    state.guiDocumentSavedRevision,
                    encoded.bytes.size(),
                    compiled.diagnostics.emitted_widgets);
                push_editor_log(
                    state,
                    "[gui] " + state.guiDocumentStatus);
                return true;
            }
            catch (...)
            {
                state.guiDocumentStatus =
                    "GUI save failed during filesystem publication.";
                return false;
            }
        }

