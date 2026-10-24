// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] bool project_scene_document_into_editor(EditorState& state)
        {
            const auto projection = state.sceneDocument.project_snapshot(
                state.timeSnapshot.frame_index,
                state.timeSnapshot.simulated_seconds);
            if (!projection)
                return false;

            const scene::SceneObjectId selectedId = state.selectedEntityId;
            state.entities.clear();
            state.entities.reserve(projection.snapshot.objects.size());
            for (const scene::SceneObjectSnapshot& source : projection.snapshot.objects)
            {
                state.entities.push_back(EditorEntity{
                    .sceneObjectId = source.id,
                    .name = source.name,
                    .type = source.type,
                    .category = source.category,
                    .position = source.position,
                    .rotation = source.rotation,
                    .scale = source.scale,
                    .visible = source.visible,
                    .editorOnly = source.editor_only,
                    .textureMaterial = source.texture_material
                });
            }
            state.sceneDocumentRevision = projection.snapshot.revision;
            state.selectedEntityId = selectedId;
            synchronize_editor_selection(state);
            state.sceneInteractionDirty = true;
            state.sceneLightingDirty = true;
            return true;
        }

        [[nodiscard]] bool rebuild_editor_scene_document_from_snapshot(
            EditorState& state,
            const scene::SceneSnapshot& snapshot,
            std::string_view reason)
        {
            auto built = authoring::scene::SceneDocument::from_snapshot(snapshot);
            if (!built)
            {
                state.projectStatus = epochengine::format_text(
                    "Scene document rebuild blocked (code {}).",
                    static_cast<unsigned>(built.code));
                push_editor_log(
                    state,
                    epochengine::format_text("[scene] {}: {}", reason, state.projectStatus));
                return false;
            }

            state.sceneDocument = std::move(built.value);
            if (!project_scene_document_into_editor(state))
            {
                state.projectStatus = "Scene document projection failed after a validated rebuild.";
                push_editor_log(
                    state,
                    epochengine::format_text("[scene] {}: {}", reason, state.projectStatus));
                return false;
            }
            return true;
        }

        [[nodiscard]] bool rebuild_editor_scene_document(
            EditorState& state,
            std::string_view reason)
        {
            normalize_editor_entity_ids(state);
            return rebuild_editor_scene_document_from_snapshot(
                state,
                capture_editor_entity_projection(state),
                reason);
        }

        [[nodiscard]] scene::SceneObjectSnapshot editor_entity_snapshot(
            const EditorEntity& entity)
        {
            return {
                .id = entity.sceneObjectId,
                .name = entity.name,
                .type = entity.type,
                .category = entity.category,
                .position = entity.position,
                .rotation = entity.rotation,
                .scale = entity.scale,
                .visible = entity.visible,
                .editor_only = entity.editorOnly,
                .texture_material = entity.textureMaterial
            };
        }

        [[nodiscard]] scene::SceneObjectId allocate_editor_scene_object_id(
            const EditorState& state,
            std::string_view name)
        {
            std::unordered_set<scene::SceneObjectId> occupied{};
            const auto objects = state.sceneDocument.objects();
            occupied.reserve(objects.size());
            for (const auto& object : objects)
                occupied.insert(object.descriptor.id.value);

            scene::SceneObjectId candidate = scene::stable_scene_object_id(
                editor_scene_identity(state),
                name);
            std::uint64_t salt = static_cast<std::uint64_t>(objects.size()) + 1u;
            while (candidate == scene::kInvalidSceneObjectId || occupied.contains(candidate))
            {
                candidate ^= 0x9e3779b97f4a7c15ull
                    + salt
                    + (candidate << 6u)
                    + (candidate >> 2u);
                ++salt;
            }
            return candidate;
        }

        [[nodiscard]] bool apply_editor_scene_transaction(
            EditorState& state,
            std::span<const authoring::scene::SceneOperation> operations,
            std::string_view label,
            std::optional<scene::SceneObjectId> selected = std::nullopt)
        {
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(state, "Initialize semantic scene commands"))
            {
                return false;
            }

            const auto committed = state.sceneDocument.apply_transaction(operations, label);
            if (!committed)
            {
                (void)project_scene_document_into_editor(state);
                state.projectStatus = epochengine::format_text(
                    "Scene transaction '{}' rejected (code {}).",
                    label,
                    static_cast<unsigned>(committed.code));
                push_editor_log(state, "[scene] " + state.projectStatus);
                return false;
            }

            if (selected)
                state.selectedEntityId = *selected;
            state.sceneDocumentRevision = committed.revision.sequence;
            if (!project_scene_document_into_editor(state))
            {
                state.projectStatus = "Scene transaction committed, but the editor projection failed.";
                push_editor_log(state, "[scene] " + state.projectStatus);
                return false;
            }
            return true;
        }

        [[nodiscard]] bool create_editor_scene_entity(
            EditorState& state,
            EditorEntity entity,
            std::string_view label)
        {
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(state, "Initialize scene before object creation"))
            {
                return false;
            }
            if (entity.sceneObjectId == scene::kInvalidSceneObjectId)
                entity.sceneObjectId = allocate_editor_scene_object_id(state, entity.name);

            const auto converted = authoring::scene::descriptor_from_snapshot_object(
                editor_entity_snapshot(entity),
                static_cast<std::uint32_t>(state.sceneDocument.objects().size()),
                state.sceneDocument.limits());
            if (!converted)
            {
                state.projectStatus = "Scene object creation rejected because its descriptor is invalid.";
                push_editor_log(state, "[scene] " + state.projectStatus);
                return false;
            }

            const std::array<authoring::scene::SceneOperation, 1> operations{
                authoring::scene::ObjectCreatedOperation{.object = *converted}
            };
            return apply_editor_scene_transaction(
                state,
                operations,
                label,
                entity.sceneObjectId);
        }

        void append_editor_descriptor_operations(
            std::vector<authoring::scene::SceneOperation>& operations,
            const authoring::scene::SceneObject& current,
            const authoring::scene::SceneObjectDescriptor& next)
        {
            namespace document = authoring::scene;
            if (current.descriptor.metadata != next.metadata)
            {
                operations.emplace_back(document::ObjectMetadataChangedOperation{
                    .object = current.handle,
                    .value = next.metadata
                });
            }
            if (current.descriptor.transform != next.transform)
            {
                operations.emplace_back(document::ObjectTransformChangedOperation{
                    .object = current.handle,
                    .value = next.transform
                });
            }
            if (current.descriptor.runtime_visible != next.runtime_visible ||
                current.descriptor.editor_visible != next.editor_visible ||
                current.descriptor.editor_only != next.editor_only)
            {
                operations.emplace_back(document::ObjectVisibilityChangedOperation{
                    .object = current.handle,
                    .runtime_visible = next.runtime_visible,
                    .editor_visible = next.editor_visible,
                    .editor_only = next.editor_only
                });
            }
            if (current.descriptor.camera != next.camera)
            {
                operations.emplace_back(document::CameraComponentChangedOperation{
                    .object = current.handle,
                    .value = next.camera
                });
            }
            if (current.descriptor.ground != next.ground)
            {
                operations.emplace_back(document::GroundComponentChangedOperation{
                    .object = current.handle,
                    .value = next.ground
                });
            }
            if (current.descriptor.light != next.light)
            {
                operations.emplace_back(document::LightComponentChangedOperation{
                    .object = current.handle,
                    .value = next.light
                });
            }
            if (current.descriptor.spawn != next.spawn)
            {
                operations.emplace_back(document::SpawnComponentChangedOperation{
                    .object = current.handle,
                    .value = next.spawn
                });
            }
        }

        [[nodiscard]] bool create_editor_scene_entities(
            EditorState& state,
            std::vector<EditorEntity> entities,
            std::string_view label)
        {
            if (entities.empty())
                return true;
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(state, "Initialize scene before batch creation"))
            {
                return false;
            }

            const auto existing = state.sceneDocument.objects();
            std::unordered_set<scene::SceneObjectId> occupied{};
            occupied.reserve(existing.size() + entities.size());
            for (const auto& object : existing)
                occupied.insert(object.descriptor.id.value);

            std::vector<authoring::scene::SceneOperation> operations{};
            operations.reserve(entities.size());
            std::optional<scene::SceneObjectId> selected{};
            for (std::size_t index = 0; index < entities.size(); ++index)
            {
                auto& entity = entities[index];
                scene::SceneObjectId candidate = entity.sceneObjectId;
                if (candidate == scene::kInvalidSceneObjectId || occupied.contains(candidate))
                {
                    candidate = scene::stable_scene_object_id(
                        editor_scene_identity(state),
                        entity.name);
                    std::uint64_t salt =
                        static_cast<std::uint64_t>(existing.size() + index) + 1u;
                    while (candidate == scene::kInvalidSceneObjectId ||
                           occupied.contains(candidate))
                    {
                        candidate ^= 0x9e3779b97f4a7c15ull
                            + salt
                            + (candidate << 6u)
                            + (candidate >> 2u);
                        ++salt;
                    }
                }
                entity.sceneObjectId = candidate;
                occupied.insert(candidate);

                const auto converted =
                    authoring::scene::descriptor_from_snapshot_object(
                        editor_entity_snapshot(entity),
                        static_cast<std::uint32_t>(existing.size() + index),
                        state.sceneDocument.limits());
                if (!converted)
                {
                    state.projectStatus =
                        "Scene batch creation rejected because an object descriptor is invalid.";
                    push_editor_log(state, "[scene] " + state.projectStatus);
                    return false;
                }
                operations.emplace_back(
                    authoring::scene::ObjectCreatedOperation{.object = *converted});
                if (!selected)
                    selected = candidate;
            }
            return apply_editor_scene_transaction(state, operations, label, selected);
        }

        [[nodiscard]] bool synchronize_editor_scene_category(
            EditorState& state,
            std::string_view category,
            std::vector<EditorEntity> desired,
            std::string_view label,
            std::string_view selected_name = {})
        {
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(state, "Initialize scene before category synchronization"))
            {
                return false;
            }

            const auto existing = state.sceneDocument.objects();
            std::unordered_map<std::string, const authoring::scene::SceneObject*> existingByName{};
            std::unordered_set<scene::SceneObjectId> occupied{};
            occupied.reserve(existing.size() + desired.size());
            for (const auto& object : existing)
            {
                occupied.insert(object.descriptor.id.value);
                if (object.descriptor.metadata.category == category)
                    existingByName.emplace(object.descriptor.metadata.name, &object);
            }

            std::unordered_set<std::string> desiredNames{};
            desiredNames.reserve(desired.size());
            std::vector<authoring::scene::SceneOperation> operations{};
            std::optional<scene::SceneObjectId> selected{};
            for (std::size_t index = 0; index < desired.size(); ++index)
            {
                auto& entity = desired[index];
                entity.category = std::string(category);
                desiredNames.insert(entity.name);
                const auto found = existingByName.find(entity.name);
                std::uint32_t sortOrder =
                    static_cast<std::uint32_t>(existing.size() + index);
                if (found != existingByName.end())
                {
                    entity.sceneObjectId = found->second->descriptor.id.value;
                    sortOrder = found->second->descriptor.metadata.sort_order;
                }
                else
                {
                    scene::SceneObjectId candidate = scene::stable_scene_object_id(
                        editor_scene_identity(state),
                        entity.name);
                    std::uint64_t salt =
                        static_cast<std::uint64_t>(existing.size() + index) + 1u;
                    while (candidate == scene::kInvalidSceneObjectId ||
                           occupied.contains(candidate))
                    {
                        candidate ^= 0x9e3779b97f4a7c15ull
                            + salt
                            + (candidate << 6u)
                            + (candidate >> 2u);
                        ++salt;
                    }
                    entity.sceneObjectId = candidate;
                    occupied.insert(candidate);
                }

                const auto converted =
                    authoring::scene::descriptor_from_snapshot_object(
                        editor_entity_snapshot(entity),
                        sortOrder,
                        state.sceneDocument.limits());
                if (!converted)
                {
                    state.projectStatus =
                        "Scene category synchronization rejected because an object descriptor is invalid.";
                    push_editor_log(state, "[scene] " + state.projectStatus);
                    return false;
                }
                if (found == existingByName.end())
                {
                    operations.emplace_back(
                        authoring::scene::ObjectCreatedOperation{.object = *converted});
                }
                else
                {
                    append_editor_descriptor_operations(
                        operations,
                        *found->second,
                        *converted);
                }
                if (!selected_name.empty() && entity.name == selected_name)
                    selected = entity.sceneObjectId;
            }

            for (const auto& [name, object] : existingByName)
            {
                if (!desiredNames.contains(name))
                {
                    operations.emplace_back(
                        authoring::scene::ObjectDestroyedOperation{
                            .object = object->handle
                        });
                }
            }
            if (operations.empty())
            {
                if (selected)
                {
                    state.selectedEntityId = *selected;
                    synchronize_editor_selection(state);
                }
                return true;
            }
            return apply_editor_scene_transaction(state, operations, label, selected);
        }
        [[nodiscard]] bool destroy_editor_scene_entity(
            EditorState& state,
            scene::SceneObjectId id,
            std::string_view label)
        {
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(state, "Initialize scene before object deletion"))
            {
                return false;
            }
            const auto handle = state.sceneDocument.find(authoring::scene::ObjectId{id});
            if (!handle)
                return false;
            std::vector<authoring::scene::SceneOperation> operations{};
            operations.emplace_back(
                authoring::scene::ObjectDestroyedOperation{.object = *handle});
            auto project = state.sceneDocument.project_settings();
            bool projectChanged = false;
            if (project.primary_camera.value == id)
            {
                project.primary_camera = {};
                projectChanged = true;
            }
            if (project.primary_spawn.value == id)
            {
                project.primary_spawn = {};
                projectChanged = true;
            }
            if (projectChanged)
            {
                project.run_state = scene_tier0::ProjectRunState::blocked;
                operations.emplace_back(
                    authoring::scene::ProjectSettingsChangedOperation{
                        .value = project
                    });
            }
            return apply_editor_scene_transaction(state, operations, label);
        }

        [[nodiscard]] bool clear_editor_scene(
            EditorState& state,
            std::string_view label)
        {
            if (!state.sceneDocument.initialized() &&
                !rebuild_editor_scene_document(
                    state,
                    "Initialize scene before clearing"))
            {
                return false;
            }

            const auto objects = state.sceneDocument.objects();
            std::vector<authoring::scene::SceneOperation> operations{};
            operations.reserve(objects.size() + 1u);
            for (const auto& object : objects)
            {
                operations.emplace_back(
                    authoring::scene::ObjectDestroyedOperation{
                        .object = object.handle});
            }

            auto project = state.sceneDocument.project_settings();
            if (project.primary_camera.value != scene::kInvalidSceneObjectId
                || project.primary_spawn.value != scene::kInvalidSceneObjectId
                || project.run_state != scene_tier0::ProjectRunState::blocked)
            {
                project.primary_camera = {};
                project.primary_spawn = {};
                project.run_state = scene_tier0::ProjectRunState::blocked;
                operations.emplace_back(
                    authoring::scene::ProjectSettingsChangedOperation{
                        .value = project});
            }

            if (operations.empty())
            {
                clear_editor_selection(state);
                return true;
            }

            if (!apply_editor_scene_transaction(state, operations, label))
                return false;

            clear_editor_selection(state);
            return true;
        }

        [[nodiscard]] bool undo_editor_scene(EditorState& state)
        {
            const auto result = state.sceneDocument.undo();
            if (!result)
                return false;
            return project_scene_document_into_editor(state);
        }

        [[nodiscard]] bool redo_editor_scene(EditorState& state)
        {
            const auto result = state.sceneDocument.redo();
            if (!result)
                return false;
            return project_scene_document_into_editor(state);
        }
        [[nodiscard]] bool transform_editor_scene_entity(
            EditorState& state,
            const EditorEntity& entity,
            std::string_view label)
        {
            const auto handle = state.sceneDocument.find(
                authoring::scene::ObjectId{entity.sceneObjectId});
            if (!handle)
                return false;
            const std::array<authoring::scene::SceneOperation, 1> operations{
                authoring::scene::ObjectTransformChangedOperation{
                    .object = *handle,
                    .value = {
                        .position = {entity.position[0], entity.position[1], entity.position[2]},
                        .rotation_degrees = {entity.rotation[0], entity.rotation[1], entity.rotation[2]},
                        .scale = {entity.scale[0], entity.scale[1], entity.scale[2]}
                    }
                }
            };
            return apply_editor_scene_transaction(
                state,
                operations,
                label,
                entity.sceneObjectId);
        }
        [[nodiscard]] EditorContextSnapshotEntity capture_snapshot_entity(const EditorEntity& entity)
        {
            return EditorContextSnapshotEntity{
                .scene_object_id = entity.sceneObjectId,
                .name = entity.name,
                .type = entity.type,
                .category = entity.category,
                .position = entity.position,
                .rotation = entity.rotation,
                .scale = entity.scale,
                .visible = entity.visible,
                .editor_only = entity.editorOnly,
                .texture_material = entity.textureMaterial
            };
        }

        [[nodiscard]] EditorEntity restore_snapshot_entity(const EditorContextSnapshotEntity& entity)
        {
            return EditorEntity{
                .sceneObjectId = entity.scene_object_id,
                .name = entity.name,
                .type = entity.type,
                .category = entity.category,
                .position = entity.position,
                .rotation = entity.rotation,
                .scale = entity.scale,
                .visible = entity.visible,
                .editorOnly = entity.editor_only,
                .textureMaterial = entity.texture_material
            };
        }

        [[nodiscard]] EditorEntity editor_entity_from_seed(const EditorSceneSeedEntity& entity)
        {
            return EditorEntity{
                .name = entity.name,
                .type = entity.type,
                .category = entity.category,
                .position = entity.position,
                .rotation = entity.rotation,
                .scale = entity.scale,
                .visible = entity.visible,
                .editorOnly = entity.editor_only
            };
        }

