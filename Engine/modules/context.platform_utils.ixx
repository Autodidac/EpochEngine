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

#include <algorithm>
#include <memory>

#if defined(_WIN32)
#include <wtypes.h>
#endif

export module context.platform_utils;

#if defined(_WIN32)
#endif

import core.context;
import engine.input;

namespace
{
#if defined(_WIN32)
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<epochnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx) return;

        HWND hwnd = ctx->get_hwnd();
        if (!hwnd) return;

        if (epochnamespace::input::are_mouse_coords_global())
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
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<epochnamespace::core::Context>& ctx, int& x, int& y) noexcept
    {
        if (!ctx) return;

        if (epochnamespace::input::are_mouse_coords_global())
            return;

        const int width = (std::max)(1, ctx->width);
        const int height = (std::max)(1, ctx->height);

        if (x < 0 || y < 0 || x >= width || y >= height) {
            x = -1;
            y = -1;
        }
    }
#else
    void ClampMouseToClientRectIfNeeded(const std::shared_ptr<epochnamespace::core::Context>&, int&, int&) noexcept {}
#endif
}
