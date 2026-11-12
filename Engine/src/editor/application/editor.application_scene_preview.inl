// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] bool frame_editor_scene_target(const void* context,
            const std::array<double,3>& target,float radius,bool precise=false)
        {
            // Editor Focus frames an object even while flight controls are
            // selected. The shared FPS focus operation deliberately teleports
            // its eye to the target; use the existing orbit framing first,
            // then preserve that view when restoring the selected controls.
            const bool flight=previewgrid::camera_mode_for(context)==previewgrid::CameraMode::FPS;
            const auto previous=flight?std::optional{previewgrid::capture_camera_rig_snapshot(context)}:std::nullopt;
            if(flight)previewgrid::set_camera_mode(context,previewgrid::CameraMode::Editor);
            const bool focused=precise?previewgrid::focus_camera_precise(context,target,radius)
                :previewgrid::focus_camera(context,{float(target[0]),float(target[1]),float(target[2])},radius);
            if(flight)
            {
                if(focused)previewgrid::set_camera_mode(context,previewgrid::CameraMode::FPS);
                else (void)previewgrid::restore_camera_rig_snapshot(context,*previous);
            }
            return focused;
        }

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
            bool reportMissing, bool* sourceMissing = nullptr)
        {
            if (sourceMissing) *sourceMissing = false;
            if (state.projectId.empty() || state.projectRoot.empty())
            {
                const bool scratch = state.projectId.empty() && state.projectRoot.empty();
                if (sourceMissing) *sourceMissing = scratch;
                state.plantLabAssetStatus = scratch
                    ? "No active project can own a Plant Lab asset."
                    : "Plant Lab project identity is incomplete; the existing document was retained.";
                return false;
            }
            if (!state.projectLifecycle.evidence().project_selected)
            {
                state.plantLabAssetStatus = "Select a project before reopening its Plant Lab source.";
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
            const auto identity = editor_inspect_project_manifest_identity(
                (library.project_root() / "project.epoch.json").generic_string(),
                state.projectId, library.project_root().generic_string());
            if (!identity.succeeded)
            {
                state.plantLabAssetStatus = "Plant Lab project reopen failed: " + identity.summary;
                return false;
            }
            const auto source_is_genuinely_absent = [&]() noexcept {
                namespace fs = std::filesystem;
                try
                {
                    const auto root = library.project_root();
                    std::error_code error;
                    const auto manifest = fs::symlink_status(root / "project.epoch.json", error);
                    if (error || !fs::is_regular_file(manifest)) return false;
                    const auto path = root / kDefaultPlantLabLogicalPath;
                    auto current = path.root_path();
                    bool insideProject = false;
                    for (const auto& part : path.relative_path())
                    {
                        current /= part;
                        if (current == root) insideProject = true;
                        error.clear();
                        const auto status = fs::symlink_status(current, error);
                        if (status.type() == fs::file_type::not_found
                            && (!error || error == std::errc::no_such_file_or_directory))
                            return insideProject && current != root;
                        if (error || fs::is_symlink(status)) return false;
                        std::error_code redirect;
                        (void)fs::read_symlink(current, redirect);
                        if (!redirect || current == path || !fs::is_directory(status)) return false;
                    }
                    return false;
                }
                catch (...) { return false; }
            };
            const auto source = library.load_source(
                kDefaultPlantLabLogicalPath);
            if (!source)
            {
                const bool missing = source.code == project_forests::ForestLibraryCode::not_found
                    && source_is_genuinely_absent();
                if (sourceMissing) *sourceMissing = missing;
                state.plantLabAssetStatus = missing
                    ? "No published Plant Lab asset exists in this project yet."
                    : epochengine::format_text(
                        "Plant Lab source reopen failed: {}.",
                        project_forests::forest_library_code_name(source.code));
                if (reportMissing)
                    push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
                return false;
            }
            auto artifact = library.load_latest(kDefaultPlantLabLogicalPath);
            if (artifact.code == project_forests::ForestLibraryCode::rebuild_required)
            {
                // Recompile the separately verified authoring document. Do not
                // accept or overwrite the obsolete compiled descriptor.
                const auto rebuilt = library.publish(kDefaultPlantLabLogicalPath, source.document);
                if (rebuilt) artifact = library.load_exact(kDefaultPlantLabLogicalPath, rebuilt.artifact.artifact_key);
                else artifact.code = rebuilt.code;
            }
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
                bool sourceMissing = false;
                if (!reopen_plant_lab_project_asset(state, false, &sourceMissing))
                {
                    if (!sourceMissing)
                    {
                        state.plantLabStatus = state.plantLabAssetStatus;
                        return false;
                    }
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
                "Plant revision {}: {} branch segments and {} leaf cards.",
                state.plantLabCompiled.sourceRevision,
                state.plantLabCompiled.preview.segmentCount,
                state.plantLabCompiled.preview.leafCount);
            return true;
        }
        [[nodiscard]] bool publish_plant_lab_project_asset(EditorState& state,
            const forest::ForestAssetDocument* preparedDocument = nullptr,
            std::string_view logicalPath = kDefaultPlantLabLogicalPath)
        {
            if (!preparedDocument && !ensure_plant_lab_compiled_asset(state))
            {
                state.plantLabAssetStatus = state.plantLabStatus.empty()
                    ? "Plant Lab cannot publish an invalid compiled revision."
                    : state.plantLabStatus;
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
            const auto& document = preparedDocument ? *preparedDocument : state.plantLabDocument;
            if (logicalPath != kDefaultPlantLabLogicalPath)
            {
                const auto existing = library.load_source(logicalPath);
                if (existing.code != project_forests::ForestLibraryCode::not_found)
                {
                    const auto stored = existing ? library.load_latest(logicalPath) : project_forests::LoadedForestArtifact{};
                    if (stored && existing.document.revision == document.revision
                        && existing.document.contentHash == document.contentHash
                        && existing.document.genome.stableHash == document.genome.stableHash
                        && stored.locator.source_revision == document.revision
                        && stored.locator.source_content_hash == document.contentHash) return true;
                    state.plantLabAssetStatus = "The saved forest species recipe could not be verified.";
                    return false;
                }
            }
            const auto publication = library.publish(logicalPath, document);
            if (!publication)
            {
                state.plantLabAssetStatus = epochengine::format_text(
                    "Plant Lab publication failed: {}.",
                    project_forests::forest_library_code_name(publication.code));
                push_editor_log(state, "[plant] " + state.plantLabAssetStatus);
                return false;
            }
            if (logicalPath != kDefaultPlantLabLogicalPath) return true;
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
                const forest::CompiledForestAsset& compiledAsset,
                std::string_view namePrefix,
                std::string_view category,
                bool editorOnly,
                float placementX = 0.0f,
                float placementZ = 0.0f,
                bool rootedMetric = false)
        {
            if (!compiledAsset.valid) return {};
            const auto compiled = forest::preview_mesh::build(compiledAsset.graph, compiledAsset.sample);
            if (!compiled.valid || compiledAsset.graph.nodes.empty()) return {};
            const auto root=compiledAsset.graph.nodes.front().position;
            constexpr float previewScale = 0.58f;
            const std::array<float, 3> minimum{compiled.bounds.minimum.x,
                compiled.bounds.minimum.y, compiled.bounds.minimum.z};
            const std::array<float, 3> maximum{compiled.bounds.maximum.x,
                compiled.bounds.maximum.y, compiled.bounds.maximum.z};
            std::array<float, 3> extent{}, center{};
            for (std::size_t axis = 0u; axis < 3u; ++axis)
            {
                extent[axis] = maximum[axis] - minimum[axis];
                center[axis] = (minimum[axis] + maximum[axis]) * 0.5f;
                if (!std::isfinite(extent[axis]) || extent[axis] <= 0.0f) return {};
            }
            const auto wood = previewgrid::visual_rgb(visuals::forest_trunk());
            const auto foliage = previewgrid::visual_rgb(visuals::forest_foliage_cluster());
            std::vector<asset::mesh::Vertex> vertices;
            vertices.reserve(compiled.vertices.size());
            for (const auto& source : compiled.vertices)
            {
                const std::array<float, 3> position{source.position.x, source.position.y, source.position.z};
                std::array<float, 3> normal{source.normal.x * (rootedMetric?1.0f:extent[0]),
                    source.normal.y * (rootedMetric?1.0f:extent[1]), source.normal.z * (rootedMetric?1.0f:extent[2])};
                const float magnitude = std::sqrt(normal[0] * normal[0]
                    + normal[1] * normal[1] + normal[2] * normal[2]);
                if (!std::isfinite(magnitude) || magnitude <= 0.0f) return {};
                const auto color = source.material == forest::preview_mesh::MaterialSlot::wood
                    ? wood : foliage;
                const auto uv = forest::preview_mesh::material_uv(source.material, source.uv);
                vertices.push_back({
                    {rootedMetric?position[0]-root.x:(position[0] - center[0]) / extent[0],
                     rootedMetric?position[1]-root.y:(position[1] - center[1]) / extent[1],
                     rootedMetric?position[2]-root.z:(position[2] - center[2]) / extent[2]},
                    {normal[0] / magnitude, normal[1] / magnitude, normal[2] / magnitude},
                    {uv.x, uv.y}, {color.x, color.y, color.z}});
            }
            const auto built = asset::mesh::build_artifact(vertices, compiled.indices);
            if (!built) return {};
            // Centered unit local bounds keep the existing transform, focus,
            // grounding and scene transaction contracts accurate. Entity scale
            // restores the full authored dimensions; no branches are skipped.
            EditorEntity entity = editor_entity_from_seed(EditorSceneSeedEntity{
                std::string{namePrefix} + "Trunk", "ForestTrunk", std::string{category},
                {placementX + center[0] * previewScale, center[1] * previewScale,
                    placementZ + center[2] * previewScale}, {},
                {extent[0] * previewScale, extent[1] * previewScale, extent[2] * previewScale},
                true, editorOnly});
            entity.meshSnapshot = built.artifact;
            entity.meshLoadAttempted = true;
            if(rootedMetric){entity.position={};entity.scale={1,1,1};}
            std::vector<EditorEntity> entities;
            entities.push_back(std::move(entity));
            return entities;
        }
        [[nodiscard]] forest::preview_mesh::MaterialAtlasResult prepare_plant_material_atlas(
            const std::filesystem::path& projectRoot, std::string_view projectId,
            const forest::ForestFactoryProfile& profile)
        {
            using Result = forest::preview_mesh::MaterialAtlasResult;
            if (profile.barkTexture.empty() && profile.foliageTexture.empty()) return Result{};
            try
            {
                project_textures::TextureLibraryLimits limits;
                limits.maximum_serialized_bytes = limits.read.maximum_serialized_bytes = 80u * 1024u * 1024u;
                limits.read.maximum_payload_bytes = 64u * 1024u * 1024u;
                limits.read.maximum_dimension = 8192u;
                project_textures::TextureArtifactLibrary library{std::string{projectId}, projectRoot.string(), limits};
                project_textures::LoadedTextureArtifact bark, foliage;
                if (!profile.barkTexture.empty())
                {
                    if (!profile.barkTexture.valid()) return Result{.diagnostic="The saved bark texture reference is invalid."};
                    bark = library.load_exact(profile.barkTexture.logicalPath, {profile.barkTexture.artifactKey});
                    if (!bark) return Result{.diagnostic="The exact saved bark texture is unavailable. Reimport it or choose another project texture."};
                }
                if (!profile.foliageTexture.empty())
                {
                    if (!profile.foliageTexture.valid()) return Result{.diagnostic="The saved foliage texture reference is invalid."};
                    foliage = library.load_exact(profile.foliageTexture.logicalPath, {profile.foliageTexture.artifactKey});
                    if (!foliage) return Result{.diagnostic="The exact saved foliage texture is unavailable. Reimport it or choose another project texture."};
                }
                const auto wood = previewgrid::visual_rgb(visuals::forest_trunk());
                const auto leaf = previewgrid::visual_rgb(visuals::forest_foliage_cluster());
                return forest::preview_mesh::compose_material_atlas(bark ? &bark.artifact : nullptr,
                    foliage ? &foliage.artifact : nullptr, profile.barkTileIndex,
                    {wood.x,wood.y,wood.z}, {leaf.x,leaf.y,leaf.z});
            }
            catch (...) { return Result{.diagnostic="The project forest material could not be restored safely."}; }
        }

        [[nodiscard]] bool bind_plant_preview_material(EditorState& state, std::vector<EditorEntity>& entities,
            const forest::ForestFactoryProfile& profile)
        {
            if (profile.barkTexture.empty() && profile.foliageTexture.empty()) return true;
            auto material = prepare_plant_material_atlas(resolve_editor_path(std::filesystem::path{state.projectRoot}),state.projectId,profile);
            if (!material) { state.plantLabStatus=material.diagnostic; return false; }
            auto sealed=texture_artifact::seal_compiled_texture(material.artifact);
            if (!sealed.artifact) return false;
            const auto& atlas=material.artifact;
            const auto key=atlas.identity.key.words[0] ? atlas.identity.key.words[0] : 1u;
            auto binding=std::make_shared<previewgrid::MeshBaseColorBinding>(previewgrid::MeshBaseColorBinding{
                std::move(sealed.artifact),{key,texture_artifact::compiled_artifact_revision(atlas.identity)},
                {.min_filter=FilterMode::linear,.mag_filter=FilterMode::linear,.address_v=AddressMode::repeat},true,0.5f,false});
            if (!previewgrid::valid_mesh_base_color_binding(*binding)) return false;
            for (auto& entity:entities) entity.meshBaseColorTexture=binding;
            return true;
        }

        [[nodiscard]] std::optional<scene::SceneTextureMaterialSnapshot> publish_forest_scene_material(
            const std::filesystem::path& root, std::string_view projectId,
            const asset::texture::CompiledTextureArtifact& atlas, std::uint32_t barkTileIndex, bool custom)
        {
            project_textures::ProjectTexturePipeline textures{std::string{projectId}, root.string()};
            std::string logical = forest::preview_mesh::reference_material_available()
                ? "Textures/Forest/reference_bark_foliage_v1" : "Textures/Forest/bark_foliage_v1";
            if (barkTileIndex != 0u) logical += "_bark_" + std::to_string(barkTileIndex);
            if (custom) logical = "Textures/Forest/project_bark_foliage_" + asset::texture::content_hash_hex(atlas.identity.key);
            const auto published = textures.publish(logical, atlas, texture_artifact::TransferUsage::base_color);
            if (!published) return {};
            return scene::SceneTextureMaterialSnapshot{
                .logical_path = logical,
                .artifact_key = atlas.identity.key.words,
                .stable_key = published.logical.asset_key,
                .address_v = scene::SceneTextureAddress::repeat,
                .alpha = scene::SceneTextureAlphaMode::cutout,
                .color_space = atlas.identity.profile.color_space == asset::texture::ColorSpace::srgb
                    ? scene::SceneTextureColorSpace::srgb : scene::SceneTextureColorSpace::linear};
        }

        [[nodiscard]] bool persist_forest_scene_mesh(const std::filesystem::path& projectRoot,
            std::vector<EditorEntity>& entities, std::string_view projectId, std::uint32_t barkTileIndex = 0u,
            const forest::ForestFactoryProfile* profile = nullptr)
        {
            if (entities.size() != 1u || !entities.front().meshSnapshot) return false;
            auto& entity = entities.front();
            const auto& mesh = *entity.meshSnapshot;
            try
            {
                // Resolve all pinned source textures before creating a mesh or
                // replacing any scene material. Missing artifacts never fall back.
                const bool custom=profile&&(!profile->barkTexture.empty()||!profile->foliageTexture.empty());
                auto material=custom ? prepare_plant_material_atlas(projectRoot,projectId,*profile)
                    : forest::preview_mesh::MaterialAtlasResult{};
                if(custom&&!material)return false;
                const auto root = std::filesystem::canonical(
                    projectRoot);
                if (!asset::mesh::prepare_project_mesh_directory(root)) return false;
                const std::string relative = "Assets/Mesh/"
                    + asset::texture::content_hash_hex(mesh.key()) + ".epoch_mesh";
                const auto destination = root / relative;
                if (std::filesystem::exists(destination))
                {
                    if (!asset::mesh::read_project_artifact(root, relative, mesh.key())) return false;
                }
                else
                {
                    const auto bytes = asset::mesh::encode_artifact(mesh);
                    std::error_code error;
                    if (bytes.empty() || !platform::filesystem::exclusive_create_and_write(
                            destination, bytes, error)) return false;
                }
                entity.meshAsset = scene::SceneMeshSnapshot{relative, mesh.key().words,
                    static_cast<std::uint32_t>(mesh.vertices().size()),
                    static_cast<std::uint32_t>(mesh.indices().size())};
                const auto& atlas = custom ? material.artifact : forest::preview_mesh::material_atlas(barkTileIndex);
                entity.textureMaterial = publish_forest_scene_material(root, projectId, atlas, barkTileIndex, custom);
                return entity.textureMaterial.has_value();
            }
            catch (...) { return false; }
        }
        [[nodiscard]] bool persist_forest_scene_mesh(EditorState& state,
            std::vector<EditorEntity>& entities)
        {
            return persist_forest_scene_mesh(resolve_editor_path(std::filesystem::path{state.projectRoot}),
                entities,state.projectId,state.plantLabDocument.profile.barkTileIndex,&state.plantLabDocument.profile);
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
            {
                state.plantLabStatus = "The plant exceeds the smooth mesh preview limits or has invalid geometry. The previous preview is retained.";
                return false;
            }
            if (!bind_plant_preview_material(state,desired,state.plantLabDocument.profile)) return false;
            // Preview residency is deliberately outside the project's entities
            // and authoring journal. Only Add to Scene performs a transaction.
            state.forestPreviewEntities = std::move(desired);
            state.forestPreviewRevision = state.plantLabCompiled.sourceRevision;
            state.forestPreviewContentHash = state.plantLabCompiled.sourceContentHash;
            return true;
        }

        [[nodiscard]] bool capture_ai_forest_recipe_binding(EditorState& state,std::string_view tool)
        {
            state.aiToolPlantRecipeBound=false;
            state.aiToolPlantRevision=state.aiToolPlantHash=0u;
            if(tool!="forest.configure"&&tool!="forest.paint"&&tool!="forest.material"
                &&tool!="forest.material.import"&&tool!="forest.update"&&tool!="forest.species.add")return true;
            if(!has_open_project(state)||!ensure_plant_lab_compiled_asset(state))
            {
                state.aiToolStatus=!has_open_project(state)?"Open a project before changing or painting its Plant Lab tree."
                    :"The current Plant Lab recipe could not be verified: "+state.plantLabStatus;
                return false;
            }
            state.aiToolPlantRecipeBound=true;
            state.aiToolPlantRevision=state.plantLabDocument.revision;
            state.aiToolPlantHash=state.plantLabDocument.contentHash;
            return true;
        }

        [[nodiscard]] bool ai_forest_recipe_binding_current(const EditorState& state) noexcept
        {
            return state.aiToolPlantRecipeBound&&state.plantLabDocumentInitialized
                &&state.aiToolPlantRevision==state.plantLabDocument.revision
                &&state.aiToolPlantHash==state.plantLabDocument.contentHash;
        }

        [[nodiscard]] bool configure_ai_forest_recipe(EditorState& state,const ai::McpToolCall& call)
        {
            const auto fail=[&](std::string message){state.aiToolStatus="forest.configure refused: "+std::move(message);return false;};
            if(call.tool!="forest.configure"||!ai_forest_recipe_binding_current(state))
                return fail("The Plant Lab recipe changed after this request. Review a fresh plan before editing it.");
            using Property=forest::ForestProfileProperty;
            struct Setting {std::string_view name;Property property;float minimum,maximum;bool whole;};
            constexpr std::array settings{
                Setting{"height_m",Property::TargetHeightMeters,.1f,256.f,false},
                Setting{"trunk_radius_m",Property::TrunkRadiusMeters,.005f,8.f,false},
                Setting{"branch_length_m",Property::BranchLengthMeters,.01f,128.f,false},
                Setting{"leaf_size_m",Property::LeafSizeMeters,.02f,2.f,false},
                Setting{"leaf_sites",Property::LeafSitesPerBranch,1.f,16.f,true},
                Setting{"bark_tile",Property::BarkTileIndex,0.f,15.f,true}};
            if(call.arguments.empty()||call.arguments.size()>settings.size())return fail("Choose one to six supported tree controls.");
            try
            {
                auto candidate=state.plantLabDocument;
                std::array<bool,settings.size()> used{};
                for(const auto& argument:call.arguments)
                {
                    const auto item=std::find_if(settings.begin(),settings.end(),[&](const auto& value){return argument.name==value.name;});
                    if(item==settings.end())return fail("Unknown tree control. Nothing changed.");
                    const auto index=static_cast<std::size_t>(item-settings.begin());
                    if(std::exchange(used[index],true))return fail("A tree control was repeated. Nothing changed.");
                    float value{};const auto parsed=std::from_chars(argument.value.data(),argument.value.data()+argument.value.size(),value);
                    if(parsed.ec!=std::errc{}||parsed.ptr!=argument.value.data()+argument.value.size()
                        ||!std::isfinite(value)||value<item->minimum||value>item->maximum
                        ||(item->whole&&std::floor(value)!=value))return fail("A tree control is outside its allowed range. Nothing changed.");
                    if(forest::forest_profile_value(candidate.profile,item->property)!=value
                        &&!forest::apply_forest_profile_edit(candidate,item->property,value))
                        return fail("The existing Plant Lab edit could not be applied. Nothing changed.");
                }
                auto compiled=forest::compile_forest_asset(candidate);
                auto preview=project_compiled_forest_entities(compiled,"ForestPreview","PlantLabPreview",true);
                if(!compiled.valid||preview.empty()||!align_editor_entity_group_to_surface(preview,0.f))
                    return fail("The requested tree exceeds the existing smooth mesh limits or is invalid. The current recipe is retained.");
                if(!bind_plant_preview_material(state,preview,candidate.profile))return fail(state.plantLabStatus);
                // Publish the verified candidate through the same source/library
                // owner before replacing visible controls. Failed publication
                // keeps the current document, history, preview and scene intact.
                if(!publish_plant_lab_project_asset(state,&candidate))return fail(state.plantLabAssetStatus);
                state.plantLabDocument=std::move(candidate);
                state.plantLabCompiled=std::move(compiled);
                state.plantLabCompiledRevision=state.plantLabDocument.revision;
                state.plantLabCompiledContentHash=state.plantLabDocument.contentHash;
                state.forestPreviewEntities=std::move(preview);
                state.forestPreviewRevision=state.plantLabCompiledRevision;
                state.forestPreviewContentHash=state.plantLabCompiledContentHash;
                const auto& profile = state.plantLabDocument.profile;
                const char* foliage = !profile.foliageTexture.empty() ? "selected project texture"
                    : forest::preview_mesh::reference_material_available() ? "packaged oak image"
                    : "procedural foliage";
                state.aiToolStatus=epochengine::format_text("Plant Lab tree configured and saved at revision {}. Existing painted trees were not changed. Foliage: {}; bark tile {}.",
                    state.plantLabDocument.revision,foliage,profile.barkTileIndex);
                state.plantLabStatus=state.aiToolStatus;
                push_editor_log(state,"[plant] "+state.aiToolStatus);
                return true;
            }
            catch(...){return fail("Tree configuration could not be prepared safely. Check the retained Plant Lab source before retrying.");}
        }

        [[nodiscard]] bool change_plant_lab_texture(EditorState& state, forest::ForestTextureSlot slot,
            const forest::ForestTextureReference& reference)
        {
            const auto fail=[&](std::string message){state.plantLabStatus=std::move(message);return false;};
            if(!has_open_project(state)||state.projectBuildPending||state.scriptBuildPending
                ||state.planetaryGeneratePending||state.forestSurfacePending||state.projectRuntimeProcess.valid()
                ||(state.projectRuntimeSnapshot&&state.projectRuntimeSnapshot->active()))
                return fail("Open a stopped project and finish generation or placement before changing its tree textures.");
            if((slot!=forest::ForestTextureSlot::bark&&slot!=forest::ForestTextureSlot::foliage)
                ||(!reference.empty()&&!reference.valid()))return fail("Choose a verified project bark or foliage texture.");
            if(!reference.empty())
            {
                if(!state.projectTextures)return fail("Import the texture in Asset Browser before assigning it to a tree.");
                const auto catalog=state.projectTextures->catalog();
                const auto current=std::find_if(catalog.begin(),catalog.end(),[&](const auto& item){
                    return bool(item)&&item.logical_path==reference.logicalPath&&item.artifact_key.words==reference.artifactKey;});
                if(current==catalog.end())return fail("The project texture or its full artifact key changed. Select the current imported texture and review again.");
            }
            if(!ensure_plant_lab_compiled_asset(state))return false;
            try
            {
                auto candidate=state.plantLabDocument;
                const auto& old=slot==forest::ForestTextureSlot::bark?candidate.profile.barkTexture:candidate.profile.foliageTexture;
                if(old!=reference&&!forest::apply_forest_material_edit(candidate,slot,reference))
                    return fail("The tree texture edit could not be recorded. Nothing changed.");
                auto compiled=forest::compile_forest_asset(candidate);
                auto preview=project_compiled_forest_entities(compiled,"ForestPreview","PlantLabPreview",true);
                if(!compiled.valid||preview.empty()||!align_editor_entity_group_to_surface(preview,0.f))
                    return fail("The current tree geometry cannot be previewed. Nothing changed.");
                if(!bind_plant_preview_material(state,preview,candidate.profile))return false;
                if(!publish_plant_lab_project_asset(state,&candidate))return fail(state.plantLabAssetStatus);
                state.plantLabDocument=std::move(candidate);
                state.plantLabCompiled=std::move(compiled);
                state.plantLabCompiledRevision=state.forestPreviewRevision=state.plantLabDocument.revision;
                state.plantLabCompiledContentHash=state.forestPreviewContentHash=state.plantLabDocument.contentHash;
                state.forestPreviewEntities=std::move(preview);
                state.plantLabStatus=std::string{slot==forest::ForestTextureSlot::bark?"Bark":"Foliage"}
                    +" texture saved in the Plant Lab recipe. Use Update Painted Trees to apply it to existing forests.";
                push_editor_log(state,"[plant] "+state.plantLabStatus);
                return true;
            }
            catch(...){return fail("The tree texture could not be prepared safely. The current recipe and painted trees were retained.");}
        }

        [[nodiscard]] bool assign_plant_lab_texture(EditorState& state, forest::ForestTextureSlot slot,
            const scene::SceneTextureMaterialSnapshot& material)
        {
            return change_plant_lab_texture(state,slot,{material.logical_path,material.artifact_key});
        }

        [[nodiscard]] bool clear_plant_lab_texture(EditorState& state, forest::ForestTextureSlot slot)
        {
            return change_plant_lab_texture(state,slot,{});
        }

        [[nodiscard]] bool assign_ai_forest_material(EditorState& state,const ai::McpToolCall& call)
        {
            const auto fail=[&](std::string message){state.aiToolStatus="forest.material refused: "+std::move(message);return false;};
            if(call.tool!="forest.material"||!ai_forest_recipe_binding_current(state))
                return fail("The Plant Lab recipe changed after this request. Review a fresh plan.");
            if(call.arguments.size()!=3u)return fail("Choose a slot, the listed project texture and its complete artifact key.");
            std::string_view slot,path,key;
            for(const auto& arg:call.arguments)
            {
                auto* destination=arg.name=="slot"?&slot:arg.name=="texture"?&path:arg.name=="artifact"?&key:nullptr;
                if(!destination||!destination->empty()||arg.value.empty())return fail("A texture argument is missing, duplicated or unknown.");
                *destination=arg.value;
            }
            if((slot!="bark"&&slot!="foliage")||key.size()!=64u)return fail("Use bark or foliage and the full 64-digit artifact key.");
            forest::ForestTextureReference reference{std::string{path}};
            for(std::size_t i=0;i<4u;++i)
            {
                const auto part=key.substr(i*16u,16u);
                if(!std::all_of(part.begin(),part.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
                    return fail("The artifact key must match the host-listed lowercase hexadecimal key.");
                const auto parsed=std::from_chars(part.data(),part.data()+part.size(),reference.artifactKey[i],16);
                if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size())return fail("The texture artifact key is invalid.");
            }
            if(!change_plant_lab_texture(state,slot=="bark"?forest::ForestTextureSlot::bark:forest::ForestTextureSlot::foliage,reference))
                return fail(state.plantLabStatus);
            state.aiToolStatus=state.plantLabStatus;
            return true;
        }

        void focus_forest_factory_preview(const core::Context* ctx, EditorState& state)
        {
            if (!ctx || !ensure_forest_factory_preview(state)) return;
            const float height = (std::max)(1.0f,
                state.plantLabDocument.profile.config.targetHeightMeters * 0.58f);
            (void)previewgrid::focus_camera(ctx, {0.0f, height * 0.5f, 0.0f}, height);
        }

        [[nodiscard]] bool focus_forest_paint_region(const void* ctx,
            const EditorEntity& terrain,const EditorEntity& tree)
        {
            if(!ctx||!tree.forestRegion||!tree.forestRegion->valid()||!terrain.meshAsset
                ||terrain.category!="Planetary Terrain")return false;
            const auto& region=*tree.forestRegion;
            if(region.terrain_object_id!=terrain.sceneObjectId
                ||region.terrain_artifact_key!=terrain.meshAsset->artifact_key)return false;
            const auto pivot=scene::world_position(terrain.position,terrain.positionAnchor);
            if(!pivot)return false;
            for(const auto scale:terrain.scale)if(!std::isfinite(scale)||scale<=0.0f)return false;
            const auto parent=previewgrid::make_precise_mesh_transform(*pivot,
                {terrain.scale[0],terrain.scale[1],terrain.scale[2]},
                {terrain.rotation[0],terrain.rotation[1],terrain.rotation[2]});
            if(!parent)return false;
            std::array<double,3> local{},normal{};
            for(std::size_t axis=0;axis<3u;++axis)
                local[axis]=region.admitted_body_center_meters[axis]-region.mesh_source_origin_meters[axis]
                    +region.body_local_root_meters[axis];
            const auto target=parent->apply(local);
            // Paint view shows terrain/regions, not the individual canopy. Use
            // its sampled ground and current parent, never a legacy tree anchor.
            for(std::size_t row=0;row<3u;++row)for(std::size_t axis=0;axis<3u;++axis)
                normal[row]+=parent->rotation[row*3u+axis]*region.radial_up[axis]/parent->scale[axis];
            const double length=std::hypot(normal[0],normal[1],normal[2]);
            const double radius=region.radius_meters*(std::max)({terrain.scale[0],terrain.scale[1],terrain.scale[2]});
            if(!std::isfinite(length)||length<=0.0||!std::isfinite(radius)||radius<=0.0
                ||radius>(std::numeric_limits<float>::max)()/3.25)return false;
            for(auto& value:normal)value/=length;
            if(!previewgrid::focus_camera_precise(ctx,target,static_cast<float>(radius)))return false;
            auto view=previewgrid::capture_camera_rig_snapshot(ctx);
            if(!view.valid)return false;
            constexpr double degrees=57.29577951308232088;
            view.mode=previewgrid::CameraMode::Editor;
            view.projection=epochengine::render_camera::ProjectionKind::perspective;
            view.orientation=epochengine::render_camera::ViewOrientation::free;
            view.focus={};
            view.distance=(std::max)(0.5f,static_cast<float>(radius*3.25));
            view.yaw_degrees=static_cast<float>(std::atan2(-normal[2],-normal[0])*degrees);
            view.pitch_degrees=static_cast<float>(std::asin(std::clamp(-normal[1],-1.0,1.0))*degrees);
            view.free_yaw_degrees=view.yaw_degrees;view.free_pitch_degrees=view.pitch_degrees;
            view.roll_degrees=0.0f;
            return previewgrid::restore_camera_rig_snapshot(ctx,view);
        }

        [[nodiscard]] bool focus_forest_factory_placement(const core::Context* ctx, EditorState& state)
        {
            const auto selected = editor_entity_index(state, state.selectedEntityId);
            if (!ctx || !selected) return false;
            const auto& trunk = state.entities[*selected];
            if (trunk.category != "ForestFactory" || trunk.type != "ForestTrunk"
                || !trunk.name.starts_with("ForestAsset_") || !trunk.name.ends_with("_Trunk"))
                return false;
            const std::string prefix = trunk.name.substr(0u, trunk.name.size() - 5u);
            if (!ensure_editor_scene_interaction(state)) return false;
            std::vector<scene_interaction::SceneObjectId> group;
            for (const auto& entity : state.entities)
                if (entity.category == "ForestFactory" && entity.name.starts_with(prefix))
                    group.push_back({entity.sceneObjectId, 1u});
            const auto focus = state.sceneInteraction.focus_target(group);
            return focus && previewgrid::focus_camera_precise(ctx,
                {state.sceneInteractionOrigin[0]+focus->target.x,
                 state.sceneInteractionOrigin[1]+focus->target.y,
                 state.sceneInteractionOrigin[2]+focus->target.z}, focus->radius);
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
            const auto mesh = desired.empty() ? nullptr : desired.front().meshSnapshot;
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
            // The semantic scene stores descriptors, not transient residency.
            // Restore this one authoring preview after document projection.
            if (mesh)
                for (auto& entity : state.entities)
                    if (entity.category == "PlantLabPreview" && entity.type == "ForestTrunk")
                    { entity.meshSnapshot = mesh; entity.meshLoadAttempted = true; }
        }

        [[nodiscard]] bool forest_area_matches_terrain_frame(
            const EditorEntity& terrain,const scene::SceneForestRegionInstance& region,
            const EditorState::PlanetaryMeshPolicy& policy)
        {
            if(!region.valid()||terrain.category!="Planetary Terrain"||!terrain.meshAsset
                ||!terrain.meshAsset->coordinates||!policy.body_inventory_checked
                ||region.terrain_object_id!=terrain.sceneObjectId
                ||region.terrain_artifact_key!=terrain.meshAsset->artifact_key
                ||region.mesh_source_origin_meters!=terrain.meshAsset->coordinates->source_origin_meters)
                return false;
            const auto body=std::find_if(policy.generated_bodies.begin(),policy.generated_bodies.end(),
                [&](const auto& value){return value.id==region.body_index;});
            return body!=policy.generated_bodies.end()
                &&body->center_meters==region.admitted_body_center_meters
                &&body->radius_meters==region.body_radius_meters;
        }

        [[nodiscard]] std::optional<std::array<double,3>> verified_forest_surface_anchor(
            const EditorEntity& terrain,const scene::SceneForestRegionInstance& region,
            const EditorState::PlanetaryMeshPolicy& policy)
        {
            if(!forest_area_matches_terrain_frame(terrain,region,policy)
                ||policy.source.content_hash!="sha256:"+region.provider_content_hash
                ||region.recipe!=project_planetary::encode_settings(policy.settings))return {};
            const auto pivot=scene::world_position(terrain.position,terrain.positionAnchor);
            if(!pivot)return {};
            for(const auto scale:terrain.scale)if(!std::isfinite(scale)||scale<=0.0f)return {};
            const auto transform=previewgrid::make_precise_mesh_transform(*pivot,
                {terrain.scale[0],terrain.scale[1],terrain.scale[2]},
                {terrain.rotation[0],terrain.rotation[1],terrain.rotation[2]});
            if(!transform)return {};
            std::array<double,3> local{};
            for(std::size_t axis=0;axis<3u;++axis)
                local[axis]=region.admitted_body_center_meters[axis]-region.mesh_source_origin_meters[axis]
                    +region.body_local_root_meters[axis];
            const auto anchor=transform->apply(local);
            for(const auto value:anchor)if(!std::isfinite(value)||std::abs(value)>1e15)return {};
            return anchor;
        }

        [[nodiscard]] bool painted_tree_uses_plant_lab(const EditorEntity& entity) noexcept
        {
            return entity.category == "ForestFactory" && entity.type == "ForestTrunk"
                && entity.forestRegion && entity.forestRegion->plant_source == kDefaultPlantLabLogicalPath;
        }

        struct PaintedTreeUpdateResult final
        {
            bool applied{}, saved{};
            [[nodiscard]] explicit operator bool() const noexcept { return applied; }
        };

        [[nodiscard]] PaintedTreeUpdateResult refresh_forest_factory_instances(EditorState& state)
        {
            if (!has_open_project(state) || state.forestSurfacePending
                || state.projectBuildPending || state.projectRuntimeProcess.valid()
                || !ensure_plant_lab_compiled_asset(state)) return {};
            auto prototype = project_compiled_forest_entities(state.plantLabCompiled,
                "ForestRefresh_", "ForestFactory", false, 0.0f, 0.0f, true);
            if (prototype.size() != 1u || !persist_forest_scene_mesh(state, prototype)
                || !publish_plant_lab_project_asset(state)) return {};
            // Source/receipt currency is verified once for this explicit edit,
            // never once per instance or implicitly while reopening a project.
            const EditorEntity* verifiedTerrain{};
            if(state.planetaryMeshPolicy&&state.planetaryMeshPolicy->generated_mesh)
            {
                const auto terrain=std::find_if(state.entities.begin(),state.entities.end(),
                    [&](const auto& entity){return editor_planetary_stream_identity_matches(state,entity);});
                if(terrain!=state.entities.end())
                {
                    const auto root=resolve_editor_path(std::filesystem::path{state.projectRoot});
                    const auto& policy=*state.planetaryMeshPolicy;
                    if(project_planetary::source_generation_is_current(root,policy.source)
                        &&project_planetary::mesh_generation_is_current(root,policy.source,policy.settings,*policy.generated_mesh))
                        verifiedTerrain=&*terrain;
                }
            }
            std::vector<EditorEntity> desired;
            std::size_t updated = 0u,reattachedRoots=0u,unverifiedRoots=0u;
            std::string selectedName;
            for (const auto& current : state.entities)
            {
                if (current.category != "ForestFactory") continue;
                auto entity = current;
                if (entity.sceneObjectId == state.selectedEntityId) selectedName = entity.name;
                if (painted_tree_uses_plant_lab(entity))
                {
                    entity.meshAsset = prototype.front().meshAsset;
                    entity.meshSnapshot = prototype.front().meshSnapshot;
                    entity.meshLoadAttempted = true;
                    entity.textureMaterial = prototype.front().textureMaterial;
                    entity.forestRegion->plant_revision = state.plantLabCompiled.sourceRevision;
                    entity.forestRegion->plant_hash = state.plantLabCompiled.sourceContentHash;
                    entity.forestRegion->plant_genome = state.plantLabCompiled.genome.stableHash;
                    const auto anchor=verifiedTerrain
                        ?verified_forest_surface_anchor(*verifiedTerrain,*current.forestRegion,*state.planetaryMeshPolicy)
                        :std::nullopt;
                    if(anchor)
                    {
                        // The surface sample owns the root. Position, rotation
                        // and scale remain the user's separate authored edits.
                        if(entity.positionAnchor!=anchor)
                        {entity.positionAnchor=anchor;++reattachedRoots;}
                    }
                    else ++unverifiedRoots;
                    ++updated;
                }
                desired.push_back(std::move(entity));
            }
            if (updated == 0u)
            {
                state.plantLabStatus = "No painted trees use this project's Plant Lab design.";
                return {};
            }
            if (!synchronize_editor_scene_category(state, "ForestFactory", std::move(desired),
                    "Update painted trees", selectedName)) return {};
            const bool saved = save_editor_scene_snapshot(state);
            state.plantLabStatus = saved
                ? "Painted trees updated and saved. Scene Undo restores the previous design."
                : "Painted trees updated; saving failed. Retry Save before closing.";
            if(reattachedRoots)
                state.plantLabStatus+=epochengine::format_text(" {} tree root{} reattached to the current terrain.",
                    reattachedRoots,reattachedRoots==1u?"":"s");
            if(unverifiedRoots)
                state.plantLabStatus+=epochengine::format_text(" {} root{} retained: terrain attachment could not be verified. Use Refresh Forest Placement to resample their existing roots.",
                    unverifiedRoots,unverifiedRoots==1u?"":"s");
            push_editor_log(state, "[forest] " + state.plantLabStatus);
            return {.applied = true, .saved = saved};
        }

        [[nodiscard]] project_planetary::SurfaceBatchResult query_forest_surfaces(
            const std::filesystem::path& root, const project_planetary::SurfaceBatchQuery& request, std::stop_token stop)
        {
            auto batch = project_planetary::query_surfaces(root, request, stop);
            // The passive preview provider may still be retiring when
            // a forest job takes over its process slot.
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{1500};
            while (batch.code == project_planetary::SurfaceCode::busy && !stop.stop_requested()
                && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{50});
                batch = project_planetary::query_surfaces(root, request, stop);
            }
            return batch;
        }

        // Query exact procedural ground through the existing owned provider.
        // A scene transform/recipe/plant edit invalidates the queued intent.
        [[nodiscard]] bool begin_forest_surface_placement(EditorState& state,const EditorEntity& planet,
            std::size_t ordinal, std::optional<forest::ForestAreaDistribution> area = {},
            std::optional<std::array<double,3>> brushDirection = {}, bool findOceanShoreline = false,
            const scene::SceneForestRegionInstance* existingForest = nullptr,
            std::span<const std::array<double,3>> stroke = {})
        {
            if(state.forestSurfacePending)
            {state.plantLabStatus="A plant surface placement is already running.";return false;}
            state.forestPlacementSaved=false;
            if(state.projectBuildPending||state.projectRuntimeProcess.valid())
            {state.plantLabStatus="Stop the project before placing a plant.";return false;}
            if(!editor_planetary_stream_identity_matches(state,planet))
            {state.plantLabStatus="The selected terrain has no current generated surface evidence.";return false;}
            refresh_editor_planetary_body_inventory(state,planet);
            const auto& policy=*state.planetaryMeshPolicy;
            auto bodyId=policy.settings.body_index;
            if(existingForest)
            {
                // The existing footprint is expressed in the unchanged body frame.
                // Only the new trees are sampled against the current provider.
                if(!forest_area_matches_terrain_frame(planet,*existingForest,policy))
                {state.plantLabStatus="Refresh Forest Placement before adding trees to this area.";return false;}
                bodyId=existingForest->body_index;
            }
            else if(state.planetaryBodySelection&&state.planetaryBodySelection->entity==planet.sceneObjectId)
                bodyId=state.planetaryBodySelection->body;
            const auto body=std::find_if(policy.generated_bodies.begin(),policy.generated_bodies.end(),
                [&](const auto& value){return value.id==bodyId;});
            if(body==policy.generated_bodies.end())
            {state.plantLabStatus="Select a generated planet body before placing a plant.";return false;}
            if (findOceanShoreline && (!area || !policy.settings.ocean_surface || body->id != 0u))
            {state.plantLabStatus="Shoreline placement requires a forest area on the generated ocean-bearing body.";return false;}
            auto context=state.projectRuntimeViewportContext.lock();
            if(!existingForest&&!findOceanShoreline&&!state.forestPlacementCamera&&!context)
            {state.plantLabStatus="The project camera is unavailable; no plant was placed.";return false;}
            const auto pivot=scene::world_position(planet.position,planet.positionAnchor);
            if(!pivot)return false;
            const auto transform=previewgrid::make_precise_mesh_transform(*pivot,
                {planet.scale[0],planet.scale[1],planet.scale[2]},
                {planet.rotation[0],planet.rotation[1],planet.rotation[2]});
            if(!transform)return false;
            std::array<double,3> direction{0.0,1.0,0.0}, heading{0.0,0.0,1.0};
            if (existingForest) direction=existingForest->center_direction;
            else if (!findOceanShoreline)
            {
                const auto camera=state.forestPlacementCamera?*state.forestPlacementCamera:previewgrid::camera_for(context.get());
                const auto eye=previewgrid::camera_world_eye(camera);
                for(std::size_t axis=0;axis<3;++axis)
                {
                    double local{};
                    for(std::size_t row=0;row<3;++row)
                        local+=transform->rotation[row*3+axis]*(eye[row]-transform->position[row]);
                    direction[axis]=local/transform->scale[axis]
                        -body->center_meters[axis]+policy.generated_mesh->coordinates.source_origin_meters[axis];
                }
                const auto forward=previewgrid::camera_forward(camera);
                heading={forward.x,forward.y,forward.z};
            }
            if(brushDirection)direction=*brushDirection;
            double square{};for(const auto value:direction)square+=value*value;
            if(!std::isfinite(square)||square<1e-12)
            {state.plantLabStatus="Move the project camera away from the planet center before placing a plant.";return false;}
            for(auto& value:direction)value/=std::sqrt(square);
            const auto root=resolve_editor_path(std::filesystem::path{state.projectRoot});
            std::shared_ptr<const asset::mesh::Artifact> terrain=planet.meshSnapshot;
            if(terrain&&terrain->key().words!=policy.generated_mesh->artifact_key)terrain.reset();
            const auto terrainPath=planet.meshAsset->logical_path;
            const auto terrainLimits=editor_planetary_mesh_budget(state).artifact;
            EditorState::ForestSurfacePlacement intent;
            intent.projectId=state.projectId;intent.projectRoot=state.projectRoot;
            intent.projectGeneration=state.projectLifecycle.evidence().generations.selected_project.generation;
            intent.plantRevision=state.plantLabCompiled.sourceRevision;
            intent.plantHash=state.plantLabCompiled.sourceContentHash;
            intent.entity=planet.sceneObjectId;intent.ordinal=ordinal;
            intent.position=planet.position;intent.rotation=planet.rotation;intent.scale=planet.scale;intent.anchor=planet.positionAnchor;
            intent.heading=heading;
            intent.query={policy.source,policy.settings,*policy.generated_mesh,*body,terrain,direction,++state.forestSurfaceSequence};
            intent.area=area;
            intent.findOceanShoreline=findOceanShoreline;
            intent.existingRegion=existingForest?existingForest->region_id:0u;
            intent.sceneRevision=state.sceneDocumentRevision;
            if (area && !stroke.empty())
            {
                // Four surrounding samples per candidate fit in one admitted
                // surface batch for the shoreline setback.
                constexpr auto maximum = project_planetary::kMaximumSurfaceBatchSamples / 4u;
                if (stroke.size() > maximum) return false;
                for (std::size_t i = 0; i < stroke.size(); ++i)
                {
                    auto stamp = *area;
                    stamp.seed = forest::mix_seed(area->seed, i);
                    const auto candidates = forest::forest_area_directions(stamp, stroke[i], body->radius_meters);
                    const auto count = (std::min)(candidates.size(), maximum / stroke.size() + (i < maximum % stroke.size()));
                    intent.directions.insert(intent.directions.end(), candidates.begin(), candidates.begin() + count);
                    intent.brushCenters.insert(intent.brushCenters.end(), count, stroke[i]);
                }
            }
            else
                intent.directions=area?forest::forest_area_directions(*area,direction,body->radius_meters)
                    :std::vector<std::array<double,3>>{direction};
            if(intent.directions.empty())
            {state.plantLabStatus="The forest area radius or density is invalid; no trees were placed.";return false;}
            auto submitted=schedule_editor_cancellable_task(state,area?"Paint forest area on planetary ground":"Place plant on generated planetary ground",
                [root,query=intent.query,directions=intent.directions,area,findOceanShoreline,terrainPath,terrainLimits,
                    brush=!stroke.empty(),
                    plant=state.plantLabCompiled,profile=state.plantLabDocument.profile,
                    ordinal,projectId=intent.projectId](std::stop_token stop) mutable {
                    EditorState::ForestSurfacePrepared prepared;
                    const auto refused=[&](std::string message) {
                        prepared.surface.code=stop.stop_requested()?project_planetary::SurfaceCode::cancelled:project_planetary::SurfaceCode::invalid;
                        prepared.surface.request_id=query.request_id;prepared.surface.operation.message=std::move(message);
                        return std::move(prepared);
                    };
                    if(stop.stop_requested())return refused("Plant placement cancelled.");
                    if(!query.artifact)
                    {
                        const auto loaded=asset::mesh::read_project_artifact(root,terrainPath,{query.mesh.artifact_key},terrainLimits);
                        if(!loaded)return refused("The saved terrain mesh could not be verified.");
                        query.artifact=loaded.artifact;
                    }
                    if(stop.stop_requested())return refused("Plant placement cancelled.");
                    if (findOceanShoreline)
                    {
                        auto shore = project_planetary::query_ocean_shoreline(root,query,100.0,stop);
                        if (shore.code != project_planetary::SurfaceCode::ready)
                        {
                            prepared.surface.code = shore.code;
                            prepared.surface.operation = std::move(shore.operation);
                            prepared.surface.request_id = query.request_id;
                            return prepared;
                        }
                        query.direction = shore.land.radial_up;
                        directions = forest::forest_area_directions(*area,query.direction,query.body.radius_meters);
                        if (directions.empty()) return refused("The shoreline forest area is invalid.");
                        prepared.shorelineCenter = query.direction;
                    }
                    if(stop.stop_requested())return refused("Plant placement cancelled.");
                    prepared.prototype=project_compiled_forest_entities(plant,
                        epochengine::format_text("ForestAsset_{:02}_",ordinal),"ForestFactory",false,0,0,true);
                    if(stop.stop_requested())return refused("Plant placement cancelled.");
                    if(prepared.prototype.size()!=1||!persist_forest_scene_mesh(root,prepared.prototype,projectId,profile.barkTileIndex,&profile))
                        return refused("The compiled plant mesh could not be prepared safely.");
                    if(stop.stop_requested())return refused("Plant placement cancelled.");
                    if(area)prepared.surface=query_forest_surfaces(root,{query,directions},stop);
                    else
                    {
                        auto sample=project_planetary::query_surface(root,query,stop);
                        prepared.surface.code=sample.code;prepared.surface.operation=sample.operation;
                        prepared.surface.request_id=sample.request_id;
                        if(sample.code==project_planetary::SurfaceCode::ready)prepared.surface.samples.push_back(std::move(sample));
                    }
                    if(brush&&query.settings.ocean_surface&&prepared.surface.code==project_planetary::SurfaceCode::ready)
                    {
                        // Verify land along four 24-metre offsets around each
                        // painted root. The provider supplies the real height.
                        constexpr double setback=24.0;
                        const double angle=setback/query.body.radius_meters;
                        const double radial=std::cos(angle),lateral=std::sin(angle);
                        std::vector<std::array<double,3>> probes;
                        probes.reserve(directions.size()*4u);
                        for(const auto& point:directions)
                        {
                            std::array<double,3> east=std::abs(point[1])<0.9
                                ?std::array<double,3>{point[2],0.0,-point[0]}
                                :std::array<double,3>{0.0,-point[2],point[1]};
                            const double length=std::hypot(east[0],east[1],east[2]);
                            for(auto& axis:east)axis/=length;
                            const std::array<double,3> north{point[1]*east[2]-point[2]*east[1],
                                point[2]*east[0]-point[0]*east[2],point[0]*east[1]-point[1]*east[0]};
                            for(const auto& tangent:{east,north})
                                for(double sign:{-1.0,1.0})
                                    probes.push_back({point[0]*radial+sign*tangent[0]*lateral,
                                        point[1]*radial+sign*tangent[1]*lateral,point[2]*radial+sign*tangent[2]*lateral});
                        }
                        const auto neighborhood=query_forest_surfaces(root,{query,probes},stop);
                        if(neighborhood.code!=project_planetary::SurfaceCode::ready
                            ||neighborhood.request_id!=query.request_id||neighborhood.samples.size()!=probes.size())
                            return refused("Could not verify the shoreline setback for this stroke.");
                        for(std::size_t i=0;i<neighborhood.samples.size();i+=4u)
                        {
                            const auto first=neighborhood.samples.begin()+i;
                            prepared.brushInland.push_back(std::all_of(first,first+4,[](const auto& sample){
                                return sample.code==project_planetary::SurfaceCode::ready&&sample.height_meters>0.0;
                            }));
                        }
                    }
                    return prepared;
                });
            state.forestSurfacePlacement=std::move(intent);
            state.forestSurfaceCancellation=submitted.cancellation;
            state.forestSurfacePending.emplace(std::move(submitted.completion));
            state.forestPlacementSaved.reset();
            state.plantLabStatus=existingForest?"Sampling space for the current Plant Lab tree in the selected forest..."
                :findOceanShoreline?"Finding dry ground beside verified ocean water..."
                :!stroke.empty()?"Checking spacing and shoreline clearance for the painted forest..."
                :area?"Sampling the forest area beneath the project camera (maximum 64 trees)..."
                :"Finding dry terrain beneath the project camera...";
            push_editor_log(state,"[forest] "+state.plantLabStatus);
            return true;
        }

        [[nodiscard]] std::array<double, 3> terrain_radial_direction(const previewgrid::PreciseMeshTransform& terrain,
            const std::array<double, 3>& direction) noexcept
        {
            std::array<double, 3> up{};
            for (std::size_t row = 0; row < up.size(); ++row)
                for (std::size_t axis = 0; axis < up.size(); ++axis)
                    up[row] += terrain.rotation[row * 3 + axis] * terrain.scale[axis] * direction[axis];
            const auto length = std::hypot(up[0], up[1], up[2]);
            for (auto& axis : up) axis /= length;
            return up;
        }

        [[nodiscard]] std::optional<double> terrain_attachment_support(const scene::SceneObjectSnapshot& object,
            const asset::mesh::Artifact& mesh, const std::array<double, 3>& worldUp, std::stop_token stop)
        {
            const auto scale = scene::mesh_render_scale(object.scale, object.mesh_asset);
            const auto basis = scale ? previewgrid::make_precise_mesh_transform({}, {(*scale)[0], (*scale)[1], (*scale)[2]},
                {object.rotation[0], object.rotation[1], object.rotation[2]}) : std::nullopt;
            if (!basis || mesh.vertices().empty()) return {};
            double minimum = std::numeric_limits<double>::infinity();
            std::size_t index{};
            for (const auto& vertex : mesh.vertices())
            {
                if ((index++ & 4095u) == 0u && stop.stop_requested()) return {};
                const auto p = basis->apply({vertex.position[0], vertex.position[1], vertex.position[2]});
                minimum = (std::min)(minimum, p[0] * worldUp[0] + p[1] * worldUp[1] + p[2] * worldUp[2]);
            }
            return std::isfinite(minimum) ? std::optional{-minimum} : std::nullopt;
        }

        [[nodiscard]] const EditorEntity* selected_terrain_attachment_refresh_target(const EditorState& state)
        {
            if (state.selectedEntityIds.size() > 1u) return nullptr;
            const auto index = editor_entity_index(state, state.selectedEntityId);
            if (!index) return nullptr;
            const auto& entity = state.entities[*index];
            return entity.category == "Planetary Terrain" || entity.terrainAttachment ? &entity : nullptr;
        }

        [[nodiscard]] bool begin_forest_surface_refresh(EditorState& state, bool attachments = false)
        {
            const auto fail=[&](std::string message) {
                auto& status = attachments ? state.projectStatus : state.plantLabStatus;
                status = std::move(message);
                push_editor_log(state, (attachments ? "[scene] " : "[forest] ") + status);
                return false;
            };
            if(!has_open_project(state)||state.forestSurfacePending||state.projectBuildPending
                ||state.planetaryGeneratePending||state.projectRuntimeProcess.valid()||state.modelShorelinePending
                ||(state.projectRuntimeSnapshot&&state.projectRuntimeSnapshot->active()))
                return fail("Stop the project and finish terrain work before refreshing placement.");
            if(!state.sceneDocument.initialized())
                return fail("Save the current scene before refreshing forest placement.");
            if(attachments&&!selected_terrain_attachment_refresh_target(state))
                return fail("Select the generated terrain, an attached model, or PlayerStart.");
            const auto selected=editor_entity_index(state,state.selectedEntityId);
            const auto regionId=selected&&state.entities[*selected].forestRegion
                ?state.entities[*selected].forestRegion->region_id:0u;
            const auto terrainId=attachments && selected && state.entities[*selected].terrainAttachment
                ? state.entities[*selected].terrainAttachment->terrain_object_id
                : regionId?state.entities[*selected].forestRegion->terrain_object_id
                :selected&&state.entities[*selected].category=="Planetary Terrain"?state.selectedEntityId:0u;
            if (attachments && !terrainId) return fail("Select the generated terrain, an attached model, or PlayerStart.");
            const auto terrain=std::find_if(state.entities.begin(),state.entities.end(),[&](const auto& entity) {
                return (!terrainId||entity.sceneObjectId==terrainId)&&editor_planetary_stream_identity_matches(state,entity);
            });
            if(terrain==state.entities.end())return fail("The selected object has no current generated terrain to refresh against.");
            refresh_editor_planetary_body_inventory(state,*terrain);
            const auto& policy=*state.planetaryMeshPolicy;
            if(!policy.body_inventory_checked||!policy.generated_mesh)return fail("The generated terrain body inventory is unavailable; placement is unchanged.");
            EditorState::ForestSurfacePlacement intent;
            intent.projectId=state.projectId;intent.projectRoot=state.projectRoot;
            intent.projectGeneration=state.projectLifecycle.evidence().generations.selected_project.generation;
            intent.sceneRevision=state.sceneDocument.revision().sequence;
            intent.refreshAttachments=attachments;
            intent.entity=terrain->sceneObjectId;intent.position=terrain->position;intent.rotation=terrain->rotation;
            intent.scale=terrain->scale;intent.anchor=terrain->positionAnchor;
            const auto pivot=scene::world_position(terrain->position,terrain->positionAnchor);
            const auto transform=pivot?previewgrid::make_precise_mesh_transform(*pivot,
                {terrain->scale[0],terrain->scale[1],terrain->scale[2]},
                {terrain->rotation[0],terrain->rotation[1],terrain->rotation[2]}):std::nullopt;
            if(!transform)return fail("The terrain transform is invalid.");
            for(const auto& object:state.entities)
            {
                std::uint32_t bodyId{};
                std::array<double,3> direction{};
                if (attachments)
                {
                    if (!object.terrainAttachment || object.terrainAttachment->terrain_object_id != intent.entity) continue;
                    const auto& attachment = *object.terrainAttachment;
                    const auto point = scene::world_position(object.position, object.positionAnchor);
                    if (!point || !attachment.valid() || (object.type != "Spawn" && !object.meshAsset))
                        return fail("An attached object has no valid saved position or mesh.");
                    direction = transform->inverse_apply(*point);
                    for (std::size_t axis = 0; axis < direction.size(); ++axis)
                        direction[axis] -= attachment.reference_center_meters[axis];
                    const auto radius = std::hypot(direction[0], direction[1], direction[2]);
                    if (!std::isfinite(radius) || radius <= 0.0) return fail("An attached object has no recoverable surface direction.");
                    for (auto& axis : direction) axis /= radius;
                    bodyId = attachment.body_index;
                }
                else
                {
                    if(object.category!="ForestFactory"||object.type!="ForestTrunk"||!object.forestRegion
                        ||object.forestRegion->terrain_object_id!=intent.entity
                        ||(regionId&&object.forestRegion->region_id!=regionId))continue;
                    const auto& region=*object.forestRegion;
                    if(!region.valid()) return fail("A painted tree has no valid surface direction.");
                    direction=region.radial_up;bodyId=region.body_index;
                }
                const auto body=std::find_if(policy.generated_bodies.begin(),policy.generated_bodies.end(),
                    [&](const auto& value){return value.id==bodyId;});
                if(body==policy.generated_bodies.end()||!state.sceneDocument.find({object.sceneObjectId}))
                    return fail("A saved object has no matching generated body; placement is unchanged.");
                intent.refreshObjects.push_back(editor_entity_snapshot(object));intent.refreshBodies.push_back(*body);
                intent.directions.push_back(direction);
            }
            if(intent.refreshObjects.empty())return fail(attachments ? "This terrain has no attached models or spawns."
                : "There are no painted trees on the selected terrain or forest area.");
            if(!attachments&&intent.refreshObjects.size()>project_planetary::kMaximumSurfaceBatchSamples)
                return fail("Select one forest area to refresh at most 64 trees in a single undoable operation.");
            const auto root=resolve_editor_path(std::filesystem::path{state.projectRoot});
            std::shared_ptr<const asset::mesh::Artifact> artifact=terrain->meshSnapshot;
            if(artifact&&artifact->key().words!=policy.generated_mesh->artifact_key)artifact.reset();
            intent.query={policy.source,policy.settings,*policy.generated_mesh,intent.refreshBodies.front(),artifact,
                intent.directions.front(),++state.forestSurfaceSequence};
            const auto terrainPath=terrain->meshAsset->logical_path;
            const auto terrainLimits=editor_planetary_mesh_budget(state).artifact;
            auto submitted=schedule_editor_cancellable_task(state,attachments ? "Refresh models and spawns on generated terrain"
                : "Refresh forest placement on generated terrain",
                [root,query=intent.query,bodies=intent.refreshBodies,directions=intent.directions,
                    objects=attachments ? intent.refreshObjects : std::vector<scene::SceneObjectSnapshot>{},
                    transform=*transform,terrainPath,terrainLimits](std::stop_token stop) mutable {
                    EditorState::ForestSurfacePrepared prepared;
                    auto& result=prepared.surface;result.request_id=query.request_id;
                    const auto refused=[&](std::string message) {
                        result.code=stop.stop_requested()?project_planetary::SurfaceCode::cancelled:project_planetary::SurfaceCode::invalid;
                        result.operation.message=std::move(message);result.samples.clear();return std::move(prepared);
                    };
                    if(stop.stop_requested())return refused("Forest refresh cancelled.");
                    if(!query.artifact)
                    {
                        const auto loaded=asset::mesh::read_project_artifact(root,terrainPath,{query.mesh.artifact_key},terrainLimits);
                        if(!loaded)return refused("The saved terrain mesh could not be verified.");
                        query.artifact=loaded.artifact;
                    }
                    result.samples.resize(directions.size());
                    for(std::size_t first=0u;first<bodies.size();++first)
                    {
                        if(std::find(bodies.begin(),bodies.begin()+first,bodies[first])!=bodies.begin()+first)continue;
                        if(stop.stop_requested())return refused("Forest refresh cancelled.");
                        std::vector<std::array<double,3>> bodyDirections;std::vector<std::size_t> indices;
                        for(std::size_t index=first;index<bodies.size();++index)
                            if(bodies[index]==bodies[first]){bodyDirections.push_back(directions[index]);indices.push_back(index);}
                        query.body=bodies[first];
                        for (std::size_t offset = 0; offset < indices.size(); offset += project_planetary::kMaximumSurfaceBatchSamples)
                        {
                            const auto count = (std::min)(indices.size() - offset, project_planetary::kMaximumSurfaceBatchSamples);
                            const auto from = bodyDirections.begin() + offset;
                            query.direction = *from;
                            auto batch = query_forest_surfaces(root, {query, {from, from + count}}, stop);
                            if(batch.code!=project_planetary::SurfaceCode::ready||batch.request_id!=query.request_id
                                ||batch.samples.size()!=count)
                            {
                                result=std::move(batch);result.samples.clear();
                                if(result.code==project_planetary::SurfaceCode::ready)
                                {result.code=project_planetary::SurfaceCode::invalid;result.operation.message="The terrain returned an incomplete or mismatched placement batch.";}
                                return prepared;
                            }
                            for(std::size_t sample=0u;sample<count;++sample)
                                result.samples[indices[offset+sample]]=std::move(batch.samples[sample]);
                        }
                    }
                    prepared.groundSupports.resize(objects.size());
                    for (std::size_t index = 0; index < objects.size(); ++index)
                    {
                        if(stop.stop_requested())return refused("Placement refresh cancelled.");
                        const auto& object = objects[index];
                        if (!object.mesh_asset) continue;
                        const auto mesh = asset::mesh::read_project_artifact(root, object.mesh_asset->logical_path,
                            {object.mesh_asset->artifact_key}, terrainLimits);
                        if (!mesh) return refused("An attached model mesh could not be verified.");
                        const auto support = terrain_attachment_support(object, *mesh.artifact,
                            terrain_radial_direction(transform, directions[index]), stop);
                        if (!support) return refused("An attached model's ground support could not be measured.");
                        prepared.groundSupports[index] = *support;
                    }
                    // Parts imported together share an exact placement. Use
                    // their lowest vertex across the assembly for one pivot.
                    std::vector<bool> grouped(objects.size());
                    for (std::size_t first = 0; first < objects.size(); ++first)
                    {
                        if(stop.stop_requested())return refused("Placement refresh cancelled.");
                        const auto& object = objects[first];
                        if (grouped[first] || !object.mesh_asset) continue;
                        const auto& original = *object.terrain_attachment;
                        std::vector<std::size_t> parts{first};
                        double support = prepared.groundSupports[first];
                        for (std::size_t other = first + 1; other < objects.size(); ++other)
                        {
                            const auto& part = objects[other];
                            const auto& link = part.terrain_attachment;
                            if (!grouped[other] && part.mesh_asset && link && link->body_index == original.body_index
                                && link->reference_center_meters == original.reference_center_meters
                                && link->terrain_artifact_key == original.terrain_artifact_key
                                && link->terrain_coordinates == original.terrain_coordinates
                                && part.position_anchor == object.position_anchor && part.position == object.position
                                && part.rotation == object.rotation && part.scale == object.scale)
                            {
                                grouped[other] = true;
                                parts.push_back(other);
                                support = (std::max)(support, prepared.groundSupports[other]);
                            }
                        }
                        for (const auto part : parts) prepared.groundSupports[part] = support;
                    }
                    if(stop.stop_requested())return refused("Placement refresh cancelled.");
                    result.code=project_planetary::SurfaceCode::ready;return prepared;
                });
            const auto count=intent.refreshObjects.size();state.forestSurfacePlacement=std::move(intent);
            state.forestSurfaceCancellation=submitted.cancellation;
            state.forestSurfacePending.emplace(std::move(submitted.completion));
            auto& status=attachments ? state.projectStatus : state.plantLabStatus;
            status=epochengine::format_text(attachments ? "Sampling current ground for {} attached model part(s) or spawns..."
                : "Refreshing {} tree root(s) against current terrain; tree designs and area layout are retained...",count);
            push_editor_log(state,(attachments ? "[scene] " : "[forest] ")+status);return true;
        }

        void complete_forest_surface_refresh(EditorState& state,const EditorState::ForestSurfacePlacement& intent,
            const project_planetary::SurfaceBatchResult& result, std::span<const double> supports)
        {
            const bool attachments = intent.refreshAttachments;
            auto& status = attachments ? state.projectStatus : state.plantLabStatus;
            const std::string_view log = attachments ? "[scene] " : "[forest] ";
            const auto fail=[&](std::string message) {
                status=(attachments ? "Model and spawn placement unchanged: " : "Forest placement unchanged: ")+std::move(message);
                push_editor_log(state,std::string{log}+status);
            };
            const auto index=editor_entity_index(state,intent.entity);
            if(state.projectId!=intent.projectId||state.projectRoot!=intent.projectRoot
                ||state.projectLifecycle.evidence().generations.selected_project.generation!=intent.projectGeneration
                ||!index||!state.planetaryMeshPolicy||state.projectRuntimeProcess.valid()||state.projectBuildPending
                ||state.planetaryGeneratePending||state.sceneDocument.revision().sequence!=intent.sceneRevision
                ||(state.projectRuntimeSnapshot&&state.projectRuntimeSnapshot->active()))
            {fail("the project, scene or running state changed during refresh.");return;}
            const auto& planet=state.entities[*index];const auto& policy=*state.planetaryMeshPolicy;
            const auto root=resolve_editor_path(std::filesystem::path{state.projectRoot});
            if(planet.position!=intent.position||planet.rotation!=intent.rotation||planet.scale!=intent.scale
                ||planet.positionAnchor!=intent.anchor||!editor_planetary_stream_identity_matches(state,planet)
                ||policy.settings!=intent.query.settings||policy.generated_mesh!=intent.query.mesh
                ||policy.source.content_hash!=intent.query.source.content_hash
                ||!project_planetary::source_generation_is_current(root,intent.query.source)
                ||!project_planetary::mesh_generation_is_current(root,intent.query.source,intent.query.settings,intent.query.mesh))
            {fail("the approved terrain source, settings, geometry or transform changed.");return;}
            const auto& objects = intent.refreshObjects;
            if(objects.empty()||(!attachments&&objects.size()>project_planetary::kMaximumSurfaceBatchSamples)
                ||objects.size()!=intent.refreshBodies.size()||objects.size()!=intent.directions.size()
                ||(attachments && supports.size()!=objects.size())
                ||result.code!=project_planetary::SurfaceCode::ready||result.request_id!=intent.query.request_id
                ||result.samples.size()!=objects.size())
            {fail(result.operation.message.empty()?"the surface query returned no complete verified batch.":result.operation.message);return;}
            const auto pivot=scene::world_position(planet.position,planet.positionAnchor);
            const auto transform=pivot?previewgrid::make_precise_mesh_transform(*pivot,
                {planet.scale[0],planet.scale[1],planet.scale[2]},
                {planet.rotation[0],planet.rotation[1],planet.rotation[2]}):std::nullopt;
            if(!transform){fail("the planet transform is invalid.");return;}
            std::vector<authoring::scene::SceneOperation> operations;operations.reserve(objects.size());
            for(std::size_t sampleIndex=0;sampleIndex<objects.size();++sampleIndex)
            {
                const auto& before=objects[sampleIndex];const auto object=editor_entity_index(state,before.id);
                const auto& body=intent.refreshBodies[sampleIndex];const auto& sample=result.samples[sampleIndex];
                if(!object||!(attachments ? before.terrain_attachment.has_value() : before.forest_region.has_value())
                    ||std::find(policy.generated_bodies.begin(),policy.generated_bodies.end(),body)==policy.generated_bodies.end())
                {fail("an object or its generated body changed.");return;}
                const auto current=editor_entity_snapshot(state.entities[*object]);
                if(current.id!=before.id||current.name!=before.name||current.type!=before.type||current.category!=before.category
                    ||current.position!=before.position||current.rotation!=before.rotation||current.scale!=before.scale
                    ||current.visible!=before.visible||current.editor_only!=before.editor_only
                    ||current.mesh_asset!=before.mesh_asset||current.texture_material!=before.texture_material
                    ||current.position_anchor!=before.position_anchor||current.forest_region!=before.forest_region
                    ||current.terrain_attachment!=before.terrain_attachment)
                {fail("an object was edited during refresh.");return;}
                if(sample.code!=project_planetary::SurfaceCode::ready||sample.request_id!=intent.query.request_id
                    ||!std::isfinite(sample.height_meters))
                {fail("one terrain sample was invalid.");return;}
                if((attachments && body.id==0u && sample.height_meters<=0.0)
                    ||(!attachments && intent.query.settings.ocean_surface&&sample.height_meters<0.0))
                {fail("a saved placement is now underwater.");return;}
                for(std::size_t axis=0;axis<sample.radial_up.size();++axis)
                    if(!std::isfinite(sample.radial_up[axis])||std::abs(sample.radial_up[axis]-intent.directions[sampleIndex][axis])>1e-6)
                    {fail("the provider returned ground for a different direction.");return;}
                const auto handle=state.sceneDocument.find({before.id});
                if(!handle){fail("an object was removed.");return;}
                if (attachments)
                {
                    if (before.type == "Spawn")
                    {
                        double grade{};
                        for (std::size_t axis = 0; axis < 3; ++axis) grade += sample.normal[axis] * sample.radial_up[axis];
                        if (!std::isfinite(grade) || grade < 0.85)
                        {fail("the saved spawn is now on a steep slope.");return;}
                    }
                    const auto& old = *before.terrain_attachment;
                    const auto support = supports[sampleIndex];
                    auto link = old;
                    link.terrain_artifact_key = intent.query.mesh.artifact_key;
                    link.terrain_coordinates = {intent.query.mesh.coordinates.meters_per_unit, intent.query.mesh.coordinates.source_origin_meters};
                    std::array<double, 3> local{};
                    for (std::size_t axis = 0; axis < local.size(); ++axis)
                    {
                        link.reference_center_meters[axis] = body.center_meters[axis] - link.terrain_coordinates.source_origin_meters[axis];
                        local[axis] = link.reference_center_meters[axis] + sample.point_meters[axis];
                    }
                    auto anchor = transform->apply(local);
                    const auto up = terrain_radial_direction(*transform, sample.radial_up);
                    for (std::size_t axis = 0; axis < anchor.size(); ++axis)
                        anchor[axis] += up[axis] * support - before.position[axis];
                    if (!link.valid() || !scene::world_position(before.position, anchor))
                    {fail("a refreshed model or spawn position is invalid.");return;}
                    if (before.position_anchor == std::optional{anchor} && old == link) continue;
                    operations.emplace_back(authoring::scene::GeometryComponentChangedOperation{
                        .object=*handle, .mesh_asset=before.mesh_asset, .position_anchor=anchor, .terrain_attachment=link});
                    continue;
                }
                auto region=*before.forest_region;region.body_local_root_meters=sample.point_meters;region.radial_up=sample.radial_up;
                region.admitted_body_center_meters=body.center_meters;region.body_radius_meters=body.radius_meters;
                region.mesh_source_origin_meters=intent.query.mesh.coordinates.source_origin_meters;
                region.terrain_artifact_key=intent.query.mesh.artifact_key;
                std::string_view providerHash=intent.query.source.content_hash;
                if(providerHash.starts_with("sha256:"))providerHash.remove_prefix(7u);
                region.provider_content_hash=std::string{providerHash};region.recipe=project_planetary::encode_settings(intent.query.settings);
                if(!region.valid()){fail("the new ground sample has invalid placement evidence.");return;}
                std::array<double,3> local{};
                for(std::size_t axis=0;axis<3u;++axis)
                    local[axis]=body.center_meters[axis]-region.mesh_source_origin_meters[axis]+sample.point_meters[axis];
                auto anchor=transform->apply(local);
                for (std::size_t axis = 0; axis < anchor.size(); ++axis)
                    anchor[axis] -= before.position[axis];
                if(!scene::world_position(before.position,anchor)){fail("the refreshed tree position is invalid.");return;}
                if(before.position_anchor==std::optional{anchor}&&before.forest_region==region)continue;
                operations.emplace_back(authoring::scene::GeometryComponentChangedOperation{
                    .object=*handle,.mesh_asset=before.mesh_asset,.position_anchor=anchor,.forest_region=std::move(region)});
            }
            if(operations.empty())
            {
                status="Placement is already current.";
                push_editor_log(state,std::string{log}+status);return;
            }
            if(!apply_editor_scene_transaction(state,operations,attachments ? "Refresh model and spawn placement" : "Refresh forest placement"))
            {fail("the scene rejected the complete refresh transaction.");return;}
            const auto saved=save_editor_scene_snapshot(state);
            status=epochengine::format_text(attachments ? "{} model part(s) or spawns refreshed{}. Undo restores the previous placement."
                : "{} tree root(s) refreshed{}; designs, materials and area layout retained. Scene Undo restores the previous placement.",
                operations.size(),saved?" and saved":"; scene save failed, retry Save before closing");
            push_editor_log(state,std::string{log}+status);
        }

        void poll_forest_surface_placement(EditorState& state)
        {
            if(!state.forestSurfacePending)return;
            if(!state.forestSurfacePlacement&&state.taskScheduler)
                (void)state.taskScheduler->cancel(state.forestSurfaceCancellation);
            if(state.forestSurfacePending->wait_for(std::chrono::seconds{0})!=std::future_status::ready)return;
            const bool attachments = state.forestSurfacePlacement && state.forestSurfacePlacement->refreshAttachments;
            try
            {
                auto prepared=state.forestSurfacePending->get();state.forestSurfacePending.reset();
                auto& result=prepared.surface;
                auto intent=std::move(state.forestSurfacePlacement);state.forestSurfacePlacement.reset();
                if(!intent)return;
                if(!intent->refreshObjects.empty())
                {complete_forest_surface_refresh(state,*intent,result,prepared.groundSupports);return;}
                const auto fail=[&](std::string message){
                    state.forestPlacementSaved=false;
                    state.plantLabStatus=std::move(message);
                    push_editor_log(state,"[forest] "+state.plantLabStatus);
                };
                const auto index=editor_entity_index(state,intent->entity);
                if(state.projectId!=intent->projectId||state.projectRoot!=intent->projectRoot
                    ||state.projectLifecycle.evidence().generations.selected_project.generation!=intent->projectGeneration
                    ||(intent->existingRegion && state.sceneDocumentRevision!=intent->sceneRevision)
                    ||!index||!state.planetaryMeshPolicy||state.projectRuntimeProcess.valid()||state.projectBuildPending
                    ||!ensure_plant_lab_compiled_asset(state)
                    ||state.plantLabCompiled.sourceRevision!=intent->plantRevision||state.plantLabCompiled.sourceContentHash!=intent->plantHash)
                {fail("Plant placement was cancelled because its project, plant or running state changed.");return;}
                const auto& planet=state.entities[*index];const auto& policy=*state.planetaryMeshPolicy;
                const auto root=resolve_editor_path(std::filesystem::path{state.projectRoot});
                if(planet.position!=intent->position||planet.rotation!=intent->rotation||planet.scale!=intent->scale
                    ||planet.positionAnchor!=intent->anchor||!editor_planetary_stream_identity_matches(state,planet)
                    ||policy.settings!=intent->query.settings||policy.generated_mesh!=intent->query.mesh
                    ||std::find(policy.generated_bodies.begin(),policy.generated_bodies.end(),intent->query.body)==policy.generated_bodies.end()
                    ||!project_planetary::source_generation_is_current(root,intent->query.source)
                    ||!project_planetary::mesh_generation_is_current(root,intent->query.source,intent->query.settings,intent->query.mesh))
                {fail("Plant placement was cancelled because the generated planet changed.");return;}
                if(result.code!=project_planetary::SurfaceCode::ready||result.request_id!=intent->query.request_id)
                {fail(result.code==project_planetary::SurfaceCode::busy
                    ?"Terrain is busy. Retry Add to Scene after its current operation finishes."
                    :"No plant was added: "+result.operation.message);return;}
                if (intent->findOceanShoreline)
                {
                    if (!intent->area || !prepared.shorelineCenter)
                    {fail("The shoreline search returned no verified location.");return;}
                    intent->query.direction = *prepared.shorelineCenter;
                    intent->directions = forest::forest_area_directions(*intent->area,
                        intent->query.direction,intent->query.body.radius_meters);
                }
                if(result.samples.size()!=intent->directions.size()
                    ||(!intent->brushCenters.empty()&&intent->brushCenters.size()!=result.samples.size())
                    ||(!prepared.brushInland.empty()&&prepared.brushInland.size()!=result.samples.size()))
                {fail("The terrain returned an incomplete forest sample set; no trees were placed.");return;}
                const auto pivot=scene::world_position(planet.position,planet.positionAnchor);
                if(!pivot){fail("Planet transform is invalid.");return;}
                const auto transform=previewgrid::make_precise_mesh_transform(*pivot,
                    {planet.scale[0],planet.scale[1],planet.scale[2]},
                    {planet.rotation[0],planet.rotation[1],planet.rotation[2]});
                if(!transform){fail("Planet transform is invalid.");return;}
                auto prototype=std::move(prepared.prototype);
                if(prototype.size()!=1 || !prototype.front().meshSnapshot)
                {fail("The compiled plant mesh is invalid.");return;}
                const auto& geometry=*prototype.front().meshSnapshot;
                const auto perInstance=64u+std::uint64_t{geometry.vertices().size()}*sizeof(asset::mesh::Vertex)
                    +std::uint64_t{geometry.indices().size()}*sizeof(std::uint32_t);
                // Charge each published instance even when the meshes share bytes.
                std::uint64_t ordinaryBytes{},ordinaryIndices{},sceneBytes{},meshEntries{};
                for(const auto& entity:state.entities)
                    if(entity.meshAsset)
                    {
                        ++meshEntries;
                        const auto& mesh=*entity.meshAsset;
                        const auto bytes=64u+std::uint64_t{mesh.vertex_count}*sizeof(asset::mesh::Vertex)
                            +std::uint64_t{mesh.index_count}*sizeof(std::uint32_t);
                        sceneBytes+=bytes;
                        if(entity.category!="Planetary Terrain")
                        {ordinaryBytes+=bytes;if(entity.visible)ordinaryIndices+=mesh.index_count;}
                    }
                const auto budget=editor_planetary_mesh_budget(state);
                const auto available=[](std::uint64_t maximum,std::uint64_t used,std::uint64_t each) {
                    return each&&maximum>used?(maximum-used)/each:0u;};
                const auto sceneCapacity=(std::min)({available(64u,meshEntries,1u),
                    available(16ull*1024u*1024u,ordinaryBytes,perInstance),
                    available(196'608u,ordinaryIndices,geometry.indices().size()),
                    available(budget.cache_bytes,sceneBytes,perInstance)});
                if(!sceneCapacity)
                {fail("The scene mesh budget is full. No trees were added.");return;}
                std::vector<EditorEntity> placed;placed.reserve(result.samples.size());
                auto regionId=intent->existingRegion;
                std::vector<std::array<double,3>> occupiedRoots;
                std::string plantSource{kDefaultPlantLabLogicalPath};
                if(regionId)
                {
                    for(const auto& entity:state.entities)
                        if(entity.forestRegion&&entity.forestRegion->region_id==regionId)
                        {
                            const auto rootPosition=scene::world_position(entity.position,entity.positionAnchor);
                            if(!rootPosition){fail("An existing forest root is invalid.");return;}
                            auto local=transform->inverse_apply(*rootPosition);
                            for(std::size_t axis=0;axis<local.size();++axis)
                                local[axis]+=intent->query.mesh.coordinates.source_origin_meters[axis]
                                    -intent->query.body.center_meters[axis];
                            const double length=std::hypot(local[0],local[1],local[2]);
                            if(!std::isfinite(length)||length<=0.0)
                            {fail("An existing forest root has no valid surface direction.");return;}
                            for(auto& axis:local)axis/=length;
                            occupiedRoots.push_back(local);
                        }
                    if(occupiedRoots.empty()||occupiedRoots.size()>=forest::kMaximumForestAreaInstances)
                    {fail("The reviewed forest area is missing or full.");return;}
                    plantSource=epochengine::format_text("Assets/Forest/species_{}_{}_{}.epoch_forest",
                        state.plantLabCompiled.genome.stableHash,intent->plantHash,intent->plantRevision);
                }
                else
                {
                    regionId=scene::stable_scene_object_id(editor_scene_identity(state),
                        "ForestArea_"+std::to_string(intent->query.request_id)+"_"+std::to_string(intent->ordinal));
                    while(std::any_of(state.entities.begin(),state.entities.end(),[&](const auto& entity){
                        return entity.forestRegion&&entity.forestRegion->region_id==regionId;}))
                        regionId=forest::mix_seed(regionId,1u);
                    if(!intent->brushCenters.empty())
                        for(const auto& entity:state.entities)
                            if(const auto& region=entity.forestRegion;region&&region->valid()
                                &&region->terrain_object_id==intent->entity&&region->body_index==intent->query.body.id)
                                occupiedRoots.push_back(region->radial_up);
                }
                std::vector<std::size_t> drySamples;
                drySamples.reserve(result.samples.size());
                for(std::size_t sampleIndex=0;sampleIndex<result.samples.size();++sampleIndex)
                {
                    const auto& sample=result.samples[sampleIndex];
                    if(sample.code!=project_planetary::SurfaceCode::ready||!std::isfinite(sample.height_meters))
                    {fail("The terrain returned an invalid sample; no trees were placed.");return;}
                    if(intent->query.settings.ocean_surface&&sample.height_meters<0)continue;
                    if(!prepared.brushInland.empty()&&(!prepared.brushInland[sampleIndex]||sample.height_meters<2.0))continue;
                    drySamples.push_back(sampleIndex);
                }
                const auto plantBudget = intent->brushCenters.empty() ? drySamples.size()
                    : (std::min)(drySamples.size(), sceneCapacity);
                for (std::size_t candidate = 0; candidate < plantBudget; ++candidate)
                {
                    const auto sampleIndex = drySamples[candidate * drySamples.size() / plantBudget];
                    const auto& sample = result.samples[sampleIndex];
                    if(intent->existingRegion||!intent->brushCenters.empty())
                    {
                        if((intent->existingRegion&&occupiedRoots.size()==forest::kMaximumForestAreaInstances)
                            ||placed.size()==sceneCapacity)break;
                        const double spacing=intent->existingRegion?1.0:24.0;
                        // Use surface spacing so a height change cannot put a new tree over an old root.
                        if(std::ranges::any_of(occupiedRoots,[&](const auto& point){
                            return std::hypot(point[0]-sample.radial_up[0],point[1]-sample.radial_up[1],
                                point[2]-sample.radial_up[2])*intent->query.body.radius_meters<spacing;})) continue;
                        occupiedRoots.push_back(sample.radial_up);
                    }
                    std::array<double,3> point{},up{};
                    for(std::size_t axis=0;axis<3;++axis)
                        point[axis]=intent->query.body.center_meters[axis]-intent->query.mesh.coordinates.source_origin_meters[axis]+sample.point_meters[axis];
                    const auto world=transform->apply(point);
                    for(std::size_t row=0;row<3;++row)for(std::size_t axis=0;axis<3;++axis)
                        up[row]+=transform->rotation[row*3+axis]*sample.radial_up[axis]*transform->scale[axis];
                    const auto rotation=forest::preview_mesh::surface_rotation(up,intent->heading);
                    if(!rotation){fail("Planetary plant orientation is invalid; no trees were placed.");return;}
                    auto entity=prototype.front();
                    entity.name=intent->existingRegion
                        ?epochengine::format_text("ForestAsset_{}_{}_{}_Trunk",regionId,intent->plantHash,sampleIndex)
                        :intent->area?epochengine::format_text("ForestAsset_{}_{}_Trunk",regionId,sampleIndex)
                        :epochengine::format_text("ForestAsset_{:02}_Trunk",intent->ordinal);
                    entity.positionAnchor=world;entity.rotation=*rotation;
                    if(intent->area)
                    {
                        scene::SceneForestRegionInstance region;
                        region.region_id=regionId;region.terrain_object_id=intent->entity;
                        region.seed=intent->area->seed;region.plant_genome=state.plantLabCompiled.genome.stableHash;
                        region.plant_revision=intent->plantRevision;region.plant_hash=intent->plantHash;
                        region.body_index=intent->query.body.id;region.candidate_index=static_cast<std::uint32_t>(sampleIndex);
                        region.candidate_count=static_cast<std::uint32_t>(result.samples.size());
                        region.radius_meters=intent->area->radiusMeters;region.density_per_square_meter=intent->area->densityPerSquareMeter;
                        region.body_radius_meters=intent->query.body.radius_meters;
                        region.center_direction=intent->brushCenters.empty()?intent->query.direction:intent->brushCenters[sampleIndex];
                        region.body_local_root_meters=sample.point_meters;region.radial_up=sample.radial_up;
                        region.admitted_body_center_meters=intent->query.body.center_meters;
                        region.mesh_source_origin_meters=intent->query.mesh.coordinates.source_origin_meters;
                        region.terrain_artifact_key=intent->query.mesh.artifact_key;
                        std::string_view providerHash=intent->query.source.content_hash;
                        if(providerHash.starts_with("sha256:"))providerHash.remove_prefix(7u);
                        region.provider_content_hash=std::string{providerHash};
                        region.recipe=project_planetary::encode_settings(intent->query.settings);
                        region.plant_source=plantSource;
                        if(!region.valid()){fail("The forest area source identity is invalid; no trees were placed.");return;}
                        entity.forestRegion=std::move(region);
                    }
                    if(!scene::world_position(entity.position,entity.positionAnchor))
                    {fail("The plant root position is invalid; no trees were placed.");return;}
                    placed.push_back(std::move(entity));
                }
                const auto plantedCount=placed.size();
                if(!plantedCount)
                {fail(intent->existingRegion?"No dry, unoccupied ground was found in the selected forest."
                    :!intent->brushCenters.empty()?"No painted roots passed the shoreline and spacing checks."
                    :"The sampled area is underwater; no trees were placed.");return;}
                if(plantedCount>sceneCapacity)
                {fail("The forest area exceeds the existing scene mesh budget. No trees were added.");return;}
                if(!create_editor_scene_entities(state,std::move(placed),intent->existingRegion?"Add tree species to forest"
                    :intent->area?"Paint forest area":"Place plant on planetary ground",
                    [&]{return publish_plant_lab_project_asset(state,nullptr,plantSource);}))
                {fail("Plant placement could not be committed; the scene is unchanged.");return;}
                const bool saved=save_editor_scene_snapshot(state);
                state.aiGoalForestReceipt = saved;
                state.forestPlacementSaved=saved;
                state.plantLabStatus=epochengine::format_text("{} rooted tree(s) added{}.",plantedCount,
                    saved?" and saved":"; scene save failed, retry Save");
                if(intent->existingRegion)
                    state.plantLabStatus+=" The original forest trees were preserved.";
                if (intent->findOceanShoreline)
                    state.plantLabStatus += epochengine::format_text(
                        " Ocean shoreline body {} direction ({}, {}, {}).",intent->query.body.id,
                        intent->query.direction[0],intent->query.direction[1],intent->query.direction[2]);
                state.forestPlacementFocusPending=!intent->area||intent->existingRegion!=0u;
                push_editor_log(state,"[forest] "+state.plantLabStatus);
            }
            catch(const editor_tasks::TaskCancelled&)
            {
                state.forestSurfacePending.reset();state.forestSurfacePlacement.reset();
                if(!attachments)state.forestPlacementSaved=false;
                state.plantLabStatus="Forest surface operation cancelled; the scene is unchanged.";
                push_editor_log(state,"[forest] "+state.plantLabStatus);
            }
            catch(...)
            {
                state.forestSurfacePending.reset();state.forestSurfacePlacement.reset();
                if(!attachments)state.forestPlacementSaved=false;
                auto& status = attachments ? state.projectStatus : state.plantLabStatus;
                status=attachments ? "Model and spawn placement refresh failed. The previous scene is retained."
                    : "Plant surface query failed; no object was added.";
                push_editor_log(state,(attachments ? "[scene] " : "[forest] ")+status);
            }
        }

        [[nodiscard]] bool paint_forest_factory_area(EditorState& state, bool findOceanShoreline = false)
        {
            if(!has_open_project(state)||!ensure_plant_lab_compiled_asset(state))return false;
            const auto selected=editor_entity_index(state,state.selectedEntityId);
            if(!selected||state.entities[*selected].category!="Planetary Terrain")
            {state.plantLabStatus="Select the generated planet in 3D Objects, then Paint Area beneath the project camera.";return false;}
            const auto ordinal=static_cast<std::size_t>(std::count_if(state.entities.begin(),state.entities.end(),
                [](const auto& entity){return entity.category=="ForestFactory"&&entity.type=="ForestTrunk";}));
            return begin_forest_surface_placement(state,state.entities[*selected],ordinal,state.forestArea,{},findOceanShoreline);
        }

        [[nodiscard]] bool fill_painted_forest_areas(EditorState& state)
        {
            const auto selected=editor_entity_index(state,state.selectedEntityId);
            const EditorEntity* terrain=selected ? &state.entities[*selected] : nullptr;
            if(terrain&&terrain->forestRegion)
            {
                const auto parent=editor_entity_index(state,terrain->forestRegion->terrain_object_id);
                terrain=parent ? &state.entities[*parent] : nullptr;
            }
            if(!terrain||terrain->category!="Planetary Terrain"
                ||!editor_planetary_stream_identity_matches(state,*terrain)||!ensure_plant_lab_compiled_asset(state))
            {
                state.plantLabStatus="Select the painted planet or one of its trees to fill painted areas.";
                return false;
            }
            auto area=state.forestArea;
            const auto& policy=*state.planetaryMeshPolicy;
            const auto body=state.planetaryBodySelection&&state.planetaryBodySelection->entity==terrain->sceneObjectId
                ?state.planetaryBodySelection->body:policy.settings.body_index;
            std::vector<std::array<double,3>> centers;
            for(const auto& entity:state.entities)
            {
                if(!entity.forestRegion)continue;
                const auto& region=*entity.forestRegion;
                if(!region.valid()||region.terrain_object_id!=terrain->sceneObjectId||region.body_index!=body
                    ||region.terrain_artifact_key!=terrain->meshAsset->artifact_key)continue;
                area.radiusMeters=(std::min)(area.radiusMeters,region.radius_meters);
                if(std::find(centers.begin(),centers.end(),region.center_direction)==centers.end())
                    centers.push_back(region.center_direction);
            }
            if(centers.empty()||centers.size()>project_planetary::kMaximumSurfaceBatchSamples/4u)
            {
                state.plantLabStatus=centers.empty()?"There are no verified painted areas on the selected body."
                    :"The selected body has more than 16 painted areas. Paint a smaller region per stroke.";
                return false;
            }
            return begin_forest_surface_placement(state,*terrain,state.entities.size(),area,centers.front(),
                false,nullptr,centers);
        }

        [[nodiscard]] const EditorEntity* selected_forest_species_target(const EditorState& state)
        {
            if(state.selectedEntityIds.size()>1u)return nullptr;
            const auto selected=editor_entity_index(state,state.selectedEntityId);
            if(!selected)return nullptr;
            const auto& tree=state.entities[*selected];
            return tree.visible&&tree.forestRegion&&tree.forestRegion->valid()?&tree:nullptr;
        }

        [[nodiscard]] bool refresh_selected_forest_material(EditorState& state)
        {
            const auto report = [&](std::string message, bool success = false) {
                state.plantLabStatus = std::move(message);
                push_editor_log(state, "[forest] " + state.plantLabStatus);
                return success;
            };
            const auto* tree = selected_forest_species_target(state);
            if (!tree || !has_open_project(state) || state.forestSurfacePending
                || state.projectBuildPending || state.planetaryGeneratePending || state.projectRuntimeProcess.valid())
                return report("Select one placed tree and finish build or placement work before refreshing its material.");
            const auto& region = *tree->forestRegion;
            const auto handle = state.sceneDocument.find({tree->sceneObjectId});
            if (!handle) return report("The selected tree no longer exists in the scene.");
            try
            {
                const auto root = resolve_editor_path(std::filesystem::path{state.projectRoot});
                project_forests::ForestAssetLibrary library{state.projectId, root};
                const auto saved = library.load_source(region.plant_source);
                if (!saved || saved.document.revision != region.plant_revision
                    || saved.document.contentHash != region.plant_hash
                    || saved.document.genome.stableHash != region.plant_genome)
                    return report("The exact saved design is unavailable for the selected tree.");
                const auto& profile = saved.document.profile;
                if (profile.barkTexture.empty() && profile.foliageTexture.empty())
                    return report("The selected tree already uses the packaged material.", true);
                const auto atlas = prepare_plant_material_atlas(root, state.projectId, profile);
                if (!atlas) return report(atlas.diagnostic);
                if (tree->textureMaterial && tree->textureMaterial->artifact_key == atlas.artifact.identity.key.words)
                    return report("The selected tree material is already current.", true);
                const auto material = publish_forest_scene_material(root, state.projectId, atlas.artifact, profile.barkTileIndex, true);
                if (!material) return report("The refreshed material could not be published to this project.");
                const std::array<authoring::scene::SceneOperation, 1u> operations{
                    authoring::scene::TextureMaterialComponentChangedOperation{*handle, material}};
                if (!apply_editor_scene_transaction(state, operations, "Refresh selected tree material"))
                    return report("The scene rejected the material update.");
                const bool persisted = save_editor_scene_snapshot(state);
                return report(persisted ? "Selected tree material refreshed and saved. Scene Undo restores its previous material."
                    : "Selected tree material refreshed. Retry Save before closing.", persisted);
            }
            catch (...) { return report("The selected tree's saved material could not be refreshed."); }
        }

        [[nodiscard]] bool add_plant_lab_species_to_forest(EditorState& state)
        {
            if(!has_open_project(state)||!ensure_plant_lab_compiled_asset(state))return false;
            const auto* target=selected_forest_species_target(state);
            if(!target)
            {state.plantLabStatus="Select a tree from the forest you want to add to.";return false;}
            const auto& region=*target->forestRegion;
            const auto terrain=editor_entity_index(state,region.terrain_object_id);
            if(!terrain)
            {state.plantLabStatus="The selected forest has no generated terrain.";return false;}
            std::size_t regionSize{};
            for(const auto& entity:state.entities)
            {
                if(!entity.forestRegion||entity.forestRegion->region_id!=region.region_id)continue;
                ++regionSize;
                if(entity.forestRegion->plant_hash==state.plantLabCompiled.sourceContentHash)
                {state.plantLabStatus="This forest already contains the current Plant Lab design.";return false;}
            }
            if(regionSize>=forest::kMaximumForestAreaInstances)
            {state.plantLabStatus="The selected forest area has reached its tree limit.";return false;}
            const forest::ForestAreaDistribution area{region.radius_meters,region.density_per_square_meter,
                forest::mix_seed(region.seed,state.plantLabCompiled.sourceContentHash)};
            return begin_forest_surface_placement(state,state.entities[*terrain],state.entities.size(),area,{},false,&region);
        }

        [[nodiscard]] bool erase_selected_forest_area(EditorState& state)
        {
            if(state.forestSurfacePending||state.projectBuildPending||state.projectRuntimeProcess.valid())return false;
            const auto selected=editor_entity_index(state,state.selectedEntityId);
            if(!selected||!state.entities[*selected].forestRegion)
            {state.plantLabStatus="Select a tree from a painted forest area before Erase Selected Area.";return false;}
            if(!state.sceneDocument.initialized()&&!rebuild_editor_scene_document(state,"Initialize scene before forest erase"))return false;
            const auto region=state.entities[*selected].forestRegion->region_id;
            std::vector<authoring::scene::SceneOperation> operations;
            for(const auto& object:state.sceneDocument.objects())
                if(object.descriptor.forest_region&&object.descriptor.forest_region->region_id==region)
                    operations.emplace_back(authoring::scene::ObjectDestroyedOperation{.object=object.handle});
            if(operations.empty()||operations.size()>forest::kMaximumForestAreaInstances
                ||!apply_editor_scene_transaction(state,operations,"Erase forest area"))return false;
            state.plantLabStatus=save_editor_scene_snapshot(state)?"Forest area erased and saved. Scene Undo restores every tree."
                :"Forest area erased; scene save failed, retry Save.";
            return true;
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

            if(const auto selected=editor_entity_index(state,state.selectedEntityId);
                selected&&state.entities[*selected].category=="Planetary Terrain")
            {
                (void)begin_forest_surface_placement(state,state.entities[*selected],assetOrdinal);
                return 0u;
            }

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
            if (!persist_forest_scene_mesh(state, placed))
            {
                state.plantLabStatus = "The plant mesh could not be saved safely in this project. No scene object was added.";
                push_editor_log(state, "[forest] " + state.plantLabStatus);
                return 0u;
            }
            if (!create_editor_scene_entities(
                    state,
                    std::move(placed),
                    epochengine::format_text(
                        "Place Forest Factory asset {:02}",
                        assetOrdinal),
                    [&state]
                    {
                        if (publish_plant_lab_project_asset(state)) return true;
                        state.plantLabStatus = "Plant source could not be saved; no objects were added to the scene.";
                        return false;
                    }))
            {
                push_editor_log(
                    state,
                    "[forest] Placement was not committed; the plant source or scene transaction was rejected.");
                return 0u;
            }

            state.surfaceSettleFrames =
                (std::max)(state.surfaceSettleFrames, 2);
            state.projectStatus = epochengine::format_text(
                "Placed Forest Factory asset {:02} from Plant Lab "
                "revision {} as {} editable mesh object.",
                assetOrdinal,
                state.plantLabCompiled.sourceRevision,
                placedCount);
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
        [[nodiscard]] bool begin_forest_project_creation(EditorState& state)
        {
            if (!state.projectId.empty() || !state.projectRoot.empty()
                || state.projectLifecycle.evidence().project_selected
                || !state.plantLabDocumentInitialized) return false;
            state.forestProjectCreationIntent = EditorState::ForestProjectCreationIntent{
                state.projectId, state.projectRoot,
                state.projectLifecycle.evidence().generations.selected_project.generation,
                state.plantLabDocument.revision, state.plantLabDocument.contentHash};
            state.projectCreationKind = 0u;
            state.projectCreationOpen = true;
            state.projectStatus = "Confirm Create Project to add this plant to a new Game project. Cancel keeps the plant preview unchanged.";
            return true;
        }

        [[nodiscard]] std::optional<forest::ForestAssetDocument>
        take_forest_project_creation(EditorState& state)
        {
            const auto intent = std::exchange(state.forestProjectCreationIntent, {});
            if (!intent) return {};
            if (state.projectCreationKind != 0u || !state.plantLabDocumentInitialized
                || state.projectId != intent->project_id || state.projectRoot != intent->project_root
                || state.projectLifecycle.evidence().project_selected
                || state.projectLifecycle.evidence().generations.selected_project.generation != intent->project_generation
                || state.plantLabDocument.revision != intent->profile_revision
                || state.plantLabDocument.contentHash != intent->profile_hash)
            {
                state.projectStatus = "The project choice or plant changed. Return to Forest Factory and choose Add to Scene again.";
                return {};
            }
            // Snapshot only at human confirmation. Existing project initialization
            // resets plant state; the authored profile and its history survive here.
            return state.plantLabDocument;
        }

        [[nodiscard]] std::size_t finish_forest_project_creation(EditorState& state,
            forest::ForestAssetDocument document, std::string_view projectId,
            std::string_view projectRoot, std::string_view manifestPath,
            std::uint64_t previousProjectGeneration)
        {
            namespace fs = std::filesystem;
            const auto& evidence = state.projectLifecycle.evidence();
            // Creation reports a native path; admission reports a generic path.
            // Compare their native path identities, then recheck the admitted
            // manifest against the exact newly-created root before placing.
            if (projectId.empty() || projectRoot.empty() || manifestPath.empty()
                || state.projectId != projectId || !evidence.project_selected
                || evidence.generations.selected_project.generation <= previousProjectGeneration
                || evidence.generations.selected_project.project_key != project_assets::stable_project_identity(projectId)
                || fs::path{state.projectRoot}.lexically_normal() != fs::path{projectRoot}.lexically_normal()
                || fs::path{state.projectManifest}.lexically_normal() != fs::path{manifestPath}.lexically_normal()
                || !editor_inspect_project_manifest_identity(manifestPath, projectId, projectRoot).succeeded)
            {
                state.projectStatus = "The new project could not be verified. The plant was not added; return to Forest Factory and retry Add to Scene.";
                return 0u;
            }
            state.plantLabDocument = std::move(document);
            state.plantLabDocumentInitialized = true;
            state.plantLabCompiled = {};
            state.plantLabCompiledRevision = state.plantLabCompiledContentHash = 0u;
            state.forestPreviewEntities.clear();
            state.forestPreviewRevision = state.forestPreviewContentHash = 0u;
            return place_forest_factory_asset(state);
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
            const auto ambient = state.sceneDocument.initialized()
                ? state.sceneDocument.project_settings().ambient : scene::kDefaultAmbientLight;
            state.sceneLighting.set_environment({ { ambient[0], ambient[1], ambient[2] } });
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
            if (!ctx || editor_project_viewport_input_reserved(state)
                || state.previewMode != core::ScenePreviewMode::Editor
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

        [[nodiscard]] bool restore_editor_mesh_base_color(
            const core::Context* ctx, EditorState& state, EditorEntity& entity)
        {
            if (!ctx) return false;
            // Forest vertex colors remain the portable fallback until the
            // other backends implement compiled mesh base-color textures.
            const bool forestMaterial = entity.type == "ForestTrunk"
                && (entity.category == "PlantLabPreview" || entity.category == "ForestFactory");
            if (forestMaterial && ctx->type != core::ContextType::OpenGL)
            {
                entity.meshBaseColorTexture.reset();
                entity.meshMaterialAttempted.reset();
                return true;
            }
            if (forestMaterial && entity.editorOnly && !entity.meshAsset)
            {
                if (!entity.meshBaseColorTexture)
                {
                    const auto& atlas = forest::preview_mesh::material_atlas(state.plantLabDocument.profile.barkTileIndex);
                    auto sealed = texture_artifact::seal_compiled_texture(atlas);
                    if (!sealed.artifact) return false;
                    const auto key = atlas.identity.key.words[0] ? atlas.identity.key.words[0] : 1u;
                    auto binding = std::make_shared<previewgrid::MeshBaseColorBinding>(
                        previewgrid::MeshBaseColorBinding{std::move(sealed.artifact),
                            {key, texture_artifact::compiled_artifact_revision(atlas.identity)},
                            {.min_filter=FilterMode::linear,.mag_filter=FilterMode::linear,
                             .address_v=AddressMode::repeat},true,0.5f,
                            !forest::preview_mesh::reference_material_available()});
                    if (!previewgrid::valid_mesh_base_color_binding(*binding)) return false;
                    entity.meshBaseColorTexture = std::move(binding);
                }
                return true;
            }
            if (!entity.meshAsset || !entity.textureMaterial)
            {
                entity.meshMaterialAttempted.reset();
                entity.meshBaseColorTexture.reset();
                return true;
            }
            if (entity.meshMaterialAttempted != entity.textureMaterial)
            {
                entity.meshMaterialAttempted = entity.textureMaterial;
                entity.meshBaseColorTexture.reset();
                if (entity.textureMaterial->alpha == scene::SceneTextureAlphaMode::opaque
                    || entity.textureMaterial->alpha == scene::SceneTextureAlphaMode::cutout)
                    if (auto* controller = project_texture_controller(state))
                    {
                        auto resolved = controller->resolve_scene_material(*entity.textureMaterial);
                        if (resolved && resolved.artifact)
                        {
                            auto binding = std::make_shared<previewgrid::MeshBaseColorBinding>(
                                previewgrid::MeshBaseColorBinding{std::move(resolved.artifact),resolved.logical,resolved.sampler,
                                    entity.textureMaterial->alpha == scene::SceneTextureAlphaMode::cutout,
                                    entity.textureMaterial->alpha_cutoff,
                                    !entity.textureMaterial->logical_path.starts_with("Textures/Forest/reference_bark_foliage_v1")
                                        && !entity.textureMaterial->logical_path.starts_with("Textures/Forest/project_bark_foliage_")});
                            if (previewgrid::valid_mesh_base_color_binding(*binding))
                                entity.meshBaseColorTexture = std::move(binding);
                        }
                    }
                if (!entity.meshBaseColorTexture)
                    push_editor_log(state,"[asset] Textured mesh not drawn: exact opaque/cutout base-color texture could not be restored: "
                        + entity.textureMaterial->logical_path);
            }
            if (!entity.meshBaseColorTexture || ctx->type != core::ContextType::OpenGL)
            {
                state.projectTextureStatus = ctx->type != core::ContextType::OpenGL
                    ? "Textured mesh drawing currently requires OpenGL. No color substitute was drawn."
                    : "Textured mesh drawing was refused: restore the exact compiled base-color texture.";
                return false;
            }
            return true;
        }

        [[nodiscard]] std::shared_ptr<const previewgrid::MeshPaintView> forest_paint_view(
            const EditorState& state,const EditorEntity& terrain)
        {
            auto view=std::make_shared<previewgrid::MeshPaintView>();
            if(terrain.meshSnapshotFrame) {
                view->origin=terrain.meshSnapshotFrame->source_origin_meters;
                view->meters_per_unit=terrain.meshSnapshotFrame->meters_per_unit; }
            else if(terrain.meshAsset&&terrain.meshAsset->coordinates) {
                view->origin=terrain.meshAsset->coordinates->source_origin_meters;
                view->meters_per_unit=terrain.meshAsset->coordinates->meters_per_unit; }
            std::vector<std::pair<std::uint64_t, std::array<double,3>>> admitted;
            for(const auto& tree:state.entities)
            {
                if(!tree.forestRegion||!tree.forestRegion->valid())continue;
                const auto& region=*tree.forestRegion;
                const auto cap = std::pair{region.region_id, region.center_direction};
                if(region.terrain_object_id!=terrain.sceneObjectId||!terrain.meshAsset
                    ||region.terrain_artifact_key!=terrain.meshAsset->artifact_key
                    ||std::find(admitted.begin(),admitted.end(),cap)!=admitted.end())continue;
                admitted.push_back(cap);
                const bool selected=std::any_of(state.entities.begin(),state.entities.end(),[&](const auto& instance){
                    return instance.forestRegion&&instance.forestRegion->region_id==region.region_id
                        &&editor_entity_selected(state,instance.sceneObjectId);});
                previewgrid::MeshPaintRegion tint{
                    .center=region.admitted_body_center_meters,.direction=region.center_direction,
                    .radius=region.body_radius_meters,
                    .minimum_dot=std::cos(region.radius_meters/region.body_radius_meters),
                    .color=selected?std::array<float,3>{0.95f,0.67f,0.18f}:std::array<float,3>{0.20f,0.66f,0.27f}};
                // Selected regions win where painted caps overlap.
                if(selected)view->regions.insert(view->regions.begin(),tint);
                else view->regions.push_back(tint);
            }
            return view;
        }

        // The existing Scheduler prepares immutable CPU buffers. Context state
        // and visible publication remain exclusively on the Editor owner.
        [[nodiscard]] bool pump_editor_planetary_preview(const void* ctx,EditorState& state,
            previewgrid::SceneFrameRequest request,bool meshWireframe)
        {
            const auto generation=state.projectLifecycle.evidence().generations.selected_project.generation;
            const auto current=[&]{return state.projectRoot==state.planetaryPreviewProjectRoot
                &&generation==state.planetaryPreviewProjectGeneration
                &&state.sceneDocumentRevision==state.planetaryPreviewSceneRevision
                &&state.selectedEntityId==state.planetaryPreviewSelection
                &&state.selectedEntityIds==state.planetaryPreviewSelections
                &&meshWireframe==state.planetaryPreviewWireframe&&state.planetaryPreviewRequest
                &&state.planetaryPreviewRequest->depth_fit_aspect==request.depth_fit_aspect;};
            bool refused{};
            const auto failed=[&](std::string_view message){
                refused=true;
                if(state.projectStatus!=message)push_editor_log(state,"[asset] "+std::string{message});
                state.projectStatus=message;
                if(state.viewportMeshWireframeRequest==meshWireframe)
                    state.viewportMeshWireframeRequest.reset();
            };
            if(state.planetaryPreviewPending&&!current()&&state.taskScheduler
                &&state.planetaryPreviewCancellation.valid())
            {
                (void)state.taskScheduler->cancel(state.planetaryPreviewCancellation);
                state.planetaryPreviewCancellation={};
            }
            bool published{};
            if(state.planetaryPreviewPending
                &&state.planetaryPreviewPending->wait_for(std::chrono::milliseconds{0})==std::future_status::ready)
            {
                try
                {
                    auto prepared=state.planetaryPreviewPending->get();
                    state.planetaryPreviewPending.reset();
                    if(!current())state.planetaryPreviewRequest.reset();
                    else if(prepared)
                    {
                        published=previewgrid::commit_scene_frame(ctx,std::move(*prepared),&request);
                        if(published)
                        {
                            state.viewportMeshWireframe=state.planetaryPreviewWireframe;
                            if(state.viewportMeshWireframeRequest==state.viewportMeshWireframe)
                                state.viewportMeshWireframeRequest.reset();
                        }
                        else state.planetaryPreviewRequest.reset();
                    }
                    else failed("Planetary preview could not prepare an admitted mesh; the previous scene was retained.");
                }
                catch(...)
                {
                    state.planetaryPreviewPending.reset();
                    state.planetaryPreviewRequest.reset();
                    if(current())failed("Planetary preview preparation failed; the previous scene was retained.");
                }
                state.planetaryPreviewCancellation={};
            }
            const auto camera=request.camera;
            const auto origin=request.render_origin;
            const auto clouds=request.clouds;
            if(!refused&&!state.planetaryPreviewPending&&(!state.planetaryPreviewRequest
                ||!previewgrid::same_scene_frame_request(request,*state.planetaryPreviewRequest)))
            {
                if(auto source=previewgrid::capture_scene_frame_source(ctx))
                {
                    try
                    {
                        auto owned=std::make_shared<const previewgrid::SceneFrameRequest>(std::move(request));
                        auto submitted=schedule_editor_cancellable_task(state,"Prepare planetary preview geometry",
                            [source=std::move(*source),owned](std::stop_token cancellation)
                                ->std::optional<previewgrid::PreparedSceneFrame>{
                                if(cancellation.stop_requested())return {};
                                const auto fitted=previewgrid::fit_visible_depth_range(owned->camera,owned->markers,owned->depth_fit_aspect);
                                return previewgrid::prepare_scene_frame(source,owned->markers,fitted,
                                    owned->lighting,owned->limits,owned->render_origin,cancellation,owned->clouds);
                            });
                        state.planetaryPreviewCancellation=submitted.cancellation;
                        state.planetaryPreviewPending.emplace(std::move(submitted.completion));
                        state.planetaryPreviewRequest=std::move(owned);
                        state.planetaryPreviewProjectRoot=state.projectRoot;
                        state.planetaryPreviewProjectGeneration=generation;
                        state.planetaryPreviewSceneRevision=state.sceneDocumentRevision;
                        state.planetaryPreviewSelection=state.selectedEntityId;
                        state.planetaryPreviewSelections=state.selectedEntityIds;
                        state.planetaryPreviewWireframe=meshWireframe;
                    }
                    catch(...)
                    {
                        state.planetaryPreviewRequest.reset();
                        failed("Planetary preview could not queue preparation; the previous scene was retained.");
                    }
                }
            }
            // Navigation cannot wait for CPU flattening. Keep the previous
            // complete packet and convert the current camera to its origin.
            if(const auto previous=previewgrid::scene_frame_for(ctx))
            {
                auto retained=camera;
                retained.nearPlane=previous->camera.nearPlane;
                retained.farPlane=previous->camera.farPlane;
                (void)previewgrid::advance_scene_frame_camera(ctx,retained,origin,std::span<const previewgrid::CloudVolume>{clouds});
            }
            return published;
        }

        struct EditorPreviewMeshCacheUsage
        {
            std::uint64_t bytes{},ordinaryBytes{},ordinaryIndices{};
            std::uint32_t entries{};
        };

        [[nodiscard]] std::optional<EditorPreviewMeshCacheUsage> editor_preview_mesh_cache_usage(
            std::span<const EditorEntity> entities,const project_planetary::SceneMeshBudget& budget,bool forestPainting)
        {
            EditorPreviewMeshCacheUsage usage;
            const bool planetaryPolicy=budget.artifact.maximum_bytes!=0u;
            for(const auto& entity:entities)
                if(entity.meshSnapshot&&(!forestPainting||entity.category=="Planetary Terrain"))
                {
                    if(entity.category=="Planetary Terrain"&&planetaryPolicy
                        &&(entity.meshSnapshot->vertices().size()>budget.artifact.maximum_vertices
                            ||entity.meshSnapshot->indices().size()>budget.artifact.maximum_indices
                            ||48u+std::uint64_t(entity.meshSnapshot->vertices().size())*44u
                                +std::uint64_t(entity.meshSnapshot->indices().size())*4u>budget.artifact.maximum_bytes))return {};
                    const auto bytes=64u+entity.meshSnapshot->vertices().size()*sizeof(asset::mesh::Vertex)
                        +entity.meshSnapshot->indices().size()*sizeof(std::uint32_t);
                    usage.bytes+=bytes;
                    if(entity.category!="Planetary Terrain"||!planetaryPolicy)
                    {
                        usage.ordinaryBytes+=bytes;
                        if(entity.visible)usage.ordinaryIndices+=entity.meshSnapshot->indices().size();
                    }
                    ++usage.entries;
                }
            if(usage.bytes>budget.cache_bytes||usage.ordinaryBytes>16ull*1024u*1024u||usage.ordinaryIndices>196'608u)return {};
            return usage;
        }

        [[nodiscard]] bool restore_editor_preview_mesh(EditorState& state,EditorEntity& entity,
            const project_planetary::SceneMeshBudget& budget,bool awaitingPlanetaryBudget,EditorPreviewMeshCacheUsage& usage)
        {
            if(!entity.meshAsset)return true;
            if(!entity.meshLoadAttempted)
            {
                entity.meshLoadAttempted=true;
                const std::uint64_t expected=64u+std::uint64_t(entity.meshAsset->vertex_count)*sizeof(asset::mesh::Vertex)
                    +std::uint64_t(entity.meshAsset->index_count)*sizeof(std::uint32_t);
                const bool planetary=entity.category=="Planetary Terrain"&&budget.artifact.maximum_bytes!=0u;
                constexpr std::uint64_t ordinaryBudget=16ull*1024u*1024u;
                if(usage.entries>=64u||expected>budget.cache_bytes-(std::min)(budget.cache_bytes,usage.bytes)
                    ||(!planetary&&(expected>ordinaryBudget-(std::min)(ordinaryBudget,usage.ordinaryBytes)
                        ||entity.meshAsset->index_count>196'608u-(std::min)(std::uint64_t{196'608u},usage.ordinaryIndices))))
                {
                    if(awaitingPlanetaryBudget&&usage.entries<64u)
                        push_editor_log(state,"[asset] Mesh loading deferred while saved Planetary data is verified: "+entity.meshAsset->logical_path);
                    else push_editor_log(state,"[asset] Mesh not loaded: "+entity.meshAsset->logical_path
                        +". The admitted scene mesh cache is full. Remove an unused mesh and retry.");
                    return false;
                }
                auto limits=planetary?budget.artifact:asset::mesh::Limits{};
                limits.maximum_bytes=(std::min)(limits.maximum_bytes,budget.cache_bytes-usage.bytes);
                const auto loaded=asset::mesh::read_project_artifact(resolve_editor_path(std::filesystem::path{state.projectRoot}),
                    entity.meshAsset->logical_path,{entity.meshAsset->artifact_key},limits);
                if(loaded&&loaded.artifact->vertices().size()==entity.meshAsset->vertex_count
                    &&loaded.artifact->indices().size()==entity.meshAsset->index_count)
                {
                    entity.meshSnapshot=loaded.artifact;entity.meshSnapshotFrame.reset();
                    if(planetary)refresh_editor_planetary_body_inventory(state,entity);
                    usage.bytes+=expected;
                    if(!planetary){usage.ordinaryBytes+=expected;usage.ordinaryIndices+=entity.meshAsset->index_count;}
                    ++usage.entries;state.sceneInteractionDirty=true;
                }
                else push_editor_log(state,"[asset] Could not load the scene mesh: "+entity.meshAsset->logical_path
                    +". Its file, content identity or geometry is invalid; no substitute object was drawn.");
            }
            return bool(entity.meshSnapshot);
        }

        [[nodiscard]] bool ensure_editor_forest_focus_mesh(EditorState& state,EditorEntity& entity)
        {
            if(entity.category!="ForestFactory"||!entity.meshAsset||entity.meshSnapshot)return true;
            const auto budget=editor_planetary_mesh_budget(state);
            auto usage=editor_preview_mesh_cache_usage(state.entities,budget,false);
            if(!usage)return false;
            // Focus is an explicit retry. Reuse draw admission and exact content
            // validation; unreadable/unadmitted geometry cannot use cube bounds.
            entity.meshLoadAttempted=false;
            return restore_editor_preview_mesh(state,entity,budget,false,*usage);
        }

        [[nodiscard]] const std::shared_ptr<const asset::mesh::Artifact>& editor_forest_silhouette()
        {
            static const auto silhouette=[] {
                std::array<asset::mesh::Vertex,8> vertices{};
                vertices[0].position={0.0f,1.0f,0.0f};
                vertices[7].position={0.0f,0.25f,0.0f};
                std::array<std::uint32_t,36> indices{};
                for(std::uint32_t side=0;side<6u;++side)
                {
                    const float angle=static_cast<float>(side)*1.04719755f;
                    const float x=std::cos(angle),z=std::sin(angle);
                    const auto current=side+1u,next=(side+1u)%6u+1u;
                    vertices[current].position={0.5f*x,0.25f,0.5f*z};
                    vertices[current].normal={x,0.5f,z};
                    const std::array face{0u,next,current,7u,current,next};
                    std::copy(face.begin(),face.end(),indices.begin()+6u*side);
                }
                for(auto& vertex:vertices)vertex.color={0.16f,0.31f,0.13f};
                return asset::mesh::build_artifact(vertices,indices).artifact;
            }();
            return silhouette;
        }

        void append_editor_translation_gizmo(std::vector<previewgrid::ObjectMarker>& markers,
            const EditorState& state, const EditorEntity& entity,
            const previewgrid::ObjectMarker& marker, const std::array<double,3>& renderOrigin)
        {
            auto center = marker.preciseTransform ? marker.preciseTransform->position
                : std::array<double,3>{marker.position.x,marker.position.y,marker.position.z};
            double radius = std::hypot(marker.scale.x, marker.scale.y, marker.scale.z) * 0.5;
            if (const auto body = editor_planetary_body_focus(state, entity))
            {
                center = {body->target.x,body->target.y,body->target.z};
                const auto anchor = entity.positionAnchor.value_or(std::array<double,3>{});
                for (std::size_t axis = 0; axis < center.size(); ++axis)
                    center[axis] += anchor[axis] - renderOrigin[axis];
                radius = body->radius;
            }
            else if (marker.mesh)
            {
                const auto low = marker.mesh->minimum(), high = marker.mesh->maximum();
                const auto transform = marker.preciseTransform ? marker.preciseTransform
                    : previewgrid::make_precise_mesh_transform(center,
                        {marker.scale.x,marker.scale.y,marker.scale.z}, marker.rotationDegrees);
                if (!transform) return;
                std::array<double,3> half{};
                std::array<double,3> midpoint{};
                for (std::size_t axis = 0; axis < half.size(); ++axis)
                {
                    midpoint[axis] = (double(low[axis]) + high[axis]) * 0.5;
                    half[axis] = (double(high[axis]) - low[axis]) * std::abs(transform->scale[axis]) * 0.5;
                }
                center = transform->apply(midpoint);
                radius = std::hypot(half[0],half[1],half[2]);
            }
            const double extent = (std::max)(radius * 1.25, 0.5);
            const double maximum = (std::numeric_limits<float>::max)();
            if (!std::isfinite(extent) || extent > maximum) return;
            for (const double component : center)
                if (!std::isfinite(component) || std::abs(component) > maximum - extent) return;

            const auto mode = state.sceneDragActive ? state.sceneDragMode : state.scenePointerMode;
            constexpr std::array axisModes{ScenePointerMode::move_x,ScenePointerMode::move_y,ScenePointerMode::move_z};
            constexpr std::array rotations{previewgrid::Vec3{},previewgrid::Vec3{0,0,90},previewgrid::Vec3{0,-90,0}};
            constexpr std::array colors{previewgrid::Vec3{1.0f,0.28f,0.24f},
                previewgrid::Vec3{0.26f,1.0f,0.38f},previewgrid::Vec3{0.26f,0.61f,1.0f}};
            const bool locked = std::find(axisModes.begin(),axisModes.end(),mode) != axisModes.end();
            for (std::size_t axis = 0; axis < axisModes.size(); ++axis)
                markers.push_back({.position={float(center[0]),float(center[1]),float(center[2])},
                    .color=previewgrid::scale(colors[axis],locked && mode != axisModes[axis] ? 0.32f : 1.0f),
                    .scale={float(extent),1,1},.rotationDegrees=rotations[axis],
                    .primitive=previewgrid::ObjectPreviewPrimitive::TranslationArrow,.editorOnly=true});
        }

        void publish_editor_preview_markers(const core::Context* ctx, EditorState& state)
        {
            // Build/Play owns a stable scene. Retain the last authoring packet
            // for restoration without rebuilding hidden terrain or lighting.
            if (editor_project_viewport_input_reserved(state)) return;
            // A mode change is committed with the geometry, never with a
            // dropdown click whose wire working set may be refused.
            const bool meshWireframe = state.viewportMeshWireframeRequest.value_or(state.viewportMeshWireframe);
            if (!ctx || state.previewMode != core::ScenePreviewMode::Editor)
            {
                epochengine::previewgrid::clear_object_markers(ctx);
                epochengine::previewgrid::clear_lighting_frame(ctx);
                return;
            }

            rebuild_editor_lighting(state);
            const auto sceneLighting=state.sceneLighting.build_frame();
            epochengine::previewgrid::set_lighting_frame(ctx, sceneLighting);

            std::vector<epochengine::previewgrid::ObjectMarker> markers{};
            const EditorEntity* gizmoEntity{};
            std::size_t gizmoMarker{};
            const auto camera=previewgrid::camera_for(ctx);
            const auto cameraEye=previewgrid::camera_world_eye(camera);
            const bool forestPainting = state.mainSurface == EditorMainSurface::ForestFactory && state.forestTerrainPainting;
            const bool forestPreview = state.mainSurface == EditorMainSurface::ForestFactory && !forestPainting;
            if (forestPreview) (void)ensure_forest_factory_preview(state);
            auto& previewEntities = forestPreview ? state.forestPreviewEntities : state.entities;
            const double sampleTime = state.timelineState.playhead_seconds;
            const bool waterVisible = !forestPreview && !forestPainting && std::any_of(previewEntities.begin(),previewEntities.end(),
                [](const auto& entity){return entity.visible && entity.type == "Water";});
            if (waterVisible && (!state.temporalWaterMesh || state.temporalWaterSampleTime != sampleTime))
            {
                const auto sampled=water::surface::build(water::surface::default_body(),sampleTime);
                if (!sampled)
                {
                    state.projectStatus="Water surface sampling failed; the previous scene was retained.";
                    return;
                }
                state.temporalWaterMesh=sampled.artifact;
                state.temporalWaterSampleTime=sampleTime;
            }
            if (!waterVisible) state.temporalWaterMesh.reset();
            for (auto& entity:previewEntities)
                if (entity.type == "Water") entity.meshSnapshot=state.temporalWaterMesh;
            markers.reserve(previewEntities.size());
            const auto meshBudget=forestPreview?project_planetary::SceneMeshBudget{}:editor_planetary_mesh_budget(state);
            const bool planetaryPolicy=meshBudget.artifact.maximum_bytes!=0u;
            const bool awaitingPlanetaryBudget = !forestPreview && !planetaryPolicy && !state.projectRoot.empty()
                && (state.planetaryCheckedRoot != state.projectRoot || state.planetaryAttachmentPending)
                && std::any_of(previewEntities.begin(),previewEntities.end(),[](const auto& entity)
                    { return entity.meshAsset && entity.category == "Planetary Terrain"; });
            auto meshUsage=editor_preview_mesh_cache_usage(previewEntities,meshBudget,forestPainting);
            if(!meshUsage)return;
            for (std::size_t i = 0; i < previewEntities.size(); ++i)
            {
                auto& entity = previewEntities[i];
                if (!entity.visible || is_screen_space_gui_entity(entity)
                    || (forestPainting && entity.category!="Planetary Terrain"))
                    continue;

                if(entity.meshAsset&&!restore_editor_preview_mesh(state,entity,meshBudget,awaitingPlanetaryBudget,*meshUsage))continue;

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
                        * sampleTime + motion->phase_radians;
                    const auto axis = static_cast<std::size_t>(motion->axis);
                    previewPosition = motion->base_position;
                    previewPosition[axis] += motion->amplitude
                        * static_cast<float>(std::sin(phase));
                }
                if (!forestPreview)
                    if (const auto track=state.scriptTranslationTracks.find(entity.sceneObjectId);
                        track!=state.scriptTranslationTracks.end())
                    {
                        const project_motion_track::Binding binding{
                            .project_generation=state.projectLifecycle.evidence().generations.selected_project.generation,
                            .scene_id=state.sceneDocument.identity().scene.value,
                            .entity_id=entity.sceneObjectId,.project_id=state.projectId};
                        const auto sampled=project_motion_track::sample_translation(track->second,binding,sampleTime);
                        if (sampled)
                            for (std::size_t axis=0;axis<3u;++axis)
                                previewPosition[axis]=entity.position[axis]+static_cast<float>(sampled.offset[axis]);
                    }

                const bool selected = !forestPreview && editor_entity_selected(
                    state, entity.sceneObjectId);
                auto snapshotAsset=entity.meshAsset;
                if(snapshotAsset&&entity.meshSnapshotFrame)
                    snapshotAsset->coordinates=scene::SceneMeshCoordinates{entity.meshSnapshotFrame->meters_per_unit,entity.meshSnapshotFrame->source_origin_meters};
                const auto meshScale = scene::mesh_render_scale(entity.scale, snapshotAsset);
                if (!meshScale)
                {
                    state.projectStatus = "Mesh coordinates exceed the supported preview range; the previous scene is retained.";
                    return;
                }
                const auto worldPivot=scene::world_position(previewPosition,entity.positionAnchor);
                if(!worldPivot)return;
                if(!forestPreview&&entity.forestRegion&&entity.meshSnapshot&&!selected
                    &&camera.projection==render_camera::ProjectionKind::perspective)
                {
                    const auto minimum=entity.meshSnapshot->minimum();
                    const auto maximum=entity.meshSnapshot->maximum();
                    const std::array<double,3> size{
                        (maximum[0]-minimum[0])*std::abs((*meshScale)[0]),
                        (maximum[1]-minimum[1])*std::abs((*meshScale)[1]),
                        (maximum[2]-minimum[2])*std::abs((*meshScale)[2])};
                    const double distance=std::hypot((*worldPivot)[0]-cameraEye[0],
                        (*worldPivot)[1]-cameraEye[1],(*worldPivot)[2]-cameraEye[2]);
                    const double focal=double(state.planetaryViewportHeight)/(2.0*std::tan(camera.fovRadians*0.5));
                    const double pixels=size[1]*focal/(std::max)(1.0,distance);
                    if(pixels<1.5)continue;
                    if(pixels<24.0)
                    {
                        const previewgrid::Vec3 rotation{entity.rotation[0],entity.rotation[1],entity.rotation[2]};
                        const auto transform=previewgrid::make_precise_mesh_transform(*worldPivot,
                            {(*meshScale)[0],(*meshScale)[1],(*meshScale)[2]},rotation);
                        if(!transform)continue;
                        const auto base=transform->apply({(minimum[0]+maximum[0])*0.5,
                            minimum[1],(minimum[2]+maximum[2])*0.5});
                        auto proxy=previewgrid::ObjectMarker{
                            .color={0.16f,0.31f,0.13f},
                            .rotationDegrees=rotation,.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,
                            .mesh=editor_forest_silhouette(),.wireframe=meshWireframe};
                        proxy.preciseTransform=previewgrid::make_precise_mesh_transform(base,size,rotation);
                        if(proxy.mesh&&proxy.preciseTransform)markers.push_back(std::move(proxy));
                        continue;
                    }
                }
                if (!forestPainting && !restore_editor_mesh_base_color(ctx,state,entity)) continue;
                markers.push_back(epochengine::previewgrid::ObjectMarker{
                    .position{
                        previewPosition[0],
                        previewPosition[1],
                        previewPosition[2]
                    },
                    .color = selection_color_for_entity(entity, selected),
                    .scale{
                        (*meshScale)[0],
                        (*meshScale)[1],
                        (*meshScale)[2]
                    },
                    .rotationDegrees{
                        entity.rotation[0],
                        entity.rotation[1],
                        entity.rotation[2]
                    },
                    .radius = marker_radius_for_entity(entity),
                    .primitive = entity.meshAsset || entity.meshSnapshot
                        ? previewgrid::ObjectPreviewPrimitive::IndexedMesh : preview_primitive_for_entity(entity),
                    .selected = !forestPainting && marker_selected_for_entity(entity, selected),
                    .editorOnly = entity.editorOnly || entity.category == "Editor" || is_plant_lab_preview_entity(entity),
                    .sampledRenderSurface = entity.category == "EngineArcade" && entity.name == epochengine::render_arcade::kScreenSceneNode.name,
                    .mesh = entity.meshSnapshot,
                    .wireframe = forestPainting ? false : meshWireframe,
                    .baseColorTexture = forestPainting ? nullptr : entity.meshBaseColorTexture,
                    .forestLighting = entity.type == "ForestTrunk" && bool(entity.meshSnapshot)
                });
                if(forestPainting)markers.back().paintView=forest_paint_view(state,entity);
                if(entity.positionAnchor)
                {
                    markers.back().preciseTransform=previewgrid::make_precise_mesh_transform(*worldPivot,
                        {(*meshScale)[0],(*meshScale)[1],(*meshScale)[2]},markers.back().rotationDegrees);
                    if(!markers.back().preciseTransform)return;
                }
                if(entity.meshSnapshotFrame)
                {
                    auto authored=previewgrid::make_precise_mesh_transform(
                        *worldPivot,
                        {entity.scale[0],entity.scale[1],entity.scale[2]},
                        {entity.rotation[0],entity.rotation[1],entity.rotation[2]});
                    if(!authored)return;
                    const auto translated=authored->apply(entity.meshSnapshotFrame->source_origin_meters);
                    markers.back().preciseTransform=previewgrid::make_precise_mesh_transform(translated,
                        {(*meshScale)[0],(*meshScale)[1],(*meshScale)[2]},markers.back().rotationDegrees);
                    if(!markers.back().preciseTransform)return;
                }
                if (selected && entity.sceneObjectId == state.selectedEntityId && state.helpersVisible
                    && state.mainSurface == EditorMainSurface::Scene)
                {
                    gizmoEntity = &entity;
                    gizmoMarker = markers.size() - 1u;
                }
            }

            std::vector<previewgrid::CloudVolume> cloudVolumes;
            // The admitted package snapshot owns cloud geometry. It is not a
            // selectable scene entity and is omitted from the forest paint view.
            if(!forestPreview&&!forestPainting&&state.planetaryStreamCloud
                &&(state.planetaryStreamCloud->artifact||state.planetaryStreamCloud->density))
            {
                const auto terrain=std::ranges::find_if(previewEntities,[&](const auto& entity){
                    return entity.visible&&entity.sceneObjectId==state.planetaryStreamEntity
                        &&entity.meshSnapshot==state.planetaryStreamSnapshot;});
                if(terrain!=previewEntities.end())
                {
                    const auto& layer=*state.planetaryStreamCloud;
                    const auto pivot=scene::world_position(terrain->position,terrain->positionAnchor);
                    if(!pivot)return;
                    const auto authored=previewgrid::make_precise_mesh_transform(*pivot,
                        {terrain->scale[0],terrain->scale[1],terrain->scale[2]},
                        {terrain->rotation[0],terrain->rotation[1],terrain->rotation[2]});
                    if(!authored)return;
                    const auto center=authored->apply(layer.center_meters);
                    if(layer.density)
                    {
                        const double scale=terrain->scale[0];
                        if(scale<=0.0||std::abs(terrain->scale[1]-scale)>scale*1e-6
                            ||std::abs(terrain->scale[2]-scale)>scale*1e-6)return;
                        if(ctx->type!=core::ContextType::OpenGL)
                        {
                            state.projectTextureStatus="Volumetric clouds currently require OpenGL; this backend has no density pass.";
                        }
                        else cloudVolumes.push_back({.center=center,.radius=layer.radius_meters*scale,
                            .meters_per_world_unit=1.0/scale,
                            .time_seconds=std::isfinite(sampleTime)?(std::max)(0.0,sampleTime):0.0,
                            .rotation=authored->rotation,.recipe=*layer.density});
                    }
                    else
                    {
                    const double ratio=layer.radius_meters/10.0;
                    auto transform=previewgrid::make_precise_mesh_transform(center,
                        {terrain->scale[0]*ratio,terrain->scale[1]*ratio,terrain->scale[2]*ratio},
                        {terrain->rotation[0],terrain->rotation[1],terrain->rotation[2]});
                    const double seconds=std::isfinite(sampleTime)?(std::max)(0.0,sampleTime):0.0;
                    const auto wind=previewgrid::make_precise_mesh_transform({0,0,0},{1,1,1},
                        {0,static_cast<float>(std::fmod(seconds,layer.wind_period_seconds)/layer.wind_period_seconds*360.0),0});
                    if(!transform||!wind)return;
                    const auto rotation=transform->rotation;
                    for(unsigned row=0;row<3u;++row)for(unsigned column=0;column<3u;++column)
                    {
                        double value{};for(unsigned axis=0;axis<3u;++axis)
                            value+=rotation[row*3u+axis]*wind->rotation[axis*3u+column];
                        transform->rotation[row*3u+column]=value;
                    }
                    if(!previewgrid::valid_precise_mesh_transform(*transform))return;
                    markers.push_back({.color={1,1,1},.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,
                        .mesh=layer.artifact,.wireframe=meshWireframe,.preciseTransform=std::move(transform)});
                    }
                }
            }
            const bool planetaryFrame=!forestPreview
                &&(std::any_of(previewEntities.begin(),previewEntities.end(),[](const auto& entity){
                    return entity.visible&&entity.positionAnchor.has_value();})
                ||(planetaryPolicy&&camera.projection==render_camera::ProjectionKind::perspective
                    &&std::any_of(previewEntities.begin(),previewEntities.end(),[](const auto& entity){
                        return entity.visible&&entity.category=="Planetary Terrain"&&bool(entity.meshSnapshot);}))); 
            bool published{};
            if(planetaryFrame)
            {
                auto relativeCamera=camera;
                relativeCamera.eye={};
                relativeCamera.target=previewgrid::camera_forward(camera);
                relativeCamera.navigationValid=false;
                auto relativeLighting=sceneLighting;
                const auto origin=previewgrid::camera_world_eye(camera);
                const auto relativePoint=[&](previewgrid::Vec3 point)->std::optional<previewgrid::Vec3>{
                    std::array<float,3> result{};
                    for(std::size_t axis=0;axis<3;++axis)
                    {
                        const double value=double(axis==0?point.x:axis==1?point.y:point.z)-origin[axis];
                        if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())return {};
                        result[axis]=static_cast<float>(value);
                    }
                    return previewgrid::Vec3{result[0],result[1],result[2]};
                };
                bool valid=true;
                for(auto& marker:markers)
                {
                    const auto position=marker.preciseTransform
                        ?std::optional{previewgrid::Vec3{
                            static_cast<float>(marker.preciseTransform->position[0]-origin[0]),
                            static_cast<float>(marker.preciseTransform->position[1]-origin[1]),
                            static_cast<float>(marker.preciseTransform->position[2]-origin[2])}}
                        :relativePoint(marker.position);
                    if(!position){valid=false;break;}
                    if(marker.mesh)
                    {
                        if(marker.preciseTransform)
                            for(std::size_t axis=0;axis<3u;++axis)marker.preciseTransform->position[axis]-=origin[axis];
                        else marker.preciseTransform=previewgrid::make_precise_mesh_transform(
                            {double(marker.position.x)-origin[0],double(marker.position.y)-origin[1],double(marker.position.z)-origin[2]},
                            {marker.scale.x,marker.scale.y,marker.scale.z},marker.rotationDegrees);
                        if(!marker.preciseTransform){valid=false;break;}
                    }
                    marker.position=*position;
                }
                for(auto& light:relativeLighting.lights)
                {
                    if(light.desc.kind==lighting::LightKind::Directional)continue;
                    const auto point=relativePoint({light.desc.position.x,light.desc.position.y,light.desc.position.z});
                    if(!point){valid=false;break;}
                    light.desc.position={point->x,point->y,point->z};
                }
                if(valid)
                {
                    if (gizmoEntity) append_editor_translation_gizmo(markers,state,*gizmoEntity,markers[gizmoMarker],origin);
                    for(auto& cloud:cloudVolumes)for(unsigned axis=0;axis<3u;++axis)cloud.center[axis]-=origin[axis];
                    previewgrid::SceneFrameRequest request{std::move(markers),relativeCamera,
                        std::move(relativeLighting),{meshBudget.frame_indices,meshBudget.frame_bytes},origin,std::move(cloudVolumes)};
                    const auto viewport=ctx->scene_viewport();
                    if(viewport.width>0&&viewport.height>0)
                        request.depth_fit_aspect=double(viewport.width)/double(viewport.height);
                    published=pump_editor_planetary_preview(ctx,state,std::move(request),meshWireframe);
                    if(!published)
                        return;
                }
            }
            else
            {
                if (gizmoEntity) append_editor_translation_gizmo(markers,state,*gizmoEntity,markers[gizmoMarker],{});
                // Inline previews replace this same context's planetary packet.
                // Retire its cached request so returning to an unchanged planet
                // still queues a fresh publication, and old work cannot commit.
                if(state.planetaryPreviewRequest||state.planetaryPreviewPending)
                    reset_planetary_preview_publication(state);
                published=epochengine::previewgrid::set_object_markers(ctx, markers,
                    previewgrid::MeshFrameLimits{meshBudget.frame_indices,meshBudget.frame_bytes});
            }
            if (!published)
            {
                const std::string failure = "Mesh preview exceeds the frame geometry budget; the previous scene was retained.";
                if (state.projectStatus != failure) push_editor_log(state, "[asset] " + failure);
                state.projectStatus = failure;
                state.viewportMeshWireframeRequest.reset();
                return;
            }
            state.viewportMeshWireframe = meshWireframe;
            state.viewportMeshWireframeRequest.reset();
            if (state.planetaryInitialFramePending && !state.lodInspectionReturnCamera
                && state.mainSurface == EditorMainSurface::Scene)
            {
                const auto viewport=ctx->scene_viewport();
                if(viewport.width<=0||viewport.height<=0)return;
                if(state.planetaryInitialFrameGeneration!=state.projectLifecycle.evidence().generations.selected_project.generation)
                {state.planetaryInitialFramePending=false;return;}
                const EditorEntity* planet{};
                std::size_t planets{};
                std::vector<EditorSceneSeedEntity> cameras;
                for(const auto& entity:state.entities)
                {
                    if(entity.visible&&entity.category=="Planetary Terrain"&&entity.meshAsset)
                    {++planets;planet=&entity;}
                    if(entity.type=="Camera")cameras.push_back({entity.name,entity.type,entity.category,
                        entity.position,entity.rotation,entity.scale,entity.visible,entity.editorOnly});
                }
                if(planets!=1u){state.planetaryInitialFramePending=false;return;}
                if(!planet->meshSnapshot)return; // Wait for the normal validated asset load.
                state.planetaryInitialFramePending=false;
                const auto recipe=project_planetary::load_settings(resolve_editor_path(std::filesystem::path{state.projectRoot}));
                if(!recipe||!project_planetary::supports_exploration(*recipe))return;
                const auto seeds=editor_seed_entities_for_project(state.projectId);
                if(!editor_seed_cameras_unchanged(cameras,seeds))return;
                const auto planetScale = scene::mesh_render_scale(planet->scale, planet->meshAsset);
                if (!planetScale) return;
                if(const auto body=editor_planetary_body_focus(state,*planet))
                {
                    (void)frame_editor_scene_target(ctx,{body->target.x,body->target.y,body->target.z},body->radius);
                    return;
                }
                (void)previewgrid::frame_initial_orbit_mesh(ctx,
                    previewgrid::ObjectMarker{.position={planet->position[0],planet->position[1],planet->position[2]},
                        .scale={(*planetScale)[0],(*planetScale)[1],(*planetScale)[2]},
                        .rotationDegrees={planet->rotation[0],planet->rotation[1],planet->rotation[2]},
                        .primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=planet->meshSnapshot},
                    static_cast<float>(viewport.width)/static_cast<float>(viewport.height));
            }
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
