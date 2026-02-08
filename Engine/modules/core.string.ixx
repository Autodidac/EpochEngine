module;

#include "../include/_epoch.stl_types.hpp"

export module core.string;

export namespace epoch::core::string
{
    // Returns a view with leading/trailing ASCII whitespace removed.
    [[nodiscard]] epoch::string_view trim(epoch::string_view s) noexcept;

    // Splits on a single character delimiter. Empty tokens are preserved.
    [[nodiscard]] epoch::small_vector<epoch::string_view> split(epoch::string_view s, char delim);

    // Joins views with a delimiter into a new string.
    [[nodiscard]] epoch::string join(epoch::span<const epoch::string_view> parts,
        epoch::string_view delim);
}
