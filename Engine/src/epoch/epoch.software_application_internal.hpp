// SPDX-License-Identifier: LicenseRef-MIT-NoSell
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
        std::unique_ptr<platform::IWindowSystem>> (*)() noexcept;

    [[nodiscard]] RunReport run_with_window_factory(
        const LaunchOptions& options,
        const app_callbacks_v1& callbacks,
        WindowSystemFactory factory,
        bool native_window_supported) noexcept;
}
