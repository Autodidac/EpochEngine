module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "../src/platform.framework.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <cstring>
#include <memory>
#include <mutex>
#include <source_location>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

module directx.context;

import core.commandline;
import core.context;
import core.logger;
import context.commandqueue;
import gui.engine;
import render.preview_grid;
import atlas.texture;
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_presentation;
import render.canvas2d_runtime;
import render.canvas2d_scene;
import render.device;
import sprite.handle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "directx.context_detail.hpp"
namespace epochengine::directxcontext::detail
{
    namespace
    {
        class CanvasCommandContext final : public ICommandContext
        {
        public:
            void begin(const char*) override {}
            void end() override {}
            void debug_marker(const char*) override {}
            void barrier() override {}
        };

        struct CanvasTexture final
        {
            TextureDesc desc{};
            ID3D11Texture2D* texture{};
            ID3D11ShaderResourceView* view{};
            bool active{};
            bool ready{};
        };

        class CanvasRenderDevice final : public IRenderDevice
        {
        public:
            explicit CanvasRenderDevice(DirectXState& state) noexcept
                : state_(&state)
            {
            }

            ~CanvasRenderDevice() override
            {
                for (CanvasTexture& texture : textures_)
                    release(texture);
            }

            std::string backend_name() const override
            {
                return "D3D11.Canvas2D";
            }

            BufferHandle create_buffer(const BufferDesc&) override
            {
                return {};
            }

            TextureHandle create_texture(const TextureDesc& desc) override
            {
                if (!state_ || !state_->device || desc.width == 0 || desc.height == 0
                    || desc.format != TextureFormat::rgba8_unorm || !desc.sampled)
                {
                    return {};
                }

                D3D11_TEXTURE2D_DESC nativeDesc{};
                nativeDesc.Width = desc.width;
                nativeDesc.Height = desc.height;
                nativeDesc.MipLevels = 1u;
                nativeDesc.ArraySize = 1u;
                nativeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                nativeDesc.SampleDesc.Count = 1u;
                nativeDesc.Usage = D3D11_USAGE_DEFAULT;
                nativeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                CanvasTexture created{};
                created.desc = desc;
                HRESULT hr = state_->device->CreateTexture2D(
                    &nativeDesc, nullptr, &created.texture);
                if (!succeeded(hr) || !created.texture)
                {
                    log_failure("ID3D11Device::CreateTexture2D(Canvas2D)", hr);
                    return {};
                }

                D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
                viewDesc.Format = nativeDesc.Format;
                viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MostDetailedMip = 0u;
                viewDesc.Texture2D.MipLevels = 1u;
                hr = state_->device->CreateShaderResourceView(
                    created.texture, &viewDesc, &created.view);
                if (!succeeded(hr) || !created.view)
                {
                    log_failure(
                        "ID3D11Device::CreateShaderResourceView(Canvas2D)", hr);
                    release(created);
                    return {};
                }
                created.active = true;

                for (std::uint32_t index = 0; index < textures_.size(); ++index)
                {
                    if (!textures_[index].active)
                    {
                        textures_[index] = created;
                        return TextureHandle{index + 1u};
                    }
                }
                try
                {
                    textures_.push_back(created);
                }
                catch (...)
                {
                    release(created);
                    return {};
                }
                return TextureHandle{static_cast<std::uint32_t>(textures_.size())};
            }

            bool upload_texture(
                TextureHandle handle,
                const TextureUploadDesc& upload) override
            {
                CanvasTexture* const texture = resolve(handle);
                if (!texture || !state_ || !state_->immediate
                    || !epochengine::valid(upload)
                    || upload.mip_level != 0u || upload.x != 0u || upload.y != 0u
                    || upload.width != texture->desc.width
                    || upload.height != texture->desc.height
                    || upload.format != texture->desc.format)
                {
                    return false;
                }

                constexpr std::uint32_t bytesPerPixel = 4u;
                const std::uint32_t rowPitch = upload.row_pitch_bytes == 0u
                    ? upload.width * bytesPerPixel
                    : upload.row_pitch_bytes;
                state_->immediate->UpdateSubresource(
                    texture->texture,
                    0u,
                    nullptr,
                    upload.data,
                    rowPitch,
                    0u);
                texture->ready = true;
                return true;
            }

