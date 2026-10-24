// SPDX-License-Identifier: MPL-2.0
#include <epochplatformengine/application.hpp>

#if defined(_WIN32) && defined(EPOCH_PLATFORM_WINDOWED_ENTRYPOINT)
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace
{
    [[nodiscard]] std::string utf8_from_wide(const wchar_t* value)
    {
        if (value == nullptr || *value == L'\0')
            return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
            return {};
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
        result.pop_back();
        return result;
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command)
{
    int wide_count{};
    wchar_t** wide_arguments = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    std::vector<std::string> storage;
    std::vector<const char*> arguments;
    storage.reserve(static_cast<std::size_t>(wide_count));
    arguments.reserve(static_cast<std::size_t>(wide_count));
    for (int index = 0; index < wide_count; ++index)
        storage.emplace_back(utf8_from_wide(wide_arguments[index]));
    for (const auto& value : storage)
        arguments.emplace_back(value.c_str());
    if (wide_arguments != nullptr)
        LocalFree(wide_arguments);

    const epochengine::platform::ApplicationLaunchInfo launch{
        .argc = wide_count,
        .argv = arguments.data(),
        .native_application_instance = instance,
        .native_show_command = show_command
    };
    epochengine::platform::ApplicationFactory factory{};
    epoch_platform_application_factory(&factory);
    return epochengine::platform::run_application(factory, launch);
}
#else
int main(const int argc, char** argv)
{
    const epochengine::platform::ApplicationLaunchInfo launch{
        .argc = argc,
        .argv = const_cast<const char* const*>(argv)
    };
    epochengine::platform::ApplicationFactory factory{};
    epoch_platform_application_factory(&factory);
    return epochengine::platform::run_application(factory, launch);
}
#endif
