// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <include/engine.config.hpp>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <new>
#include <source_location>
#include <string_view>
#include <utility>
#if defined(_WIN32) && ((defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1) \
    || (defined(EPOCH_USING_SFML) && EPOCH_USING_SFML == 1) \
    || (defined(EPOCH_USING_RAYLIB) && EPOCH_USING_RAYLIB == 1))
#include "../../platform/platform.framework.hpp"
#include <d3d11.h>
#include <glad/glad.h>
#endif

module opengl.native_present;
import core.env;
import core.logger;
import opengl.platform;

namespace epochengine::openglcontext::native_present
{
    namespace
    {
        // A failed retirement prevents another generation of native presentation resources.
        // This is an admission circuit breaker, not a resource registry or worker.
        std::atomic<bool> retirementFailed{};
        void report_failure(const char* stage, std::uint32_t code) noexcept
        {
            try
            {
                logger::get("OpenGL.Present").logf(logger::LogLevel::Error,
                    std::source_location::current(),
                    "DXGI presentation failed at {} (code=0x{:08X}); no native-swap fallback.", stage, code);
            }
            catch (...) {}
        }
    }

    namespace
    {
        enum class Policy { automatic, dxgi, wgl, invalid };
        Policy policy() noexcept
        {
            static const Policy configured = []() noexcept
            {
                try
                {
                    const auto value = core::env::get("EPOCH_OPENGL_PRESENTATION");
                    if (!value || value->empty()) return Policy::automatic;
                    const std::string_view text{value->data(), value->size()};
                    if (text == "auto") return Policy::automatic;
                    if (text == "dxgi") return Policy::dxgi;
                    if (text == "wgl") return Policy::wgl;
                }
                catch (...) {}
                return Policy::invalid;
            }();
            return configured;
        }
        void reject_selection(State& state, const char* stage, std::uint32_t code = 0) noexcept
        {
            state.selectionResolved = true;
            state.useDxgi = false;
            state.failed = true;
            report_failure(stage, code);
        }
    }
    bool admission_failed() noexcept { return retirementFailed.load(std::memory_order_acquire); }

#if defined(_WIN32) && ((defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1) \
    || (defined(EPOCH_USING_SFML) && EPOCH_USING_SFML == 1) \
    || (defined(EPOCH_USING_RAYLIB) && EPOCH_USING_RAYLIB == 1))
    struct Resource final
    {
        // Resolved for this resource's current context, independent of GLAD's
        // process-global table and of which renderer initialized first.
        struct GlFunctions final
        {
            PFNGLGETSTRINGPROC getString{};
            PFNGLGETERRORPROC getError{};
            PFNGLGETINTEGERVPROC getIntegerv{};
            PFNGLGETBOOLEANVPROC getBooleanv{};
            PFNGLISENABLEDPROC isEnabled{};
            PFNGLENABLEPROC enable{};
            PFNGLDISABLEPROC disable{};
            PFNGLBINDFRAMEBUFFERPROC bindFramebuffer{};
            PFNGLREADBUFFERPROC readBuffer{};
            PFNGLDRAWBUFFERPROC drawBuffer{};
            PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers{};
            PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers{};
            PFNGLGENRENDERBUFFERSPROC genRenderbuffers{};
            PFNGLGENFRAMEBUFFERSPROC genFramebuffers{};
            PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer{};
            PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus{};
            PFNGLBLITFRAMEBUFFERPROC blitFramebuffer{};
            PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC getFramebufferAttachmentParameteriv{};
        } gl{};
        HWND window{};
        HDC dc{};
        HGLRC context{};
        ID3D11Device* device{};
        ID3D11DeviceContext* immediate{};
        IDXGISwapChain* chain{};
        ID3D11Texture2D* buffer{};
        HANDLE interopDevice{}, interopObject{};
        GLuint framebuffer{}, renderbuffer{};
        int width{}, height{};
        bool locked{}, identityChecked{};
        bool retainedReported{};
        HANDLE(WINAPI* open)(void*){};
        BOOL(WINAPI* close)(HANDLE){};
        HANDLE(WINAPI* registerObject)(HANDLE, void*, GLuint, GLenum, GLenum){};
        BOOL(WINAPI* unregisterObject)(HANDLE, HANDLE){};
        BOOL(WINAPI* lock)(HANDLE, GLint, HANDLE*){};
        BOOL(WINAPI* unlock)(HANDLE, GLint, HANDLE*){};
    };

    namespace
    {
        struct Failure { const char* stage; std::uint32_t code; bool unsupported{}; };
        template<class T> void release_com(T*& object) noexcept
        {
            if (object) { object->Release(); object = nullptr; }
        }
        void require_native(BOOL success, const char* stage)
        {
            if (!success) throw Failure{ stage, GetLastError() };
        }
        void require_hr(HRESULT value, const char* stage)
        {
            if (FAILED(value)) throw Failure{ stage, static_cast<std::uint32_t>(value) };
        }
        void require_gl(const Resource& r, const char* stage)
        {
            const GLenum error = r.gl.getError();
            if (error != GL_NO_ERROR) throw Failure{ stage, error };
        }
        template<class T> T load(const char* name)
        {
            const auto address = PlatformGL::get_proc_address(name);
            const DWORD error = GetLastError();
            if (!address) throw Failure{ name, error, true };
            return reinterpret_cast<T>(address);
        }
        bool has_extension(std::string_view list, std::string_view name) noexcept
        {
            std::size_t at{};
            while ((at = list.find(name, at)) != std::string_view::npos)
            {
                const auto end = at + name.size();
                if ((!at || list[at - 1] == ' ') && (end == list.size() || list[end] == ' ')) return true;
                at = end;
            }
            return false;
        }
        bool current(const Resource& r) noexcept
        {
            return r.context && wglGetCurrentContext() == r.context && wglGetCurrentDC() == r.dc;
        }
        bool retain(State& state, const char* stage, std::uint32_t error) noexcept
        {
            state.failed = true;
            retirementFailed.store(true, std::memory_order_release);
            if (state.resource && !state.resource->retainedReported)
            {
                state.resource->retainedReported = true;
                report_failure(stage, error);
                try
                {
                    logger::get("OpenGL.Present").log(logger::LogLevel::Error,
                        "Registered NV resource retained; native owner retirement is incomplete. New DXGI resources are disabled.",
                        std::source_location::current());
                }
                catch (...) {}
            }
            return false;
        }

        // Default READ_BUFFER belongs to framebuffer zero, not to the caller's
        // read FBO. Save it after binding zero; restore it before caller bindings.
        struct SavedFramebufferState final
        {
            const Resource& r;
            GLint read{}, draw{}, defaultRead{};
            GLboolean scissor{}, srgb{};
            explicit SavedFramebufferState(const Resource& owner) noexcept : r(owner)
            {
                r.gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
                r.gl.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
                scissor = r.gl.isEnabled(GL_SCISSOR_TEST);
                srgb = r.gl.isEnabled(GL_FRAMEBUFFER_SRGB);
                r.gl.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                r.gl.getIntegerv(GL_READ_BUFFER, &defaultRead);
            }
            ~SavedFramebufferState()
            {
                r.gl.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                r.gl.readBuffer(static_cast<GLenum>(defaultRead));
                r.gl.bindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
                r.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
                if (scissor) r.gl.enable(GL_SCISSOR_TEST); else r.gl.disable(GL_SCISSOR_TEST);
                if (srgb) r.gl.enable(GL_FRAMEBUFFER_SRGB); else r.gl.disable(GL_FRAMEBUFFER_SRGB);
            }
        };

        bool release_buffer(State& state) noexcept
        {
            auto& r = *state.resource;
            if (r.interopObject && !current(r)) return retain(state, "retirement context unavailable", 0);
            if (r.locked)
            {
                if (!r.unlock(r.interopDevice, 1, &r.interopObject))
                    return retain(state, "retirement NV unlock", GetLastError());
                r.locked = false;
            }
            if (r.interopObject)
            {
                if (!r.unregisterObject(r.interopDevice, r.interopObject))
                    return retain(state, "retirement NV unregister", GetLastError());
                r.interopObject = nullptr;
            }
            if (current(r))
            {
                if (r.framebuffer) r.gl.deleteFramebuffers(1, &r.framebuffer);
                if (r.renderbuffer) r.gl.deleteRenderbuffers(1, &r.renderbuffer);
            }
            // Once unregistered, absent-context GL names may be retired by the
            // native context; the D3D resource no longer has a GL ownership claim.
            r.framebuffer = r.renderbuffer = 0;
            release_com(r.buffer);
            r.identityChecked = false;
            return true;
        }

        void register_buffer(Resource& r)
        {
            require_hr(r.chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&r.buffer)), "GetBuffer0");
            D3D11_TEXTURE2D_DESC desc{};
            r.buffer->GetDesc(&desc);
            if (desc.Usage != D3D11_USAGE_DEFAULT || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
                || desc.SampleDesc.Count != 1 || desc.Width != static_cast<UINT>(r.width)
                || desc.Height != static_cast<UINT>(r.height)) throw Failure{ "backbuffer descriptor", 0 };
            r.gl.genRenderbuffers(1, &r.renderbuffer);
            r.gl.genFramebuffers(1, &r.framebuffer);
            require_gl(r, "interop object allocation");
            if (!r.renderbuffer || !r.framebuffer) throw Failure{ "zero interop GL object", 0 };
            r.interopObject = r.registerObject(r.interopDevice, r.buffer, r.renderbuffer,
                GL_RENDERBUFFER, 0x0002); // WRITE_DISCARD: each submission overwrites the full image.
            if (!r.interopObject) throw Failure{ "NV register", GetLastError() };
            require_native(r.lock(r.interopDevice, 1, &r.interopObject), "initial NV lock");
            r.locked = true;
            {
                const SavedFramebufferState saved{r};
                r.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, r.framebuffer);
                r.gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, r.renderbuffer);
                r.gl.drawBuffer(GL_COLOR_ATTACHMENT0);
                const GLenum status = r.gl.checkFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                if (status != GL_FRAMEBUFFER_COMPLETE) throw Failure{ "interop framebuffer", status };
            }
            require_gl(r, "interop framebuffer state");
            require_native(r.unlock(r.interopDevice, 1, &r.interopObject), "initial NV unlock");
            r.locked = false;
        }

        void verify_buffer_identity(Resource& r)
        {
            ID3D11Texture2D* observed{};
            IUnknown* before{};
            IUnknown* after{};
            HRESULT hr = r.chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&observed));
            if (SUCCEEDED(hr)) hr = r.buffer->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&before));
            if (SUCCEEDED(hr)) hr = observed->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&after));
            const bool same = SUCCEEDED(hr) && before == after;
            release_com(after); release_com(before); release_com(observed);
            require_hr(hr, "post-present buffer identity query");
            if (!same) throw Failure{ "DISCARD backbuffer identity changed", 0 };
            r.identityChecked = true;
        }

        // Core framebuffer blit and FRAMEBUFFER_SRGB need GL3.0; interop2
        // itself requires GL2.1. Do not impose the prototype's unrelated GL4.6 floor.
        void preflight(Resource& r)
        {
            r.gl.getString = load<decltype(r.gl.getString)>("glGetString");
            const auto* vendor = r.gl.getString(GL_VENDOR);
            if (!vendor) throw Failure{ "GL vendor unavailable", 0 };
            if (std::string_view{reinterpret_cast<const char*>(vendor)}.find("NVIDIA") == std::string_view::npos)
                throw Failure{ "NVIDIA framebuffer support required", 0, true };
            // GL_VERSION exists on legacy GL too; GL_MAJOR_VERSION would itself
            // raise INVALID_ENUM on GL2 and pollute automatic native-swap fallback.
            const auto* bytes = r.gl.getString(GL_VERSION);
            if (!bytes) throw Failure{ "GL version unavailable", 0 };
            const auto* version = reinterpret_cast<const char*>(bytes);
            std::size_t length{};
            while (length < 128u && version[length]) ++length;
            int major{};
            const auto parsed = std::from_chars(version, version + length, major);
            if (parsed.ec != std::errc{} || parsed.ptr == version + length || *parsed.ptr != '.')
                throw Failure{ "GL version format unavailable", 0 };
            if (major < 3) throw Failure{ "GL3 framebuffer support required", 0, true };
            using Extensions = const char*(WINAPI*)(HDC);
            const auto extensions = load<Extensions>("wglGetExtensionsStringARB");
            const char* list = extensions(r.dc);
            if (!list || !has_extension(list, "WGL_NV_DX_interop") || !has_extension(list, "WGL_NV_DX_interop2"))
                throw Failure{ "NV_DX_interop2 unavailable", 0, true };
#define EPOCH_LOAD_NV(member, name) r.member = load<decltype(r.member)>(name)
            EPOCH_LOAD_NV(open, "wglDXOpenDeviceNV"); EPOCH_LOAD_NV(close, "wglDXCloseDeviceNV");
            EPOCH_LOAD_NV(registerObject, "wglDXRegisterObjectNV"); EPOCH_LOAD_NV(unregisterObject, "wglDXUnregisterObjectNV");
            EPOCH_LOAD_NV(lock, "wglDXLockObjectsNV"); EPOCH_LOAD_NV(unlock, "wglDXUnlockObjectsNV");
#undef EPOCH_LOAD_NV
#define EPOCH_LOAD_GL(member, name) r.gl.member = load<decltype(r.gl.member)>(name)
            EPOCH_LOAD_GL(getError, "glGetError");
            EPOCH_LOAD_GL(getIntegerv, "glGetIntegerv");
            EPOCH_LOAD_GL(getBooleanv, "glGetBooleanv");
            EPOCH_LOAD_GL(isEnabled, "glIsEnabled");
            EPOCH_LOAD_GL(enable, "glEnable");
            EPOCH_LOAD_GL(disable, "glDisable");
            EPOCH_LOAD_GL(bindFramebuffer, "glBindFramebuffer");
            EPOCH_LOAD_GL(readBuffer, "glReadBuffer");
            EPOCH_LOAD_GL(drawBuffer, "glDrawBuffer");
            EPOCH_LOAD_GL(deleteFramebuffers, "glDeleteFramebuffers");
            EPOCH_LOAD_GL(deleteRenderbuffers, "glDeleteRenderbuffers");
            EPOCH_LOAD_GL(genRenderbuffers, "glGenRenderbuffers");
            EPOCH_LOAD_GL(genFramebuffers, "glGenFramebuffers");
            EPOCH_LOAD_GL(framebufferRenderbuffer, "glFramebufferRenderbuffer");
            EPOCH_LOAD_GL(checkFramebufferStatus, "glCheckFramebufferStatus");
            EPOCH_LOAD_GL(blitFramebuffer, "glBlitFramebuffer");
            EPOCH_LOAD_GL(getFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv");
#undef EPOCH_LOAD_GL
            {
                const SavedFramebufferState saved{r};
                r.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                GLboolean doubleBuffered{};
                r.gl.getBooleanv(GL_DOUBLEBUFFER, &doubleBuffered);
                if (!doubleBuffered) throw Failure{ "default BACK buffer unavailable", 0, true };
                r.gl.readBuffer(GL_BACK);
                if (r.gl.checkFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    throw Failure{ "default framebuffer incomplete", 0, true };
                GLint sampleBuffers{};
                r.gl.getIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
                if (sampleBuffers > 0)
                {
                    // Older GL3 multisample blits require matching color formats.
                    // Admit an equal-size RGBA8 resolve, with the existing Y reversal;
                    // never substitute a scaled resolve or change application MSAA.
                    const auto attachment = [&r](GLenum property)
                    {
                        GLint value{};
                        r.gl.getFramebufferAttachmentParameteriv(
                            GL_READ_FRAMEBUFFER, GL_BACK_LEFT, property, &value);
                        return value;
                    };
                    if (attachment(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) != GL_UNSIGNED_NORMALIZED
                        || attachment(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING) != GL_LINEAR
                        || attachment(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE) != 8
                        || attachment(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE) != 8
                        || attachment(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE) != 8
                        || attachment(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE) != 8)
                        throw Failure{ "multisample default color must match linear RGBA8", 0, true };
                }
            }
            require_gl(r, "GL interop preflight");
        }

        void create_device(Resource& r)
        {
            DXGI_SWAP_CHAIN_DESC desc{};
            desc.BufferDesc.Width = static_cast<UINT>(r.width);
            desc.BufferDesc.Height = static_cast<UINT>(r.height);
            desc.BufferDesc.RefreshRate.Numerator = 60;
            desc.BufferDesc.RefreshRate.Denominator = 1;
            desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 2;
            desc.OutputWindow = r.window;
            desc.Windowed = TRUE;
            desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            constexpr UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
            static_assert((flags & D3D11_CREATE_DEVICE_SINGLETHREADED) == 0);
            constexpr D3D_FEATURE_LEVEL levels[]{ D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
            D3D_FEATURE_LEVEL selected{};
            HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                flags, levels, 4, D3D11_SDK_VERSION, &desc, &r.chain, &r.device, &selected, &r.immediate);
            if (hr == E_INVALIDARG)
            {
                release_com(r.immediate); release_com(r.chain); release_com(r.device);
                hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                    flags, levels + 1, 3, D3D11_SDK_VERSION, &desc, &r.chain, &r.device, &selected, &r.immediate);
            }
            require_hr(hr, "D3D11CreateDeviceAndSwapChain");
            IDXGIDevice* dxgiDevice{};
            IDXGIAdapter* adapter{};
            DXGI_ADAPTER_DESC adapterDesc{};
            hr = r.device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
            if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr)) hr = adapter->GetDesc(&adapterDesc);
            release_com(adapter); release_com(dxgiDevice);
            require_hr(hr, "created adapter identity");
            char name[512]{};
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name, 512, nullptr, nullptr);
            if (adapterDesc.VendorId != 0x10DE || bytes <= 0) throw Failure{ "created NVIDIA adapter required", 0 };
            try
            {
                logger::get("OpenGL.Present").logf(logger::LogLevel::INFO, std::source_location::current(),
                    "DXGI adapter vendor={:04X} device={:04X} LUID={:08X}:{:08X} name={}",
                    adapterDesc.VendorId, adapterDesc.DeviceId, static_cast<std::uint32_t>(adapterDesc.AdapterLuid.HighPart),
                    adapterDesc.AdapterLuid.LowPart, std::string_view{name, static_cast<std::size_t>((std::min)(bytes - 1, 64))});
            }
            catch (...) {}
        }
    }

    bool selected(State& state) noexcept
    {
        if (state.failed) return false;
        const auto requested = policy();
        if (requested == Policy::invalid)
        {
            reject_selection(state, "invalid EPOCH_OPENGL_PRESENTATION (auto/dxgi/wgl)");
            return false;
        }
        const HGLRC context = wglGetCurrentContext();
        const HDC dc = wglGetCurrentDC();
        if (!context || !dc) return false; // No first-context decision without a current owner.
        if (state.selectionResolved)
        {
            if (state.selectionContext != context || state.selectionDc != dc)
            {
                reject_selection(state, "cached presentation belongs to another context");
                return false;
            }
            return state.useDxgi;
        }
        state.selectionContext = context;
        state.selectionDc = dc;
        if (requested == Policy::wgl)
        {
            state.selectionResolved = true;
            return false; // Explicit legacy: no GL or NV capability probes.
        }
        try
        {
            Resource probe{};
            probe.context = context; probe.dc = dc;
            preflight(probe);
            state.selectionResolved = true;
            state.useDxgi = true;
            return true;
        }
        catch (const Failure& error)
        {
            if (requested == Policy::automatic && error.unsupported)
            {
                state.selectionResolved = true;
                state.useDxgi = false;
                try
                {
                    logger::get("OpenGL.Present").logf(logger::LogLevel::INFO,
                        std::source_location::current(), "Automatic presentation uses native swap: {}.", error.stage);
                }
                catch (...) {}
            }
            else reject_selection(state, error.stage, error.code);
        }
        catch (...) { reject_selection(state, "presentation selection exception"); }
        return false;
    }

    bool initialize(State& state, void* window, void* dc, void* context, int width, int height) noexcept
    {
        if (!selected(state) || state.failed || admission_failed()) return false;
        if (!window || !dc || !context || width <= 0 || height <= 0) return false;
        if (state.selectionContext != context || state.selectionDc != dc)
        {
            reject_selection(state, "resource creation does not match selected context");
            return false;
        }
        if (state.resource)
            return current(*state.resource) && state.resource->window == window
                && state.resource->dc == dc && state.resource->context == context;
        auto* resource = new (std::nothrow) Resource;
        if (!resource) return false;
        state.resource = resource;
        auto& r = *resource;
        r.window = static_cast<HWND>(window); r.dc = static_cast<HDC>(dc); r.context = static_cast<HGLRC>(context);
        r.width = width; r.height = height;
        try
        {
            if (!current(r)) throw Failure{ "initialization context unavailable", 0 };
            preflight(r);
            create_device(r);
            r.interopDevice = r.open(r.device);
            if (!r.interopDevice) throw Failure{ "NV open device", GetLastError() };
            register_buffer(r);
            try
            {
                logger::get("OpenGL.Present").log(logger::LogLevel::INFO,
                    "Selected DXGI/NV presentation: full-frame GL copy, DISCARD/RGBA8/two buffers; no WGL swap fallback.",
                    std::source_location::current());
            }
            catch (...) {}
            return true;
        }
        catch (const Failure& error) { report_failure(error.stage, error.code); }
        catch (...) { report_failure("initialization exception", 0); }
        state.failed = true;
        (void)release(state);
        return false;
    }

    Result present(State& state, int width, int height, unsigned syncInterval) noexcept
    {
        if (state.failed || !state.resource) return Result::failed;
        auto& r = *state.resource;
        try
        {
            if (!current(r)) throw Failure{ "presentation context unavailable", 0 };
            if (width <= 0 || height <= 0 || syncInterval > 1) throw Failure{ "invalid presentation arguments", 0 };
            if (width != r.width || height != r.height)
            {
                if (admission_failed()) throw Failure{ "resize blocked after retirement failure", 0 };
                if (!release_buffer(state)) return Result::failed;
                require_hr(r.chain->ResizeBuffers(2, static_cast<UINT>(width), static_cast<UINT>(height),
                    DXGI_FORMAT_R8G8B8A8_UNORM, 0), "ResizeBuffers");
                r.width = width; r.height = height;
                register_buffer(r);
            }
            require_native(r.lock(r.interopDevice, 1, &r.interopObject), "frame NV lock");
            r.locked = true;
            {
                const SavedFramebufferState saved{r};
                r.gl.disable(GL_SCISSOR_TEST); r.gl.disable(GL_FRAMEBUFFER_SRGB);
                r.gl.readBuffer(GL_BACK);
                r.gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, r.framebuffer);
                r.gl.drawBuffer(GL_COLOR_ATTACHMENT0);
                r.gl.blitFramebuffer(0, 0, width, height, 0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            require_gl(r, "GL frame/copy state");
            require_native(r.unlock(r.interopDevice, 1, &r.interopObject), "frame NV unlock");
            r.locked = false;
            const HRESULT hr = r.chain->Present(syncInterval, 0);
            if (hr == DXGI_STATUS_OCCLUDED) return Result::occluded;
            require_hr(hr, "DXGI Present");
            if (hr != S_OK) throw Failure{ "unexpected DXGI present status", static_cast<std::uint32_t>(hr) };
            if (!r.identityChecked) verify_buffer_identity(r);
            return Result::presented;
        }
        catch (const Failure& error) { report_failure(error.stage, error.code); }
        catch (...) { report_failure("presentation exception", 0); }
        state.failed = true;
        return Result::failed;
    }

    bool release(State& state) noexcept
    {
        if (!state.resource) return true;
        auto& r = *state.resource;
        if (!release_buffer(state)) return false;
        if (r.interopDevice)
        {
            if (!current(r)) return retain(state, "NV device close context unavailable", 0);
            if (!r.close(r.interopDevice)) return retain(state, "NV device close", GetLastError());
            r.interopDevice = nullptr;
        }
        release_com(r.immediate); release_com(r.chain); release_com(r.device);
        delete state.resource; state.resource = nullptr;
        return true;
    }
#else
    struct Resource {};
    bool selected(State& state) noexcept
    {
        if (state.failed) return false;
        if (state.selectionResolved) return state.useDxgi;
        const auto requested = policy();
        if (requested == Policy::invalid)
            reject_selection(state, "invalid EPOCH_OPENGL_PRESENTATION (auto/dxgi/wgl)");
        else if (requested == Policy::dxgi)
            reject_selection(state, "DXGI/NV presentation unsupported on this build");
        else state.selectionResolved = true; // Ordinary non-Windows presentation remains native.
        return false;
    }
    bool initialize(State& state, void*, void*, void*, int, int) noexcept
    {
        (void)selected(state);
        return false;
    }
    Result present(State&, int, int, unsigned) noexcept { return Result::unsupported; }
    bool release(State&) noexcept { return true; }
#endif

    State::~State()
    {
        // A failed release has already latched admission and logged retention.
        // Keep the opaque bundle's COM references intact through process exit;
        // an automatic destructor must never free an NV-owned native resource.
        (void)release(*this);
    }
    State::State(State&& other) noexcept
        : resource(std::exchange(other.resource, nullptr)), failed(std::exchange(other.failed, false)),
          selectionResolved(std::exchange(other.selectionResolved, false)), useDxgi(std::exchange(other.useDxgi, false)),
          selectionContext(std::exchange(other.selectionContext, nullptr)), selectionDc(std::exchange(other.selectionDc, nullptr)) {}
    State& State::operator=(State&& other) noexcept
    {
        if (this != &other)
        {
            // Keep both owners intact if this resource cannot retire. Do not
            // overwrite the sole address of a retained registered bundle.
            if (!release(*this)) return *this;
            resource = std::exchange(other.resource, nullptr);
            failed = std::exchange(other.failed, false);
            selectionResolved = std::exchange(other.selectionResolved, false);
            useDxgi = std::exchange(other.useDxgi, false);
            selectionContext = std::exchange(other.selectionContext, nullptr);
            selectionDc = std::exchange(other.selectionDc, nullptr);
        }
        return *this;
    }
}