            bool texture_ready(TextureHandle handle) const noexcept override
            {
                const CanvasTexture* const texture = resolve(handle);
                return texture && texture->ready;
            }

            void destroy(BufferHandle) noexcept override {}

            void destroy(TextureHandle handle) noexcept override
            {
                CanvasTexture* const texture = resolve(handle);
                if (texture)
                    release(*texture);
            }

            ICommandContext& acquire_graphics_context() override
            {
                return commandContext_;
            }

            void present(ISwapchain&) override {}

            [[nodiscard]] CanvasTexture* resolve(TextureHandle handle) noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                CanvasTexture& texture = textures_[handle.value - 1u];
                return texture.active ? &texture : nullptr;
            }

            [[nodiscard]] const CanvasTexture* resolve(
                TextureHandle handle) const noexcept
            {
                if (!handle || handle.value > textures_.size())
                    return nullptr;
                const CanvasTexture& texture = textures_[handle.value - 1u];
                return texture.active ? &texture : nullptr;
            }

        private:
            static void release(CanvasTexture& texture) noexcept
            {
                safe_release(texture.view);
                safe_release(texture.texture);
                texture = {};
            }

            DirectXState* state_{};
            CanvasCommandContext commandContext_{};
            std::vector<CanvasTexture> textures_{};
        };

        [[nodiscard]] bool valid_canvas_packet(
            const canvas2d::presentation::NativePresentationPacket& packet,
            const CanvasTexture& texture) noexcept
        {
            if (packet.texture.value == 0u || packet.compose == nullptr
                || packet.frame_sequence == 0u || packet.content_hash == 0u
                || packet.image.extent.empty()
                || !texture.active || !texture.ready || !texture.view)
                return false;

            if (packet.image.origin
                != canvas2d::presentation::PixelOrigin::top_left)
                return false;

            if (packet.image.color_space != canvas2d::SpriteColorSpace::linear)
                return false;

            if (packet.image.alpha_encoding
                != canvas2d::cpu::AlphaEncoding::premultiplied)
                return false;

            const auto& compose = *packet.compose;
            const auto& viewport = compose.viewport;
            const auto& visible = viewport.visible_canvas;
            return compose.requires_offscreen_canvas
                && compose.destination
                    == canvas2d::ComposeTargetKind::presentation_surface
                && texture.desc.width == viewport.render_extent.width
                && texture.desc.height == viewport.render_extent.height
                && texture.desc.format == TextureFormat::rgba8_unorm
                && packet.image.extent == viewport.render_extent
                && std::isfinite(visible.x) && std::isfinite(visible.y)
                && std::isfinite(visible.width) && std::isfinite(visible.height)
                && visible.x >= 0.0f && visible.y >= 0.0f
                && visible.width > 0.0f && visible.height > 0.0f
                && visible.x + visible.width
                    <= static_cast<float>(texture.desc.width) + 0.001f
                && visible.y + visible.height
                    <= static_cast<float>(texture.desc.height) + 0.001f;
        }

