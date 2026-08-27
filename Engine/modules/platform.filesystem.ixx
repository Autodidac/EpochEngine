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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

export module platform.filesystem;

export namespace epochengine::platform::filesystem
{
    struct OpenFileDialogResult final
    {
        bool accepted{};
        bool cancelled{};
        std::filesystem::path path{};
        std::string diagnostic{};
    };

    [[nodiscard]] inline OpenFileDialogResult open_file_dialog(
        std::wstring_view title,
        std::wstring_view filter_label,
        std::wstring_view filter_pattern,
        const std::filesystem::path& initial_directory = {}) noexcept
    {
#if defined(_WIN32)
        try
        {
            std::array<wchar_t, 32'768> selected{};
            std::wstring titleStorage{title};
            std::wstring initialStorage = initial_directory.empty()
                ? std::wstring{}
                : initial_directory.wstring();
            std::wstring filterStorage{filter_label};
            filterStorage.push_back(L'\0');
            filterStorage.append(filter_pattern);
            filterStorage.push_back(L'\0');
            filterStorage.append(L"All files");
            filterStorage.push_back(L'\0');
            filterStorage.append(L"*.*");
            filterStorage.push_back(L'\0');
            filterStorage.push_back(L'\0');

            OPENFILENAMEW request{};
            request.lStructSize = sizeof(request);
            request.hwndOwner = ::GetForegroundWindow();
            request.lpstrFilter = filterStorage.c_str();
            request.lpstrFile = selected.data();
            request.nMaxFile = static_cast<DWORD>(selected.size());
            request.lpstrInitialDir = initialStorage.empty()
                ? nullptr
                : initialStorage.c_str();
            request.lpstrTitle = titleStorage.c_str();
            request.Flags = OFN_DONTADDTORECENT
                | OFN_FILEMUSTEXIST
                | OFN_PATHMUSTEXIST
                | OFN_NOCHANGEDIR;

            if (::GetOpenFileNameW(&request) != FALSE)
            {
                return {
                    .accepted = true,
                    .cancelled = false,
                    .path = std::filesystem::path{selected.data()},
                    .diagnostic = "File selected."
                };
            }

            const DWORD error = ::CommDlgExtendedError();
            if (error == 0u)
            {
                return {
                    .accepted = false,
                    .cancelled = true,
                    .diagnostic = "File selection cancelled."
                };
            }
            return {
                .accepted = false,
                .cancelled = false,
                .diagnostic = "Windows file selection failed with common-dialog error "
                    + std::to_string(static_cast<unsigned long>(error))
                    + "."
            };
        }
        catch (...)
        {
            return {
                .accepted = false,
                .cancelled = false,
                .diagnostic = "File selection failed before the native dialog completed."
            };
        }
#else
        (void)title;
        (void)filter_label;
        (void)filter_pattern;
        (void)initial_directory;
        return {
            .accepted = false,
            .cancelled = false,
            .diagnostic =
                "The native file-selection adapter is not available on this platform build."
        };
#endif
    }

    struct SaveFileDialogResult final
    {
        bool accepted{};
        bool cancelled{};
        std::filesystem::path path{};
        std::string diagnostic{};
    };

