module;

#include <memory>

#include <include/engine.config.hpp>

module core.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.backend;

namespace epochengine::core::detail
{
    void register_opengl_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::OpenGL;
        ctx->backendName = "OpenGL";
        epochengine::openglbackend::configure(ctx);
        AddContextForBackend(ContextType::OpenGL, std::move(ctx));
    }
}
#endif
