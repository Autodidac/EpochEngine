/**************************************************************
 *   epochengine - Modular C++ Framework
 *   Editor API
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
export module aeditor;

import aengine.core.context;
import aengine.gui;
import <memory>;

export namespace epochnamespace
{
    // Returns true when the user clicks "Run Game" in the editor UI.
    bool editor_run(const std::shared_ptr<core::Context>& ctx,
        gui::WidgetBounds* out_bounds = nullptr);

    void cleanup_chat_context(const core::Context* ctx);
    void shutdown_chat_system();
}
