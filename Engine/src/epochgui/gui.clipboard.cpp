// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <cstring>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include "../platform/platform.framework.hpp"
#endif

module gui.engine;

namespace epochengine::gui::clipboard_detail
{
#if defined(_WIN32)
    [[nodiscard]] static std::wstring utf8_to_wide(std::string_view text)
    {
        if (text.empty())
            return {};

        const int required = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);

        if (required <= 0)
        {
            std::wstring fallback{};
            fallback.reserve(text.size());
            for (unsigned char ch : text)
                fallback.push_back(ch < 128u ? static_cast<wchar_t>(ch) : L'?');
            return fallback;
        }

        std::wstring out(static_cast<std::size_t>(required), L'\0');
        ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            out.data(),
            required);
        return out;
    }

    [[nodiscard]] static std::string wide_to_utf8(std::wstring_view text)
    {
        if (text.empty())
            return {};

        const int required = ::WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (required <= 0)
        {
            std::string fallback{};
            fallback.reserve(text.size());
            for (wchar_t ch : text)
                fallback.push_back((ch >= 0 && ch < 128) ? static_cast<char>(ch) : '?');
            return fallback;
        }

        std::string out(static_cast<std::size_t>(required), '\0');
        ::WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            out.data(),
            required,
            nullptr,
            nullptr);
        return out;
    }

    [[nodiscard]] std::string clipboard_read_text()
    {
        std::string out{};
        if (!::OpenClipboard(nullptr))
            return out;

        if (HANDLE handle = ::GetClipboardData(CF_UNICODETEXT))
        {
            if (const auto* wide = static_cast<const wchar_t*>(::GlobalLock(handle)))
            {
                out = wide_to_utf8(std::wstring_view{ wide });
                ::GlobalUnlock(handle);
            }
        }

        ::CloseClipboard();
        return out;
    }

    [[nodiscard]] bool clipboard_write_text(std::string_view text)
    {
        if (!::OpenClipboard(nullptr))
            return false;

        const std::wstring wide = utf8_to_wide(text);
        const SIZE_T bytes = (wide.size() + 1u) * sizeof(wchar_t);
        HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
        {
            ::CloseClipboard();
            return false;
        }

        bool wrote = false;
        if (void* locked = ::GlobalLock(memory))
        {
            std::memcpy(locked, wide.c_str(), bytes);
            ::GlobalUnlock(memory);
            ::EmptyClipboard();
            if (!::SetClipboardData(CF_UNICODETEXT, memory))
                ::GlobalFree(memory);
            else
                wrote = true;
        }
        else
        {
            ::GlobalFree(memory);
        }

        ::CloseClipboard();
        return wrote;
    }
#else
    [[nodiscard]] std::string clipboard_read_text()
    {
        return {};
    }

    [[nodiscard]] bool clipboard_write_text(std::string_view)
    {
        return false;
    }
#endif
}
