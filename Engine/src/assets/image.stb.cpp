// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Sole private stb_image provider. No raw stbi_* symbol is exported.
#include "image.decode.hpp"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace
{
    struct Budget { std::size_t limit{}, used{}; bool exhausted{}; };
    thread_local Budget* active_budget{};
    union Allocation { std::max_align_t alignment; std::size_t bytes; };
    void* allocate(std::size_t bytes) noexcept
    {
        if (bytes > (std::numeric_limits<std::size_t>::max)() - sizeof(Allocation)) return nullptr;
        if (active_budget && bytes > active_budget->limit - active_budget->used)
        { active_budget->exhausted = true; return nullptr; }
        auto* block = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
        if (!block) { if (active_budget) active_budget->exhausted = true; return nullptr; }
        block->bytes = bytes;
        if (active_budget) active_budget->used += bytes;
        return block + 1;
    }
    void deallocate(void* memory) noexcept
    {
        if (!memory) return;
        auto* block = static_cast<Allocation*>(memory) - 1;
        if (active_budget) active_budget->used -= block->bytes;
        std::free(block);
    }
    void* reallocate(void* memory, std::size_t bytes) noexcept
    {
        if (!memory) return allocate(bytes);
        auto* block = static_cast<Allocation*>(memory) - 1;
        const auto old = block->bytes;
        if (bytes > (std::numeric_limits<std::size_t>::max)() - sizeof(Allocation)) return nullptr;
        if (active_budget && bytes > active_budget->limit - (active_budget->used - old))
        { active_budget->exhausted = true; return nullptr; }
        auto* resized = static_cast<Allocation*>(std::realloc(block, sizeof(Allocation) + bytes));
        if (!resized) { if (active_budget) active_budget->exhausted = true; return nullptr; }
        resized->bytes = bytes;
        if (active_budget) active_budget->used = active_budget->used - old + bytes;
        return resized + 1;
    }
}
#define STBI_MALLOC(size) allocate(size)
#define STBI_REALLOC(memory, size) reallocate(memory, size)
#define STBI_FREE(memory) deallocate(memory)
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505) // Private third-party entry points not used by this PNG owner.
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../stb/stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb/stb_image_write.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace epochengine::image_codec
{
    bool make_magenta_cutout(std::span<std::byte> rgba, std::uint32_t width, std::uint32_t height) noexcept
    {
        if (!width || !height || width > 2048u || height > 2048u
            || rgba.size() != std::size_t{width} * height * 4u) return false;
        const auto keyed = [](const std::byte* pixel) {
            const auto red = std::to_integer<unsigned>(pixel[0]);
            const auto green = std::to_integer<unsigned>(pixel[1]);
            const auto blue = std::to_integer<unsigned>(pixel[2]);
            return red >= 120u && blue >= 100u && red >= green + 35u && blue >= green + 35u && blue + 60u >= red;
        };
        std::size_t background{}, transparent{}, border{}, keyedBorder{};
        for (std::uint32_t y{}; y < height; ++y)
            for (std::uint32_t x{}; x < width; ++x)
            {
                const auto* pixel = rgba.data() + (std::size_t{y} * width + x) * 4u;
                const bool key = keyed(pixel);
                background += key;
                transparent += std::to_integer<unsigned>(pixel[3]) < 128u;
                if (x == 0u || y == 0u || x + 1u == width || y + 1u == height)
                { ++border; keyedBorder += key; }
            }
        const auto pixels = rgba.size() / 4u;
        if (transparent) return transparent < pixels;
        if (keyedBorder * 4u < border * 3u || background * 20u < pixels || background * 20u > pixels * 19u)
            return false;
        for (std::size_t offset{}; offset < rgba.size(); offset += 4u)
            if (keyed(rgba.data() + offset))
                for (std::size_t channel{}; channel < 4u; ++channel) rgba[offset + channel] = std::byte{};
        // Suppress magenta spill only on the new cutout edge. Interior foliage
        // colors and pre-existing alpha images keep their original samples.
        const auto stride = std::size_t{width} * 4u;
        for (std::uint32_t y{}; y < height; ++y)
            for (std::uint32_t x{}; x < width; ++x)
            {
                const auto offset = std::size_t{y} * stride + x * 4u;
                auto* pixel = rgba.data() + offset;
                if (pixel[3] == std::byte{} || pixel[0] <= pixel[1] || pixel[2] <= pixel[1]) continue;
                if ((x && rgba[offset - 1u] == std::byte{})
                    || (x + 1u < width && rgba[offset + 7u] == std::byte{})
                    || (y && rgba[offset - stride + 3u] == std::byte{})
                    || (y + 1u < height && rgba[offset + stride + 3u] == std::byte{}))
                    pixel[2] = pixel[1];
            }
        return true;
    }

    std::vector<std::byte> encode_rgba_png(std::span<const std::byte> rgba,
        std::uint32_t width, std::uint32_t height) noexcept
    {
        if (!width || !height || width > 2048u || height > 2048u
            || rgba.size() != std::size_t{width} * height * 4u) return {};
        int size{};
        auto* encoded = stbi_write_png_to_mem(reinterpret_cast<const unsigned char*>(rgba.data()),
            int(width * 4u), int(width), int(height), 4, &size);
        std::vector<std::byte> bytes;
        if (encoded && size > 0)
        {
            const auto* begin = reinterpret_cast<const std::byte*>(encoded);
            try { bytes.assign(begin, begin + size); }
            catch (...) { bytes.clear(); }
        }
        std::free(encoded);
        return bytes;
    }

    namespace
    {
        std::uint32_t be32(std::span<const std::byte> source, std::size_t offset) noexcept
        {
            return (std::to_integer<std::uint32_t>(source[offset]) << 24u)
                | (std::to_integer<std::uint32_t>(source[offset + 1]) << 16u)
                | (std::to_integer<std::uint32_t>(source[offset + 2]) << 8u)
                | std::to_integer<std::uint32_t>(source[offset + 3]);
        }
        std::uint32_t crc32(std::span<const std::byte> bytes) noexcept
        {
            std::uint32_t crc = 0xffffffffu;
            for (const auto byte : bytes)
            {
                crc ^= std::to_integer<std::uint32_t>(byte);
                for (unsigned bit = 0; bit < 8; ++bit)
                    crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
            }
            return crc ^ 0xffffffffu;
        }
        bool complete_png(std::span<const std::byte> source) noexcept
        {
            if (source.size() < 45 || std::memcmp(source.data(), "\x89PNG\r\n\x1a\n", 8) != 0) return false;
            bool header{}, data{}, ended_data{};
            for (std::size_t offset = 8; offset < source.size();)
            {
                if (source.size() - offset < 12) return false;
                const std::size_t size = be32(source, offset);
                if (size > source.size() - offset - 12) return false;
                const auto* type = reinterpret_cast<const char*>(source.data() + offset + 4);
                if (crc32(source.subspan(offset + 4, size + 4)) != be32(source, offset + 8 + size)) return false;
                if (!header)
                {
                    if (size != 13 || std::memcmp(type, "IHDR", 4) != 0) return false;
                    header = true;
                }
                else if (std::memcmp(type, "IHDR", 4) == 0) return false;
                if (std::memcmp(type, "IDAT", 4) == 0)
                { if (ended_data) return false; data = true; }
                else if (data) ended_data = true;
                if (std::memcmp(type, "IEND", 4) == 0)
                    return size == 0 && data && offset + 12 == source.size();
                offset += size + 12;
            }
            return false;
        }
    }
    PngResult decode_png(std::span<const std::byte> source, std::uint32_t maximum_dimension,
        std::uint64_t maximum_decoded_bytes) noexcept
    {
        PngResult result;
        if (!maximum_dimension || !maximum_decoded_bytes
            || source.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())
            || !complete_png(source)) return result;
        const auto width = be32(source, 16), height = be32(source, 20);
        result.width = width; result.height = height;
        if (!width || !height) return result;
        const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
        if (width > maximum_dimension || height > maximum_dimension
            || pixels > maximum_decoded_bytes / 4u
            || pixels > ((std::numeric_limits<std::size_t>::max)() - 65536u) / 12u)
        { result.status = PngStatus::dimensions_exceeded; return result; }
        // Bound scratch/inflate allocations too: malformed IDAT cannot grow without limit.
        Budget budget{static_cast<std::size_t>(pixels * 12u + 65536u)};
        struct Scope
        {
            Budget* previous;
            explicit Scope(Budget* value) : previous(active_budget) { active_budget = value; }
            ~Scope() { active_budget = previous; }
        } scope{&budget};
        stbi_set_flip_vertically_on_load_thread(0);
        stbi_set_unpremultiply_on_load_thread(0);
        stbi_convert_iphone_png_to_rgb_thread(0);
        int decoded_width{}, decoded_height{}, channels{};
        auto* decoded = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(source.data()),
            static_cast<int>(source.size()), &decoded_width, &decoded_height, &channels, 4);
        if (!decoded)
        { if (budget.exhausted) result.status = PngStatus::allocation_failure; return result; }
        if (decoded_width != static_cast<int>(width) || decoded_height != static_cast<int>(height))
        { stbi_image_free(decoded); return result; }
        try
        {
            result.rgba.resize(static_cast<std::size_t>(pixels * 4u));
            std::memcpy(result.rgba.data(), decoded, result.rgba.size());
            result.status = PngStatus::ready;
        }
        catch (const std::bad_alloc&) { result.status = PngStatus::allocation_failure; }
        stbi_image_free(decoded);
        return result;
    }
}
