// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

export module voxel.storage;

import voxel.field;

export namespace epochengine::voxel
{
    struct VoxelWrite final
    {
        CellCoord coord{};
        VoxelCell cell{};
    };

    struct VoxelRecord final
    {
        CellCoord coord{};
        VoxelCell cell{};
    };

    struct FieldSnapshot final
    {
        ChunkDesc chunkDesc{};
        std::uint64_t revision{};
        std::uint64_t contentHash{};
        std::vector<VoxelRecord> cells{};
    };

    struct SparseFieldMetrics final
    {
        std::size_t allocatedChunkCount{};
        std::size_t storedCellCount{};
        std::size_t solidCellCount{};
        std::size_t boundaryCellCount{};
        std::size_t waterCellCount{};
        std::uint64_t denseEquivalentCellCount{};
        std::uint64_t denseEquivalentBytes{};
        std::uint64_t approximateSparseBytes{};
        std::uint64_t revision{};
        std::uint64_t contentHash{};
    };

    struct SparseFieldLimits final
    {
        std::size_t maximumChunks{4'096};
        std::size_t maximumStoredCells{1'000'000};
        std::uint64_t maximumApproximateBytes{256ull * 1024ull * 1024ull};
    };

    enum class VoxelWriteStatus : std::uint8_t
    {
        Applied,
        Unchanged,
        Invalid,
        BudgetExceeded
    };

    [[nodiscard]] constexpr bool same_coord(CellCoord lhs, CellCoord rhs) noexcept
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    [[nodiscard]] constexpr bool same_coord(ChunkCoord lhs, ChunkCoord rhs) noexcept
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    [[nodiscard]] constexpr CellCoord local_cell(
        CellCoord world,
        ChunkCoord chunk,
        const ChunkDesc& desc) noexcept
    {
        return {
            world.x - chunk.x * static_cast<std::int32_t>(desc.cellsX),
            world.y - chunk.y * static_cast<std::int32_t>(desc.cellsY),
            world.z - chunk.z * static_cast<std::int32_t>(desc.cellsZ)
        };
    }

    [[nodiscard]] constexpr std::uint32_t local_cell_index(
        CellCoord local,
        const ChunkDesc& desc) noexcept
    {
        const std::uint64_t index =
            static_cast<std::uint64_t>(static_cast<std::uint32_t>(local.x)) +
            static_cast<std::uint64_t>(static_cast<std::uint32_t>(local.y)) * desc.cellsX +
            static_cast<std::uint64_t>(static_cast<std::uint32_t>(local.z)) *
                desc.cellsX * desc.cellsY;
        return static_cast<std::uint32_t>(index);
    }

    [[nodiscard]] constexpr CellCoord cell_from_local_index(
        std::uint32_t index,
        ChunkCoord chunk,
        const ChunkDesc& desc) noexcept
    {
        const std::uint64_t plane =
            static_cast<std::uint64_t>(desc.cellsX) * desc.cellsY;
        const std::uint32_t z = static_cast<std::uint32_t>(index / plane);
        const std::uint32_t remainder = static_cast<std::uint32_t>(index % plane);
        const std::uint32_t y = remainder / desc.cellsX;
        const std::uint32_t x = remainder % desc.cellsX;
        return {
            chunk.x * static_cast<std::int32_t>(desc.cellsX) + static_cast<std::int32_t>(x),
            chunk.y * static_cast<std::int32_t>(desc.cellsY) + static_cast<std::int32_t>(y),
            chunk.z * static_cast<std::int32_t>(desc.cellsZ) + static_cast<std::int32_t>(z)
        };
    }

    [[nodiscard]] inline bool valid(const VoxelCell& cell) noexcept
    {
        return std::isfinite(cell.density);
    }

    [[nodiscard]] constexpr bool is_implicit_empty(const VoxelCell& cell) noexcept
    {
        return cell.density == 0.0f &&
               cell.material == 0 &&
               cell.biome == 0 &&
               cell.light == 0 &&
               cell.navigationCost == 0 &&
               cell.semantics == CellSemantic::None;
    }

