/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

module authoring.gui_compiler;
import authoring.document;

namespace epochengine::authoring::gui
{
    namespace
    {
        [[nodiscard]] GuiDocument document()
        {
            GuiDocument result{
                DocumentHandle{41u, 3u},
                BranchIdentity{17u, 9u}};

            WidgetDescriptor canvas{};
            canvas.kind = WidgetKind::canvas;
            canvas.name = "ProjectGui";
            canvas.content = CanvasContent{1'024.0f, 576.0f, true};
            const auto root = result.create_widget(std::move(canvas)).widget;

            WidgetDescriptor panel{};
            panel.kind = WidgetKind::panel;
            panel.name = "MainPanel";
            panel.content = PanelContent{"Project"};
            panel.layout.flow = LayoutFlow::vertical;
            panel.layout.width_rule = SizeRule::fill;
            panel.layout.height_rule = SizeRule::fill;
            panel.layout.spacing = 8.0f;
            const auto body = result.create_widget(std::move(panel), root).widget;

            WidgetDescriptor imageButton{};
            imageButton.kind = WidgetKind::image_button;
            imageButton.name = "PlayButton";
            imageButton.content = ImageButtonContent{
                "Assets/Textures/play.epoch_texture",
                "Play",
                true};
            imageButton.interaction.enabled = true;
            imageButton.interaction.visible = true;
            imageButton.interaction.focusable = true;
            imageButton.interaction.accepts_pointer = true;
            imageButton.interaction.tab_index = 0;
            imageButton.interaction.action = "project.play";
            (void)result.create_widget(std::move(imageButton), body);

            WidgetDescriptor tabs{};
            tabs.kind = WidgetKind::tab_set;
            tabs.name = "ModeTabs";
            tabs.content = TabSetContent{};
            const auto tabSet = result.create_widget(std::move(tabs), body).widget;

            WidgetDescriptor page{};
            page.kind = WidgetKind::tab_page;
            page.name = "SettingsPage";
            page.content = TabPageContent{"Settings", true};
            const auto tabPage = result.create_widget(std::move(page), tabSet).widget;
            (void)result.update_content(
                tabSet,
                WidgetContent{TabSetContent{tabPage}});
            return result;
        }

        [[nodiscard]] asset::gui::ContentHash expected_source_hash(
            const authoring::ContentHash& source) noexcept
        {
            asset::gui::ContentHash result{};
            for (std::size_t word = 0u; word < source.words.size(); ++word)
            {
                for (std::size_t byte = 0u; byte < 8u; ++byte)
                {
                    result.bytes[word * 8u + byte] =
                        static_cast<std::uint8_t>(
                            source.words[word] >> (byte * 8u));
                }
            }
            return result;
        }
    }

    CompilerContractFailure run_compiler_contract() noexcept
    {
        const GuiDocument source = document();
        if (!source.valid() || source.validate())
            return CompilerContractFailure::valid_document_rejected;

        const auto first = compile_gui_document(source.snapshot());
        const auto second = compile_gui_document(source.snapshot());
        if (!first || !second)
            return CompilerContractFailure::valid_document_rejected;
        if (first.artifact != second.artifact
            || first.artifact.identity.key != second.artifact.identity.key)
        {
            return CompilerContractFailure::deterministic_compilation;
        }
        if (first.artifact.identity.source_revision.content
                != expected_source_hash(source.revision().content)
            || first.artifact.identity.source_revision.sequence
                != source.revision().sequence)
        {
            return CompilerContractFailure::source_revision_mismatch;
        }
        if (first.artifact.widgets.size() != source.widgets().size()
            || first.diagnostics.emitted_widgets != source.widgets().size()
            || first.diagnostics.referenced_assets != 1u
            || first.artifact.virtual_width != 1'024.0f
            || first.artifact.virtual_height != 576.0f
            || first.artifact.widgets[first.artifact.root].kind
                != asset::gui::WidgetKind::canvas)
        {
            return CompilerContractFailure::content_projection;
        }

        const auto bytes = asset::gui::serialize(first.artifact);
        const auto decoded = bytes
            ? asset::gui::deserialize(bytes.bytes)
            : asset::gui::DeserializationResult{};
        if (!bytes || !decoded || decoded.artifact != first.artifact)
            return CompilerContractFailure::artifact_round_trip;

        auto malformed = source.snapshot();
        malformed.roots.clear();
        if (compile_gui_document(malformed))
            return CompilerContractFailure::malformed_document_accepted;

        return CompilerContractFailure::none;
    }
}

#if defined(EPOCH_AUTHORING_GUI_COMPILER_CONTRACT_MAIN)
int main()
{
    return static_cast<int>(
        epochengine::authoring::gui::run_compiler_contract());
}
#endif
