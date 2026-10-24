/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include "../include/core.stl_types.hpp"

module platform.context;

import core.error;

namespace epochengine::platform
{
    namespace
    {
        class NullGraphicsContext final : public IGraphicsContext
        {
        public:
            explicit NullGraphicsContext(GraphicsContextDesc desc) noexcept
                : desc_(std::move(desc))
            {
            }

            [[nodiscard]] core::error::result<void> create_surface(window_adapter::WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return epochengine::unexpected(core::error::invalid_argument("invalid window handle for surface creation"));

                has_surface_ = true;
                return {}; // ok
            }

            void resize_surface(window_adapter::WindowHandle handle, std::int32_t, std::int32_t) noexcept override
            {
                if (!handle.valid())
                    return;
            }

            void teardown() noexcept override
            {
                has_surface_ = false;
            }

            [[nodiscard]] GraphicsBackend backend() const noexcept override
            {
                return desc_.backend;
            }

        private:
            GraphicsContextDesc desc_{};
            bool has_surface_ = false;
        };
    }

    core::error::result<std::unique_ptr<IGraphicsContext>> create_graphics_context(const GraphicsContextDesc& desc) noexcept
    {
        switch (desc.backend)
        {
        case GraphicsBackend::null_backend:
            return std::make_unique<NullGraphicsContext>(desc); // ok (value converts)
        default:
            return epochengine::unexpected(
                core::error::make(
                    { core::error::core_domain::id, core::error::core_domain::unsupported },
                    "graphics backend not available"
                )
            );
        }
    }

} // namespace epochengine::platform
