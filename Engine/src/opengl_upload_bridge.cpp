#include "opengl_upload_bridge.hpp"

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import atlas.manager;
import context.type;

namespace epochnamespace::openglbridge
{
    void process_pending_uploads()
    {
        atlasmanager::process_pending_uploads(core::ContextType::OpenGL);
    }
}
#endif
