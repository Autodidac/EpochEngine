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
#pragma once

#include <memory>

namespace epochengine::core
{
    class Context;

    namespace detail
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        void register_opengl_backend();
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        void register_sfml_backend();
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        extern "C" void epoch_register_raylib_backend();
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        void register_sdl_backend();
#endif
    }
}

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
namespace epochengine::openglbackend
{
    void configure(const std::shared_ptr<epochengine::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
namespace epochengine::sfmlbackend
{
    void configure(const std::shared_ptr<epochengine::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
namespace epochengine::raylibbackend
{
    void configure(const std::shared_ptr<epochengine::core::Context>& ctx);
}
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
namespace epochengine::sdlbackend
{
    void configure(const std::shared_ptr<epochengine::core::Context>& ctx);
}
#endif
