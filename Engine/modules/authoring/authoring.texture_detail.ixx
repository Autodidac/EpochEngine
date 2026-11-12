// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

export module authoring.texture_detail;
import authoring.texture;

// Shared implementation of the texture document and disposable residency planner.
// The public texture module does not re-export these implementation helpers.
export namespace epochengine::authoring::texture::detail
{
    constexpr std::uint64_t kFnvPrime = 1'099'511'628'211ull;

    class StableHash final
    {
    public:
        StableHash() noexcept
            : words_{
                0xcbf29ce484222325ull,
                0x9e3779b97f4a7c15ull,
                0x6a09e667f3bcc909ull,
                0xbb67ae8584caa73bull
            }
        {
        }

        void add_byte(std::uint8_t value) noexcept
        {
            words_[0] ^= value;
            words_[0] *= kFnvPrime;

            words_[1] ^= static_cast<std::uint64_t>(value)
                + 0x9e3779b97f4a7c15ull
                + (words_[1] << 6)
                + (words_[1] >> 2);
            words_[1] = std::rotl(words_[1], 17) *
                0xbf58476d1ce4e5b9ull;

            words_[2] += static_cast<std::uint64_t>(value)
                + (words_[0] ^ std::rotl(words_[1], 11));
            words_[2] ^= words_[2] >> 29;
            words_[2] *= 0x94d049bb133111ebull;

            words_[3] ^= static_cast<std::uint64_t>(value)
                + std::rotl(words_[0], 7)
                + std::rotl(words_[2], 31);
            words_[3] *= 0x9e3779b185ebca87ull;
        }

        template <typename Value>
        void add_integral(Value value) noexcept
        {
            static_assert(
                std::is_integral_v<Value> || std::is_enum_v<Value>);
            using Raw = typename std::conditional_t<
                std::is_enum_v<Value>,
                std::underlying_type<Value>,
                std::type_identity<Value>>::type;
            if constexpr (std::is_same_v<std::remove_cv_t<Raw>, bool>)
            {
                add_byte(value ? 1u : 0u);
            }
            else
            {
                using Unsigned = std::make_unsigned_t<Raw>;
                Unsigned bits = static_cast<Unsigned>(value);
                for (std::size_t index = 0;
                     index < sizeof(Unsigned);
                     ++index)
                {
                    add_byte(static_cast<std::uint8_t>(
                        bits & static_cast<Unsigned>(0xff)));
                    if constexpr (sizeof(Unsigned) > 1)
                    {
                        if (index + 1u < sizeof(Unsigned))
                            bits = static_cast<Unsigned>(bits >> 8u);
                    }
                }
            }
        }

        void add_bytes(const std::byte* bytes, std::size_t size) noexcept
        {
            add_integral(size);
            for (std::size_t index = 0; index < size; ++index)
            {
                add_byte(std::to_integer<std::uint8_t>(bytes[index]));
            }
        }

        void add_string(std::string_view value) noexcept
        {
            add_integral(value.size());
            for (const char character : value)
            {
                add_byte(static_cast<std::uint8_t>(character));
            }
        }

        void add_hash(const ContentHash& hash) noexcept
        {
            for (const std::uint64_t word : hash.words)
            {
                add_integral(word);
            }
        }

        [[nodiscard]] ContentHash finish() const noexcept
        {
            ContentHash result{ words_ };
            for (std::size_t index = 0; index < result.words.size(); ++index)
            {
                std::uint64_t value = result.words[index]
                    ^ std::rotl(
                        result.words[(index + 1) % result.words.size()],
                        static_cast<int>(13 + index * 7));
                value ^= value >> 30;
                value *= 0xbf58476d1ce4e5b9ull;
                value ^= value >> 27;
                value *= 0x94d049bb133111ebull;
                value ^= value >> 31;
                result.words[index] = value;
            }
            if (result.empty())
            {
                result.words[0] = 1;
            }
            return result;
        }

    private:
        std::array<std::uint64_t, 4> words_{};
    };

    [[nodiscard]] constexpr std::uint32_t mip_dimension(
        std::uint32_t base,
        std::uint8_t mip) noexcept
    {
        return mip >= 32
            ? 1u
            : (std::max)(1u, base >> mip);
    }

    [[nodiscard]] constexpr bool checked_add(
        std::uint64_t left,
        std::uint64_t right,
        std::uint64_t& result) noexcept
    {
        if (right > (std::numeric_limits<std::uint64_t>::max)() - left)
            return false;
        result = left + right;
        return true;
    }

    [[nodiscard]] constexpr bool checked_multiply(
        std::uint64_t left,
        std::uint64_t right,
        std::uint64_t& result) noexcept
    {
        if (left != 0 &&
            right > (std::numeric_limits<std::uint64_t>::max)() / left)
        {
            return false;
        }
        result = left * right;
        return true;
    }
}
