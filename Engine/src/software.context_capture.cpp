module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <include/aengine.config.hpp>

module software.context;

import core.commandline;
import core.log;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    namespace detail
    {
        [[nodiscard]] std::mutex& capture_mutex() noexcept
        {
            static std::mutex mutex{};
            return mutex;
        }

        [[nodiscard]] std::unordered_set<std::string>& captured_outputs()
        {
            static std::unordered_set<std::string> outputs{};
            return outputs;
        }

        [[nodiscard]] std::unordered_map<std::string, std::uint32_t>& capture_frame_counts()
        {
            static std::unordered_map<std::string, std::uint32_t> counts{};
            return counts;
        }

        [[nodiscard]] std::filesystem::path capture_output_root()
        {
#if defined(_WIN32)
            char* env = nullptr;
            std::size_t envLength = 0;
            if (_dupenv_s(&env, &envLength, "EPOCH_CAPTURE_DIR") == 0 && env && *env)
            {
                const std::filesystem::path overridePath(env);
                std::free(env);
                return overridePath;
            }
            std::free(env);
#else
            if (const char* env = std::getenv("EPOCH_CAPTURE_DIR"); env && *env)
                return std::filesystem::path(env);
#endif

            return std::filesystem::current_path() / "logs" / "captures";
        }

        [[nodiscard]] std::string sanitize_capture_token(const std::string_view value)
        {
            std::string token;
            token.reserve(value.size());

            bool previousWasDash = false;
            for (const unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0)
                {
                    token.push_back(static_cast<char>(std::tolower(ch)));
                    previousWasDash = false;
                }
                else if (!previousWasDash)
                {
                    token.push_back('-');
                    previousWasDash = true;
                }
            }

            while (!token.empty() && token.front() == '-')
                token.erase(token.begin());
            while (!token.empty() && token.back() == '-')
                token.pop_back();

            return token.empty() ? std::string{ "capture" } : token;
        }

        [[nodiscard]] std::string capture_output_stem()
        {
            if (!core::cli::scene_name.empty())
                return sanitize_capture_token(core::cli::scene_name);

            if (!core::cli::exe_path.empty())
                return sanitize_capture_token(core::cli::exe_path.stem().string());

            return "capture";
        }

        [[nodiscard]] std::filesystem::path reserve_capture_path(
            const std::string_view backend,
            const std::uintptr_t windowId,
            const std::string_view extension = ".bmp")
        {
            if (!core::cli::capture_requested)
                return {};

            const std::string stem = capture_output_stem();
            const std::string backendToken = sanitize_capture_token(backend);
            const std::string key = stem + "|" + backendToken + "|" + std::to_string(windowId);

            std::lock_guard guard(capture_mutex());
            const auto frameCount = ++capture_frame_counts()[key];
            if (core::cli::smoke_requested && frameCount < core::cli::capture_warmup_frames)
                return {};

            if (!captured_outputs().insert(key).second)
                return {};

            const auto root = capture_output_root();
            std::error_code ec;
            std::filesystem::create_directories(root, ec);

            std::string filename = stem + "-" + backendToken;
            if (windowId != 0)
                filename += "-" + std::to_string(windowId);
            filename += std::string(extension);
            return root / filename;
        }

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
        const std::uintptr_t windowId)
    {
        const auto capturePath = detail::reserve_capture_path("software", windowId);
        if (capturePath.empty() || framebuffer.empty() || width <= 0 || height <= 0)
            return;

        const bool wrote = detail::write_bmp(capturePath, framebuffer, width, height);
        const auto level = wrote ? epoch::core::log::level::info : epoch::core::log::level::warn;
        const std::string message = std::string(wrote ? "Captured software frame: " : "Failed to capture software frame: ")
            + capturePath.string();
        epoch::core::log::write(
            level,
            "Software.Capture",
            { message.c_str(), message.size() });
    }
#endif
}
