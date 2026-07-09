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
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include <deque>
#include <mutex>

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module assets.streaming;

//import <optional>;

export namespace epoch
{
    enum class AssetKind : u8 { texture, mesh, shader_blob };

    struct StreamingRequest
    {
        AssetKind kind{};
        u32 asset_id = 0;
        u32 a = 0, b = 0, c = 0;
    };

    class Streamer
    {
    public:
        void push(StreamingRequest r)
        {
            std::scoped_lock lk(m_mtx);
            m_q.push_back(r);
        }

        [[nodiscard]] std::optional<StreamingRequest> pop()
        {
            std::scoped_lock lk(m_mtx);
            if (m_q.empty()) return std::nullopt;
            auto r = m_q.front();
            m_q.pop_front();
            return r;
        }

    private:
        mutable std::mutex m_mtx{};
        std::deque<StreamingRequest> m_q{};
    };
} // namespace epoch
