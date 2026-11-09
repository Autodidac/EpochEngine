// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>

import water.system;
import voxel.field;
import voxel.storage;

namespace
{
    using namespace epochengine::water;

    [[nodiscard]] bool same_sample(const WaterSample& left, const WaterSample& right)
    {
        return left.found == right.found && left.submerged == right.submerged &&
            left.body == right.body && left.surfaceHeight == right.surfaceHeight &&
            left.depth == right.depth && left.surfaceNormal == right.surfaceNormal &&
            left.velocity == right.velocity && left.material == right.material &&
            left.densityKilogramsPerCubicMeter == right.densityKilogramsPerCubicMeter &&
            left.kinematicViscosity == right.kinematicViscosity;
    }
}

#define CHECK(...) do { if (!(__VA_ARGS__)) return __LINE__; } while (false)

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    WaterBodyDesc invalid{};
    CHECK(validate(invalid).valid);
    invalid.center.x = (std::numeric_limits<float>::quiet_NaN)();
    CHECK(!validate(invalid).valid && !validate(invalid).finite);
    invalid = {};
    invalid.halfExtents.z = 0.0f;
    CHECK(!validate(invalid).valid && !validate(invalid).positiveExtent);
    invalid = {};
    invalid.wavelength = 0.0f;
    CHECK(!validate(invalid).valid && !validate(invalid).positivePhysicalProperties);
    const auto sanitized = sanitize({.waveAmplitude = -1.0f, .waveDirection = {0.0f, 8.0f, 0.0f}});
    CHECK(sanitized.waveAmplitude == 0.0f && sanitized.waveDirection == Float3{1.0f, 0.0f, 0.0f});

    WaterManager bounded{0};
    CHECK(!bounded.create(invalid).valid() && bounded.metrics().revision == 0);
    const auto oldHandle = bounded.create({});
    CHECK(oldHandle.valid() && !bounded.create({}).valid());
    CHECK(bounded.destroy(oldHandle) && !bounded.destroy(oldHandle));
    CHECK(!bounded.get(oldHandle) && !bounded.update(oldHandle, {}));
    const auto reused = bounded.create({});
    CHECK(reused.valid() && reused.index == oldHandle.index && reused.generation != oldHandle.generation);
    CHECK(!bounded.sample(oldHandle, {{}, 0.0}).found);

    // Same contract used by the Engine's build-safe temporal/voxel checks.
    WaterManager manager;
    const auto body = manager.create({
        .kind = WaterBodyKind::BoxVolume, .center = {}, .halfExtents = {2.0f, 2.0f, 2.0f},
        .surfaceHeight = 0.5f, .waveAmplitude = 0.0f, .material = 11u});
    const auto sample = manager.sample(body, {{}, 4.0});
    const auto overlapping = manager.create({
        .kind = WaterBodyKind::BoxVolume, .center = {}, .halfExtents = {2.0f, 2.0f, 2.0f},
        .surfaceHeight = 1.0f, .waveAmplitude = 0.0f, .material = 12u});
    CHECK(body.valid() && overlapping.valid() && sample.found && sample.submerged);
    CHECK(std::abs(sample.surfaceHeight - 0.5f) < 0.001f);
    CHECK(manager.sample({{}, 4.0}).body == overlapping);
    CHECK(!manager.sample(body, {{3.0f, 0.0f, 0.0f}, 4.0}).found);
    CHECK(!manager.sample(body, {{0.0f, -3.0f, 0.0f}, 4.0}).found);
    const auto above = manager.sample(body, {{0.0f, 2.0f, 0.0f}, 4.0});
    CHECK(above.found && !above.submerged && above.depth == 0.0f);
    const auto queries = manager.metrics().queryCount;
    CHECK(!manager.sample({{}, (std::numeric_limits<double>::infinity)()}).found);
    CHECK(manager.metrics().queryCount == queries);

    epochengine::voxel::SparseVoxelField field{{4u, 4u, 4u, 1.0f, 0u}};
    WaterStampRequest stamp{body, {-1, -1, -1}, {1, 0, 1}, 4.0, 64u};
    const auto applied = stamp_voxel_water(manager, stamp, field);
    CHECK(applied.applied && applied.bounded && applied.waterCells > 0u);
    CHECK(applied.visitedCells == 18 && field.metrics().waterCellCount == applied.waterCells);
    CHECK(field.read({0, 0, 0})->material == 11u);
    const auto fieldBeforeRefusal = field.snapshot();
    stamp.maximumCells = 1;
    CHECK(!stamp_voxel_water(manager, stamp, field).bounded);
    stamp.maximumCells = (std::numeric_limits<std::uint64_t>::max)();
    stamp.minimum = {(std::numeric_limits<std::int32_t>::min)(), 0, 0};
    stamp.maximum = {(std::numeric_limits<std::int32_t>::max)(),
                     (std::numeric_limits<std::int32_t>::max)(),
                     (std::numeric_limits<std::int32_t>::max)()};
    CHECK(!stamp_voxel_water(manager, stamp, field).bounded);
    CHECK(field.snapshot().contentHash == fieldBeforeRefusal.contentHash);
    CHECK(field.revision() == fieldBeforeRefusal.revision);

    // Explicit-time queries have no mutable clock: seek forward/backward and
    // repeat at the same address without changing the body or its revision.
    WaterManager waves;
    WaterBodyDesc desc{.kind = WaterBodyKind::PlaneRegion, .center = {1.0f, 0.0f, -2.0f},
        .halfExtents = {8.0f, 0.0f, 8.0f}, .surfaceHeight = 2.0f,
        .waveAmplitude = 0.7f, .wavelength = 5.0f, .waveSpeed = 1.3f,
        .waveDirection = {2.0f, 0.0f, 1.0f}, .phase = 0.2f, .material = 29u};
    const auto waveBody = waves.create(desc);
    CHECK(waveBody.valid());
    const auto revision = waves.metrics().revision;
    std::ofstream replay{argv[1], std::ios::binary};
    CHECK(replay.is_open());
    for (const double seconds : {-10.0, 0.0, 0.25, 1.5, 8.0, 50.0})
    {
        const WaterQuery query{{2.5f, 1.0f, -1.0f}, seconds};
        const auto expected = waves.sample(waveBody, query);
        CHECK(expected.found && std::isfinite(expected.surfaceHeight));
        (void)waves.sample(waveBody, {query.position, seconds + 100.0});
        const auto rewind = waves.sample(waveBody, query);
        CHECK(same_sample(expected, rewind));
        CHECK(same_sample(expected, waves.sample(query)));
        CHECK(waves.metrics().revision == revision);
        replay << std::bit_cast<std::uint32_t>(expected.surfaceHeight) << ' '
               << std::bit_cast<std::uint32_t>(expected.depth) << ' '
               << std::bit_cast<std::uint32_t>(expected.surfaceNormal.x) << ' '
               << std::bit_cast<std::uint32_t>(expected.surfaceNormal.y) << ' '
               << std::bit_cast<std::uint32_t>(expected.surfaceNormal.z) << ' '
               << std::bit_cast<std::uint32_t>(expected.velocity.x) << ' '
               << std::bit_cast<std::uint32_t>(expected.velocity.y) << ' '
               << std::bit_cast<std::uint32_t>(expected.velocity.z) << '\n';
    }
    desc.enabled = false;
    CHECK(waves.update(waveBody, desc) && !waves.sample({{2.5f, 1.0f, -1.0f}, 0.0}).found);
    CHECK(waves.metrics().enabledBodyCount == 0 && waves.metrics().planeRegionCount == 1);
    desc.enabled = true;
    CHECK(waves.update(waveBody, desc));
    const auto beforeInvalidUpdate = waves.metrics().revision;
    desc.waveSpeed = (std::numeric_limits<float>::quiet_NaN)();
    CHECK(!waves.update(waveBody, desc) && waves.metrics().revision == beforeInvalidUpdate);
    CHECK(waves.destroy(waveBody) && waves.metrics().activeBodyCount == 0);

    WaterBodyDesc crossed{.halfExtents = {8.0f, 1.0f, 8.0f}, .waveAmplitude = 0.4f,
        .wavelength = 5.0f, .waveSpeed = 1.3f, .waveDirection = {1.0f, 0.0f, 0.0f}, .phase = 0.2f,
        .secondaryWave = {.amplitude = 0.25f, .wavelength = 3.0f, .speed = 0.9f,
            .direction = {0.0f, 0.0f, 1.0f}, .phase = -0.4f}};
    WaterManager crossing{3};
    const auto combined = crossing.create(crossed);
    auto primary = crossed;
    primary.secondaryWave.amplitude = 0.0f;
    const auto primaryHandle = crossing.create(primary);
    auto secondary = primary;
    secondary.waveAmplitude = crossed.secondaryWave.amplitude;
    secondary.wavelength = crossed.secondaryWave.wavelength;
    secondary.waveSpeed = crossed.secondaryWave.speed;
    secondary.waveDirection = crossed.secondaryWave.direction;
    secondary.phase = crossed.secondaryWave.phase;
    const auto secondaryHandle = crossing.create(secondary);
    CHECK(combined.valid() && primaryHandle.valid() && secondaryHandle.valid());
    for (const double seconds : {-2.0, 0.0, 0.3, 3.0})
    {
        const WaterQuery query{{0.7f, 0.0f, 0.9f}, seconds};
        const auto both = crossing.sample(combined, query);
        const auto one = crossing.sample(primaryHandle, query);
        const auto two = crossing.sample(secondaryHandle, query);
        CHECK(std::abs(both.surfaceHeight - (one.surfaceHeight + two.surfaceHeight)) < 1.0e-6f);
        CHECK(std::abs(both.velocity.x - (one.velocity.x + two.velocity.x)) < 1.0e-6f);
        CHECK(std::abs(both.velocity.y - (one.velocity.y + two.velocity.y)) < 1.0e-6f);
        CHECK(std::abs(both.velocity.z - (one.velocity.z + two.velocity.z)) < 1.0e-6f);
        constexpr float epsilon = 0.001f;
        const auto height = [&](float x, float z, double time)
        { return crossing.sample(combined, {{x, 0.0f, z}, time}).surfaceHeight; };
        const float dx = (height(0.7f + epsilon, 0.9f, seconds) -
                          height(0.7f - epsilon, 0.9f, seconds)) / (2.0f * epsilon);
        const float dz = (height(0.7f, 0.9f + epsilon, seconds) -
                          height(0.7f, 0.9f - epsilon, seconds)) / (2.0f * epsilon);
        CHECK(std::abs(dx + both.surfaceNormal.x / both.surfaceNormal.y) < 0.0002f);
        CHECK(std::abs(dz + both.surfaceNormal.z / both.surfaceNormal.y) < 0.0002f);
        const float dt = (height(0.7f, 0.9f, seconds + epsilon) -
                          height(0.7f, 0.9f, seconds - epsilon)) / (2.0f * epsilon);
        CHECK(std::abs(dt - both.velocity.y) < 0.0002f);
        (void)crossing.sample(combined, {{-1.0f, 0.0f, 1.0f}, seconds + 50.0});
        CHECK(same_sample(both, crossing.sample(combined, query)));
    }
    invalid = crossed;
    invalid.secondaryWave.phase = (std::numeric_limits<float>::quiet_NaN)();
    const auto beforeInvalidSecondary = crossing.metrics().revision;
    CHECK(!validate(invalid).finite && !crossing.update(combined, invalid));
    CHECK(crossing.metrics().revision == beforeInvalidSecondary);
    CHECK(crossing.get(combined)->secondaryWave.phase == crossed.secondaryWave.phase);
    invalid = crossed;
    invalid.secondaryWave.wavelength = 0.0f;
    CHECK(!validate(invalid).valid);
    invalid = crossed;
    invalid.secondaryWave.direction.x = (std::numeric_limits<float>::infinity)();
    CHECK(!validate(invalid).valid);
    invalid = crossed;
    invalid.waveAmplitude = invalid.secondaryWave.amplitude = (std::numeric_limits<float>::max)();
    CHECK(!validate(invalid).finite);
    auto sanitizedSecondary = crossed;
    sanitizedSecondary.secondaryWave.amplitude = -1.0f;
    sanitizedSecondary.secondaryWave.direction = {0.0f, 5.0f, 0.0f};
    sanitizedSecondary = sanitize(sanitizedSecondary);
    CHECK(sanitizedSecondary.secondaryWave.amplitude == 0.0f);
    CHECK(sanitizedSecondary.secondaryWave.direction == Float3{0.0f, 0.0f, 1.0f});
    replay << "PASS: handles, validation, temporal replay, bounded voxel stamping\n";
    replay.close();
    CHECK(replay.good());
    return 0;
}
