/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

export module project.gui_epochgui;

import asset.gui_artifact;
import gui.engine;
import sprite.handle;
import project.gui_runtime;

export namespace epochengine::project_gui
{
    struct EpochGuiRenderResult final
    {
        project_gui_runtime::RuntimeCode code{
            project_gui_runtime::RuntimeCode::invalid_artifact};
        bool pointer_captured{};
        std::uint32_t visible_widgets{};
        std::uint32_t resolved_images{};
        std::vector<project_gui_runtime::RuntimeEvent> events{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == project_gui_runtime::RuntimeCode::ready;
        }
    };

    class EpochGuiAdapter final
    {
    public:
        EpochGuiAdapter(std::string projectId, std::string projectRoot) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] EpochGuiRenderResult render(
            project_gui_runtime::RuntimeSession& runtime,
            project_gui_runtime::Float2 viewport) noexcept;
        void clear() noexcept;

    private:
        struct ImageState final
        {
            SpriteHandle sprite{};
            std::vector<std::uint8_t> rgba{};
            std::uint32_t width{};
            std::uint32_t height{};
            bool load_attempted{};
        };

        [[nodiscard]] ImageState& resolve_image(std::string_view logicalPath);
        void render_tabs(
            project_gui_runtime::RuntimeSession& runtime,
            const project_gui_runtime::RuntimeFrame& frame);
        void render_widget(
            project_gui_runtime::RuntimeSession& runtime,
            const project_gui_runtime::WidgetView& view);

        std::string project_id_{};
        std::string project_root_{};
        std::map<std::uint64_t, std::string> text_state_{};
        std::map<std::string, ImageState, std::less<>> images_{};
    };
}
