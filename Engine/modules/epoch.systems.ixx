/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

// ABI-facing primitives (string_view/span/function_ref + forward decl epoch::string).
#include "../include/_epoch.stl_types.hpp"

// For the template constraint checks (optional but useful).
#include <concepts>
#include <type_traits>

export module epoch.systems;

export namespace epoch::systems
{
    // ---------------------------------------------------------------------
    // System interface
    // ---------------------------------------------------------------------
    struct ISystem
    {
        virtual ~ISystem() noexcept = default;

        [[nodiscard]] virtual epoch::string_view name() const noexcept = 0;

        // Returned views must remain valid for the lifetime of the system object.
        [[nodiscard]] virtual epoch::array_view<const epoch::string_view> dependencies() const noexcept = 0;

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

    template <class T>
    [[nodiscard]] inline SystemFactory make_factory() noexcept
    {
        static_assert(std::is_base_of_v<ISystem, T>, "T must derive from epoch::systems::ISystem");
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

        [[nodiscard]] ISystem* find(epoch::string_view name) noexcept;

        // Topologically sorted by dependencies; valid after resolve_order().
        [[nodiscard]] epoch::array_view<ISystem* const> ordered_systems() const noexcept;

        bool resolve_order() noexcept;
        bool initialize() noexcept;
        void update(double dt_seconds) noexcept;
        void shutdown() noexcept;
    };
} // namespace epoch::systems
