// Epoch Engine — LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
// Copy the pixels of this editor window as an ordinary Windows clipboard DIB.
// No model access, disk write, filesystem scan, or desktop-wide capture.
#include <cstdint>
#include <string>

#if defined(_WIN32)
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <cstring>
# include <limits>
#endif

namespace epochengine::editor_clipboard_screenshot
{
    struct Result final { bool ok{}; std::string message{}; };

    [[nodiscard]] inline Result copy_window(std::uintptr_t nativeWindow)
    {
#if defined(_WIN32)
        HWND hwnd = reinterpret_cast<HWND>(nativeWindow);
        if (!hwnd || !::IsWindow(hwnd) || !::IsWindowVisible(hwnd))
            return {false, "Editor window is not available for capture."};
        DWORD pid{};
        (void)::GetWindowThreadProcessId(hwnd, &pid);
        if (pid != ::GetCurrentProcessId())
            return {false, "Screenshot refused: target is not an Epoch-owned window."};
        // Render contexts can be child windows. Capture their owned top-level
        // docking window so the native title bar is included as requested.
        if (const HWND root = ::GetAncestor(hwnd, GA_ROOT); root && root != hwnd)
        {
            DWORD rootPid{};
            (void)::GetWindowThreadProcessId(root, &rootPid);
            if (rootPid == ::GetCurrentProcessId()) hwnd = root;
        }
        RECT bounds{};
        if (!::GetWindowRect(hwnd, &bounds))
            return {false, "Could not read editor window dimensions."};
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        if (width < 16 || height < 16 || width > 8192 || height > 8192
            || static_cast<std::uint64_t>(width) * height > 32'000'000ull)
            return {false, "Screenshot dimensions exceed the safe capture budget."};

        HDC source = ::GetWindowDC(hwnd);
        if (!source) return {false, "Could not obtain editor window pixels."};
        HDC surface = ::CreateCompatibleDC(source);
        if (!surface) { ::ReleaseDC(hwnd, source); return {false, "Screenshot surface allocation failed."}; }
        HBITMAP bitmap = ::CreateCompatibleBitmap(source, width, height);
        if (!bitmap)
        {
            ::DeleteDC(surface);
            ::ReleaseDC(hwnd, source);
            return {false, "Screenshot bitmap allocation failed."};
        }
        HGDIOBJ previous = ::SelectObject(surface, bitmap);
        const BOOL copied = ::BitBlt(surface, 0, 0, width, height, source, 0, 0, SRCCOPY | CAPTUREBLT);
        ::SelectObject(surface, previous);
        ::DeleteDC(surface);
        if (!copied)
        {
            ::DeleteObject(bitmap);
            ::ReleaseDC(hwnd, source);
            return {false, "Windows could not copy the rendered editor window."};
        }

        const std::size_t pixelBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
        const std::size_t allocation = sizeof(BITMAPINFOHEADER) + pixelBytes;
        HGLOBAL dib = ::GlobalAlloc(GMEM_MOVEABLE, allocation);
        if (!dib)
        {
            ::DeleteObject(bitmap);
            ::ReleaseDC(hwnd, source);
            return {false, "Clipboard bitmap allocation failed."};
        }
        void* destination = ::GlobalLock(dib);
        if (!destination)
        {
            ::GlobalFree(dib);
            ::DeleteObject(bitmap);
            ::ReleaseDC(hwnd, source);
            return {false, "Clipboard bitmap lock failed."};
        }
        BITMAPINFOHEADER header{};
        header.biSize = sizeof(BITMAPINFOHEADER);
        header.biWidth = width;
        header.biHeight = height; // DIB is bottom-up.
        header.biPlanes = 1;
        header.biBitCount = 32;
        header.biCompression = BI_RGB;
        header.biSizeImage = static_cast<DWORD>(pixelBytes);
        std::memcpy(destination, &header, sizeof(header));
        BITMAPINFO info{};
        info.bmiHeader = header;
        const int converted = ::GetDIBits(source, bitmap, 0, static_cast<UINT>(height),
            static_cast<unsigned char*>(destination) + sizeof(header), &info, DIB_RGB_COLORS);
        ::GlobalUnlock(dib);
        ::ReleaseDC(hwnd, source);
        ::DeleteObject(bitmap);
        if (converted != height)
        {
            ::GlobalFree(dib);
            return {false, "Could not convert screenshot to clipboard image format."};
        }
        if (!::OpenClipboard(hwnd))
        {
            ::GlobalFree(dib);
            return {false, "Clipboard is busy. Try again."};
        }
        bool success = false;
        if (::EmptyClipboard())
            success = ::SetClipboardData(CF_DIB, dib) != nullptr;
        ::CloseClipboard();
        if (!success) ::GlobalFree(dib); // Ownership transfers on success only.
        return success ? Result{true, "Entire Epoch window copied as an image; paste with Ctrl+V."}
            : Result{false, "Windows clipboard rejected the screenshot."};
#else
        (void)nativeWindow;
        return {false, "Window screenshot clipboard capture is available on Windows."};
#endif
    }
}
