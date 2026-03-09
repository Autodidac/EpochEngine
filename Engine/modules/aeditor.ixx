export module aeditor;

import aengine.core.context;
import aengine.gui;
import <memory>;
import <string>;

export namespace epochnamespace
{
    export enum class EditorCommand : unsigned char
    {
        None = 0,
        OpenProject,
        Settings,
        RunGame,
        Exit
    };

    export struct EditorFrameResult
    {
        gui::WidgetBounds scene_viewport{};
        EditorCommand command{ EditorCommand::None };
        std::string command_argument{};
    };

    EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx);

    void cleanup_chat_context(const core::Context* ctx);
    void shutdown_chat_system();
}

