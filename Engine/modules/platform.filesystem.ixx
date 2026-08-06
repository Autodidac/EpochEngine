/************************************************
 *   This file is part of the Epoch Project.    *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   See LICENSE file for full terms.           *
 ************************************************/
module;

#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

export module platform.filesystem;

export namespace epochengine::platform::filesystem
{
    [[nodiscard]] inline bool atomic_replace_same_filesystem(
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        std::error_code& error) noexcept
    {
        error.clear();

#if defined(_WIN32)
        if (::MoveFileExW(
                source.c_str(),
                destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE)
        {
            return true;
        }

        error = std::error_code(
            static_cast<int>(::GetLastError()),
            std::system_category());
        return false;
#else
        std::filesystem::rename(source, destination, error);
        return !error;
#endif
    }
}
