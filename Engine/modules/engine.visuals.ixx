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

#include <array>
#include <string_view>

export module engine.visuals;

namespace epochengine::visuals
{
    export struct Rgb
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };

    export using Rgba = std::array<float, 4>;

    export [[nodiscard]] constexpr std::string_view active_profile_name() noexcept
    {
        return "Epoch Dark Visuals";
    }

    export [[nodiscard]] constexpr std::string_view parity_gate() noexcept
    {
        return "Frame clears, scene clears, object role colors, selection colors, markers, and editor opacity come from engine.visuals.";
    }

    export [[nodiscard]] constexpr Rgba frame_background() noexcept
    {
        return { 0.06f, 0.08f, 0.11f, 1.0f };
    }

    export [[nodiscard]] constexpr Rgba scene_background() noexcept
    {
        return { 0.31f, 0.36f, 0.42f, 1.0f };
    }

    export [[nodiscard]] constexpr Rgb object_default() noexcept
    {
        return { 0.95f, 0.62f, 0.28f };
    }

    export [[nodiscard]] constexpr Rgb object_light() noexcept
    {
        return { 1.0f, 0.82f, 0.25f };
    }

    export [[nodiscard]] constexpr Rgb object_spawn() noexcept
    {
        return { 0.28f, 0.94f, 0.48f };
    }

    export [[nodiscard]] constexpr Rgb object_camera() noexcept
    {
        return { 0.42f, 0.80f, 1.0f };
    }

    export [[nodiscard]] constexpr Rgb object_world() noexcept
    {
        return { 0.62f, 0.78f, 0.98f };
    }

    export [[nodiscard]] constexpr Rgb object_editor_helper() noexcept
    {
        return { 0.72f, 0.72f, 0.78f };
    }

    export [[nodiscard]] constexpr Rgb object_selected() noexcept
    {
        return { 1.0f, 0.93f, 0.32f };
    }

    export [[nodiscard]] constexpr Rgb object_selected_outline() noexcept
    {
        return { 1.0f, 0.95f, 0.22f };
    }

    export [[nodiscard]] constexpr Rgb look_marker() noexcept
    {
        return { 0.99f, 0.89f, 0.34f };
    }

    export [[nodiscard]] constexpr Rgb forest_trunk() noexcept
    {
        return { 0.58f, 0.36f, 0.20f };
    }

    export [[nodiscard]] constexpr Rgb forest_branch_joint() noexcept
    {
        return { 0.42f, 0.70f, 0.32f };
    }

    export [[nodiscard]] constexpr Rgb forest_foliage_cluster() noexcept
    {
        return { 0.22f, 0.86f, 0.38f };
    }

    export [[nodiscard]] constexpr Rgb forest_default() noexcept
    {
        return { 0.30f, 0.82f, 0.36f };
    }

    export [[nodiscard]] constexpr float editor_wire_opacity_factor() noexcept
    {
        return 0.68f;
    }

    export [[nodiscard]] constexpr float editor_solid_opacity_factor() noexcept
    {
        return 0.62f;
    }
}
