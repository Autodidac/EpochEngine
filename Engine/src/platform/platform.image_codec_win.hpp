// Epoch Engine — LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
#pragma once
#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
namespace epochengine::platform_image_codec {
struct Result final { bool succeeded{}; std::string message{}; };
struct Thumbnail final { std::vector<std::uint8_t> rgba{}; std::uint32_t width{}, height{}; };
}
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <propidl.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
namespace epochengine::platform_image_codec
{
        template <class Interface>
        class ComHandle final
        {
        public:
            ComHandle() = default;
            ComHandle(const ComHandle&) = delete;
            ComHandle& operator=(const ComHandle&) = delete;
            ~ComHandle()
            {
                if (object_) object_->Release();
            }
            [[nodiscard]] Interface* get() const noexcept { return object_; }
            [[nodiscard]] Interface** put() noexcept { return &object_; }
            [[nodiscard]] Interface* operator->() const noexcept { return object_; }

        private:
            Interface* object_{};
        };

        class ComApartment final
        {
        public:
            ComApartment() noexcept
                : status_{::CoInitializeEx(nullptr, COINIT_MULTITHREADED)} {}
            ComApartment(const ComApartment&) = delete;
            ComApartment& operator=(const ComApartment&) = delete;
            ~ComApartment()
            {
                if (SUCCEEDED(status_)) ::CoUninitialize();
            }
            [[nodiscard]] bool usable() const noexcept
            {
                // An already-initialized STA can also use the WIC COM objects.
                return SUCCEEDED(status_) || status_ == RPC_E_CHANGED_MODE;
            }

        private:
            HRESULT status_{};
        };

        [[nodiscard]] inline Thumbnail decode_thumbnail(std::string_view bytes)
        {
            if (bytes.empty() || bytes.size() > 4u * 1024u * 1024u) return {};
            ComApartment apartment{};
            if (!apartment.usable()) return {};
            ComHandle<IWICImagingFactory> factory;
            if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())))) return {};
            ComHandle<IWICStream> stream;
            if (FAILED(factory->CreateStream(stream.put()))
                || FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(
                    const_cast<char*>(bytes.data())), static_cast<DWORD>(bytes.size())))) return {};
            ComHandle<IWICBitmapDecoder> decoder;
            ComHandle<IWICBitmapFrameDecode> frame;
            UINT width{}, height{};
            if (FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr,
                    WICDecodeMetadataCacheOnDemand, decoder.put()))
                || FAILED(decoder->GetFrame(0u, frame.put()))
                || FAILED(frame->GetSize(&width, &height))
                || width == 0u || height == 0u || width > 16384u || height > 16384u
                || static_cast<std::uint64_t>(width) * height > 64u * 1024u * 1024u) return {};
            const auto scale = (std::min)({1.0, 160.0 / width, 160.0 / height});
            Thumbnail result{{}, static_cast<UINT>((std::max)(1.0, width * scale)),
                static_cast<UINT>((std::max)(1.0, height * scale))};
            ComHandle<IWICBitmapScaler> scaler;
            ComHandle<IWICFormatConverter> converter;
            if (FAILED(factory->CreateBitmapScaler(scaler.put()))
                || FAILED(scaler->Initialize(frame.get(), result.width, result.height,
                    WICBitmapInterpolationModeFant))
                || FAILED(factory->CreateFormatConverter(converter.put()))
                || FAILED(converter->Initialize(scaler.get(), GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return {};
            result.rgba.resize(static_cast<std::size_t>(result.width) * result.height * 4u);
            if (FAILED(converter->CopyPixels(nullptr, result.width * 4u,
                    static_cast<UINT>(result.rgba.size()), result.rgba.data()))) return {};
            return result;
        }

        [[nodiscard]] inline Result encode_png(HBITMAP bitmap, long width,
            long height, const std::filesystem::path& output, double maximumWidth = 1280.0, double maximumHeight = 800.0)
        {
            std::error_code ec;
            std::filesystem::create_directories(output.parent_path(), ec);
            if (ec) return {false, "Could not create candidate screenshot directory."};

            // A failed encoder must never leave behind a misleading new PNG.
            std::filesystem::path pending = output;
            pending += L".pending";
            std::filesystem::remove(pending, ec);
            ec.clear();
            const auto cleanup = [&]() noexcept
            {
                std::error_code ignored;
                std::filesystem::remove(pending, ignored);
            };

            ComApartment apartment{};
            if (!apartment.usable())
                return {false, "Windows imaging COM initialization failed."};

            ComHandle<IWICImagingFactory> factory;
            if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put()))))
                return {false, "Windows Imaging Component is unavailable."};

            ComHandle<IWICBitmap> source;
            if (FAILED(factory->CreateBitmapFromHBITMAP(bitmap, nullptr,
                    WICBitmapIgnoreAlpha, source.put())))
                return {false, "Could not convert captured bitmap into WIC pixels."};

            const double scale = (std::min)({1.0,
                maximumWidth / static_cast<double>(width),
                maximumHeight / static_cast<double>(height)});
            const auto outWidth = static_cast<UINT>(
                (std::max)(1.0, static_cast<double>(width) * scale));
            const auto outHeight = static_cast<UINT>(
                (std::max)(1.0, static_cast<double>(height) * scale));

            ComHandle<IWICBitmapScaler> scaler;
            if (FAILED(factory->CreateBitmapScaler(scaler.put()))
                || FAILED(scaler->Initialize(source.get(), outWidth, outHeight,
                    WICBitmapInterpolationModeFant)))
                return {false, "Could not resize candidate screenshot."};

            ComHandle<IWICFormatConverter> converter;
            if (FAILED(factory->CreateFormatConverter(converter.put()))
                || FAILED(converter->Initialize(scaler.get(),
                    GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone,
                    nullptr, 0.0, WICBitmapPaletteTypeCustom)))
                return {false, "Could not prepare RGB screenshot pixels."};

            Result result{false, "PNG encoding failed."};
            {
                ComHandle<IWICStream> stream;
                ComHandle<IWICBitmapEncoder> encoder;
                ComHandle<IWICBitmapFrameEncode> frame;
                if (SUCCEEDED(factory->CreateStream(stream.put()))
                    && SUCCEEDED(stream->InitializeFromFilename(
                        pending.c_str(), GENERIC_WRITE))
                    && SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,
                        nullptr, encoder.put()))
                    && SUCCEEDED(encoder->Initialize(stream.get(),
                        WICBitmapEncoderNoCache))
                    && SUCCEEDED(encoder->CreateNewFrame(frame.put(), nullptr))
                    && SUCCEEDED(frame->Initialize(nullptr))
                    && SUCCEEDED(frame->SetSize(outWidth, outHeight)))
                {
                    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
                    if (SUCCEEDED(frame->SetPixelFormat(&format))
                        && ::IsEqualGUID(format, GUID_WICPixelFormat24bppBGR)
                        && SUCCEEDED(frame->WriteSource(converter.get(), nullptr))
                        && SUCCEEDED(frame->Commit())
                        && SUCCEEDED(encoder->Commit()))
                        result = {true, "Captured PNG from admitted candidate window."};
                }
            }
            // Release the WIC stream before atomically publishing the capture.
            if (result.succeeded
                && !::MoveFileExW(pending.c_str(), output.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                result = {false, "Could not publish candidate screenshot PNG."};
            if (!result.succeeded) cleanup();
            return result;
        }
    }
#endif
