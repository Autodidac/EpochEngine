module;

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <include/engine.config.hpp>

module software.context;

import core.commandline;
import core.log;

extern "C" const char* epoch_reserve_capture_path_utf8(
    const char* backend, std::uintptr_t window);
extern "C" void epoch_release_capture_path_utf8(
    const char* backend, std::uintptr_t window);

namespace epochengine::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    namespace detail
    {
        [[nodiscard]] bool write_bmp(
            const std::filesystem::path& filepath,
            const std::vector<std::uint32_t>& pixels,
            const int width,
            const int height) noexcept
        {
            if (width <= 0 || height <= 0 || pixels.empty())
                return false;

            const int rowBytes = width * 3;
            const int padSize = (4 - (rowBytes % 4)) % 4;
            const int stride = rowBytes + padSize;

            std::vector<std::uint8_t> bmpData(static_cast<std::size_t>(stride) * static_cast<std::size_t>(height), 0);

            for (int y = 0; y < height; ++y)
            {
                const int srcY = height - 1 - y;
                const auto* srcRow = pixels.data() + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(width);
                auto* dstRow = bmpData.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);

                for (int x = 0; x < width; ++x)
                {
                    const std::uint32_t pixel = srcRow[x];
                    dstRow[x * 3 + 0] = static_cast<std::uint8_t>(pixel & 0xFFu);
                    dstRow[x * 3 + 1] = static_cast<std::uint8_t>((pixel >> 8) & 0xFFu);
                    dstRow[x * 3 + 2] = static_cast<std::uint8_t>((pixel >> 16) & 0xFFu);
                }
            }

            std::uint8_t fileHeader[14] = {
                'B','M',
                0,0,0,0,
                0,0,
                0,0,
                54,0,0,0
            };

            std::uint8_t infoHeader[40] = {
                40,0,0,0,
                0,0,0,0,
                0,0,0,0,
                1,0,
                24,0,
                0,0,0,0,
                0,0,0,0,
                0,0,0,0,
                0,0,0,0,
                0,0,0,0,
                0,0,0,0
            };

            const std::uint32_t fileSize = 54u + static_cast<std::uint32_t>(bmpData.size());
            std::memcpy(&fileHeader[2], &fileSize, 4);
            std::memcpy(&infoHeader[4], &width, 4);
            std::memcpy(&infoHeader[8], &height, 4);

            std::ofstream out(filepath, std::ios::binary);
            if (!out)
                return false;

            out.write(reinterpret_cast<const char*>(fileHeader), sizeof(fileHeader));
            out.write(reinterpret_cast<const char*>(infoHeader), sizeof(infoHeader));
            out.write(reinterpret_cast<const char*>(bmpData.data()), static_cast<std::streamsize>(bmpData.size()));
            return out.good();
        }
    }

    void capture_frame_if_requested(
        const std::vector<std::uint32_t>& framebuffer,
        const int width,
        const int height,
        const std::uintptr_t windowId,
        bool projectPreviewRendered)
    {
        if (!core::cli::capture_requested || width <= 0 || height <= 0
            || framebuffer.size() / static_cast<std::size_t>(width) < static_cast<std::size_t>(height))
            return;
        if (core::cli::smoke_requested && core::cli::scene_name.starts_with("project:")
            && !projectPreviewRendered)
            return;

        const char* capturePath = epoch_reserve_capture_path_utf8("software", windowId);
        if (!capturePath || !*capturePath)
            return;

        const bool wrote = detail::write_bmp(
            std::filesystem::path(capturePath), framebuffer, width, height);
        if (!wrote)
            epoch_release_capture_path_utf8("software", windowId);
        const auto level = wrote ? epochengine::core::log::level::info : epochengine::core::log::level::warn;
        const std::string message = std::string(wrote ? "Captured software frame: " : "Failed to capture software frame: ")
            + capturePath;
        epochengine::core::log::write(
            level,
            "Software.Capture",
            { message.c_str(), message.size() });
    }
#endif
}
