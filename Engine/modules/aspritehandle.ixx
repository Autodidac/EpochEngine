module;

// no macros in global module fragment except directives
#undef max

export module aspritehandle;

import <cstdint>;
import <compare>;
import <limits>;
import <cstddef>;

export namespace almondnamespace
{
    struct SpriteHandle
    {
        std::uint32_t id =
            std::numeric_limits<std::uint32_t>::max();

        std::uint32_t generation = 0;
        std::uint32_t atlasIndex = 0;
        std::uint32_t localIndex = 0;

        constexpr SpriteHandle() noexcept = default;

        constexpr SpriteHandle(
            std::uint32_t id_,
            std::uint32_t generation_) noexcept
            : id(id_)
            , generation(generation_)
            , atlasIndex(0)
            , localIndex(0)
        {
        }

        constexpr SpriteHandle(
            std::uint32_t id_,
            std::uint32_t generation_,
            std::uint32_t atlasIndex_,
            std::uint32_t localIndex_) noexcept
            : id(id_)
            , generation(generation_)
            , atlasIndex(atlasIndex_)
            , localIndex(localIndex_)
        {
        }

        [[nodiscard]]
        constexpr bool is_valid() const noexcept
        {
            return id != std::numeric_limits<std::uint32_t>::max();
        }

        [[nodiscard]]
        constexpr std::uint64_t pack() const noexcept
        {
            // Layout: [atlasIndex:16][localIndex:16][generation:16][id:16]
            return (static_cast<std::uint64_t>(atlasIndex & 0xFFFF) << 48)
                | (static_cast<std::uint64_t>(localIndex & 0xFFFF) << 32)
                | (static_cast<std::uint64_t>(generation & 0xFFFF) << 16)
                | (static_cast<std::uint64_t>(id & 0xFFFF));
        }

        static constexpr SpriteHandle invalid() noexcept
        {
            return {};
        }

        auto operator<=>(const SpriteHandle&) const = default;

        explicit constexpr operator std::uint32_t() const noexcept
        {
            return id;
        }
    };

    struct SpriteHandleHash
    {
        [[nodiscard]]
        constexpr std::size_t operator()(
            const SpriteHandle& handle) const noexcept
        {
            return static_cast<std::size_t>(handle.pack());
        }
    };
}
