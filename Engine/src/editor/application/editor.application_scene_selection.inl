// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::string_view editor_scene_identity(const EditorState& state) noexcept
        {
            if (!state.activeRuntimeScene.empty())
                return state.activeRuntimeScene;
            if (!state.projectId.empty())
                return state.projectId;
            if (!state.activeWorld.empty())
                return state.activeWorld;
            return "epoch.editor.scene";
        }

        [[nodiscard]] std::optional<std::size_t> editor_entity_index(
            const EditorState& state,
            scene::SceneObjectId id) noexcept
        {
            if (id == scene::kInvalidSceneObjectId)
                return std::nullopt;

            const auto found = std::find_if(
                state.entities.begin(),
                state.entities.end(),
                [id](const EditorEntity& entity)
                {
                    return entity.sceneObjectId == id;
                });
            if (found == state.entities.end())
                return std::nullopt;
            return static_cast<std::size_t>(std::distance(state.entities.begin(), found));
        }

        void normalize_editor_entity_ids(EditorState& state)
        {
            std::unordered_set<scene::SceneObjectId> occupied{};
            occupied.reserve(state.entities.size());
            const std::string sceneIdentity{ editor_scene_identity(state) };

            for (std::size_t index = 0; index < state.entities.size(); ++index)
            {
                EditorEntity& entity = state.entities[index];
                scene::SceneObjectId candidate = entity.sceneObjectId;
                if (candidate == scene::kInvalidSceneObjectId || occupied.contains(candidate))
                {
                    candidate = scene::stable_scene_object_id(sceneIdentity, entity.name);
                    std::uint64_t salt = static_cast<std::uint64_t>(index) + 1u;
                    while (candidate == scene::kInvalidSceneObjectId || occupied.contains(candidate))
                    {
                        candidate ^= 0x9e3779b97f4a7c15ull
                            + salt
                            + (candidate << 6u)
                            + (candidate >> 2u);
                        ++salt;
                    }
                    entity.sceneObjectId = candidate;
                }
                occupied.insert(entity.sceneObjectId);
            }
        }

        void clear_editor_selection(EditorState& state) noexcept
        {
            state.selectedEntity = 0u;
            state.selectedEntityId = scene::kInvalidSceneObjectId;
            state.selectedEntityIds.clear();
            state.sceneDragActive = false;
            state.sceneDragEntity = 0u;
            state.sceneDragEntityId = scene::kInvalidSceneObjectId;
            state.sceneDragStartPositions.clear();
            state.sceneDragHasPlaneHit = false;
            state.sceneMarqueeActive = false;
            state.guiContentEditorEntityId = scene::kInvalidSceneObjectId;
            state.textureBrushActive = false;
        }

        [[nodiscard]] bool editor_entity_selected(
            const EditorState& state,
            scene::SceneObjectId id) noexcept
        {
            if (id == scene::kInvalidSceneObjectId)
                return false;
            if (state.selectedEntityIds.empty())
                return id == state.selectedEntityId
                    && editor_entity_index(state, id).has_value();
            return std::find(
                    state.selectedEntityIds.begin(),
                    state.selectedEntityIds.end(),
                    id) != state.selectedEntityIds.end();
        }

        [[nodiscard]] bool editor_has_selection(const EditorState& state) noexcept
        {
            return editor_entity_index(
                state, state.selectedEntityId).has_value();
        }

        void apply_editor_primary_selection_side_effects(EditorState& state) noexcept
        {
            const auto selectedIndex = editor_entity_index(state, state.selectedEntityId);
            if (!selectedIndex)
                return;
            state.selectedEntity = *selectedIndex;
            EditorEntity& selected = state.entities[state.selectedEntity];
            if (selected.textureMaterial)
            {
                const auto& material = *selected.textureMaterial;
                state.textureFilter = material.filter;
                state.textureAddressU = material.address_u;
                state.textureAddressV = material.address_v;
                state.textureAlpha = material.alpha;
                state.textureColorSpace = material.color_space;
                state.textureAlphaCutoff = material.alpha_cutoff;
                state.selectedAssetPath = material.logical_path;
                if (state.projectTextures
                    && state.projectTextures->select(material.logical_path))
                {
                    if (const auto* texture = state.projectTextures->selected())
                    {
                        state.selectedProjectFile =
                            texture->source_path.generic_string();
                    }
                }
            }
        }

        void select_editor_entity(EditorState& state, std::size_t index) noexcept
        {
            if (state.entities.empty())
            {
                clear_editor_selection(state);
                return;
            }

            state.selectedEntity = (std::min)(index, state.entities.size() - 1u);
            EditorEntity& selected = state.entities[state.selectedEntity];
            state.selectedEntityId = selected.sceneObjectId;
            state.selectedEntityIds.assign(1u, selected.sceneObjectId);
            if (selected.textureMaterial)
            {
                const auto& material = *selected.textureMaterial;
                state.textureFilter = material.filter;
                state.textureAddressU = material.address_u;
                state.textureAddressV = material.address_v;
                state.textureAlpha = material.alpha;
                state.textureColorSpace = material.color_space;
                state.textureAlphaCutoff = material.alpha_cutoff;
                state.selectedAssetPath = material.logical_path;
                if (state.projectTextures
                    && state.projectTextures->select(material.logical_path))
                {
                    if (const auto* texture = state.projectTextures->selected())
                    {
                        state.selectedProjectFile =
                            texture->source_path.generic_string();
                    }
                }
            }
        }

        void add_editor_entity_selection(EditorState& state, std::size_t index) noexcept
        {
            if (state.entities.empty())
                return;
            if (state.selectedEntityIds.empty()
                && state.selectedEntityId != scene::kInvalidSceneObjectId
                && editor_entity_index(state, state.selectedEntityId))
            {
                state.selectedEntityIds.push_back(state.selectedEntityId);
            }
            index = (std::min)(index, state.entities.size() - 1u);
            const scene::SceneObjectId id = state.entities[index].sceneObjectId;
            if (!editor_entity_selected(state, id))
                state.selectedEntityIds.push_back(id);
            state.selectedEntity = index;
            state.selectedEntityId = id;
            apply_editor_primary_selection_side_effects(state);
        }

        void toggle_editor_entity_selection(EditorState& state, std::size_t index) noexcept
        {
            if (state.entities.empty())
                return;
            if (state.selectedEntityIds.empty()
                && state.selectedEntityId != scene::kInvalidSceneObjectId
                && editor_entity_index(state, state.selectedEntityId))
            {
                state.selectedEntityIds.push_back(state.selectedEntityId);
            }
            index = (std::min)(index, state.entities.size() - 1u);
            const scene::SceneObjectId id = state.entities[index].sceneObjectId;
            const auto found = std::find(
                state.selectedEntityIds.begin(),
                state.selectedEntityIds.end(),
                id);
            if (found == state.selectedEntityIds.end())
            {
                add_editor_entity_selection(state, index);
                return;
            }

            state.selectedEntityIds.erase(found);
            if (state.selectedEntityIds.empty())
            {
                clear_editor_selection(state);
                return;
            }
            if (state.selectedEntityId == id)
                state.selectedEntityId = state.selectedEntityIds.back();
            apply_editor_primary_selection_side_effects(state);
        }

        void synchronize_editor_selection(EditorState& state)
        {
            normalize_editor_entity_ids(state);
            if (state.entities.empty())
            {
                select_editor_entity(state, 0u);
                state.sceneDragEntity = 0u;
                state.sceneDragEntityId = scene::kInvalidSceneObjectId;
                return;
            }

            state.selectedEntityIds.erase(
                std::remove_if(
                    state.selectedEntityIds.begin(),
                    state.selectedEntityIds.end(),
                    [&](scene::SceneObjectId id)
                    {
                        return !editor_entity_index(state, id).has_value();
                    }),
                state.selectedEntityIds.end());
            std::sort(state.selectedEntityIds.begin(), state.selectedEntityIds.end());
            state.selectedEntityIds.erase(
                std::unique(state.selectedEntityIds.begin(), state.selectedEntityIds.end()),
                state.selectedEntityIds.end());

            if (state.selectedEntityId != scene::kInvalidSceneObjectId
                && editor_entity_index(state, state.selectedEntityId)
                && !editor_entity_selected(state, state.selectedEntityId))
            {
                state.selectedEntityIds.push_back(state.selectedEntityId);
            }

            if (state.selectedEntityIds.empty())
            {
                state.selectedEntity = 0u;
                state.selectedEntityId = scene::kInvalidSceneObjectId;
            }
            else
            {
                if (!editor_entity_selected(state, state.selectedEntityId))
                    state.selectedEntityId = state.selectedEntityIds.back();
                if (const auto selected = editor_entity_index(state, state.selectedEntityId))
                    state.selectedEntity = *selected;
                apply_editor_primary_selection_side_effects(state);
            }

            if (state.sceneDragActive)
            {
                if (const auto dragged = editor_entity_index(state, state.sceneDragEntityId))
                    state.sceneDragEntity = *dragged;
                else
                {
                    state.sceneDragActive = false;
                    state.sceneDragEntity = 0u;
                    state.sceneDragEntityId = scene::kInvalidSceneObjectId;
                }
            }
        }

        [[nodiscard]] scene::SceneSnapshot capture_editor_entity_projection(const EditorState& state)
        {
            scene::SceneSnapshot snapshot{};
            if (state.sceneDocument.initialized())
            {
                const auto canonical = state.sceneDocument.project_snapshot(
                    state.timeSnapshot.frame_index,
                    state.timeSnapshot.simulated_seconds);
                if (canonical)
                    snapshot = canonical.snapshot;
            }
            snapshot.scene_id = std::string(editor_scene_identity(state));
            snapshot.project_id = state.projectId;
            snapshot.world_name = state.activeWorld;
            if (snapshot.document_kind.empty())
                snapshot.document_kind = "game";
            if (snapshot.support_tier.empty())
                snapshot.support_tier = "baseline";
            snapshot.revision = (std::max)(std::uint64_t{ 1u }, state.sceneDocumentRevision);
            snapshot.captured_frame_index = state.timeSnapshot.frame_index;
            snapshot.captured_simulated_seconds = state.timeSnapshot.simulated_seconds;
            snapshot.primary_camera = scene::kInvalidSceneObjectId;
            snapshot.primary_spawn = scene::kInvalidSceneObjectId;
            snapshot.objects.clear();
            snapshot.objects.reserve(state.entities.size());

            for (const EditorEntity& entity : state.entities)
            {
                scene::SceneObjectSnapshot object{};
                object.id = entity.sceneObjectId;
                object.name = entity.name;
                object.type = entity.type;
                object.category = entity.category;
                object.position = entity.position;
                object.rotation = entity.rotation;
                object.scale = entity.scale;
                object.visible = entity.visible;
                object.editor_only = entity.editorOnly;
                object.texture_material = entity.textureMaterial;
                snapshot.objects.emplace_back(std::move(object));
            }

            const bool hasArcade = std::any_of(
                state.entities.begin(),
                state.entities.end(),
                [](const EditorEntity& entity)
                {
                    return entity.category == "EngineArcade";
                });
            std::erase(snapshot.packages, "engine_arcade");
            if (hasArcade)
                snapshot.packages.emplace_back("engine_arcade");
            scene::normalize_scene_document(snapshot);
            return snapshot;
        }

        [[nodiscard]] scene::SceneSnapshot capture_editor_scene_projection(const EditorState& state)
        {
            if (state.sceneDocument.initialized())
            {
                const auto projection = state.sceneDocument.project_snapshot(
                    state.timeSnapshot.frame_index,
                    state.timeSnapshot.simulated_seconds);
                if (projection)
                    return projection.snapshot;
            }
            return capture_editor_entity_projection(state);
        }

        [[nodiscard]] bool ensure_editor_scene_interaction(EditorState& state)
        {
            synchronize_editor_selection(state);
            if (!state.sceneInteractionDirty)
                return true;

            const scene::SceneSnapshot projection = capture_editor_scene_projection(state);
            const auto validation = scene::validate_scene_document(
                projection,
                scene::SceneDocumentRequirement::authoring);
            if (!validation)
                return false;

            std::vector<scene_interaction::SceneObjectInteractionOverride>
                interactionOverrides{};
            interactionOverrides.reserve(projection.objects.size());
            for (const auto& object : projection.objects)
            {
                if (object.type != "Camera")
                    continue;
                const auto objectId =
                    scene_interaction::derive_scene_object_id(
                        projection, object);
                if (!objectId)
                    continue;
                interactionOverrides.push_back({
                    .object = *objectId,
                    .selectability =
                        scene_interaction::Selectability::Unselectable});
            }
            const auto report = state.sceneInteraction.rebuild(
                projection,
                scene_interaction::SceneInteractionBuildOptions{},
                interactionOverrides);
            if (!report.committed())
                return false;

            state.sceneInteractionDirty = false;
            return true;
        }

        [[nodiscard]] bool rebuild_editor_scene_document(
            EditorState& state,
            std::string_view reason);
        [[nodiscard]] bool rebuild_editor_scene_document_from_snapshot(
            EditorState& state,
            const scene::SceneSnapshot& snapshot,
            std::string_view reason);

        [[nodiscard]] bool load_editor_scene_snapshot(EditorState& state);
        [[nodiscard]] bool save_editor_scene_snapshot(EditorState& state);
        [[nodiscard]] std::filesystem::path resolve_editor_path(
            const std::filesystem::path& path);
        [[nodiscard]] std::string display_project_path(
            const std::filesystem::path& path);
        [[nodiscard]] bool project_gui_document_layout(EditorState& state);
        [[nodiscard]] bool save_project_gui_document(EditorState& state);

