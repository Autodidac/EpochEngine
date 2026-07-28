/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <cstddef>
#include <cstdint>
#include <limits>

#include "../include/_epoch.stl_types.hpp"

export module voxel.field;

export namespace epochengine::voxel
{
    struct Float3
    {
        float x{};
        float y{};
        float z{};
    };

    struct CellCoord
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t z{};
    };

    struct ChunkCoord
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t z{};
    };

    enum class OccupancyClass : std::uint8_t
    {
        Empty,
        Boundary,
        Solid
    };

    enum class CellSemantic : std::uint16_t
    {
        None = 0,
        Geometry = 1u << 0u,
        Lighting = 1u << 1u,
        Navigation = 1u << 2u,
        Visibility = 1u << 3u,
        Smoke = 1u << 4u,
        Biome = 1u << 5u,
        Water = 1u << 6u,
        Atmosphere = 1u << 7u,
        ProceduralVegetation = 1u << 8u
    };

    [[nodiscard]] constexpr CellSemantic operator|(CellSemantic lhs, CellSemantic rhs) noexcept
    {
        return static_cast<CellSemantic>(
            static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
    }

    [[nodiscard]] constexpr bool has_semantic(CellSemantic flags, CellSemantic flag) noexcept
    {
        return (static_cast<std::uint16_t>(flags) & static_cast<std::uint16_t>(flag)) != 0u;
    }

    using MaterialId = std::uint32_t;
    using BiomeId = std::uint32_t;

    struct VoxelCell
    {
        float density{};
        MaterialId material{};
        BiomeId biome{};
        std::uint16_t light{};
        std::uint16_t navigationCost{};
        CellSemantic semantics{CellSemantic::None};
    };

    struct ChunkDesc
    {
        std::uint32_t cellsX{32};
        std::uint32_t cellsY{32};
        std::uint32_t cellsZ{32};
        float cellSizeMeters{1.0F};
        std::uint8_t lodLevel{};
    };

    struct ChunkKey
    {
        ChunkCoord coord{};
        std::uint8_t lodLevel{};
        std::uint64_t stableHash{};
    };

    struct LodPolicy
    {
        std::uint8_t nearLod{};
        std::uint8_t farLod{8};
        float nearMeters{32.0F};
        float farMeters{4096.0F};
    };

    [[nodiscard]] constexpr bool valid(const ChunkDesc& desc) noexcept
    {
        constexpr std::uint64_t maximumIndexedCells =
            static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)());
        constexpr std::uint32_t maximumSignedDimension =
            static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)());

        if (desc.cellsX == 0u || desc.cellsY == 0u || desc.cellsZ == 0u ||
            desc.cellsX > maximumSignedDimension ||
            desc.cellsY > maximumSignedDimension ||
            desc.cellsZ > maximumSignedDimension ||
            !(desc.cellSizeMeters > 0.0F) ||
            desc.cellSizeMeters > (std::numeric_limits<float>::max)())
        {
            return false;
        }

        const std::uint64_t plane =
            static_cast<std::uint64_t>(desc.cellsX) * desc.cellsY;
        return plane <= maximumIndexedCells &&
               desc.cellsZ <= maximumIndexedCells / plane;
    }

    [[nodiscard]] constexpr std::uint64_t dense_cell_count(const ChunkDesc& desc) noexcept
    {
        if (!valid(desc))
        {
            return 0;
        }
        return static_cast<std::uint64_t>(desc.cellsX) *
               static_cast<std::uint64_t>(desc.cellsY) *
               static_cast<std::uint64_t>(desc.cellsZ);
    }

    [[nodiscard]] constexpr std::uint64_t dense_cell_bytes(const ChunkDesc& desc) noexcept
    {
        return dense_cell_count(desc) * static_cast<std::uint64_t>(sizeof(VoxelCell));
    }

    [[nodiscard]] constexpr bool contains(const ChunkDesc& desc, CellCoord cell) noexcept
    {
        return cell.x >= 0 && cell.y >= 0 && cell.z >= 0 &&
               static_cast<std::uint32_t>(cell.x) < desc.cellsX &&
               static_cast<std::uint32_t>(cell.y) < desc.cellsY &&
               static_cast<std::uint32_t>(cell.z) < desc.cellsZ;
    }

    [[nodiscard]] constexpr OccupancyClass classify_density(float density) noexcept
    {
        if (density <= -0.01F)
        {
            return OccupancyClass::Empty;
        }

        if (density >= 0.01F)
        {
            return OccupancyClass::Solid;
        }

        return OccupancyClass::Boundary;
    }

    [[nodiscard]] constexpr std::int32_t floor_div(std::int32_t value, std::int32_t divisor) noexcept
    {
        const std::int32_t quotient = value / divisor;
        const std::int32_t remainder = value % divisor;
        return (remainder != 0 && ((remainder < 0) != (divisor < 0))) ? quotient - 1 : quotient;
    }

    [[nodiscard]] constexpr ChunkCoord chunk_for_cell(CellCoord cell, const ChunkDesc& desc) noexcept
    {
        const auto x = static_cast<std::int32_t>(desc.cellsX);
        const auto y = static_cast<std::int32_t>(desc.cellsY);
        const auto z = static_cast<std::int32_t>(desc.cellsZ);
        return {floor_div(cell.x, x), floor_div(cell.y, y), floor_div(cell.z, z)};
    }

    [[nodiscard]] constexpr std::uint64_t mix_hash(std::uint64_t hash, std::uint32_t value) noexcept
    {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ull;
        return hash;
    }

    [[nodiscard]] constexpr std::uint64_t stable_chunk_hash(ChunkCoord coord, std::uint8_t lodLevel) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        hash = mix_hash(hash, static_cast<std::uint32_t>(coord.x));
        hash = mix_hash(hash, static_cast<std::uint32_t>(coord.y));
        hash = mix_hash(hash, static_cast<std::uint32_t>(coord.z));
        hash = mix_hash(hash, static_cast<std::uint32_t>(lodLevel));
        return hash;
    }

    [[nodiscard]] constexpr ChunkKey make_chunk_key(ChunkCoord coord, std::uint8_t lodLevel) noexcept
    {
        return {coord, lodLevel, stable_chunk_hash(coord, lodLevel)};
    }

    [[nodiscard]] constexpr bool vegetation_semantics(CellSemantic semantics) noexcept
    {
        return has_semantic(semantics, CellSemantic::ProceduralVegetation);
    }
}
