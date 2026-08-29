/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

module authoring.gui_document;

namespace epochengine::authoring::gui
{
    namespace
    {
        [[nodiscard]] GuiDocument make_document(DocumentLimits limits = {})
        {
            return GuiDocument{
                DocumentHandle{17u, 2u},
                BranchIdentity{29u, 5u},
                limits
            };
        }

        [[nodiscard]] WidgetDescriptor widget_descriptor(
            WidgetKind kind,
            std::string name,
            WidgetContent content)
        {
            WidgetDescriptor descriptor{};
            descriptor.kind = kind;
            descriptor.name = std::move(name);
            descriptor.content = std::move(content);
            return descriptor;
        }

        [[nodiscard]] WidgetDescriptor canvas(std::string name)
        {
            return widget_descriptor(
                WidgetKind::canvas,
                std::move(name),
                CanvasContent{});
        }

        [[nodiscard]] WidgetDescriptor panel(std::string name)
        {
            return widget_descriptor(
                WidgetKind::panel,
                std::move(name),
                PanelContent{});
        }

        [[nodiscard]] WidgetDescriptor button(
            std::string name,
            std::string label)
        {
            WidgetDescriptor descriptor = widget_descriptor(
                WidgetKind::button,
                std::move(name),
                ButtonContent{std::move(label)});
            descriptor.interaction.focusable = true;
            descriptor.interaction.accepts_pointer = true;
            return descriptor;
        }

        [[nodiscard]] WidgetDescriptor text(
            std::string name,
            std::string value)
        {
            return widget_descriptor(
                WidgetKind::text,
                std::move(name),
                TextContent{std::move(value), true});
        }

        [[nodiscard]] WidgetDescriptor scroll_area(std::string name)
        {
            return widget_descriptor(
                WidgetKind::scroll_area,
                std::move(name),
                ScrollAreaContent{});
        }

        [[nodiscard]] WidgetDescriptor tab_set(std::string name)
        {
            return widget_descriptor(
                WidgetKind::tab_set,
                std::move(name),
                TabSetContent{});
        }

        [[nodiscard]] WidgetDescriptor tab_page(
            std::string name,
            std::string label)
        {
            return widget_descriptor(
                WidgetKind::tab_page,
                std::move(name),
                TabPageContent{std::move(label), false});
        }

        [[nodiscard]] bool contains_kind(
            const std::vector<OperationRecord>& journal,
            OperationKind kind)
        {
            return std::any_of(
                journal.begin(),
                journal.end(),
                [kind](const OperationRecord& record)
                {
                    return record.kind() == kind;
                });
        }

        [[nodiscard]] GuiDocument deterministic_document()
        {
            GuiDocument document = make_document();
            const WidgetHandle root =
                document.create_widget(canvas("Root")).widget;
            const WidgetHandle body =
                document.create_widget(panel("Body"), root).widget;
            const WidgetHandle tabs =
                document.create_widget(tab_set("ModeTabs"), body).widget;
            const WidgetHandle page = document.create_widget(
                tab_page("PropertiesPage", "Properties"), tabs).widget;
            WidgetDescriptor imageButton = widget_descriptor(
                WidgetKind::image_button,
                "ApplyTexture",
                ImageButtonContent{
                    "Assets/Textures/apply.png",
                    "Apply",
                    true});
            imageButton.interaction.action = "texture.apply";
            const WidgetHandle apply =
                document.create_widget(std::move(imageButton), page).widget;
            (void)apply;
            (void)document.update_content(
                tabs,
                WidgetContent{TabSetContent{page}});
            LayoutDescriptor layout = document.widget(body)->descriptor.layout;
            layout.flow = LayoutFlow::vertical;
            layout.spacing = 6.0f;
            (void)document.update_layout(body, layout);
            return document;
        }
    }

