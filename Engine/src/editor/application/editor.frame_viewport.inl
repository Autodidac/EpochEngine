// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell

        const auto projectBuildActivity = project_build_activity_label(editor);
        bool guiDocumentWorkspace =
            editor.mainSurface == EditorMainSurface::Game2D
            && (editor.game2DGuiWorkspace
                || editor_application_owns_dedicated_gui_workspace(
                    editor.applicationKind));
        bool active_center_uses_scene =
            main_surface_uses_scene(editor.mainSurface)
            && !guiDocumentWorkspace;
        if (editor.surfaceSettleFrames > 0)
            --editor.surfaceSettleFrames;
        // Document tabs provide the surface identity. A second generic
        // "Editor Workbench" header only consumes valuable vertical space.
        gui::begin_window(
            active_center_uses_scene || editor.mainSurface == EditorMainSurface::AssetGeneration
                ? std::string_view{} : std::string_view{"Editor Workbench"},
            viewport_pos,
            viewport_size,
            !active_center_uses_scene && editor.mainSurface != EditorMainSurface::AssetGeneration,
            false);
        if (editor.mainSurface == EditorMainSurface::Game2D
            && !editor_application_owns_dedicated_gui_workspace(
                editor.applicationKind))
        {
            // The transparent scene window can retain the previous 3D title
            // behind this shorter tab strip. Line advance bounds glyph height;
            // include the control's inner padding and following layout gap.
            if (!guiDocumentWorkspace)
                gui::panel_rect({viewport_pos.x, gui::cursor_position().y},
                    {viewport_size.x, (std::max)(31.0f, gui::line_height() + 10.0f) + 8.0f});
            const std::array modeTabs{
                gui::TabButtonSpec{
                    .id = "game2d.workspace.tilemap",
                    .label = "Tile Map",
                    .width = 0.0f,
                    .active = !guiDocumentWorkspace},
                gui::TabButtonSpec{
                    .id = "game2d.workspace.gui",
                    .label = "GUI Overlay",
                    .width = 0.0f,
                    .active = guiDocumentWorkspace}
            };
            if (const auto selected = gui::tab_bar_buttons(
                    modeTabs, 31.0f, 2.0f);
                selected.selected_index)
            {
                guiDocumentWorkspace = *selected.selected_index == 1u;
                active_center_uses_scene = !guiDocumentWorkspace;
                guiDocumentWorkspace = select_editor_2d_workspace(editor, guiDocumentWorkspace);
                activate_workspace_companions(EditorMainSurface::Game2D);
                push_editor_log(
                    editor,
                    guiDocumentWorkspace
                        ? "[editor] 2D / UI switched to GUI Overlay placement."
                        : "[editor] 2D / UI switched to Tile Map authoring.");
            }
            const gui::Vec2 instructionPos = gui::cursor_position();
            const std::string_view instruction = guiDocumentWorkspace
                    ? "Place and adjust project GUI elements here. Full widget "
                      "construction remains in the dedicated Epoch GUI Editor."
                    : "Paint tiles, manage layers, and attach project textures. "
                      "Undo and Redo stay on this tile-map document.";
            const float instructionWidth = (std::max)(1.0f,
                viewport_pos.x + viewport_size.x - instructionPos.x - 6.0f);
            if (!guiDocumentWorkspace)
                gui::panel_rect({viewport_pos.x, instructionPos.y},
                    {viewport_size.x,
                        gui::measure_wrapped_label_height(instruction, instructionWidth) + 8.0f});
            gui::wrapped_label(instruction, instructionWidth);
            // Reserve the complete measured text block plus a gap before
            // drawing the scene title; a clipped draw is not layout height.
            gui::set_cursor({instructionPos.x, instructionPos.y
                + gui::measure_wrapped_label_height(instruction, instructionWidth) + 8.0f});
        }
        if (active_center_uses_scene)
        {
            const auto forestPanelBounds = [](gui::Vec2 position, gui::Vec2 size)
            {
                size = {(std::max)(0.0f, size.x), (std::max)(0.0f, size.y)};
                std::array<gui::WidgetBounds, 2> panels{};
                if (size.x >= 600.0f)
                {
                    const float controlsWidth = (std::clamp)(size.x * .38f, 260.0f, 360.0f);
                    panels[0] = {position, {controlsWidth, size.y}};
                    panels[1] = {{position.x + controlsWidth + 8.0f, position.y},
                        {size.x - controlsWidth - 8.0f, size.y}};
                }
                else
                {
                    const float gap = (std::min)(8.0f, size.y);
                    const float controlsHeight = (std::min)(300.0f, (size.y - gap) * .4f);
                    panels[0] = {position, {size.x, controlsHeight}};
                    panels[1] = {{position.x, position.y + controlsHeight + gap},
                        {size.x, size.y - controlsHeight - gap}};
                }
                return panels;
            };
            if (editor.mainSurface == EditorMainSurface::ForestFactory)
            {
                const auto panels = forestPanelBounds(gui::cursor_position(), viewport_size);
                gui::panel_rect(panels[0].position, panels[0].size);
                (void)gui::begin_scroll_area({.id="forest-authoring-controls",
                    .size=panels[0].size, .draw_background=false});
                {
                const auto forestControlPosition = panels[0].position;
                const auto forestControlSize = panels[0].size;
            if(editor.mainSurface==EditorMainSurface::ForestFactory)
            {
                const float width=(std::max)(48.0f,forestControlSize.x-16.0f);
                const auto backing=[&](float height){gui::panel_rect({forestControlPosition.x,gui::cursor_position().y},{forestControlSize.x,height});};
                backing(gui::measure_section_heading_height("Tree Materials",width));
                gui::section_heading("Tree Materials");
                std::vector<std::string> names{"Built-in texture"};
                std::vector<forest::ForestTextureReference> materials(1u);
                if(editor.projectTextures)
                    for(const auto& texture:editor.projectTextures->catalog())
                    {
                        forest::ForestTextureReference reference{texture.logical_path,texture.artifact_key.words};
                        if(!texture||!reference.valid())continue;
                        names.push_back(texture.logical_path);materials.push_back(std::move(reference));
                    }
                std::vector<std::string_view> labels;labels.reserve(names.size());
                for(const auto& name:names)labels.push_back(name);
                for(const auto slot:{forest::ForestTextureSlot::bark,forest::ForestTextureSlot::foliage})
                {
                    const bool bark=slot==forest::ForestTextureSlot::bark;
                    const auto current=bark?editor.plantLabDocument.profile.barkTexture:editor.plantLabDocument.profile.foliageTexture;
                    backing(gui::line_height()+38.0f);
                    gui::label(bark?"Bark texture":"Leaf texture (transparent PNG)");
                    const auto selected=gui::select_box({.id=bark?"forest-bark-texture":"forest-leaf-texture",
                        .placeholder="Choose an imported project texture",
                        .selected=current.empty()?std::string_view{"Built-in texture"}:std::string_view{current.logicalPath},
                        .options=labels,.size={width,28.0f}});
                    if(selected.changed&&selected.selected_index&&*selected.selected_index<materials.size())
                    {
                        if(*selected.selected_index==0u)(void)clear_plant_lab_texture(editor,slot);
                        else if(editor.projectTextures)
                        {
                            const auto& expected=materials[*selected.selected_index];
                            if(editor.projectTextures->select(expected.logicalPath))
                                if(const auto material=editor.projectTextures->selected_material();material&&material->artifact_key==expected.artifactKey)
                                    (void)assign_plant_lab_texture(editor,slot,*material);
                        }
                    }
                }
                const gui::InlineButtonRowOptions options{.sizing=gui::InlineButtonSizing::FitContent,
                    .alignment=gui::InlineButtonAlignment::Center,.wrap=true,.available_width=width};
                const std::array actions{
                    gui::InlineButtonSpec{.label="Import Texture...",.enabled=has_open_project(editor)&&!editor.forestSurfacePending},
                    gui::InlineButtonSpec{.label="Generate Texture..."}};
                backing(gui::inline_button_row_height(actions,28.0f,6.0f,options)+8.0f);
                if(const auto selected=gui::inline_button_row(actions,28.0f,6.0f,options))
                {
                    if(*selected==0u)browse_project_image_source(editor,ctx);
                    else editor.requestedMainSurface=EditorMainSurface::AssetGeneration;
                }
            }
            if (editor.mainSurface == EditorMainSurface::ForestFactory && !editor.forestTerrainPainting)
            {
                bool ready = ensure_forest_factory_preview(editor);
                // Preview admission may fail after an edit. Keep the document's
                // recovery controls available without admitting stale geometry.
                const bool editable = editor.plantLabDocumentInitialized;
                const float forestTextWidth = (std::max)(1.0f, (std::min)(forestControlSize.x,
                    (std::max)(80.0f, forestControlSize.x - 16.0f)));
                // Scene-backed windows leave a hole for the renderer. Their
                // controls still need opaque backing: the previous workspace's
                // pixels above the scene are not cleared by the scene pass.
                const auto paintForestHeader = [&](float height)
                {
                    gui::panel_rect({forestControlPosition.x, gui::cursor_position().y},
                        {forestControlSize.x, height});
                };
                const std::string_view forestInstructions = ready
                    ? "Adjust the plant here; release a slider to update its preview. Higher branch levels add smaller branches. Add to Scene places a copy in your project."
                    : editable ? "Preview unavailable. Reduce branches or leaves, or use Undo. The previous preview is retained."
                    : "The plant source could not be loaded. Its saved files have not been replaced.";
                const gui::InlineButtonRowOptions forestButtons{
                    .sizing = gui::InlineButtonSizing::FitContent,
                    .alignment = gui::InlineButtonAlignment::Center,
                    .wrap = true, .available_width = forestTextWidth};
                paintForestHeader(gui::measure_section_heading_height("Plant preview", forestTextWidth)
                    + gui::measure_wrapped_label_height(forestInstructions, forestTextWidth));
                gui::section_heading("Plant preview");
                gui::wrapped_label(forestInstructions, forestTextWidth);
                // Sliders own pointer capture. Sample the shared mouse state
                // before they consume release, and keep drafts outside the
                // document so dragging neither rebuilds meshes nor fills Undo.
                const bool sliderMouseDown = gui::is_mouse_down();
                const bool sliderMouseReleased = gui::was_mouse_released();
                const bool sliderMousePressed = gui::was_mouse_pressed();
                auto& sliderDraft = editor.forestProfileSliderDraft;
                if (sliderDraft && (sliderMousePressed || !editable
                        || (!sliderMouseDown && !sliderMouseReleased)))
                    sliderDraft.reset();
                if (sliderDraft && (sliderDraft->revision != editor.plantLabDocument.revision
                        || sliderDraft->contentHash != editor.plantLabDocument.contentHash
                        || sliderDraft->projectId != editor.projectId
                        || sliderDraft->projectRoot != editor.projectRoot))
                    sliderDraft->cancelled = true;

                using ForestProperty = forest::ForestProfileProperty;
                struct ForestSliderSpec
                {
                    std::string_view id, label, unit;
                    ForestProperty property;
                    float minimum, standardMaximum, maximum, step;
                };
                const std::array sliderSpecs{
                    ForestSliderSpec{"forest.height", "Height", " m", ForestProperty::TargetHeightMeters, .1f, 32.f, 256.f, .05f},
                    ForestSliderSpec{"forest.branch_levels", "Branch levels", "", ForestProperty::BranchLevels, 0.f, 8.f, 32.f, 1.f},
                    ForestSliderSpec{"forest.branch_length", "Branch length", " m", ForestProperty::BranchLengthMeters, .01f, 8.f, 128.f, .01f},
                    ForestSliderSpec{"forest.leaf_size", "Leaf size", " m", ForestProperty::LeafSizeMeters, .02f, 2.f, 2.f, .01f},
                    ForestSliderSpec{"forest.leaf_sites", "Leaf groups per branch", "", ForestProperty::LeafSitesPerBranch, 1.f, 16.f, 16.f, 1.f}};
                const auto& sliderProfile = editor.plantLabDocument.profile;
                const auto sliderCanopy = forest::forest_canopy_settings(sliderProfile);
                const std::array sliderValues{sliderProfile.config.targetHeightMeters,
                    static_cast<float>(sliderProfile.branch.levels), sliderProfile.branch.branchLengthMeters,
                    sliderCanopy.leafSizeMeters, static_cast<float>(sliderCanopy.leafSitesPerBranch)};
                const bool fitsStandardRange = sliderValues[0] <= 32.f
                    && sliderValues[1] <= 8.f && sliderValues[2] <= 8.f;
                if (!fitsStandardRange) editor.forestExtendedRange = true;
                const auto sliderLabel = [](const ForestSliderSpec& spec, float value)
                {
                    return spec.unit.empty() ? epochengine::format_text("{}: {:.0f}", spec.label, value)
                        : epochengine::format_text("{}: {:.2f}{}", spec.label, value, spec.unit);
                };
                const auto sliderColumns = (std::clamp)(static_cast<std::size_t>(forestTextWidth / 140.f),
                    std::size_t{1u}, std::size_t{3u});
                const float sliderWidth = (forestTextWidth - 8.f * static_cast<float>(sliderColumns - 1u))
                    / static_cast<float>(sliderColumns);
                float sliderLabelHeight = 0.f;
                for (const auto& spec : sliderSpecs)
                    sliderLabelHeight = (std::max)(sliderLabelHeight,
                        gui::measure_wrapped_label_height(sliderLabel(spec, spec.maximum), sliderWidth));
                const float sliderHeight = (std::max)(28.f, gui::line_height() + 8.f);
                const float sliderRowHeight = sliderLabelHeight + sliderHeight + 8.f;
                const auto sliderOrigin = gui::cursor_position();
                const float sliderGridHeight = sliderRowHeight
                    * static_cast<float>((sliderSpecs.size() + sliderColumns - 1u) / sliderColumns);
                paintForestHeader(sliderGridHeight);
                for (std::size_t index = 0u; index < sliderSpecs.size(); ++index)
                {
                    const auto& spec = sliderSpecs[index];
                    const gui::Vec2 cell{sliderOrigin.x + static_cast<float>(index % sliderColumns) * (sliderWidth + 8.f),
                        sliderOrigin.y + static_cast<float>(index / sliderColumns) * sliderRowHeight};
                    const bool hasDraft = sliderDraft && !sliderDraft->cancelled && sliderDraft->property == spec.property;
                    gui::set_cursor({cell.x, cell.y + sliderLabelHeight});
                    const auto control = gui::slider({.id = spec.id, .minimum = spec.minimum,
                        .maximum = editor.forestExtendedRange ? spec.maximum : spec.standardMaximum,
                        .value = hasDraft ? sliderDraft->value : sliderValues[index], .step = spec.step,
                        .size = {sliderWidth, sliderHeight}, .enabled = editable && !(sliderDraft && sliderDraft->cancelled)});
                    if ((control.changed || (sliderMousePressed && control.hovered))
                        && editable && !(sliderDraft && sliderDraft->cancelled))
                    {
                        if (!sliderDraft)
                            sliderDraft.emplace(decltype(editor.forestProfileSliderDraft)::value_type{
                                .property = spec.property, .value = control.value,
                                .revision = editor.plantLabDocument.revision, .contentHash = editor.plantLabDocument.contentHash,
                                .projectId = editor.projectId, .projectRoot = editor.projectRoot});
                        else if (sliderDraft->property == spec.property)
                            sliderDraft->value = control.value;
                    }
                    gui::set_cursor(cell);
                    gui::wrapped_label(sliderLabel(spec, control.value), sliderWidth);
                }
                gui::set_cursor({sliderOrigin.x, sliderOrigin.y + sliderGridHeight});
                if (sliderMouseReleased && sliderDraft)
                {
                    if (!sliderDraft->cancelled && forest::apply_forest_profile_edit(editor.plantLabDocument,
                            sliderDraft->property, sliderDraft->value))
                        ready = ensure_forest_factory_preview(editor);
                    sliderDraft.reset();
                }
                const auto& foliageProfile = editor.plantLabDocument.profile;
                const std::array<gui::InlineButtonSpec, 3> foliageEdits{{
                    {"2D Plant", 0.0f, editable}, {"3D Plant", 0.0f, editable},
                    {editor.forestExtendedRange ? "Extended range" : "Standard range", 0.0f,
                        editable && (!editor.forestExtendedRange || fitsStandardRange)}}};
                paintForestHeader(gui::inline_button_row_height(foliageEdits, 28.0f, 6.0f, forestButtons) + 8.0f);
                if (const auto clicked = gui::inline_button_row(foliageEdits, 28.0f, 6.0f, forestButtons))
                {
                    using Property = forest::ForestProfileProperty;
                    sliderDraft.reset();
                    if (*clicked == 2u) editor.forestExtendedRange = !editor.forestExtendedRange;
                    else if (forest::apply_forest_profile_edit(editor.plantLabDocument, Property::PreviewMode,
                            static_cast<float>(*clicked == 0u ? forest::ForestPreviewMode::Mode2D : forest::ForestPreviewMode::Mode3D)))
                        ready = ensure_forest_factory_preview(editor);
                }
                const auto canopy = forest::forest_canopy_settings(foliageProfile);
                const auto foliageSummary = !canopy.enabled
                    ? std::string{"Tip-only leaves. Adjust leaf size or leaf groups to add a canopy."}
                    : epochengine::format_text(
                    "{} plant | Leaf size {:.2f} m | {} leaf groups per outer branch",
                    foliageProfile.previewMode == forest::ForestPreviewMode::Mode2D ? "2D" : "3D",
                    canopy.leafSizeMeters, canopy.leafSitesPerBranch);
                paintForestHeader(gui::measure_wrapped_label_height(foliageSummary, forestTextWidth));
                gui::wrapped_label(foliageSummary, forestTextWidth);
                const std::array<gui::InlineButtonSpec, 2> barkEdits{{
                    {"Previous bark", 0.0f, editable}, {"Next bark", 0.0f, editable}}};
                paintForestHeader(gui::inline_button_row_height(barkEdits, 28.0f, 6.0f, forestButtons) + 8.0f);
                if (const auto clicked = gui::inline_button_row(barkEdits, 28.0f, 6.0f, forestButtons))
                {
                    constexpr auto count = forest::preview_mesh::kBarkTileCount;
                    const auto current = editor.plantLabDocument.profile.barkTileIndex;
                    const auto next = (current + (*clicked == 0u ? count - 1u : 1u)) % count;
                    if (forest::apply_forest_profile_edit(editor.plantLabDocument,
                            forest::ForestProfileProperty::BarkTileIndex, static_cast<float>(next)))
                        ready = ensure_forest_factory_preview(editor);
                }
                const auto barkSummary = epochengine::format_text("Bark {} / {}",
                    editor.plantLabDocument.profile.barkTileIndex + 1u, forest::preview_mesh::kBarkTileCount);
                paintForestHeader(gui::measure_wrapped_label_height(barkSummary, forestTextWidth));
                gui::wrapped_label(barkSummary, forestTextWidth);
                const std::array<gui::InlineButtonSpec, 5> actions{{
                    {"Undo", 0.0f, editable}, {"Redo", 0.0f, editable},
                    {has_open_project(editor) ? "Add to Scene" : "Create Project & Add Plant...",
                        0.0f, ready},
                    {"Back to Scene", 0.0f}, {"Advanced Plant Lab", 0.0f, editable}}};
                paintForestHeader(gui::inline_button_row_height(actions, 28.0f, 6.0f, forestButtons) + 8.0f);
                if (const auto clicked = gui::inline_button_row(actions, 28.0f, 6.0f, forestButtons))
                {
                    if (*clicked < 2u)
                    {
                        const bool changed = *clicked == 0u
                            ? forest::undo_forest_profile_edit(editor.plantLabDocument)
                            : forest::redo_forest_profile_edit(editor.plantLabDocument);
                        if (changed) ready = ensure_forest_factory_preview(editor);
                    }
                    else if (*clicked == 2u)
                    {
                        if (!has_open_project(editor))
                        {
                            if (begin_forest_project_creation(editor))
                                apply_editor_surface(EditorMainSurface::Project, "Create Project for Plant");
                        }
                        else if (place_forest_factory_asset(editor) > 0u)
                        {
                            (void)save_editor_scene_snapshot(editor);
                            apply_editor_surface(EditorMainSurface::Scene, "Add Plant to Scene");
                            (void)focus_forest_factory_placement(ctx.get(), editor);
                        }
                    }
                    else if (*clicked == 3u)
                        apply_editor_surface(EditorMainSurface::Scene, "Plant Preview");
                    else
                    {
                        result.command = EditorCommand::OpenContextWindow;
                        result.command_argument = "application.plant_lab";
                        result.requested_context_type = ctx->type;
                    }
                }
                const bool refreshReady=ready&&has_open_project(editor)&&!editor.forestSurfacePending
                    &&!editor.projectBuildPending&&!editor.projectRuntimeProcess.valid();
                const bool placementRefreshReady=has_open_project(editor)&&!editor.forestSurfacePending
                    &&!editor.projectBuildPending&&!editor.planetaryGeneratePending&&!editor.projectRuntimeProcess.valid();
                const std::array refreshActions{gui::InlineButtonSpec{"Update Painted Trees",0.0f,refreshReady},
                    gui::InlineButtonSpec{"Refresh Forest Placement",0.0f,placementRefreshReady}};
                paintForestHeader(gui::inline_button_row_height(refreshActions,28.0f,6.0f,forestButtons)+8.0f);
                if(const auto clicked=gui::inline_button_row(refreshActions,28.0f,6.0f,forestButtons))
                {
                    if(*clicked==0u)(void)refresh_forest_factory_instances(editor);
                    else (void)begin_forest_surface_refresh(editor);
                }
                const std::array paintModeActions{
                    gui::InlineButtonSpec{"Paint Forest on Terrain",0.0f,ready&&has_open_project(editor)}};
                paintForestHeader(gui::inline_button_row_height(paintModeActions,28.0f,6.0f,forestButtons)+8.0f);
                if(gui::inline_button_row(paintModeActions,28.0f,6.0f,forestButtons))
                {
                    editor.forestDesignCamera=previewgrid::capture_camera_rig_snapshot(ctx.get());
                    editor.forestTerrainPainting=true;
                    if(editor.forestProjectCamera)
                        (void)previewgrid::restore_camera_rig_snapshot(ctx.get(),*editor.forestProjectCamera);
                }
                paintForestHeader(gui::measure_wrapped_label_height(editor.plantLabStatus,forestTextWidth));
                gui::wrapped_label(editor.plantLabStatus,forestTextWidth);
            }
            if(editor.mainSurface==EditorMainSurface::ForestFactory&&editor.forestTerrainPainting)
            {
                const bool ready=ensure_forest_factory_preview(editor);
                const float forestTextWidth=(std::max)(1.0f,forestControlSize.x-16.0f);
                const gui::InlineButtonRowOptions forestButtons{
                    .sizing=gui::InlineButtonSizing::FitContent,
                    .alignment=gui::InlineButtonAlignment::Center,
                    .wrap=true,.available_width=forestTextWidth};
                const auto paintForestHeader=[&](float height) {
                    gui::panel_rect({forestControlPosition.x,gui::cursor_position().y},{forestControlSize.x,height});
                };
                paintForestHeader(gui::measure_section_heading_height("Forest Painting",forestTextWidth));
                gui::section_heading("Forest Painting");
                constexpr std::string_view paintHelp="Hold the left mouse button and drag over terrain to paint forest areas. Right-drag moves the camera. Green marks planted regions; amber marks the selected region. Scene Undo removes an area. Done Painting restores the scene.";
                paintForestHeader(gui::measure_wrapped_label_height(paintHelp,forestTextWidth));
                gui::wrapped_label(paintHelp,forestTextWidth);
                const auto areaSummary=epochengine::format_text(
                    "Paint Area: radius {:.1f} m, density {:.3f}/m2, seed {}, up to {} selected-plant trees beneath the scene camera. Select a generated planet in 3D Objects; underwater samples are skipped.",
                    editor.forestArea.radiusMeters,editor.forestArea.densityPerSquareMeter,editor.forestArea.seed,
                    forest::forest_area_candidate_count(editor.forestArea));
                paintForestHeader(gui::measure_wrapped_label_height(areaSummary,forestTextWidth));
                gui::wrapped_label(areaSummary,forestTextWidth);
                const bool areaReady=ready&&has_open_project(editor)&&!editor.forestSurfacePending
                    &&!editor.projectBuildPending&&!editor.projectRuntimeProcess.valid();
                const std::array<gui::InlineButtonSpec,9> areaActions{{
                    {"Radius -",0.0f,areaReady},{"Radius +",0.0f,areaReady},
                    {"Density -",0.0f,areaReady},{"Density +",0.0f,areaReady},{"Seed +",0.0f,areaReady},
                    {"Paint Area",0.0f,areaReady},{"Erase Selected Area",0.0f,areaReady},
                    {"Cancel Placement",0.0f,editor.forestSurfacePending.has_value()},
                    {"Refresh Forest Placement",0.0f,has_open_project(editor)&&!editor.forestSurfacePending
                        &&!editor.projectBuildPending&&!editor.planetaryGeneratePending&&!editor.projectRuntimeProcess.valid()}}};
                paintForestHeader(gui::inline_button_row_height(areaActions,28.0f,6.0f,forestButtons)+8.0f);
                if(const auto clicked=gui::inline_button_row(areaActions,28.0f,6.0f,forestButtons))
                {
                    if(*clicked==0u)editor.forestArea.radiusMeters=(std::max)(0.25,editor.forestArea.radiusMeters-2.0);
                    else if(*clicked==1u)editor.forestArea.radiusMeters=(std::min)(1000.0,editor.forestArea.radiusMeters+2.0);
                    else if(*clicked==2u)editor.forestArea.densityPerSquareMeter=(std::max)(0.001,editor.forestArea.densityPerSquareMeter-0.005);
                    else if(*clicked==3u)editor.forestArea.densityPerSquareMeter=(std::min)(1.0,editor.forestArea.densityPerSquareMeter+0.005);
                    else if(*clicked==4u)++editor.forestArea.seed;
                    else if(*clicked==5u)
                    {
                        editor.forestPlacementCamera=previewgrid::camera_for(ctx.get());
                        (void)paint_forest_factory_area(editor);
                    }
                    else if(*clicked==6u)(void)erase_selected_forest_area(editor);
                    else if(*clicked==8u)(void)begin_forest_surface_refresh(editor);
                    else if(editor.taskScheduler)
                    {
                        (void)editor.taskScheduler->cancel(editor.forestSurfaceCancellation);
                        editor.forestSurfacePlacement.reset();
                        editor.plantLabStatus="Plant placement cancelled; scene unchanged.";
                    }
                }
                paintForestHeader(gui::measure_wrapped_label_height(editor.plantLabStatus, forestTextWidth));
                gui::wrapped_label(editor.plantLabStatus, forestTextWidth);
                const std::array paintViewActions{
                    gui::InlineButtonSpec{"Edit Plant",0.0f},gui::InlineButtonSpec{"Done Painting",0.0f}};
                paintForestHeader(gui::inline_button_row_height(paintViewActions,28.0f,6.0f,forestButtons)+8.0f);
                if(const auto selected=gui::inline_button_row(paintViewActions,28.0f,6.0f,forestButtons))
                {
                    editor.forestTerrainPainting=false;
                    editor.forestPlacementCamera.reset();
                    editor.forestProjectCamera=previewgrid::capture_camera_rig_snapshot(ctx.get());
                    if (*selected == 1u)
                        editor.requestedMainSurface = EditorMainSurface::Scene;
                    else if(editor.forestDesignCamera)
                        (void)previewgrid::restore_camera_rig_snapshot(ctx.get(),*editor.forestDesignCamera);
                    else focus_forest_factory_preview(ctx.get(),editor);
                }
            }
                }
                gui::end_scroll_area();
                if (panels[1].position.x > panels[0].position.x)
                    gui::panel_rect({panels[0].position.x + panels[0].size.x, panels[0].position.y},
                        {8.0f, panels[0].size.y});
                else
                    gui::panel_rect({panels[0].position.x, panels[0].position.y + panels[0].size.y},
                        {panels[0].size.x, panels[1].position.y - panels[0].position.y - panels[0].size.y});
                gui::set_cursor(panels[1].position);
            }
            const std::string_view sceneTitle = editor.mainSurface == EditorMainSurface::Game2D
                ? (editor.game2DGuiWorkspace ? "UI" : "2D") : main_surface_title(editor.mainSurface);
            const gui::Vec2 sceneCursor = gui::cursor_position();
            const gui::Vec2 scene_pos{
                std::clamp(sceneCursor.x, viewport_pos.x, viewport_pos.x + (std::max)(0.0f, viewport_size.x)),
                std::clamp(sceneCursor.y, viewport_pos.y, viewport_pos.y + (std::max)(0.0f, viewport_size.y))};
            const float sceneAvailableWidth = (std::max)(0.0f, viewport_pos.x + viewport_size.x - scene_pos.x);
            const float sceneAvailableHeight = (std::max)(0.0f, viewport_pos.y + viewport_size.y - scene_pos.y);
            // One icon/readout row and the real scrubber. Reserve exactly the
            // shared measured content, including wrapping in a narrow viewport.
            const float timelineStripRequiredHeight = 8.0f
                + timeline_compact_controls_height((std::max)(1.0f, sceneAvailableWidth - 16.0f));
            constexpr float sceneMinHeightWithTimeline = 100.0f;
            const bool showSceneTimeline = !projectViewportMaximized
                && editor.mainSurface != EditorMainSurface::ForestFactory && sceneAvailableWidth>=80.0f&&sceneAvailableHeight >=
                timelineStripRequiredHeight + sceneMinHeightWithTimeline;
            const float timelineStripGap = 0.0f;
            const float timelineStripHeight = showSceneTimeline
                ? timelineStripRequiredHeight : 0.0f;
            const gui::Vec2 scene_size{
                sceneAvailableWidth,
                (std::max)(0.0f, sceneAvailableHeight - timelineStripHeight - timelineStripGap)
            };
            // A dock can be shorter than its instructions and title. Do not
            // manufacture a minimum scene height outside the space it owns.
            const bool sceneFits = scene_size.x >= 80.0f
                && scene_size.y >= gui::titled_window_total_height(0.0f);
            result.scene_viewport = sceneFits
                ? gui::scene_viewport(projectViewportMaximized ? std::string_view{} : sceneTitle, scene_pos, scene_size)
                : gui::WidgetBounds{.position=scene_pos, .size={0.0f, 0.0f}};
            editor.planetaryViewportHeight = static_cast<std::uint32_t>(std::clamp(scene_size.y, 64.0f, 16384.0f));
            editor.planetaryViewportWidth = static_cast<std::uint32_t>(std::clamp(scene_size.x, 64.0f, 16384.0f));
            // None stops scene submissions, but does not erase the old native
            // pixels. Paint exposed content during child moves/resizes and when
            // preview is disabled; preserve the user's authoring mode for Stop.
            if (sceneFits && (editor_project_viewport_input_reserved(editor)
                || editor.previewMode == core::ScenePreviewMode::None))
                gui::panel_rect(result.scene_viewport.position, result.scene_viewport.size);
            // Ordinary scenes run in the current Editor. A compiled optional
            // provider keeps its verified child and docks that owned window.
            // Separate Window remains an explicit external launch.
            constexpr std::array<std::string_view, 3> kSceneRunModes{
                "Separate Window", "In Editor", "In-Editor Full Screen"};
            const bool compiledProjectRunning = editor.projectRuntimeInEditor
                && editor.projectRuntimeProcess.valid();
            // Until the owned runtime attaches, its viewport still shows the
            // authoring scene. A click intended for play must not edit that scene.
            result.scene_input_captured = result.scene_input_captured
                || editor_project_viewport_input_reserved(editor);
            const auto sceneRunButtonLabel = project_scene_run_button_label(editor, compiledProjectRunning);
            const float runButtonWidth = sceneRunButtonLabel.size() > 13u ? 196.0f : 112.0f;
            const float runControlOffset = projectViewportMaximized ? 8.0f : 96.0f;
            const float runModeWidth = projectViewportMaximized
                ? (std::min)(172.0f, (std::max)(80.0f, scene_size.x - runButtonWidth - 20.0f))
                : (std::min)(172.0f, (std::max)(100.0f, scene_size.x * 0.20f));
            const bool showSceneRun = sceneFits
                && scene_size.x >= runControlOffset + runButtonWidth + (editor.projectBuildPending ? 0.0f : runModeWidth) + 12.0f
                && application.allow_project_run
                && has_open_project(editor) && editor.mainSurface != EditorMainSurface::ForestFactory;
            const bool showMeshDrawMode = sceneFits && !projectViewportMaximized
                && editor.mainSurface != EditorMainSurface::Game2D;
            const float meshDrawWidth = (std::min)(112.0f, (std::max)(0.0f, scene_size.x - 16.0f));
            const float headerOccupiedLeft = showSceneRun
                ? runControlOffset + runButtonWidth + 4.0f
                    + (editor.projectBuildPending ? 0.0f : runModeWidth)
                : 96.0f;
            const bool meshDrawSecondRow = showMeshDrawMode
                && scene_size.x < headerOccupiedLeft + meshDrawWidth + 16.0f;
            if (meshDrawSecondRow)
            {
                // Native project children must begin below the wrapped toolbar.
                const float headerBottom = scene_pos.y + 52.0f;
                const float inset = std::clamp(headerBottom - result.scene_viewport.position.y,
                    0.0f, result.scene_viewport.size.y);
                result.scene_viewport.position.y += inset;
                result.scene_viewport.size.y -= inset;
            }
            const float sceneNoticeTop = meshDrawSecondRow ? 52.0f : 26.0f;
            if (showSceneRun)
            {
                gui::set_cursor({scene_pos.x + runControlOffset, scene_pos.y + 2.0f});
                if (editor.projectBuildPending && !compiledProjectRunning)
                {
                    const std::array busyRun{gui::InlineButtonSpec{
                        .label=sceneRunButtonLabel, .width=runButtonWidth, .enabled=false}};
                    (void)gui::inline_button_row(busyRun, 20.0f, 0.0f,
                        gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent,
                            .available_width=runButtonWidth});
                }
                else
                {
                    // Restore events for this existing Stop only, never for Run
                    // or the adjacent authoring controls during automation.
                    if (automationInputReserved && compiledProjectRunning)
                        automationBackgroundInput.reset();
                    const bool runClicked = gui::button(sceneRunButtonLabel, {runButtonWidth, 20.0f})
                        && !menusBlockedByModal && !toolbarControlsBlockedByMenu;
                    if (automationInputReserved && compiledProjectRunning)
                        automationBackgroundInput.emplace();
                    if (runClicked)
                    {
                        if (compiledProjectRunning)
                            (void)stop_project_runtime(editor,
                                platform::child_process::StopMode::graceful);
                        else if (editor.sceneRunFullscreenInEditor
                            || editor.projectKind == "Engine Development")
                            play_active_context();
                        else
                            launch_active_project_context();
                    }
                }
                // A Run click above may have queued the build in this frame.
                // Read its current state instead of the pre-click activity label.
                const auto currentBuildActivity = project_build_activity_label(editor);
                const std::string runStatus = editor.projectBuildPending
                    ? currentBuildActivity + " " + editor.projectBuildStatus
                    : !editor.projectRuntimeAttachmentError.empty()
                        ? editor.projectRuntimeAttachmentError
                        : compiledProjectRunning && !editor.projectRuntimeWindow
                            ? editor.projectRuntimeStatus : std::string{};
                record_editor_run_output(editor, runStatus);
                if (!runStatus.empty())
                {
                    const std::string shortRunStatus = editor.projectBuildPending
                        ? currentBuildActivity
                        : !editor.projectRuntimeAttachmentError.empty()
                            ? "Project launch failed. See Output."
                            : editor.projectRuntimeForceStopAvailable
                                ? "Stopping project..." : "Starting project in the Editor...";
                    const float availableStatusWidth = (std::max)(1.0f, scene_size.x - 16.0f);
                    const gui::Vec2 statusPosition{scene_pos.x + 8.0f, scene_pos.y + sceneNoticeTop};
                    const std::array statusMetric{gui::SegmentedButtonSpec{.label=shortRunStatus}};
                    const std::array outputMetric{gui::SegmentedButtonSpec{.label="Show Output"}};
                    const float outputButtonWidth = (std::min)(availableStatusWidth,
                        gui::segmented_button_row_width(outputMetric));
                    const float statusWidth = (std::min)(availableStatusWidth,
                        gui::segmented_button_row_width(statusMetric) + outputButtonWidth + 8.0f);
                    const bool inlineOutputButton = statusWidth >= outputButtonWidth + 112.0f;
                    const float statusTextWidth = inlineOutputButton
                        ? statusWidth - outputButtonWidth - 8.0f : statusWidth;
                    const float statusTextHeight = (std::min)(2.0f * gui::line_height(),
                        gui::measure_wrapped_label_height(shortRunStatus, statusTextWidth));
                    const float statusHeight = (std::min)(
                        inlineOutputButton ? (std::max)(24.0f, statusTextHeight) : statusTextHeight + 28.0f,
                        (std::max)(0.0f, scene_size.y - sceneNoticeTop));
                    if (statusHeight > 0.0f)
                    {
                        const gui::ContentClipScope statusClip{statusPosition,
                            {statusPosition.x + statusWidth, statusPosition.y + statusHeight}};
                        gui::panel_rect(statusPosition, {statusWidth, statusHeight});
                        gui::set_cursor(statusPosition);
                        gui::wrapped_label(shortRunStatus, statusTextWidth);
                        gui::set_cursor(inlineOutputButton
                            ? gui::Vec2{statusPosition.x + statusWidth - outputButtonWidth, statusPosition.y}
                            : gui::Vec2{statusPosition.x, statusPosition.y + statusTextHeight + 4.0f});
                        if (gui::button("Show Output", {outputButtonWidth, 24.0f})
                            && !menusBlockedByModal && !toolbarControlsBlockedByMenu)
                        {
                            exit_project_runtime_full_screen(editor);
                            editor.dockStatusTab = EditorWorkspaceTab::Output;
                            editor.outputGeneralOnly = false;
                            editor.outputShowDetails = false;
                            editor.outputSearch.clear();
                            request_pane_region("pane.output", "Output", pane_dock_region("pane.output"));
                        }
                    }
                }
                if (projectViewportMaximized)
                {
                    gui::set_cursor({scene_pos.x + runControlOffset + runButtonWidth + 4.0f,
                        scene_pos.y + 2.0f});
                    if (gui::button(runModeWidth >= 140.0f ? "Exit Full Screen" : "Exit (Esc)", {runModeWidth, 20.0f})
                        && !menusBlockedByModal && !toolbarControlsBlockedByMenu)
                        exit_project_runtime_full_screen(editor);
                }
                else if (!editor.projectBuildPending && scene_size.x >= 440.0f)
                {
                    gui::set_cursor({scene_pos.x + runControlOffset + runButtonWidth + 4.0f,
                        scene_pos.y + 2.0f});
                    const auto modeChoice = gui::select_box(gui::SelectBoxOptions{
                        .id = "scene-run-mode-select",
                        .placeholder = "Run mode",
                        .selected = kSceneRunModes[scene_run_mode_index(editor)],
                        .options = kSceneRunModes,
                        .size = {runModeWidth, 20.0f},
                        .row_height = 26.0f,
                        .max_visible_options = 3u,
                        .enabled = !menusBlockedByModal});
                    if (modeChoice.changed && modeChoice.selected_index)
                        select_scene_run_mode(editor, static_cast<unsigned>(*modeChoice.selected_index));
                    result.scene_input_captured = result.scene_input_captured || modeChoice.opened;
                }
                else if (!editor.projectBuildPending)
                {
                    // At narrow widths the dropdown becomes a directly
                    // actionable mode toggle; Run itself stays visible.
                    gui::set_cursor({scene_pos.x + runControlOffset + runButtonWidth + 4.0f,
                        scene_pos.y + 2.0f});
                    constexpr std::array<std::string_view, 3> compactRunModes{
                        "Separate", "In Editor", "Full Screen"};
                    if (gui::button(compactRunModes[scene_run_mode_index(editor)],
                        {runModeWidth, 20.0f}))
                        select_scene_run_mode(editor, (scene_run_mode_index(editor) + 1u) % 3u);
                }
            }
            result.scene_input_captured = result.scene_input_captured
                || editor_project_viewport_input_reserved(editor);
            if (showMeshDrawMode)
            {
                const gui::Vec2 modePosition{scene_pos.x + scene_size.x - meshDrawWidth - 8.0f,
                    scene_pos.y + (meshDrawSecondRow ? 26.0f : 2.0f)};
                if (meshDrawSecondRow) gui::panel_rect(modePosition, {meshDrawWidth, 20.0f});
                gui::set_cursor(modePosition);
                const bool authoringView = !editor_project_viewport_input_reserved(editor)
                    && editor.previewMode == core::ScenePreviewMode::Editor;
                using ControlStatus=platform::child_process::RuntimeControlStatus;
                const auto runtimeControl=editor.projectRuntimeSnapshot
                    ?editor.projectRuntimeSnapshot->runtime_control:platform::child_process::RuntimeControlSnapshot{};
                const bool runtimeView=editor_project_viewport_covered(editor)
                    &&runtimeControl.status!=ControlStatus::unavailable;
                constexpr std::array<std::string_view, 2> meshDrawModes{"Solid", "Wireframe"};
                const bool wireframe=runtimeView
                    ?runtimeControl.acknowledged_mode==platform::child_process::MeshDrawMode::wireframe
                    :editor.viewportMeshWireframe;
                const std::string_view drawModeLabel=runtimeView&&runtimeControl.status==ControlStatus::pending
                    ?(wireframe?"Wire (wait)":"Solid (wait)")
                    :runtimeView&&runtimeControl.status==ControlStatus::rejected
                        ?(wireframe?"Wire (refused)":"Solid (refused)")
                        :runtimeView&&runtimeControl.status==ControlStatus::disconnected
                            ?"Disconnected":wireframe?(meshDrawWidth<100.0f?"Wire":"Wireframe"):"Solid";
                const auto drawChoice = gui::select_box(gui::SelectBoxOptions{
                    .id = "scene-mesh-draw-mode", .placeholder = "Mesh drawing",
                    .selected = drawModeLabel,
                    .options = meshDrawModes, .size = {meshDrawWidth, 20.0f},
                    .row_height = 26.0f, .max_visible_options = 2u,
                    .enabled = (authoringView||runtimeView)&&!menusBlockedByModal&&!toolbarControlsBlockedByMenu
                        &&(!runtimeView||(runtimeControl.status!=ControlStatus::pending
                            &&runtimeControl.status!=ControlStatus::disconnected))});
                if (!menusBlockedByModal && !toolbarControlsBlockedByMenu && drawChoice.changed
                    && drawChoice.selected_index && *drawChoice.selected_index < meshDrawModes.size())
                {
                    if(runtimeView)(void)request_project_runtime_mesh_mode(editor,*drawChoice.selected_index==1u);
                    else if(authoringView)editor.viewportMeshWireframeRequest=*drawChoice.selected_index==1u;
                }
                if(!authoringView)editor.viewportMeshWireframeRequest.reset();
                result.scene_input_captured = result.scene_input_captured || drawChoice.opened;
            }
            constexpr std::array<std::string_view, 8> kEditorViewLabels{
                "Perspective",
                "Orthographic",
                "Front",
                "Back",
                "Left",
                "Right",
                "Top",
                "Bottom"
            };
            constexpr std::array<previewgrid::EditorView, 8> kEditorViews{
                previewgrid::EditorView::Perspective,
                previewgrid::EditorView::FreeOrthographic,
                previewgrid::EditorView::Front,
                previewgrid::EditorView::Back,
                previewgrid::EditorView::Left,
                previewgrid::EditorView::Right,
                previewgrid::EditorView::Top,
                previewgrid::EditorView::Bottom
            };
            const float viewSelectWidth = (std::min)(148.0f, (std::max)(112.0f, scene_size.x * 0.22f));
            const float focusButtonWidth = 58.0f;
            const float meshHeaderInset = showMeshDrawMode && !meshDrawSecondRow ? meshDrawWidth + 6.0f : 0.0f;
            const bool authoringCameraControls = !editor_project_viewport_input_reserved(editor)
                && editor.previewMode == core::ScenePreviewMode::Editor;
            const bool showViewSelect = sceneFits && !projectViewportMaximized
                && authoringCameraControls
                && scene_size.x >= headerOccupiedLeft + viewSelectWidth + focusButtonWidth + meshHeaderInset + 22.0f;
            gui::set_cursor({
                scene_pos.x + scene_size.x - viewSelectWidth - focusButtonWidth - meshHeaderInset - 14.0f,
                scene_pos.y + 2.0f
            });
            if (showViewSelect)
            {
            const auto editorViewSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "scene-viewport-view-select",
                .placeholder = "Camera view",
                .selected = previewgrid::editor_view_name(previewgrid::editor_view_for(ctx.get())),
                .options = std::span<const std::string_view>{ kEditorViewLabels.data(), kEditorViewLabels.size() },
                .size = { viewSelectWidth, 20.0f },
                .row_height = 26.0f,
                .max_visible_options = kEditorViewLabels.size()
            });
            if (editorViewSelect.changed && editorViewSelect.selected_index
                && *editorViewSelect.selected_index < kEditorViews.size())
            {
                (void)previewgrid::set_editor_view(
                    ctx.get(),
                    kEditorViews[*editorViewSelect.selected_index]);
            }
            result.scene_input_captured = result.scene_input_captured || editorViewSelect.opened;
            }
            gui::set_cursor({ scene_pos.x + scene_size.x - focusButtonWidth - meshHeaderInset - 8.0f, scene_pos.y + 2.0f });
            if (sceneFits && !projectViewportMaximized && authoringCameraControls
                && scene_size.x >= headerOccupiedLeft + focusButtonWidth + meshHeaderInset + 16.0f
                && gui::button("Focus", { focusButtonWidth, 20.0f }))
            {
                if (editor.mainSurface == EditorMainSurface::ForestFactory && !editor.forestTerrainPainting)
                    focus_forest_factory_preview(ctx.get(), editor);
                else handle_scene_tool(editor, ctx.get(), "focus_selection");
            }
            ctx->set_scene_preview_mode(sceneFits && !editor_project_viewport_input_reserved(editor)
                ? editor.previewMode : core::ScenePreviewMode::None);
            const int viewportX = (std::max)(0, static_cast<int>(std::floor(result.scene_viewport.position.x)));
            const int viewportY = (std::max)(0, static_cast<int>(std::floor(result.scene_viewport.position.y)));
            const int viewportRight = (std::max)(
                viewportX,
                static_cast<int>(std::ceil(result.scene_viewport.position.x + result.scene_viewport.size.x)));
            const int viewportBottom = (std::max)(
                viewportY,
                static_cast<int>(std::ceil(result.scene_viewport.position.y + result.scene_viewport.size.y)));
            ctx->set_scene_viewport(core::RenderViewport{
                viewportX,
                viewportY,
                (std::max)(0, viewportRight - viewportX),
                (std::max)(0, viewportBottom - viewportY)
            });
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (editor.mainSurface == EditorMainSurface::Game2D)
            {
                if (auto* tileMap = tilemap_workspace_controller(editor))
                {
                    const auto tileMapFrame =
                        editor_tilemaps::render_canvas_workspace(
                            *tileMap,
                            editor_tilemaps::CanvasRenderOptions{
                                .viewport = result.scene_viewport,
                                .interactive = true,
                                .show_inspector = editor.showInspector});
                    result.scene_input_captured = result.scene_input_captured
                        || tileMapFrame.input_captured;
                    editor.tileMapStatus = std::string(tileMap->status());
                }
            }
