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
/**************************************************************
 *   epochengine - Modular C++ Framework
 *   Raylib Input Adapter
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/

module;

#include <atomic>
#include <mutex>
#include <shared_mutex>

#include <include/engine.config.hpp> // for EPOCH_USING_RAYLIB

export module raylib.input;
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

import raylib.api;
import input.engine;

namespace epochengine::raylibcontext
{
    export inline void poll_input()
    {
        using namespace epochengine::input;

        std::unique_lock<std::shared_mutex> lock(g_inputMutex);

        // ---- Clear "pressed this frame" + wheel ----
        // Do NOT rely on .reset() existing (types differ across your backends).
        for (int k = 0; k < Key::Count; ++k)
            keyPressed[k] = false;

        for (int b = 0; b < MouseButton::MouseCount; ++b)
            mousePressed[b] = false;

        mouseWheel.store(0, std::memory_order_relaxed);

        // ---- Keyboard ----
        for (int k = 0; k < Key::Count; ++k)
        {
            int ray = 0;
            switch (static_cast<Key>(k))
            {
            case Key::A: ray = epochengine::raylib_api::key_a; break;
            case Key::B: ray = epochengine::raylib_api::key_b; break;
            case Key::C: ray = epochengine::raylib_api::key_c; break;
            case Key::D: ray = epochengine::raylib_api::key_d; break;
            case Key::E: ray = epochengine::raylib_api::key_e; break;
            case Key::F: ray = epochengine::raylib_api::key_f; break;
            case Key::G: ray = epochengine::raylib_api::key_g; break;
            case Key::H: ray = epochengine::raylib_api::key_h; break;
            case Key::I: ray = epochengine::raylib_api::key_i; break;
            case Key::J: ray = epochengine::raylib_api::key_j; break;
            case Key::K: ray = epochengine::raylib_api::key_k; break;
            case Key::L: ray = epochengine::raylib_api::key_l; break;
            case Key::M: ray = epochengine::raylib_api::key_m; break;
            case Key::N: ray = epochengine::raylib_api::key_n; break;
            case Key::O: ray = epochengine::raylib_api::key_o; break;
            case Key::P: ray = epochengine::raylib_api::key_p; break;
            case Key::Q: ray = epochengine::raylib_api::key_q; break;
            case Key::R: ray = epochengine::raylib_api::key_r; break;
            case Key::S: ray = epochengine::raylib_api::key_s; break;
            case Key::T: ray = epochengine::raylib_api::key_t; break;
            case Key::U: ray = epochengine::raylib_api::key_u; break;
            case Key::V: ray = epochengine::raylib_api::key_v; break;
            case Key::W: ray = epochengine::raylib_api::key_w; break;
            case Key::X: ray = epochengine::raylib_api::key_x; break;
            case Key::Y: ray = epochengine::raylib_api::key_y; break;
            case Key::Z: ray = epochengine::raylib_api::key_z; break;

            case Key::Num0: ray = epochengine::raylib_api::key_zero; break;
            case Key::Num1: ray = epochengine::raylib_api::key_one; break;
            case Key::Num2: ray = epochengine::raylib_api::key_two; break;
            case Key::Num3: ray = epochengine::raylib_api::key_three; break;
            case Key::Num4: ray = epochengine::raylib_api::key_four; break;
            case Key::Num5: ray = epochengine::raylib_api::key_five; break;
            case Key::Num6: ray = epochengine::raylib_api::key_six; break;
            case Key::Num7: ray = epochengine::raylib_api::key_seven; break;
            case Key::Num8: ray = epochengine::raylib_api::key_eight; break;
            case Key::Num9: ray = epochengine::raylib_api::key_nine; break;

            case Key::Space:     ray = epochengine::raylib_api::key_space; break;
            case Key::Enter:     ray = epochengine::raylib_api::key_enter; break;
            case Key::Escape:    ray = epochengine::raylib_api::key_escape; break;
            case Key::Tab:       ray = epochengine::raylib_api::key_tab; break;
            case Key::Backspace: ray = epochengine::raylib_api::key_backspace; break;

            case Key::Left:  ray = epochengine::raylib_api::key_left; break;
            case Key::Right: ray = epochengine::raylib_api::key_right; break;
            case Key::Up:    ray = epochengine::raylib_api::key_up; break;
            case Key::Down:  ray = epochengine::raylib_api::key_down; break;

            default: ray = 0; break;
            }

            if (ray == 0) continue;

            const bool down = epochengine::raylib_api::is_key_down(ray);

            // "pressed" = down this frame, was up last frame
            keyPressed[k] = down && !static_cast<bool>(keyDown[k]);
            keyDown[k] = down;
        }

        // ---- Mouse ----
        for (int b = 0; b < MouseButton::MouseCount; ++b)
        {
            int ray = 0;
            switch (static_cast<MouseButton>(b))
            {
            case MouseButton::MouseLeft:    ray = epochengine::raylib_api::mouse_button_left; break;
            case MouseButton::MouseRight:   ray = epochengine::raylib_api::mouse_button_right; break;
            case MouseButton::MouseMiddle:  ray = epochengine::raylib_api::mouse_button_middle; break;
            case MouseButton::MouseButton4: ray = epochengine::raylib_api::mouse_button_side; break;
            case MouseButton::MouseButton5: ray = epochengine::raylib_api::mouse_button_extra; break;
            default: ray = 0; break;
            }

            if (ray == 0) continue;

            const bool down = epochengine::raylib_api::is_mouse_button_down(ray);
            mousePressed[b] = down && !static_cast<bool>(mouseDown[b]);
            mouseDown[b] = down;
        }

        mouseX.store(epochengine::raylib_api::get_mouse_x(), std::memory_order_relaxed);
        mouseY.store(epochengine::raylib_api::get_mouse_y(), std::memory_order_relaxed);
        set_mouse_coords_are_global(false);

        mouseWheel.store(
            static_cast<int>(epochengine::raylib_api::get_mouse_wheel_move()),
            std::memory_order_relaxed);
    }
}

#endif // EPOCH_USING_RAYLIB