        [[nodiscard]] std::vector<DirectXVertex> solid_quad(
            const canvas2d::LinearColor& color)
        {
            const float r = (std::clamp)(color.r, 0.0f, 1.0f);
            const float g = (std::clamp)(color.g, 0.0f, 1.0f);
            const float b = (std::clamp)(color.b, 0.0f, 1.0f);
            const float a = (std::clamp)(color.a, 0.0f, 1.0f);
            return {
                {-1.0f,  1.0f, 0.0f, 1.0f, r, g, b, a},
                { 1.0f,  1.0f, 0.0f, 1.0f, r, g, b, a},
                { 1.0f, -1.0f, 0.0f, 1.0f, r, g, b, a},
                { 1.0f, -1.0f, 0.0f, 1.0f, r, g, b, a},
                {-1.0f, -1.0f, 0.0f, 1.0f, r, g, b, a},
                {-1.0f,  1.0f, 0.0f, 1.0f, r, g, b, a}};
        }
    }

    struct DirectXCanvasState final
    {
        explicit DirectXCanvasState(
            DirectXState& ownerState,
            std::uint64_t epoch)
            : owner(&ownerState),
              device(ownerState),
              presenter(
                  device,
                  epoch,
                  canvas2d::presentation::NativePresentationHooks{
                      this,
                      &DirectXCanvasState::present_native})
        {
        }

        ~DirectXCanvasState()
        {
            presenter.retire_all();
            safe_release(premultipliedBlend);
            safe_release(linearSampler);
            safe_release(nearestSampler);
            safe_release(vertexBuffer);
        }

        [[nodiscard]] bool ensure_native_resources(DirectXState& state)
        {
            if (!vertexBuffer)
            {
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth =
                    static_cast<UINT>(sizeof(DirectXSpriteVertex) * 6u);
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                const HRESULT hr =
                    state.device->CreateBuffer(&desc, nullptr, &vertexBuffer);
                if (!succeeded(hr) || !vertexBuffer)
                {
                    log_failure("ID3D11Device::CreateBuffer(Canvas2D)", hr);
                    return false;
                }
            }

            const auto createSampler =
                [&](D3D11_FILTER filter, ID3D11SamplerState*& sampler)
            {
                if (sampler)
                    return true;
                D3D11_SAMPLER_DESC desc{};
                desc.Filter = filter;
                desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                desc.MaxLOD = D3D11_FLOAT32_MAX;
                const HRESULT hr =
                    state.device->CreateSamplerState(&desc, &sampler);
                if (!succeeded(hr) || !sampler)
                {
                    log_failure(
                        "ID3D11Device::CreateSamplerState(Canvas2D)", hr);
                    return false;
                }
                return true;
            };
            const bool samplersReady = createSampler(
                       D3D11_FILTER_MIN_MAG_MIP_POINT, nearestSampler)
                && createSampler(
                    D3D11_FILTER_MIN_MAG_MIP_LINEAR, linearSampler);
            if (!samplersReady)
                return false;
            if (premultipliedBlend)
                return true;

            D3D11_BLEND_DESC blend{};
            blend.RenderTarget[0].BlendEnable = TRUE;
            blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend.RenderTarget[0].RenderTargetWriteMask =
                D3D11_COLOR_WRITE_ENABLE_ALL;
            const HRESULT hr = state.device->CreateBlendState(
                &blend,
                &premultipliedBlend);
            if (!succeeded(hr) || !premultipliedBlend)
            {
                log_failure("ID3D11Device::CreateBlendState(Canvas2D)", hr);
                return false;
            }
            return true;
        }

        [[nodiscard]] static bool present_native(
            void* user,
            const canvas2d::presentation::NativePresentationPacket& packet)
        {
            auto* const canvas = static_cast<DirectXCanvasState*>(user);
            if (!canvas || !canvas->owner)
                return false;
            DirectXState& state = *canvas->owner;
            CanvasTexture* const texture =
                canvas->device.resolve(packet.texture);
            if (!texture || !valid_canvas_packet(packet, *texture)
                || !state.immediate || !state.renderTarget
                || !state.spriteVertexShader || !state.spritePixelShader
                || !canvas->ensure_native_resources(state))
            {
                return false;
            }

            const auto& compose = *packet.compose;
            const auto& output = compose.viewport.output_surface;
            const auto& destination = compose.viewport.clipped_destination;
            const auto& visible = compose.viewport.visible_canvas;
            const auto& surface = packet.surface.viewport;
            if (output.empty() || destination.empty())
                return false;

            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = static_cast<float>(surface.x);
            viewport.TopLeftY = static_cast<float>(surface.y);
            viewport.Width = static_cast<float>(surface.width);
            viewport.Height = static_cast<float>(surface.height);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            state.immediate->OMSetRenderTargets(
                1, &state.renderTarget, nullptr);
            state.immediate->RSSetViewports(1, &viewport);
            state.immediate->RSSetState(state.rasterizer);
            const float blendFactor[4]{};
            if (compose.clear_letterbox)
            {
                state.immediate->IASetInputLayout(state.inputLayout);
                state.immediate->OMSetBlendState(
                    nullptr, blendFactor, 0xffffffffu);
                state.immediate->VSSetShader(
                    state.vertexShader, nullptr, 0u);
                state.immediate->PSSetShader(
                    state.pixelShader, nullptr, 0u);
                draw_vertices(
                    state,
                    solid_quad(compose.letterbox_color),
                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            }

            state.immediate->OMSetBlendState(
                canvas->premultipliedBlend, blendFactor, 0xffffffffu);

            const float outputWidth = static_cast<float>(output.width);
            const float outputHeight = static_cast<float>(output.height);
            const float left =
                static_cast<float>(destination.x) / outputWidth * 2.0f - 1.0f;
            const float right =
                static_cast<float>(
                    destination.x
                    + static_cast<std::int32_t>(destination.width))
                    / outputWidth * 2.0f - 1.0f;
            const float top =
                1.0f - static_cast<float>(destination.y)
                    / outputHeight * 2.0f;
            const float bottom =
                1.0f - static_cast<float>(
                    destination.y
                    + static_cast<std::int32_t>(destination.height))
                    / outputHeight * 2.0f;
            const float textureWidth =
                static_cast<float>(texture->desc.width);
            const float textureHeight =
                static_cast<float>(texture->desc.height);
            const float u0 = visible.x / textureWidth;
            const float u1 = (visible.x + visible.width) / textureWidth;
            const float v0 = visible.y / textureHeight;
            const float v1 = (visible.y + visible.height) / textureHeight;
            const DirectXSpriteVertex vertices[6]{
                {left,  top,    0.0f, 1.0f, u0, v0},
                {right, top,    0.0f, 1.0f, u1, v0},
                {right, bottom, 0.0f, 1.0f, u1, v1},
                {right, bottom, 0.0f, 1.0f, u1, v1},
                {left,  bottom, 0.0f, 1.0f, u0, v1},
                {left,  top,    0.0f, 1.0f, u0, v0}};

            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT mapResult = state.immediate->Map(
                canvas->vertexBuffer,
                0u,
                D3D11_MAP_WRITE_DISCARD,
                0u,
                &mapped);
            if (!succeeded(mapResult) || !mapped.pData)
            {
                state.immediate->OMSetBlendState(
                    nullptr, blendFactor, 0xffffffffu);
                return false;
            }
            std::memcpy(mapped.pData, vertices, sizeof(vertices));
            state.immediate->Unmap(canvas->vertexBuffer, 0u);

            constexpr UINT stride = sizeof(DirectXSpriteVertex);
            constexpr UINT offset = 0u;
            ID3D11Buffer* vertexBuffer = canvas->vertexBuffer;
            state.immediate->IASetInputLayout(state.spriteInputLayout);
            state.immediate->IASetVertexBuffers(
                0u, 1u, &vertexBuffer, &stride, &offset);
            state.immediate->IASetPrimitiveTopology(
                D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            state.immediate->VSSetShader(
                state.spriteVertexShader, nullptr, 0u);
            state.immediate->PSSetShader(
                state.spritePixelShader, nullptr, 0u);
            ID3D11ShaderResourceView* view = texture->view;
            state.immediate->PSSetShaderResources(0u, 1u, &view);
            ID3D11SamplerState* sampler =
                compose.presentation_filter == FilterMode::nearest
                ? canvas->nearestSampler
                : canvas->linearSampler;
            state.immediate->PSSetSamplers(0u, 1u, &sampler);
            state.immediate->Draw(6u, 0u);

            ID3D11ShaderResourceView* nullView = nullptr;
            state.immediate->PSSetShaderResources(0u, 1u, &nullView);
            state.immediate->OMSetBlendState(
                nullptr, blendFactor, 0xffffffffu);
            return true;
        }

        [[nodiscard]] bool render(
            const core::Context* contextOwner,
            canvas2d::CanvasExtent nextOutput,
            canvas2d::presentation::PresentationSurface surface)
        {
            if (!contextOwner || nextOutput.empty())
                return false;
            const canvas2d::runtime::PreparedSceneView prepared =
                rasterSession.prepare(contextOwner, nextOutput);
            if (!prepared)
                return false;
            return static_cast<bool>(
                presenter.present(
                    *prepared.frame,
                    *prepared.raster,
                    surface));
        }

        DirectXState* owner{};
        CanvasRenderDevice device;
        canvas2d::presentation::Canvas2DPresenter presenter;
        canvas2d::runtime::SceneRasterSession rasterSession{};
        ID3D11Buffer* vertexBuffer{};
        ID3D11SamplerState* nearestSampler{};
        ID3D11SamplerState* linearSampler{};
        ID3D11BlendState* premultipliedBlend{};
    };

    void release_canvas2d_state(DirectXState& state) noexcept
    {
        delete state.canvas2d;
        state.canvas2d = nullptr;
    }

    bool render_canvas2d_scene(
        const std::shared_ptr<core::Context>& ctx,
        DirectXState& state) noexcept
    {
        if (!ctx)
            return false;
        const auto scene =
            canvas2d::scene_content::acquire(ctx.get());
        if (!scene)
        {
            release_canvas2d_state(state);
            return false;
        }

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || viewport.x < 0 || viewport.y < 0
            || viewport.x + viewport.width > state.width
            || viewport.y + viewport.height > state.height)
        {
            return false;
        }

        try
        {
            if (!state.canvas2d)
            {
                static std::atomic_uint64_t nextEpoch{1u};
                const std::uint64_t epoch =
                    nextEpoch.fetch_add(1u, std::memory_order_relaxed);
                state.canvas2d = new DirectXCanvasState(
                    state,
                    epoch == 0u ? 1u : epoch);
            }
            return state.canvas2d->render(
                ctx.get(),
                {
                    static_cast<std::uint32_t>(viewport.width),
                    static_cast<std::uint32_t>(viewport.height)},
                canvas2d::presentation::PresentationSurface{
                    {
                        static_cast<std::uint32_t>(state.width),
                        static_cast<std::uint32_t>(state.height)},
                    {
                        viewport.x,
                        viewport.y,
                        static_cast<std::uint32_t>(viewport.width),
                        static_cast<std::uint32_t>(viewport.height)}});
        }
        catch (...)
        {
            release_canvas2d_state(state);
            return false;
        }
    }
}


