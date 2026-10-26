// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] bool render_gui_document_widget(
            EditorState& state,
            authoring::gui::GuiDocument& document,
            const authoring::gui::WidgetView& widget,
            gui::Vec2& position,
            gui::Vec2 size,
            gui::Vec2 virtualParentSize,
            authoring::gui::LayoutFlow parentFlow,
            float canvasScale,
            bool interactive)
        {
            using namespace authoring::gui;
            const std::string label = gui_widget_display_text(widget);
            const bool selected =
                gui_widget_selected(state, widget.handle);
            bool activated = false;

            if (interactive)
            {
                const bool dragActive = state.guiCanvasWidgetDrag.pending
                    || state.guiCanvasWidgetDrag.dragging;
                const bool ownsActiveDrag =
                    state.guiCanvasWidgetDragHandle == widget.handle;
                if (!dragActive || ownsActiveDrag)
                {
                    if (!dragActive)
                        state.guiCanvasWidgetDragHandle = widget.handle;
                    const gui::DragSurfaceResult drag = gui::drag_surface(
                        state.guiCanvasWidgetDrag,
                        gui::DragSurfaceOptions{
                            .position = position,
                            .size = size,
                            .hit_padding = 7.0f,
                            .minimum_hit_extent = 36.0f,
                            .drag_threshold = 3.0f,
                            .enabled = true});
                    if (drag.pressed)
                    {
                        select_gui_widget(state, widget.handle);
                        state.guiCanvasWidgetDragOriginX =
                            widget.descriptor.layout.x;
                        state.guiCanvasWidgetDragOriginY =
                            widget.descriptor.layout.y;
                        state.guiCanvasWidgetDragPreviewX =
                            state.guiCanvasWidgetDragOriginX;
                        state.guiCanvasWidgetDragPreviewY =
                            state.guiCanvasWidgetDragOriginY;
                    }
                    if (drag.dragging || drag.released)
                    {
                        const float inverseScale =
                            canvasScale > 0.0f ? 1.0f / canvasScale : 1.0f;
                        const float maximumX = (std::max)(
                            0.0f,
                            virtualParentSize.x
                                - widget.descriptor.layout.width);
                        const float maximumY = (std::max)(
                            0.0f,
                            virtualParentSize.y
                                - widget.descriptor.layout.height);
                        if (parentFlow != LayoutFlow::horizontal)
                        {
                            state.guiCanvasWidgetDragPreviewX =
                                (std::clamp)(
                                    state.guiCanvasWidgetDragOriginX
                                        + drag.delta.x * inverseScale,
                                    0.0f,
                                    maximumX);
                        }
                        if (parentFlow != LayoutFlow::vertical)
                        {
                            state.guiCanvasWidgetDragPreviewY =
                                (std::clamp)(
                                    state.guiCanvasWidgetDragOriginY
                                        + drag.delta.y * inverseScale,
                                    0.0f,
                                    maximumY);
                        }
                        if (parentFlow != LayoutFlow::horizontal)
                        {
                            position.x +=
                                (state.guiCanvasWidgetDragPreviewX
                                    - widget.descriptor.layout.x)
                                * canvasScale;
                        }
                        if (parentFlow != LayoutFlow::vertical)
                        {
                            position.y +=
                                (state.guiCanvasWidgetDragPreviewY
                                    - widget.descriptor.layout.y)
                                * canvasScale;
                        }
                    }
                    if (drag.released)
                    {
                        LayoutDescriptor changed =
                            widget.descriptor.layout;
                        changed.x = state.guiCanvasWidgetDragPreviewX;
                        changed.y = state.guiCanvasWidgetDragPreviewY;
                        const MutationResult committed =
                            document.update_layout(
                                widget.handle,
                                std::move(changed));
                        if (committed)
                        {
                            (void)project_gui_document_layout(state);
                            state.guiDocumentStatus =
                                "Moved GUI widget on the canvas. Undo restores its previous layout.";
                        }
                        else if (committed.code != ResultCode::no_change)
                        {
                            state.guiDocumentStatus =
                                "GUI canvas move rejected: "
                                + std::string(result_code_name(committed.code));
                        }
                    }
                    if (!state.guiCanvasWidgetDrag.pending
                        && !state.guiCanvasWidgetDrag.dragging)
                    {
                        state.guiCanvasWidgetDragHandle = {};
                    }
                }
            }            gui::set_cursor(position);

            if (widget.descriptor.kind == WidgetKind::image
                || widget.descriptor.kind == WidgetKind::image_button)
            {
                SpriteHandle sprite{};
                if (auto* controller = project_texture_controller(state))
                {
                    const auto catalog = controller->catalog();
                    const auto found = std::ranges::find(
                        catalog,
                        gui_widget_asset_path(widget),
                        &editor_project_textures::
                            TextureCatalogEntry::logical_path);
                    if (found != catalog.end())
                        sprite = project_texture_thumbnail(
                            state, *controller, *found);
                }
                if (sprite.is_valid())
                {
                    const auto image = gui::image_box({
                        .id = widget.descriptor.name,
                        .sprite = sprite,
                        .size = size,
                        .source_size = size,
                        .caption = label,
                        .fit = gui::ImageFit::Contain,
                        .interactive = interactive,
                        .selected = selected});
                    activated = image.clicked;
                }
                else
                {
                    gui::panel_rect(position, size);
                    gui::set_cursor({
                        position.x + 6.0f, position.y + 4.0f});
                    gui::label(label.empty()
                        ? widget.descriptor.name : label);
                }
            }
            else if (widget.descriptor.kind == WidgetKind::button)
            {
                if (interactive)
                {
                    activated = gui::button_selected(
                        label.empty() ? widget.descriptor.name : label,
                        size,
                        selected);
                }
                else
                {
                    gui::panel_rect(position, size);
                    gui::set_cursor({
                        position.x + 6.0f, position.y + 4.0f});
                    gui::label(label);
                }
            }
            else if (widget.descriptor.kind
                == WidgetKind::text_input)
            {
                if (const auto* content =
                        std::get_if<TextInputContent>(
                            &widget.descriptor.content))
                {
                    if (interactive && selected
                        && !content->read_only)
                    {
                        hydrate_gui_content_editor(
                            state,
                            state.selectedEntityId,
                            widget);
                        const auto edited = gui::edit_box(
                            state.guiContentPrimary,
                            size,
                            content->maximum_length,
                            content->multiline);
                        activated = edited.active;
                        if (edited.changed)
                        {
                            TextInputContent changed =
                                *content;
                            changed.text =
                                state.guiContentPrimary;
                            (void)document.update_content(
                                widget.handle,
                                std::move(changed));
                            state.guiDocumentStatus =
                                "Edited text input content.";
                        }
                    }
                    else if (interactive)
                    {
                        activated =
                            gui::button_selected(
                                label.empty()
                                    ? content->placeholder
                                    : label,
                                size,
                                selected);
                    }
                    else
                    {
                        gui::panel_rect(position, size);
                        gui::set_cursor({
                            position.x + 6.0f,
                            position.y + 4.0f});
                        gui::label(
                            label.empty()
                                ? content->placeholder
                                : label);
                    }
                }
            }
            else if (widget.descriptor.kind == WidgetKind::slider)
            {
                if (const auto* content = std::get_if<SliderContent>(
                        &widget.descriptor.content))
                {
                    const auto slider = gui::slider({
                        .id = widget.descriptor.name,
                        .label = content->label,
                        .minimum = static_cast<float>(content->minimum),
                        .maximum = static_cast<float>(content->maximum),
                        .value = static_cast<float>(content->value),
                        .step = static_cast<float>(content->step),
                        .size = size,
                        .enabled = interactive
                            && widget.descriptor.interaction.enabled});
                    if (interactive && slider.changed)
                    {
                        SliderContent changed = *content;
                        changed.value = slider.value;
                        (void)document.update_content(
                            widget.handle, std::move(changed));
                        activated = true;
                    }
                }
            }
            else if (widget.descriptor.kind
                == WidgetKind::tab_set)
            {
                gui::panel_rect(position, size);
                const auto* content =
                    std::get_if<TabSetContent>(
                        &widget.descriptor.content);
                std::vector<WidgetView> pages{};
                for (const auto& candidate :
                     document.widgets())
                {
                    if (candidate.parent == widget.handle
                        && candidate.descriptor.kind
                            == WidgetKind::tab_page)
                    {
                        pages.push_back(candidate);
                    }
                }
                std::vector<gui::TabButtonSpec> tabs{};
                tabs.reserve(pages.size());
                const float tabWidth = pages.empty()
                    ? 72.0f
                    : (std::max)(
                        54.0f,
                        (size.x - 8.0f)
                            / static_cast<float>(
                                pages.size()));
                for (const auto& page : pages)
                {
                    const auto* pageContent =
                        std::get_if<TabPageContent>(
                            &page.descriptor.content);
                    tabs.push_back({
                        .id = page.descriptor.name,
                        .label = pageContent
                            ? std::string_view{
                                pageContent->label}
                            : std::string_view{
                                page.descriptor.name},
                        .width = tabWidth,
                        .active = content
                            && content->selected_page
                                == page.handle,
                        .closable = false});
                }
                gui::set_cursor({
                    position.x + 4.0f,
                    position.y + 4.0f});
                if (interactive && !tabs.empty())
                {
                    const auto tabResult =
                        gui::tab_bar_buttons(
                            tabs,
                            (std::min)(
                                30.0f,
                                size.y - 8.0f),
                            1.0f);
                    if (tabResult.selected_index
                        && *tabResult.selected_index
                            < pages.size())
                    {
                        (void)document.update_content(
                            widget.handle,
                            TabSetContent{
                                pages[*tabResult
                                    .selected_index]
                                    .handle});
                        activated = true;
                    }
                }
            }
            else
            {
                gui::panel_rect(position, size);
                gui::set_cursor({
                    position.x + 6.0f, position.y + 4.0f});
                if (widget.descriptor.kind == WidgetKind::text)
                    gui::wrapped_label(label, size.x);
                else
                    gui::label(label.empty()
                        ? widget.descriptor.name : label);
            }

            if (selected || gui_widget_selected(state, widget.handle))
                gui::selection_outline(position, size, 2.0f);
            if (!activated && interactive
                && gui::was_mouse_pressed()
                && editor_point_in_rect(
                    gui::mouse_position(), position, size))
                activated = true;
            if (activated)
                select_gui_widget(state, widget.handle);
            return activated;
        }

        struct GuiDocumentCanvasFrame final
        {
            authoring::gui::GuiDocument* document{};
            authoring::gui::WidgetHandle root{};
            gui::Vec2 position{};
            gui::Vec2 size{};
            float scale{};

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return document && static_cast<bool>(root)
                    && size.x > 0.0f && size.y > 0.0f
                    && scale > 0.0f;
            }
        };

        [[nodiscard]] GuiDocumentCanvasFrame compute_gui_document_canvas(
            authoring::gui::GuiDocument& document,
            gui::Vec2 surfacePosition,
            gui::Vec2 surfaceSize)
        {
            using namespace authoring::gui;
            const WidgetHandle rootHandle = document.roots().front();
            const auto root = document.widget(rootHandle);
            const auto* canvas = root
                ? std::get_if<CanvasContent>(&root->descriptor.content)
                : nullptr;
            const float virtualWidth = canvas
                ? (std::max)(1.0f, canvas->virtual_width)
                : 1'280.0f;
            const float virtualHeight = canvas
                ? (std::max)(1.0f, canvas->virtual_height)
                : 720.0f;
            const float surfaceWidth = (std::max)(1.0f, surfaceSize.x);
            const float surfaceHeight = (std::max)(1.0f, surfaceSize.y);
            const float insetX = (std::min)(20.0f, surfaceWidth * 0.05f);
            const float insetY = (std::min)(12.0f, surfaceHeight * 0.05f);
            const float availableWidth = surfaceWidth - 2.0f * insetX;
            const float availableHeight = surfaceHeight - 2.0f * insetY;
            const float scale = (std::min)(
                availableWidth / virtualWidth,
                availableHeight / virtualHeight);
            const gui::Vec2 canvasSize{
                virtualWidth * scale,
                virtualHeight * scale};
            const gui::Vec2 canvasPosition{
                surfacePosition.x
                    + (surfaceWidth - canvasSize.x) * 0.5f,
                surfacePosition.y + (surfaceHeight - canvasSize.y) * 0.5f};
            return {
                &document,
                rootHandle,
                canvasPosition,
                canvasSize,
                scale};
        }

        [[nodiscard]] GuiDocumentCanvasFrame prepare_gui_document_canvas(
            EditorState& state, gui::Vec2 pos, gui::Vec2 size)
        {
            auto* document = synchronize_gui_document(state);
            gui::panel_rect(pos, size);
            if (!document || !document->valid() || document->roots().empty())
                return {};
            const auto frame = compute_gui_document_canvas(*document, pos, size);
            gui::panel_rect(frame.position, frame.size);
            return frame;
        }

        void render_gui_document_canvas(
            EditorState& state,
            gui::Vec2 position,
            gui::Vec2 size,
            bool interactive)
        {
            const auto frame =
                prepare_gui_document_canvas(state, position, size);
            if (!frame)
                return;
            if (state.guiCanvasWidgetDragHandle
                && !frame.document->widget(
                    state.guiCanvasWidgetDragHandle))
            {
                state.guiCanvasWidgetDrag = {};
                state.guiCanvasWidgetDragHandle = {};
            }
            const std::vector<authoring::gui::WidgetView>
                widgets = frame.document->widgets();
            std::function<void(
                authoring::gui::WidgetHandle,
                gui::Vec2,
                gui::Vec2,
                std::size_t)> renderChildren;
            renderChildren = [&](
                authoring::gui::WidgetHandle parent,
                gui::Vec2 parentPosition,
                gui::Vec2 parentSize,
                std::size_t depth)
            {
                using namespace authoring::gui;
                if (depth >= 64u)
                    return;
                const auto parentView =
                    frame.document->widget(parent);
                if (!parentView)
                    return;

                const LayoutDescriptor& parentLayout =
                    parentView->descriptor.layout;
                const float virtualParentWidth =
                    parentSize.x / frame.scale;
                const float virtualParentHeight =
                    parentSize.y / frame.scale;
                const float availableWidth = (std::max)(
                    1.0f,
                    virtualParentWidth
                        - parentLayout.padding.left
                        - parentLayout.padding.right);
                const float availableHeight = (std::max)(
                    1.0f,
                    virtualParentHeight
                        - parentLayout.padding.top
                        - parentLayout.padding.bottom);
                float flowX = parentLayout.padding.left;
                float flowY = parentLayout.padding.top;

                for (const auto& widget : widgets)
                {
                    if (widget.parent != parent
                        || !widget.descriptor
                            .interaction.visible)
                    {
                        continue;
                    }

                    if (parentView->descriptor.kind
                        == WidgetKind::tab_set)
                    {
                        const auto* tabSet =
                            std::get_if<TabSetContent>(
                                &parentView->descriptor.content);
                        if (widget.descriptor.kind
                                == WidgetKind::tab_page
                            && tabSet
                            && tabSet->selected_page
                                == widget.handle)
                        {
                            const float tabHeight =
                                (std::min)(
                                    34.0f,
                                    parentSize.y * 0.35f);
                            renderChildren(
                                widget.handle,
                                {
                                    parentPosition.x,
                                    parentPosition.y
                                        + tabHeight},
                                {
                                    parentSize.x,
                                    (std::max)(
                                        18.0f,
                                        parentSize.y
                                            - tabHeight)},
                                depth + 1u);
                        }
                        continue;
                    }

                    const LayoutDescriptor& layout =
                        widget.descriptor.layout;
                    const float virtualWidth =
                        layout.width_rule == SizeRule::fill
                        ? availableWidth
                        : (std::clamp)(
                            layout.width,
                            (std::max)(
                                1.0f,
                                layout.minimum_width),
                            layout.maximum_width);
                    const float virtualHeight =
                        layout.height_rule == SizeRule::fill
                        ? availableHeight
                        : (std::clamp)(
                            layout.height,
                            (std::max)(
                                1.0f,
                                layout.minimum_height),
                            layout.maximum_height);
                    float virtualX = layout.x;
                    float virtualY = layout.y;
                    if (parentLayout.flow
                        == LayoutFlow::horizontal)
                    {
                        virtualX = flowX;
                        virtualY =
                            parentLayout.padding.top
                                + layout.y;
                    }
                    else if (parentLayout.flow
                        == LayoutFlow::vertical)
                    {
                        virtualX =
                            parentLayout.padding.left
                                + layout.x;
                        virtualY = flowY;
                    }

                    gui::Vec2 widgetPosition{
                        parentPosition.x
                            + virtualX * frame.scale,
                        parentPosition.y
                            + virtualY * frame.scale};
                    const gui::Vec2 widgetSize{
                        (std::max)(
                            18.0f,
                            virtualWidth * frame.scale),
                        (std::max)(
                            18.0f,
                            virtualHeight * frame.scale)};
                    (void)render_gui_document_widget(
                        state,
                        *frame.document,
                        widget,
                        widgetPosition,
                        widgetSize,
                        {availableWidth, availableHeight},
                        parentLayout.flow,
                        frame.scale,
                        interactive);

                    if (widget.descriptor.kind
                            == WidgetKind::panel
                        || widget.descriptor.kind
                            == WidgetKind::scroll_area
                        || widget.descriptor.kind
                            == WidgetKind::tab_set)
                    {
                        renderChildren(
                            widget.handle,
                            widgetPosition,
                            widgetSize,
                            depth + 1u);
                    }

                    if (parentLayout.flow
                        == LayoutFlow::horizontal)
                    {
                        flowX += virtualWidth
                            + parentLayout.spacing;
                    }
                    else if (parentLayout.flow
                        == LayoutFlow::vertical)
                    {
                        flowY += virtualHeight
                            + parentLayout.spacing;
                    }
                }
            };
            renderChildren(
                frame.root,
                frame.position,
                frame.size,
                0u);
        }

        [[nodiscard]] bool activate_project_texture(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            editor_project_textures::ProjectTextureController& controller,
            const editor_project_textures::TextureCatalogEntry& entry)
        {
            editor_project_textures::ControllerResult result{};
            if (entry.logical_path.ends_with(".epoch_texture"))
            {
                const auto renderer =
                    capability::renderer_subsystem_profile_for(
                        renderer_backend_kind(ctx));
                result = controller.open_editable(
                    entry.logical_path,
                    renderer,
                    renderer.capability.recommended_budgets);
            }
            else
            {
                result = controller.select(entry.logical_path);
            }

            if (!result)
            {
                editor.projectTextureStatus =
                    result.code
                            == editor_project_textures::
                                ControllerCode::unsaved_changes
                        ? "Save or undo the current texture before opening another document."
                        : epochengine::format_text(
                            "Texture selection refused: {}.",
                            editor_project_textures::
                                controller_code_name(result.code));
                return false;
            }

            editor.selectedAssetPath = entry.logical_path;
            editor.selectedProjectFile = display_project_path(
                entry.source_path);
            editor.projectTexturePreviewSprite = {};
            editor.projectTextureStatus = epochengine::format_text(
                "{} selected ({}x{}, revision {}).",
                entry.logical_path,
                entry.width,
                entry.height,
                entry.source_sequence);
            return true;
        }

        void draw_project_texture_catalog(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            editor_project_textures::ProjectTextureController& controller,
            float width,
            std::string_view identity,
            bool thumbnails)
        {
            constexpr std::size_t kMaximumVisibleTextures = 64u;
            std::size_t visible = 0u;
            const auto containsNoCase = [](
                std::string_view value,
                std::string_view query) noexcept
            {
                if (query.empty())
                    return true;
                const auto equal = [](char left, char right) noexcept
                {
                    return std::tolower(
                        static_cast<unsigned char>(left))
                        == std::tolower(
                            static_cast<unsigned char>(right));
                };
                return std::search(
                    value.begin(), value.end(),
                    query.begin(), query.end(),
                    equal) != value.end();
            };

            for (const auto& texture : controller.catalog())
            {
                if (!containsNoCase(
                        texture.logical_path,
                        editor.assetSearch))
                {
                    continue;
                }
                if (visible >= kMaximumVisibleTextures)
                    break;
                ++visible;

                const bool selected =
                    controller.selected_logical_path()
                        == texture.logical_path;
                bool activated = false;
                if (thumbnails)
                {
                    const SpriteHandle sprite =
                        project_texture_thumbnail(
                            editor,
                            controller,
                            texture);
                    if (sprite.is_valid())
                    {
                        const std::string buttonId =
                            epochengine::format_text(
                                "{}.thumbnail.{}",
                                identity,
                                texture.logical_path);
                        activated = gui::image_button(
                            gui::ImageButtonOptions{
                                .id = buttonId,
                                .sprite = sprite,
                                .size = {
                                    (std::min)(width, 176.0f),
                                    112.0f},
                                .source_size = {
                                    static_cast<float>(texture.width),
                                    static_cast<float>(texture.height)},
                                .fit = gui::ImageFit::Contain,
                                .selected = selected
                            });
                        gui::wrapped_label(
                            std::filesystem::path{
                                texture.logical_path}
                                .filename()
                                .string(),
                            width);
                        gui::property_row(
                            "Texture",
                            epochengine::format_text(
                                "{}x{} | {} KiB",
                                texture.width,
                                texture.height,
                                (texture.decoded_bytes + 1023u) / 1024u),
                            68.0f);
                    }
                    else
                    {
                        activated = gui::button_selected(
                            epochengine::format_text(
                                "{}  {}x{}",
                                texture.logical_path,
                                texture.width,
                                texture.height),
                            {width, 29.0f},
                            selected);
                    }
                }
                else
                {
                    activated = gui::button_selected(
                        epochengine::format_text(
                            "{}  {}x{}",
                            texture.logical_path,
                            texture.width,
                            texture.height),
                        {width, 28.0f},
                        selected);
                }

                if (activated)
                {
                    (void)activate_project_texture(
                        editor,
                        ctx,
                        controller,
                        texture);
                }
            }

            if (visible == 0u)
            {
                gui::wrapped_label(
                    controller.catalog().empty()
                        ? "No project textures have been compiled yet."
                        : "No compiled texture matches the current search.",
                    width);
            }
            else if (visible == kMaximumVisibleTextures
                && controller.catalog().size() > visible)
            {
                gui::property_row(
                    "Showing",
                    epochengine::format_text(
                        "{} / {} textures",
                        visible,
                        controller.catalog().size()),
                    68.0f);
            }
        }
        void create_starter_project_texture(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx)
        {
            try
            {
                auto* controller = project_texture_controller(editor);
                if (!controller)
                    return;

                std::string stem{};
                stem.reserve(editor.newTextureName.size());
                for (const unsigned char character : editor.newTextureName)
                {
                    if (std::isalnum(character)
                        || character == '_'
                        || character == '-')
                    {
                        stem.push_back(static_cast<char>(character));
                    }
                    else if ((character == ' ' || character == '.')
                        && !stem.empty()
                        && stem.back() != '_')
                    {
                        stem.push_back('_');
                    }
                }
                while (!stem.empty() && stem.back() == '_')
                    stem.pop_back();
                if (stem.empty())
                {
                    editor.projectTextureStatus =
                        "Texture name must contain a letter or number.";
                    return;
                }

                constexpr std::array<std::uint32_t, 4> extents{
                    64u, 128u, 256u, 512u};
                editor.newTextureExtentIndex = (std::min)(
                    editor.newTextureExtentIndex,
                    extents.size() - 1u);
                editor.newTexturePatternIndex = (std::min)(
                    editor.newTexturePatternIndex,
                    std::size_t{2u});
                const std::uint32_t extent =
                    extents[editor.newTextureExtentIndex];
                const std::string logicalPath =
                    "Assets/Textures/" + stem + ".epoch_texture";

                authoring::texture::CanvasDescriptor canvas{};
                canvas.width = extent;
                canvas.height = extent;
                canvas.tile_extent = static_cast<std::uint16_t>(
                    (std::min)(128u, extent));
                canvas.mip_count = 1u;
                canvas.format =
                    authoring::texture::PixelFormat::rgba8_srgb;
                canvas.color_space = asset::texture::ColorSpace::srgb;

                const RendererBackendKind backend =
                    renderer_backend_kind(ctx);
                const capability::SubsystemProfile renderer =
                    capability::renderer_subsystem_profile_for(backend);
                const Budgets budgets =
                    renderer.capability.recommended_budgets;
                const auto created = controller->create_editable(
                    logicalPath,
                    canvas,
                    renderer,
                    budgets);
                if (!created)
                {
                    editor.projectTextureStatus =
                        epochengine::format_text(
                            "Editable texture creation refused: {} | source {} | authoring {}.",
                            editor_project_textures::controller_code_name(
                                created.code),
                            project_texture_sources::source_code_name(
                                created.source_code),
                            authoring::texture::result_code_name(
                                created.authoring_code));
                    return;
                }

                const std::uint32_t cells = 8u;
                const std::uint32_t cellExtent =
                    (std::max)(1u, extent / cells);
                const std::uint32_t radius =
                    cellExtent * 196u;
                const auto makeStroke = [&](
                    authoring::texture::PixelRgba8 color,
                    std::uint32_t firstX,
                    std::uint32_t lastX,
                    bool alternating)
                {
                    authoring::texture::StrokeDescriptor stroke{};
                    stroke.color = color;
                    stroke.radius_subpixels = radius;
                    for (std::uint32_t y = 0u; y < cells; ++y)
                    {
                        for (std::uint32_t x = firstX;
                             x < lastX;
                             ++x)
                        {
                            if (alternating
                                && ((x + y) % 2u != 0u))
                            {
                                continue;
                            }
                            stroke.samples.push_back({
                                .x_subpixels =
                                    static_cast<std::int64_t>(
                                        x * cellExtent
                                        + cellExtent / 2u) * 256,
                                .y_subpixels =
                                    static_cast<std::int64_t>(
                                        y * cellExtent
                                        + cellExtent / 2u) * 256
                            });
                        }
                    }
                    return controller->paint_editable(
                        std::move(stroke));
                };

                bool initialized = true;
                if (editor.newTexturePatternIndex == 0u)
                {
                    initialized = static_cast<bool>(makeStroke(
                        {38u, 44u, 54u, 255u},
                        0u,
                        cells,
                        false));
                    initialized = initialized
                        && static_cast<bool>(makeStroke(
                            {232u, 236u, 242u, 255u},
                            0u,
                            cells,
                            true));
                }
                else if (editor.newTexturePatternIndex == 1u)
                {
                    for (std::uint32_t band = 0u;
                         band < cells && initialized;
                         ++band)
                    {
                        const float u = static_cast<float>(band)
                            / static_cast<float>(cells - 1u);
                        initialized = static_cast<bool>(makeStroke(
                            {
                                static_cast<std::uint8_t>(
                                    32.0f + u * 196.0f),
                                static_cast<std::uint8_t>(
                                    42.0f + u * 172.0f),
                                static_cast<std::uint8_t>(
                                    216.0f - u * 120.0f),
                                255u
                            },
                            band,
                            band + 1u,
                            false));
                    }
                }
                else
                {
                    initialized = static_cast<bool>(makeStroke(
                        {72u, 132u, 196u, 255u},
                        0u,
                        cells,
                        false));
                }

                const auto saved = initialized
                    ? controller->save_editable(renderer, budgets)
                    : editor_project_textures::ControllerResult{};
                if (!initialized || !saved)
                {
                    editor.projectTextureStatus =
                        "The editable texture was created, but its initial paint or publish step failed.";
                    return;
                }

                editor.selectedProjectFile = display_project_path(
                    resolve_editor_path(
                        std::filesystem::path{editor.projectRoot})
                    / std::filesystem::path{logicalPath});
                editor.selectedAssetPath = logicalPath;
                editor.projectTexturePreviewSprite = {};
                editor.projectTextureStatus =
                    epochengine::format_text(
                        "{} created as a temporal texture document ({}x{}, revision {}).",
                        stem,
                        extent,
                        extent,
                        saved.entry->source_sequence);
                push_editor_log(
                    editor,
                    "[assets] " + editor.projectTextureStatus);
            }
            catch (...)
            {
                editor.projectTextureStatus =
                    "Texture source creation failed safely.";
            }
        }
