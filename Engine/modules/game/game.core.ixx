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

#include <vector>
#include <utility>
#include <cassert>

export module game.core;

 // agamecore.hpp
//
//#include "acontext.hpp"   // Context & draw_sprite()
//
//#include <vector>
//#include <utility>
//#include <cassert>
//
export namespace epochengine
{
    namespace gamecore
    {
        template <typename T>
        using grid_t = std::vector<T>;

        template <typename T>
        inline grid_t<T> make_grid(std::size_t width, std::size_t height, T default_value) {
            return grid_t<T>(width * height, default_value);
        }

        inline constexpr bool in_bounds(std::size_t w, std::size_t h, std::size_t x, std::size_t y) noexcept {
            return x < w && y < h;
        }

        template <typename T>
        inline decltype(auto) at(grid_t<T>& grid, std::size_t width, std::size_t height, std::size_t x, std::size_t y) {
            assert(in_bounds(width, height, x, y) && "Grid access out of bounds!");
            return grid[y * width + x];
        }

        template <typename T>
        inline decltype(auto) at(const grid_t<T>& grid, std::size_t width, std::size_t height, std::size_t x, std::size_t y) {
            assert(in_bounds(width, height, x, y) && "Grid access out of bounds!");
            return grid[y * width + x];
        }

        inline constexpr std::size_t idx(std::size_t w, std::size_t x, std::size_t y) noexcept {
            return y * w + x;
        }

        inline std::vector<std::pair<std::size_t, std::size_t>>
            neighbors(std::size_t w, std::size_t h, std::size_t x, std::size_t y) noexcept {
            std::vector<std::pair<std::size_t, std::size_t>> n;
            if (y > 0)          n.emplace_back(x, y - 1);
            if (x + 1 < w)      n.emplace_back(x + 1, y);
            if (y + 1 < h)      n.emplace_back(x, y + 1);
            if (x > 0)          n.emplace_back(x - 1, y);
            return n;
        }

        template<typename T>
        inline bool is_free(const grid_t<T>& grid,
            std::size_t w, std::size_t h,
            std::size_t x, std::size_t y,
            T free_tile_value) noexcept
        {
            return in_bounds(w, h, x, y) && grid[idx(w, x, y)] == free_tile_value;
        }

    }
}
