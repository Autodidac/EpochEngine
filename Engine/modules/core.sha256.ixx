/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

export module core.sha256;

export namespace epochengine::core::sha256
{
    struct Digest final
    {
        std::array<std::uint8_t, 32u> bytes{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            for (const std::uint8_t byte : bytes)
            {
                if (byte != 0u)
                    return true;
            }
            return false;
        }

        friend constexpr bool operator==(
            const Digest& left,
            const Digest& right) noexcept
        {
            for (std::size_t index = 0u; index < left.bytes.size(); ++index)
            {
                if (left.bytes[index] != right.bytes[index])
                    return false;
            }
            return true;
        }
    };

    class Hasher final
    {
    public:
        void update(std::span<const std::uint8_t> bytes) noexcept
        {
            if (finished_)
                return;
            total_bytes_ += static_cast<std::uint64_t>(bytes.size());
            for (const std::uint8_t byte : bytes)
            {
                buffer_[buffered_++] = byte;
                if (buffered_ == buffer_.size())
                {
                    transform(buffer_);
                    buffered_ = 0u;
                }
            }
        }

        void update(std::string_view text) noexcept
        {
            update(std::span<const std::uint8_t>{
                reinterpret_cast<const std::uint8_t*>(text.data()),
                text.size()});
        }

        [[nodiscard]] Digest finish() noexcept
        {
            if (finished_)
                return finished_digest_;

            const std::uint64_t bit_count = total_bytes_ * 8u;
            buffer_[buffered_++] = 0x80u;
            if (buffered_ > 56u)
            {
                while (buffered_ < buffer_.size())
                    buffer_[buffered_++] = 0u;
                transform(buffer_);
                buffered_ = 0u;
            }
            while (buffered_ < 56u)
                buffer_[buffered_++] = 0u;
            for (std::size_t index = 0u; index < 8u; ++index)
            {
                buffer_[56u + index] = static_cast<std::uint8_t>(
                    bit_count >> ((7u - index) * 8u));
            }
            transform(buffer_);

            for (std::size_t word = 0u; word < state_.size(); ++word)
            {
                finished_digest_.bytes[word * 4u] =
                    static_cast<std::uint8_t>(state_[word] >> 24u);
                finished_digest_.bytes[word * 4u + 1u] =
                    static_cast<std::uint8_t>(state_[word] >> 16u);
                finished_digest_.bytes[word * 4u + 2u] =
                    static_cast<std::uint8_t>(state_[word] >> 8u);
                finished_digest_.bytes[word * 4u + 3u] =
                    static_cast<std::uint8_t>(state_[word]);
            }
            finished_ = true;
            return finished_digest_;
        }

    private:
        [[nodiscard]] static constexpr std::uint32_t choose(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t z) noexcept
        {
            return (x & y) ^ (~x & z);
        }

        [[nodiscard]] static constexpr std::uint32_t majority(
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t z) noexcept
        {
            return (x & y) ^ (x & z) ^ (y & z);
        }

        [[nodiscard]] static constexpr std::uint32_t sigma0(
            std::uint32_t value) noexcept
        {
            return std::rotr(value, 2) ^ std::rotr(value, 13)
                ^ std::rotr(value, 22);
        }

        [[nodiscard]] static constexpr std::uint32_t sigma1(
            std::uint32_t value) noexcept
        {
            return std::rotr(value, 6) ^ std::rotr(value, 11)
                ^ std::rotr(value, 25);
        }

        [[nodiscard]] static constexpr std::uint32_t gamma0(
            std::uint32_t value) noexcept
        {
            return std::rotr(value, 7) ^ std::rotr(value, 18)
                ^ (value >> 3u);
        }

        [[nodiscard]] static constexpr std::uint32_t gamma1(
            std::uint32_t value) noexcept
        {
            return std::rotr(value, 17) ^ std::rotr(value, 19)
                ^ (value >> 10u);
        }