    [[nodiscard]] constexpr bool same_cell(const VoxelCell& lhs, const VoxelCell& rhs) noexcept
    {
        return lhs.density == rhs.density &&
               lhs.material == rhs.material &&
               lhs.biome == rhs.biome &&
               lhs.light == rhs.light &&
               lhs.navigationCost == rhs.navigationCost &&
               lhs.semantics == rhs.semantics;
    }

    class SparseVoxelField final
    {
    public:
        explicit SparseVoxelField(
            ChunkDesc desc = {},
            SparseFieldLimits limits = {}) noexcept
            : chunkDesc_(voxel::valid(desc) ? desc : ChunkDesc{}),
              limits_(valid_limits(limits) ? limits : SparseFieldLimits{})
        {
        }

        [[nodiscard]] const ChunkDesc& chunk_desc() const noexcept
        {
            return chunkDesc_;
        }

        [[nodiscard]] std::uint64_t revision() const noexcept
        {
            return revision_;
        }

        [[nodiscard]] const SparseFieldLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] VoxelWriteStatus write_status(CellCoord coord, VoxelCell cell)
        {
            const VoxelWriteStatus status = apply_write(coord, cell);
            if (status == VoxelWriteStatus::Applied)
            {
                ++revision_;
            }
            return status;
        }

        [[nodiscard]] bool write(CellCoord coord, VoxelCell cell)
        {
            const VoxelWriteStatus status = write_status(coord, cell);
            return status == VoxelWriteStatus::Applied ||
                   status == VoxelWriteStatus::Unchanged;
        }

        [[nodiscard]] bool write_many(std::span<const VoxelWrite> writes)
        {
            SparseVoxelField candidate = *this;
            bool changed = false;
            for (const VoxelWrite& write : writes)
            {
                const VoxelWriteStatus status = candidate.apply_write(write.coord, write.cell);
                if (status == VoxelWriteStatus::Invalid ||
                    status == VoxelWriteStatus::BudgetExceeded)
                {
                    return false;
                }
                changed = changed || status == VoxelWriteStatus::Applied;
            }

            if (changed)
            {
                candidate.revision_ = revision_ + 1;
                *this = std::move(candidate);
            }
            return true;
        }

        [[nodiscard]] bool erase(CellCoord coord)
        {
            return write(coord, {});
        }

        [[nodiscard]] std::optional<VoxelCell> read(CellCoord coord) const noexcept
        {
            const ChunkCoord chunkCoord = chunk_for_cell(coord, chunkDesc_);
            const ChunkOrderKey key{chunkCoord.x, chunkCoord.y, chunkCoord.z};
            const auto chunkIt = chunks_.find(key);
            if (chunkIt == chunks_.end())
            {
                return std::nullopt;
            }

            const CellCoord local = local_cell(coord, chunkCoord, chunkDesc_);
            const std::uint32_t index = local_cell_index(local, chunkDesc_);
            const auto cellIt = chunkIt->second.cells.find(index);
            if (cellIt == chunkIt->second.cells.end())
            {
                return std::nullopt;
            }
            return cellIt->second;
        }

        [[nodiscard]] VoxelCell read_or_empty(CellCoord coord) const noexcept
        {
            const auto value = read(coord);
            return value.has_value() ? *value : VoxelCell{};
        }

        [[nodiscard]] bool occupied(CellCoord coord) const noexcept
        {
            const auto value = read(coord);
            if (!value.has_value())
            {
                return false;
            }
            return classify_density(value->density) != OccupancyClass::Empty ||
                   value->semantics != CellSemantic::None;
        }

        [[nodiscard]] bool has_semantic(CellCoord coord, CellSemantic semantic) const noexcept
        {
            const auto value = read(coord);
            return value.has_value() && voxel::has_semantic(value->semantics, semantic);
        }

