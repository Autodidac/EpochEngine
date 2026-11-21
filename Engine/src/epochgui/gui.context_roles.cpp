// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module gui.engine;

import core.context;
import context.type;
import context.commandqueue;

namespace epochengine::gui
{
    [[nodiscard]] core::RenderPath render_path_for_context(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return core::RenderPath::Unknown;

        switch (ctx->type)
        {
        case core::ContextType::OpenGL:
            return core::RenderPath::OpenGL;
        case core::ContextType::SFML:
            return core::RenderPath::SFML;
        case core::ContextType::Vulkan:
            return core::RenderPath::Vulkan;
        case core::ContextType::DirectX:
            return core::RenderPath::DirectX;
        default:
            return core::RenderPath::Unknown;
        }
    }

    [[nodiscard]] bool backend_owns_scene_viewport(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return false;

        switch (ctx->type)
        {
        case core::ContextType::OpenGL:
        case core::ContextType::Vulkan:
        case core::ContextType::RayLib:
        case core::ContextType::SFML:
        case core::ContextType::SDL:
        case core::ContextType::DirectX:
        case core::ContextType::Software:
            return true;
        default:
            return false;
        }
    }
}
