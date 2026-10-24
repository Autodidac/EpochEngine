// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        void draw_texture_source_controls(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            float width)
        {
            gui::label("Texture Sources");
            (void)gui::edit_box(
                editor.newTextureName,
                {width, 28.0f},
                64u,
                false);
            const std::array textureExtentTabs{
                gui::SegmentedButtonSpec{
                    "64", 48.0f,
                    editor.newTextureExtentIndex == 0u},
                gui::SegmentedButtonSpec{
                    "128", 52.0f,
                    editor.newTextureExtentIndex == 1u},
                gui::SegmentedButtonSpec{
                    "256", 52.0f,
                    editor.newTextureExtentIndex == 2u},
                gui::SegmentedButtonSpec{
                    "512", 52.0f,
                    editor.newTextureExtentIndex == 3u}
            };
            if (const auto extent = gui::segmented_button_row(
                    textureExtentTabs, 26.0f, 3.0f))
            {
                editor.newTextureExtentIndex = *extent;
            }

            const std::array texturePatternTabs{
                gui::SegmentedButtonSpec{
                    "Checker", 74.0f,
                    editor.newTexturePatternIndex == 0u},
                gui::SegmentedButtonSpec{
                    "Gradient", 78.0f,
                    editor.newTexturePatternIndex == 1u},
                gui::SegmentedButtonSpec{
                    "Solid", 62.0f,
                    editor.newTexturePatternIndex == 2u}
            };
            if (const auto pattern = gui::segmented_button_row(
                    texturePatternTabs, 26.0f, 3.0f))
            {
                editor.newTexturePatternIndex = *pattern;
            }

            const bool importableSelection =
                project_texture_source_path(editor.selectedProjectFile);
            const std::array sourceActions{
                gui::InlineButtonSpec{
                    .label = "Create Editable",
                    .width = 126.0f},
                gui::InlineButtonSpec{
                    .label = "Open / Import",
                    .width = 112.0f,
                    .enabled = importableSelection},
                gui::InlineButtonSpec{
                    .label = "Refresh",
                    .width = 72.0f}
            };
            if (const auto action = gui::inline_button_row(
                    sourceActions, 27.0f, 4.0f))
            {
                if (*action == 0u)
                {
                    create_starter_project_texture(editor, ctx);
                }
                else if (*action == 1u)
                {
                    import_selected_project_texture(editor, ctx);
                }
                else if (auto* controller =
                        project_texture_controller(editor))
                {
                    const auto refreshed = controller->refresh_catalog();
                    editor.projectTextureThumbnails.clear();
                    editor.projectSourceTextureThumbnails.clear();
                    editor.projectTextureThumbnailAtlasSignature = 0u;
                    editor.projectTextureStatus = refreshed
                        ? epochengine::format_text(
                            "Texture Library refreshed: {} sources, {} restored, {} regenerated, {} skipped.",
                            refreshed.discovered_sources,
                            refreshed.restored_artifacts,
                            refreshed.regenerated_artifacts,
                            refreshed.skipped_sources)
                        : epochengine::format_text(
                            "Texture Library refresh refused: {}.",
                            editor_project_textures::controller_code_name(
                                refreshed.code));
                }
                editor.projectTexturePreviewSprite = {};
            }
        }

        void draw_active_texture_authoring(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            float width,
            std::string_view identity,
            float previewHeight)
        {
            auto* controller = project_texture_controller(editor);
            if (!controller)
                return;
            auto state = controller->editable_state();
            if (!state)
                return;

            gui::label("Texture Document");
            gui::property_row(
                "Source",
                state->logical_path,
                72.0f);
            gui::property_row(
                "Revision",
                epochengine::format_text(
                    "{}{}",
                    state->revision.sequence,
                    state->dirty ? " *" : ""),
                72.0f);
            gui::property_row(
                "Canvas",
                epochengine::format_text(
                    "{}x{} | {} layers | {} tiles",
                    state->canvas.width,
                    state->canvas.height,
                    state->metrics.active_layers,
                    state->metrics.sparse_tile_count),
                72.0f);
            gui::property_row(
                "History",
                epochengine::format_text(
                    "{} / {} operations | {} KiB",
                    state->metrics.applied_operation_count,
                    state->metrics.operation_count,
                    state->metrics.retained_operation_bytes / 1024u),
                72.0f);

            const std::array historyActions{
                gui::InlineButtonSpec{
                    .label = "Undo",
                    .width = 66.0f,
                    .enabled = state->can_undo},
                gui::InlineButtonSpec{
                    .label = "Redo",
                    .width = 66.0f,
                    .enabled = state->can_redo},
                gui::InlineButtonSpec{
                    .label = "Save + Publish",
                    .width = 126.0f,
                    .enabled = state->dirty}
            };
            if (const auto action = gui::inline_button_row(
                    historyActions,
                    27.0f,
                    4.0f))
            {
                if (*action == 0u)
                {
                    const auto result = controller->undo_editable();
                    editor.projectTextureStatus = result
                        ? "Texture edit undone."
                        : epochengine::format_text(
                            "Undo refused: {}.",
                            authoring::texture::result_code_name(
                                result.authoring_code));
                }
                else if (*action == 1u)
                {
                    const auto result = controller->redo_editable();
                    editor.projectTextureStatus = result
                        ? "Texture edit redone."
                        : epochengine::format_text(
                            "Redo refused: {}.",
                            authoring::texture::result_code_name(
                                result.authoring_code));
                }
                else
                {
                    const RendererBackendKind backend =
                        renderer_backend_kind(ctx);
                    const capability::SubsystemProfile renderer =
                        capability::renderer_subsystem_profile_for(
                            backend);
                    const auto saved = controller->save_editable(
                        renderer,
                        renderer.capability.recommended_budgets);
                    editor.projectTextureStatus = saved
                        ? epochengine::format_text(
                            "Saved and published revision {}.",
                            saved.entry->source_sequence)
                        : epochengine::format_text(
                            "Texture save refused: {} | source {} | compile {}.",
                            editor_project_textures::
                                controller_code_name(saved.code),
                            project_texture_sources::
                                source_code_name(saved.source_code),
                            asset::texture::
                                artifact_compilation_status_name(
                                    saved.compilation_status));
                }
                editor.projectTexturePreviewSprite = {};
                state = controller->editable_state();
                if (!state)
                    return;
            }

            gui::label("Layers");
            for (const auto& layer : state->layers)
            {
                std::string label = layer.descriptor.name;
                if (layer.descriptor.role
                    == authoring::texture::LayerRole::mask)
                {
                    label += "  [mask]";
                }
                if (!layer.descriptor.visible)
                    label += "  [hidden]";
                if (layer.descriptor.locked)
                    label += "  [locked]";
                label += epochengine::format_text(
                    "  {} KiB",
                    layer.stored_bytes / 1024u);
                if (gui::button_selected(
                        label,
                        {width, 27.0f},
                        layer.handle == state->selected_layer))
                {
                    (void)controller->select_editable_layer(
                        layer.handle);
                    state = controller->editable_state();
                    if (!state)
                        return;
                }
            }

            (void)gui::edit_box(
                editor.newTextureLayerName,
                {width, 27.0f},
                64u,
                false);
            const auto selectedBeforeCreate = std::find_if(
                state->layers.begin(),
                state->layers.end(),
                [&](const authoring::texture::LayerInfo& layer)
                {
                    return layer.handle == state->selected_layer;
                });
            const bool canCreateMask =
                selectedBeforeCreate != state->layers.end()
                && selectedBeforeCreate->descriptor.role
                    == authoring::texture::LayerRole::content
                && !selectedBeforeCreate->descriptor.mask.source;
            const std::array createLayerActions{
                gui::InlineButtonSpec{
                    .label = "Add Layer",
                    .width = 104.0f},
                gui::InlineButtonSpec{
                    .label = "Add Mask",
                    .width = 96.0f,
                    .enabled = canCreateMask}
            };
            if (const auto createAction = gui::inline_button_row(
                    createLayerActions,
                    27.0f,
                    4.0f))
            {
                std::string name = editor.newTextureLayerName;
                if (name.empty())
                {
                    name = epochengine::format_text(
                        *createAction == 0u ? "Layer {}" : "Mask {}",
                        state->layers.size() + 1u);
                }
                const auto result = *createAction == 0u
                    ? controller->create_editable_layer({
                        .name = std::move(name)
                    })
                    : controller->create_editable_layer_mask(
                        selectedBeforeCreate->handle,
                        std::move(name));
                editor.projectTextureStatus = result
                    ? (*createAction == 0u
                        ? "Texture layer added."
                        : "Sparse layer mask created and attached.")
                    : epochengine::format_text(
                        "Layer creation refused: {}.",
                        authoring::texture::result_code_name(
                            result.authoring_code));
                state = controller->editable_state();
                if (!state)
                    return;
            }

            const auto selectedLayer = std::find_if(
                state->layers.begin(),
                state->layers.end(),
                [&](const authoring::texture::LayerInfo& layer)
                {
                    return layer.handle == state->selected_layer;
                });
            if (selectedLayer != state->layers.end())
            {
                auto layerDescriptor = selectedLayer->descriptor;
                bool layerDescriptorChanged = false;
                if (layerDescriptor.role
                    == authoring::texture::LayerRole::content)
                {
                const std::array blendTabs{
                    gui::SegmentedButtonSpec{
                        "Normal", 64.0f,
                        layerDescriptor.blend
                            == authoring::texture::BlendMode::normal},
                    gui::SegmentedButtonSpec{
                        "Multiply", 72.0f,
                        layerDescriptor.blend
                            == authoring::texture::BlendMode::multiply},
                    gui::SegmentedButtonSpec{
                        "Screen", 64.0f,
                        layerDescriptor.blend
                            == authoring::texture::BlendMode::screen},
                    gui::SegmentedButtonSpec{
                        "Add", 48.0f,
                        layerDescriptor.blend
                            == authoring::texture::BlendMode::add},
                    gui::SegmentedButtonSpec{
                        "Subtract", 76.0f,
                        layerDescriptor.blend
                            == authoring::texture::BlendMode::subtract}
                };
                if (const auto blend = gui::segmented_button_row(
                        blendTabs, 25.0f, 2.0f))
                {
                    constexpr std::array<
                        authoring::texture::BlendMode,
                        5> values{
                            authoring::texture::BlendMode::normal,
                            authoring::texture::BlendMode::multiply,
                            authoring::texture::BlendMode::screen,
                            authoring::texture::BlendMode::add,
                            authoring::texture::BlendMode::subtract
                        };
                    layerDescriptor.blend = values[*blend];
                    layerDescriptorChanged = true;
                }
                const std::array opacityTabs{
                    gui::SegmentedButtonSpec{
                        "25%", 52.0f,
                        layerDescriptor.opacity == 16'384u},
                    gui::SegmentedButtonSpec{
                        "50%", 52.0f,
                        layerDescriptor.opacity == 32'768u},
                    gui::SegmentedButtonSpec{
                        "75%", 52.0f,
                        layerDescriptor.opacity == 49'151u},
                    gui::SegmentedButtonSpec{
                        "100%", 60.0f,
                        layerDescriptor.opacity == 65'535u}
                };
                if (const auto opacity = gui::segmented_button_row(
                        opacityTabs, 25.0f, 3.0f))
                {
                    constexpr std::array<std::uint16_t, 4> values{
                        16'384u, 32'768u, 49'151u, 65'535u};
                    layerDescriptor.opacity = values[*opacity];
                    layerDescriptorChanged = true;
                }
                gui::label("Layer Transform");
                const auto offsetX = gui::slider({
                    .id = "texture.layer.offset_x",
                    .label = "Offset X",
                    .minimum = -static_cast<float>(state->canvas.width),
                    .maximum = static_cast<float>(state->canvas.width),
                    .value = static_cast<float>(
                        layerDescriptor.transform.offset_x_pixels),
                    .step = 1.0f,
                    .size = {width, 28.0f}
                });
                if (offsetX.changed)
                {
                    layerDescriptor.transform.offset_x_pixels =
                        static_cast<std::int32_t>(
                            std::lround(offsetX.value));
                    layerDescriptorChanged = true;
                }
                const auto offsetY = gui::slider({
                    .id = "texture.layer.offset_y",
                    .label = "Offset Y",
                    .minimum = -static_cast<float>(state->canvas.height),
                    .maximum = static_cast<float>(state->canvas.height),
                    .value = static_cast<float>(
                        layerDescriptor.transform.offset_y_pixels),
                    .step = 1.0f,
                    .size = {width, 28.0f}
                });
                if (offsetY.changed)
                {
                    layerDescriptor.transform.offset_y_pixels =
                        static_cast<std::int32_t>(
                            std::lround(offsetY.value));
                    layerDescriptorChanged = true;
                }
                layerDescriptorChanged |= gui::toggle_switch(
                    "Mirror horizontally",
                    layerDescriptor.transform.mirror_x,
                    {width, 27.0f});
                layerDescriptorChanged |= gui::toggle_switch(
                    "Mirror vertically",
                    layerDescriptor.transform.mirror_y,
                    {width, 27.0f});

                gui::label("Layer Filter");
                const auto brightness = gui::slider({
                    .id = "texture.layer.brightness",
                    .label = "Brightness",
                    .minimum = -255.0f,
                    .maximum = 255.0f,
                    .value = static_cast<float>(
                        layerDescriptor.filter.brightness),
                    .step = 1.0f,
                    .size = {width, 28.0f}
                });
                if (brightness.changed)
                {
                    layerDescriptor.filter.brightness =
                        static_cast<std::int16_t>(
                            std::lround(brightness.value));
                    layerDescriptorChanged = true;
                }
                layerDescriptorChanged |= gui::toggle_switch(
                    "Grayscale",
                    layerDescriptor.filter.grayscale,
                    {width, 27.0f});
                layerDescriptorChanged |= gui::toggle_switch(
                    "Invert RGB",
                    layerDescriptor.filter.invert,
                    {width, 27.0f});

                gui::label("Layer Mask");
                if (gui::button_selected(
                        "None",
                        {width, 27.0f},
                        !layerDescriptor.mask.source))
                {
                    layerDescriptor.mask.source = {};
                    layerDescriptorChanged = true;
                }
                for (const auto& maskLayer : state->layers)
                {
                    if (maskLayer.descriptor.role
                            != authoring::texture::LayerRole::mask
                        || maskLayer.handle == selectedLayer->handle)
                    {
                        continue;
                    }
                    if (gui::button_selected(
                            maskLayer.descriptor.name,
                            {width, 27.0f},
                            layerDescriptor.mask.source
                                == maskLayer.handle))
                    {
                        layerDescriptor.mask.source = maskLayer.handle;
                        layerDescriptorChanged = true;
                    }
                }
                if (layerDescriptor.mask.source)
                {
                    const auto maskStrength = gui::slider({
                        .id = "texture.layer.mask_strength",
                        .label = "Mask Strength",
                        .minimum = 0.0f,
                        .maximum = 1.0f,
                        .value = static_cast<float>(
                            layerDescriptor.mask.strength)
                            / 65'535.0f,
                        .step = 0.01f,
                        .size = {width, 28.0f}
                    });
                    if (maskStrength.changed)
                    {
                        layerDescriptor.mask.strength =
                            static_cast<std::uint16_t>(std::lround(
                                std::clamp(
                                    maskStrength.value,
                                    0.0f,
                                    1.0f)
                                * 65'535.0f));
                        layerDescriptorChanged = true;
                    }
                    layerDescriptorChanged |= gui::toggle_switch(
                        "Invert Mask",
                        layerDescriptor.mask.invert,
                        {width, 27.0f});
                }
                if (layerDescriptorChanged)
                {
                    const auto changed =
                        controller->set_editable_layer_properties(
                            selectedLayer->handle,
                            std::move(layerDescriptor));
                    editor.projectTextureStatus = changed
                        ? "Texture layer properties updated."
                        : epochengine::format_text(
                            "Layer update refused: {}.",
                            authoring::texture::result_code_name(
                                changed.authoring_code));
                    editor.projectTexturePreviewSprite = {};
                }
                }
                else
                {
                    gui::label("Sparse R8 Mask");
                    gui::property_row(
                        "Coverage",
                        epochengine::format_text(
                            "{} tile{} | {} bytes",
                            selectedLayer->stored_tile_count,
                            selectedLayer->stored_tile_count == 1u ? "" : "s",
                            selectedLayer->stored_bytes),
                        78.0f);
                }

                const bool layerIsReferencedMask =
                    selectedLayer->descriptor.role
                        == authoring::texture::LayerRole::mask
                    && std::ranges::any_of(
                        state->layers,
                        [&](const authoring::texture::LayerInfo& layer)
                        {
                            return layer.descriptor.mask.source
                                == selectedLayer->handle;
                        });
                const std::array layerActions{
                    gui::InlineButtonSpec{
                        .label = selectedLayer->descriptor.visible
                            ? "Hide"
                            : "Show",
                        .width = 62.0f},
                    gui::InlineButtonSpec{
                        .label = selectedLayer->descriptor.locked
                            ? "Unlock"
                            : "Lock",
                        .width = 68.0f},
                    gui::InlineButtonSpec{
                        .label = "Up",
                        .width = 48.0f,
                        .enabled = selectedLayer->order > 0u},
                    gui::InlineButtonSpec{
                        .label = "Down",
                        .width = 58.0f,
                        .enabled = selectedLayer->order + 1u
                            < state->layers.size()},
                    gui::InlineButtonSpec{
                        .label = layerIsReferencedMask ? "In Use" : "Remove",
                        .width = 74.0f,
                        .enabled = state->layers.size() > 1u
                            && !layerIsReferencedMask}
                };
                if (const auto action = gui::inline_button_row(
                        layerActions,
                        27.0f,
                        3.0f))
                {
                    if (*action <= 1u)
                    {
                        auto descriptor =
                            selectedLayer->descriptor;
                        if (*action == 0u)
                            descriptor.visible = !descriptor.visible;
                        else
                            descriptor.locked = !descriptor.locked;
                        (void)controller->
                            set_editable_layer_properties(
                                selectedLayer->handle,
                                std::move(descriptor));
                    }
                    else if (*action == 2u)
                    {
                        (void)controller->move_editable_layer(
                            selectedLayer->handle,
                            selectedLayer->order - 1u);
                    }
                    else if (*action == 3u)
                    {
                        (void)controller->move_editable_layer(
                            selectedLayer->handle,
                            selectedLayer->order + 1u);
                    }
                    else
                    {
                        (void)controller->remove_editable_layer(
                            selectedLayer->handle);
                    }
                    editor.projectTexturePreviewSprite = {};
                    state = controller->editable_state();
                    if (!state)
                        return;
                }
            }

            const auto brushLayer = std::find_if(
                state->layers.begin(),
                state->layers.end(),
                [&](const authoring::texture::LayerInfo& layer)
                {
                    return layer.handle == state->selected_layer;
                });
            const bool paintingMask = brushLayer != state->layers.end()
                && brushLayer->descriptor.role
                    == authoring::texture::LayerRole::mask;

            gui::label(paintingMask ? "Mask Brush" : "Brush");
            constexpr std::array<
                authoring::texture::PixelRgba8,
                6> brushColors{
                    authoring::texture::PixelRgba8{
                        240u, 244u, 250u, 255u},
                    authoring::texture::PixelRgba8{
                        28u, 32u, 40u, 255u},
                    authoring::texture::PixelRgba8{
                        46u, 132u, 224u, 255u},
                    authoring::texture::PixelRgba8{
                        232u, 72u, 72u, 255u},
                    authoring::texture::PixelRgba8{
                        72u, 196u, 116u, 255u},
                    authoring::texture::PixelRgba8{
                        244u, 188u, 64u, 255u}
                };
            if (!paintingMask)
            {
                const std::array colorTabs{
                    gui::SegmentedButtonSpec{
                        "White", 58.0f,
                        editor.textureBrushColorIndex == 0u},
                    gui::SegmentedButtonSpec{
                        "Ink", 48.0f,
                        editor.textureBrushColorIndex == 1u},
                    gui::SegmentedButtonSpec{
                        "Blue", 54.0f,
                        editor.textureBrushColorIndex == 2u},
                    gui::SegmentedButtonSpec{
                        "Red", 48.0f,
                        editor.textureBrushColorIndex == 3u},
                    gui::SegmentedButtonSpec{
                        "Green", 62.0f,
                        editor.textureBrushColorIndex == 4u},
                    gui::SegmentedButtonSpec{
                        "Gold", 52.0f,
                        editor.textureBrushColorIndex == 5u}
                };
                if (const auto color = gui::segmented_button_row(
                        colorTabs,
                        25.0f,
                        2.0f))
                {
                    editor.textureBrushColorIndex = *color;
                }
            }
            else
            {
                const std::array maskModes{
                    gui::SegmentedButtonSpec{
                        "Conceal", 76.0f,
                        !editor.textureMaskPaintReveal},
                    gui::SegmentedButtonSpec{
                        "Reveal", 68.0f,
                        editor.textureMaskPaintReveal}
                };
                if (const auto mode = gui::segmented_button_row(
                        maskModes,
                        25.0f,
                        3.0f))
                {
                    editor.textureMaskPaintReveal = *mode == 1u;
                }
            }
            constexpr std::array<std::uint32_t, 4> brushRadii{
                1u, 4u, 12u, 32u};
            const std::array radiusTabs{
                gui::SegmentedButtonSpec{
                    "1 px", 52.0f,
                    editor.textureBrushRadiusIndex == 0u},
                gui::SegmentedButtonSpec{
                    "4 px", 52.0f,
                    editor.textureBrushRadiusIndex == 1u},
                gui::SegmentedButtonSpec{
                    "12 px", 60.0f,
                    editor.textureBrushRadiusIndex == 2u},
                gui::SegmentedButtonSpec{
                    "32 px", 60.0f,
                    editor.textureBrushRadiusIndex == 3u}
            };
            if (const auto radius = gui::segmented_button_row(
                    radiusTabs,
                    25.0f,
                    3.0f))
            {
                editor.textureBrushRadiusIndex = *radius;
            }
            editor.textureBrushColorIndex = (std::min)(
                editor.textureBrushColorIndex,
                brushColors.size() - 1u);
            editor.textureBrushRadiusIndex = (std::min)(
                editor.textureBrushRadiusIndex,
                brushRadii.size() - 1u);

            const auto normalizedBrushValue = [](std::uint16_t value) noexcept
            {
                return static_cast<float>(value) / 65'535.0f;
            };
            const auto authoringBrushValue = [](float value) noexcept
            {
                return static_cast<std::uint16_t>(std::lround(
                    std::clamp(value, 0.0f, 1.0f) * 65'535.0f));
            };
            const auto opacity = gui::slider({
                .id = "texture.brush.opacity",
                .label = "Opacity",
                .minimum = 0.0f,
                .maximum = 1.0f,
                .value = normalizedBrushValue(
                    editor.textureBrushOpacity),
                .step = 0.01f,
                .size = {width, 28.0f}
            });
            if (opacity.changed)
            {
                editor.textureBrushOpacity =
                    authoringBrushValue(opacity.value);
            }
            const auto hardness = gui::slider({
                .id = "texture.brush.hardness",
                .label = "Hardness",
                .minimum = 0.0f,
                .maximum = 1.0f,
                .value = normalizedBrushValue(
                    editor.textureBrushHardness),
                .step = 0.01f,
                .size = {width, 28.0f}
            });
            if (hardness.changed)
            {
                editor.textureBrushHardness =
                    authoringBrushValue(hardness.value);
            }

            if (!paintingMask)
            {
                gui::label("Color Channels");
                constexpr std::array<std::string_view, 4> channelLabels{
                    "Red", "Green", "Blue", "Alpha"};
                for (std::size_t channel = 0u;
                     channel < channelLabels.size();
                     ++channel)
                {
                    const std::uint8_t channelBit =
                        static_cast<std::uint8_t>(1u << channel);
                    bool enabled =
                        (editor.textureBrushChannelMask & channelBit) != 0u;
                    if (!gui::toggle_switch(
                            channelLabels[channel],
                            enabled,
                            {width, 27.0f}))
                    {
                        continue;
                    }
                    const std::uint8_t nextMask = enabled
                        ? static_cast<std::uint8_t>(
                            editor.textureBrushChannelMask | channelBit)
                        : static_cast<std::uint8_t>(
                            editor.textureBrushChannelMask & ~channelBit);
                    if (nextMask == 0u)
                    {
                        editor.projectTextureStatus =
                            "Brush requires at least one color channel.";
                        continue;
                    }
                    editor.textureBrushChannelMask = nextMask;
                }
            }

            refresh_project_texture_preview(editor);
            if (!editor.projectTexturePreviewSprite.is_valid())
                return;
            const std::string imageId =
                std::string{identity} + ".texture_canvas";
            const gui::ImageBoxResult canvasResult =
                gui::image_box({
                    .id = imageId,
                    .sprite = editor.projectTexturePreviewSprite,
                    .size = {width, previewHeight},
                    .source_size = {
                        static_cast<float>(
                            editor.projectTexturePreviewWidth),
                        static_cast<float>(
                            editor.projectTexturePreviewHeight)},
                    .caption = state->logical_path,
                    .fit = gui::ImageFit::Contain,
                    .interactive = true,
                    .selected = true
                });
            if (canvasResult.image_bounds.size.x <= 0.0f
                || canvasResult.image_bounds.size.y <= 0.0f)
            {
                return;
            }

            if (editor.textureBrushActive
                && editor.textureBrushLogicalPath != state->logical_path)
            {
                editor.textureBrushActive = false;
                editor.textureBrushStroke = {};
                editor.textureBrushLogicalPath.clear();
            }

            const gui::Vec2 mouse = gui::mouse_position();
            const bool pointsAtCanvas = editor_point_in_rect(
                mouse,
                canvasResult.image_bounds.position,
                canvasResult.image_bounds.size);
            const auto appendSample = [&]()
            {
                const float u = std::clamp(
                    (mouse.x - canvasResult.image_bounds.position.x)
                        / canvasResult.image_bounds.size.x,
                    0.0f,
                    0.999999f);
                const float v = std::clamp(
                    (mouse.y - canvasResult.image_bounds.position.y)
                        / canvasResult.image_bounds.size.y,
                    0.0f,
                    0.999999f);
                const std::uint32_t pixelX =
                    static_cast<std::uint32_t>(
                        u * static_cast<float>(state->canvas.width));
                const std::uint32_t pixelY =
                    static_cast<std::uint32_t>(
                        v * static_cast<float>(state->canvas.height));
                if (pixelX == editor.textureBrushLastX
                    && pixelY == editor.textureBrushLastY)
                {
                    return;
                }
                if (editor.textureBrushStroke.samples.size() >= 4'096u)
                    return;
                editor.textureBrushStroke.samples.push_back({
                    .x_subpixels =
                        static_cast<std::int64_t>(pixelX) * 256 + 128,
                    .y_subpixels =
                        static_cast<std::int64_t>(pixelY) * 256 + 128
                });
                editor.textureBrushLastX = pixelX;
                editor.textureBrushLastY = pixelY;
            };

            if (gui::was_mouse_pressed() && pointsAtCanvas)
            {
                editor.textureBrushActive = true;
                editor.textureBrushLogicalPath = state->logical_path;
                editor.textureBrushStroke = {};
                editor.textureBrushStroke.program =
                    authoring::texture::BrushProgram::round_path_v2;
                editor.textureBrushStroke.algorithm_version = 2u;
                editor.textureBrushStroke.color = paintingMask
                    ? authoring::texture::PixelRgba8{
                        0u,
                        0u,
                        0u,
                        static_cast<std::uint8_t>(
                            editor.textureMaskPaintReveal ? 0u : 255u)}
                    : brushColors[editor.textureBrushColorIndex];
                editor.textureBrushStroke.radius_subpixels =
                    brushRadii[editor.textureBrushRadiusIndex] * 256u;
                editor.textureBrushStroke.opacity =
                    editor.textureBrushOpacity;
                editor.textureBrushStroke.hardness =
                    editor.textureBrushHardness;
                editor.textureBrushStroke.channel_mask = paintingMask
                    ? 0x08u
                    : editor.textureBrushChannelMask;
                editor.textureBrushLastX = 0xffffffffu;
                editor.textureBrushLastY = 0xffffffffu;
                appendSample();
                editor.projectTextureStatus = paintingMask
                    ? "Painting temporal mask stroke..."
                    : "Painting temporal stroke...";
            }
            else if (editor.textureBrushActive
                && gui::is_mouse_down()
                && pointsAtCanvas)
            {
                appendSample();
            }

            if (!editor.textureBrushActive || !gui::was_mouse_released())
                return;
            if (pointsAtCanvas)
                appendSample();

            const auto painted = controller->paint_editable(
                std::move(editor.textureBrushStroke));
            editor.textureBrushActive = false;
            editor.textureBrushStroke = {};
            editor.textureBrushLogicalPath.clear();
            editor.textureBrushLastX = 0xffffffffu;
            editor.textureBrushLastY = 0xffffffffu;
            editor.projectTextureStatus = painted
                ? epochengine::format_text(
                    paintingMask
                        ? "Painted mask revision {} across {} sparse R8 tile{}."
                        : "Painted revision {} across {} sparse tile{}.",
                    painted.revision.sequence,
                    painted.affected_tiles,
                    painted.affected_tiles == 1u ? "" : "s")
                : epochengine::format_text(
                    "Paint refused: {}.",
                    authoring::texture::result_code_name(
                        painted.authoring_code));
            editor.projectTexturePreviewSprite = {};
        }
        [[nodiscard]] bool selected_entity_accepts_canvas2d_material(
            const EditorState& editor) noexcept
        {
            const auto index =
                editor_entity_index(editor, editor.selectedEntityId);
            if (!index)
                return false;
            const EditorEntity& entity = editor.entities[*index];
            return entity.type != "Camera"
                && entity.type != "Light"
                && entity.type != "Spawn";
        }

        void assign_selected_project_texture(
            EditorState& editor)
        {
            auto* controller = project_texture_controller(editor);
            if (!controller)
                return;
            if (!selected_entity_accepts_canvas2d_material(editor))
            {
                editor.projectTextureStatus =
                    "Select a renderable scene object before assigning a texture.";
                return;
            }
            auto material = controller->selected_material();
            const auto handle = editor.sceneDocument.find(
                authoring::scene::ObjectId{editor.selectedEntityId});
            if (!material || !handle)
            {
                editor.projectTextureStatus =
                    "Import and select a texture before assignment.";
                return;
            }

            material->filter = editor.textureFilter;
            material->address_u = editor.textureAddressU;
            material->address_v = editor.textureAddressV;
            material->alpha = editor.textureAlpha;
            material->color_space = editor.textureColorSpace;
            material->alpha_cutoff = editor.textureAlphaCutoff;

            const std::array<authoring::scene::SceneOperation, 1>
                operations{authoring::scene::
                    TextureMaterialComponentChangedOperation{
                        .object = *handle,
                        .value = *material
                    }};
            if (apply_editor_scene_transaction(
                    editor,
                    operations,
                    "Assign texture material",
                    editor.selectedEntityId))
            {
                editor.projectTextureStatus = epochengine::format_text(
                    "{} assigned to {}.",
                    material->logical_path,
                    editor.entities[editor.selectedEntity].name);
            }
        }

        void clear_selected_project_texture(EditorState& editor)
        {
            const auto handle = editor.sceneDocument.find(
                authoring::scene::ObjectId{editor.selectedEntityId});
            if (!handle)
            {
                editor.projectTextureStatus =
                    "Select a scene object before clearing its material.";
                return;
            }
            const std::array<authoring::scene::SceneOperation, 1>
                operations{authoring::scene::
                    TextureMaterialComponentChangedOperation{
                        .object = *handle,
                        .value = std::nullopt
                    }};
            if (apply_editor_scene_transaction(
                    editor,
                    operations,
                    "Clear texture material",
                    editor.selectedEntityId))
            {
                editor.projectTextureStatus =
                    "Selected object now uses its solid fallback material.";
            }
        }
        void draw_texture_application_controls(
            EditorState& editor,
            float width)
        {
            auto* controller = project_texture_controller(editor);
            const auto* selectedTexture =
                controller ? controller->selected() : nullptr;
            const auto selectedEntity = editor_entity_index(
                editor, editor.selectedEntityId);
            const bool assignable =
                selectedTexture
                && selected_entity_accepts_canvas2d_material(editor);

            gui::label("Texture Application");
            gui::property_row(
                "Texture",
                selectedTexture
                    ? selectedTexture->logical_path
                    : std::string{"(select a compiled texture)"},
                82.0f);
            gui::property_row(
                "Target",
                selectedEntity
                    ? editor.entities[*selectedEntity].name
                    : std::string{"(select a scene object)"},
                82.0f);

            const std::array filterTabs{
                gui::SegmentedButtonSpec{
                    "Nearest", 86.0f,
                    editor.textureFilter
                        == scene::SceneTextureFilter::nearest},
                gui::SegmentedButtonSpec{
                    "Linear", 76.0f,
                    editor.textureFilter
                        == scene::SceneTextureFilter::linear}
            };
            if (const auto filter = gui::segmented_button_row(
                    filterTabs, 27.0f, 4.0f))
            {
                editor.textureFilter = *filter == 0u
                    ? scene::SceneTextureFilter::nearest
                    : scene::SceneTextureFilter::linear;
            }

            constexpr std::array<scene::SceneTextureAddress, 3>
                addressValues{
                    scene::SceneTextureAddress::clamp_to_edge,
                    scene::SceneTextureAddress::repeat,
                    scene::SceneTextureAddress::mirrored_repeat};
            const auto drawAddress = [&](
                std::string_view label,
                scene::SceneTextureAddress& value)
            {
                gui::label(label);
                const std::array tabs{
                    gui::SegmentedButtonSpec{
                        "Clamp", 68.0f,
                        value == addressValues[0]},
                    gui::SegmentedButtonSpec{
                        "Repeat", 72.0f,
                        value == addressValues[1]},
                    gui::SegmentedButtonSpec{
                        "Mirror", 72.0f,
                        value == addressValues[2]}
                };
                if (const auto selected = gui::segmented_button_row(
                        tabs, 27.0f, 3.0f))
                {
                    value = addressValues[*selected];
                }
            };
            drawAddress("Address U", editor.textureAddressU);
            drawAddress("Address V", editor.textureAddressV);

            gui::label("Alpha");
            const std::array alphaTabs{
                gui::SegmentedButtonSpec{
                    "Opaque", 66.0f,
                    editor.textureAlpha
                        == scene::SceneTextureAlphaMode::opaque},
                gui::SegmentedButtonSpec{
                    "Blend", 62.0f,
                    editor.textureAlpha
                        == scene::SceneTextureAlphaMode::straight},
                gui::SegmentedButtonSpec{
                    "Premult", 76.0f,
                    editor.textureAlpha
                        == scene::SceneTextureAlphaMode::premultiplied},
                gui::SegmentedButtonSpec{
                    "Cutout", 66.0f,
                    editor.textureAlpha
                        == scene::SceneTextureAlphaMode::cutout}
            };
            if (const auto alpha = gui::segmented_button_row(
                    alphaTabs, 27.0f, 3.0f))
            {
                constexpr std::array<scene::SceneTextureAlphaMode, 4>
                    values{
                        scene::SceneTextureAlphaMode::opaque,
                        scene::SceneTextureAlphaMode::straight,
                        scene::SceneTextureAlphaMode::premultiplied,
                        scene::SceneTextureAlphaMode::cutout};
                editor.textureAlpha = values[*alpha];
            }

            if (editor.textureAlpha
                == scene::SceneTextureAlphaMode::cutout)
            {
                const auto cutoff = gui::slider({
                    .id = "assets.texture.alpha_cutoff",
                    .label = "Alpha cutoff",
                    .minimum = 0.0f,
                    .maximum = 1.0f,
                    .value = editor.textureAlphaCutoff,
                    .step = 0.01f,
                    .size = {
                        (std::min)(width, 420.0f),
                        28.0f}
                });
                if (cutoff.changed)
                    editor.textureAlphaCutoff = cutoff.value;
            }

            gui::label("Color Space");
            const std::array colorTabs{
                gui::SegmentedButtonSpec{
                    "Linear", 76.0f,
                    editor.textureColorSpace
                        == scene::SceneTextureColorSpace::linear},
                gui::SegmentedButtonSpec{
                    "sRGB", 68.0f,
                    editor.textureColorSpace
                        == scene::SceneTextureColorSpace::srgb}
            };
            if (const auto color = gui::segmented_button_row(
                    colorTabs, 27.0f, 4.0f))
            {
                editor.textureColorSpace = *color == 0u
                    ? scene::SceneTextureColorSpace::linear
                    : scene::SceneTextureColorSpace::srgb;
            }

            const std::array actions{
                gui::InlineButtonSpec{
                    .label = "Assign To Selection",
                    .width = 154.0f,
                    .enabled = assignable},
                gui::InlineButtonSpec{
                    .label = "Clear Material",
                    .width = 118.0f,
                    .enabled = selectedEntity.has_value()}
            };
            if (const auto action = gui::inline_button_row(
                    actions, 29.0f, 6.0f))
            {
                if (*action == 0u)
                    assign_selected_project_texture(editor);
                else
                    clear_selected_project_texture(editor);
            }
            gui::wrapped_label(editor.projectTextureStatus, width);
        }

