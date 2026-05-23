module;

#include <cstdint>

#include "../include/_epoch.stl_types.hpp"

export module voxel.trace;

import voxel.field;

export namespace epoch::voxel
{
    enum class TracePurpose : std::uint8_t
    {
        Rendering,
        Lighting,
        Navigation,
        Visibility,
        Smoke,
        AiQuery
    };

    enum class TraceQuality : std::uint8_t
    {
        Coarse,
        Balanced,
        High,
        Reference
    };

    struct TraceRay
    {
        Float3 origin{};
        Float3 direction{0.0F, 1.0F, 0.0F};
    };

    struct TraceBudget
    {
        std::uint32_t maxSteps{256};
        float maxDistanceMeters{1024.0F};
        float minStepMeters{0.05F};
    };

    struct TraceRequest
    {
        TraceRay ray{};
        TraceBudget budget{};
        TracePurpose purpose{TracePurpose::Rendering};
        TraceQuality quality{TraceQuality::Balanced};
        std::uint8_t requestedLod{};
    };

    struct TraceHit
    {
        bool hit{};
        Float3 position{};
        Float3 normal{};
        CellCoord cell{};
        MaterialId material{};
        float distanceMeters{};
        OccupancyClass occupancy{OccupancyClass::Empty};
    };

    struct TraceResult
    {
        TraceHit closest{};
        std::uint32_t steps{};
        bool budgetExhausted{};
    };

    struct PathSample
    {
        Float3 position{};
        CellCoord cell{};
        float cost{};
        bool blocked{};
    };

    struct PathQuery
    {
        Float3 start{};
        Float3 goal{};
        TraceQuality quality{TraceQuality::Balanced};
        std::uint32_t maxSamples{512};
    };

    struct QueryCapabilities
    {
        bool geometry{};
        bool lighting{};
        bool navigation{};
        bool visibility{};
        bool smoke{};
        bool ai{};
    };

    [[nodiscard]] constexpr bool non_zero(Float3 value) noexcept
    {
        return value.x != 0.0F || value.y != 0.0F || value.z != 0.0F;
    }

    [[nodiscard]] constexpr bool valid(const TraceBudget& budget) noexcept
    {
        return budget.maxSteps > 0u && budget.maxDistanceMeters > 0.0F && budget.minStepMeters > 0.0F;
    }

    [[nodiscard]] constexpr bool valid(const TraceRequest& request) noexcept
    {
        return non_zero(request.ray.direction) && valid(request.budget);
    }

    [[nodiscard]] constexpr TraceResult miss(std::uint32_t steps = 0u, bool budgetExhausted = false) noexcept
    {
        return {{}, steps, budgetExhausted};
    }

    [[nodiscard]] constexpr QueryCapabilities capabilities_for(TracePurpose purpose) noexcept
    {
        switch (purpose)
        {
        case TracePurpose::Rendering:
            return {.geometry = true, .lighting = true, .visibility = true};
        case TracePurpose::Lighting:
            return {.geometry = true, .lighting = true, .visibility = true};
        case TracePurpose::Navigation:
            return {.geometry = true, .navigation = true, .visibility = true, .ai = true};
        case TracePurpose::Visibility:
            return {.geometry = true, .visibility = true};
        case TracePurpose::Smoke:
            return {.geometry = true, .visibility = true, .smoke = true};
        case TracePurpose::AiQuery:
            return {.geometry = true, .navigation = true, .visibility = true, .smoke = true, .ai = true};
        }

        return {};
    }

    [[nodiscard]] constexpr bool is_core_trace_purpose(TracePurpose purpose) noexcept
    {
        return capabilities_for(purpose).geometry || capabilities_for(purpose).ai;
    }
}
