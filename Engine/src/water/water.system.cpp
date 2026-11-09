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

module water.system;

namespace epochengine::water
{
    [[nodiscard]] WaterValidation validate(const WaterBodyDesc& body) noexcept
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
            std::isfinite(body.secondaryWave.amplitude) &&
            std::isfinite(body.secondaryWave.wavelength) &&
            std::isfinite(body.secondaryWave.speed) &&
            epochengine::render_math::finite(body.secondaryWave.direction) &&
            std::isfinite(body.secondaryWave.phase) &&
            std::isfinite((std::max)(0.0f, body.waveAmplitude) +
                          (std::max)(0.0f, body.secondaryWave.amplitude)) &&
            std::isfinite(body.densityKilogramsPerCubicMeter) &&
            std::isfinite(body.kinematicViscosity);
        const bool positiveExtent =
            body.halfExtents.x > 0.0f &&
            body.halfExtents.y >= 0.0f &&
            body.halfExtents.z > 0.0f;
        const bool positivePhysicalProperties =
            body.wavelength > 0.0f &&
            body.secondaryWave.wavelength > 0.0f &&
            body.densityKilogramsPerCubicMeter > 0.0f &&
            body.kinematicViscosity >= 0.0f;
        return {
            finite && positiveExtent && positivePhysicalProperties,
            finite,
            positiveExtent,
            positivePhysicalProperties
        };
    }

    [[nodiscard]] WaterBodyDesc sanitize(WaterBodyDesc body) noexcept
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
        body.secondaryWave.amplitude = (std::max)(0.0f, body.secondaryWave.amplitude);
        body.secondaryWave.wavelength = (std::max)(0.001f, body.secondaryWave.wavelength);
        body.secondaryWave.direction.y = 0.0f;
        body.secondaryWave.direction = epochengine::render_math::normalize(
            body.secondaryWave.direction, {0.0f, 0.0f, 1.0f});
        body.densityKilogramsPerCubicMeter =
            (std::max)(0.001f, body.densityKilogramsPerCubicMeter);
        body.kinematicViscosity = (std::max)(0.0f, body.kinematicViscosity);
        return body;
    }

    WaterManager::WaterManager(std::size_t capacity) noexcept
        : capacity_((std::max)(std::size_t{1}, capacity))
    {
    }

    [[nodiscard]] WaterBodyHandle WaterManager::create(WaterBodyDesc desc)
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

    [[nodiscard]] bool WaterManager::destroy(WaterBodyHandle handle) noexcept
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

    [[nodiscard]] bool WaterManager::update(WaterBodyHandle handle, WaterBodyDesc desc) noexcept
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

    [[nodiscard]] std::optional<WaterBodyDesc> WaterManager::get(WaterBodyHandle handle) const noexcept
    {
        const Slot* slot = resolve(handle);
        return slot == nullptr ? std::nullopt : std::optional<WaterBodyDesc>{slot->desc};
    }

    [[nodiscard]] WaterSample WaterManager::sample(const WaterQuery& query) const noexcept
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
            if(!wave.valid)continue;
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
                    slot.desc.material,
                    wave.displacement,
                    wave.foam
                };
            }
        }
        return best;
    }

    [[nodiscard]] WaterSample WaterManager::sample(
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
        if(!wave.valid)return result;
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
            slot->desc.material,
            wave.displacement,
            wave.foam
        };
    }

    [[nodiscard]] WaterMetrics WaterManager::metrics() const noexcept
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

    [[nodiscard]] bool WaterManager::contains_xz(
        const WaterBodyDesc& body,
        Float3 position) noexcept
    {
        return std::abs(position.x - body.center.x) <= body.halfExtents.x &&
               std::abs(position.z - body.center.z) <= body.halfExtents.z;
    }

    [[nodiscard]] WaterManager::WaveSample WaterManager::evaluate_wave(
        const WaterBodyDesc& body,
        const WaterQuery& query) noexcept
    {
        if(body.waveTile)
        {
            const auto value=body.waveTile->sample(double(query.position.x)-body.center.x,
                double(query.position.z)-body.center.z,query.simulationTimeSeconds);
            if(!value)return {.valid=false};
            return {body.surfaceHeight+value->height,
                {value->normal[0],value->normal[1],value->normal[2]}, {},
                {value->displacement_x,0.0f,value->displacement_z},value->foam};
        }
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
        // Preserve the original query's exact float operations when the
        // optional component is disabled.
        if (body.secondaryWave.amplitude == 0.0f)
            return {height, normal, velocity};

        const auto& secondary = body.secondaryWave;
        const double secondaryNumber = tau / static_cast<double>(secondary.wavelength);
        const double secondaryProjected =
            static_cast<double>(query.position.x - body.center.x) * secondary.direction.x +
            static_cast<double>(query.position.z - body.center.z) * secondary.direction.z;
        const double secondaryPhase = secondaryNumber * secondaryProjected -
            static_cast<double>(secondary.speed) * query.simulationTimeSeconds + secondary.phase;
        const float secondarySine = static_cast<float>(std::sin(secondaryPhase));
        const double secondaryCosine = std::cos(secondaryPhase);
        const float secondarySlope = secondary.amplitude *
            static_cast<float>(secondaryNumber * secondaryCosine);
        const float secondaryVelocity = secondary.speed * secondary.amplitude * static_cast<float>(secondaryCosine);
        const Float3 combinedNormal = epochengine::render_math::normalize({
            -slope * body.waveDirection.x - secondarySlope * secondary.direction.x,
            1.0f,
            -slope * body.waveDirection.z - secondarySlope * secondary.direction.z});
        return {
            height + secondary.amplitude * secondarySine,
            combinedNormal,
            {velocity.x + secondary.direction.x * secondaryVelocity,
             velocity.y - secondaryVelocity,
             velocity.z + secondary.direction.z * secondaryVelocity}
        };
    }

    [[nodiscard]] WaterManager::Slot* WaterManager::resolve(WaterBodyHandle handle) noexcept
    {
        if (!handle.valid() || handle.index >= slots_.size())
        {
            return nullptr;
        }
        Slot& slot = slots_[handle.index];
        return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
    }

    [[nodiscard]] const WaterManager::Slot* WaterManager::resolve(WaterBodyHandle handle) const noexcept
    {
        if (!handle.valid() || handle.index >= slots_.size())
        {
            return nullptr;
        }
        const Slot& slot = slots_[handle.index];
        return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
    }

    [[nodiscard]] std::size_t WaterManager::active_count() const noexcept
    {
        return slots_.size() - freeSlots_.size();
    }

    [[nodiscard]] WaterStampResult stamp_voxel_water(
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