        void transform(
            const std::array<std::uint8_t, 64u>& block) noexcept
        {
            static constexpr std::array<std::uint32_t, 64u> constants{
                0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
                0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
                0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
                0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
                0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
                0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
                0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
                0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
                0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
                0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
                0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
                0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
                0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
                0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

            std::array<std::uint32_t, 64u> words{};
            for (std::size_t index = 0u; index < 16u; ++index)
            {
                const std::size_t offset = index * 4u;
                words[index] =
                    (static_cast<std::uint32_t>(block[offset]) << 24u)
                    | (static_cast<std::uint32_t>(block[offset + 1u]) << 16u)
                    | (static_cast<std::uint32_t>(block[offset + 2u]) << 8u)
                    | static_cast<std::uint32_t>(block[offset + 3u]);
            }
            for (std::size_t index = 16u; index < words.size(); ++index)
            {
                words[index] = gamma1(words[index - 2u])
                    + words[index - 7u]
                    + gamma0(words[index - 15u])
                    + words[index - 16u];
            }

            std::uint32_t a = state_[0u];
            std::uint32_t b = state_[1u];
            std::uint32_t c = state_[2u];
            std::uint32_t d = state_[3u];
            std::uint32_t e = state_[4u];
            std::uint32_t f = state_[5u];
            std::uint32_t g = state_[6u];
            std::uint32_t h = state_[7u];
            for (std::size_t index = 0u; index < words.size(); ++index)
            {
                const std::uint32_t first =
                    h + sigma1(e) + choose(e, f, g)
                    + constants[index] + words[index];
                const std::uint32_t second =
                    sigma0(a) + majority(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + first;
                d = c;
                c = b;
                b = a;
                a = first + second;
            }
            state_[0u] += a;
            state_[1u] += b;
            state_[2u] += c;
            state_[3u] += d;
            state_[4u] += e;
            state_[5u] += f;
            state_[6u] += g;
            state_[7u] += h;
        }

        std::array<std::uint32_t, 8u> state_{
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        std::array<std::uint8_t, 64u> buffer_{};
        std::uint64_t total_bytes_{};
        std::size_t buffered_{};
        Digest finished_digest_{};
        bool finished_{};
    };

    [[nodiscard]] inline Digest hash(
        std::span<const std::uint8_t> bytes) noexcept
    {
        Hasher hasher{};
        hasher.update(bytes);
        return hasher.finish();
    }

    [[nodiscard]] inline Digest hash(std::string_view text) noexcept
    {
        Hasher hasher{};
        hasher.update(text);
        return hasher.finish();
    }

    [[nodiscard]] inline std::string hex(const Digest& digest)
    {
        static constexpr std::string_view digits{"0123456789abcdef"};
        std::string result;
        result.resize(digest.bytes.size() * 2u);
        for (std::size_t index = 0u; index < digest.bytes.size(); ++index)
        {
            result[index * 2u] = digits[digest.bytes[index] >> 4u];
            result[index * 2u + 1u] = digits[digest.bytes[index] & 0x0fu];
        }
        return result;
    }

    [[nodiscard]] inline std::optional<Digest> from_hex(
        std::string_view value) noexcept
    {
        if (value.size() != 64u)
            return std::nullopt;
        const auto nibble = [](char ch) noexcept -> std::optional<std::uint8_t>
        {
            if (ch >= '0' && ch <= '9')
                return static_cast<std::uint8_t>(ch - '0');
            if (ch >= 'a' && ch <= 'f')
                return static_cast<std::uint8_t>(10 + ch - 'a');
            if (ch >= 'A' && ch <= 'F')
                return static_cast<std::uint8_t>(10 + ch - 'A');
            return std::nullopt;
        };

        Digest result{};
        for (std::size_t index = 0u; index < result.bytes.size(); ++index)
        {
            const auto high = nibble(value[index * 2u]);
            const auto low = nibble(value[index * 2u + 1u]);
            if (!high || !low)
                return std::nullopt;
            result.bytes[index] = static_cast<std::uint8_t>(
                (*high << 4u) | *low);
        }
        return result;
    }
}
