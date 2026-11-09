// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

import water.system;
import water.surface;
import asset.mesh_artifact;
import asset.texture_artifact;

#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)

int main(int argc, char** argv)
{
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    CHECK(argc == 2);
    using namespace epochengine;
    using asset::mesh::Code;
    water::WaterBodyDesc body{.kind = water::WaterBodyKind::PlaneRegion,
        .halfExtents = {8.0f, 1.0f, 8.0f}, .waveAmplitude = 0.35f,
        .wavelength = 4.0f, .waveSpeed = 1.4f, .waveDirection = {1.0f, 0.0f, 1.0f}};
    CHECK(water::surface::build(body, 0.0, 1).code == Code::limit_exceeded);
    CHECK(water::surface::build(body, 0.0, 65).code == Code::limit_exceeded);
    CHECK(water::surface::build(body, 0.0, (std::numeric_limits<std::uint32_t>::max)()).code == Code::limit_exceeded);
    CHECK(water::surface::build(body, (std::numeric_limits<double>::quiet_NaN)()).code == Code::invalid_geometry);
    CHECK(water::surface::build(body, (std::numeric_limits<double>::infinity)()).code == Code::invalid_geometry);
    auto invalid = body;
    invalid.center.x = (std::numeric_limits<float>::infinity)();
    CHECK(water::surface::build(invalid, 0.0).code == Code::invalid_geometry);
    invalid = body;
    invalid.waveSpeed = (std::numeric_limits<float>::quiet_NaN)();
    CHECK(water::surface::build(invalid, 0.0).code == Code::invalid_geometry);
    invalid = body;
    invalid.enabled = false;
    CHECK(water::surface::build(invalid, 0.0).code == Code::invalid_geometry);
    invalid = body;
    invalid.kind = static_cast<water::WaterBodyKind>(255);
    CHECK(water::surface::build(invalid, 0.0).code == Code::invalid_geometry);

    water::WaterManager manager{1};
    const auto handle = manager.create(body);
    CHECK(handle.valid());
    const std::array times{-1.5, 0.0, 0.125, 0.5, 1.25, 4.0};
    std::array<std::vector<std::byte>, times.size()> visited;
    std::array<asset::mesh::ContentHash, times.size()> keys;
    std::ofstream output{argv[1], std::ios::binary};
    CHECK(output.is_open());
    for (std::size_t time = 0; time < times.size(); ++time)
    {
        const auto result = water::surface::build(body, times[time]);
        CHECK(result && result.artifact->vertices().size() == 33u * 33u);
        CHECK(result.artifact->indices().size() == 32u * 32u * 6u);
        visited[time] = asset::mesh::encode_artifact(*result.artifact);
        keys[time] = result.artifact->key();
        CHECK(!keys[time].empty() && visited[time].size() < 4u * 1024u * 1024u);
        CHECK(asset::mesh::decode_artifact(visited[time]));
        CHECK(result.artifact->minimum()[0] == -8.0f && result.artifact->maximum()[0] == 8.0f);
        CHECK(result.artifact->minimum()[2] == -8.0f && result.artifact->maximum()[2] == 8.0f);
        CHECK(result.artifact->minimum()[1] >= -0.35f && result.artifact->maximum()[1] <= 0.35f);
        for (const auto& vertex : result.artifact->vertices())
        {
            const auto query = manager.sample(handle, {{vertex.position[0], 0.0f, vertex.position[2]}, times[time]});
            CHECK(query.found && vertex.position[1] == query.surfaceHeight);
            CHECK(vertex.normal == std::array{query.surfaceNormal.x, query.surfaceNormal.y, query.surfaceNormal.z});
            const double norm = double(vertex.normal[0]) * vertex.normal[0] +
                double(vertex.normal[1]) * vertex.normal[1] + double(vertex.normal[2]) * vertex.normal[2];
            CHECK(std::abs(norm - 1.0) < 1.0e-5 && vertex.normal[1] > 0.0f);
            CHECK(vertex.uv[0] >= 0.0f && vertex.uv[0] <= 1.0f && vertex.uv[1] >= 0.0f && vertex.uv[1] <= 1.0f);
            CHECK(vertex.color[2] > vertex.color[1] && vertex.color[1] > vertex.color[0]);
        }
        const auto vertices = result.artifact->vertices();
        const auto indices = result.artifact->indices();
        for (std::size_t triangle = 0; triangle < indices.size(); triangle += 3)
        {
            const auto a = vertices[indices[triangle]].position;
            const auto b = vertices[indices[triangle + 1]].position;
            const auto c = vertices[indices[triangle + 2]].position;
            const double up = (double(b[2]) - a[2]) * (double(c[0]) - a[0]) -
                              (double(b[0]) - a[0]) * (double(c[2]) - a[2]);
            CHECK(up > 0.0);
        }
        output << asset::texture::content_hash_hex(keys[time]) << '\n';
        output.flush();
    }
    CHECK(keys[0] != keys[1] && keys[1] != keys[2]);
    for (std::size_t time = times.size(); time-- > 0;)
    {
        const auto rewind = water::surface::build(body, times[time]);
        CHECK(rewind && rewind.artifact->key() == keys[time]);
        CHECK(asset::mesh::encode_artifact(*rewind.artifact) == visited[time]);
    }
    for (auto cells : {2u, 64u})
    {
        const auto result = water::surface::build(body, 0.0, cells);
        CHECK(result && result.artifact->vertices().size() == std::size_t(cells + 1u) * (cells + 1u));
        CHECK(result.artifact->indices().size() == std::size_t(cells) * cells * 6u);
        CHECK(asset::mesh::encode_artifact(*result.artifact).size() < 4u * 1024u * 1024u);
    }
    auto offset = body;
    offset.center = {1.1f, 3.0f, -2.3f};
    offset.halfExtents = {0.1f, 1.0f, 0.1f};
    offset.kind = water::WaterBodyKind::BoxVolume;
    CHECK(water::surface::build(offset, 0.25, 4));
    offset.halfExtents.x = 0.0f;
    CHECK(water::surface::build(offset, 0.25).code == Code::invalid_geometry);
    offset = body;
    offset.center.x = 2.0e9f;
    CHECK(water::surface::build(offset, 0.25).code == Code::invalid_geometry);

    CHECK(water::surface::build(body, 0.25, 4));
    const auto crossing = water::surface::default_body();
    CHECK(crossing.secondaryWave.amplitude > 0.0f);
    const float amplitudeBound = crossing.waveAmplitude + crossing.secondaryWave.amplitude;
    std::array<std::vector<std::byte>, times.size()> crossingVisited;
    bool combinedCrest{};
    for (std::size_t time = 0; time < times.size(); ++time)
    {
        const auto sampled = water::surface::build(crossing, times[time]);
        CHECK(sampled && sampled.artifact->vertices().size() == 1089u);
        crossingVisited[time] = asset::mesh::encode_artifact(*sampled.artifact);
        CHECK(sampled.artifact->minimum()[1] >= crossing.surfaceHeight - amplitudeBound - 0.00001f);
        CHECK(sampled.artifact->maximum()[1] <= crossing.surfaceHeight + amplitudeBound + 0.00001f);
        for (const auto& vertex : sampled.artifact->vertices())
            if (vertex.position[1] > crossing.surfaceHeight + crossing.waveAmplitude)
            {
                // A crest beyond the primary amplitude must still use the
                // combined range instead of saturating at the primary limit.
                combinedCrest = true;
                CHECK(vertex.color[0] < 0.14f);
            }
    }
    CHECK(combinedCrest);
    for (std::size_t time = times.size(); time-- > 0;)
    {
        const auto revisited = water::surface::build(crossing, times[time]);
        CHECK(revisited && asset::mesh::encode_artifact(*revisited.artifact) == crossingVisited[time]);
    }
    output << "PASS: bounded analytic geometry, query normals, upward winding, temporal artifact replay\n";
    output.close();
    CHECK(output.good());
    return 0;
}
