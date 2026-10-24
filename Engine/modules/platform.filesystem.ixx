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
// Epoch system notes (auto-checked):
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <utility>
#include <cstring>
#include "../src/platform/platform.image_codec_win.hpp"
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
#include <shellapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

export module platform.filesystem;

export namespace epochengine::platform::filesystem
{
    struct ImageThumbnail final
    {
        std::vector<std::uint8_t> rgba{};
        std::uint32_t width{}, height{};
    };

    [[nodiscard]] inline ImageThumbnail image_thumbnail(std::string_view encoded) noexcept
    {
#if defined(_WIN32)
        try {
            auto decoded = platform_image_codec::decode_thumbnail(encoded);
            return {std::move(decoded.rgba), decoded.width, decoded.height};
        } catch (...) { return {}; }
#else
        return {};
#endif
    }

    struct ClipboardReadResult final
    {
        std::vector<std::filesystem::path> files{};
        std::filesystem::path image{};
        std::string diagnostic{};
        bool recognized{};
    };

    // Read only on an explicit Paste action. No clipboard polling or model access.
    [[nodiscard]] inline ClipboardReadResult read_clipboard_attachment(
        const std::filesystem::path& imageDestination) noexcept
    {
#if defined(_WIN32)
        try
        {
            if (!::OpenClipboard(nullptr)) return {{}, {}, "Clipboard is busy. Try again.", true};
            struct ClipboardLease { bool open{true}; ~ClipboardLease(){if(open)::CloseClipboard();} } lease;
            ClipboardReadResult result{};
            if (::IsClipboardFormatAvailable(CF_HDROP))
            {
                result.recognized = true;
                const auto drop = static_cast<HDROP>(::GetClipboardData(CF_HDROP));
                const auto count = drop ? ::DragQueryFileW(drop, 0xffffffffu, nullptr, 0) : 0u;
                if (count == 0u || count > 16u) return {{}, {}, "Paste up to 16 files at a time.", true};
                for (UINT index = 0u; index < count; ++index)
                {
                    const auto length = ::DragQueryFileW(drop, index, nullptr, 0);
                    if (length == 0u || length > 32767u) return {{}, {}, "Clipboard file path is invalid.", true};
                    std::wstring path(length + 1u, L'\0');
                    if (::DragQueryFileW(drop, index, path.data(), length + 1u) != length)
                        return {{}, {}, "Could not read the clipboard file list.", true};
                    path.resize(length); result.files.emplace_back(path);
                }
                return result;
            }
            constexpr std::size_t limit = 4u * 1024u * 1024u;
            const UINT pngFormat = ::RegisterClipboardFormatW(L"PNG");
            if (pngFormat && ::IsClipboardFormatAvailable(pngFormat))
            {
                result.recognized = true;
                const HANDLE data = ::GetClipboardData(pngFormat);
                const SIZE_T size = data ? ::GlobalSize(data) : 0u;
                if (size < 24u || size > limit) return {{}, {}, "Clipboard PNG exceeds the 4 MiB image limit.", true};
                const auto* bytes = static_cast<const unsigned char*>(::GlobalLock(data));
                if (!bytes) return {{}, {}, "Could not read clipboard PNG.", true};
                const bool valid = std::memcmp(bytes, "\x89PNG\r\n\x1a\n", 8u) == 0;
                std::vector<unsigned char> image;
                if (valid) image.assign(bytes, bytes + size);
                ::GlobalUnlock(data);
                if (!valid) return {{}, {}, "Clipboard image is not a valid PNG envelope.", true};
                ::CloseClipboard(); lease.open = false;
                std::error_code ec; std::filesystem::create_directories(imageDestination.parent_path(), ec);
                if (ec) return {{}, {}, "Cannot create the local attachment folder.", true};
                std::ofstream out{imageDestination, std::ios::binary | std::ios::out | std::ios::noreplace};
                out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
                out.close();
                if (!out) return {{}, {}, "Cannot save the clipboard attachment.", true};
                return {{}, imageDestination, "Image attached from clipboard.", true};
            }
            if (::IsClipboardFormatAvailable(CF_BITMAP) || ::IsClipboardFormatAvailable(CF_DIB)
                || ::IsClipboardFormatAvailable(CF_DIBV5))
            {
                result.recognized = true;
                const auto source = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
                BITMAP info{};
                if (!source || !::GetObjectW(source, sizeof(info), &info) || info.bmWidth <= 0
                    || info.bmHeight <= 0 || info.bmWidth > 8192 || info.bmHeight > 8192
                    || static_cast<std::uint64_t>(info.bmWidth) * info.bmHeight > 32'000'000u)
                    return {{}, {}, "Clipboard image dimensions are invalid or too large.", true};
                const auto bitmap = static_cast<HBITMAP>(::CopyImage(source, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
                ::CloseClipboard(); lease.open = false;
                if (!bitmap) return {{}, {}, "Could not copy clipboard image pixels.", true};
                struct BitmapLease { HBITMAP bitmap; ~BitmapLease(){::DeleteObject(bitmap);} } pixels{bitmap};
                const auto encoded = platform_image_codec::encode_png(bitmap, info.bmWidth, info.bmHeight,
                    imageDestination, 2048.0, 2048.0);
                if (!encoded.succeeded) return {{}, {}, encoded.message, true};
                return {{}, imageDestination, "Image attached from clipboard (up to 2048 px).", true};
            }
            return result;
        }
        catch (...) { return {{}, {}, "Clipboard attachment could not be read.", true}; }
#else
        (void)imageDestination;
        return {};
#endif
    }

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

    // Publish an already flushed temporary file without replacing a destination
    // created since admission. Windows hard-link creation can be denied even
    // when ordinary owned-file creation/rename is permitted.
    [[nodiscard]] inline bool atomic_publish_new_same_filesystem(
        const std::filesystem::path& source,
        const std::filesystem::path& destination,
        std::error_code& error) noexcept
    {
        error.clear();
#if defined(_WIN32)
        if (::MoveFileExW(source.c_str(), destination.c_str(),
                MOVEFILE_WRITE_THROUGH) != FALSE)
            return true;
        error = std::error_code(static_cast<int>(::GetLastError()),
            std::system_category());
        return false;
#else
        // link is atomic and refuses an existing destination, unlike rename.
        if (::link(source.c_str(), destination.c_str()) != 0)
        {
            error = std::error_code(errno, std::generic_category());
            return false;
        }
        // Publication succeeded. Cleanup failure must not report a failed
        // publication after the destination has already become visible.
        (void)::unlink(source.c_str());
        return true;
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
