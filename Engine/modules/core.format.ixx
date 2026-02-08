// ============================================================================
// modules/core.format.ixx
// Tiny formatting helper around std::format / std::vformat.
// ============================================================================
module;

#include "../include/_epoch.stl_types.hpp"
#include <format>

export module core.format;

export namespace epoch::core::format
{
    // Backend: takes pre-built format_args.
    [[nodiscard]] inline epoch::string vstr(epoch::string_view fmt, std::format_args args)
    {
        return epoch::string{ std::vformat(epoch::to_std(fmt), args) };
    }

    // Convenience: build args safely (lvalues) then call vstr.
    template <class... Args>
    [[nodiscard]] inline epoch::string str(epoch::string_view fmt, const Args&... args)
    {
        return vstr(fmt, std::make_format_args(args...));
    }
}