        [[nodiscard]] FieldSnapshot snapshot() const
        {
            FieldSnapshot result{};
            result.chunkDesc = chunkDesc_;
            result.revision = revision_;
            result.cells.reserve(stored_cell_count());

            std::uint64_t hash = 14695981039346656037ull;
            hash = mix_hash(hash, chunkDesc_.cellsX);
            hash = mix_hash(hash, chunkDesc_.cellsY);
            hash = mix_hash(hash, chunkDesc_.cellsZ);
            hash = mix_hash(hash, std::bit_cast<std::uint32_t>(chunkDesc_.cellSizeMeters));
            hash = mix_hash(hash, chunkDesc_.lodLevel);
            for (const auto& [key, chunk] : chunks_)
            {
                hash = mix_hash(hash, static_cast<std::uint32_t>(key.x));
                hash = mix_hash(hash, static_cast<std::uint32_t>(key.y));
                hash = mix_hash(hash, static_cast<std::uint32_t>(key.z));
                for (const auto& [index, cell] : chunk.cells)
                {
                    const CellCoord coord = cell_from_local_index(index, chunk.coord, chunkDesc_);
                    result.cells.push_back({coord, cell});
                    hash = mix_hash(hash, index);
                    hash = mix_hash(hash, std::bit_cast<std::uint32_t>(cell.density));
                    hash = mix_hash(hash, cell.material);
                    hash = mix_hash(hash, cell.biome);
                    hash = mix_hash(hash, cell.light);
                    hash = mix_hash(hash, cell.navigationCost);
                    hash = mix_hash(hash, static_cast<std::uint16_t>(cell.semantics));
                }
            }
            result.contentHash = hash;
            return result;
        }

        [[nodiscard]] SparseFieldMetrics metrics() const
        {
            SparseFieldMetrics result{};
            result.allocatedChunkCount = chunks_.size();
            result.revision = revision_;
            for (const auto& [key, chunk] : chunks_)
            {
                (void)key;
                result.storedCellCount += chunk.cells.size();
                for (const auto& [index, cell] : chunk.cells)
                {
                    (void)index;
                    switch (classify_density(cell.density))
                    {
                    case OccupancyClass::Solid: ++result.solidCellCount; break;
                    case OccupancyClass::Boundary: ++result.boundaryCellCount; break;
                    case OccupancyClass::Empty: break;
                    }
                    if (voxel::has_semantic(cell.semantics, CellSemantic::Water))
                    {
                        ++result.waterCellCount;
                    }
                }
            }
            result.denseEquivalentCellCount =
                static_cast<std::uint64_t>(result.allocatedChunkCount) * dense_cell_count(chunkDesc_);
            result.denseEquivalentBytes =
                static_cast<std::uint64_t>(result.allocatedChunkCount) * dense_cell_bytes(chunkDesc_);
            result.approximateSparseBytes = approximate_storage_bytes(
                result.allocatedChunkCount,
                result.storedCellCount);
            result.contentHash = snapshot().contentHash;
            return result;
        }

        void clear() noexcept
        {
            if (!chunks_.empty())
            {
                chunks_.clear();
                storedCellCount_ = 0;
                ++revision_;
            }
        }

    private:
        struct ChunkOrderKey final
        {
            std::int32_t x{};
            std::int32_t y{};
            std::int32_t z{};

            friend constexpr bool operator<(const ChunkOrderKey& lhs, const ChunkOrderKey& rhs) noexcept
            {
                if (lhs.x != rhs.x) return lhs.x < rhs.x;
                if (lhs.y != rhs.y) return lhs.y < rhs.y;
                return lhs.z < rhs.z;
            }
        };

        struct Chunk final
        {
            ChunkCoord coord{};
            std::map<std::uint32_t, VoxelCell> cells{};
        };

        [[nodiscard]] static constexpr bool valid_limits(const SparseFieldLimits& limits) noexcept
        {
            return limits.maximumChunks > 0 &&
                   limits.maximumStoredCells > 0 &&
                   limits.maximumApproximateBytes > 0;
        }

