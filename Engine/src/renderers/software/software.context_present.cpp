module;

#include <cstdint>
#include <cstdlib>
#include <vector>

#include <include/engine.config.hpp>

#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
#       include "../include/framework.hpp"
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#elif defined(__linux__)
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   ifdef None
#       undef None
#   endif
#endif

module software.context;

import core.context;
import software.state;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    namespace detail
    {
#if defined(_WIN32)
        template <class T>
        static HDC try_get_hdc(const T& ctx)
        {
            if constexpr (requires { ctx.get_hdc(); })
                return reinterpret_cast<HDC>(ctx.get_hdc());
            else if constexpr (requires { ctx.hdc(); })
                return reinterpret_cast<HDC>(ctx.hdc());
            else if constexpr (requires { ctx.native_hdc(); })
                return reinterpret_cast<HDC>(ctx.native_hdc());
            else
                return nullptr;
        }
#endif

        bool present_frame(const core::Context& ctx, const SoftRendState& sr) noexcept
        {
#if defined(_WIN32)
            HDC hdc = try_get_hdc(ctx);
            bool tempDC = false;

            if (!hdc && sr.hwnd)
            {
                hdc = GetDC(sr.hwnd);
                tempDC = (hdc != nullptr);
            }

            if (hdc)
            {
                SetDIBitsToDevice(
                    hdc,
                    0,
                    0,
                    static_cast<DWORD>(sr.width),
                    static_cast<DWORD>(sr.height),
                    0,
                    0,
                    0,
                    static_cast<UINT>(sr.height),
                    sr.framebuffer.data(),
                    const_cast<BITMAPINFO*>(&sr.bmi),
                    DIB_RGB_COLORS);

                if (tempDC && sr.hwnd)
                    ReleaseDC(sr.hwnd, hdc);
                return true;
            }

            if (tempDC && sr.hwnd)
                ReleaseDC(sr.hwnd, hdc);
            return false;
#elif defined(__linux__)
            if (sr.framebuffer.empty() || sr.width <= 0 || sr.height <= 0)
                return false;

            auto* display = reinterpret_cast<Display*>(ctx.hdc);
            const auto window =
                static_cast<::Window>(reinterpret_cast<std::uintptr_t>(ctx.hwnd));

            if (!display || window == 0)
                return false;

            XLockDisplay(display);

            XWindowAttributes attrs{};
            if (XGetWindowAttributes(display, window, &attrs) == 0
                || attrs.visual == nullptr
                || attrs.map_state == IsUnmapped)
            {
                XUnlockDisplay(display);
                return false;
            }

            GC gc = XCreateGC(display, window, 0, nullptr);
            if (!gc)
            {
                XUnlockDisplay(display);
                return false;
            }

            XImage* image = XCreateImage(
                display,
                attrs.visual,
                static_cast<unsigned>(attrs.depth),
                ZPixmap,
                0,
                nullptr,
                static_cast<unsigned>(sr.width),
                static_cast<unsigned>(sr.height),
                32,
                0);

            if (!image)
            {
                XFreeGC(display, gc);
                XUnlockDisplay(display);
                return false;
            }

            const int bytesPerPixel = (image->bits_per_pixel + 7) / 8;
            const std::size_t bytesPerLine = static_cast<std::size_t>(image->bytes_per_line);
            const std::size_t bufferBytes = bytesPerLine * static_cast<std::size_t>(sr.height);

            auto* storage = static_cast<char*>(std::malloc(bufferBytes));
            if (!storage)
            {
                image->data = nullptr;
                XDestroyImage(image);
                XFreeGC(display, gc);
                XUnlockDisplay(display);
                return false;
            }

            image->data = storage;

            const auto scale_channel_to_mask = [](std::uint8_t value, unsigned long mask) noexcept -> unsigned long
            {
                if (mask == 0)
                    return 0;

                unsigned int shift = 0;
                while (((mask >> shift) & 1UL) == 0UL
                    && shift < (sizeof(unsigned long) * 8u))
                {
                    ++shift;
                }

                const unsigned long maxValue = mask >> shift;
                return ((static_cast<unsigned long>(value) * maxValue + 127UL) / 255UL) << shift;
            };

            for (int y = 0; y < sr.height; ++y)
            {
                char* row = storage + static_cast<std::size_t>(y) * bytesPerLine;
                for (int x = 0; x < sr.width; ++x)
                {
                    const std::uint32_t src =
                        sr.framebuffer[static_cast<std::size_t>(y) * static_cast<std::size_t>(sr.width)
                            + static_cast<std::size_t>(x)];

                    const std::uint8_t r = static_cast<std::uint8_t>((src >> 16) & 0xFFu);
                    const std::uint8_t g = static_cast<std::uint8_t>((src >> 8) & 0xFFu);
                    const std::uint8_t b = static_cast<std::uint8_t>(src & 0xFFu);

                    const unsigned long packed =
                        scale_channel_to_mask(r, image->red_mask)
                        | scale_channel_to_mask(g, image->green_mask)
                        | scale_channel_to_mask(b, image->blue_mask);

                    char* dst = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(bytesPerPixel);
                    if (image->byte_order == LSBFirst)
                    {
                        for (int byteIndex = 0; byteIndex < bytesPerPixel; ++byteIndex)
                            dst[byteIndex] = static_cast<char>((packed >> (byteIndex * 8)) & 0xFFu);
                    }
                    else
                    {
                        for (int byteIndex = 0; byteIndex < bytesPerPixel; ++byteIndex)
                        {
                            const int srcShift = (bytesPerPixel - 1 - byteIndex) * 8;
                            dst[byteIndex] = static_cast<char>((packed >> srcShift) & 0xFFu);
                        }
                    }
                }
            }

            XPutImage(
                display,
                window,
                gc,
                image,
                0,
                0,
                0,
                0,
                static_cast<unsigned>(sr.width),
                static_cast<unsigned>(sr.height));
            XFlush(display);

            XDestroyImage(image);
            XFreeGC(display, gc);
            XUnlockDisplay(display);
            return true;
#else
            (void)ctx;
            (void)sr;
            return false;
#endif
        }
    }
#endif
}
