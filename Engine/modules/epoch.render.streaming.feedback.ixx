/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;


#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.render.streaming.feedback;

import epoch.assets.streaming;

export namespace epoch
{
    struct FeedbackRecord
    {
        u32 kind = 0;
        u32 asset_id = 0;
        u32 a = 0, b = 0, c = 0;
    };

    [[nodiscard]] inline std::vector<StreamingRequest>
    feedback_to_requests(std::span<const FeedbackRecord> records)
    {
        std::vector<StreamingRequest> out{};
        out.reserve(records.size());

        for (const auto& r : records)
        {
            StreamingRequest req{};
            switch (r.kind)
            {
            case 1: req.kind = AssetKind::texture; req.asset_id = r.asset_id; req.a = r.a; break;
            case 2: req.kind = AssetKind::texture; req.asset_id = r.asset_id; req.a = r.a; req.b = r.b; req.c = r.c; break;
            case 3: req.kind = AssetKind::mesh;    req.asset_id = r.asset_id; req.a = r.a; break;
            default: continue;
            }
            out.push_back(req);
        }
        return out;
    }
} // namespace epoch
