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

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

export module utility.atomicfunction;

export namespace epochengine::core {

    // Thread-safe, reusable, header-only function wrapper
    template <typename Signature>
    struct EpochAtomicFunction {
        using Func = std::function<Signature>;

        EpochAtomicFunction() noexcept = default;
        EpochAtomicFunction(const EpochAtomicFunction&) = delete;
        EpochAtomicFunction& operator=(const EpochAtomicFunction&) = delete;

        // Construct/assign from any callable (lambda, free function, std::function)
        template<typename F>
            requires (!std::is_same_v<std::remove_cvref_t<F>, EpochAtomicFunction>)
        EpochAtomicFunction(F&& f) noexcept {
            store(std::forward<F>(f));
        }

        template<typename F>
            requires (!std::is_same_v<std::remove_cvref_t<F>, EpochAtomicFunction>)
        EpochAtomicFunction& operator=(F&& f) noexcept {
            store(std::forward<F>(f));
            return *this;
        }

        // Store a new callable
        template<typename F>
        void store(F&& f) noexcept {
            publish(std::make_shared<Func>(std::forward<F>(f)));
        }

        void copy_from(const EpochAtomicFunction& source) noexcept { publish(source.snapshot()); }

        // Check if a function is stored
        bool empty() const noexcept {
            return !snapshot();
        }

        // Call operator
        template<typename... Args>
        auto operator()(Args&&... args) const -> decltype(std::declval<Func>()(std::forward<Args>(args)...)) {
            auto f = snapshot();
            if (!f) throw std::bad_function_call();
            return (*f)(std::forward<Args>(args)...);
        }

        // Implicit conversion to bool
        explicit operator bool() const noexcept { return !empty(); }

    private:
        void publish(std::shared_ptr<Func> function) noexcept {
#if defined(__cpp_lib_atomic_shared_ptr)
            ptr.store(std::move(function), std::memory_order_release);
#else
            std::atomic_store_explicit(&ptr, std::move(function), std::memory_order_release);
#endif
        }

        std::shared_ptr<Func> snapshot() const noexcept {
#if defined(__cpp_lib_atomic_shared_ptr)
            return ptr.load(std::memory_order_acquire);
#else
            return std::atomic_load_explicit(&ptr, std::memory_order_acquire);
#endif
        }

#if defined(__cpp_lib_atomic_shared_ptr)
        std::atomic<std::shared_ptr<Func>> ptr{};
#else
        std::shared_ptr<Func> ptr{};
#endif
    };

} // namespace epochengine::core
