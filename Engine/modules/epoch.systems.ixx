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

// ABI-facing primitives (string_view/span/function_ref + forward decl epochengine::string).
#include "../include/_epoch.stl_types.hpp"

#include <atomic>
// For the template constraint checks (optional but useful).
#include <concepts>
#include <cstdint>
#include <type_traits>

export module epoch.systems;

export namespace epochengine::systems
{
    // ---------------------------------------------------------------------
    // System interface
    // ---------------------------------------------------------------------
    struct ISystem
    {
        virtual ~ISystem() noexcept = default;

        [[nodiscard]] virtual epochengine::string_view name() const noexcept = 0;

        // Returned views must remain valid for the lifetime of the system object.
        [[nodiscard]] virtual epochengine::array_view<const epochengine::string_view> dependencies() const noexcept = 0;

        virtual void on_init() noexcept = 0;
        virtual void on_update(double dt_seconds) noexcept = 0;
        virtual void on_shutdown() noexcept = 0;
    };

    // ---------------------------------------------------------------------
    // Factory (so modules can register systems without exporting concrete types)
    // ---------------------------------------------------------------------
    struct SystemFactory
    {
        using create_fn = ISystem * (*)();                 // may throw via `new`
        using destroy_fn = void (*)(ISystem*) noexcept;    // must not throw

        create_fn  create = nullptr;
        destroy_fn destroy = nullptr;
    };

    namespace threading
    {
        inline std::atomic<std::uint32_t> g_liveEngineThreads{ 0 };

        [[nodiscard]] inline std::uint32_t live_thread_count() noexcept
        {
            return g_liveEngineThreads.load(std::memory_order_relaxed);
        }

        inline void thread_started() noexcept
        {
            g_liveEngineThreads.fetch_add(1, std::memory_order_relaxed);
        }

        inline void thread_finished() noexcept
        {
            auto current = g_liveEngineThreads.load(std::memory_order_relaxed);
            while (current > 0)
            {
                if (g_liveEngineThreads.compare_exchange_weak(
                    current,
                    current - 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
                {
                    return;
                }
            }
        }

        class ScopedThreadActivity
        {
        public:
            ScopedThreadActivity() noexcept
            {
                thread_started();
            }

            ScopedThreadActivity(const ScopedThreadActivity&) = delete;
            ScopedThreadActivity& operator=(const ScopedThreadActivity&) = delete;

            ~ScopedThreadActivity() noexcept
            {
                thread_finished();
            }
        };
    }

    template <class T>
    [[nodiscard]] inline SystemFactory make_factory() noexcept
    {
        static_assert(std::is_base_of_v<ISystem, T>, "T must derive from epochengine::systems::ISystem");
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

        return SystemFactory{
            []() -> ISystem* { return new T(); },
            [](ISystem* ptr) noexcept { delete static_cast<T*>(ptr); }
        };
    }

    // ---------------------------------------------------------------------
    // Registry
    // ---------------------------------------------------------------------
    class Registry
    {
    public:
        static Registry& instance() noexcept;

        // Takes ownership of the created system (constructed immediately).
        bool register_system(SystemFactory factory) noexcept;

        [[nodiscard]] ISystem* find(epochengine::string_view name) noexcept;

        // Topologically sorted by dependencies; valid after resolve_order().
        [[nodiscard]] epochengine::array_view<ISystem* const> ordered_systems() const noexcept;

        bool resolve_order() noexcept;
        bool initialize() noexcept;
        void update(double dt_seconds) noexcept;
        void shutdown() noexcept;
    };
} // namespace epochengine::systems
