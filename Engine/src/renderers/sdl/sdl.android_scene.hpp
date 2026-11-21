// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

namespace epochengine::sdlcontext::android_scene
{
    struct Surface
    {
        SDL_Renderer* renderer;
        SDL_Window* window;
        int width, height;
        int framebufferWidth, framebufferHeight;
    };

    [[nodiscard]] bool draw(const Surface& surface, const core::RenderViewport& viewport,
        const previewgrid::Mat4& mvp, const previewgrid::PublishedSceneFrame& scene, bool densityClouds);
    void release() noexcept;
    void pause_presentation() noexcept;
    void observe_present();
}
