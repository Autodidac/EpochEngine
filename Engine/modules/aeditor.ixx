/**************************************************************
 *   AlmondShell - Modular C++ Framework
 *   Editor API
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
export module aeditor;

import aengine.core.context;
import aengine.gui;
import <memory>;

export namespace almondnamespace
{
    // Returns true when the user clicks "Run Game" in the editor UI.
    bool editor_run(const std::shared_ptr<core::Context>& ctx,
        gui::WidgetBounds* out_bounds = nullptr);
}
