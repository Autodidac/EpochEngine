module;

#include "../include/_epoch.stl_types.hpp"
#include <unordered_map>

module epoch.systems;

import core.format;
import core.log;

namespace epoch::systems
{
    namespace
    {
        // Use std::string as the key type for unordered_map.
        // It avoids having to provide std::hash<epoch::string>.
        using Key = std::string;

        [[nodiscard]] inline Key make_key(epoch::string_view v)
        {
            return Key{ v.data ? v.data : "", v.size };
        }

        [[nodiscard]] inline Key make_key(const epoch::string& s)
        {
            return s.impl;
        }

        struct Entry
        {
            struct Deleter
            {
                SystemFactory::destroy_fn destroy = nullptr;

                void operator()(ISystem* ptr) const noexcept
                {
                    if (!ptr) return;
                    if (destroy) destroy(ptr);
                    else delete ptr;
                }
            };

            epoch::string name{};
            SystemFactory factory{};
            std::unique_ptr<ISystem, Deleter> system{};
        };

        struct RegistryState
        {
            std::vector<Entry> entries{};
            std::unordered_map<Key, std::size_t> index{};
            std::vector<ISystem*> order{};
            bool resolved = false;
            bool initialized = false;
        };

        RegistryState& state()
        {
            static RegistryState instance{};
            return instance;
        }
    }

    Registry& Registry::instance() noexcept
    {
        static Registry instance{};
        return instance;
    }

    bool Registry::register_system(SystemFactory factory) noexcept
    {
        auto& data = state();
        if (!factory.create)
            return false;

        std::unique_ptr<ISystem, Entry::Deleter> created{
            factory.create(),
            Entry::Deleter{ factory.destroy }
        };

        if (!created)
            return false;

        epoch::string name{ created->name() };
        if (name.empty())
            return false;

        const Key key = make_key(name);
        if (data.index.contains(key))
            return false;

        const std::size_t entry_index = data.entries.size();
        data.index.emplace(key, entry_index);
        data.entries.push_back(Entry{ std::move(name), factory, std::move(created) });

        data.resolved = false;
        data.initialized = false;
        return true;
    }

    ISystem* Registry::find(epoch::string_view name) noexcept
    {
        auto& data = state();
        auto it = data.index.find(make_key(name));
        if (it == data.index.end())
            return nullptr;

        return data.entries[it->second].system.get();
    }

    array_view<ISystem* const> Registry::ordered_systems() const noexcept
    {
        const auto& data = state();

        // vector<ISystem*> -> span<ISystem* const> via pointer+size.
        return array_view<ISystem* const>{
            data.order.empty() ? nullptr : data.order.data(),
                data.order.size()
        };
    }

    bool Registry::resolve_order() noexcept
    {
        auto& data = state();
        data.order.clear();

        if (data.entries.empty())
        {
            data.resolved = true;
            return true;
        }

        const std::size_t count = data.entries.size();
        std::vector<std::size_t> indegree(count, 0);
        std::vector<std::vector<std::size_t>> outgoing(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto deps = data.entries[i].system->dependencies();
            for (const auto& dep : deps)
            {
                const Key dep_key = make_key(dep);
                auto it = data.index.find(dep_key);
                if (it == data.index.end())
                {
                    core::log::write(core::log::level::error, "systems",
                        core::format::str("system '{}' depends on unknown '{}'",
                            data.entries[i].name, dep));
                    data.resolved = false;
                    return false;
                }

                if (it->second == i)
                {
                    core::log::write(core::log::level::error, "systems",
                        core::format::str("system '{}' cannot depend on itself",
                            data.entries[i].name));
                    data.resolved = false;
                    return false;
                }

                outgoing[it->second].push_back(i);
                ++indegree[i];
            }
        }

        std::deque<std::size_t> ready{};
        for (std::size_t i = 0; i < count; ++i)
        {
            if (indegree[i] == 0)
                ready.push_back(i);
        }

        while (!ready.empty())
        {
            const std::size_t idx = ready.front();
            ready.pop_front();
            data.order.push_back(data.entries[idx].system.get());

            for (const std::size_t dependent : outgoing[idx])
            {
                if (--indegree[dependent] == 0)
                    ready.push_back(dependent);
            }
        }

        if (data.order.size() != count)
        {
            core::log::write(core::log::level::error, "systems",
                "dependency cycle detected in system registry");
            data.resolved = false;
            return false;
        }

        data.resolved = true;
        return true;
    }

    bool Registry::initialize() noexcept
    {
        auto& data = state();
        if (data.initialized)
            return true;

        if (!data.resolved && !resolve_order())
            return false;

        for (auto* system : data.order)
            system->on_init();

        data.initialized = true;
        return true;
    }

    void Registry::update(double dt_seconds) noexcept
    {
        auto& data = state();
        if (!data.initialized)
            return;

        for (auto* system : data.order)
            system->on_update(dt_seconds);
    }

    void Registry::shutdown() noexcept
    {
        auto& data = state();
        if (!data.initialized)
            return;

        for (auto it = data.order.rbegin(); it != data.order.rend(); ++it)
            (*it)->on_shutdown();

        data.initialized = false;
    }
} // namespace epoch::systems
