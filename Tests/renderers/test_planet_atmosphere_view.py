"""Exercise the shared sky camera and body selection without a graphics device."""
import hashlib
import json
from pathlib import Path
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition


ROOT = Path(__file__).resolve().parents[2]


class PlanetAtmosphereView(unittest.TestCase):
    def test_rebased_camera_sky_selection_and_lighting(self):
        grid = (ROOT / 'Engine/modules/render/render.preview_grid.ixx').read_text(encoding='utf-8')
        sky = (ROOT / 'Engine/modules/render/render.preview_atmosphere.ixx').read_text(encoding='utf-8')
        math = '\n'.join(definition(grid, f'export [[nodiscard]] inline {signature}').replace('export ', '', 1)
                         for signature in ('Vec3 scale(', 'Vec3 subtract(', 'float dot(', 'Vec3 cross(',
                                           'Vec3 normalize(', 'Vec3 camera_forward('))
        producer = definition(sky, '[[nodiscard]] inline std::optional<PlanetAtmosphereView> planet_atmosphere_view(')
        packet = definition(sky, 'struct PlanetAtmosphereView final') + ';'
        body = definition(grid, 'export struct PlanetAtmosphere final').replace('export ', '', 1) + ';'
        vec = definition(grid, 'export struct Vec3').replace('export ', '', 1) + ';'
        code = r'''
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
namespace epochengine::previewgrid {
VEC
BODY
struct Camera {
    Vec3 eye{}, target{0,0,-1}, up{0,1,0};
    float fovRadians{0.9f};
    bool navigationValid{};
    struct { Vec3 forward; } navigation{};
};
MATH
namespace lighting {
    enum class LightKind { Point, Directional };
    struct Desc { bool enabled{}; LightKind kind{}; float intensity{}; Vec3 direction{}; };
}
struct Marker { std::optional<PlanetAtmosphere> planetAtmosphere; };
struct Solids {
    struct { struct Entry { lighting::Desc desc; }; std::vector<Entry> lights; } lighting;
};
struct PublishedSceneFrame {
    Camera camera{};
    Solids* solids{};
    std::vector<Marker> markers;
    std::array<double,3> render_origin{};
    bool space_background{};
};
PACKET
PRODUCER
}

using namespace epochengine::previewgrid;
bool near(float left, float right) { return std::abs(left-right) < 1e-5f; }
#define CHECK(value) do { if (!(value)) return __LINE__; } while(false)
int main() {
    PublishedSceneFrame frame;
    constexpr double origin = 1.e12;
    frame.render_origin = {origin, origin, -origin};
    frame.camera.eye = {1, 2, 3};
    frame.camera.navigationValid = true;
    frame.camera.navigation.forward = {0, 0, -1};
    CHECK(!planet_atmosphere_view(frame, 2));
    frame.markers = {{{PlanetAtmosphere{{origin, origin, -origin + 600.0}, 100, 10}}},
                     {{PlanetAtmosphere{{origin, origin, -origin - 1000.0}, 100, 10}}}};
    auto sky = planet_atmosphere_view(frame, 2);
    CHECK(sky && near(sky->observer.z, 10.03f) && near(sky->thickness, 0.1f));
    CHECK(near(sky->observer.x, 0.01f) && near(sky->observer.y, 0.02f));
    CHECK(near(sky->right.x, 2 * std::tan(0.45f)) && near(sky->up.y, std::tan(0.45f)));
    CHECK(!sky->sun_visible);
    Solids solids;
    using Kind = lighting::LightKind;
    solids.lighting.lights = {{{true, Kind::Directional, 1, {-1,0,0}}},
                             {{true, Kind::Directional, 3, {0,-3,-4}}},
                             {{false, Kind::Directional, 6, {0,1,0}}},
                             {{true, Kind::Point, 9, {0,1,0}}}};
    frame.solids = &solids;
    sky = planet_atmosphere_view(frame, 2);
    CHECK(sky && sky->sun_visible && near(sky->sun.y, 0.6f) && near(sky->sun.z, 0.8f));
    frame.markers.erase(frame.markers.begin() + 1);
    CHECK(!planet_atmosphere_view(frame, 2));
    frame.space_background = true;
    sky = planet_atmosphere_view(frame, 2);
    CHECK(sky && sky->thickness == 0 && sky->sun_visible);
    CHECK(!planet_atmosphere_view(frame, 0));
    frame.camera.fovRadians = std::numeric_limits<float>::infinity();
    CHECK(!planet_atmosphere_view(frame, 2));
    return 0;
}
'''
        for token, value in {'VEC': vec, 'BODY': body, 'MATH': math, 'PACKET': packet, 'PRODUCER': producer}.items():
            code = code.replace(token, value)
        report = ROOT / 'Artifacts/Reports/planet-atmosphere-view' / hashlib.sha256(code.encode()).hexdigest()[:12]
        report.mkdir(parents=True, exist_ok=True)
        source = report / 'contract.cpp'
        source.write_text(code, encoding='utf-8')
        checks = compile_and_run(self, source, report)
        (report / 'result.json').write_text(json.dumps(checks, indent=2) + '\n', encoding='utf-8')
