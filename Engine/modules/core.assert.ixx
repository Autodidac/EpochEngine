module;

#include "../include/_epoch.stl_types.hpp"

export module core.assert;

export namespace epoch::core::asserts
{
    // Always-on check. Fails fast (logs then terminates).
    void that(bool condition,
        epoch::string_view message = {},
        std::source_location where = std::source_location::current());

    // Debug-only check: compiled out when NDEBUG is defined.
    void debug(bool condition,
        epoch::string_view message = {},
        std::source_location where = std::source_location::current());
}