    ContractFailure run_contract()
    {
        GuiDocument invalid{DocumentHandle{}, BranchIdentity{29u, 5u}};
        if (invalid.valid()
            || invalid.create_widget(canvas("Rejected")).code
                != ResultCode::invalid_document
            || invalid.validate().code != ResultCode::invalid_document)
        {
            return ContractFailure::invalid_document;
        }

        GuiDocument descriptors = make_document();
        WidgetDescriptor emptyName = canvas("");
        WidgetDescriptor mismatch = button("Mismatch", "Button");
        mismatch.content = TextContent{"Wrong variant", true};
        WidgetDescriptor invalidLayout = canvas("InvalidLayout");
        invalidLayout.layout.width =
            (std::numeric_limits<float>::quiet_NaN)();
        WidgetDescriptor invalidImage = widget_descriptor(
            WidgetKind::image,
            "Image",
            ImageContent{});
        WidgetDescriptor invalidSlider = widget_descriptor(
            WidgetKind::slider,
            "Slider",
            SliderContent{"Value", 1.0, 1.0, 1.0, 0.1});
        if (descriptors.create_widget(std::move(emptyName)).code
                != ResultCode::empty_name
            || descriptors.create_widget(std::move(mismatch)).code
                != ResultCode::content_kind_mismatch
            || descriptors.create_widget(std::move(invalidLayout)).code
                != ResultCode::invalid_layout
            || descriptors.create_widget(std::move(invalidImage)).code
                != ResultCode::invalid_content
            || descriptors.create_widget(std::move(invalidSlider)).code
                != ResultCode::invalid_content)
        {
            return ContractFailure::descriptor_validation;
        }

        GuiDocument document = make_document();
        const WidgetHandle root =
            document.create_widget(canvas("RootCanvas")).widget;
        const WidgetHandle inspector =
            document.create_widget(panel("Inspector"), root).widget;
        const WidgetHandle scroll = document.create_widget(
            scroll_area("InspectorScroll"), inspector).widget;
        const WidgetHandle save = document.create_widget(
            button("SaveButton", "Save"), inspector).widget;
        const WidgetHandle caption = document.create_widget(
            text("Caption", "Temporal GUI"), inspector).widget;
        if (!root || !inspector || !scroll || !save || !caption
            || !document.reorder_widget(caption, 0u)
            || document.widget(inspector)->children.front() != caption
            || !document.reparent_widget(caption, scroll)
            || document.widget(caption)->parent != scroll)
        {
            return ContractFailure::hierarchy_rejection;
        }

        if (document.create_widget(text("Child", "No"), save).code
                != ResultCode::parent_rejects_child
            || document.create_widget(
                    panel("MalformedParent"), WidgetHandle{0u, 0u}).code
                != ResultCode::invalid_handle
            || document.create_widget(
                    tab_page("LoosePage", "Loose")).code
                != ResultCode::parent_rejects_child)
        {
            return ContractFailure::hierarchy_rejection;
        }

        if (document.reparent_widget(inspector, inspector).code
                != ResultCode::cycle_detected
            || document.reparent_widget(inspector, scroll).code
                != ResultCode::cycle_detected
            || document.widget(inspector)->parent != root)
        {
            return ContractFailure::cycle_rejection;
        }

        const WidgetHandle temporary = document.create_widget(
            text("Temporary", "Discard"), inspector).widget;
        if (!temporary || !document.remove_widget(temporary))
            return ContractFailure::stale_handles;
        const WidgetHandle replacement = document.create_widget(
            text("Replacement", "Current"), inspector).widget;
        LayoutDescriptor staleLayout{};
        if (!replacement
            || replacement.index != temporary.index
            || replacement.generation == temporary.generation
            || document.update_layout(temporary, staleLayout).code
                != ResultCode::stale_handle)
        {
            return ContractFailure::stale_handles;
        }

        LayoutDescriptor layout = document.widget(save)->descriptor.layout;
        layout.width = 144.0f;
        layout.height = 36.0f;
        StyleDescriptor style = document.widget(save)->descriptor.style;
        style.accent = Color{0.82f, 0.26f, 0.18f, 1.0f};
        style.style_class = "primary-action";
        InteractionDescriptor interaction =
            document.widget(save)->descriptor.interaction;
        interaction.action = "document.save";
        interaction.tooltip = "Save document";
        interaction.tab_index = 2;
        if (!document.update_layout(save, layout)
            || !document.update_style(save, style)
            || !document.update_content(
                save,
                WidgetContent{ButtonContent{"Save Now"}})
            || !document.update_interaction(save, interaction))
        {
            return ContractFailure::descriptor_edits;
        }
        const std::optional<WidgetView> edited = document.widget(save);
        if (!edited
            || edited->descriptor.layout != layout
            || edited->descriptor.style != style
            || edited->descriptor.interaction != interaction
            || std::get<ButtonContent>(edited->descriptor.content).label
                != "Save Now")
        {
            return ContractFailure::descriptor_edits;
        }

        const WidgetHandle tabs =
            document.create_widget(tab_set("InspectorTabs"), root).widget;
        if (!tabs
            || document.create_widget(panel("RejectedTabChild"), tabs).code
                != ResultCode::parent_rejects_child)
        {
            return ContractFailure::hierarchy_rejection;
        }
        const WidgetHandle properties = document.create_widget(
            tab_page("PropertiesPage", "Properties"), tabs).widget;
        WidgetDescriptor imageButton = widget_descriptor(
            WidgetKind::image_button,
            "PickImage",
            ImageButtonContent{
                "Assets/Textures/checker.png",
                "Choose image",
                true});
        imageButton.interaction.action = "image.choose";
        imageButton.interaction.tooltip = "Choose an image";
        const WidgetHandle picker = document.create_widget(
            std::move(imageButton), properties).widget;
        if (!properties || !picker
            || !document.update_content(
                tabs,
                WidgetContent{TabSetContent{properties}}))
        {
            return ContractFailure::tab_image_button_content;
        }
        const std::optional<WidgetView> tabsView = document.widget(tabs);
        const std::optional<WidgetView> pickerView = document.widget(picker);
        if (!tabsView || !pickerView
            || std::get<TabSetContent>(tabsView->descriptor.content)
                    .selected_page != properties
            || std::get<ImageButtonContent>(pickerView->descriptor.content)
                    .asset_path != "Assets/Textures/checker.png"
            || std::get<ImageButtonContent>(pickerView->descriptor.content)
                    .label != "Choose image")
        {
            return ContractFailure::tab_image_button_content;
        }

        const authoring::ContentHash beforeUndo =
            document.revision().content;
        LayoutDescriptor moved = document.widget(picker)->descriptor.layout;
        moved.x = 24.0f;
        moved.y = 12.0f;
        const MutationResult editResult = document.update_layout(picker, moved);
        const authoring::ContentHash afterEdit = document.revision().content;
        const std::uint64_t sequenceAfterEdit = document.revision().sequence;
        const MutationResult undoResult = document.undo();
        if (!editResult || !undoResult
            || undoResult.operation != editResult.operation
            || document.revision().content != beforeUndo
            || document.revision().sequence <= sequenceAfterEdit
            || document.widget(picker)->descriptor.layout == moved
            || !document.can_redo())
        {
            return ContractFailure::undo_redo;
        }
        const MutationResult redoResult = document.redo();
        if (!redoResult
            || redoResult.operation != editResult.operation
            || document.revision().content != afterEdit
            || document.widget(picker)->descriptor.layout != moved
            || document.can_redo())
        {
            return ContractFailure::undo_redo;
        }

        DocumentLimits widgetBound{};
        widgetBound.maximum_active_widgets = 1u;
        widgetBound.maximum_widget_slots = 1u;
        widgetBound.maximum_children_per_widget = 1u;
        widgetBound.maximum_hierarchy_depth = 1u;
        GuiDocument boundedWidgets = make_document(widgetBound);
        const WidgetHandle only =
            boundedWidgets.create_widget(canvas("Only")).widget;
        if (!only
            || boundedWidgets.create_widget(panel("Overflow")).code
                != ResultCode::widget_limit_exceeded)
        {
            return ContractFailure::bounded_state;
        }

        DocumentLimits childBound{};
        childBound.maximum_active_widgets = 4u;
        childBound.maximum_widget_slots = 4u;
        childBound.maximum_children_per_widget = 1u;
        childBound.maximum_hierarchy_depth = 3u;
        GuiDocument boundedChildren = make_document(childBound);
        const WidgetHandle boundedRoot =
            boundedChildren.create_widget(canvas("Root")).widget;
        if (!boundedRoot
            || !boundedChildren.create_widget(
                panel("First"), boundedRoot)
            || boundedChildren.create_widget(
                    panel("Second"), boundedRoot).code
                != ResultCode::child_limit_exceeded)
        {
            return ContractFailure::bounded_state;
        }

        DocumentLimits depthBound{};
        depthBound.maximum_active_widgets = 3u;
        depthBound.maximum_widget_slots = 3u;
        depthBound.maximum_children_per_widget = 2u;
        depthBound.maximum_hierarchy_depth = 2u;
        GuiDocument boundedDepth = make_document(depthBound);
        const WidgetHandle depthRoot =
            boundedDepth.create_widget(canvas("Root")).widget;
        const WidgetHandle depthPanel = boundedDepth.create_widget(
            panel("Panel"), depthRoot).widget;
        if (!depthRoot || !depthPanel
            || boundedDepth.create_widget(
                    scroll_area("TooDeep"), depthPanel).code
                != ResultCode::hierarchy_depth_exceeded)
        {
            return ContractFailure::bounded_state;
        }

        DocumentLimits historyBound{};
        historyBound.maximum_history_operations = 1u;
        GuiDocument boundedHistory = make_document(historyBound);
        const WidgetHandle historyRoot =
            boundedHistory.create_widget(canvas("HistoryRoot")).widget;
        const GuiDocumentSnapshot beforeRejectedHistory =
            boundedHistory.snapshot();
        LayoutDescriptor rejectedLayout =
            boundedHistory.widget(historyRoot)->descriptor.layout;
        rejectedLayout.width = 220.0f;
        if (!historyRoot
            || boundedHistory.update_layout(historyRoot, rejectedLayout).code
                != ResultCode::history_operation_limit_exceeded
            || boundedHistory.snapshot() != beforeRejectedHistory)
        {
            return ContractFailure::bounded_state;
        }

        DocumentLimits memoryBound{};
        memoryBound.maximum_history_bytes = 1u;
        GuiDocument boundedMemory = make_document(memoryBound);
        if (boundedMemory.create_widget(canvas("MemoryBound")).code
                != ResultCode::history_memory_limit_exceeded
            || !boundedMemory.widgets().empty())
        {
            return ContractFailure::bounded_state;
        }

        const GuiDocument deterministicA = deterministic_document();
        const GuiDocument deterministicB = deterministic_document();
        if (!deterministicA.valid()
            || !deterministicB.valid()
            || deterministicA.validate()
            || deterministicB.validate()
            || deterministicA.snapshot() != deterministicB.snapshot()
            || deterministicA.revision() != deterministicB.revision())
        {
            return ContractFailure::deterministic_snapshot;
        }

        const GuiDocumentSnapshot deterministicSnapshot =
            deterministicA.snapshot();
        const SerializedGuiDocument encoded =
            serialize_gui_document(deterministicSnapshot);
        if (!encoded)
            return ContractFailure::snapshot_codec;

        const DeserializedGuiDocument decoded =
            deserialize_gui_document(encoded.bytes);
        if (!decoded || *decoded.snapshot != deterministicSnapshot)
            return ContractFailure::snapshot_codec;

        GuiDocument restored{*decoded.snapshot};
        if (!restored.valid()
            || restored.validate()
            || restored.snapshot() != deterministicSnapshot
            || restored.revision() != deterministicA.revision())
        {
            return ContractFailure::snapshot_codec;
        }

        std::vector<std::byte> corrupted = encoded.bytes;
        corrupted[corrupted.size() / 2u] ^= std::byte{0x01u};
        if (deserialize_gui_document(corrupted).code
                != SnapshotCodecCode::integrity_failure
            || serialize_gui_document(deterministicSnapshot, 32u).code
                != SnapshotCodecCode::invalid_snapshot)
        {
            return ContractFailure::snapshot_codec;
        }

        const std::vector<OperationRecord> journal = document.journal();
        if (journal.empty()
            || !contains_kind(journal, OperationKind::widget_created)
            || !contains_kind(journal, OperationKind::widget_removed)
            || !contains_kind(journal, OperationKind::widget_reparented)
            || !contains_kind(journal, OperationKind::widget_reordered)
            || !contains_kind(journal, OperationKind::layout_changed)
            || !contains_kind(journal, OperationKind::style_changed)
            || !contains_kind(journal, OperationKind::content_changed)
            || !contains_kind(journal, OperationKind::interaction_changed))
        {
            return ContractFailure::operation_journal;
        }
        for (std::size_t index = 0u; index < journal.size(); ++index)
        {
            if (!journal[index].id
                || !journal[index].applied
                || journal[index].retained_bytes == 0u
                || journal[index].before_content
                    == journal[index].after_content
                || (index != 0u
                    && journal[index - 1u].id.value
                        >= journal[index].id.value))
            {
                return ContractFailure::operation_journal;
            }
        }
        const DocumentMetrics metrics = document.metrics();
        if (metrics.active_widgets != document.widgets().size()
            || metrics.retained_operations != journal.size()
            || metrics.applied_operations != journal.size()
            || metrics.retained_history_bytes == 0u
            || document.validate())
        {
            return ContractFailure::operation_journal;
        }

        for (TemplatePreset preset : {
                 TemplatePreset::blank_canvas,
                 TemplatePreset::desktop_app,
                 TemplatePreset::dashboard,
                 TemplatePreset::mobile_app,
                 TemplatePreset::game_hud})
        {
            const auto created = make_template_document(preset);
            if (!created || !created->valid() || created->validate()
                || created->roots().size() != 1u
                || created->widgets().empty())
            {
                return ContractFailure::template_factory;
            }
        }
        const auto starter =
            make_template_document(TemplatePreset::game_hud);
        if (!starter || starter->widgets().size() <= 1u)
            return ContractFailure::starter_roundtrip;
        const SerializedGuiDocument encodedStarter =
            serialize_gui_document(starter->snapshot());
        const DeserializedGuiDocument decodedStarter =
            encodedStarter
                ? deserialize_gui_document(encodedStarter.bytes)
                : DeserializedGuiDocument{};
        if (!decodedStarter)
            return ContractFailure::starter_roundtrip;

        GuiDocument reopenedStarter{*decodedStarter.snapshot};
        const auto starterRoots = reopenedStarter.roots();
        const auto starterWidgets = reopenedStarter.widgets();
        if (!reopenedStarter.valid() || reopenedStarter.validate()
            || starterRoots.size() != 1u
            || starterWidgets.size() != starter->widgets().size())
        {
            return ContractFailure::starter_roundtrip;
        }
        const WidgetHandle starterCanvas = starterRoots.front();
        const auto selected = std::find_if(
            starterWidgets.begin(),
            starterWidgets.end(),
            [](const WidgetView& widget) noexcept
            {
                return widget.descriptor.name == "Pause";
            });
        if (selected == starterWidgets.end()
            || selected->parent != starterCanvas
            || selected->descriptor.kind != WidgetKind::button)
        {
            return ContractFailure::starter_roundtrip;
        }
        for (const WidgetView& widget : starterWidgets)
        {
            if (widget.handle != starterCanvas
                && widget.parent != starterCanvas)
            {
                return ContractFailure::starter_roundtrip;
            }
        }
        const auto reopenedSelection =
            reopenedStarter.widget(selected->handle);
        const auto* pauseContent = reopenedSelection
            ? std::get_if<ButtonContent>(
                &reopenedSelection->descriptor.content)
            : nullptr;
        if (!reopenedSelection || !pauseContent
            || pauseContent->label != "Pause"
            || reopenedSelection->descriptor.interaction.action != "pause")
        {
            return ContractFailure::starter_roundtrip;
        }

        GuiDocument legacyGenerated{
            DocumentHandle{0u, 1u},
            BranchIdentity{1u, 1u}};
        WidgetDescriptor legacyCanvas{};
        legacyCanvas.kind = WidgetKind::canvas;
        legacyCanvas.name = "MainCanvas";
        legacyCanvas.layout.width = 1'280.0f;
        legacyCanvas.layout.height = 720.0f;
        legacyCanvas.content = CanvasContent{1'280.0f, 720.0f, true};
        if (!legacyGenerated.create_widget(std::move(legacyCanvas)))
            return ContractFailure::starter_migration;

        const GuiDocumentSnapshot legacySnapshot =
            legacyGenerated.snapshot();
        const auto migrated = migrate_legacy_generated_root_only_document(
            legacySnapshot,
            TemplatePreset::game_hud);
        if (legacySnapshot.revision.sequence != 2u
            || legacySnapshot.slots.size() != 1u
            || legacySnapshot.roots.size() != 1u
            || !is_legacy_generated_root_only_document(legacySnapshot)
            || !migrated
            || !migrated->valid()
            || migrated->validate()
            || migrated->widgets().size() <= 1u
            || is_legacy_generated_root_only_document(migrated->snapshot()))
        {
            return ContractFailure::starter_migration;
        }

        const auto authoredBlank =
            make_template_document(TemplatePreset::blank_canvas);
        if (!authoredBlank
            || !authoredBlank->valid()
            || authoredBlank->validate()
            || is_legacy_generated_root_only_document(
                authoredBlank->snapshot())
            || migrate_legacy_generated_root_only_document(
                authoredBlank->snapshot(),
                TemplatePreset::game_hud))
        {
            return ContractFailure::starter_migration;
        }
        if (widget_kind_name(WidgetKind::image_button) != "image_button"
            || result_code_name(ResultCode::cycle_detected)
                != "cycle_detected"
            || operation_kind_name(OperationKind::content_changed)
                != "content_changed")
        {
            return ContractFailure::result_names;
        }
        return ContractFailure::none;
    }
}

#if defined(EPOCH_AUTHORING_GUI_DOCUMENT_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::authoring::gui::run_contract());
}
#endif
