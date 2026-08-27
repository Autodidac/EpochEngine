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

#include "../include/core.stl_types.hpp"
#include <chrono>
#include <unordered_map>

module systems.registry;

import core.format;
import core.log;

namespace epochengine::systems
{
    namespace
    {
        // Use std::string as the key type for unordered_map.
        // It avoids having to provide std::hash<epochengine::string>.
        using Key = std::string;

        [[nodiscard]] inline Key make_key(epochengine::string_view v)
        {
            return Key{ v.data ? v.data : "", v.size };
        }

        [[nodiscard]] inline Key make_key(const epochengine::string& s)
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

            epochengine::string name{};
            SystemFactory factory{};
            std::unique_ptr<ISystem, Deleter> system{ nullptr, Deleter{} };
            SystemLifecycle lifecycle{SystemLifecycle::registered};
            std::size_t execution_order{};
            std::uint64_t update_count{};
            std::uint64_t last_update_nanoseconds{};
            std::uint64_t peak_update_nanoseconds{};
            std::uint64_t total_update_nanoseconds{};
        };

        struct RegistryState
        {
            std::vector<Entry> entries{};
            std::unordered_map<Key, std::size_t> index{};
            std::vector<ISystem*> order{};
            std::vector<std::size_t> order_indices{};
            bool resolved = false;
            bool initialized = false;
            std::uint64_t revision{1u};
            std::uint64_t frame_count{};
            std::uint64_t last_frame_nanoseconds{};
            std::uint64_t peak_frame_nanoseconds{};
            bool diagnostics_sampling{};
            std::string last_error{};
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

        epochengine::string name{ created->name() };
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
        data.last_error.clear();
        ++data.revision;
        return true;
    }

    ISystem* Registry::find(epochengine::string_view name) noexcept
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
        data.order_indices.clear();
        data.last_error.clear();

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
                    data.last_error = make_key(core::format::str("System {} depends on an unknown system.", data.entries[i].name));
                    data.resolved = false;
                    ++data.revision;
                    return false;
                }

                if (it->second == i)
                {
                    core::log::write(core::log::level::error, "systems",
                        core::format::str("system '{}' cannot depend on itself",
                            data.entries[i].name));
                    data.last_error = make_key(core::format::str("System {} cannot depend on itself.", data.entries[i].name));
                    data.resolved = false;
                    ++data.revision;
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
            data.order_indices.push_back(idx);

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
            data.last_error = "Dependency cycle detected in system registry.";
            data.resolved = false;
            ++data.revision;
            return false;
        }

        for (std::size_t order = 0; order < data.order.size(); ++order)
        {
            const auto found = data.index.find(make_key(data.order[order]->name()));
            if (found != data.index.end())
                data.entries[found->second].execution_order = order;
        }
        data.resolved = true;
        ++data.revision;
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
        {
            system->on_init();
            const auto found = data.index.find(make_key(system->name()));
            if (found != data.index.end())
                data.entries[found->second].lifecycle = SystemLifecycle::initialized;
        }

        data.initialized = true;
        ++data.revision;
        return true;
    }

    void Registry::update(double dt_seconds) noexcept
    {
        auto& data = state();
        if (!data.initialized)
            return;

        if (!data.diagnostics_sampling)
        {
            for (auto* system : data.order)
                system->on_update(dt_seconds);
            return;
        }

        const auto frameStarted = std::chrono::steady_clock::now();
        for (std::size_t order = 0; order < data.order.size(); ++order)
        {
            const auto started = std::chrono::steady_clock::now();
            data.order[order]->on_update(dt_seconds);
            const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started).count());
            auto& entry = data.entries[data.order_indices[order]];
            ++entry.update_count;
            entry.last_update_nanoseconds = elapsed;
            entry.peak_update_nanoseconds = (std::max)(entry.peak_update_nanoseconds, elapsed);
            entry.total_update_nanoseconds += elapsed;
        }
        data.last_frame_nanoseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - frameStarted).count());
        data.peak_frame_nanoseconds = (std::max)(data.peak_frame_nanoseconds, data.last_frame_nanoseconds);
        ++data.frame_count;
    }

    void Registry::shutdown() noexcept
    {
        auto& data = state();
        if (!data.initialized)
            return;

        for (auto it = data.order.rbegin(); it != data.order.rend(); ++it)
        {
            (*it)->on_shutdown();
            const auto found = data.index.find(make_key((*it)->name()));
            if (found != data.index.end())
                data.entries[found->second].lifecycle = SystemLifecycle::stopped;
        }

        data.initialized = false;
        ++data.revision;
    }

    RegistryDiagnosticSnapshot Registry::diagnostics() const
    {
        const auto& data = state();
        RegistryDiagnosticSnapshot snapshot{
            .revision = data.revision,
            .frame_count = data.frame_count,
            .last_frame_nanoseconds = data.last_frame_nanoseconds,
            .peak_frame_nanoseconds = data.peak_frame_nanoseconds,
            .order_resolved = data.resolved,
            .initialized = data.initialized,
            .diagnostics_sampling = data.diagnostics_sampling,
            .last_error = data.last_error};
        snapshot.systems.reserve(data.entries.size());
        for (const auto& entry : data.entries)
        {
            SystemDiagnostic diagnostic{
                .name = make_key(entry.name),
                .lifecycle = entry.lifecycle,
                .execution_order = entry.execution_order,
                .update_count = entry.update_count,
                .last_update_nanoseconds = entry.last_update_nanoseconds,
                .peak_update_nanoseconds = entry.peak_update_nanoseconds,
                .total_update_nanoseconds = entry.total_update_nanoseconds};
            const auto dependencies = entry.system->dependencies();
            diagnostic.dependencies.reserve(dependencies.size);
            for (const auto dependency : dependencies)
                diagnostic.dependencies.push_back(make_key(dependency));
            snapshot.systems.push_back(std::move(diagnostic));
        }
        std::sort(snapshot.systems.begin(), snapshot.systems.end(), [](const auto& left, const auto& right) {
            return left.execution_order < right.execution_order;
        });
        return snapshot;
    }

    void Registry::set_diagnostics_sampling(bool enabled) noexcept
    {
        auto& data = state();
        if (data.diagnostics_sampling == enabled)
            return;
        data.diagnostics_sampling = enabled;
        ++data.revision;
    }

    void Registry::reset_diagnostics() noexcept
    {
        auto& data = state();
        data.frame_count = 0u;
        data.last_frame_nanoseconds = 0u;
        data.peak_frame_nanoseconds = 0u;
        for (auto& entry : data.entries)
        {
            entry.update_count = 0u;
            entry.last_update_nanoseconds = 0u;
            entry.peak_update_nanoseconds = 0u;
            entry.total_update_nanoseconds = 0u;
        }
        ++data.revision;
    }
} // namespace epochengine::systems
