// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        void ensure_2d_canvas_entity(EditorState& state)
        {
            const auto existing = std::find_if(
                state.entities.begin(),
                state.entities.end(),
                [](const EditorEntity& entity)
                {
                    return entity.type == "Canvas2D" || entity.name == "Canvas2D";
                });
            if (existing != state.entities.end())
            {
                select_editor_entity(
                    state,
                    static_cast<std::size_t>(
                        std::distance(state.entities.begin(), existing)));
                return;
            }

            EditorEntity canvas{};
            canvas.name = "Canvas2D";
            canvas.type = "Canvas2D";
            canvas.category = "2D";
            canvas.position = {0.0f, 1.8f, 0.0f};
            canvas.scale = {6.4f, 3.6f, 0.05f};
            canvas.editorOnly = true;
            if (create_editor_scene_entity(
                    state,
                    std::move(canvas),
                    "Create Canvas2D editing plane"))
            {
                push_editor_log(
                    state,
                    "[2d] Added editor-only Canvas2D editing plane.");
            }
        }
        void ensure_engine_arcade_preview_entities(EditorState& state)
        {
            const std::array nodes{
                epochengine::render_arcade::kCabinetBodySceneNode,
                epochengine::render_arcade::kScreenSceneNode
            };
            std::vector<EditorEntity> desired{};
            desired.reserve(nodes.size());
            for (const auto& node : nodes)
            {
                EditorEntity entity{};
                entity.name = std::string(node.name);
                entity.type = std::string(node.type);
                entity.category = "EngineArcade";
                entity.position = node.position;
                entity.scale = node.scale;
                desired.push_back(std::move(entity));
            }
            (void)synchronize_editor_scene_category(
                state,
                "EngineArcade",
                std::move(desired),
                "Activate Engine Arcade preview",
                epochengine::render_arcade::kScreenSceneNode.name);
        }
        void activate_engine_arcade_preview(EditorState& state)
        {
            state.activeRuntimeScene = std::string(epochengine::package_registry::engine_arcade_default_scene_id());
            state.projectStatus =
                "Engine Arcade active: default scene selected and render-to-texture screen staged in 3D Scene.";
            state.projectCameraMode = previewgrid::CameraMode::Editor;
            state.viewportCameraMode = previewgrid::CameraMode::Editor;
            state.previewMode = core::ScenePreviewMode::Editor;
            state.mainSurface = EditorMainSurface::Scene;
            state.workspaceCompanionsPending = true;
            state.surfaceSettleFrames = (std::max)(state.surfaceSettleFrames, 2);
            ensure_engine_arcade_preview_entities(state);
            push_editor_log(
                state,
                "[package] Engine Arcade activated: default scene "
                    + state.activeRuntimeScene
                    + " selected and visible RTT preview staged.");
        }

        void deactivate_engine_arcade_preview(EditorState& state)
        {
            (void)synchronize_editor_scene_category(
                state,
                "EngineArcade",
                {},
                "Deactivate Engine Arcade preview");
            const auto sceneIds =
                epochengine::package_registry::engine_arcade_scene_ids();
            const std::string_view activeScene{state.activeRuntimeScene};
            bool activeSceneIsArcade = false;
            std::string_view remaining = sceneIds;
            while (!activeScene.empty() && !remaining.empty())
            {
                const auto comma = remaining.find(',');
                const auto scene = comma == std::string_view::npos
                    ? remaining
                    : remaining.substr(0u, comma);
                if (scene == activeScene)
                {
                    activeSceneIsArcade = true;
                    break;
                }
                if (comma == std::string_view::npos)
                    break;
                remaining.remove_prefix(comma + 1u);
            }
            if (activeSceneIsArcade)
                state.activeRuntimeScene.clear();
            state.projectStatus =
                "Engine Arcade removed from the active project preview.";
            state.surfaceSettleFrames =
                (std::max)(state.surfaceSettleFrames, 2);
            push_editor_log(
                state,
                "[package] Engine Arcade preview state removed.");
        }
        inline constexpr std::string_view kDefaultPlantLabLogicalPath{
            "Assets/Forest/default.epoch_forest"};

        [[nodiscard]] bool reopen_plant_lab_project_asset(
            EditorState& state,
            bool reportMissing)
        {
            if (state.projectId.empty() || state.projectRoot.empty())
            {
                state.plantLabAssetStatus =
                    "No active project can own a Plant Lab asset.";
                return false;
            }
            project_forests::ForestAssetLibrary library{
                state.projectId,
                resolve_editor_path(std::filesystem::path{state.projectRoot})};
            if (!library.valid())
            {
                state.plantLabAssetStatus =
                    "The active project's Forest asset library is invalid.";
                return false;
            }
            const auto source = library.load_source(
                kDefaultPlantLabLogicalPath);
            if (!source)
            {
                state.plantLabAssetStatus = source.code
                        == project_forests::ForestLibraryCode::not_found
                    ? "No published Plant Lab asset exists in this project yet."
                    : epochengine::format_text(
                        "Plant Lab source reopen failed: {}.",
                        project_forests::forest_library_code_name(source.code));
                if (reportMissing)
                    push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
                return false;
            }
            const auto artifact = library.load_latest(
                kDefaultPlantLabLogicalPath);
            if (!artifact
                || artifact.artifact.sourceRevision
                    != source.document.revision
                || artifact.artifact.sourceContentHash
                    != source.document.contentHash)
            {
                state.plantLabAssetStatus = artifact
                    ? "Plant Lab source and Library artifact revisions do not match."
                    : epochengine::format_text(
                        "Plant Lab artifact reopen failed: {}.",
                        project_forests::forest_library_code_name(artifact.code));
                push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
                return false;
            }
            state.plantLabDocument = source.document;
            state.plantLabCompiled = artifact.artifact;
            state.plantLabPublishedArtifact = artifact.locator;
            state.plantLabCompiledRevision = source.document.revision;
            state.plantLabCompiledContentHash = source.document.contentHash;
            state.plantLabDocumentInitialized = true;
            state.plantLabAssetStatus = epochengine::format_text(
                "Reopened project Plant Lab revision {} from its verified Library artifact.",
                source.document.revision);
            if (reportMissing)
                push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
            return true;
        }

        [[nodiscard]] bool ensure_plant_lab_compiled_asset(EditorState& state)
        {
            if (!state.plantLabDocumentInitialized)
            {
                if (!reopen_plant_lab_project_asset(state, false))
                {
                    state.plantLabDocument = forest::make_default_plant_lab_document();
                    state.plantLabDocumentInitialized = true;
                }
            }

            if (state.plantLabCompiledRevision !=
                    state.plantLabDocument.revision
                || state.plantLabCompiledContentHash !=
                    state.plantLabDocument.contentHash)
            {
                state.plantLabCompiled =
                    forest::compile_forest_asset(state.plantLabDocument);
                state.plantLabCompiledRevision =
                    state.plantLabDocument.revision;
                state.plantLabCompiledContentHash =
                    state.plantLabDocument.contentHash;
            }

            if (!state.plantLabCompiled.valid)
            {
                state.plantLabStatus =
                    "Plant Lab compilation failed validation; the previous "
                    "scene projection was preserved.";
                return false;
            }

            state.plantLabStatus = epochengine::format_text(
                "Compiled revision {} into {} segments, {} leaves, and {} "
                "voxel LOD levels.",
                state.plantLabCompiled.sourceRevision,
                state.plantLabCompiled.preview.segmentCount,
                state.plantLabCompiled.preview.leafCount,
                state.plantLabCompiled.voxelLods.levels.size());
            return true;
        }
        [[nodiscard]] bool publish_plant_lab_project_asset(EditorState& state)
        {
            if (!ensure_plant_lab_compiled_asset(state))
            {
                state.plantLabAssetStatus =
                    "Plant Lab cannot publish an invalid compiled revision.";
                return false;
            }
            project_forests::ForestAssetLibrary library{
                state.projectId,
                resolve_editor_path(std::filesystem::path{state.projectRoot})};
            if (!library.valid())
            {
                state.plantLabAssetStatus =
                    "The active project's Forest asset library is invalid.";
                return false;
            }
            const auto publication = library.publish(
                kDefaultPlantLabLogicalPath,
                state.plantLabDocument);
            if (!publication)
            {
                state.plantLabAssetStatus = epochengine::format_text(
                    "Plant Lab publication failed: {}.",
                    project_forests::forest_library_code_name(publication.code));
                push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
                return false;
            }
            state.plantLabPublishedArtifact = publication.artifact;
            state.plantLabAssetStatus = epochengine::format_text(
                "{} project Plant Lab revision {} and verified its immutable Library artifact.",
                publication.code == project_forests::ForestLibraryCode::unchanged
                    ? "Reused"
                    : "Published",
                publication.artifact.source_revision);
            push_editor_log(
                state,
                epochengine::format_text(
                    "[plant] {} Source: {} | artifact: {}.",
                    state.plantLabAssetStatus,
                    display_project_path(publication.source.storage_path),
                    display_project_path(publication.artifact.storage_path)));
            return true;
        }
        [[nodiscard]] std::vector<EditorEntity>
            project_compiled_forest_entities(
                const forest::CompiledForestAsset& asset,
                std::string_view namePrefix,
                std::string_view category,
                bool editorOnly,
                float placementX = 0.0f,
                float placementZ = 0.0f)
        {
            std::vector<EditorEntity> entities{};
            if (!asset.valid)
                return entities;

            constexpr float previewScale = 0.58f;
            const auto scaledPosition =
                [=](const epochengine::voxel::Float3 value) noexcept
            {
                return std::array<float, 3>{
                    placementX + value.x * previewScale,
                    value.y * previewScale,
                    placementZ + value.z * previewScale};
            };
            const auto segmentLength = [](const auto& segment) noexcept
            {
                const float x = segment.end.x - segment.start.x;
                const float y = segment.end.y - segment.start.y;
                const float z = segment.end.z - segment.start.z;
                return std::sqrt(x * x + y * y + z * z);
            };

            const auto segmentRotation = [&](const auto& segment) noexcept
            {
                const float length = segmentLength(segment);
                if (length <= 1.0e-6f)
                    return std::array<float, 3>{};
                const float inverseLength = 1.0f / length;
                const float directionX = (segment.end.x - segment.start.x) * inverseLength;
                const float directionY = (segment.end.y - segment.start.y) * inverseLength;
                const float directionZ = (segment.end.z - segment.start.z) * inverseLength;
                constexpr float radiansToDegrees = 57.295779513082320876f;
                return std::array<float, 3>{
                    std::atan2(directionZ, directionY) * radiansToDegrees,
                    0.0f,
                    -std::asin((std::clamp)(directionX, -1.0f, 1.0f)) * radiansToDegrees
                };
            };
            const auto appendEntity = [&](
                std::string name,
                std::string_view type,
                std::array<float, 3> position,
                std::array<float, 3> rotation,
                std::array<float, 3> scale)
            {
                EditorSceneSeedEntity seed{
                    std::move(name),
                    std::string(type),
                    std::string(category),
                    position,
                    rotation,
                    scale,
                    true,
                    editorOnly};
                entities.push_back(editor_entity_from_seed(seed));
            };

            const auto& geometry = asset.preview;
            if (geometry.segmentCount > 0u)
            {
                const auto& trunk = geometry.segments[0u];
                const float height = (std::max)(
                    0.32f,
                    segmentLength(trunk) * previewScale);
                const float width = (std::max)(
                    0.12f,
                    trunk.radius * 2.8f * previewScale);
                appendEntity(
                    std::string(namePrefix) + "Trunk",
                    "ForestTrunk",
                    scaledPosition({
                        (trunk.start.x + trunk.end.x) * 0.5f,
                        (trunk.start.y + trunk.end.y) * 0.5f,
                        (trunk.start.z + trunk.end.z) * 0.5f
                    }),
                    segmentRotation(trunk),
                    {width, height, width});
            }

            const std::size_t branchBudget = (std::min)(
                geometry.segmentCount > 0u
                    ? geometry.segmentCount - 1u
                    : 0u,
                std::size_t{32u});
            const std::size_t branchStep = branchBudget > 0u
                ? (std::max)(
                    std::size_t{1u},
                    (geometry.segmentCount - 1u) / branchBudget)
                : 1u;
            std::size_t branchOrdinal{};
            for (std::size_t index = 1u;
                index < geometry.segmentCount
                    && branchOrdinal < branchBudget;
                index += branchStep, ++branchOrdinal)
            {
                const auto& segment = geometry.segments[index];
                const float size = (std::clamp)(
                    segment.radius * 2.1f * previewScale,
                    0.055f,
                    0.16f);
                const float length = (std::max)(
                    0.08f,
                    segmentLength(segment) * previewScale);
                appendEntity(
                    epochengine::format_text(
                        "{}Branch_{:02}",
                        namePrefix,
                        branchOrdinal),
                    "ForestBranchSegment",
                    scaledPosition({
                        (segment.start.x + segment.end.x) * 0.5f,
                        (segment.start.y + segment.end.y) * 0.5f,
                        (segment.start.z + segment.end.z) * 0.5f
                    }),
                    segmentRotation(segment),
                    {size, length, size});
            }

            const std::size_t canopyBudget = (std::min)(
                geometry.leafCount,
                std::size_t{28u});
            const std::size_t leafStep = canopyBudget > 0u
                ? (std::max)(
                    std::size_t{1u},
                    geometry.leafCount / canopyBudget)
                : 1u;
            std::size_t canopyOrdinal{};
            for (std::size_t index = 0u;
                index < geometry.leafCount
                    && canopyOrdinal < canopyBudget;
                index += leafStep, ++canopyOrdinal)
            {
                const auto& leaf = geometry.leaves[index];
                const float size = (std::clamp)(
                    leaf.size * 1.15f * previewScale,
                    0.10f,
                    0.24f);
                appendEntity(
                    epochengine::format_text(
                        "{}Canopy_{:02}",
                        namePrefix,
                        canopyOrdinal),
                    "ForestFoliageCluster",
                    scaledPosition(leaf.position),
                    { 0.0f, static_cast<float>((canopyOrdinal * 137u) % 360u), 0.0f },
                    {size, size * 0.62f, size});
            }
            return entities;
        }
        [[nodiscard]] bool ensure_forest_factory_preview(EditorState& state)
        {
            if (!ensure_plant_lab_compiled_asset(state)) return false;
            if (state.forestPreviewRevision == state.plantLabCompiled.sourceRevision
                && state.forestPreviewContentHash == state.plantLabCompiled.sourceContentHash
                && !state.forestPreviewEntities.empty()) return true;
            auto desired = project_compiled_forest_entities(
                state.plantLabCompiled, "ForestPreview", "PlantLabPreview", true);
            if (desired.empty() || !align_editor_entity_group_to_surface(desired, 0.0f))
                return false;
            // Preview residency is deliberately outside the project's entities
            // and authoring journal. Only Add to Scene performs a transaction.
            state.forestPreviewEntities = std::move(desired);
            state.forestPreviewRevision = state.plantLabCompiled.sourceRevision;
            state.forestPreviewContentHash = state.plantLabCompiled.sourceContentHash;
            return true;
        }

        void focus_forest_factory_preview(const core::Context* ctx, EditorState& state)
        {
            if (!ctx || !ensure_forest_factory_preview(state)) return;
            const float height = (std::max)(1.0f,
                state.plantLabDocument.profile.config.targetHeightMeters * 0.58f);
            (void)previewgrid::focus_camera(ctx, {0.0f, height * 0.5f, 0.0f}, height);
        }

        void ensure_plant_lab_preview_entities(EditorState& state)
        {
            std::string previousSelection{};
            if (!state.entities.empty())
            {
                const std::size_t selectedIndex = (std::min)(
                    state.selectedEntity,
                    state.entities.size() - 1u);
                if (state.entities[selectedIndex].category !=
                    "PlantLabPreview")
                {
                    previousSelection =
                        state.entities[selectedIndex].name;
                }
            }

            if (!ensure_plant_lab_compiled_asset(state))
            {
                push_editor_log(
                    state,
                    "[plant] " + state.plantLabStatus);
                return;
            }

            auto desired = project_compiled_forest_entities(
                state.plantLabCompiled,
                "PlantLab",
                "PlantLabPreview",
                true);
            if (!align_editor_entity_group_to_surface(
                    desired,
                    primary_ground_top_y(state)))
            {
                push_editor_log(
                    state,
                    "[plant] Preview projection rejected invalid surface bounds.");
                return;
            }
            const std::size_t generatedCount = desired.size();
            if (synchronize_editor_scene_category(
                    state,
                    "PlantLabPreview",
                    std::move(desired),
                    "Regenerate Plant Lab preview",
                    previousSelection))
            {
                push_editor_log(
                    state,
                    epochengine::format_text(
                        "[plant] Projected compiled tree revision {} as {} "
                        "renderer-neutral preview entities.",
                        state.plantLabCompiled.sourceRevision,
                        generatedCount));
            }
        }

        [[nodiscard]] std::size_t place_forest_factory_asset(
            EditorState& state)
        {
            if (state.projectId.empty() || state.projectRoot.empty()
                || !state.projectLifecycle.evidence().project_selected)
            {
                state.plantLabStatus = "Open a project before placing a Forest Factory asset.";
                push_editor_log(state, "[forest] " + state.plantLabStatus);
                return 0u;
            }
            if (!ensure_plant_lab_compiled_asset(state))
            {
                push_editor_log(
                    state,
                    "[forest] Placement failed: "
                        + state.plantLabStatus);
                return 0u;
            }

            const std::size_t assetOrdinal =
                static_cast<std::size_t>(std::count_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [](const EditorEntity& entity)
                    {
                        return entity.type == "ForestTrunk"
                            && entity.name.rfind(
                                "ForestAsset_",
                                0u) == 0u;
                    }));

            const float placementX =
                (static_cast<float>(assetOrdinal % 4u) - 1.5f)
                    * 3.2f;
            const float placementZ =
                static_cast<float>(assetOrdinal / 4u) * 3.2f;
            auto placed = project_compiled_forest_entities(
                state.plantLabCompiled,
                epochengine::format_text(
                    "ForestAsset_{:02}_",
                    assetOrdinal),
                "ForestFactory",
                false,
                placementX,
                placementZ);
            if (!align_editor_entity_group_to_surface(
                    placed,
                    primary_ground_top_y(state)))
            {
                push_editor_log(
                    state,
                    "[forest] Placement failed: projected surface bounds are invalid.");
                return 0u;
            }
            const std::size_t placedCount = placed.size();
            if (placedCount == 0u)
            {
                push_editor_log(
                    state,
                    "[forest] Placement failed: the compiled Plant Lab "
                    "asset contains no projectable scene objects.");
                return 0u;
            }
            if (!create_editor_scene_entities(
                    state,
                    std::move(placed),
                    epochengine::format_text(
                        "Place Forest Factory asset {:02}",
                        assetOrdinal)))
            {
                push_editor_log(
                    state,
                    "[forest] Placement failed: scene transaction "
                    "was rejected.");
                return 0u;
            }

            state.surfaceSettleFrames =
                (std::max)(state.surfaceSettleFrames, 2);
            state.projectStatus = epochengine::format_text(
                "Placed Forest Factory asset {:02} from Plant Lab "
                "revision {} with {} scene objects and {} voxel LOD "
                "levels.",
                assetOrdinal,
                state.plantLabCompiled.sourceRevision,
                placedCount,
                state.plantLabCompiled.voxelLods.levels.size());
            push_editor_log(
                state,
                epochengine::format_text(
                    "[forest] Placed Plant Lab revision {} as "
                    "ForestAsset_{:02}; {} persistent objects share "
                    "content hash {:016x}.",
                    state.plantLabCompiled.sourceRevision,
                    assetOrdinal,
                    placedCount,
                    state.plantLabCompiled.sourceContentHash));
            return placedCount;
        }
        [[nodiscard]] bool is_forest_factory_entity(const EditorEntity& entity) noexcept
        {
            return entity.category == "ForestFactory";
        }
        [[nodiscard]] bool is_plant_lab_preview_entity(const EditorEntity& entity) noexcept
        {
            return entity.category == "PlantLabPreview";
        }

        [[nodiscard]] bool is_vegetation_entity(const EditorEntity& entity) noexcept
        {
            return is_forest_factory_entity(entity) || is_plant_lab_preview_entity(entity);
        }

        [[nodiscard]] bool is_selected_vegetation_entity(const EditorEntity& entity, bool selected) noexcept
        {
            return selected && is_vegetation_entity(entity);
        }

        [[nodiscard]] epochengine::previewgrid::Vec3 vegetation_color_for_entity(
            const EditorEntity& entity) noexcept
        {
            if (entity.type == "ForestTrunk")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::forest_trunk());
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::forest_branch_joint());
            if (entity.type == "ForestFoliageCluster")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::forest_foliage_cluster());
            return epochengine::previewgrid::visual_rgb(epochengine::visuals::forest_default());
        }

        [[nodiscard]] float vegetation_radius_for_entity(const EditorEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(entity.scale[0], (std::max)(entity.scale[1], entity.scale[2]));
            if (entity.type == "ForestTrunk")
                return (std::clamp)(0.22f * scaleMax, 0.16f, 0.34f);
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return (std::clamp)(0.34f * scaleMax, 0.16f, 0.24f);
            if (entity.type == "ForestFoliageCluster")
                return (std::clamp)(0.38f * scaleMax, 0.16f, 0.28f);
            return (std::clamp)(0.34f * scaleMax, 0.16f, 0.36f);
        }

        [[nodiscard]] epochengine::previewgrid::Vec3 marker_color_for_entity(
            const EditorEntity& entity,
            bool selected) noexcept
        {
            if (is_selected_vegetation_entity(entity, selected))
                return vegetation_color_for_entity(entity);
            if (selected)
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_selected());
            if (entity.type == "Light")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_light());
            if (entity.type == "Spawn")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_spawn());
            if (entity.type == "Camera")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_camera());
            if (is_vegetation_entity(entity))
                return vegetation_color_for_entity(entity);
            if (entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kScreenSceneNode.name)
                return { 0.08f, 0.92f, 0.64f };
            if (entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kCabinetBodySceneNode.name)
                return { 0.16f, 0.30f, 0.58f };
            if (entity.category == "World" || entity.type == "Level")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_world());
            if (entity.editorOnly || entity.category == "Editor")
                return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_editor_helper());
            return epochengine::previewgrid::visual_rgb(epochengine::visuals::object_default());
        }

        [[nodiscard]] float marker_radius_for_entity(const EditorEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(entity.scale[0], (std::max)(entity.scale[1], entity.scale[2]));
            if (entity.type == "Light")
                return 0.42f;
            if (entity.type == "Spawn")
                return 0.32f;
            if (entity.type == "Camera")
                return 0.38f;
            if (entity.category == "World" || entity.type == "Level")
                return 0.75f;
            if (is_vegetation_entity(entity))
                return vegetation_radius_for_entity(entity);
            return (std::clamp)(0.34f * scaleMax, 0.24f, 1.20f);
        }

        [[nodiscard]] epochengine::previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(
            const EditorEntity& entity) noexcept
        {
            if (entity.type == "Light")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Light;
            if (entity.type == "Spawn")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Spawn;
            if (entity.type == "Camera")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Camera;
            if (entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kCabinetBodySceneNode.name)
                return epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeCabinet;
            if (entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kScreenSceneNode.name)
                return epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen;
            if (entity.type == "Canvas2D")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Canvas2D;
            if (entity.type == "ForestTrunk")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestTrunk;
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestBranch;
            if (entity.type == "ForestFoliageCluster")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestLeafCluster;
            if (entity.category == "World" || entity.type == "Level")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Level;
            return epochengine::previewgrid::ObjectPreviewPrimitive::Cube;
        }

        [[nodiscard]] epochengine::previewgrid::Vec3 selection_color_for_entity(
            const EditorEntity& entity,
            bool selected) noexcept
        {
            if (is_selected_vegetation_entity(entity, selected))
                return vegetation_color_for_entity(entity);
            return marker_color_for_entity(entity, selected);
        }

        [[nodiscard]] bool marker_selected_for_entity(const EditorEntity& entity, bool selected) noexcept
        {
            return selected;
        }

        [[nodiscard]] std::size_t visible_entity_count(const EditorState& state) noexcept
        {
            std::size_t count = 0;
            for (const auto& entity : state.entities)
                if (entity.visible)
                    ++count;
            return count;
        }

        [[nodiscard]] epochengine::lighting::Vec3 light_direction_from_editor_rotation(
            const std::array<float, 3>& rotation) noexcept
        {
            return epochengine::lighting::direction_from_euler_degrees({
                rotation[0],
                rotation[1],
                rotation[2]
            });
        }
        void rebuild_editor_lighting(EditorState& state)
        {
            if (!state.sceneLightingDirty)
                return;

            state.sceneLighting = epochengine::lighting::LightManager{ 128 };
            state.sceneLighting.set_environment({ { 0.16f, 0.18f, 0.22f } });
            state.sceneLightHandles.assign(state.entities.size(), {});

            for (std::size_t index = 0; index < state.entities.size(); ++index)
            {
                const EditorEntity& entity = state.entities[index];
                if (!entity.visible || entity.type != "Light")
                    continue;

                const bool pointLight = entity.name.contains("Point") || entity.name.contains("Lamp");
                epochengine::lighting::LightDesc desc{};
                desc.kind = pointLight
                    ? epochengine::lighting::LightKind::Point
                    : epochengine::lighting::LightKind::Directional;
                desc.color = pointLight
                    ? epochengine::lighting::Color3{ 1.0f, 0.78f, 0.56f }
                    : epochengine::lighting::Color3{ 1.0f, 0.95f, 0.84f };
                desc.intensity = pointLight ? 24.0f : 1.35f;
                desc.position = { entity.position[0], entity.position[1], entity.position[2] };
                desc.direction = light_direction_from_editor_rotation(entity.rotation);
                desc.range = pointLight
                    ? (std::max)(12.0f, (std::max)({ entity.scale[0], entity.scale[1], entity.scale[2] }) * 12.0f)
                    : 1.0f;
                state.sceneLightHandles[index] = state.sceneLighting.create(desc);
            }

            state.sceneLightingDirty = false;
        }

        [[nodiscard]] FilterMode canvas2d_filter(
            scene::SceneTextureFilter filter) noexcept
        {
            return filter == scene::SceneTextureFilter::nearest
                ? FilterMode::nearest
                : FilterMode::linear;
        }

        [[nodiscard]] AddressMode canvas2d_address(
            scene::SceneTextureAddress address) noexcept
        {
            switch (address)
            {
            case scene::SceneTextureAddress::repeat:
                return AddressMode::repeat;
            case scene::SceneTextureAddress::mirrored_repeat:
                return AddressMode::mirrored_repeat;
            case scene::SceneTextureAddress::clamp_to_edge:
                return AddressMode::clamp_to_edge;
            }
            return AddressMode::clamp_to_edge;
        }

        [[nodiscard]] canvas2d::SpriteAlphaMode canvas2d_alpha(
            scene::SceneTextureAlphaMode alpha) noexcept
        {
            switch (alpha)
            {
            case scene::SceneTextureAlphaMode::opaque:
                return canvas2d::SpriteAlphaMode::opaque;
            case scene::SceneTextureAlphaMode::straight:
                return canvas2d::SpriteAlphaMode::straight;
            case scene::SceneTextureAlphaMode::premultiplied:
                return canvas2d::SpriteAlphaMode::premultiplied;
            case scene::SceneTextureAlphaMode::cutout:
                return canvas2d::SpriteAlphaMode::mask;
            }
            return canvas2d::SpriteAlphaMode::opaque;
        }

        [[nodiscard]] canvas2d::SpriteColorSpace canvas2d_color_space(
            scene::SceneTextureColorSpace colorSpace) noexcept
        {
            return colorSpace == scene::SceneTextureColorSpace::srgb
                ? canvas2d::SpriteColorSpace::srgb
                : canvas2d::SpriteColorSpace::linear;
        }

        [[nodiscard]] std::uint32_t canvas2d_material_key(
            std::uint64_t stableKey) noexcept
        {
            std::uint32_t result =
                static_cast<std::uint32_t>(stableKey)
                ^ static_cast<std::uint32_t>(stableKey >> 32u);
            return result == 0u ? 1u : result;
        }

        [[nodiscard]] editor_canvas2d::MaterialView
            canvas2d_material_view(
                const scene::SceneTextureMaterialSnapshot& material,
                std::span<const
                    editor_project_textures::ResolvedTextureMaterial>
                    resolved) noexcept
        {
            const auto found = std::find_if(
                resolved.begin(),
                resolved.end(),
                [&](const auto& candidate)
                {
                    return candidate.material.logical_path
                            == material.logical_path
                        && candidate.material.artifact_key
                            == material.artifact_key;
                });
            if (found == resolved.end())
                return {};
            const FilterMode filter =
                canvas2d_filter(material.filter);
            return {
                .stable_key =
                    canvas2d_material_key(material.stable_key),
                .logical_texture = found->logical,
                .sampler = {
                    .min_filter = filter,
                    .mag_filter = filter,
                    .address_u =
                        canvas2d_address(material.address_u),
                    .address_v =
                        canvas2d_address(material.address_v),
                    .maximum_anisotropy = 1.0f,
                    .mipmapped = false
                },
                .alpha = canvas2d_alpha(material.alpha),
                .color_space =
                    canvas2d_color_space(material.color_space),
                .alpha_cutoff = material.alpha_cutoff
            };
        }


        void publish_editor_canvas2d_scene(
            const core::Context* ctx,
            EditorState& state) noexcept
        {
            if (!ctx || state.previewMode != core::ScenePreviewMode::Editor
                || state.viewportCameraMode != previewgrid::CameraMode::Canvas2D)
            {
                (void)canvas2d::scene_content::retire(ctx);
                return;
            }

            try
            {
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
                editor_tilemaps::RuntimePreviewResult tileMapPreview{};
                if (!state.game2DGuiWorkspace
                    && state.tileMapWorkspace
                    && state.tileMapWorkspace->has_document())
                {
                    const double milliseconds = (std::clamp)(
                        state.timeSnapshot.simulated_seconds * 1'000.0,
                        0.0,
                        static_cast<double>((std::numeric_limits<std::uint64_t>::max)()));
                    tileMapPreview =
                        state.tileMapWorkspace->compile_runtime_preview(
                            static_cast<std::uint64_t>(milliseconds));
                    state.tileMapStatus = std::string(
                        state.tileMapWorkspace->status());
                }
#endif
                authoring::gui::GuiDocument* guiDocument =
                    synchronize_gui_document(state);
                std::vector<scene::SceneTextureMaterialSnapshot>
                    sourceMaterials{};
                for (const EditorEntity& entity : state.entities)
                {
                    if (entity.textureMaterial)
                        sourceMaterials.push_back(*entity.textureMaterial);
                }
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
                if (tileMapPreview)
                {
                    sourceMaterials.reserve(
                        sourceMaterials.size()
                        + tileMapPreview.artifact->dependencies.size());
                    for (const auto& dependency :
                         tileMapPreview.artifact->dependencies)
                    {
                        sourceMaterials.push_back(
                            scene::SceneTextureMaterialSnapshot{
                                .logical_path = dependency.logical_path,
                                .artifact_key = dependency.artifact_key,
                                .stable_key = dependency.stable_material_key,
                                .filter = scene::SceneTextureFilter::nearest,
                                .address_u = scene::SceneTextureAddress::clamp_to_edge,
                                .address_v = scene::SceneTextureAddress::clamp_to_edge,
                                .alpha = scene::SceneTextureAlphaMode::premultiplied,
                                .color_space = scene::SceneTextureColorSpace::srgb,
                                .alpha_cutoff = dependency.alpha_cutoff});
                    }
                }
#endif

                editor_project_textures::SceneTextureLeaseResult
                    textureLease{};
                if (!sourceMaterials.empty())
                {
                    auto* controller =
                        project_texture_controller(state);
                    if (!controller)
                    {
                        (void)canvas2d::scene_content::retire(ctx);
                        return;
                    }
                    textureLease =
                        controller->bind_scene_materials(sourceMaterials);
                    if (!textureLease)
                    {
                        state.projectTextureStatus =
                            epochengine::format_text(
                                "Scene texture binding refused: {} | pipeline {} | resource {}.",
                                editor_project_textures::controller_code_name(
                                    textureLease.code),
                                project_textures::pipeline_code_name(
                                    textureLease.pipeline_code),
                                project_textures::resource_code_name(
                                    textureLease.resource_code));
                        (void)canvas2d::scene_content::retire(ctx);
                        return;
                    }
                }


                std::vector<editor_canvas2d::EntityView> entities{};
                entities.reserve(state.entities.size());
                for (std::size_t index = 0; index < state.entities.size(); ++index)
                {
                    const EditorEntity& entity = state.entities[index];
                    editor_canvas2d::GuiPresentationView guiPresentation{};
                    if (guiDocument)
                    {
                        const auto mapped = state.guiDocumentWidgets.find(
                            entity.sceneObjectId);
                        if (mapped != state.guiDocumentWidgets.end())
                        {
                            const auto widget =
                                guiDocument->widget(mapped->second);
                            if (widget)
                            {
                                const auto& style = widget->descriptor.style;
                                const auto& interaction =
                                    widget->descriptor.interaction;
                                guiPresentation = {
                                    .background = {
                                        style.background.red,
                                        style.background.green,
                                        style.background.blue,
                                        style.background.alpha},
                                    .foreground = {
                                        style.foreground.red,
                                        style.foreground.green,
                                        style.foreground.blue,
                                        style.foreground.alpha},
                                    .border = {
                                        style.border.red,
                                        style.border.green,
                                        style.border.blue,
                                        style.border.alpha},
                                    .accent = {
                                        style.accent.red,
                                        style.accent.green,
                                        style.accent.blue,
                                        style.accent.alpha},
                                    .border_width = style.border_width,
                                    .corner_radius = style.corner_radius,
                                    .opacity = style.opacity,
                                    .authored = true,
                                    .visible = interaction.visible,
                                    .enabled = interaction.enabled,
                                    .focusable = interaction.focusable,
                                    .accepts_pointer =
                                        interaction.accepts_pointer
                                };
                            }
                        }
                    }
                    entities.push_back(editor_canvas2d::EntityView{
                        .stable_id = entity.sceneObjectId,
                        .generation = 1,
                        .type = entity.type,
                        .category = entity.category,
                        .position = entity.position,
                        .rotation = entity.rotation,
                        .scale = entity.scale,
                        .visible = entity.visible
                            && (!guiPresentation.authored
                                || guiPresentation.visible),
                        .editor_only = entity.editorOnly,
                        .selected = editor_entity_selected(state, entity.sceneObjectId),
                        .material = entity.textureMaterial
                            ? canvas2d_material_view(
                                *entity.textureMaterial,
                                textureLease.materials)
                            : editor_canvas2d::MaterialView{},
                        .gui = guiPresentation});
                }

                auto built = editor_canvas2d::build_scene(
                    editor_canvas2d::BuildRequest{
                        .project = canvas2d_runtime_settings(state.canvas2dProject),
                        .entities = entities,
                        .resources = textureLease.lease,
                        .source_revision = (std::max)(
                            std::uint64_t{1},
                            state.sceneDocumentRevision),
                        .include_helpers = state.helpersVisible});
                if (!built)
                {
                    (void)canvas2d::scene_content::retire(ctx);
                    return;
                }
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
                if (tileMapPreview)
                {
                    const std::uint64_t maximumSprites =
                        static_cast<std::uint64_t>(
                            built.content.project.maximum_sprites_per_batch)
                        * built.content.project.maximum_batches;
                    if (tileMapPreview.visible.sprites.size()
                        > maximumSprites - built.content.sprites.size())
                    {
                        state.tileMapStatus =
                            "Tile-map preview exceeded the Canvas2D sprite budget.";
                        (void)canvas2d::scene_content::retire(ctx);
                        return;
                    }
                    built.content.sprites.reserve(
                        built.content.sprites.size()
                        + tileMapPreview.visible.sprites.size());
                    built.content.sprites.insert(
                        built.content.sprites.end(),
                        std::make_move_iterator(
                            tileMapPreview.visible.sprites.begin()),
                        std::make_move_iterator(
                            tileMapPreview.visible.sprites.end()));
                    built.content.source_revision = (std::max)(
                        built.content.source_revision,
                        tileMapPreview.artifact->identity.source_revision.sequence);
                    if (!built.content.valid())
                    {
                        state.tileMapStatus =
                            "Tile-map preview failed Canvas2D resource validation.";
                        (void)canvas2d::scene_content::retire(ctx);
                        return;
                    }
                }
#endif
                (void)canvas2d::scene_content::publish(
                    ctx,
                    std::move(built.content));
            }
            catch (...)
            {
                (void)canvas2d::scene_content::retire(ctx);
            }
        }

        void publish_editor_preview_markers(const core::Context* ctx, EditorState& state)
        {
            if (!ctx || state.previewMode != core::ScenePreviewMode::Editor)
            {
                epochengine::previewgrid::clear_object_markers(ctx);
                epochengine::previewgrid::clear_lighting_frame(ctx);
                return;
            }

            rebuild_editor_lighting(state);
            epochengine::previewgrid::set_lighting_frame(ctx, state.sceneLighting.build_frame());

            std::vector<epochengine::previewgrid::ObjectMarker> markers{};
            const bool forestPreview = state.mainSurface == EditorMainSurface::ForestFactory;
            if (forestPreview) (void)ensure_forest_factory_preview(state);
            const auto& previewEntities = forestPreview ? state.forestPreviewEntities : state.entities;
            markers.reserve(previewEntities.size());
            for (std::size_t i = 0; i < previewEntities.size(); ++i)
            {
                const auto& entity = previewEntities[i];
                if (!entity.visible || is_screen_space_gui_entity(entity))
                    continue;

                std::array<float, 3> previewPosition = entity.position;
                const auto groupMotion = state.scriptCubeOscillators.find(entity.sceneObjectId);
                const auto* motion = forestPreview ? nullptr : groupMotion != state.scriptCubeOscillators.end()
                    ? &groupMotion->second
                    : state.scriptOscillator.enabled && entity.sceneObjectId == state.scriptOscillator.entity_id
                        ? &state.scriptOscillator : nullptr;
                if (motion && motion->enabled)
                {
                    constexpr double tau = 6.283185307179586476925286766559;
                    const double phase = tau
                        * static_cast<double>(motion->frequency_hz)
                        * state.timeSnapshot.simulated_seconds + motion->phase_radians;
                    const auto axis = static_cast<std::size_t>(motion->axis);
                    previewPosition = motion->base_position;
                    previewPosition[axis] += motion->amplitude
                        * static_cast<float>(std::sin(phase));
                }

                const bool selected = !forestPreview && editor_entity_selected(
                    state, entity.sceneObjectId);
                markers.push_back(epochengine::previewgrid::ObjectMarker{
                    .position{
                        previewPosition[0],
                        previewPosition[1],
                        previewPosition[2]
                    },
                    .color = selection_color_for_entity(entity, selected),
                    .scale{
                        entity.scale[0],
                        entity.scale[1],
                        entity.scale[2]
                    },
                    .rotationDegrees{
                        entity.rotation[0],
                        entity.rotation[1],
                        entity.rotation[2]
                    },
                    .radius = marker_radius_for_entity(entity),
                    .primitive = preview_primitive_for_entity(entity),
                    .selected = marker_selected_for_entity(entity, selected),
                    .editorOnly = entity.editorOnly || entity.category == "Editor" || is_plant_lab_preview_entity(entity),
                    .sampledRenderSurface = entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kScreenSceneNode.name
                });
            }

            epochengine::previewgrid::set_object_markers(ctx, std::move(markers));
        }

        void duplicate_selected_entity(EditorState& state)
        {
            const auto selected = editor_entity_index(
                state, state.selectedEntityId);
            if (!selected)
            {
                push_editor_log(state, "[entity] Nothing selected to duplicate.");
                return;
            }

            const std::size_t selectedIndex = *selected;
            EditorEntity duplicate = state.entities[selectedIndex];
            duplicate.name = make_entity_name(state, duplicate.name + "_Copy");
            duplicate.position[0] += 0.85f;
            duplicate.position[2] -= 0.55f;
            if (duplicate.type == "StaticMesh"
                && !align_editor_entity_bottom_to_surface(
                    duplicate,
                    primary_ground_top_y(state)))
            {
                push_editor_log(
                    state,
                    "[entity] Duplicate rejected invalid surface bounds.");
                return;
            }
            duplicate.sceneObjectId = scene::kInvalidSceneObjectId;
            const std::string sourceName = state.entities[selectedIndex].name;
            if (!create_editor_scene_entity(
                    state,
                    std::move(duplicate),
                    "Duplicate " + sourceName))
            {
                push_editor_log(state, "[entity] Could not duplicate " + sourceName + ".");
                return;
            }
            push_editor_log(state, std::string("[entity] Duplicated ") + sourceName + ".");
        }

        void delete_selected_entity(EditorState& state)
        {
            const auto selected = editor_entity_index(
                state, state.selectedEntityId);
            if (!selected)
            {
                push_editor_log(state, "[entity] Nothing selected to delete.");
                return;
            }

            const std::size_t selectedIndex = *selected;
            if (is_gui_authoring_entity(state.entities[selectedIndex]))
            {
                if (auto* document = synchronize_gui_document(state))
                {
                    (void)remove_selected_gui_widget(state, *document);
                    return;
                }
            }
            const std::string name = state.entities[selectedIndex].name;
            const scene::SceneObjectId id = state.entities[selectedIndex].sceneObjectId;
            if (!destroy_editor_scene_entity(state, id, "Delete " + name))
            {
                push_editor_log(state, "[entity] Could not delete " + name + ".");
                return;
            }
            push_editor_log(state, std::string("[entity] Deleted ") + name + ".");
        }

        enum class SurfaceEditAction : std::uint8_t
