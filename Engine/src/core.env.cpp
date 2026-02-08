module;

#include "../include/_epoch.stl_types.hpp"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <cstdlib>
#endif

module core.env;

namespace epoch::core::env
{
#if defined(_WIN32)
    namespace
    {
        std::wstring utf8_to_wide(epoch::string_view s)
        {
            if (s.empty()) return {};
            const int needed = MultiByteToWideChar(CP_UTF8, 0, s.data, (int)s.size, nullptr, 0);
            if (needed <= 0) return {};
            std::wstring w;
            w.resize((size_t)needed);
            MultiByteToWideChar(CP_UTF8, 0, s.data, (int)s.size, w.data(), needed);
            return w;
        }

        std::string wide_to_utf8(std::wstring_view w)
        {
            if (w.empty()) return {};
            const int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
            if (needed <= 0) return {};
            std::string s;
            s.resize((size_t)needed);
            WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), needed, nullptr, nullptr);
            return s;
        }
    }
#endif

    epoch::optional<epoch::string> get(epoch::string_view name)
    {
#if defined(_WIN32)
        const std::wstring wname = utf8_to_wide(name);
        if (wname.empty()) return std::nullopt;

        DWORD needed = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
        if (needed == 0) return std::nullopt;

        std::wstring buf;
        buf.resize((size_t)needed);
        const DWORD got = GetEnvironmentVariableW(wname.c_str(), buf.data(), needed);
        if (got == 0) return std::nullopt;

        if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
        return epoch::string{ wide_to_utf8(buf) };
#else
        const std::string key{ epoch::to_std(name) }; // copy
        if (const char* v = std::getenv(key.c_str()); v != nullptr)
            return epoch::string{ v };
        return std::nullopt;
#endif
    }

    bool set(epoch::string_view name, epoch::string_view value)
    {
#if defined(_WIN32)
        const std::wstring wname = utf8_to_wide(name);
        const std::wstring wval = utf8_to_wide(value);
        if (wname.empty()) return false;
        return SetEnvironmentVariableW(wname.c_str(), wval.c_str()) != 0;
#else
        const std::string n{ epoch::to_std(name) };
        const std::string v{ epoch::to_std(value) };
        return ::setenv(n.c_str(), v.c_str(), 1) == 0;
#endif
    }

    bool unset(epoch::string_view name)
    {
#if defined(_WIN32)
        const std::wstring wname = utf8_to_wide(name);
        if (wname.empty()) return false;
        return SetEnvironmentVariableW(wname.c_str(), nullptr) != 0;
#else
        const std::string n{ epoch::to_std(name) };
        return ::unsetenv(n.c_str()) == 0;
#endif
    }
}
