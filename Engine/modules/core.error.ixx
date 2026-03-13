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
#include <source_location>

export module core.error;

export namespace epoch::core::error
{
    using domain_t = std::uint32_t;

    struct code
    {
        domain_t domain = 0;
        std::int32_t value = 0;

        constexpr bool operator==(const code&) const noexcept = default;
    };

    struct err
    {
        code c{};
        epoch::string_view message{};
        std::source_location where = std::source_location::current();

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !(c.domain == 0 && c.value == 0);
        }
    };

    template <class T>
    using result = std::expected<T, err>;

    [[nodiscard]] constexpr code ok_code() noexcept { return {}; }
    [[nodiscard]] constexpr err  ok() noexcept { return {}; }

    // Core constructor (no defaults here; overloads below provide convenience).
    [[nodiscard]] constexpr err make(code c, epoch::string_view msg, std::source_location loc) noexcept
    {
        return { c, msg, loc };
    }

    // Convenience: generic core domain errors.
    namespace core_domain
    {
        inline constexpr domain_t id = 0;

        inline constexpr std::int32_t failed = 1;
        inline constexpr std::int32_t invalid_argument = 2;
        inline constexpr std::int32_t not_found = 3;
        inline constexpr std::int32_t io_error = 4;
        inline constexpr std::int32_t unsupported = 5;
    }

    // ---- make overloads (module-safe; no default-arg dependence) ----
    [[nodiscard]] constexpr err make(code c) noexcept
    {
        return make(c, epoch::string_view{}, std::source_location::current());
    }

    [[nodiscard]] constexpr err make(code c, epoch::string_view msg) noexcept
    {
        return make(c, msg, std::source_location::current());
    }

    // ---- failed overloads ----
    [[nodiscard]] constexpr err failed() noexcept
    {
        return make({ core_domain::id, core_domain::failed }, epoch::string_view{}, std::source_location::current());
    }

    [[nodiscard]] constexpr err failed(epoch::string_view msg) noexcept
    {
        return make({ core_domain::id, core_domain::failed }, msg, std::source_location::current());
    }

    [[nodiscard]] constexpr err failed(epoch::string_view msg, std::source_location loc) noexcept
    {
        return make({ core_domain::id, core_domain::failed }, msg, loc);
    }

    // ---- invalid_argument overloads ----
    [[nodiscard]] constexpr err invalid_argument() noexcept
    {
        return make({ core_domain::id, core_domain::invalid_argument }, epoch::string_view{}, std::source_location::current());
    }

    [[nodiscard]] constexpr err invalid_argument(epoch::string_view msg) noexcept
    {
        return make({ core_domain::id, core_domain::invalid_argument }, msg, std::source_location::current());
    }

    [[nodiscard]] constexpr err invalid_argument(epoch::string_view msg, std::source_location loc) noexcept
    {
        return make({ core_domain::id, core_domain::invalid_argument }, msg, loc);
    }
}