        [[nodiscard]] static constexpr std::uint64_t approximate_storage_bytes(
            std::size_t chunkCount,
            std::size_t cellCount) noexcept
        {
            constexpr std::uint64_t mapNodeLinks = 4ull * sizeof(void*);
            const std::uint64_t chunkBytes =
                static_cast<std::uint64_t>(chunkCount) *
                (sizeof(ChunkOrderKey) + sizeof(Chunk) + mapNodeLinks);
            const std::uint64_t cellBytes =
                static_cast<std::uint64_t>(cellCount) *
                (sizeof(std::uint32_t) + sizeof(VoxelCell) + mapNodeLinks);
            return chunkBytes + cellBytes;
        }

        [[nodiscard]] bool can_allocate(bool newChunk, bool newCell) const noexcept
        {
            const std::size_t chunkCount = chunks_.size() + (newChunk ? 1u : 0u);
            const std::size_t cellCount = storedCellCount_ + (newCell ? 1u : 0u);
            return chunkCount <= limits_.maximumChunks &&
                   cellCount <= limits_.maximumStoredCells &&
                   approximate_storage_bytes(chunkCount, cellCount) <=
                       limits_.maximumApproximateBytes;
        }

        [[nodiscard]] VoxelWriteStatus apply_write(CellCoord coord, const VoxelCell& cell)
        {
            if (!voxel::valid(cell))
            {
                return VoxelWriteStatus::Invalid;
            }

            const ChunkCoord chunkCoord = chunk_for_cell(coord, chunkDesc_);
            const CellCoord local = local_cell(coord, chunkCoord, chunkDesc_);
            const std::uint32_t index = local_cell_index(local, chunkDesc_);
            const ChunkOrderKey key{chunkCoord.x, chunkCoord.y, chunkCoord.z};
            auto chunkIt = chunks_.find(key);

            if (is_implicit_empty(cell))
            {
                if (chunkIt == chunks_.end())
                {
                    return VoxelWriteStatus::Unchanged;
                }
                const auto cellIt = chunkIt->second.cells.find(index);
                if (cellIt == chunkIt->second.cells.end())
                {
                    return VoxelWriteStatus::Unchanged;
                }
                chunkIt->second.cells.erase(cellIt);
                --storedCellCount_;
                if (chunkIt->second.cells.empty())
                {
                    chunks_.erase(chunkIt);
                }
                return VoxelWriteStatus::Applied;
            }

            const bool newChunk = chunkIt == chunks_.end();
            const bool newCell =
                newChunk || chunkIt->second.cells.find(index) == chunkIt->second.cells.end();
            if (newCell && !can_allocate(newChunk, true))
            {
                return VoxelWriteStatus::BudgetExceeded;
            }

            if (newChunk)
            {
                auto [insertedIt, inserted] = chunks_.try_emplace(key);
                (void)inserted;
                chunkIt = insertedIt;
                chunkIt->second.coord = chunkCoord;
            }

            const auto existing = chunkIt->second.cells.find(index);
            if (existing != chunkIt->second.cells.end())
            {
                if (same_cell(existing->second, cell))
                {
                    return VoxelWriteStatus::Unchanged;
                }
                existing->second = cell;
                return VoxelWriteStatus::Applied;
            }

            chunkIt->second.cells.emplace(index, cell);
            ++storedCellCount_;
            return VoxelWriteStatus::Applied;
        }

        [[nodiscard]] std::size_t stored_cell_count() const noexcept
        {
            std::size_t result{};
            for (const auto& [key, chunk] : chunks_)
            {
                (void)key;
                result += chunk.cells.size();
            }
            return result;
        }

        ChunkDesc chunkDesc_{};
        SparseFieldLimits limits_{};
        std::map<ChunkOrderKey, Chunk> chunks_{};
        std::size_t storedCellCount_{};
        std::uint64_t revision_{};
    };
}