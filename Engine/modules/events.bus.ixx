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
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
module;

#include "../include/core.stl_types.hpp"
#include <algorithm>
#include <memory>

export module events.bus;

export namespace epochengine::events
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
    // Meant for engine-internal notifications, not ABI boundaries or a journal.
    // One owner thread serializes access. Borrowed user data must outlive its
    // subscription; remove it before retiring that data. Callbacks may emit,
    // subscribe, unsubscribe or clear on this same bus.
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
            std::shared_ptr<void> callback{};
            bool active{true};
        };

        bus() = default;
        bus(const bus& other) : _handlers(other._handlers) { compact(); }
        bus& operator=(const bus& other)
        {
            if (this == &other) return *this;
            auto replacement = other._handlers;
            if (_dispatch_depth == 0u) _handlers = std::move(replacement);
            else
            {
                clear();
                _handlers.insert(_handlers.end(), replacement.begin(), replacement.end());
            }
            return *this;
        }

        // Register a handler for event type E.
        template <class E>
        void subscribe(void* user, void (*fn)(void* user, const E& e) noexcept)
        {
            if (!fn) return;
            using Event = std::remove_cvref_t<E>;
            handler h{};
            h.type = id_of<E>();
            h.user = user;
            h.callback = std::make_shared<typed_callback<Event>>(typed_callback<Event>{user,fn});
            h.fn = [](void* stored, const void* p) noexcept
            {
                const auto& callback = *static_cast<const typed_callback<Event>*>(stored);
                callback.fn(callback.user, *static_cast<const Event*>(p));
            };
            _handlers.push_back(std::move(h));
        }

        // Remove matching registrations, including duplicates. Retired payloads
        // survive until an executing callback returns; other callbacks see the
        // removal immediately. No callback is invoked by removal.
        template<class E>
        std::size_t unsubscribe(void* user, void (*fn)(void*,const E&) noexcept) noexcept
        {
            using Event = std::remove_cvref_t<E>;
            std::size_t removed{};
            for (auto& h : _handlers)
            {
                if (!h.active || h.type != id_of<E>() || h.user != user) continue;
                const auto& callback = *static_cast<const typed_callback<Event>*>(h.callback.get());
                if (callback.fn == fn) {h.active=false;++removed;}
            }
            if (_dispatch_depth == 0u) compact();
            return removed;
        }

        // Emit event E to all subscribers of E.
        template <class E>
        void emit(const E& e) const noexcept
        {
            const type_id_t t = id_of<E>();
            const auto count = _handlers.size();
            ++_dispatch_depth;
            for (std::size_t index=0; index<count; ++index)
            {
                // Copy only the small slot and shared payload. A callback may
                // append/reallocate the vector or retire its own registration.
                const auto h = _handlers[index];
                if (h.type == t && h.fn)
                    if (h.active) h.fn(h.callback.get(), &e);
            }
            if (--_dispatch_depth == 0u) compact();
        }

        [[nodiscard]] std::size_t handler_count() const noexcept
        {return static_cast<std::size_t>(std::count_if(_handlers.begin(),_handlers.end(),[](const handler& h){return h.active;}));}

        void clear() noexcept
        {
            if (_dispatch_depth == 0u) _handlers.clear();
            else for (auto& h : _handlers) h.active=false;
        }

    private:
        template<class E> struct typed_callback final
        {void* user{};void (*fn)(void*,const E&) noexcept{};};
        void compact() const noexcept
        {std::erase_if(_handlers,[](const handler& h){return !h.active;});}
        mutable std::vector<handler> _handlers{};
        mutable std::size_t _dispatch_depth{};
    };
} // namespace epochengine::events
