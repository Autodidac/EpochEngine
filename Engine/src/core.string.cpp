module;

#include "../include/_epoch.stl_types.hpp"
#include <string>

module core.string;

namespace epoch::core::string
{
    [[nodiscard]] static constexpr bool is_ascii_ws(unsigned char c) noexcept
    {
        // ASCII whitespace: space, \t \n \r \f \v
        return c == ' ' || c == '\t' || c == '\n' ||
            c == '\r' || c == '\f' || c == '\v';
    }

    epoch::string_view trim(epoch::string_view s) noexcept
    {
        const char* p = s.data;
        const std::size_t n = s.size;

        std::size_t b = 0;
        while (b < n && is_ascii_ws(static_cast<unsigned char>(p[b])))
            ++b;

        std::size_t e = n;
        while (e > b && is_ascii_ws(static_cast<unsigned char>(p[e - 1])))
            --e;

        return epoch::string_view{ p + b, e - b };
    }

    epoch::small_vector<epoch::string_view> split(epoch::string_view s, char delim)
    {
        epoch::small_vector<epoch::string_view> out;

        const char* p = s.data;
        const std::size_t n = s.size;

        std::size_t start = 0;
        for (std::size_t i = 0; i <= n; ++i)
        {
            if (i == n || p[i] == delim)
            {
                out.emplace_back(epoch::string_view{ p + start, i - start });
                start = i + 1;
            }
        }
        return out;
    }

    epoch::string join(epoch::span<const epoch::string_view> parts, epoch::string_view delim)
    {
        std::string out;

        const std::size_t count = parts.size;
        if (count == 0)
            return epoch::string{ std::move(out) };

        std::size_t total = 0;
        for (std::size_t i = 0; i < count; ++i)
            total += parts.data[i].size;

        total += (count - 1) * delim.size;
        out.reserve(total);

        const std::string_view d = epoch::to_std(delim);

        for (std::size_t i = 0; i < count; ++i)
        {
            if (i) out.append(d);
            out.append(epoch::to_std(parts.data[i]));
        }

        return epoch::string{ std::move(out) };
    }
}