    [[nodiscard]] inline SaveFileDialogResult save_file_dialog(
        std::wstring_view title,
        std::wstring_view filter_label,
        std::wstring_view filter_pattern,
        std::wstring_view default_filename,
        std::wstring_view default_extension,
        const std::filesystem::path& initial_directory = {}) noexcept
    {
#if defined(_WIN32)
        try
        {
            std::array<wchar_t, 32'768> selected{};
            const std::size_t filenameLength = (std::min)(
                default_filename.size(),
                selected.size() - 1u);
            std::copy_n(
                default_filename.data(),
                filenameLength,
                selected.data());

            std::wstring titleStorage{title};
            std::wstring extensionStorage{default_extension};
            std::wstring initialStorage = initial_directory.empty()
                ? std::wstring{}
                : initial_directory.wstring();
            std::wstring filterStorage{filter_label};
            filterStorage.push_back(L'\0');
            filterStorage.append(filter_pattern);
            filterStorage.push_back(L'\0');
            filterStorage.append(L"All files");
            filterStorage.push_back(L'\0');
            filterStorage.append(L"*.*");
            filterStorage.push_back(L'\0');
            filterStorage.push_back(L'\0');

            OPENFILENAMEW request{};
            request.lStructSize = sizeof(request);
            request.hwndOwner = ::GetForegroundWindow();
            request.lpstrFilter = filterStorage.c_str();
            request.lpstrFile = selected.data();
            request.nMaxFile = static_cast<DWORD>(selected.size());
            request.lpstrInitialDir = initialStorage.empty()
                ? nullptr
                : initialStorage.c_str();
            request.lpstrTitle = titleStorage.c_str();
            request.lpstrDefExt = extensionStorage.empty()
                ? nullptr
                : extensionStorage.c_str();
            request.Flags = OFN_DONTADDTORECENT
                | OFN_OVERWRITEPROMPT
                | OFN_PATHMUSTEXIST
                | OFN_NOCHANGEDIR;

            if (::GetSaveFileNameW(&request) != FALSE)
            {
                return {
                    .accepted = true,
                    .cancelled = false,
                    .path = std::filesystem::path{selected.data()},
                    .diagnostic = "File destination selected."
                };
            }

            const DWORD error = ::CommDlgExtendedError();
            if (error == 0u)
            {
                return {
                    .accepted = false,
                    .cancelled = true,
                    .diagnostic = "File save cancelled."
                };
            }
            return {
                .accepted = false,
                .cancelled = false,
                .diagnostic = "Windows file save failed with common-dialog error "
                    + std::to_string(static_cast<unsigned long>(error))
                    + "."
            };
        }
        catch (...)
        {
            return {
                .accepted = false,
                .cancelled = false,
                .diagnostic =
                    "File save failed before the native dialog completed."
            };
        }
#else
        (void)title;
        (void)filter_label;
        (void)filter_pattern;
        (void)default_filename;
        (void)default_extension;
        (void)initial_directory;
        return {
            .accepted = false,
            .cancelled = false,
            .diagnostic =
                "The native file-save adapter is not available on this platform build."
        };
#endif
    }

    [[nodiscard]] inline bool exclusive_create_and_write(
        const std::filesystem::path& destination,
        std::span<const std::byte> bytes,
        std::error_code& error) noexcept
    {
        error.clear();
#if defined(_WIN32)
        const HANDLE file = ::CreateFileW(
            destination.c_str(),
            GENERIC_WRITE,
            0u,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            error = std::error_code(
                static_cast<int>(::GetLastError()),
                std::system_category());
            return false;
        }

        bool complete = true;
        std::size_t offset{};
        while (offset < bytes.size())
        {
            const std::size_t remaining = bytes.size() - offset;
            const DWORD request = static_cast<DWORD>((std::min)(
                remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD written{};
            if (::WriteFile(
                    file,
                    bytes.data() + offset,
                    request,
                    &written,
                    nullptr) == FALSE
                || written == 0u)
            {
                complete = false;
                error = std::error_code(
                    static_cast<int>(::GetLastError()),
                    std::system_category());
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
        if (complete && ::FlushFileBuffers(file) == FALSE)
        {
            complete = false;
            error = std::error_code(
                static_cast<int>(::GetLastError()),
                std::system_category());
        }
        if (::CloseHandle(file) == FALSE && complete)
        {
            complete = false;
            error = std::error_code(
                static_cast<int>(::GetLastError()),
                std::system_category());
        }
        if (!complete)
        {
            std::error_code ignored{};
            (void)std::filesystem::remove(destination, ignored);
        }
        return complete;
#else
        const int file = ::open(
            destination.c_str(),
            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
            0666);
        if (file < 0)
        {
            error = std::error_code(errno, std::generic_category());
            return false;
        }

        bool complete = true;
        std::size_t offset{};
        while (offset < bytes.size())
        {
            const ssize_t written = ::write(
                file,
                bytes.data() + offset,
                bytes.size() - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
            {
                complete = false;
                error = std::error_code(errno, std::generic_category());
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
        if (complete && ::fsync(file) != 0)
        {
            complete = false;
            error = std::error_code(errno, std::generic_category());
        }
        if (::close(file) != 0 && complete)
        {
            complete = false;
            error = std::error_code(errno, std::generic_category());
        }
        if (!complete)
            (void)::unlink(destination.c_str());
        return complete;
#endif
    }

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
