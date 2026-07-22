/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

export module image.writer;

import core.logger;

export namespace epochengine
{
    inline std::vector<std::uint8_t> a_argb32ToRgba8(const std::vector<std::uint32_t>& pixels)
    {
        std::vector<std::uint8_t> rgba;
        rgba.resize(pixels.size() * 4u, 0u);

        for (std::size_t i = 0; i < pixels.size(); ++i)
        {
            const std::uint32_t pixel = pixels[i];
            rgba[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16) & 0xFFu);
            rgba[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8) & 0xFFu);
            rgba[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xFFu);
            rgba[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24) & 0xFFu);
        }

        return rgba;
    }

    inline bool a_writeBMP(const std::filesystem::path& filepath, const std::vector<std::uint8_t>& pixels, int width, int height, bool flipVertically)
    {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("a_writeBMP: Invalid dimensions");

        const int channels = 4;
        const int rowBytes = width * 3;
        const int padSize = (4 - (rowBytes % 4)) % 4;
        const int stride = rowBytes + padSize;

        std::vector<std::uint8_t> bmpData(stride * height, 0);

        for (int y = 0; y < height; ++y)
        {
            const int srcY = flipVertically ? y : (height - 1 - y);
            const auto* srcRow = pixels.data() + static_cast<std::size_t>(srcY) * width * channels;
            auto* dstRow = bmpData.data() + static_cast<std::size_t>(y) * stride;

            for (int x = 0; x < width; ++x)
            {
                dstRow[x * 3 + 0] = srcRow[x * channels + 2];
                dstRow[x * 3 + 1] = srcRow[x * channels + 1];
                dstRow[x * 3 + 2] = srcRow[x * channels + 0];
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
        return true;
    }

    inline bool a_writeTGA(const std::filesystem::path& filepath, const std::vector<std::uint8_t>& pixels, int width, int height, bool flipVertically)
    {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("a_writeTGA: Invalid dimensions");

        const int channels = 4;
        std::ofstream out(filepath, std::ios::binary);
        if (!out)
            return false;

        std::uint8_t header[18] = { 0 };
        header[2] = 2;
        header[12] = static_cast<std::uint8_t>(width & 0xFF);
        header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
        header[14] = static_cast<std::uint8_t>(height & 0xFF);
        header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
        header[16] = 32;
        header[17] = 0x20;

        out.write(reinterpret_cast<const char*>(header), sizeof(header));

        for (int y = 0; y < height; ++y)
        {
            const int srcY = flipVertically ? (height - 1 - y) : y;
            const auto* srcRow = pixels.data() + static_cast<std::size_t>(srcY) * width * channels;

            for (int x = 0; x < width; ++x)
            {
                out.put(static_cast<char>(srcRow[x * channels + 2]));
                out.put(static_cast<char>(srcRow[x * channels + 1]));
                out.put(static_cast<char>(srcRow[x * channels + 0]));
                out.put(static_cast<char>(srcRow[x * channels + 3]));
            }
        }

        return true;
    }

    inline bool a_writePPM(const std::filesystem::path& filepath, const std::vector<std::uint8_t>& pixels, int width, int height, bool flipVertically)
    {
        if (width <= 0 || height <= 0)
            throw std::runtime_error("a_writePPM: Invalid dimensions");

        const int channels = 4;
        std::ofstream out(filepath, std::ios::binary);
        if (!out)
            return false;

        out << "P6\n" << width << " " << height << "\n255\n";

        for (int y = 0; y < height; ++y)
        {
            const int srcY = flipVertically ? (height - 1 - y) : y;
            const auto* srcRow = pixels.data() + static_cast<std::size_t>(srcY) * width * channels;

            for (int x = 0; x < width; ++x)
            {
                out.put(static_cast<char>(srcRow[x * channels + 0]));
                out.put(static_cast<char>(srcRow[x * channels + 1]));
                out.put(static_cast<char>(srcRow[x * channels + 2]));
            }
        }

        return true;
    }

    inline bool a_writeImage(const std::filesystem::path& filepath, const std::vector<std::uint8_t>& pixels, int width, int height, bool flipVertically = false)
    {
        auto ext = filepath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".bmp") return a_writeBMP(filepath, pixels, width, height, flipVertically);
        if (ext == ".tga") return a_writeTGA(filepath, pixels, width, height, flipVertically);
        if (ext == ".ppm") return a_writePPM(filepath, pixels, width, height, flipVertically);

        logger::errorf_loc("ImageWriter", std::source_location::current(), "Unsupported image format for writing: {}", ext);
        return false;
    }

    inline bool a_writeImageFromArgb32(
        const std::filesystem::path& filepath,
        const std::vector<std::uint32_t>& pixels,
        int width,
        int height,
        bool flipVertically = false)
    {
        return a_writeImage(filepath, a_argb32ToRgba8(pixels), width, height, flipVertically);
    }
}
