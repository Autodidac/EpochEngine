module;

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <Windows.h>
#endif

export module autility.string.converter;

import <filesystem>;
import <string>;
import <string_view>;

export namespace almondnamespace::text
{
    // -----------------------------------------
    // char8_t helpers
    // -----------------------------------------
    export inline std::string u8_to_string(std::u8string_view s)
    {
        // Byte-preserving conversion (UTF-8 bytes).
        return std::string(reinterpret_cast<const char*>(s.data()), s.size());
    }

    // -----------------------------------------
    // UTF-16 (wchar_t on Windows) <-> UTF-8
    // -----------------------------------------
#if defined(_WIN32)

    export inline std::string narrow_utf8(std::wstring_view wide)
    {
        if (wide.empty()) return {};

        const int needed = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.data(), static_cast<int>(wide.size()),
            nullptr, 0, nullptr, nullptr);

        if (needed <= 0) return {};

        std::string out(static_cast<std::size_t>(needed), '\0');

        const int written = ::WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.data(), static_cast<int>(wide.size()),
            out.data(), needed, nullptr, nullptr);

        if (written != needed)
            out.resize(written > 0 ? static_cast<std::size_t>(written) : 0);

        return out;
    }

    export inline std::wstring widen_utf16(std::string_view utf8)
    {
        if (utf8.empty()) return {};

        const int needed = ::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            nullptr, 0);

        if (needed <= 0) return {};

        std::wstring out(static_cast<std::size_t>(needed), L'\0');

        const int written = ::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            out.data(), needed);

        if (written != needed)
            out.resize(written > 0 ? static_cast<std::size_t>(written) : 0);

        return out;
    }

#else

    // Non-Windows: paths are typically UTF-8 byte strings already.
    // These are best-effort and only reliably round-trip ASCII.
    export inline std::string narrow_utf8(std::wstring_view wide)
    {
        std::string out;
        out.reserve(wide.size());
        for (wchar_t wc : wide)
            out.push_back(static_cast<char>(wc <= 0x7F ? wc : '?'));
        return out;
    }

    export inline std::wstring widen_utf16(std::string_view utf8)
    {
        std::wstring out;
        out.reserve(utf8.size());
        for (unsigned char c : utf8)
            out.push_back(static_cast<wchar_t>(c));
        return out;
    }

#endif

    // -----------------------------------------
    // filesystem::path -> UTF-8 std::string
    // -----------------------------------------
    export inline std::string path_to_utf8(const std::filesystem::path& p)
    {
#if defined(_WIN32)
        // Avoid locale-dependent .string() and avoid extra wstring() work.
        return narrow_utf8(p.native());
#else
        // C++20/23: u8string() returns std::u8string
        return u8_to_string(p.u8string());
#endif
    }

} // namespace almondnamespace::text
