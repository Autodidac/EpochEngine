module;

#include "../include/_epoch.stl_types.hpp"

export module core.env;

export namespace epoch::core::env
{
    // Returns the value of an environment variable if present.
    epoch::optional<epoch::string> get(epoch::string_view name);

    // Sets an environment variable for the current process.
    // Returns true on success.
    bool set(epoch::string_view name, epoch::string_view value);

    // Unsets an environment variable for the current process.
    // Returns true on success.
    bool unset(epoch::string_view name);
}
