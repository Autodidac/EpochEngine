"""Exercise the Software preview's actual opaque and wire raster helpers."""
import json
from pathlib import Path
import unittest

from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    path = ROOT / "Engine/src/renderers/software/software.context_preview.cpp"
    source = path.read_text(encoding="utf-8-sig")
    raster = source[source.index("    struct SoftwarePreviewVertex"):
                    source.index("    struct SoftwareTextureVertex")]
    preview = (ROOT / "Engine/modules/render/render.preview_grid.ixx").read_text(encoding="utf-8")
    clip = preview[preview.index("    export struct ClipVertex"):
                   preview.index("    export [[nodiscard]] inline ClipVertex transform_point")]
    return r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace core { struct RenderViewport { int x, y, width, height; }; }
namespace previewgrid {
''' + clip.replace("export ", "") + r'''
}
namespace render_camera { enum class ProjectionKind { perspective, orthographic }; }
struct Framebuffer {
    int width=16, height=16;
    std::vector<std::uint32_t> framebuffer=std::vector<std::uint32_t>(256);
    std::vector<float> previewDepth;
} s_softrendererstate;
''' + raster + r'''
int main()
{
    using previewgrid::ClipVertex;
    using render_camera::ProjectionKind;
    using Triangle = std::array<ClipVertex, 3>;
    constexpr core::RenderViewport viewport{3, 4, 8, 8};
    auto& frame = s_softrendererstate;
    const auto pixel = [&](int x, int y) { return frame.framebuffer[y * frame.width + x]; };
    const auto reset = [&] {
        std::fill(frame.framebuffer.begin(), frame.framebuffer.end(), 7u);
        assert(prepare_preview_depth(viewport));
        assert(frame.previewDepth.size() == 64);
    };

    // These triangles cross in depth. There is no correct whole-triangle order.
    constexpr Triangle rising{{{-1,-1,-0.6f,1}, {1,-1,0.6f,1}, {0,1,0,1}}};
    constexpr Triangle falling{{{-1,-1,0.6f,1}, {1,-1,-0.6f,1}, {0,1,0,1}}};
    reset();
    draw_clipped_preview_triangle(rising, 1, viewport, ProjectionKind::orthographic);
    draw_clipped_preview_triangle(falling, 2, viewport, ProjectionKind::orthographic);
    assert(pixel(5, 10) == 1 && pixel(8, 10) == 2);
    const auto firstOrder = frame.framebuffer;
    reset();
    draw_clipped_preview_triangle(falling, 2, viewport, ProjectionKind::orthographic);
    draw_clipped_preview_triangle(rising, 1, viewport, ProjectionKind::orthographic);
    assert(frame.framebuffer == firstOrder);

    // One corner went behind the camera. Its visible remainder must survive.
    reset();
    constexpr Triangle crossing{{{-1,-1,0,1}, {1,-1,0,1}, {0,1,-2,-0.5f}}};
    draw_clipped_preview_triangle(crossing, 3, viewport, ProjectionKind::perspective);
    assert(pixel(7, 11) == 3 && pixel(7, 4) == 7);
    assert(pixel(2, 11) == 7 && pixel(11, 11) == 7 && pixel(7, 12) == 7);

    reset();
    constexpr Triangle hidden{{{-1,-1,-2,1}, {1,-1,-2,1}, {0,1,-2,1}}};
    draw_clipped_preview_triangle(hidden, 3, viewport, ProjectionKind::orthographic);
    assert(std::all_of(frame.framebuffer.begin(), frame.framebuffer.end(),
        [](auto value) { return value == 7; }));

    // Exact near-plane vertices do not duplicate an intersection. Changing
    // viewport dimensions resets the depth index for a subsequent frame.
    constexpr Triangle orthographic{{{-1,-1,-1,1}, {1,-1,-1,1}, {0,1,-1,1}}};
    constexpr core::RenderViewport resized{2, 2, 4, 4};
    assert(prepare_preview_depth(resized));
    draw_clipped_preview_triangle(orthographic, 4, resized, ProjectionKind::orthographic);
    assert(frame.previewDepth.size() == 16 && pixel(4, 4) == 4 && pixel(7, 11) == 7);

    // With planetary clip distances, both surfaces collapse to the same NDC
    // float even though they are 100 units apart. The near face must win.
    const auto distant = [](float distance) -> Triangle {
        constexpr float nearPlane = 30.04f, farPlane = 13384723.0f;
        const float zScale = (farPlane + nearPlane) / (farPlane - nearPlane);
        const float zOffset = 2.0f * nearPlane * farPlane / (farPlane - nearPlane);
        const float z = zScale * distance - zOffset;
        return {{{-distance,-distance,z,distance}, {distance,-distance,z,distance},
            {0,distance,z,distance}}};
    };
    const auto nearFace = distant(1000000.0f), farFace = distant(1000100.0f);
    assert(nearFace[0].z / nearFace[0].w == farFace[0].z / farFace[0].w);
    reset();
    draw_clipped_preview_triangle(farFace, 2, viewport, ProjectionKind::perspective);
    draw_clipped_preview_triangle(nearFace, 1, viewport, ProjectionKind::perspective);
    assert(pixel(7, 8) == 1);
    const auto perspectivePixels = frame.framebuffer;
    reset();
    draw_clipped_preview_triangle(nearFace, 1, viewport, ProjectionKind::perspective);
    draw_clipped_preview_triangle(farFace, 2, viewport, ProjectionKind::perspective);
    assert(frame.framebuffer == perspectivePixels);

    // Both wire passes use identical sample positions. Only the near surface's
    // edges should survive regardless of object submission order.
    using Pass = SoftwarePreviewRasterPass;
    const auto overlap = [&](const Triangle& first, std::uint32_t firstColor,
        const Triangle& second, std::uint32_t secondColor)
    {
        reset();
        draw_clipped_preview_triangle<Pass::wire_depth>(first, 0, viewport, ProjectionKind::perspective);
        draw_clipped_preview_triangle<Pass::wire_depth>(second, 0, viewport, ProjectionKind::perspective);
        assert(pixel(7, 11) == 7);
        draw_clipped_preview_triangle<Pass::wire_color>(first, firstColor, viewport, ProjectionKind::perspective);
        draw_clipped_preview_triangle<Pass::wire_color>(second, secondColor, viewport, ProjectionKind::perspective);
        assert(pixel(7, 11) == 1 && pixel(7, 8) == 7);
    };
    overlap(nearFace, 1, farFace, 2);
    const auto wirePixels = frame.framebuffer;
    overlap(farFace, 2, nearFace, 1);
    assert(frame.framebuffer == wirePixels);

    reset();
    draw_clipped_preview_triangle(nearFace, 3, viewport, ProjectionKind::perspective);
    const auto opaquePixels = frame.framebuffer;
    draw_clipped_preview_triangle<Pass::wire_depth>(farFace, 0, viewport, ProjectionKind::perspective);
    draw_clipped_preview_triangle<Pass::wire_color>(farFace, 2, viewport, ProjectionKind::perspective);
    assert(frame.framebuffer == opaquePixels);

    // The clipped bottom edge and the fan diagonals are not authored edges.
    reset();
    constexpr Triangle oversized{{{-2,-2,0,1}, {2,-2,0,1}, {0,3,0,1}}};
    draw_clipped_preview_triangle<Pass::wire_depth>(oversized, 0, viewport, ProjectionKind::orthographic);
    draw_clipped_preview_triangle<Pass::wire_color>(oversized, 4, viewport, ProjectionKind::orthographic);
    assert(pixel(7, 11) == 7 && pixel(7, 8) == 7 && pixel(7, 4) == 7);
    assert(std::find(frame.framebuffer.begin(), frame.framebuffer.end(), 4u) != frame.framebuffer.end());
}
'''


class SoftwarePreviewDepth(unittest.TestCase):
    def test_crossing_surfaces_and_frustum_clipping(self):
        report = ROOT / "Artifacts/Reports/software-preview-depth"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "fixture.cpp"
        source.write_text(fixture(), encoding="utf-8")
        records = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps(records, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
