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
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

//#include "aplatform.hpp"
//#include "aengineconfig.hpp"
//#include "aatlastexture.hpp"
//#include "asoftrenderer_state.hpp"
//#include "ainput.hpp"

#include <include/engine.config.hpp> // for EPOCH_USING Macros 		// for EPOCH_USING_SDL
export module software.textures;

import atlas.texture;        // TextureAtlas
import software.state;   // SoftRendState
import engine.platform;    // epochengine
import engine.input;       // epochengine::input
//import engine.config; // epochengine::input

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

export namespace epochengine::anativecontext
{
    // â”€â”€â”€ Texture container for software backend â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    struct Texture
    {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels; // RGBA8

        Texture() = default;
        Texture(int w, int h, uint32_t fill = 0xFFFFFFFF)
            : width(w), height(h), pixels(w * h, fill) {
        }

        uint32_t sample(int x, int y) const
        {
            x = std::clamp(x, 0, width - 1);
            y = std::clamp(y, 0, height - 1);
            return pixels[static_cast<size_t>(y) * width + x];
        }
    };

    using TexturePtr = std::shared_ptr<Texture>;

    inline TexturePtr create_texture(int w, int h, uint32_t fill = 0xFFFFFFFF)
    {
        return std::make_shared<Texture>(w, h, fill);
    }

    // â”€â”€â”€ BackendData for Software Renderer â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    struct BackendData
    {
        // Map atlas â†’ texture for caching
        std::unordered_map<const TextureAtlas*, TexturePtr> textures;

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

        // Renderer state (framebuffer, dimensions, etc.)
        epochengine::anativecontext::SoftRendState srState;
#endif
    };
} // namespace epochengine::anativecontext
#endif
