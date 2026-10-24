// SPDX-License-Identifier: MPL-2.0
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
#pragma once

#include "epoch.software_application.hpp"
#include "core.stl_types.hpp"

import core.error;
import platform.window;

namespace epochengine::software::detail
{
    // The production path uses the native platform factory. The contract path
    // supplies an in-memory window system; it cannot attest native rendering.
    using WindowSystemFactory = core::error::result<
        std::unique_ptr<platform::window_adapter::IWindowSystem>> (*)() noexcept;

    [[nodiscard]] RunReport run_with_window_factory(
        const LaunchOptions& options,
        const app_callbacks_v1& callbacks,
        WindowSystemFactory factory,
        bool native_window_supported) noexcept;
}
