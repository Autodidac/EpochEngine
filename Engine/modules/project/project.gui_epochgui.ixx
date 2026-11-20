/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <functional>
#include <map>
#include <span>
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

    enum class PointerDelivery { frame_snapshot, ordered };
    enum class TextScaling { canvas, desktop };

    class EpochGuiAdapter final
    {
    public:
        EpochGuiAdapter(std::string projectId, std::string projectRoot) noexcept;
        ~EpochGuiAdapter() noexcept;

        [[nodiscard]] bool valid() const noexcept;
        void prepare_images(std::span<const asset::gui::CompiledWidget> widgets);
        [[nodiscard]] EpochGuiRenderResult render(
            project_gui_runtime::RuntimeSession& runtime,
            project_gui_runtime::Float2 viewport,
            const std::function<bool(const gui::InputEvent&)>& command = {},
            PointerDelivery pointer_delivery = PointerDelivery::frame_snapshot,
            TextScaling text_scaling = TextScaling::canvas,
            project_gui_runtime::Float2 origin = {}) noexcept;
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
        [[nodiscard]] project_gui_runtime::RuntimeCode dispatch_keyboard(
            project_gui_runtime::RuntimeSession& runtime,
            const project_gui_runtime::RuntimeFrame& frame,
            const std::function<bool(const gui::InputEvent&)>& command);
        void render_tabs(
            project_gui_runtime::RuntimeSession& runtime,
            const project_gui_runtime::RuntimeFrame& frame);
        void render_widget(
            project_gui_runtime::RuntimeSession& runtime,
            const project_gui_runtime::WidgetView& view);

        std::string project_id_{};
        std::string project_root_{};
        gui::DragSurfaceState tool_pointer_{};
        std::map<std::uint64_t, std::string> text_state_{};
        std::map<std::string, ImageState, std::less<>> images_{};
    };
}
