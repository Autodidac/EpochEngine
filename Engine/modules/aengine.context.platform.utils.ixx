module;

export module aengine.context.platform.utils;

import <wtypes.h>;
import <memory>;

import aengine.core.context;
import aengine.input;

namespace
{
#if defined(_WIN32)
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx) return;

        HWND hwnd = ctx->get_hwnd();
        if (!hwnd) return;

        const auto contextId = reinterpret_cast<almondnamespace::input::MouseCoordsContextId>(ctx.get());
        if (almondnamespace::input::are_mouse_coords_global_for_context(contextId))
            return;

        RECT rc{};
        if (!::GetClientRect(hwnd, &rc))
            return;

        const int width = static_cast<int>((std::max)(static_cast<LONG>(1), rc.right - rc.left));
        const int height = static_cast<int>((std::max)(static_cast<LONG>(1), rc.bottom - rc.top));

        if (x < 0 || y < 0 || x >= width || y >= height) {
            x = -1;
            y = -1;
        }
    }
#elif defined(__linux__)
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx) return;

        const auto contextId = reinterpret_cast<almondnamespace::input::MouseCoordsContextId>(ctx.get());
        if (almondnamespace::input::are_mouse_coords_global_for_context(contextId))
            return;

        const int width = (std::max)(1, ctx->width);
        const int height = (std::max)(1, ctx->height);

        if (x < 0 || y < 0 || x >= width || y >= height) {
            x = -1;
            y = -1;
        }
    }
#else
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<almondnamespace::core::Context>&, int&, int&) noexcept {}
#endif
}