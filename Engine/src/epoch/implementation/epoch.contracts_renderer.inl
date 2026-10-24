    [[nodiscard]] inline bool engine_arcade_screen_graph_contract_ready(epochengine::IRenderDevice& device)
    {
        epochengine::GraphBuilder builder{};
        const std::string_view screenNameStd = epochengine::package_registry::engine_arcade_render_texture_name();
        const epochengine::string_view screenName{ screenNameStd.data(), screenNameStd.size() };
        const epochengine::render_arcade::ArcadeScreenGraphBuild screen = epochengine::render_arcade::add_screen_graph(builder);

        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 2u
            && graph.materials.size() == 1u
            && graph.meshes.size() == 1u
            && graph.models.size() == 1u
            && graph.passes.size() == 1u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphTexture& compiledTexture = graph.textures.front();
        const epochengine::GraphSampler& compiledSampler = graph.samplers.front();
        const epochengine::GraphRenderTarget& compiledTarget = graph.render_targets.front();
        const epochengine::GraphMaterial& compiledMaterial = graph.materials.front();
        const epochengine::GraphModel& compiledModel = graph.models.front();
        const epochengine::PassDecl& pass = graph.passes.front();
        const auto same_text = [](epochengine::string_view left, epochengine::string_view right) noexcept
        {
            if (left.size != right.size)
                return false;
            for (std::size_t i = 0; i < left.size; ++i)
            {
                if (left.data[i] != right.data[i])
                    return false;
            }
            return true;
        };

        const bool renderTextureReady =
            same_text(compiledScreen.name.view(), screenName)
            && compiledScreen.desc.width == epochengine::package_registry::engine_arcade_render_texture_width()
            && compiledScreen.desc.height == epochengine::package_registry::engine_arcade_render_texture_height()
            && compiledScreen.desc.usage == epochengine::RenderTextureUsage::arcade_cabinet
            && compiledScreen.backend.color_texture
            && compiledScreen.backend.sampler
            && compiledScreen.backend.render_target
            && compiledScreen.color_texture == screen.screen.color_texture
            && compiledScreen.sampler == screen.screen.sampler
            && compiledScreen.render_target == screen.screen.render_target
            && compiledTexture.backend == compiledScreen.backend.color_texture
            && compiledTexture.sampled_sampler == compiledScreen.backend.sampler
            && compiledTexture.owned_by_render_texture_asset
            && compiledSampler.backend == compiledScreen.backend.sampler
            && compiledSampler.owned_by_render_texture_asset
            && compiledTarget.backend == compiledScreen.backend.render_target
            && compiledTarget.owned_by_render_texture_asset;

        const bool passReady =
            pass.render_target == compiledScreen.backend.render_target
            && pass.binding_set
            && pass.bindings.read_materials.size() == 1u
            && pass.bindings.read_materials.front() == compiledMaterial.backend
            && pass.bindings.read_models.size() == 1u
            && pass.bindings.read_models.front() == compiledModel.backend
            && pass.bindings.read_material_textures.empty()
            && pass.bindings.read_textures.empty()
            && pass.bindings.read_samplers.empty()
            && pass.bindings.write_render_targets.size() == 1u
            && pass.bindings.write_render_targets.front() == compiledScreen.backend.render_target
            && pass.draw_models.size() == 1u
            && pass.draw_models.front().model == screen.model
            && pass.draw_models.front().backend == compiledModel.backend;

        graph.execute(device);
        graph.destroy(device);
        return renderTextureReady && passReady;
    }

    [[nodiscard]] inline bool engine_arcade_screen_graph_contract_ready()
    {
        epochengine::NullRenderDevice nullDevice{};
        return engine_arcade_screen_graph_contract_ready(nullDevice);
    }

    [[nodiscard]] inline bool engine_arcade_cabinet_graph_contract_ready(epochengine::IRenderDevice& device)
    {
        const epochengine::RendererCapabilities caps = device.capabilities();
        if (!epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps))
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 6u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphSampler& compiledSampler = graph.samplers.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials.front();
        const epochengine::GraphModel& compiledScreenModel = graph.models.front();
        const epochengine::GraphMaterial& compiledMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& populatePass = graph.passes.front();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool populateReady =
            populatePass.render_target == compiledScreen.backend.render_target
            && populatePass.binding_set
            && populatePass.bindings.read_materials.size() == 1u
            && populatePass.bindings.read_materials.front() == compiledScreenMaterial.backend
            && populatePass.bindings.read_models.size() == 1u
            && populatePass.bindings.read_models.front() == compiledScreenModel.backend
            && populatePass.bindings.read_material_textures.empty()
            && populatePass.bindings.read_textures.empty()
            && populatePass.bindings.read_samplers.empty()
            && populatePass.bindings.write_render_targets.size() == 1u
            && populatePass.bindings.write_render_targets.front() == compiledScreen.backend.render_target
            && populatePass.draw_models.size() == 1u
            && populatePass.draw_models.front().model == cabinet.screen_scene_model
            && populatePass.draw_models.front().backend == compiledScreenModel.backend;

        const bool materialReady =
            compiledMaterial.backend
            && compiledMaterial.texture_slots.size() == 1u
            && compiledMaterial.texture_slots.front().slot == epochengine::MaterialTextureSlot::render_surface
            && compiledMaterial.texture_slots.front().texture == cabinet.screen.color_texture
            && compiledMaterial.texture_slots.front().sampler == cabinet.screen.sampler;

        const bool bodyMaterialReady = compiledBodyMaterial.backend && compiledBodyMaterial.texture_slots.empty();

        const bool modelReady =
            compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material;

        const bool bindingReady =
            cabinetPass.binding_set
            && compiledScreen.sampler == cabinet.screen.sampler
            && compiledSampler.backend == compiledScreen.backend.sampler
            && compiledSampler.owned_by_render_texture_asset
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials.front() == compiledMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.bindings.read_material_textures.size() == 1u
            && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
            && cabinetPass.bindings.read_material_textures.front().texture == compiledScreen.backend.color_texture
            && cabinetPass.bindings.read_material_textures.front().sampler == compiledScreen.backend.sampler
            && cabinetPass.bindings.read_samplers.size() == 1u
            && cabinetPass.bindings.read_samplers.front() == compiledScreen.backend.sampler;

        const bool drawReady =
            cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        graph.execute(device);
        graph.destroy(device);
        return populateReady && materialReady && bodyMaterialReady && modelReady && bindingReady && drawReady;
    }

    [[nodiscard]] inline bool render_surface_requires_render_texture_asset_contract_ready()
    {
        epochengine::NullRenderDevice device{};
        epochengine::GraphBuilder builder{};

        epochengine::TextureDesc plainTextureDesc{};
        plainTextureDesc.width = 64u;
        plainTextureDesc.height = 64u;
        plainTextureDesc.format = epochengine::TextureFormat::rgba8_unorm;
        plainTextureDesc.sampled = true;
        plainTextureDesc.debug_name = "plain.render_surface.reject";
        const epochengine::GraphResource plainTexture =
            builder.create_texture("plain.render_surface.reject", plainTextureDesc);

        epochengine::MaterialDesc materialDesc{};
        materialDesc.name = "plain.render_surface.reject.material";
        materialDesc.unlit = true;
        materialDesc.debug_name = "plain.render_surface.reject.material";
        materialDesc.texture_slots.push_back(epochengine::MaterialTextureSlotDesc{
            .slot = epochengine::MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = epochengine::TextureFormat::rgba8_unorm,
            .required = true
        });

        const epochengine::GraphMaterialTextureSlot materialSlots[] = {
            epochengine::GraphMaterialTextureSlot{
                .slot = epochengine::MaterialTextureSlot::render_surface,
                .texture = plainTexture
            }
        };
        const epochengine::GraphResource material = builder.create_material(
            "plain.render_surface.reject.material",
            materialDesc,
            epochengine::array_view<const epochengine::GraphMaterialTextureSlot>{ materialSlots, 1u });

        const epochengine::GraphResource reads[] = { material };
        [[maybe_unused]] const auto pass = builder.add_pass(
            "plain.render_surface.reject.pass",
            epochengine::array_view<const epochengine::GraphResource>{ reads, 1u },
            {},
            [](epochengine::ICommandContext& ctx)
            {
                ctx.debug_marker("plain.render_surface.reject.pass");
            });

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool ready =
            graph.textures.size() == 1u
            && graph.samplers.empty()
            && graph.materials.size() == 1u
            && graph.passes.size() == 1u
            && graph.textures.front().backend
            && !graph.textures.front().sampled_sampler
            && !graph.textures.front().owned_by_render_texture_asset
            && graph.materials.front().backend
            && graph.passes.front().binding_set
            && graph.passes.front().bindings.read_materials.size() == 1u
            && graph.passes.front().bindings.read_materials.front() == graph.materials.front().backend
            && graph.passes.front().bindings.read_material_textures.empty()
            && graph.passes.front().bindings.read_samplers.empty()
            && graph.passes.front().bindings.read_textures.empty();

        graph.execute(device);
        graph.destroy(device);
        return ready;
    }

    [[nodiscard]] inline bool render_surface_rejects_mismatched_sampler_contract_ready()
    {
        epochengine::NullRenderDevice device{};
        epochengine::GraphBuilder builder{};

        epochengine::RenderTextureAssetDesc screenDesc = epochengine::render_arcade::make_screen_render_texture_desc();
        screenDesc.debug_name = "render_surface.mismatched_sampler.screen";
        const epochengine::GraphRenderTextureAsset screen =
            builder.create_render_texture_asset("render_surface.mismatched_sampler.screen", screenDesc);

        epochengine::SamplerDesc mismatchSamplerDesc{};
        mismatchSamplerDesc.debug_name = "render_surface.mismatched_sampler.extra";
        const epochengine::GraphResource mismatchSampler =
            builder.create_sampler("render_surface.mismatched_sampler.extra", mismatchSamplerDesc);

        epochengine::MaterialDesc materialDesc{};
        materialDesc.name = "render_surface.mismatched_sampler.material";
        materialDesc.unlit = true;
        materialDesc.debug_name = "render_surface.mismatched_sampler.material";
        materialDesc.texture_slots.push_back(epochengine::MaterialTextureSlotDesc{
            .slot = epochengine::MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = epochengine::TextureFormat::rgba8_unorm,
            .required = true
        });

        const epochengine::GraphMaterialTextureSlot materialSlots[] = {
            epochengine::GraphMaterialTextureSlot{
                .slot = epochengine::MaterialTextureSlot::render_surface,
                .texture = screen.color_texture,
                .sampler = mismatchSampler
            }
        };
        const epochengine::GraphResource material = builder.create_material(
            "render_surface.mismatched_sampler.material",
            materialDesc,
            epochengine::array_view<const epochengine::GraphMaterialTextureSlot>{ materialSlots, 1u });

        const epochengine::GraphResource reads[] = { material };
        [[maybe_unused]] const auto pass = builder.add_pass(
            "render_surface.mismatched_sampler.pass",
            epochengine::array_view<const epochengine::GraphResource>{ reads, 1u },
            {},
            [](epochengine::ICommandContext& ctx)
            {
                ctx.debug_marker("render_surface.mismatched_sampler.pass");
            });

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool ready =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 2u
            && graph.materials.size() == 1u
            && graph.passes.size() == 1u
            && graph.render_texture_assets.front().sampler == screen.sampler
            && graph.samplers.front().backend == graph.textures.front().sampled_sampler
            && graph.samplers[1u].backend
            && graph.samplers[1u].backend != graph.textures.front().sampled_sampler
            && graph.materials.front().texture_slots.size() == 1u
            && graph.materials.front().texture_slots.front().texture == screen.color_texture
            && graph.materials.front().texture_slots.front().sampler == mismatchSampler
            && graph.passes.front().binding_set
            && graph.passes.front().bindings.read_materials.size() == 1u
            && graph.passes.front().bindings.read_materials.front() == graph.materials.front().backend
            && graph.passes.front().bindings.read_material_textures.empty()
            && graph.passes.front().bindings.read_samplers.empty()
            && graph.passes.front().bindings.read_textures.empty();

        graph.execute(device);
        graph.destroy(device);
        return ready;
    }

    struct OpenGLFamilyFakeNativeRttState
    {
        int allocate_count = 0;
        int begin_count = 0;
        int end_count = 0;
        int destroy_count = 0;
        epochengine::RendererBackendKind last_backend = epochengine::RendererBackendKind::null;
        epochengine::u32 last_width = 0;
        epochengine::u32 last_height = 0;
        bool saw_depth = false;
        bool saw_sampled = false;
    };

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeRenderTextureAllocation fake_opengl_family_allocate_rtt(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::RenderTextureAssetDesc& desc,
        const epochengine::RenderTextureBackendRequirements& requirements,
        epochengine::u32 slot)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state)
        {
            ++state->allocate_count;
            state->last_backend = backend;
            state->last_width = desc.width;
            state->last_height = desc.height;
            state->saw_depth = requirements.depth_attachment;
            state->saw_sampled = requirements.sampled_color && requirements.sampler;
        }

        return epochengine::OpenGLFamilyNativeRenderTextureAllocation{
            .framebuffer_object = 1000u + slot,
            .color_object = 2000u + slot,
            .depth_object = requirements.depth_attachment ? 3000u + slot : 0u,
            .sampler_object = 4000u + slot,
            .ready = true
        };
    }

    inline void fake_opengl_family_destroy_rtt(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->destroy_count;
    }

    [[nodiscard]] inline bool fake_opengl_family_begin_rtt_pass(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record,
        const epochengine::RenderPassDesc&)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (!state || backend != state->last_backend || !record.native_allocation_ready)
            return false;

        ++state->begin_count;
        return true;
    }

    inline void fake_opengl_family_end_rtt_pass(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->end_count;
    }

    [[nodiscard]] inline bool opengl_family_arcade_screen_graph_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            const epochengine::RendererCapabilities caps = device.capabilities();
            if (!epochengine::renderer_supports_sampled_render_targets(caps)
                || !engine_arcade_screen_graph_contract_ready(device))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_cabinet_graph_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            if (!engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epochengine::OpenGLFamilyCommandContext& context = device.graphics_context();
            const epochengine::CommandResourceBindings& boundResources = context.bound_resources();
            const bool cabinetBindingEvidence =
                context.last_width() == epochengine::package_registry::engine_arcade_render_texture_width()
                && context.last_height() == epochengine::package_registry::engine_arcade_render_texture_height()
                && context.last_render_target()
                && context.bound_binding_set()
                && boundResources.read_materials.size() == 2u
                && boundResources.read_models.size() == 1u
                && boundResources.read_material_textures.size() == 1u
                && boundResources.read_samplers.size() == 1u
                && boundResources.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && boundResources.read_material_textures.front().texture
                && boundResources.read_material_textures.front().sampler
                && boundResources.read_material_textures.front().sampler == boundResources.read_samplers.front()
                && context.last_model()
                && context.last_model() == boundResources.read_models.front()
                && boundResources.read_materials.front()
                && boundResources.read_materials[1u];

            if (!cabinetBindingEvidence)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_fake_native_rtt_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            OpenGLFamilyFakeNativeRttState state{};
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            device.set_native_render_texture_hooks(epochengine::OpenGLFamilyNativeRenderTextureHooks{
                .user = &state,
                .allocate = fake_opengl_family_allocate_rtt,
                .destroy = fake_opengl_family_destroy_rtt,
                .begin_pass = fake_opengl_family_begin_rtt_pass,
                .end_pass = fake_opengl_family_end_rtt_pass
            });

            const epochengine::RendererCapabilities caps = device.capabilities();
            if (!epochengine::renderer_supports_sampled_rtt_hooks(caps)
                || !engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epochengine::OpenGLFamilyCommandContext& context = device.graphics_context();
            const bool ready =
                state.allocate_count == 1
                && state.begin_count == 1
                && state.end_count == 1
                && state.destroy_count == 1
                && state.last_backend == backend
                && state.last_width == epochengine::package_registry::engine_arcade_render_texture_width()
                && state.last_height == epochengine::package_registry::engine_arcade_render_texture_height()
                && state.saw_depth
                && state.saw_sampled
                && context.last_render_target()
                && context.last_model()
                && !context.native_pass_bound();

            if (!ready)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool sampled_render_surface_preview_marker_contract_ready()
    {
        int markerKey{};
        const void* const ctxKey = &markerKey;
        const std::array<epochengine::previewgrid::ObjectMarker, 2> markers{{
            epochengine::previewgrid::ObjectMarker{
                .position = { 0.0f, 0.0f, 0.0f },
                .color = { 0.4f, 0.5f, 0.6f },
                .scale = { 1.0f, 1.0f, 1.0f },
                .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Cube,
                .editorOnly = true,
                .sampledRenderSurface = false
            },
            epochengine::previewgrid::ObjectMarker{
                .position = {
                    epochengine::render_arcade::kScreenSceneNode.position[0],
                    epochengine::render_arcade::kScreenSceneNode.position[1],
                    epochengine::render_arcade::kScreenSceneNode.position[2] },
                .color = { 0.08f, 0.92f, 0.64f },
                .scale = {
                    epochengine::render_arcade::kScreenSceneNode.scale[0],
                    epochengine::render_arcade::kScreenSceneNode.scale[1],
                    epochengine::render_arcade::kScreenSceneNode.scale[2] },
                .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen,
                .editorOnly = false,
                .sampledRenderSurface = true
            }
        }};

        epochengine::previewgrid::set_object_markers(ctxKey, std::span<const epochengine::previewgrid::ObjectMarker>{ markers.data(), markers.size() });
        const std::vector<epochengine::previewgrid::ObjectMarker> sampledMarkers =
            epochengine::previewgrid::sampled_render_surface_markers_for(ctxKey);
        epochengine::previewgrid::clear_object_markers(ctxKey);

        if (sampledMarkers.size() != 1u)
            return false;

        const epochengine::previewgrid::ObjectMarker& screen = sampledMarkers.front();
        return screen.primitive == epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen
            && screen.sampledRenderSurface
            && std::abs(screen.position.x - epochengine::render_arcade::kScreenSceneNode.position[0]) < 0.001f
            && std::abs(screen.position.y - epochengine::render_arcade::kScreenSceneNode.position[1]) < 0.001f
            && std::abs(screen.position.z - epochengine::render_arcade::kScreenSceneNode.position[2]) < 0.001f
            && std::abs(screen.scale.x - epochengine::render_arcade::kScreenSceneNode.scale[0]) < 0.001f
            && std::abs(screen.scale.y - epochengine::render_arcade::kScreenSceneNode.scale[1]) < 0.001f
            && std::abs(screen.scale.z - epochengine::render_arcade::kScreenSceneNode.scale[2]) < 0.001f
            && epochengine::render_arcade::screen_sample_plane_z(screen.position.z, screen.scale.z)
                > screen.position.z
            && epochengine::render_arcade::screen_sample_plane_faces_viewer(
                screen.position.z,
                screen.scale.z,
                8.0f)
            && !epochengine::render_arcade::screen_sample_plane_faces_viewer(
                screen.position.z,
                screen.scale.z,
                -8.0f);
    }

    [[nodiscard]] inline bool renderer_capability_report_contract_ready()
    {
        const auto opengl = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::opengl);
        const auto sdl = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::sdl3);
        const auto sfml = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::sfml3);
        const auto raylib = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::raylib3);
        const auto vulkan = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::vulkan);
        const auto directx = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::directx);
        const auto software = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::software);

        const auto present = epochengine::RendererCapabilityStatus::present;
        const auto partial = epochengine::RendererCapabilityStatus::partial;
        const auto missing = epochengine::RendererCapabilityStatus::missing;
        const auto deferred = epochengine::RendererCapabilityStatus::deferred;

        const auto openglReady =
            opengl.descriptor_contract == present
            && opengl.build_graph_proof == present
            && opengl.hook_readiness == present
            && opengl.live_native_allocation == partial
            && opengl.presentation_proof == partial
            && opengl.sampled_render_targets == partial
            && opengl.scene_sampled_surface == partial;

        const auto sdlLivePathReady = [present, partial](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == present
                && report.build_graph_proof == present
                && report.hook_readiness == present
                && report.live_native_allocation == partial
                && report.presentation_proof == partial
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto runtimeGuardedReady = [present, partial, missing](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == present
                && report.build_graph_proof == present
                && report.hook_readiness == partial
                && report.live_native_allocation == partial
                && report.presentation_proof == missing
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto futureNativeReady = [partial, missing](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == partial
                && report.build_graph_proof == partial
                && report.hook_readiness == missing
                && report.live_native_allocation == missing
                && report.presentation_proof == missing
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto softwareReady =
            software.descriptor_contract == deferred
            && software.build_graph_proof == deferred
            && software.hook_readiness == deferred
            && software.live_native_allocation == deferred
            && software.presentation_proof == deferred
            && software.sampled_render_targets == deferred
            && software.scene_sampled_surface == partial;

        epochengine::OpenGLFamilyRenderDevice openGlNoHooks{ epochengine::RendererBackendKind::opengl };
        const epochengine::RendererCapabilities openGlNoHookCaps = openGlNoHooks.capabilities();

        OpenGLFamilyFakeNativeRttState hookState{};
        epochengine::OpenGLFamilyRenderDevice openGlHooks{ epochengine::RendererBackendKind::opengl };
        openGlHooks.set_native_render_texture_hooks(epochengine::OpenGLFamilyNativeRenderTextureHooks{
            .user = &hookState,
            .allocate = fake_opengl_family_allocate_rtt,
            .destroy = fake_opengl_family_destroy_rtt,
            .begin_pass = fake_opengl_family_begin_rtt_pass,
            .end_pass = fake_opengl_family_end_rtt_pass
        });
        const epochengine::RendererCapabilities openGlHookCaps = openGlHooks.capabilities();

        const bool openGlCapsReady =
            epochengine::renderer_supports_sampled_render_targets(openGlNoHookCaps)
            && !epochengine::renderer_supports_sampled_rtt_hooks(openGlNoHookCaps)
            && !epochengine::renderer_supports_live_sampled_rtt_allocation(openGlNoHookCaps)
            && !epochengine::renderer_supports_native_sampled_render_targets(openGlNoHookCaps)
            && epochengine::renderer_supports_sampled_render_targets(openGlHookCaps)
            && epochengine::renderer_supports_sampled_rtt_hooks(openGlHookCaps)
            && !epochengine::renderer_supports_live_sampled_rtt_allocation(openGlHookCaps)
            && !epochengine::renderer_supports_native_sampled_render_targets(openGlHookCaps);

        bool runtimeGuardCapsReady = true;
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        {
            epochengine::SdlRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            epochengine::RenderTextureAssetHandles noRuntimeHandles{};
            if (!device.runtime_renderer_available())
                noRuntimeHandles = device.create_render_texture_asset(epochengine::render_arcade::make_screen_render_texture_desc());
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available())
                && (device.runtime_renderer_available() || (!noRuntimeHandles.color_texture && !noRuntimeHandles.sampler && !noRuntimeHandles.render_target && device.render_texture_count() == 0u));
        }
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        {
            epochengine::SfmlRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available());
        }
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        {
            epochengine::RaylibRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available());
        }
