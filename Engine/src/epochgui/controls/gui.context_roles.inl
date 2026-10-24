// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] static core::RenderPath render_path_for_context(const core::Context* ctx) noexcept
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

        [[nodiscard]] static bool backend_owns_scene_viewport(const core::Context* ctx) noexcept
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

        [[nodiscard]] static const char* viewport_fallback_name(const core::Context* ctx) noexcept
        {
            if (!ctx)
                return "Scene";

            switch (ctx->type)
            {
            case core::ContextType::RayLib:
                return "RayLib";
            case core::ContextType::SDL:
                return "SDL";
            case core::ContextType::SFML:
                return "SFML";
            case core::ContextType::Software:
                return "Software";
            case core::ContextType::OpenGL:
                return "OpenGL";
            case core::ContextType::Vulkan:
                return "Vulkan";
            case core::ContextType::DirectX:
                return "DirectX";
            default:
                return "Scene";
            }
        }

