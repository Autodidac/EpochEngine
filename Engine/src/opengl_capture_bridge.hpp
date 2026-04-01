#pragma once

#include <cstdint>

namespace epochnamespace::openglbridge
{
    void capture_frame_if_requested(int framebufferWidth, int framebufferHeight, std::uintptr_t windowId);
}