#endif

        return openglReady
            && sdlLivePathReady(sdl)
            && runtimeGuardedReady(sfml)
            && runtimeGuardedReady(raylib)
            && futureNativeReady(vulkan)
            && futureNativeReady(directx)
            && softwareReady
            && openGlCapsReady
            && runtimeGuardCapsReady;
    }

    [[nodiscard]] inline bool opengl_real_native_texture_hook_contract_ready()
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        epochengine::OpenGLFamilyRenderDevice device{epochengine::RendererBackendKind::opengl};
        device.set_native_texture_hooks(
            epochengine::opengltextures::make_native_texture_hooks());
        if (!device.native_texture_hooks_ready())
            return false;

        epochengine::TextureDesc desc{};
        desc.width = 2;
        desc.height = 2;
        desc.mip_levels = 1;
        desc.format = epochengine::TextureFormat::rgba8_unorm;
        desc.sampled = true;
        desc.debug_name = "OpenGL.NativeTextureHookContract";
        const epochengine::TextureHandle texture = device.create_texture(desc);
        const epochengine::OpenGLFamilyTextureRecord* const record =
            device.resolve_texture(texture);

        std::array<std::uint8_t, 16> pixels{};
        epochengine::TextureUploadDesc upload{};
        upload.width = 2;
        upload.height = 2;
        upload.row_pitch_bytes = 8;
        upload.format = epochengine::TextureFormat::rgba8_unorm;
        upload.data = pixels.data();
        upload.size_bytes = pixels.size();
        const bool ready = texture
            && record != nullptr
            && record->active
            && !record->native_allocation_ready
            && record->texture_object == 0u
            && !device.texture_ready(texture)
            && !device.upload_texture(texture, upload);
        device.destroy(texture);
        return ready && device.resolve_texture(texture) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool opengl_real_native_rtt_hook_contract_ready()
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        epochengine::OpenGLFamilyRenderDevice device{ epochengine::RendererBackendKind::opengl };
        device.set_native_render_texture_hooks(
            epochengine::opengltextures::make_native_render_texture_hooks());

        const epochengine::RendererCapabilities caps = device.capabilities();
        if (!epochengine::renderer_supports_sampled_rtt_hooks(caps))
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::OpenGLFamilyRenderTextureRecord* const record =
            device.resolve_render_texture(handles.render_target);
        const bool ready =
            static_cast<bool>(handles)
            && record != nullptr
            && record->active
            && record->width == desc.width
            && record->height == desc.height
            && record->backend_requirements.color_attachment
            && record->backend_requirements.depth_attachment
            && record->backend_requirements.sampled_color
            && record->backend_requirements.sampler
            && record->backend_requirements.offscreen_target
            && record->backend_requirements.presentable_surface
            && !record->native_allocation_ready
            && record->native_work_order_ready();
        device.destroy(handles);

        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool opengl_family_arcade_native_requirements_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        epochengine::RenderTextureAssetDesc screenDesc{};
        screenDesc.width = epochengine::package_registry::engine_arcade_render_texture_width();
        screenDesc.height = epochengine::package_registry::engine_arcade_render_texture_height();
        screenDesc.color_format = epochengine::TextureFormat::rgba8_unorm;
        screenDesc.depth_format = epochengine::TextureFormat::depth24_stencil8;
        screenDesc.has_depth = true;
        screenDesc.sampled_after_render = true;
        screenDesc.usage = epochengine::RenderTextureUsage::arcade_cabinet;
        screenDesc.debug_name = "engine_arcade.screen";

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(screenDesc);
            const epochengine::OpenGLFamilyRenderTextureRecord* const record =
                device.resolve_render_texture(handles.render_target);
            const bool ready =
                static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->backend_requirements.color_attachment
                && record->backend_requirements.depth_attachment
                && record->backend_requirements.sampled_color
                && record->backend_requirements.sampler
                && record->backend_requirements.offscreen_target
                && record->backend_requirements.presentable_surface
                && record->native_work_order_ready()
                && !record->native_allocation_ready
                && record->color_object != 0u
                && record->depth_object != 0u
                && record->framebuffer_object != 0u
                && record->sampler_object != 0u;
            device.destroy(handles);

            if (!ready || device.resolve_render_texture(handles.render_target) != nullptr)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool sdl_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        epochengine::SdlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sdl3"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::SdlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->texture != nullptr
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sfml_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        epochengine::SfmlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sfml3"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::SfmlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->target != nullptr
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        epochengine::RaylibRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "raylib"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::RaylibRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sdl_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        epochengine::SdlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sdl3"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 6u
            && graph.samplers.size() == 1u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::SdlCommandContext& context = static_cast<epochengine::SdlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sfml_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        epochengine::SfmlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sfml3"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 6u
            && graph.samplers.size() == 1u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::SfmlCommandContext& context = static_cast<epochengine::SfmlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        epochengine::RaylibRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "raylib"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 6u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::RaylibCommandContext& context = static_cast<epochengine::RaylibCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_texture_storage_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        auto previous = epochengine::core::get_current_render_context();
        auto ctx = std::make_shared<epochengine::core::Context>();
        ctx->type = epochengine::core::ContextType::RayLib;
        ctx->native_drawable = nullptr;

        epochengine::core::set_current_render_context(ctx);
        const bool ready =
            epochengine::raylibtextures::backend_storage_is_separate_from_context_native_drawable()
            && ctx->native_drawable == nullptr;
        epochengine::core::set_current_render_context(std::move(previous));

        return ready;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool atlas_snapshot_upload_contract_ready()
    {
        epochengine::TextureAtlas atlas{};
        if (!atlas.init(epochengine::AtlasConfig{
                .name = "contract.atlas_snapshot_upload",
                .width = 16u,
                .height = 16u,
                .generate_mipmaps = false }))
        {
            return false;
        }

        auto makeTexture = [](std::string name, std::uint8_t red, std::uint8_t green, std::uint8_t blue)
        {
            epochengine::Texture texture{};
            texture.name = std::move(name);
            texture.width = 4u;
            texture.height = 4u;
            texture.channels = 4u;
            texture.pixels.resize(static_cast<std::size_t>(texture.width) * texture.height * texture.channels);
            for (std::size_t i = 0; i + 3u < texture.pixels.size(); i += 4u)
            {
                texture.pixels[i + 0u] = red;
                texture.pixels[i + 1u] = green;
                texture.pixels[i + 2u] = blue;
                texture.pixels[i + 3u] = 255u;
            }
            return texture;
        };

        const auto red = makeTexture("snapshot.red", 255u, 0u, 0u);
        if (!atlas.add_entry("snapshot.red", red))
            return false;

        const auto first = atlas.snapshot_pixels();
        if (first.width != 16u
            || first.height != 16u
            || first.version == 0u
            || first.pixels.size() != 16u * 16u * 4u)
        {
            return false;
        }

        const auto green = makeTexture("snapshot.green", 0u, 255u, 0u);
        if (!atlas.add_entry("snapshot.green", green))
            return false;

        const auto second = atlas.snapshot_pixels();
        if (second.width != first.width
            || second.height != first.height
            || second.pixels.size() != first.pixels.size()
            || second.version <= first.version
            || second.pixels == first.pixels
            || atlas.entry_count() != 2u)
        {
            return false;
        }

        for (std::uint32_t iteration = 0u; iteration < 1'024u; ++iteration)
        {
            const auto replacement = makeTexture(
                "snapshot.red",
                iteration % 2u == 0u ? 255u : 0u,
                iteration % 2u == 0u ? 255u : 0u,
                iteration % 2u == 0u ? 0u : 255u);
            if (!atlas.replace_entry_pixels("snapshot.red", replacement)
                || atlas.entry_count() != 2u)
            {
                return false;
            }
        }

        const auto replaced = atlas.snapshot_pixels();
        return replaced.width == second.width
            && replaced.height == second.height
            && replaced.pixels.size() == second.pixels.size()
            && replaced.version >= second.version + 1'024u
            && replaced.pixels != second.pixels
            && atlas.entry_count() == 2u;
    }

    [[nodiscard]] constexpr bool editor_session_restore_allowed(
        epochengine::core::BackendLifecycleState lifecycle) noexcept
    {
        return lifecycle == epochengine::core::BackendLifecycleState::ready;
    }

    [[nodiscard]] constexpr bool editor_session_deferred_for_active_replacement(
        const void* candidate,
        const void* activeReplacement,
        bool restorationAllowed) noexcept
    {
        return !restorationAllowed
            && candidate != nullptr
            && activeReplacement != nullptr
            && candidate == activeReplacement;
    }

    [[nodiscard]] constexpr bool editor_restored_frame_acknowledged(
        bool acknowledgementQueued,
        std::uint64_t acknowledgementGeneration,
        std::uint64_t completedFrameGeneration) noexcept
    {
        return acknowledgementQueued
            && acknowledgementGeneration != 0
            && completedFrameGeneration >= acknowledgementGeneration;
    }

