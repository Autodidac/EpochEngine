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

#include "../include/_epoch.stl_types.hpp"

export module epoch.ecs;

export namespace epochengine::ecs
{
    // 32-bit index + 32-bit generation fits in 64 and is plenty for engine work.
    struct entity
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;

        constexpr bool operator==(const entity&) const noexcept = default;
        [[nodiscard]] constexpr bool valid() const noexcept { return index != 0; } // 0 reserved as null
    };

    inline constexpr entity null_entity{};

    struct world_desc
    {
        std::uint32_t max_entities = 64 * 1024;
        bool allow_growth = false; // Tier-1: keep false.
    };

    namespace detail
    {
        inline constexpr std::uint32_t invalid_u32 = 0xFFFFFFFFu;
    }

    // Sparse-set storage for a component type T.
    template <class T>
    class storage
    {
    public:
        using value_type = T;

        void reserve(std::uint32_t max_entities)
        {
            // sparse maps entity.index -> dense index
            _sparse.assign(static_cast<std::size_t>(max_entities + 1u), detail::invalid_u32);
            _dense_entities.reserve(max_entities / 4u);
            _dense_values.reserve(max_entities / 4u);
        }

        [[nodiscard]] bool has(entity e) const noexcept
        {
            if (!e.valid() || e.index >= _sparse.size()) return false;
            const std::uint32_t di = _sparse[e.index];
            return di != detail::invalid_u32 && di < _dense_entities.size() && _dense_entities[di] == e;
        }

        T* get(entity e) noexcept
        {
            if (!has(e)) return nullptr;
            return &_dense_values[_sparse[e.index]];
        }

        const T* get(entity e) const noexcept
        {
            if (!has(e)) return nullptr;
            return &_dense_values[_sparse[e.index]];
        }

        template <class... Args>
        T& emplace(entity e, Args&&... args)
        {
            // overwrite existing
            if (T* existing = get(e))
            {
                *existing = T{ std::forward<Args>(args)... };
                return *existing;
            }

            const std::uint32_t di = static_cast<std::uint32_t>(_dense_entities.size());
            if (e.index >= _sparse.size())
                _sparse.resize(static_cast<std::size_t>(e.index) + 1u, detail::invalid_u32);

            _sparse[e.index] = di;
            _dense_entities.push_back(e);
            _dense_values.emplace_back(std::forward<Args>(args)...);
            return _dense_values.back();
        }

        bool remove(entity e) noexcept
        {
            if (!has(e)) return false;

            const std::uint32_t di = _sparse[e.index];
            const std::uint32_t last = static_cast<std::uint32_t>(_dense_entities.size() - 1u);

            if (di != last)
            {
                _dense_entities[di] = _dense_entities[last];
                _dense_values[di] = std::move(_dense_values[last]);
                _sparse[_dense_entities[di].index] = di;
            }

            _dense_entities.pop_back();
            _dense_values.pop_back();
            _sparse[e.index] = detail::invalid_u32;
            return true;
        }

        [[nodiscard]] std::span<const entity> entities() const noexcept { return _dense_entities; }
        [[nodiscard]] std::span<T> values() noexcept { return _dense_values; }
        [[nodiscard]] std::span<const T> values() const noexcept { return _dense_values; }

    private:
        std::vector<std::uint32_t> _sparse{};
        std::vector<entity> _dense_entities{};
        std::vector<T> _dense_values{};
    };

    // World with entity lifetime + free-list. Component storages live outside in your systems,
    // or you can aggregate them in a "registry" later.
    class world
    {
    public:
        explicit world(world_desc d = {}) : _desc(d)
        {
            _generations.resize(static_cast<std::size_t>(_desc.max_entities + 1u), 1u);
            _alive.resize(static_cast<std::size_t>(_desc.max_entities + 1u), false);

            _free.reserve(_desc.max_entities);
            for (std::uint32_t i = _desc.max_entities; i >= 1u; --i)
                _free.push_back(i);
        }

        [[nodiscard]] std::uint32_t capacity() const noexcept { return _desc.max_entities; }

        [[nodiscard]] entity create()
        {
            if (_free.empty())
            {
                if (!_desc.allow_growth) return null_entity;
                // growth policy (Tier-2/3): double
                const std::uint32_t old = _desc.max_entities;
                const std::uint32_t neu = old ? old * 2u : 1024u;
                _desc.max_entities = neu;
                _generations.resize(static_cast<std::size_t>(neu + 1u), 1u);
                _alive.resize(static_cast<std::size_t>(neu + 1u), false);
                for (std::uint32_t i = neu; i > old; --i)
                    _free.push_back(i);
            }

            const std::uint32_t idx = _free.back();
            _free.pop_back();
            _alive[idx] = true;
            return entity{ idx, _generations[idx] };
        }

        void destroy(entity e) noexcept
        {
            if (!alive(e)) return;
            _alive[e.index] = false;
            std::uint32_t& generation = _generations[e.index];
            ++generation;
            if (generation == 0u)
                generation = 1u;
            _free.push_back(e.index);
        }

        [[nodiscard]] bool alive(entity e) const noexcept
        {
            return e.valid()
                && e.index < _alive.size()
                && _alive[e.index]
                && _generations[e.index] == e.generation;
        }

    private:
        world_desc _desc{};
        std::vector<std::uint32_t> _generations{};
        std::vector<bool> _alive{};
        std::vector<std::uint32_t> _free{};
    };
} // namespace epochengine::ecs
