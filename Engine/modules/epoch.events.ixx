/**************************************************************
 *   Epoch Engine - Events (Type-safe bus + ring queue) (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/_epoch.stl_types.hpp"

export module epoch.events;

export namespace epoch::events
{
    // -------------------------------------------------------------------------
    // Type-id: stable within one binary. For plugins/ABI boundaries, use explicit
    // ids in your C interface instead.
    // -------------------------------------------------------------------------
    using type_id_t = std::uint64_t;

    namespace detail
    {
        consteval std::uint64_t fnv1a_64(const char* s)
        {
            std::uint64_t h = 14695981039346656037ull;
            for (; *s; ++s)
            {
                h ^= static_cast<unsigned char>(*s);
                h *= 1099511628211ull;
            }
            return h;
        }

#if defined(_MSC_VER)
        template <class T>
        consteval type_id_t type_id() { return fnv1a_64(__FUNCSIG__); }
#else
        template <class T>
        consteval type_id_t type_id() { return fnv1a_64(__PRETTY_FUNCTION__); }
#endif
    } // namespace detail

    template <class T>
    consteval type_id_t id_of() { return detail::type_id<std::remove_cvref_t<T>>(); }

    // -------------------------------------------------------------------------
    // Small ring buffer for "events since last pump". Good for Tier-1.
    // -------------------------------------------------------------------------
    template <class T, std::size_t Capacity>
    class ring
    {
        static_assert(Capacity > 0);
    public:
        using value_type = T;

        [[nodiscard]] constexpr bool empty() const noexcept { return _count == 0; }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return _count; }
        [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }

        // Overwrites oldest when full (lossy by design).
        constexpr void push(const T& v) noexcept
        {
            _buf[_head] = v;
            _head = (_head + 1) % Capacity;
            if (_count < Capacity) ++_count;
            else _tail = (_tail + 1) % Capacity;
        }

        constexpr void push(T&& v) noexcept
        {
            _buf[_head] = std::move(v);
            _head = (_head + 1) % Capacity;
            if (_count < Capacity) ++_count;
            else _tail = (_tail + 1) % Capacity;
        }

        // Pop oldest; returns false if empty.
        constexpr bool pop(T& out) noexcept
        {
            if (_count == 0) return false;
            out = std::move(_buf[_tail]);
            _tail = (_tail + 1) % Capacity;
            --_count;
            return true;
        }

        constexpr void clear() noexcept
        {
            _head = _tail = _count = 0;
        }

    private:
        std::array<T, Capacity> _buf{};
        std::size_t _head = 0;
        std::size_t _tail = 0;
        std::size_t _count = 0;
    };

    // -------------------------------------------------------------------------
    // Bus: subscribe/emit with zero allocations per emit (handlers stored once).
    // Meant for engine-internal events, not ABI boundaries.
    // -------------------------------------------------------------------------
    class bus
    {
    public:
        using handler_fn = void (*)(void* user, const void* evt) noexcept;

        struct handler
        {
            type_id_t type = 0;
            handler_fn fn = nullptr;
            void* user = nullptr;
        };

        // Register a handler for event type E.
        template <class E>
        void subscribe(void* user, void (*fn)(void* user, const E& e) noexcept)
        {
            handler h{};
            h.type = id_of<E>();
            h.user = user;
            h.fn = [](void* u, const void* p) noexcept
            {
                fn(u, *static_cast<const E*>(p));
            };
            _handlers.push_back(h);
        }

        // Emit event E to all subscribers of E.
        template <class E>
        void emit(const E& e) const noexcept
        {
            const type_id_t t = id_of<E>();
            for (const auto& h : _handlers)
            {
                if (h.type == t && h.fn)
                    h.fn(h.user, &e);
            }
        }

        [[nodiscard]] std::size_t handler_count() const noexcept { return _handlers.size(); }

        void clear() noexcept { _handlers.clear(); }

    private:
        std::vector<handler> _handlers{};
    };
} // namespace epoch::events
