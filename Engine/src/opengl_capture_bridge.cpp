#include "opengl_capture_bridge.hpp"

#include <include/aengine.config.hpp>

#if defined(_MSC_VER)
namespace epochnamespace::openglbridge
{
    void capture_frame_if_requested(int, int, std::uintptr_t)
    {
    }
}
#elif defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.capture;

namespace epochnamespace::openglbridge
{
    void capture_frame_if_requested(int framebufferWidth, int framebufferHeight, std::uintptr_t windowId)
    {
        openglcapture::capture_frame_if_requested(framebufferWidth, framebufferHeight, windowId);
    }
}
#endif
