/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.assets.streaming;

import <deque>;
import <mutex>;
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
