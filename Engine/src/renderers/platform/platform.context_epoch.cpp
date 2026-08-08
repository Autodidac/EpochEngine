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
            explicit NullGraphicsContext(ContextDesc desc) noexcept
                : desc_(std::move(desc))
            {
            }

            [[nodiscard]] core::error::result<void> create_surface(WindowHandle handle) noexcept override
            {
                if (!handle.valid())
                    return epochengine::unexpected(core::error::invalid_argument("invalid window handle for surface creation"));

                has_surface_ = true;
                return {}; // ok
            }

            void resize_surface(WindowHandle handle, std::int32_t, std::int32_t) noexcept override
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
            ContextDesc desc_{};
            bool has_surface_ = false;
        };
    }

    core::error::result<std::unique_ptr<IGraphicsContext>> create_graphics_context(const ContextDesc& desc) noexcept
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
