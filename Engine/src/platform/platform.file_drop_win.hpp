// Epoch Engine — LicenseRef-MIT-NoSell
// Native file-drop ownership and bounded copying; no file reads or shell calls.
#pragma once
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <utility>
#include <vector>
#pragma comment(lib, "shell32.lib")
namespace epochengine::platform_file_drop
{
    class OwnedDrop final
    {
    public:
        explicit OwnedDrop(HDROP value) noexcept : value_{value} {}
        OwnedDrop(const OwnedDrop&) = delete;
        OwnedDrop& operator=(const OwnedDrop&) = delete;
        ~OwnedDrop() { if (value_) ::DragFinish(value_); }
        [[nodiscard]] HDROP get() const noexcept { return value_; }
        [[nodiscard]] std::vector<std::string> files() const
        {
            const UINT count = value_ ? ::DragQueryFileW(value_, 0xffffffffu, nullptr, 0u) : 0u;
            if (count == 0u || count > 16u) return {};
            std::vector<std::string> result;
            std::size_t total{};
            for (UINT index = 0u; index < count; ++index)
            {
                const UINT length = ::DragQueryFileW(value_, index, nullptr, 0u);
                if (length == 0u || length > 32767u || (total += length) > 65536u) return {};
                std::wstring path(length + 1u, L'\0');
                if (::DragQueryFileW(value_, index, path.data(), length + 1u) != length) return {};
                const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                    path.data(), static_cast<int>(length), nullptr, 0, nullptr, nullptr);
                if (size <= 0) return {};
                std::string utf8(static_cast<std::size_t>(size), '\0');
                if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(),
                    static_cast<int>(length), utf8.data(), size, nullptr, nullptr) != size) return {};
                result.push_back(std::move(utf8));
            }
            return result;
        }
    private:
        HDROP value_{};
    };
}
#endif
