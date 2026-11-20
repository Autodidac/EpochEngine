// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

export module water.system;

import render.math;
import voxel.field;
import voxel.storage;
export import water.wave;

export namespace epochengine::water
{
    using Float3 = epochengine::render_math::Float3;

    enum class WaterBodyKind : std::uint8_t
    {
        PlaneRegion,
        BoxVolume
    };

    struct WaterBodyHandle final
    {
        std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != (std::numeric_limits<std::uint32_t>::max)() && generation != 0;
        }

        friend constexpr bool operator==(const WaterBodyHandle&, const WaterBodyHandle&) noexcept = default;
    };

    struct WaveComponent final
    {
        float amplitude{};
        float wavelength{8.0f};
        float speed{1.0f};
        Float3 direction{0.0f, 0.0f, 1.0f};
        float phase{};
    };

    struct WaterBodyDesc final
    {
        WaterBodyKind kind{WaterBodyKind::PlaneRegion};
        Float3 center{};
        Float3 halfExtents{32.0f, 8.0f, 32.0f};
        float surfaceHeight{};
        float waveAmplitude{0.15f};
        float wavelength{8.0f};
        float waveSpeed{1.0f};
        Float3 waveDirection{1.0f, 0.0f, 0.0f};
        float phase{};
        float densityKilogramsPerCubicMeter{997.0f};
        float kinematicViscosity{1.0e-6f};
        epochengine::voxel::MaterialId material{};
        bool enabled{true};
        WaveComponent secondaryWave{};
        std::shared_ptr<const wave::Tile> waveTile{};
    };

    struct WaterValidation final
    {
        bool valid{};
        bool finite{};
        bool positiveExtent{};
        bool positivePhysicalProperties{};
    };

    [[nodiscard]] WaterValidation validate(const WaterBodyDesc& body) noexcept;

    [[nodiscard]] WaterBodyDesc sanitize(WaterBodyDesc body) noexcept;

    struct WaterQuery final
    {
        Float3 position{};
        double simulationTimeSeconds{};
    };

    struct WaterSample final
    {
        bool found{};
        bool submerged{};
        WaterBodyHandle body{};
        float surfaceHeight{};
        float depth{};
        Float3 surfaceNormal{0.0f, 1.0f, 0.0f};
        Float3 velocity{};
        float densityKilogramsPerCubicMeter{};
        float kinematicViscosity{};
        epochengine::voxel::MaterialId material{};
        Float3 displacement{};
        float foam{};
    };

    struct WaterMetrics final
    {
        std::size_t slotCount{};
        std::size_t activeBodyCount{};
        std::size_t enabledBodyCount{};
        std::size_t planeRegionCount{};
        std::size_t boxVolumeCount{};
        std::uint64_t revision{};
        std::uint64_t queryCount{};
    };

    class WaterManager final
    {
    public:
        explicit WaterManager(std::size_t capacity = 256) noexcept;

        [[nodiscard]] WaterBodyHandle create(WaterBodyDesc desc);

        [[nodiscard]] bool destroy(WaterBodyHandle handle) noexcept;

        [[nodiscard]] bool update(WaterBodyHandle handle, WaterBodyDesc desc) noexcept;

        [[nodiscard]] std::optional<WaterBodyDesc> get(WaterBodyHandle handle) const noexcept;

        [[nodiscard]] WaterSample sample(const WaterQuery& query) const noexcept;

        [[nodiscard]] WaterSample sample(
            WaterBodyHandle handle,
            const WaterQuery& query) const noexcept;

        [[nodiscard]] WaterMetrics metrics() const noexcept;

    private:
        struct Slot final
        {
            WaterBodyDesc desc{};
            std::uint32_t generation{1};
            bool occupied{};
        };

        struct WaveSample final
        {
            float height{};
            Float3 normal{0.0f, 1.0f, 0.0f};
            Float3 velocity{};
            Float3 displacement{};
            float foam{};
            bool valid{true};
        };

        [[nodiscard]] static bool contains_xz(
            const WaterBodyDesc& body,
            Float3 position) noexcept;

        [[nodiscard]] static WaveSample evaluate_wave(
            const WaterBodyDesc& body,
            const WaterQuery& query) noexcept;

        [[nodiscard]] static constexpr std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] Slot* resolve(WaterBodyHandle handle) noexcept;

        [[nodiscard]] const Slot* resolve(WaterBodyHandle handle) const noexcept;

        [[nodiscard]] std::size_t active_count() const noexcept;

        std::vector<Slot> slots_{};
        std::vector<std::uint32_t> freeSlots_{};
        std::size_t capacity_{256};
        std::uint64_t revision_{};
        mutable std::atomic<std::uint64_t> queryCount_{};
    };

    struct WaterStampRequest final
    {
        WaterBodyHandle body{};
        epochengine::voxel::CellCoord minimum{};
        epochengine::voxel::CellCoord maximum{};
        double simulationTimeSeconds{};
        std::uint64_t maximumCells{1'000'000};
    };

    struct WaterStampResult final
    {
        bool applied{};
        bool bounded{};
        std::uint64_t visitedCells{};
        std::uint64_t waterCells{};
        std::uint64_t fieldRevision{};
    };

    [[nodiscard]] WaterStampResult stamp_voxel_water(
        const WaterManager& water,
        const WaterStampRequest& request,
        epochengine::voxel::SparseVoxelField& field);
}
