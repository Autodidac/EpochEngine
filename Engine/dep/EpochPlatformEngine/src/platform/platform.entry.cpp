// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <string>
#include <vector>
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#endif
import platform.application;
import core.log;

namespace
{
    int enter_application(int argc, char** argv) noexcept
    {
        try
        {
            const auto* options = app_get_launch_options(argc, argv);
            const auto* callbacks = app_get_callbacks();
            if (!options || !callbacks)
            {
                epochengine::core::log::error("Platform.Entry", "Application did not provide launch options and callbacks.");
                return 2;
            }
            const auto report = epochengine::platform::application::run(*options, *callbacks);
            if (report.succeeded()) return 0;
            return report.application_init_code != 0 ? report.application_init_code : 1;
        }
        catch (...)
        {
            // Callback-provider failures cannot bypass the executable boundary.
            return 2;
        }
    }
}

// This translation unit is an opt-in object target. Merely linking PlatformEngine
// never injects main or selects an application/editor/updater composition.
int main(int argc, char** argv)
{
    return enter_application(argc, argv);
}

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    struct Arguments
    {
        LPWSTR* values{};
        ~Arguments() { if (values) ::LocalFree(values); }
    };
    try
    {
        int count{};
        Arguments wide{::CommandLineToArgvW(::GetCommandLineW(), &count)};
        if (!wide.values) return 2;
        std::vector<std::string> storage;
        storage.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
        {
            const auto bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                wide.values[index], -1, nullptr, 0, nullptr, nullptr);
            if (bytes <= 0) return 2;
            std::string value(static_cast<std::size_t>(bytes), '\0');
            if (!::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                    wide.values[index], -1, value.data(), bytes, nullptr, nullptr)) return 2;
            value.pop_back();
            storage.push_back(std::move(value));
        }
        std::vector<char*> argv;
        argv.reserve(storage.size() + 1);
        for (auto& value : storage) argv.push_back(value.data());
        argv.push_back(nullptr);
        return enter_application(count, argv.data());
    }
    catch (...) { return 2; }
}
#endif
