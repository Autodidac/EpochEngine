module;

#include <filesystem>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <vector>
#else
#  include <unistd.h>
#  include <vector>
#endif

module core.path;

namespace epoch::core::path
{
    path executable_path()
    {
#if defined(_WIN32)
        std::wstring buf;
        buf.resize(32768);
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n == 0) return {};
        buf.resize((size_t)n);
        return path{buf};
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> tmp(size + 1u, '\0');
        if (_NSGetExecutablePath(tmp.data(), &size) != 0) return {};
        return path{tmp.data()};
#else
        std::vector<char> tmp(4096, '\0');
        for (;;)
        {
            const ssize_t n = ::readlink("/proc/self/exe", tmp.data(), tmp.size() - 1);
            if (n < 0) return {};
            if ((size_t)n < tmp.size() - 1)
            {
                tmp[(size_t)n] = '\0';
                return path{tmp.data()};
            }
            tmp.resize(tmp.size() * 2);
        }
#endif
    }

    path executable_dir()
    {
        const auto p = executable_path();
        if (p.empty()) return {};
        return p.parent_path();
    }

    path normalize(const path& p)
    {
        return p.lexically_normal();
    }

    path join(const path& p, const path& child)
    {
        return normalize(p / child);
    }
}