namespace epochengine::directxcontext
{
    int directx_get_width()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        return it != detail::g_states.end() ? (std::max)(1, it->second.width) : (std::max)(1, ctx->width);
    }

    int directx_get_height()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        return it != detail::g_states.end() ? (std::max)(1, it->second.height) : (std::max)(1, ctx->height);
    }

    bool directx_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd,
        unsigned int w,
        unsigned int h,
        std::function<void(int, int)> onResize)
    {
        if (!ctx || !parentWnd)
            return false;

        detail::DirectXState state{};
        state.hwnd = static_cast<HWND>(parentWnd);
        state.width = static_cast<int>((std::max)(1u, w));
        state.height = static_cast<int>((std::max)(1u, h));
        state.onResize = std::move(onResize);
        (void)detail::client_size(state.hwnd, state.width, state.height);

        if (!detail::create_device(state))
        {
            detail::release_state(state);
            return false;
        }

        ctx->width = state.width;
        ctx->height = state.height;
        ctx->virtualWidth = state.width;
        ctx->virtualHeight = state.height;
        ctx->framebufferWidth = state.width;
        ctx->framebufferHeight = state.height;
        if (ctx->windowData)
            ctx->windowData->set_size(state.width, state.height);

        {
            std::lock_guard lock(detail::g_directxMutex);
            auto& slot = detail::g_states[ctx.get()];
            detail::release_state(slot);
            slot = std::move(state);
        }

        logger::get(detail::kLogDirectX).log(
            logger::LogLevel::INFO,
            "Initialized DirectX preview context.",
            std::source_location::current());
        return true;
    }

    bool directx_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!ctx)
            return false;

        if (ctx->windowData && ctx->windowData->get_should_close())
        {
            queue.clear();
            return false;
        }

        std::lock_guard lock(detail::g_directxMutex);
        auto it = detail::g_states.find(ctx.get());
        if (it == detail::g_states.end())
            return false;

        auto& state = it->second;
        if (!detail::update_size(*ctx, state) || !state.renderTarget)
            return false;

        const auto clearColor = previewgrid::kClearColor;
        const float clear[] = {
            clearColor[0],
            clearColor[1],
            clearColor[2],
            clearColor[3]
        };
        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        state.immediate->ClearRenderTargetView(state.renderTarget, clear);

        const D3D11_VIEWPORT viewport = detail::full_window_viewport(state);
        state.immediate->RSSetViewports(1, &viewport);
        state.immediate->RSSetState(state.rasterizer);
        state.immediate->IASetInputLayout(state.inputLayout);
        state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.pixelShader, nullptr, 0);

        const bool overlayPriority = ctx->gui_overlay_priority();
        if (!overlayPriority)
        {
            queue.drain();
            (void)gui::render_deferred_batch(ctx.get());
        }

        const auto sceneViewport = ctx->scene_viewport();
        if (ctx->scene_preview_mode() == core::ScenePreviewMode::Editor
            && sceneViewport.valid())
        {
            if (!detail::render_canvas2d_scene(ctx, state))
            {
                std::vector<detail::DirectXVertex> solid{};
                std::vector<detail::DirectXVertex> lines{};
                solid.reserve(256);
                lines.reserve(512);
                detail::build_preview_geometry(*ctx, state, solid, lines);
                const D3D11_VIEWPORT previewViewport =
                    detail::scene_viewport_for(*ctx, state);
                state.immediate->RSSetViewports(1, &previewViewport);
                state.immediate->IASetInputLayout(state.inputLayout);
                state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
                state.immediate->PSSetShader(state.pixelShader, nullptr, 0);
                detail::draw_vertices(
                    state, solid, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                detail::render_engine_arcade_sampled_surface_preview(*ctx, state);
                detail::draw_vertices(
                    state, lines, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            }
        }

        queue.drain();
        (void)gui::render_deferred_batch(ctx.get());
        (void)gui::render_top_layer_batch(ctx.get());

        (void)state.swapchain->Present(0, 0);

        if (core::cli::smoke_requested)
        {
            ++state.smokeFrames;
            if (state.smokeFrames >= 3u)
            {
                if (ctx->windowData)
                    ctx->windowData->set_should_close(true);
                queue.clear();
                return false;
            }
        }

        return true;
    }

    void directx_draw_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h) noexcept
    {
        detail::draw_gui_sprite(sprite, atlases, x, y, w, h);
    }

    void directx_cleanup(std::shared_ptr<core::Context> ctx)
    {
        if (!ctx)
            return;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        if (it == detail::g_states.end())
            return;

        detail::release_state(it->second);
        detail::g_states.erase(it);
    }
}
#else
namespace epochengine::directxcontext
{
    int directx_get_width()
    {
        return 1;
    }

    int directx_get_height()
    {
        return 1;
    }

    bool directx_initialize(
        std::shared_ptr<core::Context>,
        void*,
        unsigned int,
        unsigned int,
        std::function<void(int, int)>)
    {
        return false;
    }

    bool directx_process(std::shared_ptr<core::Context>, core::CommandQueue&)
    {
        return false;
    }

    void directx_draw_sprite(
        SpriteHandle,
        std::span<const TextureAtlas* const>,
        float,
        float,
        float,
        float) noexcept
    {
    }

    void directx_cleanup(std::shared_ptr<core::Context>)
    {
    }
}
#endif
