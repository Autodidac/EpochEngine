// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

export module water.system;

import render.math;
import voxel.field;
import voxel.storage;

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
    };

    struct WaterValidation final
    {
        bool valid{};
        bool finite{};
        bool positiveExtent{};
        bool positivePhysicalProperties{};
    };

    [[nodiscard]] inline WaterValidation validate(const WaterBodyDesc& body) noexcept
    {
        const bool finite =
            epochengine::render_math::finite(body.center) &&
            epochengine::render_math::finite(body.halfExtents) &&
            epochengine::render_math::finite(body.waveDirection) &&
            std::isfinite(body.surfaceHeight) &&
            std::isfinite(body.waveAmplitude) &&
            std::isfinite(body.wavelength) &&
            std::isfinite(body.waveSpeed) &&
            std::isfinite(body.phase) &&
            std::isfinite(body.densityKilogramsPerCubicMeter) &&
            std::isfinite(body.kinematicViscosity);
        const bool positiveExtent =
            body.halfExtents.x > 0.0f &&
            body.halfExtents.y >= 0.0f &&
            body.halfExtents.z > 0.0f;
        const bool positivePhysicalProperties =
            body.wavelength > 0.0f &&
            body.densityKilogramsPerCubicMeter > 0.0f &&
            body.kinematicViscosity >= 0.0f;
        return {
            finite && positiveExtent && positivePhysicalProperties,
            finite,
            positiveExtent,
            positivePhysicalProperties
        };
    }

    [[nodiscard]] inline WaterBodyDesc sanitize(WaterBodyDesc body) noexcept
    {
        body.halfExtents = {
            (std::max)(0.001f, body.halfExtents.x),
            (std::max)(0.0f, body.halfExtents.y),
            (std::max)(0.001f, body.halfExtents.z)
        };
        body.waveAmplitude = (std::max)(0.0f, body.waveAmplitude);
        body.wavelength = (std::max)(0.001f, body.wavelength);
        body.waveDirection.y = 0.0f;
        body.waveDirection = epochengine::render_math::normalize(
            body.waveDirection,
            {1.0f, 0.0f, 0.0f});
        body.densityKilogramsPerCubicMeter =
            (std::max)(0.001f, body.densityKilogramsPerCubicMeter);
        body.kinematicViscosity = (std::max)(0.0f, body.kinematicViscosity);
        return body;
    }

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
        explicit WaterManager(std::size_t capacity = 256) noexcept
            : capacity_((std::max)(std::size_t{1}, capacity))
        {
        }

        [[nodiscard]] WaterBodyHandle create(WaterBodyDesc desc)
        {
            if (!validate(desc).valid || active_count() >= capacity_)
            {
                return {};
            }
            desc = sanitize(desc);

            std::uint32_t index{};
            if (!freeSlots_.empty())
            {
                index = freeSlots_.back();
                freeSlots_.pop_back();
            }
            else
            {
                index = static_cast<std::uint32_t>(slots_.size());
                slots_.push_back({});
            }

            Slot& slot = slots_[index];
            if (slot.generation == 0)
            {
                slot.generation = 1;
            }
            slot.desc = desc;
            slot.occupied = true;
            ++revision_;
            return {index, slot.generation};
        }

        [[nodiscard]] bool destroy(WaterBodyHandle handle) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr)
            {
                return false;
            }
            slot->occupied = false;
            slot->generation = next_generation(slot->generation);
            freeSlots_.push_back(handle.index);
            ++revision_;
            return true;
        }

        [[nodiscard]] bool update(WaterBodyHandle handle, WaterBodyDesc desc) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr || !validate(desc).valid)
            {
                return false;
            }
            slot->desc = sanitize(desc);
            ++revision_;
            return true;
        }

        [[nodiscard]] std::optional<WaterBodyDesc> get(WaterBodyHandle handle) const noexcept
        {
            const Slot* slot = resolve(handle);
            return slot == nullptr ? std::nullopt : std::optional<WaterBodyDesc>{slot->desc};
        }

        [[nodiscard]] WaterSample sample(const WaterQuery& query) const noexcept
        {
            WaterSample best{};
            if (!epochengine::render_math::finite(query.position) ||
                !std::isfinite(query.simulationTimeSeconds))
            {
                return best;
            }
            queryCount_.fetch_add(1, std::memory_order_relaxed);

            float bestDepth = -(std::numeric_limits<float>::max)();
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& slot = slots_[index];
                if (!slot.occupied || !slot.desc.enabled || !contains_xz(slot.desc, query.position))
                {
                    continue;
                }

                const WaveSample wave = evaluate_wave(slot.desc, query);
                if (slot.desc.kind == WaterBodyKind::BoxVolume)
                {
                    const float floor = slot.desc.center.y - slot.desc.halfExtents.y;
                    if (query.position.y < floor)
                    {
                        continue;
                    }
                }

                const float depth = wave.height - query.position.y;
                if (!best.found || depth > bestDepth)
                {
                    bestDepth = depth;
                    best = {
                        true,
                        depth >= 0.0f,
                        {index, slot.generation},
                        wave.height,
                        (std::max)(0.0f, depth),
                        wave.normal,
                        wave.velocity,
                        slot.desc.densityKilogramsPerCubicMeter,
                        slot.desc.kinematicViscosity,
                        slot.desc.material
                    };
                }
            }
            return best;
        }

        [[nodiscard]] WaterSample sample(
            WaterBodyHandle handle,
            const WaterQuery& query) const noexcept
        {
            WaterSample result{};
            const Slot* slot = resolve(handle);
            if (slot == nullptr || !slot->desc.enabled ||
                !epochengine::render_math::finite(query.position) ||
                !std::isfinite(query.simulationTimeSeconds) ||
                !contains_xz(slot->desc, query.position))
            {
                return result;
            }
            queryCount_.fetch_add(1, std::memory_order_relaxed);

            const WaveSample wave = evaluate_wave(slot->desc, query);
            if (slot->desc.kind == WaterBodyKind::BoxVolume)
            {
                const float floor = slot->desc.center.y - slot->desc.halfExtents.y;
                if (query.position.y < floor)
                {
                    return result;
                }
            }

            const float depth = wave.height - query.position.y;
            return {
                true,
                depth >= 0.0f,
                handle,
                wave.height,
                (std::max)(0.0f, depth),
                wave.normal,
                wave.velocity,
                slot->desc.densityKilogramsPerCubicMeter,
                slot->desc.kinematicViscosity,
                slot->desc.material
            };
        }

        [[nodiscard]] WaterMetrics metrics() const noexcept
        {
            WaterMetrics result{};
            result.slotCount = slots_.size();
            result.revision = revision_;
            result.queryCount = queryCount_.load(std::memory_order_relaxed);
            for (const Slot& slot : slots_)
            {
                if (!slot.occupied)
                {
                    continue;
                }
                ++result.activeBodyCount;
                if (slot.desc.enabled)
                {
                    ++result.enabledBodyCount;
                }
                switch (slot.desc.kind)
                {
                case WaterBodyKind::PlaneRegion: ++result.planeRegionCount; break;
                case WaterBodyKind::BoxVolume: ++result.boxVolumeCount; break;
                }
            }
            return result;
        }

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
        };

        [[nodiscard]] static bool contains_xz(
            const WaterBodyDesc& body,
            Float3 position) noexcept
        {
            return std::abs(position.x - body.center.x) <= body.halfExtents.x &&
                   std::abs(position.z - body.center.z) <= body.halfExtents.z;
        }

        [[nodiscard]] static WaveSample evaluate_wave(
            const WaterBodyDesc& body,
            const WaterQuery& query) noexcept
        {
            constexpr double tau = 6.2831853071795864769;
            const double waveNumber = tau / static_cast<double>(body.wavelength);
            const double projected =
                static_cast<double>(query.position.x - body.center.x) * body.waveDirection.x +
                static_cast<double>(query.position.z - body.center.z) * body.waveDirection.z;
            const double phase =
                waveNumber * projected -
                static_cast<double>(body.waveSpeed) * query.simulationTimeSeconds +
                body.phase;
            const double sine = std::sin(phase);
            const double cosine = std::cos(phase);
            const float height = body.surfaceHeight +
                body.waveAmplitude * static_cast<float>(sine);
            const float slope = body.waveAmplitude *
                static_cast<float>(waveNumber * cosine);
            const Float3 normal = epochengine::render_math::normalize({
                -slope * body.waveDirection.x,
                1.0f,
                -slope * body.waveDirection.z
            });
            const float waveVelocity =
                body.waveSpeed * body.waveAmplitude * static_cast<float>(cosine);
            const Float3 velocity{
                body.waveDirection.x * waveVelocity,
                -waveVelocity,
                body.waveDirection.z * waveVelocity
            };
            return {height, normal, velocity};
        }

        [[nodiscard]] static constexpr std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] Slot* resolve(WaterBodyHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] const Slot* resolve(WaterBodyHandle handle) const noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            const Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] std::size_t active_count() const noexcept
        {
            return slots_.size() - freeSlots_.size();
        }

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

    [[nodiscard]] inline WaterStampResult stamp_voxel_water(
        const WaterManager& water,
        const WaterStampRequest& request,
        epochengine::voxel::SparseVoxelField& field)
    {
        WaterStampResult result{};
        const auto body = water.get(request.body);
        if (!body.has_value() ||
            request.minimum.x > request.maximum.x ||
            request.minimum.y > request.maximum.y ||
            request.minimum.z > request.maximum.z ||
            request.maximumCells == 0)
        {
            return result;
        }

        const std::uint64_t width =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(request.maximum.x) - request.minimum.x + 1);
        const std::uint64_t height =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(request.maximum.y) - request.minimum.y + 1);
        const std::uint64_t depth =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(request.maximum.z) - request.minimum.z + 1);
        if (width > request.maximumCells ||
            height > request.maximumCells / width ||
            depth > request.maximumCells / (width * height))
        {
            return result;
        }

        result.bounded = true;
        const float cellSize = field.chunk_desc().cellSizeMeters;
        for (std::int64_t z = request.minimum.z; z <= request.maximum.z; ++z)
        {
            for (std::int64_t y = request.minimum.y; y <= request.maximum.y; ++y)
            {
                for (std::int64_t x = request.minimum.x; x <= request.maximum.x; ++x)
                {
                    ++result.visitedCells;
                    const Float3 position{
                        (static_cast<float>(x) + 0.5f) * cellSize,
                        (static_cast<float>(y) + 0.5f) * cellSize,
                        (static_cast<float>(z) + 0.5f) * cellSize
                    };
                    const WaterQuery query{position, request.simulationTimeSeconds};
                    const WaterSample sample = water.sample(request.body, query);
                    const epochengine::voxel::CellCoord coord{
                        static_cast<std::int32_t>(x),
                        static_cast<std::int32_t>(y),
                        static_cast<std::int32_t>(z)
                    };
                    epochengine::voxel::VoxelCell cell = field.read_or_empty(coord);
                    if (sample.found && sample.submerged && sample.body == request.body)
                    {
                        cell.semantics = cell.semantics | epochengine::voxel::CellSemantic::Water;
                        if (cell.material == 0)
                        {
                            cell.material = body->material;
                        }
                        (void)field.write(coord, cell);
                        ++result.waterCells;
                    }
                }
            }
        }
        result.applied = true;
        result.fieldRevision = field.revision();
        return result;
    }
}