#endif
            const auto render_preview_context_menu = [&]()
            {
                auto& menu = editor.outlinerContextMenu;
                const auto pointer = gui::mouse_position();
                const auto frame = editor.timeSnapshot.frame_index;
                const auto generation = editor.projectLifecycle.evidence().generations.selected_project.generation;
                const auto cameraRevision = previewgrid::camera_revision_for(ctx.get());
                const bool rightHeld = ctx->is_mouse_button_held_safe(input::MouseButton::MouseRight);
                const bool inPreview = std::isfinite(pointer.x) && std::isfinite(pointer.y)
                    && editor_point_in_rect(pointer, result.scene_viewport.position, result.scene_viewport.size);
                const bool available = sceneFits && editor.mainSurface == EditorMainSurface::Scene
                    && editor.previewMode == core::ScenePreviewMode::Editor
                    && !editor_project_viewport_input_reserved(editor) && !modal_visible_now()
                    && !floating_gui_visible() && editor.openMenu == TopMenu::None
                    && ctx->has_input_focus_safe();
                if (rightHeld && !menu.preview_right_held)
                {
                    menu.preview_right_candidate = available && inPreview && !result.scene_input_captured
                        && !ctx->is_mouse_button_held_safe(input::MouseButton::MouseLeft)
                        && !ctx->is_mouse_button_held_safe(input::MouseButton::MouseMiddle)
                        && !ctx->is_key_held_safe(input::Key::LeftAlt)
                        && !ctx->is_key_held_safe(input::Key::RightAlt);
                    if (menu.preview_right_candidate)
                    {
                        menu.kind = EditorState::OutlinerContextMenu::Kind::preview_selection;
                        menu.scene = editor_scene_identity(editor);
                        menu.project_generation = generation;
                        menu.preview_press = pointer;
                        menu.preview_camera_revision = cameraRevision;
                        menu.last_frame = frame;
                    }
                }
                const bool boundCurrent = menu.scene == editor_scene_identity(editor)
                    && menu.project_generation == generation;
                bool navigating{};
                for (const auto action : {input::Action::MoveForward,input::Action::MoveBackward,
                    input::Action::MoveLeft,input::Action::MoveRight,input::Action::MoveUp,input::Action::MoveDown,
                    input::Action::RollLeft,input::Action::RollRight,input::Action::LookLeft,
                    input::Action::LookRight,input::Action::LookUp,input::Action::LookDown})
                    navigating = navigating || ctx->action_held_safe(action);
                const bool current = available && inPreview && boundCurrent && frame >= menu.last_frame
                    && frame - menu.last_frame <= 1u;
                if (!current || navigating || ctx->is_key_held_safe(input::Key::LeftAlt)
                    || ctx->is_key_held_safe(input::Key::RightAlt)
                    || ctx->is_mouse_button_held_safe(input::MouseButton::MouseLeft)
                    || ctx->is_mouse_button_held_safe(input::MouseButton::MouseMiddle)
                    || cameraRevision != menu.preview_camera_revision
                    || std::abs(pointer.x-menu.preview_press.x) >= 4.0f
                    || std::abs(pointer.y-menu.preview_press.y) >= 4.0f)
                    menu.preview_right_candidate = false;
                const bool open = !rightHeld && menu.preview_right_held && menu.preview_right_candidate;
                menu.preview_right_held = rightHeld;
                if (!rightHeld) menu.preview_right_candidate = false;
                if (menu.kind != EditorState::OutlinerContextMenu::Kind::preview_selection) return;
                menu.last_frame = frame;
                if (open) menu.anchor = pointer;
                if (!open && !menu.menu.popup.open) return;
                gui::block_background_for_popup();
                if (available) gui::clear_modal_input_capture();
                const std::array commands{gui::MenuItemOptions{
                    .label="Deselect", .enabled=editor_has_selection(editor), .command_id=1u}};
                const auto popup = gui::command_menu(menu.menu, commands, gui::CommandMenuOptions{
                    .owner={.position=menu.anchor,.size={1.0f,1.0f}}, .viewport_size={w,h},
                    .open_requested=open, .close_requested=!available || !boundCurrent, .dismiss_on_hover_exit=false, .gap=0.0f});
                result.scene_input_captured = result.scene_input_captured || open || popup.captured;
                if (popup.activated_id == 1u && commands[0].enabled && available
                    && menu.scene == editor_scene_identity(editor) && menu.project_generation == generation)
                {
                    gui::close_command_menu(menu.menu);
                    clear_editor_selection(editor);
                    push_editor_log(editor,"[edit] World selection cleared.");
                }
            };
            render_preview_context_menu();
            update_forest_brush(ctx,editor,result,modal_visible_now());
            if (!editor_project_viewport_input_reserved(editor)
                && ctx->action_pressed_safe(input::Action::FrameSelection)
                && editor.mainSurface != EditorMainSurface::ForestFactory)
                handle_scene_tool(editor, ctx.get(), "focus_selection");
            if (!editor_project_viewport_input_reserved(editor)
                && editor.mainSurface != EditorMainSurface::ForestFactory)
            {
                update_scene_object_interaction(ctx, editor, result);
                draw_scene_marquee_overlay(editor);
                publish_editor_canvas2d_scene(ctx.get(), editor);
            }
            else (void)canvas2d::scene_content::retire(ctx.get());
            publish_editor_preview_markers(ctx.get(), editor);

            if (showSceneTimeline)
            {
                const gui::Vec2 timelinePos{ scene_pos.x, scene_pos.y + scene_size.y + timelineStripGap };
                gui::panel_rect(timelinePos, { scene_size.x, timelineStripHeight });
                gui::set_cursor({ timelinePos.x + 8.0f, timelinePos.y + 4.0f });
                render_timeline_time_controls((std::max)(1.0f, scene_size.x - 16.0f), true);
            }
        }
        else if(editor.mainSurface == EditorMainSurface::AssetGeneration)
        {
            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
            ctx->clear_scene_viewport();
            (void)canvas2d::scene_content::retire(ctx.get());
            // This workspace owns its full surface. Its native preview is the
            // only intentional transparent hole; generic document headers and
            // outer scroll regions belong to the other workspaces below.
                auto& generation=editor.assetGeneration;
                const bool reviewReady=editor.modelImportReview&&model_import_review_current(editor)
                    &&generation.generated_model&&editor.modelImportReview->sourceSha256==generation.generated_model->result.output_sha256;
                const auto generated=editor_asset_generation::draw(generation,viewport_size.x,viewport_size.y,
                    bool(editor.assetGenerationPending),editor_asset_generation::DrawPermissions{
                        .project_open=has_open_project(editor),
                        .reference_project_current=generation.reference&&editor_asset_generation::current_project(
                            *generation.reference,asset_generation_project(editor)),
                        .model_project_current=generation.generated_model&&editor_asset_generation::current_project(
                            *generation.generated_model,asset_generation_project(editor)),
                        .model_review_ready=reviewReady});
                dispatch_asset_generation(editor,ctx,generated.action);
                if(generated.preview_visible&&editor_asset_generation::prepare_preview(generation))
                {
                    if(editor.assetGenerationFocusPending)
                    {
                        previewgrid::Vec3 low{1e30f,1e30f,1e30f},high{-1e30f,-1e30f,-1e30f};
                        for(const auto& marker:generation.preview_markers)
                        {
                            const auto a=marker.mesh->minimum(),b=marker.mesh->maximum();
                            low.x=(std::min)(low.x,a[0]*marker.scale.x);low.y=(std::min)(low.y,a[1]*marker.scale.y);low.z=(std::min)(low.z,a[2]*marker.scale.z);
                            high.x=(std::max)(high.x,b[0]*marker.scale.x);high.y=(std::max)(high.y,b[1]*marker.scale.y);high.z=(std::max)(high.z,b[2]*marker.scale.z);
                        }
                        previewgrid::set_camera_mode(ctx.get(),previewgrid::CameraMode::Editor);
                        (void)previewgrid::focus_camera(ctx.get(),{(low.x+high.x)*.5f,(low.y+high.y)*.5f,(low.z+high.z)*.5f},
                            (std::max)(.01f,std::hypot(high.x-low.x,high.y-low.y,high.z-low.z)*.5f));
                        editor.assetGenerationFocusPending=false;
                    }
                    if(previewgrid::set_object_markers(ctx.get(),generation.preview_markers))
                    {
                        previewgrid::clear_lighting_frame(ctx.get());
                        result.scene_viewport=generated.preview;
                        ctx->set_scene_viewport(core::RenderViewport{
                            static_cast<int>(generated.preview.position.x),static_cast<int>(generated.preview.position.y),
                            static_cast<int>(generated.preview.size.x),static_cast<int>(generated.preview.size.y)});
                        ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                    }
                    else
                    {
                        previewgrid::clear_object_markers(ctx.get());
                        gui::panel_rect(generated.preview.position,generated.preview.size);
                        gui::set_cursor({generated.preview.position.x+8.0f,generated.preview.position.y+8.0f});
                        gui::wrapped_label("This model exceeds the current preview budget. Its verified output is retained; no scene was changed.",
                            (std::max)(1.0f,generated.preview.size.x-16.0f));
                    }
                }
                else
                {
                    previewgrid::clear_object_markers(ctx.get());
                    previewgrid::clear_lighting_frame(ctx.get());
                }
        }
        else
        {
            const gui::Vec2 centerSurfacePos = gui::cursor_position();
            const gui::Vec2 centerSurfaceSize{
                (std::max)(48.0f, viewport_pos.x + viewport_size.x - centerSurfacePos.x - 6.0f),
                (std::max)(48.0f, viewport_pos.y + viewport_size.y - centerSurfacePos.y - 6.0f)
            };
            result.scene_viewport = gui::WidgetBounds{ .position = centerSurfacePos, .size = centerSurfaceSize };
            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
            ctx->clear_scene_viewport();
            (void)canvas2d::scene_content::retire(ctx.get());

            gui::label(editor.mainSurface == EditorMainSurface::Game2D
                ? (editor.game2DGuiWorkspace ? "UI" : "2D") : std::string(main_surface_title(editor.mainSurface)));
            if (editor.mainSurface == EditorMainSurface::Project
                && has_open_project(editor))
            {
                if (editor.projectWorkspaceSection > 4u)
                    editor.projectWorkspaceSection = 0u;
                const std::array projectWorkspaceTabs{
                    gui::TabButtonSpec{
                        .id = "project.workspace.defaults",
                        .label = "Project Defaults",
                        .width = 132.0f,
                        .active = editor.projectWorkspaceSection == 0u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.input",
                        .label = "Input Manager",
                        .width = 122.0f,
                        .active = editor.projectWorkspaceSection == 1u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.audio",
                        .label = "Audio",
                        .width = 72.0f,
                        .active = editor.projectWorkspaceSection == 2u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.canvas2d",
                        .label = "Canvas2D",
                        .width = 92.0f,
                        .active = editor.projectWorkspaceSection == 3u},
                    gui::TabButtonSpec{
                        .id = "project.workspace.run",
                        .label = "Run + Build",
                        .width = 108.0f,
                        .active = editor.projectWorkspaceSection == 4u}
                };
                if (const auto tabs = gui::tab_bar_buttons(
                        projectWorkspaceTabs, 31.0f, 2.0f);
                    tabs.selected_index)
                {
                    editor.projectWorkspaceSection = *tabs.selected_index;
                }
            }
            const gui::Vec2 centerScrollStart = gui::cursor_position();
            const float centerWidth = (std::max)(1.0f, centerSurfaceSize.x - 12.0f);
            const float centerScrollHeight = (std::max)(
                1.0f,
                viewport_pos.y + viewport_size.y - centerScrollStart.y - 14.0f);
            std::string centerScrollId = std::string("center-surface-")
                + std::string(main_surface_title(editor.mainSurface));
            if (editor.mainSurface == EditorMainSurface::Project)
            {
                centerScrollId += "-"
                    + std::to_string(editor.projectWorkspaceSection);
            }
            const bool centerHasNestedSourceEditor =
#if EPOCH_EDITION_PUBLIC
                false
#endif
                || editor.mainSurface == EditorMainSurface::AssetGeneration
                || (editor.mainSurface == EditorMainSurface::Assets
                    && editor.assetWorkspaceSection == 2u);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = centerScrollId,
                .size = { (std::max)(1.0f, centerSurfaceSize.x), centerScrollHeight },
                .draw_background = false,
                .show_scrollbar = !centerHasNestedSourceEditor,
                .capture_wheel = !centerHasNestedSourceEditor
            });

            switch (editor.mainSurface)
            {
            case EditorMainSurface::Project:
            {
                gui::section_heading("Project");
                draw_project_browser(editor, ctx, centerWidth);
                const bool projectOpen = has_open_project(editor);
                if (gui::button(editor.projectCreationOpen ? "Cancel New Project" : "New Project...",
                        {(std::min)(centerWidth, 206.0f), 30.0f}))
                {
                    editor.projectCreationOpen = !editor.projectCreationOpen;
                    if (!editor.projectCreationOpen) editor.forestProjectCreationIntent.reset();
                }
                if (editor.projectCreationOpen)
                {
                    constexpr std::string_view kinds[]{"Game", "Software / Tool", "Epoch Editor"};
                    constexpr EditorProjectKind projectKinds[]{
                        EditorProjectKind::Game, EditorProjectKind::Tool, EditorProjectKind::Editor};
                    const std::size_t selectedKind = (std::min)(
                        editor.projectCreationKind, std::size(kinds) - 1u);
                    const auto kind = gui::select_box(gui::SelectBoxOptions{
                        .id = "new-project-kind", .placeholder = "Project type",
                        .selected = kinds[selectedKind],
                        .options = kinds, .size = {(std::min)(centerWidth, 340.0f), 30.0f}});
                    if (kind.changed && kind.selected_index)
                        editor.projectCreationKind = *kind.selected_index;
                    gui::wrapped_label("Creates a new project folder using the selected starter. Existing projects are never overwritten.", centerWidth);
                    if (editor.forestProjectCreationIntent)
                        gui::wrapped_label("Your current plant will be added after you confirm this new Game project.", centerWidth);
                    if (gui::button("Create Project", {(std::min)(centerWidth, 206.0f), 30.0f}))
                    {
                        const bool addingPlant = editor.forestProjectCreationIntent.has_value();
                        auto plantDocument = take_forest_project_creation(editor);
                        if (addingPlant && !plantDocument)
                            push_editor_log(editor, "[forest] " + editor.projectStatus);
                        else
                        {
                        const auto previousProjectGeneration = editor.projectLifecycle.evidence()
                            .generations.selected_project.generation;
                        const auto created = editor_create_project_shell(
                            projectKinds[selectedKind],
                            EditorProjectInputProvision::project_default
                            );
                        editor.projectStatus = created.summary;
                        if (created.succeeded)
                        {
                            const auto admitted = editor_admit_project_manifest(created.manifest_path);
                            if (admitted.succeeded && admitted.project_id == created.project_id
                                && editor_open_project(ctx, admitted.project_id))
                            {
                                editor.projectCreationOpen = false;
                                if (plantDocument && finish_forest_project_creation(editor,
                                        std::move(*plantDocument), admitted.project_id, created.root_path,
                                        admitted.manifest_path, previousProjectGeneration) > 0u)
                                {
                                    (void)save_editor_scene_snapshot(editor);
                                    apply_editor_surface(EditorMainSurface::Scene, "Add Plant to New Project");
                                    (void)focus_forest_factory_placement(ctx.get(), editor);
                                }
                            }
                            else
                                editor.projectStatus = "Project created at " + created.root_path
                                    + "; open it with Open Project File. " + admitted.summary;
                        }
                        push_editor_log(editor, "[project] " + editor.projectStatus);
                        }
                    }
                }

                if (!has_open_project(editor))
                {
                gui::section_heading("No project open", gui::SemanticTone::warning);
                    gui::wrapped_label(
                        "Choose New Project to create a starter, or open a project.epoch.json manifest. Opening loads that project authoring session without rewriting its source; Close Project releases the session without deleting files.",
                        centerWidth);
                    gui::wrapped_label(editor.projectStatus, centerWidth);
                    break;
                }

                const auto& projectProfile = active_project_profile(editor);
                if (editor.projectWorkspaceSection == 0u)
                {
                    gui::section_heading("Terrain");
                    const bool planetBusy = planetary_project_busy(editor);
                    const bool planetAttached = editor.planetaryAttachment.source_attached();
                    const gui::InlineButtonRowOptions planetButtons{
                        .sizing=gui::InlineButtonSizing::FitContent,
                        .alignment=gui::InlineButtonAlignment::Center,
                        .wrap=true, .available_width=centerWidth};
                    if (planetAttached)
                    {
                        if (editor.planetaryRadiusSynced != editor.planetarySettings.reference_radius_meters
                            || editor.planetaryRadiusReferenceSynced != editor.planetarySettings.size_reference_body)
                        {
                            editor.planetaryRadiusDraft = epochengine::format_text("{:.9g}", editor.planetarySettings.reference_radius_meters);
                            editor.planetaryRadiusSynced = editor.planetarySettings.reference_radius_meters;
                            editor.planetaryRadiusReferenceSynced = editor.planetarySettings.size_reference_body;
                        }
                        const auto readPlanetRadius = [&]() -> std::optional<double>
                        {
                            double radius{};
                            const auto parsed=std::from_chars(editor.planetaryRadiusDraft.data(),
                                editor.planetaryRadiusDraft.data()+editor.planetaryRadiusDraft.size(),radius);
                            if(parsed.ec!=std::errc{}||parsed.ptr!=editor.planetaryRadiusDraft.data()+editor.planetaryRadiusDraft.size()
                                ||!std::isfinite(radius)||radius<250.0||radius>100000000.0) return {};
                            return radius;
                        };
                        const auto readPlanetSeed = [&]() -> std::optional<std::uint32_t>
                        {
                            std::uint32_t seed{};
                            const auto parsed = std::from_chars(editor.planetarySeedDraft.data(),
                                editor.planetarySeedDraft.data() + editor.planetarySeedDraft.size(), seed);
                            if (parsed.ec != std::errc{}
                                || parsed.ptr != editor.planetarySeedDraft.data() + editor.planetarySeedDraft.size())
                                return {};
                            return seed;
                        };
                        const auto integerControl=[&](std::string_view id,std::string_view label,std::string_view unit,
                            std::uint32_t& value,float minimum,float maximum)
                        {
                            gui::wrapped_label(std::string(label)+": "+std::to_string(value)+std::string(unit),centerWidth);
                            const auto control=gui::slider(gui::SliderOptions{
                                .id=id,.minimum=minimum,.maximum=maximum,.value=static_cast<float>(value),.step=1.0f,
                                .size={(std::min)(centerWidth,400.0f),28.0f}});
                            if(control.changed)value=static_cast<std::uint32_t>(control.value);
                        };
                        const bool validPlanetSettings = project_planetary::valid_settings(editor.planetarySettings)
                            && readPlanetRadius().has_value();
                        const bool validPlanetSeed = readPlanetSeed().has_value();
                        if (!validPlanetSeed)
                            gui::wrapped_label("Enter a whole-number seed from 0 to 4294967295.", centerWidth);
                        if (!readPlanetRadius())
                            gui::wrapped_label("Enter a radius from 250 to 100000000 metres. The previous size is retained until the value is valid.", centerWidth);
                        if (!validPlanetSettings)
                            gui::wrapped_label("This combination exceeds the terrain limits. Reduce grid detail, tiles or patches, or increase the mesh budget below before saving or building.", centerWidth);
                        if (!editor.planetaryStatus.empty())
                            gui::wrapped_label(editor.planetaryStatus, centerWidth);
                        if (const auto blocked = planetary_generation_block_reason(editor); !blocked.empty())
                            gui::wrapped_label(blocked, centerWidth);
                        gui::wrapped_label("Saving settings does not run Build & Generate. The displayed mesh may be from an earlier recipe; generate to apply the requested settings.", centerWidth);
                        gui::section_heading("Terrain Settings");
                        gui::wrapped_label("Build & Generate saves the terrain overview. Camera LOD adds detail during exploration; Run opens the project controls.", centerWidth);
                        if(std::any_of(editor.entities.begin(),editor.entities.end(),[](const auto& entity) {
                            return entity.category=="Planetary Terrain"&&entity.meshAsset&&!entity.meshAsset->coordinates;
                        }))
                            gui::wrapped_label("This saved planet uses the older overview scale. Choose a size and Build & Generate with the updated extension to correct its scale. Existing placement stays until generation succeeds.",centerWidth);
                        if (!planetBusy)
                        {
                            constexpr std::string_view modes[]{"classic", "tiled", "hemisphere", "planet", "universe"};
                            const auto coverage = gui::select_box(gui::SelectBoxOptions{
                                .id="planet-coverage", .placeholder="Coverage",
                                .selected=editor.planetarySettings.mode, .options=modes,
                                .size={(std::min)(centerWidth, 320.0f), 30.0f}});
                            if (coverage.changed && coverage.selected_index)
                            {
                                editor.planetarySettings.mode = std::string(modes[*coverage.selected_index]);
                                if (!project_planetary::supports_ocean_surface(editor.planetarySettings))
                                    editor.planetarySettings.ocean_surface = false;
                                if (!project_planetary::supports_exploration(editor.planetarySettings))
                                    editor.planetarySettings.original_sunlight = false;
                            }
                            gui::label("Seed (same seed and settings produce the same terrain)");
                            (void)gui::edit_box(editor.planetarySeedDraft, {(std::min)(centerWidth, 320.0f), 30.0f}, 10u, false);
                            gui::wrapped_label("Grid detail: "+std::to_string(editor.planetarySettings.cells_per_tile)+" cells",centerWidth);
                            const auto cells = gui::slider(gui::SliderOptions{
                                .id="planet-cells", .minimum=2.0f, .maximum=64.0f,
                                .value=static_cast<float>(editor.planetarySettings.cells_per_tile), .step=1.0f,
                                .size={(std::min)(centerWidth, 400.0f), 28.0f}});
                            if (cells.changed) editor.planetarySettings.cells_per_tile = static_cast<std::uint32_t>(cells.value);
                            gui::wrapped_label("Tiles per side: "+std::to_string(editor.planetarySettings.tiles_per_axis),centerWidth);
                            const auto tiles = gui::slider(gui::SliderOptions{
                                .id="planet-tiles", .minimum=1.0f, .maximum=16.0f,
                                .value=static_cast<float>(editor.planetarySettings.tiles_per_axis), .step=1.0f,
                                .size={(std::min)(centerWidth, 400.0f), 28.0f}});
                            if (tiles.changed) editor.planetarySettings.tiles_per_axis = static_cast<std::uint32_t>(tiles.value);
                            gui::wrapped_label("Flat tile width: "+std::to_string(editor.planetarySettings.tile_span_meters)+" m",centerWidth);
                            const auto span = gui::slider(gui::SliderOptions{
                                .id="planet-span", .minimum=1.0f, .maximum=16384.0f,
                                .value=static_cast<float>(editor.planetarySettings.tile_span_meters), .step=1.0f,
                                .size={(std::min)(centerWidth, 400.0f), 28.0f}});
                            if (span.changed) editor.planetarySettings.tile_span_meters = span.value;
                            gui::wrapped_label("Planet body: "+std::to_string(editor.planetarySettings.body_index),centerWidth);
                            const auto body = gui::slider(gui::SliderOptions{
                                .id="planet-body", .minimum=0.0f, .maximum=5.0f,
                                .value=static_cast<float>(editor.planetarySettings.body_index), .step=1.0f,
                                .size={(std::min)(centerWidth, 400.0f), 28.0f}});
                            if (body.changed)
                            {
                                editor.planetarySettings.body_index = static_cast<std::uint32_t>(body.value);
                                if (!project_planetary::supports_ocean_surface(editor.planetarySettings))
                                    editor.planetarySettings.ocean_surface = false;
                            }
                            gui::section_heading("World Size");
                            gui::wrapped_label("Choose a size separately from terrain detail. Editing the radius sizes the selected body; other worlds keep their relative proportions.", centerWidth);
                            const std::array sizePresets{
                                gui::InlineButtonSpec{.label="Small: 2 km",.width=0.0f},
                                gui::InlineButtonSpec{.label="Walkable: 25 km",.width=0.0f},
                                gui::InlineButtonSpec{.label="Moon-sized",.width=0.0f},
                                gui::InlineButtonSpec{.label="Earth-sized",.width=0.0f}};
                            const auto applyRadius = [&](double radius)
                            {
                                editor.planetarySettings.size_reference_body=editor.planetarySettings.body_index;
                                editor.planetarySettings.reference_radius_meters=radius;
                                editor.planetaryRadiusDraft=epochengine::format_text("{:.9g}",radius);
                                editor.planetaryRadiusSynced=radius;
                                editor.planetaryRadiusReferenceSynced=editor.planetarySettings.size_reference_body;
                            };
                            if(const auto selected=gui::inline_button_row(sizePresets,30.0f,6.0f,planetButtons))
                                applyRadius(std::array{2000.0,25000.0,1737000.0,6371000.0}[*selected]);
                            gui::label("Radius (metres)");
                            if(gui::edit_box(editor.planetaryRadiusDraft,{(std::min)(centerWidth,320.0f),30.0f},32u,false).changed)
                                if(const auto radius=readPlanetRadius())
                                {
                                    editor.planetarySettings.size_reference_body=editor.planetarySettings.body_index;
                                    editor.planetarySettings.reference_radius_meters=*radius;
                                    editor.planetaryRadiusSynced=*radius;
                                    editor.planetaryRadiusReferenceSynced=editor.planetarySettings.size_reference_body;
                                }
                            const auto sizeSlider=gui::slider(gui::SliderOptions{
                                .id="planet-radius",.minimum=static_cast<float>(std::log10(250.0)),.maximum=8.0f,
                                .value=static_cast<float>(std::log10(editor.planetarySettings.reference_radius_meters)),.step=0.001f,
                                .size={(std::min)(centerWidth,400.0f),28.0f}});
                            if(sizeSlider.changed)applyRadius(std::clamp(std::pow(10.0,double(sizeSlider.value)),250.0,100000000.0));
                            gui::wrapped_label(epochengine::format_text("Size reference: body {}. Radius {:.3f} km; diameter {:.3f} km.",
                                editor.planetarySettings.size_reference_body,editor.planetarySettings.reference_radius_meters/1000.0,
                                editor.planetarySettings.reference_radius_meters/500.0),centerWidth);
                            gui::wrapped_label(epochengine::format_text("One full lap: about {:.1f} hours at the base game walk speed of 5.5 m/s, before terrain or stops.",
                                6.2831853071795864769*editor.planetarySettings.reference_radius_meters/5.5/3600.0),centerWidth);
                            if(editor.planetarySettings.reference_radius_meters>=100000.0)
                                gui::wrapped_label("Large sizes can lose fine ground detail with the current renderer. Test the ground view before relying on this size.",centerWidth);
                            if (project_planetary::supports_ocean_surface(editor.planetarySettings))
                            {
                                const std::array oceanActions{
                                    gui::InlineButtonSpec{
                                        .label=editor.planetarySettings.ocean_surface
                                            ? "Ocean surface: On" : "Ocean surface: Off"}};
                                if (gui::inline_button_row(oceanActions, 30.0f, 6.0f, planetButtons))
                                {
                                    editor.planetarySettings.ocean_surface =
                                        !editor.planetarySettings.ocean_surface;
                                    editor.planetaryStreamFailed = false;
                                    editor.planetaryStreamContinue = true;
                                }
                                gui::wrapped_label(
                                    "Ocean On includes Water (FFT Ocean) automatically before saving or generating. Its exact source approval appears if needed. Ocean Off keeps a dry planet without this dependency.",
                                    centerWidth);
                            }
                        }
                        if (project_planetary::supports_exploration(editor.planetarySettings))
                        {
                            if(!planetBusy&&gui::toggle_switch("Original planetary sunlight",
                                editor.planetarySettings.original_sunlight,{(std::min)(centerWidth,330.0f),28.0f}))
                                editor.planetaryStatus="Sunlight choice changed. Save Settings, then rebuild Run to apply it.";
                            gui::wrapped_label("Adds the planet system's moving sun during Run. Your scene lights and ambient lighting stay in place.",centerWidth);
                            const std::array lodActions{
                                gui::InlineButtonSpec{.label=editor.planetarySettings.mode=="universe"
                                    ?(editor.planetarySettings.stream_lod?"Runtime terrain LOD: On":"Runtime terrain LOD: Off")
                                    :(editor.planetarySettings.stream_lod?"Camera LOD: On":"Camera LOD: Off"), .width=0.0f},
                                gui::InlineButtonSpec{.label="Retry LOD", .width=0.0f, .enabled=editor.planetaryStreamFailed}};
                            if (const auto lodAction = gui::inline_button_row(lodActions, 30.0f, 6.0f, planetButtons))
                            {
                                if (*lodAction == 0u) editor.planetarySettings.stream_lod = !editor.planetarySettings.stream_lod;
                                editor.planetaryStreamFailed = false;
                                editor.planetaryStreamContinue = true;
                            }
                            if(editor.planetarySettings.mode=="universe")
                                gui::wrapped_label("The editor shows a system overview. Run the project to fly, land and stream detail on individual worlds.",centerWidth);
                            else if (!editor.planetaryStreamStatus.empty()) gui::wrapped_label(editor.planetaryStreamStatus, centerWidth);
                            if (!planetBusy)
                            {
                                const std::array detailPresets{
                                    gui::InlineButtonSpec{.label="Compact",.width=0.0f},
                                    gui::InlineButtonSpec{.label="Fast",.width=0.0f},
                                    gui::InlineButtonSpec{.label="Balanced",.width=0.0f},
                                    gui::InlineButtonSpec{.label="Fine",.width=0.0f}};
                                if(const auto profile=gui::inline_button_row(detailPresets,30.0f,6.0f,planetButtons))
                                {
                                    constexpr std::array presets{
                                        project_planetary::DetailPreset::compact,
                                        project_planetary::DetailPreset::fast,
                                        project_planetary::DetailPreset::detailed,
                                        project_planetary::DetailPreset::fine};
                                    const auto settings=project_planetary::with_detail_preset(editor.planetarySettings,
                                        presets[*profile]);
                                    if(settings)
                                    {
                                        editor.planetarySettings=*settings;
                                        editor.planetaryStatus="Detail preset selected. You can adjust its values below, then Save Settings or Build & Generate.";
                                    }
                                }
                                gui::section_heading("Custom Detail and Memory");
                                gui::wrapped_label("Presets fill in these limits. Custom values work without selecting a preset; the complete settings must fit the mesh budget.",centerWidth);
                                integerControl("planet-lod-level","Maximum terrain level","",editor.planetarySettings.maximum_level,0.0f,20.0f);
                                integerControl("planet-lod-leaves","Maximum visible terrain patches","",editor.planetarySettings.maximum_leaves,6.0f,2048.0f);
                                gui::wrapped_label(epochengine::format_text(
                                    "Terrain pixel target: {:.1f} px", editor.planetarySettings.patch_pixels), centerWidth);
                                const auto pixels = gui::slider(gui::SliderOptions{
                                    .id="planet-lod-pixels", .minimum=32.0f, .maximum=4096.0f,
                                    .value=static_cast<float>(editor.planetarySettings.patch_pixels), .step=1.0f,
                                    .size={(std::min)(centerWidth,400.0f),28.0f}});
                                if (pixels.changed) editor.planetarySettings.patch_pixels = pixels.value;
                                gui::wrapped_label("Smaller pixel targets request finer terrain. The patch and memory limits still apply.", centerWidth);
                                gui::wrapped_label(epochengine::format_text(
                                    "LOD stability: {:.0f}%", editor.planetarySettings.lod_hysteresis * 100.0), centerWidth);
                                const auto stability = gui::slider(gui::SliderOptions{
                                    .id="planet-lod-stability", .minimum=0.0f, .maximum=45.0f,
                                    .value=static_cast<float>(editor.planetarySettings.lod_hysteresis * 100.0), .step=1.0f,
                                    .size={(std::min)(centerWidth,400.0f),28.0f}});
                                if (stability.changed) editor.planetarySettings.lod_hysteresis = stability.value / 100.0;
                            }
                        }
                        if (!planetBusy)
                        {
                            gui::section_heading("Terrain Memory");
                            integerControl("planet-lod-memory","Terrain patch cache"," MiB",editor.planetarySettings.resident_megabytes,4.0f,256.0f);
                            integerControl("planet-snapshot-memory","Mesh snapshot limit"," MiB",editor.planetarySettings.snapshot_megabytes,1.0f,64.0f);
                            const auto footprint=project_planetary::preview_memory_footprint(editor.planetarySettings,64ull*1024u*1024u);
                            if(footprint.cpu_upper_bytes)
                                gui::wrapped_label("Estimated RAM allowance: "+std::to_string(footprint.cpu_upper_bytes/(1024u*1024u))
                                    +" MiB. Includes cached terrain and mesh replacement; this is an estimate, not measured usage. Renderer memory is extra.",centerWidth);
                        }
                        // Act on this frame's completed form, including edits
                        // that make a previously invalid combination valid.
                        const bool canApplyTerrain = project_planetary::valid_settings(editor.planetarySettings)
                            && readPlanetSeed().has_value() && readPlanetRadius().has_value() && !planetBusy;
                        const std::array planetActions{
                            gui::InlineButtonSpec{.label="Build & Generate", .width=0.0f,
                                .enabled=canApplyTerrain && !editor.projectBuildPending && !editor.scriptBuildPending},
                            gui::InlineButtonSpec{.label="Save Settings", .width=0.0f, .enabled=canApplyTerrain},
                            gui::InlineButtonSpec{.label="Cancel", .width=0.0f, .enabled=planetBusy}};
                        const auto planetAction = gui::inline_button_row(
                            planetActions, 30.0f, 6.0f, planetButtons);
                        if (planetAction)
                        {
                            if (*planetAction == 2u)
                                cancel_planetary_project_task(editor);
                            else if (const auto seed = readPlanetSeed(); !seed)
                                editor.planetaryStatus = "Enter a whole-number seed from 0 to 4294967295.";
                            else if (!readPlanetRadius())
                                editor.planetaryStatus = "Enter a valid radius before building or saving. The previous size is retained.";
                            else if (!project_planetary::valid_settings(editor.planetarySettings))
                                editor.planetaryStatus = "Settings exceed the current mesh budget. No build or save was started.";
                            else if (!planetBusy)
                            {
                                editor.planetarySettings.seed = *seed;
                                if (*planetAction == 0u)
                                {
                                    if (!editor.projectBuildPending && !editor.scriptBuildPending)
                                        start_planetary_project_generation(editor);
                                }
                                else
                                {
                                    request_planetary_feature_action(editor, false);
                                }
                            }
                        }
                    }
                    else if (planetBusy)
                    {
                        if (gui::inline_button_row(std::array{gui::InlineButtonSpec{
                                .label="Cancel Package Operation", .width=0.0f}}, 30.0f, 6.0f, planetButtons))
                            cancel_planetary_project_task(editor);
                    }
                    else if (gui::inline_button_row(std::array{gui::InlineButtonSpec{
                            .label="Add Planetary Terrain...", .width=0.0f, .enabled=!planetBusy}},
                            30.0f, 6.0f, planetButtons))
                    {
                        editor.showPackageManagerModal = true;
                        open_package_source_review(editor, "research_planetary_terrain");
                    }
                    if (!planetAttached)
                        gui::wrapped_label(editor.planetaryStatus, centerWidth);
                    if (!editor.planetaryBuildLog.empty())
                    {
                        gui::wrapped_label(epochengine::format_text("Build {:.1f}s / Generate {:.1f}s",
                            editor.planetaryBuildSeconds, editor.planetaryGenerationSeconds), centerWidth);
                        gui::wrapped_label("Build log: " + editor.planetaryBuildLog.generic_string(), centerWidth);
                        if (gui::inline_button_row(std::array{gui::InlineButtonSpec{
                                .label=editor.planetaryLogLines.empty() ? "View Build Log" : "Hide Build Log",
                                .width=0.0f, .enabled=!planetBusy}}, 30.0f, 6.0f, planetButtons))
                        {
                            if (!editor.planetaryLogLines.empty()) editor.planetaryLogLines.clear();
                            else
                            {
                                std::istringstream log{read_text_tail(editor.planetaryBuildLog, 16'000u)};
                                for (std::string line; std::getline(log, line); )
                                    editor.planetaryLogLines.push_back(std::move(line));
                            }
                        }
                        if (!editor.planetaryLogLines.empty())
                            (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                                .id="planet-build-log", .size={centerWidth,180.0f},
                                .lines=editor.planetaryLogLines, .max_line_chars=4096u,
                                .selectable=true, .stick_to_bottom=false, .wrap_lines=true});
                    }
                    gui::section_heading("Build & Run");
                const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot, editor.projectBuildConfiguration);
                const std::filesystem::path buildLog = project_build_transcript_for_display(editor);
                gui::property_row("Project", editor.projectName, 118.0f);
                gui::property_row("Kind", editor.projectKind);
                gui::property_row("Root", display_project_path(editor.projectRoot), 108.0f);
                gui::property_row("Manifest", file_ready_summary(editor.projectManifest), 108.0f);
                gui::property_row("Scene", file_ready_summary(editor.projectScenePath), 108.0f);
                gui::property_row("Output", file_ready_summary(outputExe.string()), 108.0f);
                gui::property_row("Build log", file_ready_summary(buildLog.string()), 108.0f);
                gui::wrapped_label(
                    "Save, build and run this project. Build output and errors appear in Output.",
                    centerWidth);
                const auto runChoices = project_run_backend_choices();
                std::vector<std::string_view> runChoiceLabels;
                runChoiceLabels.reserve(runChoices.size());
                for (const auto& choice : runChoices)
                    runChoiceLabels.emplace_back(choice.label);
                const auto runBackendSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-run-backend-select",
                    .placeholder = "Choose project runtime backend",
                    .selected = project_run_backend_label(editor.projectRunBackend),
                    .options = std::span<const std::string_view>{ runChoiceLabels.data(), runChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 360.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 6
                });
                if (runBackendSelect.changed && runBackendSelect.selected_index && *runBackendSelect.selected_index < runChoices.size())
                {
                    editor.projectRunBackend = std::string(runChoices[*runBackendSelect.selected_index].argument);
                    persist_project_run_settings(editor);
                    push_editor_log(editor, std::string("[project] Project Run backend set to ") + std::string(runChoices[*runBackendSelect.selected_index].label) + ".");
                }
                gui::property_row("Run mode", std::string(project_run_backend_label(editor.projectRunBackend)), 108.0f);
                gui::property_row("Capability policy", projectProfile.renderer_capability.id, 148.0f);
                gui::property_row(
                    "Renderer evidence",
                    renderer_admission_summary(
                        renderer_backend_kind(editor.projectRunBackend),
                        projectProfile.renderer_capability),
                    148.0f);
                if (frame_limit_control("project-run-frame-limit-select", editor.projectRunFrameLimitFps,
                        editor.projectFrameLimitEdit, centerWidth))
                {
                    persist_project_run_settings(editor);
                    push_editor_log(editor, "[project] Project Run frame limit set to "
                        + frame_limit_argument(editor.projectRunFrameLimitFps) + " FPS.");
                }
                gui::property_row("Frame limit", std::string(frame_limit_label(editor.projectRunFrameLimitFps)), 108.0f);
                const bool activeProjectIsEngineSandbox = editor.projectId == "sandbox";
                if (!activeProjectIsEngineSandbox)
                {
                    const auto cameraChoices = project_camera_choices();
                    std::vector<std::string_view> cameraChoiceLabels;
                    cameraChoiceLabels.reserve(cameraChoices.size());
                    for (const auto& choice : cameraChoices)
                        cameraChoiceLabels.emplace_back(choice.label);
                    const auto cameraSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-camera-style-select",
                        .placeholder = "Choose project camera style",
                        .selected = project_camera_label(editor.projectCameraMode),
                        .options = std::span<const std::string_view>{ cameraChoiceLabels.data(), cameraChoiceLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 3
                    });
                    if (cameraSelect.changed && cameraSelect.selected_index && *cameraSelect.selected_index < cameraChoices.size())
                    {
                        editor.projectCameraMode = cameraChoices[*cameraSelect.selected_index].mode;
                        if (editor.projectCameraMode == previewgrid::CameraMode::Canvas2D)
                        {
                            ensure_2d_canvas_entity(editor);
                            previewgrid::reset_camera(ctx.get());
                        }
                        push_editor_log(editor, std::string("[project] Project camera style set to ") + std::string(cameraChoices[*cameraSelect.selected_index].label) + ".");
                    }
                    gui::property_row("Camera style", std::string(project_camera_label(editor.projectCameraMode)), 108.0f);
                }
                else
                {
                    gui::property_row("Camera style", "Engine engine development mirrors the editor workbench.", 108.0f);
                }
                const auto inputChoices = input_profile_choices();
                std::vector<std::string_view> inputChoiceLabels;
                inputChoiceLabels.reserve(inputChoices.size());
                for (const auto& choice : inputChoices)
                    inputChoiceLabels.emplace_back(choice.label);
                const auto inputSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-input-profile-select",
                    .placeholder = "Choose shared input profile",
                    .selected = input_profile_label(editor.inputProfilePreset),
                    .options = std::span<const std::string_view>{ inputChoiceLabels.data(), inputChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 4
                });
                if (inputSelect.changed && inputSelect.selected_index && *inputSelect.selected_index < inputChoices.size())
                {
                    editor.inputProfilePreset = inputChoices[*inputSelect.selected_index].preset;
                    input::set_active_profile(editor.inputProfilePreset);
                    push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
                }
                gui::property_row("Camera input", std::string(input_profile_label(editor.inputProfilePreset)), 108.0f);
                gui::label("New Project Self-Iteration");
                gui::wrapped_label(
                    "Creation default only. Existing projects are never changed. Enabled kits may propose project-source edits only, require explicit approval, and never auto-connect, listen, bind, serve, or launch a model.",
                    centerWidth);
                }

                if (editor.projectWorkspaceSection >= 1u
                    && editor.projectWorkspaceSection <= 3u)
                {
                    editor.canvas2dProject = sanitize_canvas2d_project(editor.canvas2dProject);
                }

                if (editor.projectWorkspaceSection == 1u)
                {
                gui::section_heading("Input");
                    gui::wrapped_label(
                        "These bindings belong to the active project. Edits stay in a validated draft until Apply publishes the canonical source and compiled Library artifact.",
                        centerWidth);
                    const bool inputLoaded = load_project_input_settings(editor);
                    const auto projectInput = inputLoaded
                        ? editor.projectInputSettings->snapshot()
                        : editor_project_input_settings::SettingsSnapshot{};
                    gui::property_row(
                        "Source",
                        inputLoaded
                            ? std::string{project_input::canonical_source_path}
                            : std::string{"Not loaded"},
                        132.0f);
                    gui::property_row(
                        "Profile",
                        projectInput.display_name.empty()
                            ? std::string{"Not declared"}
                            : projectInput.display_name,
                        132.0f);
                    gui::property_row(
                        "Actions",
                        epochengine::format_text(
                            "{} actions | {} bindings",
                            projectInput.action_count,
                            projectInput.binding_count),
                        132.0f);
                    gui::property_row(
                        "Draft",
                        inputLoaded
                            ? epochengine::format_text(
                                "{} | applied r{} | working r{}",
                                projectInput.dirty ? "Modified" : "Clean",
                                projectInput.applied_revision.sequence,
                                projectInput.working_revision.sequence)
                            : std::string{"Unavailable"},
                        132.0f);
                    gui::property_row(
                        "Validation",
                        editor.projectInputSettingsStatus,
                        132.0f);
                    if (!projectInput.evidence.conflicts.empty())
                    {
                        for (const auto& conflict : projectInput.evidence.conflicts)
                        {
                            gui::wrapped_label(
                                epochengine::format_text(
                                    "Conflict: binding {} and binding {} resolve to the same physical input.",
                                    conflict.binding.value,
                                    conflict.conflicting_binding.value),
                                centerWidth);
                        }
                    }
                    if (inputLoaded)
                    {
                        const auto physicalControllerSnapshot =
                            controller_input::snapshot();
                        gui::property_row(
                            "Keyboard",
                            "Canonical project bindings sampled live",
                            132.0f);
                        gui::property_row(
                            "Controller",
                            physicalControllerSnapshot.provider_available
                                ? epochengine::format_text(
                                    "{} | {} connected | snapshot {}",
                                    controller_input::physical_provider_name(),
                                    controller_input::connected_count(),
                                    physicalControllerSnapshot.revision)
                                : epochengine::format_text(
                                    "{} unavailable",
                                    controller_input::physical_provider_name()),
                            132.0f);
                        gui::property_row(
                            "Dead zone",
                            project_controller_dead_zone_evidence(
                                projectInput.controller_dead_zone_q15,
                                projectInput.uniform_controller_dead_zone),
                            132.0f);

                gui::section_heading("Input bindings");
                        const auto recordInputEdit =
                            [&](const editor_project_input_settings::SettingsEvidence& evidence)
                            {
                                editor.projectInputSettingsStatus =
                                    project_input_settings_evidence(evidence);
                                push_editor_log(
                                    editor,
                                    std::string{evidence
                                        ? "[input] Draft updated: "
                                        : "[input][ERROR] Draft rejected: "}
                                        + editor.projectInputSettingsStatus);
                            };

                        const auto keyChoices = project_key_choices();
                        std::vector<std::string_view> keyChoiceLabels{};
                        keyChoiceLabels.reserve(keyChoices.size());
                        for (const auto key : keyChoices)
                        {
                            keyChoiceLabels.emplace_back(
                                epochengine::project_input::key_code_name(key));
                        }

                        const auto buttonChoices =
                            project_controller_button_choices();
                        std::vector<std::string_view> buttonChoiceLabels{};
                        buttonChoiceLabels.reserve(buttonChoices.size());
                        for (const auto button : buttonChoices)
                        {
                            buttonChoiceLabels.emplace_back(
                                epochengine::project_input::
                                    controller_button_name(button));
                        }

                        const auto axisChoices =
                            project_controller_axis_choices();
                        std::vector<std::string_view> axisChoiceLabels{};
                        axisChoiceLabels.reserve(axisChoices.size());
                        for (const auto axis : axisChoices)
                        {
                            axisChoiceLabels.emplace_back(
                                epochengine::project_input::
                                    controller_axis_name(axis));
                        }
                        const auto slotLabels =
                            project_controller_slot_labels();

                        const bool hasControllerAxis = std::any_of(
                            projectInput.bindings.begin(),
                            projectInput.bindings.end(),
                            [](const editor_project_input_settings::BindingView& binding)
                            {
                                return binding.device
                                    == project_input::BindingDevice::controller_axis;
                            });
                        if (hasControllerAxis)
                        {
                            const auto deadZoneChoices =
                                project_controller_dead_zone_choices();
                            std::vector<std::string_view> deadZoneLabels{};
                            deadZoneLabels.reserve(deadZoneChoices.size());
                            for (const auto& choice : deadZoneChoices)
                                deadZoneLabels.emplace_back(choice.label);
                            const auto deadZoneSelect = gui::select_box(
                                gui::SelectBoxOptions{
                                    .id = "project-input-controller-dead-zone",
                                    .placeholder = "Controller dead zone",
                                    .selected =
                                        project_controller_dead_zone_label(
                                            projectInput
                                                .controller_dead_zone_q15,
                                            projectInput
                                                .uniform_controller_dead_zone),
                                    .options =
                                        std::span<const std::string_view>{
                                            deadZoneLabels.data(),
                                            deadZoneLabels.size()},
                                    .size = {
                                        (std::min)(centerWidth, 320.0f),
                                        30.0f},
                                    .row_height = 27.0f,
                                    .max_visible_options = 8});
                            if (deadZoneSelect.changed
                                && deadZoneSelect.selected_index
                                && *deadZoneSelect.selected_index
                                    < deadZoneChoices.size())
                            {
                                recordInputEdit(
                                    editor.projectInputSettings
                                        ->set_controller_dead_zone(
                                            deadZoneChoices[
                                                *deadZoneSelect.selected_index]
                                                .value_q15));
                            }
                        }

                        for (const auto& binding : projectInput.bindings)
                        {
                            gui::label(binding.action_label);
                            gui::property_row(
                                "Device",
                                binding.source_label,
                                132.0f);
                            switch (binding.device)
                            {
                            case project_input::BindingDevice::keyboard:
                            {
                                const auto bindingSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id = "project-input-key-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose project key",
                                        .selected =
                                            project_key_label(binding.code),
                                        .options =
                                            std::span<const std::string_view>{
                                                keyChoiceLabels.data(),
                                                keyChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 8});
                                if (bindingSelect.changed
                                    && bindingSelect.selected_index
                                    && *bindingSelect.selected_index
                                        < keyChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_keyboard(
                                                binding.id,
                                                keyChoices[
                                                    *bindingSelect
                                                        .selected_index]));
                                }
                                break;
                            }
                            case project_input::BindingDevice::controller_button:
                            {
                                const auto sourceSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-button-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder =
                                            "Choose controller button",
                                        .selected =
                                            epochengine::project_input::
                                                controller_button_name(
                                                    static_cast<
                                                        epochengine::
                                                            project_input::
                                                                ControllerButton>(
                                                        binding.code)),
                                        .options =
                                            std::span<const std::string_view>{
                                                buttonChoiceLabels.data(),
                                                buttonChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 8});
                                if (sourceSelect.changed
                                    && sourceSelect.selected_index
                                    && *sourceSelect.selected_index
                                        < buttonChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_button(
                                                binding.id,
                                                buttonChoices[
                                                    *sourceSelect
                                                        .selected_index],
                                                binding.controller_slot));
                                }
                                const auto slotSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-button-slot-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose controller",
                                        .selected =
                                            project_controller_slot_label(
                                                binding.controller_slot),
                                        .options = slotLabels,
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 4});
                                if (slotSelect.changed
                                    && slotSelect.selected_index
                                    && *slotSelect.selected_index
                                        < slotLabels.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_button(
                                                binding.id,
                                                static_cast<project_input::ControllerButton>(
                                                    binding.code),
                                                static_cast<std::uint8_t>(
                                                    *slotSelect.selected_index)));
                                }
                                break;
                            }
                            case project_input::BindingDevice::controller_axis:
                            {
                                const auto sourceSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-axis-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder =
                                            "Choose controller axis",
                                        .selected =
                                            epochengine::project_input::
                                                controller_axis_name(
                                                    static_cast<
                                                        epochengine::
                                                            project_input::
                                                                ControllerAxis>(
                                                        binding.code)),
                                        .options =
                                            std::span<const std::string_view>{
                                                axisChoiceLabels.data(),
                                                axisChoiceLabels.size()},
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 6});
                                if (sourceSelect.changed
                                    && sourceSelect.selected_index
                                    && *sourceSelect.selected_index
                                        < axisChoices.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_axis(
                                                binding.id,
                                                axisChoices[
                                                    *sourceSelect
                                                        .selected_index],
                                                binding.controller_slot));
                                }
                                const auto slotSelect = gui::select_box(
                                    gui::SelectBoxOptions{
                                        .id =
                                            "project-input-controller-axis-slot-"
                                            + std::to_string(
                                                binding.id.value),
                                        .placeholder = "Choose controller",
                                        .selected =
                                            project_controller_slot_label(
                                                binding.controller_slot),
                                        .options = slotLabels,
                                        .size = {
                                            (std::min)(centerWidth, 320.0f),
                                            30.0f},
                                        .row_height = 27.0f,
                                        .max_visible_options = 4});
                                if (slotSelect.changed
                                    && slotSelect.selected_index
                                    && *slotSelect.selected_index
                                        < slotLabels.size())
                                {
                                    recordInputEdit(
                                        editor.projectInputSettings
                                            ->rebind_controller_axis(
                                                binding.id,
                                                static_cast<project_input::ControllerAxis>(
                                                    binding.code),
                                                static_cast<std::uint8_t>(
                                                    *slotSelect.selected_index)));
                                }
                                break;
                            }
                            }
                        }

                        const auto actionSnapshot =
                            editor.projectInputSettings->snapshot();
                        const std::array inputDraftActions{
                            gui::InlineButtonSpec{
                                .label = "Apply Changes",
                                .width = 132.0f,
                                .enabled = actionSnapshot.dirty
                                    && actionSnapshot.evidence.conflicts.empty()},
                            gui::InlineButtonSpec{
                                .label = "Discard Draft",
                                .width = 124.0f,
                                .enabled = actionSnapshot.dirty},
                            gui::InlineButtonSpec{
                                .label = "Stage Defaults",
                                .width = 128.0f},
                            gui::InlineButtonSpec{
                                .label = "Reload Disk",
                                .width = 112.0f,
                                .enabled = !actionSnapshot.dirty}};
                        if (const auto action = gui::inline_button_row(
                                inputDraftActions, 30.0f, 6.0f))
                        {
                            if (*action == 0u)
                            {
                                const auto applied =
                                    apply_project_input_settings(editor);
                                push_editor_log(
                                    editor,
                                    std::string{applied.succeeded
                                        ? "[input] " : "[input][ERROR] "}
                                        + applied.summary);
                            }
                            else if (*action == 1u)
                            {
                                recordInputEdit(
                                    editor.projectInputSettings->discard());
                            }
                            else if (*action == 2u)
                            {
                                recordInputEdit(
                                    editor.projectInputSettings
                                        ->reset_defaults());
                            }
                            else
                            {
                                (void)load_project_input_settings(editor, true);
                                push_editor_log(
                                    editor,
                                    "[input] Reloaded project input from disk.");
                            }
                        }
                    }
                    else if (gui::button(
                        "Retry Input Load",
                        {(std::min)(centerWidth, 220.0f), 30.0f}))
                    {
                        (void)load_project_input_settings(editor, true);
                    }
                }

                if (editor.projectWorkspaceSection == 2u)
                {
                gui::section_heading("Audio");
                    const auto projectAudio =
                        editor_project_audio_profile_summary(
                            editor.projectId);
                    gui::label("Project Audio");
                    gui::property_row(
                        "[audio] Cues",
                        epochengine::format_text(
                            "{} cues | {} buses",
                            projectAudio.cue_count,
                            projectAudio.bus_count),
                        132.0f);
                    gui::property_row(
                        "[audio] Source",
                        projectAudio.source_materialized
                            ? std::string{"Saved"}
                            : std::string{"Default draft"},
                        132.0f);
                    gui::property_row(
                        "[audio] Status",
                        projectAudio.diagnostic,
                        132.0f);
                    if (gui::button(
                            "Open Project Audio",
                            {(std::min)(centerWidth, 220.0f), 30.0f}))
                    {
                        editor.assetWorkspaceSection = 4u;
                        editor.outlinerToolTab = OutlinerToolTab::Assets;
                        open_editor_surface(
                            EditorMainSurface::Assets,
                            "Project audio controls");
                    }
                }

                if (editor.projectWorkspaceSection == 3u)
                {
                    gui::section_heading("2D presentation");
                    gui::wrapped_label(
                        "Logical presentation, sampling, density, batching, chunking, and texture budgets for this project.",
                        centerWidth);
                    gui::property_row(
                        "[texture] Policy",
                        projectProfile.renderer_capability.allow_experimental
                            ? "Experimental providers allowed"
                            : "Validated providers only",
                        132.0f);
                    gui::property_row(
                        "[texture] Upload budget",
                        epochengine::format_text(
                            "{} MiB/frame",
                            renderer_capability_profile(ctx).recommended_budgets.upload_budget_bytes
                                / (1024u * 1024u)),
                        132.0f);
                    gui::property_row(
                        "[texture] Residency budget",
                        epochengine::format_text(
                            "{} MiB",
                            renderer_capability_profile(ctx).recommended_budgets.vram_budget_bytes
                                / (1024u * 1024u)),
                        132.0f);
                    if (auto* controller = project_texture_controller(editor))
                    {
                        const auto textureMetrics = controller->metrics();
                        gui::property_row(
                            "[texture] Library",
                            epochengine::format_text(
                                "{} textures | {} KiB decoded",
                                textureMetrics.texture_count,
                                textureMetrics.decoded_bytes / 1024u
                                    + (textureMetrics.decoded_bytes % 1024u != 0u
                                        ? 1u
                                        : 0u)),
                            132.0f);
                    }

                    const auto resolutionChoices = canvas2d_resolution_choices();
                    std::vector<std::string_view> resolutionLabels;
                    resolutionLabels.reserve(resolutionChoices.size());
                    for (const auto& choice : resolutionChoices)
                        resolutionLabels.emplace_back(choice.label);
                    const auto resolutionSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-resolution-select",
                        .placeholder = "Logical canvas resolution",
                        .selected = canvas2d_resolution_label(editor.canvas2dProject),
                        .options = std::span<const std::string_view>{ resolutionLabels.data(), resolutionLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (resolutionSelect.changed && resolutionSelect.selected_index
                        && *resolutionSelect.selected_index < resolutionChoices.size())
                    {
                        const auto& choice = resolutionChoices[*resolutionSelect.selected_index];
                        editor.canvas2dProject.logical_width = choice.width;
                        editor.canvas2dProject.logical_height = choice.height;
                        push_editor_log(editor, std::string("[canvas2d] Logical extent set to ") + std::string(choice.label) + ".");
                    }
                    gui::property_row("[canvas2d] Logical extent", std::string(canvas2d_resolution_label(editor.canvas2dProject)), 132.0f);

                    static constexpr std::array<std::string_view, 4> scaleLabels{
                        "Integer fit", "Fractional fit", "Fill and crop", "Stretch"
                    };
                    const auto scaleSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-scale-select",
                        .placeholder = "Presentation scaling",
                        .selected = canvas2d_scale_policy_label(editor.canvas2dProject.scale_policy),
                        .options = std::span<const std::string_view>{ scaleLabels.data(), scaleLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (scaleSelect.changed && scaleSelect.selected_index)
                    {
                        editor.canvas2dProject.scale_policy = static_cast<std::uint8_t>(*scaleSelect.selected_index);
                        push_editor_log(editor, std::string("[canvas2d] Presentation scale set to ")
                            + std::string(canvas2d_scale_policy_label(editor.canvas2dProject.scale_policy)) + ".");
                    }

                    static constexpr std::array<std::string_view, 2> samplingLabels{ "Nearest", "Linear" };
                    const auto samplingSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-sampling-select",
                        .placeholder = "Canvas sampling",
                        .selected = canvas2d_sampling_policy_label(editor.canvas2dProject.sampling_policy),
                        .options = std::span<const std::string_view>{ samplingLabels.data(), samplingLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 2
                    });
                    if (samplingSelect.changed && samplingSelect.selected_index)
                    {
                        editor.canvas2dProject.sampling_policy = static_cast<std::uint8_t>(*samplingSelect.selected_index);
                        push_editor_log(editor, std::string("[canvas2d] Sampling set to ")
                            + std::string(canvas2d_sampling_policy_label(editor.canvas2dProject.sampling_policy)) + ".");
                    }

                    static constexpr std::array<Canvas2DBudgetChoice, 5> pixelsPerUnitChoices{ {
                        { "16 pixels per world unit", 16u },
                        { "32 pixels per world unit", 32u },
                        { "64 pixels per world unit", 64u },
                        { "100 pixels per world unit", 100u },
                        { "128 pixels per world unit", 128u }
                    } };
                    std::array<std::string_view, pixelsPerUnitChoices.size()> pixelsPerUnitLabels{};
                    std::string_view selectedPixelsPerUnit = "Custom pixel density";
                    for (std::size_t index = 0; index < pixelsPerUnitChoices.size(); ++index)
                    {
                        pixelsPerUnitLabels[index] = pixelsPerUnitChoices[index].label;
                        if (editor.canvas2dProject.pixels_per_world_unit == static_cast<float>(pixelsPerUnitChoices[index].value))
                            selectedPixelsPerUnit = pixelsPerUnitChoices[index].label;
                    }
                    const auto pixelsPerUnitSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-pixels-per-unit-select",
                        .placeholder = "Pixel density",
                        .selected = selectedPixelsPerUnit,
                        .options = std::span<const std::string_view>{ pixelsPerUnitLabels.data(), pixelsPerUnitLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 5
                    });
                    if (pixelsPerUnitSelect.changed && pixelsPerUnitSelect.selected_index)
                        editor.canvas2dProject.pixels_per_world_unit = static_cast<float>(pixelsPerUnitChoices[*pixelsPerUnitSelect.selected_index].value);

                    const auto spriteBudgetChoices = canvas2d_sprite_budget_choices();
                    std::vector<std::string_view> spriteBudgetLabels;
                    spriteBudgetLabels.reserve(spriteBudgetChoices.size());
                    for (const auto& choice : spriteBudgetChoices)
                        spriteBudgetLabels.emplace_back(choice.label);
                    const auto spriteBudgetSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-sprite-budget-select",
                        .placeholder = "Sprite batch budget",
                        .selected = canvas2d_sprite_budget_label(editor.canvas2dProject.maximum_sprites_per_batch),
                        .options = std::span<const std::string_view>{ spriteBudgetLabels.data(), spriteBudgetLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (spriteBudgetSelect.changed && spriteBudgetSelect.selected_index
                        && *spriteBudgetSelect.selected_index < spriteBudgetChoices.size())
                    {
                        editor.canvas2dProject.maximum_sprites_per_batch = spriteBudgetChoices[*spriteBudgetSelect.selected_index].value;
                        push_editor_log(editor, std::string("[canvas2d] ")
                            + std::string(spriteBudgetChoices[*spriteBudgetSelect.selected_index].label) + ".");
                    }

                    static constexpr std::array<Canvas2DBudgetChoice, 4> batchCapChoices{ {
                        { "64 logical batches", 64u },
                        { "256 logical batches", 256u },
                        { "1024 logical batches", 1024u },
                        { "4096 logical batches", 4096u }
                    } };
                    std::array<std::string_view, batchCapChoices.size()> batchCapLabels{};
                    std::string_view selectedBatchCap = "Custom logical batch cap";
                    for (std::size_t index = 0; index < batchCapChoices.size(); ++index)
                    {
                        batchCapLabels[index] = batchCapChoices[index].label;
                        if (editor.canvas2dProject.maximum_batches == batchCapChoices[index].value)
                            selectedBatchCap = batchCapChoices[index].label;
                    }
                    const auto batchCapSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-batch-cap-select",
                        .placeholder = "Logical batch cap",
                        .selected = selectedBatchCap,
                        .options = std::span<const std::string_view>{ batchCapLabels.data(), batchCapLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 4
                    });
                    if (batchCapSelect.changed && batchCapSelect.selected_index)
                        editor.canvas2dProject.maximum_batches = batchCapChoices[*batchCapSelect.selected_index].value;

                    const auto chunkChoices = canvas2d_chunk_extent_choices();
                    std::vector<std::string_view> chunkLabels;
                    chunkLabels.reserve(chunkChoices.size());
                    for (const auto& choice : chunkChoices)
                        chunkLabels.emplace_back(choice.label);
                    const auto chunkSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-canvas2d-chunk-select",
                        .placeholder = "Tile chunk extent",
                        .selected = canvas2d_chunk_extent_label(editor.canvas2dProject.tile_chunk_extent),
                        .options = std::span<const std::string_view>{ chunkLabels.data(), chunkLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 5
                    });
                    if (chunkSelect.changed && chunkSelect.selected_index && *chunkSelect.selected_index < chunkChoices.size())
                    {
                        editor.canvas2dProject.tile_chunk_extent = chunkChoices[*chunkSelect.selected_index].value;
                        push_editor_log(editor, std::string("[canvas2d] Tile chunk set to ")
                            + std::string(chunkChoices[*chunkSelect.selected_index].label) + ".");
                    }

                    if (gui::button(editor.canvas2dProject.pixel_snapping ? "Pixel Snap: On" : "Pixel Snap: Off", { 180.0f, 30.0f }))
                    {
                        editor.canvas2dProject.pixel_snapping = !editor.canvas2dProject.pixel_snapping;
                        push_editor_log(editor, editor.canvas2dProject.pixel_snapping
                            ? "[canvas2d] Pixel snapping enabled."
                            : "[canvas2d] Pixel snapping disabled.");
                    }
                }
                if (editor.projectWorkspaceSection == 4u)
                {
                    gui::label("Run + Build");
                    constexpr std::array<std::string_view, 2> buildConfigurations{"Debug", "Release"};
                    gui::label("Build configuration");
                    const auto configurationSelect = gui::select_box(gui::SelectBoxOptions{
                        .id="project-build-configuration",
                        .placeholder="Choose Debug or Release",
                        .selected=editor.projectBuildConfiguration,
                        .options=buildConfigurations,
                        .size={(std::min)(centerWidth, 260.0f), 30.0f},
                        .fit_content=true, .alignment=gui::InlineButtonAlignment::Center,
                        .enabled=!editor.projectBuildPending && !editor.scriptBuildPending
                            && !editor.aiContinuousBuildPending && !planetary_project_busy(editor)
                            && !editor.projectLifecycle.evidence().runtime_active
                            && !(editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active())});
                    if (configurationSelect.changed && configurationSelect.selected_index
                        && *configurationSelect.selected_index < buildConfigurations.size())
                        (void)select_project_build_configuration(editor,
                            buildConfigurations[*configurationSelect.selected_index]);
                    gui::wrapped_label(
                        "Save the active project, play the composed runtime in this editor, build a new child artifact generation, or launch the accepted standalone executable.",
                        centerWidth);
                if (gui::button("Save Active Project", { 220.0f, 30.0f }))
                    repair_active_project_evidence(editor);
                if (application.allow_project_run)
                {
                    gui::wrapped_label(
                        "Play In Editor uses the saved scene. Build verifies compilation; runtime and renderer behavior are tested separately. Launch opens the accepted standalone program.",
                        centerWidth);
                    if (editor.projectBuildPending)
                    {
                        const std::array busyPlay{gui::InlineButtonSpec{
                            .label=projectBuildActivity, .width=220.0f, .enabled=false}};
                        (void)gui::inline_button_row(busyPlay, 30.0f, 0.0f,
                            gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent,
                                .wrap=true, .available_width=centerWidth});
                        gui::wrapped_label(editor.projectBuildStatus, centerWidth);
                    }
                    else if (gui::button("Play In Editor", {220.0f, 30.0f}))
                        play_active_context();

                    // Keep both controls in fixed slots as a build settles. A
                    // delayed Cancel click must never land on a new Build.
                    const bool projectBuildWasPending =
                        static_cast<bool>(editor.projectBuildPending);
                    const std::array projectBuildActions{
                        gui::InlineButtonSpec{
                            .label="Build Active Project", .width=220.0f,
                            .enabled=!projectBuildWasPending},
                        gui::InlineButtonSpec{
                            .label="Cancel Project Build", .width=220.0f,
                            .enabled=projectBuildWasPending}};
                    if (const auto action = gui::inline_button_row(
                            projectBuildActions, 30.0f, 6.0f,
                            gui::InlineButtonRowOptions{
                                .sizing=gui::InlineButtonSizing::FitContent,
                                .alignment=gui::InlineButtonAlignment::Center,
                                .wrap=true, .available_width=centerWidth}))
                    {
                        if (*action == 0u && !projectBuildWasPending)
                            start_project_build(
                                editor, false, "Project surface");
                        else if (*action == 1u && projectBuildWasPending)
                            (void)cancel_project_build(editor);
                    }

                    const bool runtimeActive =
                        editor.projectRuntimeSnapshot
                        && editor.projectRuntimeSnapshot->active();
                    if (runtimeActive)
                    {
                        if (gui::button(
                                "Focus Project Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)focus_project_runtime(editor);
                        }
                        if (gui::button(
                                "Stop Project Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)stop_project_runtime(
                                editor,
                                platform::child_process::StopMode::graceful);
                        }
                        if (editor.projectRuntimeForceStopAvailable
                            && gui::button(
                                "Force Stop Runtime",
                                {220.0f, 30.0f}))
                        {
                            (void)stop_project_runtime(
                                editor,
                                platform::child_process::StopMode::force);
                        }
                    }
                    else if (!editor.projectBuildPending && gui::button(
                        "Launch Single Context", {220.0f, 30.0f}))
                    {
                        launch_active_project_context();
                    }
                    gui::wrapped_label(
                        editor.projectRuntimeStatus, centerWidth);
                }
                else
                {
                    gui::property_row("Runtime", "Not owned by this dedicated editor", 108.0f);
                }
                gui::wrapped_label(editor.projectStatus, centerWidth);
                gui::wrapped_label(editor.projectBuildStatus, centerWidth);
                if (!editor.projectBuildDiagnosticTail.empty())
                {
                    gui::label("Build Diagnostics");
                    gui::wrapped_label(
                        editor.projectBuildDiagnosticTail, centerWidth);
                }
                }
                break;
            }
            case EditorMainSurface::Assets:
            {
                gui::section_heading("Assets");
                const std::array workspaceTabs{
                    gui::TabButtonSpec{
                        .id = "assets.workspace.browser",
                        .label = "Browser",
                        .width = 88.0f,
                        .active = editor.assetWorkspaceSection == 0u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.textures",
                        .label = "Textures",
                        .width = 92.0f,
                        .active = editor.assetWorkspaceSection == 1u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.scripts",
                        .label = "Scripts",
                        .width = 80.0f,
                        .active = editor.assetWorkspaceSection == 2u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.graph",
                        .label = "Graph",
                        .width = 72.0f,
                        .active = editor.assetWorkspaceSection == 3u},
                    gui::TabButtonSpec{
                        .id = "assets.workspace.audio",
                        .label = "Audio",
                        .width = 72.0f,
                        .active = editor.assetWorkspaceSection == 4u}
                };
                if (const auto tabs = gui::responsive_tab_bar_buttons(
                        {.overflow_id="assets-workspace-tools", .overflow_label="Asset tools",
                            .tabs=workspaceTabs, .available_width=centerWidth,
                            .height=31.0f, .gap=2.0f});
                    tabs.selected_index)
                {
                    editor.assetWorkspaceSection = *tabs.selected_index;
                }

                if (editor.assetWorkspaceSection == 0u)
                {
                    gui::section_heading("Files");
                gui::property_row("Project", editor.projectName, 120.0f);
                gui::property_row(
                    "Folder",
                    editor.assetBrowserFolder.empty()
                        ? std::string{"Project"}
                        : editor.assetBrowserFolder,
                    120.0f);
                if (!editor.assetBrowserFolder.empty()
                    && gui::button("Up", {54.0f, 28.0f}))
                {
                    leave_asset_browser_folder(editor);
                }
                const gui::InlineButtonRowOptions meshImportLayout{
                    .sizing=gui::InlineButtonSizing::FitContent,
                    .alignment=gui::InlineButtonAlignment::Center,
                    .wrap=true, .available_width=centerWidth};
                if (!editor.meshImportOpen)
                {
                    if (const auto action = gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{
                            .label="Add Image...", .width=0.0f,
                            .enabled=!editor.projectRoot.empty()},
                        gui::InlineButtonSpec{
                            .label="Import Model / Mesh...", .width=0.0f,
                            .enabled=!editor.projectRoot.empty()}}, 30.0f, 6.0f, meshImportLayout))
                    {
                        if (*action == 0u) browse_project_image_source(editor, ctx);
                        else editor.meshImportOpen = true;
                    }
                }
                else
                {
                    gui::label("Model or mesh file (.glb / .epoch_mesh)");
                    (void)gui::edit_box(editor.meshImportPathDraft, {centerWidth, 30.0f}, 32768u, false);
                    if (editor.modelImportReview && !model_import_review_current(editor))
                    {
                        editor.modelImportReview.reset();
                        editor.projectStatus = "Model review expired after a path or project change. Review the file again.";
                    }
                    if (editor.modelImportReview)
                    {
                        auto& review = *editor.modelImportReview;
                        const auto& compiled = review.compiled;
                        gui::wrapped_label(epochengine::format_text("Add {} model part(s) to {}. Units: metres. One Undo restores the scene.",
                            compiled.model.primitives.size(),review.projectId),centerWidth);
                        gui::wrapped_label("Reviewed file SHA-256: "+review.sourceSha256,centerWidth);
                        if (!review.texturePreviewAttempted)
                        {
                            review.texturePreviewAttempted = true;
                            const auto& mip = compiled.textures.front().artifact.mips.front();
                            review.texturePreview = gui::register_runtime_surface("model.import.review."+review.sourceSha256,
                                std::span{reinterpret_cast<const std::uint8_t*>(mip.texels.data()),mip.texels.size()},mip.width,mip.height);
                        }
                        if (review.texturePreview.is_valid())
                        {
                            const auto& mip = compiled.textures.front().artifact.mips.front();
                            (void)gui::image_box({.id="model.import.review.texture",.sprite=review.texturePreview,
                                .size={(std::min)(centerWidth,256.0f),160.0f},
                                .source_size={static_cast<float>(mip.width),static_cast<float>(mip.height)},
                                .caption="Base-color texture. 3D rendering is checked after import."});
                        }
                    }
                    const std::array meshActions{
                        gui::InlineButtonSpec{.label=editor.modelImportReview ? "Approve Import" : "Review / Import", .width=0.0f,
                            .enabled=!editor.projectRoot.empty() && !editor.meshImportPathDraft.empty()},
                        gui::InlineButtonSpec{.label="Browse...", .width=0.0f},
                        gui::InlineButtonSpec{.label="Cancel", .width=0.0f}};
                    if (const auto action = gui::inline_button_row(meshActions, 30.0f, 6.0f, meshImportLayout))
                    {
                        if (*action == 0u)
                        {
                            try
                            {
                                const std::filesystem::path source{std::u8string{editor.meshImportPathDraft.begin(), editor.meshImportPathDraft.end()}};
                                auto extension = source.extension().string();
                                std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                                bool imported{};
                                if (editor.modelImportReview) imported = approve_project_model_import(editor);
                                else if (extension == ".glb") (void)review_project_model(editor,source);
                                else imported = import_project_mesh(editor,source);
                                if (imported)
                                {
                                    editor.meshImportOpen = false;
                                    open_editor_surface(EditorMainSurface::Scene, "Import Mesh");
                                    handle_scene_tool(editor, ctx.get(), "focus_selection");
                                }
                            }
                            catch (...)
                            {
                                editor.projectStatus = "Could not read that mesh path. Check the path and try again.";
                                push_editor_log(editor, "[assets] " + editor.projectStatus);
                            }
                        }
                        else if (*action == 1u)
                        {
                            const auto chosen = platform::filesystem::open_file_dialog(
                                L"Review a model or import a mesh", L"GLB model or Epoch mesh", L"*.glb;*.epoch_mesh",
                                std::filesystem::path{editor.projectRoot});
                            if (chosen.accepted)
                            {
                                const auto utf8 = chosen.path.generic_u8string();
                                editor.meshImportPathDraft.assign(utf8.begin(), utf8.end());
                            }
                            else if (!chosen.cancelled) editor.projectStatus = chosen.diagnostic;
                        }
                        else { editor.modelImportReview.reset(); editor.meshImportOpen = false; }
                    }
                    gui::wrapped_label("GLB: Review first, then Approve Import. Existing Epoch meshes import directly. Save Project keeps placement and material references.", centerWidth);
                    gui::wrapped_label(editor.projectStatus, centerWidth);
                }
                draw_asset_category_tabs(editor, true);
                (void)gui::edit_box(
                    editor.assetSearch,
                    {centerWidth, 30.0f},
                    96u,
                    false);

                const auto assetEntries = collect_asset_manager_entries(
                    editor.projectRoot,
                    editor.assetBrowserFolder,
                    512u);
                auto* textureController = project_texture_controller(editor);
                const auto textureForEntry = [&](const EditorBrowserEntry& entry)
                    -> const editor_project_textures::TextureCatalogEntry*
                {
                    if (!textureController
                        || !project_texture_source_path(entry.path))
                        return nullptr;
                    return project_texture_for_asset_path(
                        *textureController,
                        editor.projectRoot,
                        entry.path);
                };
                const auto gridKind = [](const EditorBrowserEntry& entry) noexcept
                {
                    if (entry.directory)
                        return gui::AssetGridItemKind::Folder;
                    if (entry.kind == "IMAGE")
                        return gui::AssetGridItemKind::Image;
                    if (entry.kind == "TEXTURE")
                        return gui::AssetGridItemKind::Texture;
                    if (entry.kind == "MODEL")
                        return gui::AssetGridItemKind::Model;
                    if (entry.kind == "AUDIO")
                        return gui::AssetGridItemKind::Audio;
                    if (entry.kind == "MAP")
                        return gui::AssetGridItemKind::Scene;
                    if (entry.kind == "GUI" || entry.kind == "SCRIPT"
                        || entry.kind == "CODE" || entry.kind == "TEXT")
                    {
                        return gui::AssetGridItemKind::Document;
                    }
                    return gui::AssetGridItemKind::Generic;
                };
                struct AssetGridPresentation final
                {
                    std::size_t entry_index{};
                    std::uint64_t id{};
                    std::string label{};
                    std::string detail{};
                    std::string search_terms{};
                    SpriteHandle image{};
                    gui::Vec2 source_size{};
                    gui::AssetGridItemKind kind{
                        gui::AssetGridItemKind::Generic};
                    std::string logical_texture{};
                };
                std::vector<AssetGridPresentation> presentations{};
                presentations.reserve(assetEntries.size());
                for (std::size_t index = 0u;
                     index < assetEntries.size();
                     ++index)
                {
                    const EditorBrowserEntry& entry = assetEntries[index];
                    if (!asset_category_visible(editor.assetCategory, entry.kind))
                        continue;

                    AssetGridPresentation presentation{};
                    presentation.entry_index = index;
                    presentation.id = asset_graph_id(entry.path);
                    const auto filenameUtf8 = std::filesystem::path{
                        std::u8string(entry.path.begin(), entry.path.end())}.filename().generic_u8string();
                    presentation.label.assign(filenameUtf8.begin(), filenameUtf8.end());
                    presentation.detail = entry.directory
                        ? (entry.child_count_truncated
                            ? epochengine::format_text(
                                "{}+ items", entry.child_count)
                            : epochengine::format_text(
                                "{} item{}",
                                entry.child_count,
                                entry.child_count == 1u ? "" : "s"))
                        : epochengine::format_text(
                            "{} | {} KiB",
                            entry.kind,
                            (entry.size + 1023u) / 1024u);
                    presentation.search_terms = entry.label + " " + entry.path
                        + (entry.directory ? " folder directory" : "");
                    presentation.kind = gridKind(entry);

                    if (const auto* texture = textureForEntry(entry))
                    {
                        presentation.logical_texture = texture->logical_path;
                        if (const auto* thumbnail =
                                project_texture_thumbnail_state(
                                    editor,
                                    *textureController,
                                    *texture))
                        {
                            presentation.image = thumbnail->sprite;
                            presentation.source_size = {
                                static_cast<float>(thumbnail->source_width),
                                static_cast<float>(thumbnail->source_height)};
                            presentation.detail = epochengine::format_text(
                                "TEXTURE | {}x{} | {} KiB",
                                thumbnail->source_width,
                                thumbnail->source_height,
                                (entry.size + 1023u) / 1024u);
                        }
                    }
                    else if (entry.kind == "TEXTURE" || entry.kind == "IMAGE")
                    {
                        if (const auto* thumbnail =
                                project_source_texture_thumbnail(
                                    editor,
                                    entry.path))
                        {
                            presentation.image = thumbnail->sprite;
                            presentation.source_size = {
                                static_cast<float>(thumbnail->source_width),
                                static_cast<float>(thumbnail->source_height)};
                            presentation.detail = epochengine::format_text(
                                "{} | {}x{} | {} KiB",
                                entry.kind,
                                thumbnail->source_width,
                                thumbnail->source_height,
                                (entry.size + 1023u) / 1024u);
                        }
                    }
                    presentations.push_back(std::move(presentation));
                }

                std::vector<gui::AssetGridItem> gridItems{};
                gridItems.reserve(presentations.size());
                std::optional<std::uint64_t> selectedGridId{};
                for (const AssetGridPresentation& presentation : presentations)
                {
                    const EditorBrowserEntry& entry =
                        assetEntries[presentation.entry_index];
                    const bool selected = editor.selectedAssetPath == entry.path
                        || (!presentation.logical_texture.empty()
                            && (editor.selectedAssetPath
                                    == presentation.logical_texture
                                || (textureController
                                    && textureController->selected_logical_path()
                                        == presentation.logical_texture)));
                    if (selected)
                        selectedGridId = presentation.id;
                    gridItems.push_back(gui::AssetGridItem{
                        .id = presentation.id,
                        .label = presentation.label,
                        .detail = presentation.detail,
                        .search_terms = presentation.search_terms,
                        .image = presentation.image,
                        .source_size = presentation.source_size,
                        .kind = presentation.kind
                    });
                }

                const float assetGridHeight = (std::clamp)(
                    centerScrollHeight - 180.0f,
                    300.0f,
                    680.0f);
                const std::array assetContextActions{
                    gui::ContextMenuActionSpec{
                        .id = "open",
                        .label = "Open"},
                    gui::ContextMenuActionSpec{
                        .id = "details",
                        .label = "Show Details"},
                    gui::ContextMenuActionSpec{
                        .id = "copy_path",
                        .label = "Copy Path"}
                };
                const gui::AssetGridResult gridResult = gui::asset_grid({
                    .id = "asset-manager.browser-grid",
                    .items = gridItems,
                    .context_actions = assetContextActions,
                    .query = editor.assetSearch,
                    .size = {centerWidth, assetGridHeight},
                    .tile_size = {148.0f, 166.0f},
                    .gap = {8.0f, 8.0f},
                    .padding = {8.0f, 8.0f},
                    .image_height = 108.0f,
                    .scroll_offset = editor.assetGridScroll,
                    .selected_id = selectedGridId
                });
                editor.assetGridScroll = gridResult.scroll_offset;
                editor.assetGridVisibleIds = gridResult.visible_ids;

                const auto presentationForId =
                    [&](std::uint64_t id)
                    {
                        return std::find_if(
                            presentations.begin(),
                            presentations.end(),
                            [id](
                                const AssetGridPresentation&
                                    candidate)
                            {
                                return candidate.id == id;
                            });
                    };
                const auto selectAssetPresentation =
                    [&](const AssetGridPresentation&
                            presentation,
                        bool open)
                    {
                        const EditorBrowserEntry& entry =
                            assetEntries[
                                presentation.entry_index];
                        editor.selectedAssetPath =
                            entry.path;
                        editor.selectedProjectFile =
                            entry.path;
                        if (!open)
                            return;
                        if (entry.directory)
                        {
                            enter_asset_browser_folder(
                                editor, entry);
                            return;
                        }
                        if (const auto* texture =
                                textureForEntry(entry))
                        {
                            (void)activate_project_texture(
                                editor,
                                ctx,
                                *textureController,
                                *texture);
                            editor.assetWorkspaceSection =
                                1u;
                        }
                        else if (entry.kind == "IMAGE")
                        {
                            if (project_texture_source_path(entry.path))
                                import_selected_project_texture(editor, ctx);
                            else
                                editor.projectTextureStatus =
                                    "JPEG is a project reference. Its thumbnail is available on Windows; full texture compilation supports PNG, BMP, TGA and PPM.";
                        }
                        else if (entry.kind == "AUDIO")
                        {
                            const auto audio =
                                editor_project_audio_profile_summary(
                                    editor.projectId);
                            const auto cue = std::find_if(
                                audio.cues.begin(),
                                audio.cues.end(),
                                [&entry](const auto& candidate)
                                {
                                    return candidate.logical_path
                                        == entry.path;
                                });
                            if (cue != audio.cues.end())
                            {
                                editor.selectedProjectAudioCueId =
                                    cue->stable_id;
                            }
                            editor.assetWorkspaceSection = 4u;
                        }
                        if (std::filesystem::path{
                                std::u8string(entry.path.begin(), entry.path.end())}
                                .filename()
                                .generic_u8string()
                                .ends_with(
                                    u8".ascript.cpp"))
                        {
                            editor.activeScript =
                                script_id_from_source_path(
                                    std::filesystem::path{
                                        entry.path});
                            editor.scriptBuildStatus =
                                "Selected script asset: "
                                + entry.path;
                            editor.assetWorkspaceSection =
                                2u;
                            (void)load_script_source_editor(
                                editor,
                                entry.path,
                                false);
                        }
                        push_editor_log(
                            editor,
                            "[assets] Opened "
                                + entry.path);
                    };

                if (gridResult.context_target_id)
                {
                    const auto contextTarget =
                        presentationForId(
                            *gridResult.context_target_id);
                    if (contextTarget
                        != presentations.end())
                    {
                        selectAssetPresentation(
                            *contextTarget,
                            false);
                        if (gridResult.selected_action_id)
                        {
                            if (*gridResult
                                    .selected_action_id
                                == "open")
                            {
                                selectAssetPresentation(
                                    *contextTarget,
                                    true);
                            }
                            else if (*gridResult
                                    .selected_action_id
                                == "details")
                            {
                                editor.showInspector = true;
                                editor.inspectorToolTab =
                                    InspectorToolTab::
                                        Properties;
                            }
                            else if (*gridResult
                                    .selected_action_id
                                == "copy_path")
                            {
                                (void)gui::
                                    set_clipboard_text(
                                        assetEntries[
                                            contextTarget
                                                ->entry_index]
                                            .path);
                            }
                        }
                    }
                }
                else if (gridResult.activated_id)
                {
                    const auto activated =
                        presentationForId(
                            *gridResult.activated_id);
                    if (activated != presentations.end())
                    {
                        selectAssetPresentation(
                            *activated,
                            true);
                    }
                }
                gui::property_row(
                    "Visible",
                    epochengine::format_text(
                        "{} / {}",
                        gridResult.matched_count,
                        presentations.size()),
                    120.0f);
                gui::property_row(
                    "Selected",
                    editor.selectedAssetPath.empty()
                        ? std::string{"(none)"}
                        : editor.selectedAssetPath,
                    120.0f);
                if (gridResult.truncated)
                {
                    gui::wrapped_label(
                        "The current asset view reached its bounded catalog or visible-tile limit.",
                        centerWidth);
                }

                }
                else if (editor.assetWorkspaceSection == 1u)
                {
                    gui::label("Texture Workspace");
                    draw_texture_source_controls(
                        editor,
                        ctx,
                        (std::min)(centerWidth, 620.0f));

                if (auto* textures =
                        project_texture_controller(editor))
                {
                    gui::property_row(
                        "[texture] Compiled",
                        std::to_string(textures->catalog().size()),
                        120.0f);
                    if (editor.assetCategory == 0u
                        || editor.assetCategory == 1u)
                    {
                        gui::label("Compiled Texture Library");
                        draw_project_texture_catalog(
                            editor,
                            ctx,
                            *textures,
                            (std::min)(centerWidth, 560.0f),
                            "central.assets",
                            true);
                    }
                    draw_active_texture_authoring(
                        editor,
                        ctx,
                        (std::min)(centerWidth, 560.0f),
                        "central.assets",
                        300.0f);
                    draw_texture_application_controls(
                        editor,
                        (std::min)(centerWidth, 620.0f));
                }
                }
                else if (editor.assetWorkspaceSection == 2u)
                {
                const auto codeWorkspace = draw_project_code_workspace(
                    editor,
                    centerWidth,
                    centerScrollHeight);
                if (codeWorkspace.returnToScene)
                {
                    apply_editor_surface(
                        EditorMainSurface::Scene,
                        "Project Scripts");
                }
                if (codeWorkspace.handled)
                    break;
                const std::string activeScriptSource =
                    editor_resolve_script_source_path(
                        editor.activeScript,
                        editor.projectRoot);
                {
                    gui::label("Script Assets");
                    gui::property_row("[script asset] Active", editor.activeScript, 120.0f);
                    gui::property_row("[script asset] Source", activeScriptSource, 120.0f);
                    gui::property_row(
                        "[script asset] Source exists",
                        std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false",
                        120.0f);
                    if (const auto* activeScript = active_script_profile(editor))
                    {
                        gui::property_row("[script asset] Build", activeScript->build_action, 120.0f);
                        gui::property_row("[script asset] Run", "active project only", 120.0f);
                        gui::property_row("[script asset] Hint", activeScript->diagnostic_hint, 120.0f);
                    }
                    gui::property_row(
                        "[script asset] Build state",
                        editor.scriptBuildPending ? "Running" : "Idle",
                        120.0f);
                    gui::property_row(
                        "[script asset] Output",
                        editor.scriptBuildTarget.empty() ? std::string("(not built this session)") : editor.scriptBuildTarget,
                        120.0f);
                    gui::wrapped_label(editor.scriptBuildStatus, centerWidth);
                    gui::property_row("[script asset] New", "type a safe id, then create a project-local .ascript.cpp", 120.0f);
                    (void)gui::edit_box(editor.newScriptName, { centerWidth, 28.0f }, 64, false);
                    if (editor.scriptBuildPending)
                    {
                        gui::progress_bar(gui::ProgressBarOptions{
                            .label = "Compiling C++23 Script Asset",
                            .status = editor.scriptBuildStatus,
                            .value = 0.0f,
                            .size = { (std::min)(centerWidth, 520.0f), 20.0f },
                            .show_percent = false,
                            .activity = true
                        });
                        if (gui::button(
                                "Cancel Script Build",
                                { (std::min)(220.0f, centerWidth), 30.0f }))
                        {
                            (void)cancel_script_build(editor);
                        }
                    }
                    else
                    {
                        if (gui::button("Create Script Asset", { (std::min)(220.0f, centerWidth), 30.0f }))
                            create_project_script_starter(editor);
                        if (gui::button("Build Selected Script Asset", { (std::min)(240.0f, centerWidth), 30.0f }))
                            start_script_build(editor, "Scripting workspace build request");
                    }
                    draw_script_source_editor(
                        editor,
                        activeScriptSource,
                        centerWidth,
                        (std::clamp)(
                            centerScrollHeight - 360.0f,
                            320.0f,
                            760.0f));
                }
                }
                else if (editor.assetWorkspaceSection == 3u)
                {
                    gui::label("Asset Dependency Graph");
                    gui::wrapped_label(
                        "This graph projects project files and authenticated "
                        "compiled texture artifacts. Selecting a texture node "
                        "opens its real texture workspace state.",
                        centerWidth);
                    draw_asset_dependency_graph(
                        editor,
                        (std::min)(centerWidth, 760.0f));
                }
                else
                {
                    draw_project_audio_workspace(
                        editor,
                        (std::min)(centerWidth, 720.0f));
                }
                break;
            }
            case EditorMainSurface::PlantLab:
            {
                const auto forestPackage = std::find_if(
                    epochengine::package_registry::kKnownPackages.begin(),
                    epochengine::package_registry::kKnownPackages.end(),
                    [](const epochengine::package_registry::PackageDescriptor& package) {
                        return package.id == epochengine::package_registry::kEngineForestFactoryPackageId;
                    });
                (void)ensure_plant_lab_compiled_asset(editor);
                const auto& document = editor.plantLabDocument;
                const auto& profile = document.profile;
                const auto& compiled = editor.plantLabCompiled;
                const auto& geometry = compiled.preview;
                const auto stats = geometry.stats;
                const auto& voxelSummary = compiled.voxelOccupancy;
                const bool plantAssetReady =
                    ensure_plant_lab_compiled_asset(editor);
                const auto& plantAsset = editor.plantLabCompiled;
                const std::filesystem::path manifestPath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory.package.json";
                const std::filesystem::path profilePath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory" / "default.forest.json";
                const bool manifestReady = std::filesystem::exists(manifestPath);
                const bool profileReady = std::filesystem::exists(profilePath);

                gui::label("Plant Lab");
                gui::wrapped_label(
                    "Separate temporal graph and parametric tree/forest authoring editor. Plant Lab owns custom tree assets, forest configurations, and live preview; Forest Factory later places those outputs in standard project scenes.",
                    centerWidth);
                gui::property_row("[forest] Editor name", std::string(epochengine::forest::kPlantLabWorkspace), 148.0f);
                gui::property_row("[forest] Technique", std::string(epochengine::forest::kForestFactoryTechnique), 148.0f);
                gui::property_row("[forest] Reference repo", std::string(epochengine::forest::kForestFactoryReferenceRepo), 148.0f);
                gui::property_row("[forest] Package", forestPackage != epochengine::package_registry::kKnownPackages.end() ? std::string(forestPackage->displayName) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Package source", forestPackage != epochengine::package_registry::kKnownPackages.end() ? std::string(forestPackage->externalSourceRepo) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Manifest", manifestReady ? display_project_path(manifestPath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row("[forest] Profile", profileReady ? display_project_path(profilePath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row(
                    "[forest] Source revision",
                    std::to_string(document.revision),
                    148.0f);
                gui::property_row(
                    "[forest] Content",
                    epochengine::format_text(
                        "{:016x}", document.contentHash),
                    148.0f);
                gui::property_row(
                    "[forest] History",
                    epochengine::format_text(
                        "{} / {} semantic edits",
                        document.historyCursor,
                        document.journal.size()),
                    148.0f);
                gui::property_row(
                    "[forest] Compiler",
                    editor.plantLabStatus,
                    148.0f);
                gui::property_row(
                    "[forest] Project asset",
                    editor.plantLabAssetStatus,
                    148.0f);
                gui::property_row(
                    "[forest] Voxel LODs",
                    std::to_string(compiled.voxelLods.levels.size()),
                    148.0f);
                gui::property_row("[forest] Preset", std::string(epochengine::forest::preset_name(profile.preset)), 148.0f);
                gui::property_row("[forest] Mode", profile.previewMode == epochengine::forest::ForestPreviewMode::Mode3D ? "3D" : "2D", 148.0f);
                gui::property_row("[forest] Stage", std::string(epochengine::forest::stage_name(profile.editStage)), 148.0f);
                gui::property_row("[forest] Seed", std::to_string(profile.seed.value), 148.0f);
                gui::property_row("[forest] Time", epochengine::format_text("{:.2f}s / {:.2f}s", profile.temporal.timeSeconds, profile.temporal.durationSeconds), 148.0f);
                gui::property_row("[forest] Speed", epochengine::format_text("{:.2f}x", profile.temporal.speed), 148.0f);
                gui::property_row("[forest] Nodes", std::to_string(stats.nodes), 148.0f);
                gui::property_row("[forest] Branches", std::to_string(stats.branches), 148.0f);
                gui::property_row("[forest] Leaves", std::to_string(stats.leaves), 148.0f);
                gui::property_row("[forest] Preview segments", std::to_string(geometry.segmentCount), 148.0f);
                gui::property_row("[forest] Verts", std::to_string(stats.vertices), 148.0f);
                gui::property_row("[forest] Tris", std::to_string(stats.triangles), 148.0f);
                gui::property_row("[forest] Voxel chunk",
                    epochengine::format_text(
                        "{}x{}x{} @ {:.2f}m",
                        voxelSummary.chunk.cellsX,
                        voxelSummary.chunk.cellsY,
                        voxelSummary.chunk.cellsZ,
                        voxelSummary.chunk.cellSizeMeters),
                    148.0f);
                gui::property_row("[forest] Active voxels", std::to_string(voxelSummary.activeCells), 148.0f);
                gui::property_row("[forest] Dense bytes", std::to_string(voxelSummary.denseBytes), 148.0f);
                gui::property_row("[forest] Voxel split",
                    epochengine::format_text(
                        "trunk {} | branch {} | foliage {}",
                        voxelSummary.trunkCells,
                        voxelSummary.branchCells,
                        voxelSummary.foliageCells),
                    148.0f);
                gui::wrapped_label(
                    "Plant Lab authors deterministic tree assets and forest configurations with temporal graph, foliage, and voxel LOD intent. Forest Factory browsing, placement, scene overrides, and package activation remain explicit standard-editor actions.",
                    centerWidth);
                const auto refreshPlantLab = [&]()
                {
                    editor.plantLabCompiledRevision = 0u;
                    editor.plantLabCompiledContentHash = 0u;
                    ensure_plant_lab_preview_entities(editor);
                };
                const auto applyPlantEdit = [&](
                    epochengine::forest::ForestProfileProperty property,
                    float requestedValue,
                    std::string_view label)
                {
                    if (!epochengine::forest::apply_forest_profile_edit(
                            editor.plantLabDocument,
                            property,
                            requestedValue))
                    {
                        editor.plantLabStatus =
                            std::string(label)
                            + " is already at its validated value.";
                        return;
                    }
                    refreshPlantLab();
                    push_editor_log(
                        editor,
                        epochengine::format_text(
                            "[plant] {} committed as semantic revision {}.",
                            label,
                            editor.plantLabDocument.revision));
                };

                std::array<gui::InlineButtonSpec, 3> historyActions{{
                    {"Undo", 104.0f},
                    {"Redo", 104.0f},
                    {"Reset Default", 142.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(historyActions, 30.0f, 8.0f))
                {
                    bool changed = false;
                    if (*clicked == 0)
                    {
                        changed =
                            epochengine::forest::undo_forest_profile_edit(
                                editor.plantLabDocument);
                        if (!changed)
                            editor.plantLabStatus =
                                "No Plant Lab semantic edit is available to undo.";
                    }
                    else if (*clicked == 1)
                    {
                        changed =
                            epochengine::forest::redo_forest_profile_edit(
                                editor.plantLabDocument);
                        if (!changed)
                            editor.plantLabStatus =
                                "No Plant Lab semantic edit is available to redo.";
                    }
                    else
                    {
                        editor.plantLabDocument =
                            epochengine::forest::
                                make_default_plant_lab_document();
                        editor.plantLabDocumentInitialized = true;
                        changed = true;
                    }
                    if (changed)
                    {
                        refreshPlantLab();
                        push_editor_log(
                            editor,
                            epochengine::format_text(
                                "[plant] {} produced revision {}.",
                                *clicked == 0
                                    ? "Undo"
                                    : (*clicked == 1
                                        ? "Redo"
                                        : "Default reset"),
                                editor.plantLabDocument.revision));
                    }
                }

                std::array<gui::InlineButtonSpec, 4> structureActions{{
                    {"Height -", 106.0f},
                    {"Height +", 106.0f},
                    {"Branches -", 118.0f},
                    {"Branches +", 118.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(
                            structureActions,
                            30.0f,
                            8.0f))
                {
                    if (*clicked == 0)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                TargetHeightMeters,
                            editor.plantLabDocument.profile.config
                                .targetHeightMeters - 0.25f,
                            "Target height");
                    else if (*clicked == 1)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                TargetHeightMeters,
                            editor.plantLabDocument.profile.config
                                .targetHeightMeters + 0.25f,
                            "Target height");
                    else if (*clicked == 2)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLevels,
                            static_cast<float>(
                                editor.plantLabDocument.profile.branch.levels)
                                - 1.0f,
                            "Branch levels");
                    else
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLevels,
                            static_cast<float>(
                                editor.plantLabDocument.profile.branch.levels)
                                + 1.0f,
                            "Branch levels");
                }

                std::array<gui::InlineButtonSpec, 4> shapeActions{{
                    {"Length -", 106.0f},
                    {"Length +", 106.0f},
                    {"Growth -1s", 118.0f},
                    {"Growth +1s", 118.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(shapeActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLengthMeters,
                            editor.plantLabDocument.profile.branch
                                .branchLengthMeters - 0.10f,
                            "Branch length");
                    else if (*clicked == 1)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                BranchLengthMeters,
                            editor.plantLabDocument.profile.branch
                                .branchLengthMeters + 0.10f,
                            "Branch length");
                    else if (*clicked == 2)
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                GrowthTimeSeconds,
                            editor.plantLabDocument.profile.temporal.timeSeconds
                                - 1.0f,
                            "Growth time");
                    else
                        applyPlantEdit(
                            epochengine::forest::ForestProfileProperty::
                                GrowthTimeSeconds,
                            editor.plantLabDocument.profile.temporal.timeSeconds
                                + 1.0f,
                            "Growth time");
                }

                std::array<gui::InlineButtonSpec, 2> previewActions{{
                    {"Rebuild Preview", 164.0f},
                    {"Select Lead Tip", 154.0f}
                }};
                if (auto clicked =
                        gui::inline_button_row(previewActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                    {
                        refreshPlantLab();
                        push_editor_log(
                            editor,
                            "[plant] Rebuilt the compiled temporal preview.");
                    }
                    else
                    {
                        const auto selected = std::find_if(
                            editor.entities.begin(),
                            editor.entities.end(),
                            [](const EditorEntity& entity)
                            {
                                return entity.name.rfind(
                                    "PlantLabCanopy_",
                                    0u) == 0u;
                            });
                        if (selected != editor.entities.end())
                        {
                            select_editor_entity(
                                editor,
                                static_cast<std::size_t>(
                                    std::distance(
                                        editor.entities.begin(),
                                        selected)));
                            push_editor_log(
                                editor,
                                "[plant] Selected the first visible lead tip.");
                        }
                        else
                        {
                            refreshPlantLab();
                        }
                    }
                }

                const std::array<gui::InlineButtonSpec, 2> persistenceActions{{
                    {"Publish To Project", 178.0f},
                    {"Reopen Published", 168.0f}
                }};
                if (const auto clicked = gui::inline_button_row(
                        persistenceActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                    {
                        (void)publish_plant_lab_project_asset(editor);
                    }
                    else if (reopen_plant_lab_project_asset(editor, true))
                    {
                        ensure_plant_lab_preview_entities(editor);
                    }
                }

                if (gui::button("Open Package Manager", { 220.0f, 30.0f }))
                {
                    editor.showPackageManagerModal = true;
                    editor.selectedPackageId = std::string(epochengine::package_registry::kEngineForestFactoryPackageId);
                    editor.packageInstallStatus = manifestReady && profileReady
                        ? "Forest Factory project package is already staged."
                        : "Select Install to stage the Forest Factory project package.";
                    push_editor_log(editor, "[forest] Package Manager opened for Forest Factory.");
                }
                if (gui::button("Stage Forest Factory Package", { 240.0f, 30.0f }))
                {
                    if (forestPackage != epochengine::package_registry::kKnownPackages.end())
                        (void)stage_forest_factory_package_opt_in(editor, *forestPackage);
                    else
                        editor.packageInstallStatus = "Forest Factory package registry entry is missing.";
                }
                break;
            }
            case EditorMainSurface::Timeline:
                render_timeline_workspace(editor, centerWidth, render_timeline_time_controls);
                break;
#if EPOCH_EDITION_PUBLIC
            case EditorMainSurface::AISandbox:
            {
                gui::wrapped_label("Ask about your project, plan a scene change, or choose a model connection. Engine source tools are available in the Development edition.", centerWidth);
                if (gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Model Manager"}},
                        30.0f, 6.0f, gui::InlineButtonRowOptions{
                            .sizing=gui::InlineButtonSizing::FitContent,
                            .alignment=gui::InlineButtonAlignment::Center,
                            .wrap=true, .available_width=centerWidth}))
                    editor.showAiModelManagerModal = true;
                if (editor.taskScheduler)
                {
                    const auto activity = editor.taskScheduler->activity();
                    gui::property_row("Tasks", std::to_string(activity.running_tasks) + " running / "
                        + std::to_string(activity.queued_tasks + activity.waiting_tasks) + " queued");
                }
                break;
            }
#endif
            case EditorMainSurface::Systems:
            {
                if (!editor.systems.panel)
                    editor.systems.panel = std::make_unique<editor_systems_panel::Panel>();
                auto& registry = epochengine::systems::Registry::instance();
                const std::size_t hardwareThreadCount = (std::max)(
                    std::size_t{1},
                    std::thread::hardware_concurrency() > 0
                        ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                        : std::size_t{6});
                const auto& projectProfile = active_project_profile(editor);
                const auto editorBackend = renderer_backend_kind(ctx);
                const auto runBackend = renderer_backend_kind(editor.projectRunBackend);
                editor.systems.panel->render(
                    centerWidth,
                    registry,
                    editor_systems_panel::Snapshot{
                        .renderer = renderer_name(ctx),
                        .capability = renderer_capability_profile_summary(ctx),
                        .editor_admission = renderer_admission_summary(
                            editorBackend,
                            projectProfile.renderer_capability),
                        .project_admission = renderer_admission_summary(
                            runBackend,
                            projectProfile.renderer_capability),
                        .resource_spine = renderer_resource_spine_summary(ctx),
                        .proof_stages = renderer_capability_proof_stage_status(ctx),
                        .rtt_overall = renderer_sampled_rtt_rollup_status(ctx),
                        .rtt_presentation = renderer_sampled_rtt_presentation_status(ctx),
                        .mesh_model = renderer_native_mesh_model_status(ctx),
                        .next_gate = renderer_next_feature_gate(ctx),
                        .backend_guidance = backend_runtime_guidance(
                            ctx,
                            recommended_support_tier(ctx, hardwareThreadCount)),
                        .convergence_focus = backend_convergence_focus(ctx),
                        .support_tier = recommended_support_tier(
                            ctx,
                            hardwareThreadCount),
                        .project_build = editor.projectBuildPending
                            ? std::string{"Running"}
                            : editor.projectBuildStatus,
                        .script_build = editor.scriptBuildPending
                            ? std::string{"Running"}
                            : editor.scriptBuildStatus,
                        .live_threads = epochengine::systems::threading::live_thread_count(),
                        .hardware_threads = hardwareThreadCount,
                        .gpu = toolbarResources.gpu
                    },
                    editor.taskScheduler ? &editor.taskScheduler->graph() : nullptr,
                    "Editor Project and Script Tasks");
                break;
            }
            case EditorMainSurface::Game2D:
            {
                if (!guiDocumentWorkspace)
                    break;
                const bool dedicatedGuiEditor =
                    editor.applicationKind
                        == EditorApplicationKind::GuiEditor;
                if (!dedicatedGuiEditor)
                {
                    editor.guiDocumentTab = 0u;
                    gui::section_heading("GUI placement");
                    const gui::Vec2 documentPosition =
                        gui::cursor_position();
                    const gui::Vec2 documentSize{
                        centerWidth,
                        (std::max)(1.0f, centerScrollStart.y + centerScrollHeight
                            - documentPosition.y - 4.0f)};
                    render_gui_document_canvas(
                        editor,
                        documentPosition,
                        documentSize,
                        true);
                }
                else
                {
                    const auto layout =
                        editor_workspace::make_workspace_layout(
                            editor_workspace::ApplicationKind::gui_editor);
                    constexpr std::array roles{
                        editor_workspace::ContentRole::gui_canvas,
                        editor_workspace::ContentRole::gui_runtime_preview,
                        editor_workspace::ContentRole::gui_component_graph,
                        editor_workspace::ContentRole::gui_style_document
                    };
                    constexpr std::array widths{
                        82.0f, 132.0f, 132.0f, 78.0f};
                    std::array<gui::TabButtonSpec, roles.size()> tabs{};
                    for (std::size_t index = 0u;
                        index < roles.size();
                        ++index)
                    {
                        if (const auto* tab =
                                layout.find_content(roles[index]))
                        {
                            tabs[index] = {
                                .id = tab->canonical_name,
                                .label = tab->label,
                                .width = widths[index],
                                .active = editor.guiDocumentTab == index
                            };
                        }
                    }
                    if (const auto selected =
                            gui::responsive_tab_bar_buttons({
                                .overflow_id="gui-document-views", .overflow_label="GUI views",
                                .tabs=tabs, .available_width=centerWidth,
                                .height=30.0f, .gap=2.0f});
                        selected.selected_index)
                    {
                        editor.guiDocumentTab =
                            *selected.selected_index;
                    }

                    const gui::Vec2 documentPosition =
                        gui::cursor_position();
                    const gui::Vec2 documentSize{
                        centerWidth,
                        (std::max)(1.0f, centerScrollStart.y + centerScrollHeight
                            - documentPosition.y - 4.0f)};
                    if (editor.guiDocumentTab < 2u)
                    {
                        render_gui_document_canvas(
                            editor,
                            documentPosition,
                            documentSize,
                            editor.guiDocumentTab == 0u);
                    }
                    else if (auto* document =
                            synchronize_gui_document(editor))
                    {
                        const auto widgets = document->widgets();
                        if (editor.guiDocumentTab == 2u)
                        {
                            for (const auto& widget : widgets)
                            {
                                if (widget.handle
                                    == editor.guiDocumentRoot)
                                {
                                    continue;
                                }
                                if (gui::menu_item({
                                        .label=widget.descriptor.name,
                                        .size={centerWidth, gui::menu_item_row_height()},
                                        .tone=gui::SemanticTone::info,
                                        .selected=gui_widget_selected(editor, widget.handle)}))
                                {
                                    select_gui_widget(
                                        editor,
                                        widget.handle);
                                }
                            }
                        }
                        else
                        {
                            for (const auto& widget : widgets)
                            {
                                if (widget.handle
                                    == editor.guiDocumentRoot)
                                {
                                    continue;
                                }
                                gui::property_row(
                                    widget.descriptor.name,
                                    widget.descriptor.style.style_class.empty()
                                        ? gui_widget_display_text(widget)
                                        : widget.descriptor.style.style_class,
                                    180.0f);
                            }
                        }
                    }
                }                break;
            }
            case EditorMainSurface::Scene:
            default:
                break;
            }

            gui::end_scroll_area();
        }
        gui::end_window();

        constexpr float toolGroupTabHeight = 38.0f;
        std::array<float, 4> toolGroupTabHeights{
            toolGroupTabHeight, toolGroupTabHeight, toolGroupTabHeight, toolGroupTabHeight};
        auto tool_group_position = [&](EditorToolDockRegion region) noexcept
        {
            switch (region)
            {
            case EditorToolDockRegion::Right:
                return details_pos;
            case EditorToolDockRegion::BottomLeft:
                return bottomLeftPosition;
            case EditorToolDockRegion::BottomRight:
                return bottomRightPosition;
            case EditorToolDockRegion::Left:
            default:
                return outliner_pos;
            }
        };
        auto tool_group_size = [&](EditorToolDockRegion region) noexcept
        {
            switch (region)
            {
            case EditorToolDockRegion::Right:
                return details_size;
            case EditorToolDockRegion::BottomLeft:
                return bottomLeftSize;
            case EditorToolDockRegion::BottomRight:
                return bottomRightSize;
            case EditorToolDockRegion::Left:
            default:
                return outliner_size;
            }
        };
        auto tool_group_content_position = [&](EditorToolDockRegion region) noexcept
        {
            const gui::Vec2 position = tool_group_position(region);
            return gui::Vec2{ position.x, position.y + toolGroupTabHeights[tool_dock_region_index(region)] };
        };
        auto tool_group_content_size = [&](EditorToolDockRegion region) noexcept
        {
            const gui::Vec2 size = tool_group_size(region);
            return gui::Vec2{
                size.x,
                (std::max)(0.0f, size.y - toolGroupTabHeights[tool_dock_region_index(region)]) };
        };
        auto tool_pane_label = [&](std::string_view route) noexcept
        {
            for (std::size_t i = 0; i < toolPaneRoutes.size(); ++i)
                if (toolPaneRoutes[i] == route)
                    return toolPaneLabels[i];
            return std::string_view{ "Tool" };
        };
        auto render_tool_group_tabs = [&](EditorToolDockRegion region)
        {
            if (!region_has_visible_pane(region))
                return;

            std::vector<gui::TabButtonSpec> tabs{};
            std::vector<std::string_view> routes{};
            const std::string_view active = active_pane_for_region(region);
            for (const std::size_t i : ordered_pane_indices_for_region(region))
            {
                const std::string_view route = toolPaneRoutes[i];
                routes.push_back(route);
                tabs.push_back(gui::TabButtonSpec{
                    .id = route,
                    .label = toolPaneLabels[i],
                    .width = 0.0f,
                    .active = active == route,
                    .closable = true,
                    .dirty = false,
                    .enabled = true
                });
            }
            if (tabs.empty())
                return;

            const gui::Vec2 position = tool_group_position(region);
            const gui::Vec2 size = tool_group_size(region);
            const bool paneDragAllowed = !modal_visible_now()
                && editor.openMenu == TopMenu::None && !floating_gui_visible();
            if (!paneDragAllowed) reset_pane_title_drag();
            const bool ownsPaneDrag = !editor.paneTitleDragActive
                || std::ranges::find(routes, editor.paneTitleDragRoute) != routes.end();
            const std::size_t regionIndex = tool_dock_region_index(region);
            const gui::ResponsiveTabBarOptions tabOptions{
                .tabs=tabs, .available_width=(std::max)(1.0f, size.x - 6.0f),
                .height=toolGroupTabHeight - 3.0f, .gap=1.0f,
                .presentation=gui::TabBarPresentation::Document,
                .keyboard_navigation=true,
                .drag=paneDragAllowed && ownsPaneDrag
                    ? gui::TabDragOptions{&editor.paneTitleDrag, editor.paneTitleDragRoute, 12.0f}
                    : gui::TabDragOptions{},
                .wrap=true};
            toolGroupTabHeights[regionIndex] = gui::responsive_tab_bar_height(tabOptions) + 4.0f;
            gui::begin_window({}, position, {size.x, toolGroupTabHeights[regionIndex]});
            gui::set_cursor({position.x + 3.0f, position.y + 2.0f});
            // Keep every full title directly selectable and draggable, on measured
            // rows when the pane narrows. No equal-width clipping or hidden aggregate.
            const auto tabResult = gui::responsive_tab_bar_buttons(tabOptions);
            directTabRoutes[regionIndex] = routes;
            directTabBounds[regionIndex].clear();
            directTabBounds[regionIndex].reserve(tabResult.items.size());
            for (const auto& item : tabResult.items)
            {
                directTabBounds[regionIndex].push_back({
                    {item.position.x, item.position.y},
                    {item.size.x, item.size.y}
                });
            }
            directTabStripBounds[regionIndex] = {
                {position.x + 3.0f, position.y + 2.0f},
                {(std::max)(1.0f, size.x - 6.0f), toolGroupTabHeights[regionIndex] - 4.0f}
            };
            const std::optional<std::size_t> activationIndex =
                tabResult.dragged_index && tabResult.drag.pressed
                    ? tabResult.dragged_index
                : tabResult.pressed_index
                    ? tabResult.pressed_index
                    : tabResult.selected_index;
            if (activationIndex && *activationIndex < routes.size())
            {
                set_active_pane(region, routes[*activationIndex]);
                result.scene_input_captured = true;
            }
            if (tabResult.closed_index
                && *tabResult.closed_index < routes.size())
            {
                const std::string_view closed = routes[*tabResult.closed_index];
                if (editor.paneTitleDragRoute == closed) reset_pane_title_drag();
                set_pane_shown(closed, false);
                push_editor_log(
                    editor,
                    "[window] " + std::string(tool_pane_label(closed))
                    + " hidden. Reopen it from Window.");
                result.scene_input_captured = true;
            }
            if (tabResult.dragged_index
                && *tabResult.dragged_index < routes.size())
            {
                const std::string_view route =
                    routes[*tabResult.dragged_index];
                capture_pane_title_drag(route, tool_pane_label(route), tabResult);
            }
            gui::end_window();
        };

        render_tool_group_tabs(EditorToolDockRegion::Left);
        render_tool_group_tabs(EditorToolDockRegion::Right);
        render_tool_group_tabs(EditorToolDockRegion::BottomLeft);
        render_tool_group_tabs(EditorToolDockRegion::BottomRight);

        const auto render_structural_tool_pane =
            [&](EditorToolDockRegion region)
        {
            const std::string_view active = active_pane_for_region(region);
            const auto pane = editor_tool_pane_from_route(active);
            if (!pane)
                return;
            const gui::Vec2 position = tool_group_content_position(region);
            const gui::Vec2 size = tool_group_content_size(region);
            const std::string_view label =
                toolPaneLabels[tool_pane_index(*pane)];
            switch (*pane)
            {
            case EditorToolPane::WorldOutliner:
                render_outliner_window(
                    position, size, OutlinerToolTab::World, label);
                break;
            case EditorToolPane::AssetBrowser:
                render_outliner_window(
                    position, size, OutlinerToolTab::Assets, label);
                break;
            case EditorToolPane::GuiHierarchy:
                render_outliner_window(
                    position, size, OutlinerToolTab::Gui, label);
                break;
            case EditorToolPane::ScriptBrowser:
                render_outliner_window(
                    position, size, OutlinerToolTab::Scripting, label);
                break;
            case EditorToolPane::TileMap:
                render_outliner_window(
                    position, size, OutlinerToolTab::TileMap, label);
                break;
            case EditorToolPane::Properties:
                render_inspector_window(
                    position, size, InspectorToolTab::Properties, label);
                break;
            case EditorToolPane::WorldSettings:
                render_inspector_window(
                    position, size, InspectorToolTab::WorldSettings, label);
                break;
            case EditorToolPane::AiControls:
                break; // Retired persistence slot; no separate settings surface.
            case EditorToolPane::AiDevelopment:
                render_inspector_window(
                    position, size, InspectorToolTab::AiDevelopment, label);
                break;
            default:
                break;
            }
        };
        render_structural_tool_pane(EditorToolDockRegion::Left);
        render_structural_tool_pane(EditorToolDockRegion::Right);
        render_structural_tool_pane(EditorToolDockRegion::BottomLeft);
        render_structural_tool_pane(EditorToolDockRegion::BottomRight);

        const bool outlinerSplitHovered = layout_outliner_visible && editor_point_in_rect(mouse, outliner_split_pos, outliner_split_size);
        const bool inspectorSplitHovered = layout_inspector_visible && editor_point_in_rect(mouse, inspector_split_pos, inspector_split_size);
        const bool bottomSplitHovered = bottom_visible && editor_point_in_rect(mouse, bottom_split_pos, bottom_split_size);
        const bool bottomColumnsSplitHovered = splitBottomColumns && editor_point_in_rect(mouse, bottomColumnsSplitPosition, bottomColumnsSplitSize);
