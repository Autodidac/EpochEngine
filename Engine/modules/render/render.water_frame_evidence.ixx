// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <source_location>
#include <string_view>

export module render.water_frame_evidence;

import asset.mesh_artifact;
import asset.texture_artifact;
import core.env;
import core.logger;
import render.preview_grid;

export namespace epochengine::render_water_evidence
{
    [[nodiscard]] inline bool enabled() noexcept
    {
        static const bool enabled = []() noexcept
        {
            try
            {
                const auto value = core::env::get("EPOCH_TRACE_WATER_FRAMES");
                return value && value->size() == 1u && value->data()[0] == '1';
            }
            catch (...) { return false; }
        }();
        return enabled;
    }

    inline void record(const previewgrid::PublishedSceneFrame& frame,
        std::string_view backend, bool detail_enabled) noexcept
    {
        if (!enabled() || !frame.solids || frame.water_source.terrain_request == 0u)
            return;
        // A bounded diagnostic for the draw packet. No file or log work runs in an ordinary frame.
        static std::atomic_uint32_t recorded{};
        if (recorded.load(std::memory_order_relaxed) >= 2048u
            || recorded.fetch_add(1u, std::memory_order_relaxed) >= 2048u)
            return;
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto& detail_ranges = frame.solids->water_detail_ranges;
        std::uint64_t range_signature = 14695981039346656037ull;
        std::uint64_t detail_vertices{};
        for (const auto& range : detail_ranges)
        {
            range_signature = (range_signature ^ range.first_vertex) * 1099511628211ull;
            range_signature = (range_signature ^ range.vertex_count) * 1099511628211ull;
            detail_vertices += range.vertex_count;
        }
        try
        {
            for (const auto& marker : frame.markers)
            {
                const auto& water = marker.sphericalWater;
                if (!water)
                    continue;
                const auto resolved = std::ranges::count(water->ranges, true,
                    &asset::mesh::SphericalWaterRange::vertex_specular);
                std::uint64_t appearance_signature = 14695981039346656037ull;
                for (const auto& range : water->ranges)
                {
                    appearance_signature = (appearance_signature ^ range.first_vertex) * 1099511628211ull;
                    appearance_signature = (appearance_signature ^ range.vertex_count) * 1099511628211ull;
                    appearance_signature = (appearance_signature ^ std::bit_cast<std::uint32_t>(range.specular_scale))
                        * 1099511628211ull;
                    appearance_signature = (appearance_signature ^ std::uint64_t{range.vertex_specular})
                        * 1099511628211ull;
                }
                logger::infof_loc("Render.WaterFrame", std::source_location::current(),
                    "backend={} draw_us={} revision={} terrain_request={} artifact={} "
                    "appearance_ranges={} resolved_ranges={} appearance_signature={} detail_ranges={} detail_vertices={} "
                    "range_signature={} shader_detail={} sampled={} sample_s={:.6f} phase_s={:.6f} "
                    "eye={:.9f},{:.9f},{:.9f} near={:.8f} far={:.8f}",
                    backend, stamp, frame.revision, frame.water_source.terrain_request,
                    asset::texture::content_hash_hex(water->artifact_key), water->ranges.size(),
                    resolved, appearance_signature, detail_ranges.size(), detail_vertices, range_signature, detail_enabled,
                    frame.water_source.sampled_seconds.has_value(),
                    frame.water_source.sampled_seconds.value_or(0.0), frame.water_detail_seconds.value_or(0.0),
                    frame.preciseNavigationEye[0], frame.preciseNavigationEye[1], frame.preciseNavigationEye[2],
                    frame.camera.nearPlane, frame.camera.farPlane);
            }
        }
        catch (...) {}
    }
}
