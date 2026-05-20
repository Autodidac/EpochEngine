module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

module directx.context;

import core.commandline;
import core.context;
import core.logger;
import context.commandqueue;
import engine.gui;
import render.preview_grid;
import atlas.texture;
import spritehandle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
    constexpr const char* kLogDirectX = "Context.DirectX";

    template <typename T>
    void safe_release(T*& value) noexcept
    {
        if (value)
        {
            value->Release();
            value = nullptr;
        }
    }

    struct DirectXVertex
    {
        float x{};
        float y{};
        float z{};
        float w{ 1.0f };
        float r{};
        float g{};
        float b{};
    };

    struct DirectXSpriteVertex
    {
        float x{};
        float y{};
        float z{};
        float w{ 1.0f };
        float u{};
        float v{};
    };

    struct DirectXAtlasGPU
    {
        ID3D11Texture2D* texture{};
        ID3D11ShaderResourceView* view{};
        std::uint64_t version{ static_cast<std::uint64_t>(-1) };
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct DirectXState
    {
        HWND hwnd{};
        int width{ 1 };
        int height{ 1 };
        std::function<void(int, int)> onResize{};
        std::uint32_t smokeFrames{ 0 };

        ID3D11Device* device{};
        ID3D11DeviceContext* immediate{};
        IDXGISwapChain* swapchain{};
        ID3D11RenderTargetView* renderTarget{};
        ID3D11VertexShader* vertexShader{};
        ID3D11PixelShader* pixelShader{};
        ID3D11InputLayout* inputLayout{};
        ID3D11VertexShader* spriteVertexShader{};
        ID3D11PixelShader* spritePixelShader{};
        ID3D11InputLayout* spriteInputLayout{};
        ID3D11SamplerState* spriteSampler{};
        ID3D11BlendState* spriteBlend{};
        ID3D11RasterizerState* rasterizer{};
        ID3D11Buffer* vertexBuffer{};
        std::size_t vertexCapacity{};
        ID3D11Buffer* spriteVertexBuffer{};
        std::size_t spriteVertexCapacity{};
        std::unordered_map<const epochnamespace::TextureAtlas*, DirectXAtlasGPU> guiAtlases{};
    };

    std::recursive_mutex g_directxMutex;
    std::unordered_map<const epochnamespace::core::Context*, DirectXState> g_states;

    bool succeeded(HRESULT hr) noexcept
    {
        return SUCCEEDED(hr);
    }

    void log_failure(const char* action, HRESULT hr)
    {
        epochnamespace::logger::get(kLogDirectX).log(
            epochnamespace::logger::LogLevel::WARN,
            std::string(action) + " failed, HRESULT=" + std::to_string(static_cast<long>(hr)),
            std::source_location::current());
    }

    bool client_size(HWND hwnd, int& width, int& height) noexcept
    {
        RECT rect{};
        if (!hwnd || ::GetClientRect(hwnd, &rect) == FALSE)
            return false;

        width = (std::max)(1, static_cast<int>(rect.right - rect.left));
        height = (std::max)(1, static_cast<int>(rect.bottom - rect.top));
        return true;
    }

    void release_render_target(DirectXState& state) noexcept
    {
        safe_release(state.renderTarget);
    }

    bool create_render_target(DirectXState& state)
    {
        if (!state.swapchain || !state.device)
            return false;

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = state.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (!succeeded(hr) || !backBuffer)
        {
            log_failure("IDXGISwapChain::GetBuffer", hr);
            return false;
        }

        hr = state.device->CreateRenderTargetView(backBuffer, nullptr, &state.renderTarget);
        safe_release(backBuffer);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateRenderTargetView", hr);
            return false;
        }

        return true;
    }

    void release_state(DirectXState& state) noexcept
    {
        if (state.immediate)
            state.immediate->ClearState();

        for (auto& [_, gpu] : state.guiAtlases)
        {
            safe_release(gpu.view);
            safe_release(gpu.texture);
        }
        state.guiAtlases.clear();
        release_render_target(state);
        safe_release(state.spriteVertexBuffer);
        safe_release(state.vertexBuffer);
        safe_release(state.spriteBlend);
        safe_release(state.spriteSampler);
        safe_release(state.rasterizer);
        safe_release(state.spriteInputLayout);
        safe_release(state.inputLayout);
        safe_release(state.spritePixelShader);
        safe_release(state.spriteVertexShader);
        safe_release(state.pixelShader);
        safe_release(state.vertexShader);
        safe_release(state.swapchain);
        safe_release(state.immediate);
        safe_release(state.device);
    }

    bool compile_shader(
        const char* source,
        const char* entry,
        const char* target,
        ID3DBlob** output)
    {
        ID3DBlob* errors = nullptr;
        const HRESULT hr = D3DCompile(
            source,
            std::strlen(source),
            nullptr,
            nullptr,
            nullptr,
            entry,
            target,
            0,
            0,
            output,
            &errors);

        if (!succeeded(hr))
        {
            std::string message = "D3DCompile failed";
            if (errors && errors->GetBufferPointer())
                message = static_cast<const char*>(errors->GetBufferPointer());
            safe_release(errors);
            epochnamespace::logger::get(kLogDirectX).log(
                epochnamespace::logger::LogLevel::Error,
                message,
                std::source_location::current());
            return false;
        }

        safe_release(errors);
        return true;
    }

    bool create_sprite_pipeline(DirectXState& state)
    {
        static constexpr const char* kSpriteShaderSource = R"(
Texture2D atlasTex : register(t0);
SamplerState atlasSampler : register(s0);

struct VSIn
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSIn VSMain(VSIn input)
{
    PSIn output;
    output.pos = input.pos;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSIn input) : SV_Target
{
    return atlasTex.Sample(atlasSampler, input.uv);
}
)";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        if (!compile_shader(kSpriteShaderSource, "VSMain", "vs_4_0", &vsBlob))
            return false;
        if (!compile_shader(kSpriteShaderSource, "PSMain", "ps_4_0", &psBlob))
        {
            safe_release(vsBlob);
            return false;
        }

        HRESULT hr = state.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &state.spriteVertexShader);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateVertexShader(sprite)", hr);
            safe_release(vsBlob);
            safe_release(psBlob);
            return false;
        }

        hr = state.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &state.spritePixelShader);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreatePixelShader(sprite)", hr);
            safe_release(vsBlob);
            safe_release(psBlob);
            return false;
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = state.device->CreateInputLayout(
            layout,
            static_cast<UINT>(sizeof(layout) / sizeof(layout[0])),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            &state.spriteInputLayout);

        safe_release(vsBlob);
        safe_release(psBlob);

        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateInputLayout(sprite)", hr);
            return false;
        }

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        hr = state.device->CreateSamplerState(&sampler, &state.spriteSampler);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateSamplerState(sprite)", hr);
            return false;
        }

        D3D11_BLEND_DESC blend{};
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = state.device->CreateBlendState(&blend, &state.spriteBlend);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateBlendState(sprite)", hr);
            return false;
        }

        return true;
    }

    bool create_shaders(DirectXState& state)
    {
        static constexpr const char* kShaderSource = R"(
struct VSIn
{
    float4 pos : POSITION;
    float3 color : COLOR;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

PSIn VSMain(VSIn input)
{
    PSIn output;
    output.pos = input.pos;
    output.color = input.color;
    return output;
}

float4 PSMain(PSIn input) : SV_Target
{
    return float4(input.color, 1.0);
}
)";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        if (!compile_shader(kShaderSource, "VSMain", "vs_4_0", &vsBlob))
            return false;
        if (!compile_shader(kShaderSource, "PSMain", "ps_4_0", &psBlob))
        {
            safe_release(vsBlob);
            return false;
        }

        HRESULT hr = state.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &state.vertexShader);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateVertexShader", hr);
            safe_release(vsBlob);
            safe_release(psBlob);
            return false;
        }

        hr = state.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &state.pixelShader);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreatePixelShader", hr);
            safe_release(vsBlob);
            safe_release(psBlob);
            return false;
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = state.device->CreateInputLayout(
            layout,
            static_cast<UINT>(sizeof(layout) / sizeof(layout[0])),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            &state.inputLayout);

        safe_release(vsBlob);
        safe_release(psBlob);

        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateInputLayout", hr);
            return false;
        }

        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        hr = state.device->CreateRasterizerState(&raster, &state.rasterizer);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateRasterizerState", hr);
            return false;
        }

        return true;
    }

    bool ensure_vertex_capacity(DirectXState& state, std::size_t count)
    {
        if (count == 0)
            return false;

        if (state.vertexBuffer && state.vertexCapacity >= count)
            return true;

        safe_release(state.vertexBuffer);
        state.vertexCapacity = (std::max<std::size_t>)(256u, count + 128u);

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(sizeof(DirectXVertex) * state.vertexCapacity);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        const HRESULT hr = state.device->CreateBuffer(&desc, nullptr, &state.vertexBuffer);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateBuffer", hr);
            state.vertexCapacity = 0;
            return false;
        }

        return true;
    }

    void draw_vertices(
        DirectXState& state,
        const std::vector<DirectXVertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology)
    {
        if (vertices.empty() || !ensure_vertex_capacity(state, vertices.size()))
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = state.immediate->Map(state.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (!succeeded(hr) || !mapped.pData)
        {
            log_failure("ID3D11DeviceContext::Map", hr);
            return;
        }

        std::memcpy(mapped.pData, vertices.data(), sizeof(DirectXVertex) * vertices.size());
        state.immediate->Unmap(state.vertexBuffer, 0);

        constexpr UINT stride = sizeof(DirectXVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* buffer = state.vertexBuffer;
        state.immediate->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
        state.immediate->IASetPrimitiveTopology(topology);
        state.immediate->Draw(static_cast<UINT>(vertices.size()), 0);
    }

    bool update_size(epochnamespace::core::Context& ctx, DirectXState& state)
    {
        int width = state.width;
        int height = state.height;
        if (!client_size(state.hwnd, width, height))
            return false;

        if (width == state.width && height == state.height && state.renderTarget)
            return true;

        state.width = width;
        state.height = height;
        ctx.width = width;
        ctx.height = height;
        ctx.virtualWidth = width;
        ctx.virtualHeight = height;
        ctx.framebufferWidth = width;
        ctx.framebufferHeight = height;

        if (ctx.windowData)
            ctx.windowData->set_size(width, height);
        if (state.onResize)
            state.onResize(width, height);

        if (!state.swapchain)
            return false;

        if (state.immediate)
            state.immediate->OMSetRenderTargets(0, nullptr, nullptr);
        release_render_target(state);

        const HRESULT hr = state.swapchain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
        if (!succeeded(hr))
        {
            log_failure("IDXGISwapChain::ResizeBuffers", hr);
            return false;
        }

        return create_render_target(state);
    }

    bool append_clip_vertex(
        std::vector<DirectXVertex>& out,
        const epochnamespace::previewgrid::Mat4& mvp,
        const epochnamespace::previewgrid::Vertex& vertex) noexcept
    {
        auto clip = epochnamespace::previewgrid::transform_point(mvp, vertex.position);
        if (clip.w <= 1.0e-4f)
            return false;

        clip.z = (clip.z + clip.w) * 0.5f;
        out.push_back(DirectXVertex{
            .x = clip.x,
            .y = clip.y,
            .z = clip.z,
            .w = clip.w,
            .r = vertex.color.x,
            .g = vertex.color.y,
            .b = vertex.color.z
        });
        return true;
    }

    epochnamespace::previewgrid::Mat4 preview_projection(
        const epochnamespace::core::Context& ctx,
        const float aspect,
        const epochnamespace::previewgrid::Camera& camera) noexcept
    {
        if (epochnamespace::previewgrid::camera_mode_for(&ctx) == epochnamespace::previewgrid::CameraMode::Canvas2D)
        {
            const auto delta = epochnamespace::previewgrid::subtract(camera.eye, camera.target);
            const float distance = std::sqrt(epochnamespace::previewgrid::dot(delta, delta));
            const float halfHeight = (std::max)(2.0f, distance * 0.42f);
            const float halfWidth = halfHeight * aspect;
            return epochnamespace::previewgrid::orthographic(
                -halfWidth,
                halfWidth,
                -halfHeight,
                halfHeight,
                camera.nearPlane,
                camera.farPlane);
        }

        return epochnamespace::previewgrid::perspective(
            camera.fovRadians,
            aspect,
            camera.nearPlane,
            camera.farPlane);
    }

    void build_preview_geometry(
        const epochnamespace::core::Context& ctx,
        const DirectXState& state,
        std::vector<DirectXVertex>& solid,
        std::vector<DirectXVertex>& lines)
    {
        auto viewport = ctx.scene_viewport();
        if (!viewport.valid())
        {
            viewport = epochnamespace::core::RenderViewport{
                0,
                0,
                (std::max)(1, state.width),
                (std::max)(1, state.height)
            };
        }

        const auto camera = epochnamespace::previewgrid::camera_for(&ctx);
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = preview_projection(ctx, aspect, camera);
        const auto view = epochnamespace::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochnamespace::previewgrid::multiply(proj, view);

        const auto gridVertices = epochnamespace::previewgrid::grid_vertices();
        const auto gridIndices = epochnamespace::previewgrid::grid_indices();
        for (std::size_t i = 0; i + 1 < gridIndices.size(); i += 2)
        {
            const auto first = static_cast<std::size_t>(gridIndices[i]);
            const auto second = static_cast<std::size_t>(gridIndices[i + 1]);
            if (first >= gridVertices.size() || second >= gridVertices.size())
                continue;

            (void)append_clip_vertex(lines, mvp, gridVertices[first]);
            (void)append_clip_vertex(lines, mvp, gridVertices[second]);
        }

        const auto solidVertices = epochnamespace::previewgrid::object_solid_vertices_for(&ctx);
        for (const auto& vertex : solidVertices)
            (void)append_clip_vertex(solid, mvp, vertex);

        const auto focusVertices = epochnamespace::previewgrid::look_marker_vertices_for(&ctx);
        const std::size_t focusCount = epochnamespace::previewgrid::look_marker_vertex_count_for(&ctx);
        for (std::size_t i = 0; i < focusCount && i < focusVertices.size(); ++i)
            (void)append_clip_vertex(lines, mvp, focusVertices[i]);

        const auto objectVertices = epochnamespace::previewgrid::object_marker_vertices_for(&ctx);
        for (const auto& vertex : objectVertices)
            (void)append_clip_vertex(lines, mvp, vertex);
    }

    D3D11_VIEWPORT full_window_viewport(const DirectXState& state) noexcept
    {
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>((std::max)(1, state.width));
        viewport.Height = static_cast<float>((std::max)(1, state.height));
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        return viewport;
    }

    D3D11_VIEWPORT scene_viewport_for(
        const epochnamespace::core::Context& ctx,
        const DirectXState& state) noexcept
    {
        const auto sceneViewport = ctx.scene_viewport();
        if (!sceneViewport.valid())
            return full_window_viewport(state);

        const int x = (std::clamp)(sceneViewport.x, 0, (std::max)(1, state.width - 1));
        const int y = (std::clamp)(sceneViewport.y, 0, (std::max)(1, state.height - 1));
        const int maxWidth = (std::max)(1, state.width - x);
        const int maxHeight = (std::max)(1, state.height - y);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = static_cast<float>(x);
        viewport.TopLeftY = static_cast<float>(y);
        viewport.Width = static_cast<float>((std::clamp)(sceneViewport.width, 1, maxWidth));
        viewport.Height = static_cast<float>((std::clamp)(sceneViewport.height, 1, maxHeight));
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        return viewport;
    }

    bool create_device(DirectXState& state)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = static_cast<UINT>((std::max)(1, state.width));
        desc.BufferDesc.Height = static_cast<UINT>((std::max)(1, state.height));
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.OutputWindow = state.hwnd;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_11_0;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
            D3D11_SDK_VERSION,
            &desc,
            &state.swapchain,
            &state.device,
            &selectedLevel,
            &state.immediate);

        if (hr == E_INVALIDARG)
        {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                flags,
                featureLevels + 1,
                static_cast<UINT>((sizeof(featureLevels) / sizeof(featureLevels[0])) - 1u),
                D3D11_SDK_VERSION,
                &desc,
                &state.swapchain,
                &state.device,
                &selectedLevel,
                &state.immediate);
        }

        if (!succeeded(hr))
        {
            log_failure("D3D11CreateDeviceAndSwapChain", hr);
            return false;
        }

        return create_render_target(state) && create_shaders(state) && create_sprite_pipeline(state);
    }

    bool ensure_sprite_vertex_capacity(DirectXState& state, const std::size_t count)
    {
        if (count == 0)
            return false;

        if (state.spriteVertexBuffer && state.spriteVertexCapacity >= count)
            return true;

        safe_release(state.spriteVertexBuffer);
        state.spriteVertexCapacity = (std::max<std::size_t>)(256u, count + 128u);

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(sizeof(DirectXSpriteVertex) * state.spriteVertexCapacity);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        const HRESULT hr = state.device->CreateBuffer(&desc, nullptr, &state.spriteVertexBuffer);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateBuffer(sprite)", hr);
            state.spriteVertexCapacity = 0;
            return false;
        }

        return true;
    }

    bool ensure_gui_atlas(DirectXState& state, const epochnamespace::TextureAtlas& atlas)
    {
        if (!state.device)
            return false;

        if (atlas.pixel_data.empty())
            const_cast<epochnamespace::TextureAtlas&>(atlas).rebuild_pixels();

        if (atlas.pixel_data.empty() || atlas.width == 0 || atlas.height == 0)
            return false;

        auto& gpu = state.guiAtlases[&atlas];
        if (gpu.view && gpu.version == atlas.version)
            return true;

        safe_release(gpu.view);
        safe_release(gpu.texture);

        D3D11_TEXTURE2D_DESC textureDesc{};
        textureDesc.Width = atlas.width;
        textureDesc.Height = atlas.height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = atlas.pixel_data.data();
        data.SysMemPitch = atlas.width * 4u;
        data.SysMemSlicePitch = atlas.width * atlas.height * 4u;

        HRESULT hr = state.device->CreateTexture2D(&textureDesc, &data, &gpu.texture);
        if (!succeeded(hr) || !gpu.texture)
        {
            log_failure("ID3D11Device::CreateTexture2D(gui atlas)", hr);
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.Format = textureDesc.Format;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipLevels = 1;
        hr = state.device->CreateShaderResourceView(gpu.texture, &viewDesc, &gpu.view);
        if (!succeeded(hr) || !gpu.view)
        {
            log_failure("ID3D11Device::CreateShaderResourceView(gui atlas)", hr);
            safe_release(gpu.texture);
            return false;
        }

        gpu.version = atlas.version;
        gpu.width = atlas.width;
        gpu.height = atlas.height;
        return true;
    }
}

namespace epochnamespace::directxcontext
{
    int directx_get_width()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(g_directxMutex);
        const auto it = g_states.find(ctx.get());
        return it != g_states.end() ? (std::max)(1, it->second.width) : (std::max)(1, ctx->width);
    }

    int directx_get_height()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(g_directxMutex);
        const auto it = g_states.find(ctx.get());
        return it != g_states.end() ? (std::max)(1, it->second.height) : (std::max)(1, ctx->height);
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

        DirectXState state{};
        state.hwnd = static_cast<HWND>(parentWnd);
        state.width = static_cast<int>((std::max)(1u, w));
        state.height = static_cast<int>((std::max)(1u, h));
        state.onResize = std::move(onResize);
        (void)client_size(state.hwnd, state.width, state.height);

        if (!create_device(state))
        {
            release_state(state);
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
            std::lock_guard lock(g_directxMutex);
            auto& slot = g_states[ctx.get()];
            release_state(slot);
            slot = std::move(state);
        }

        logger::get(kLogDirectX).log(
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

        std::lock_guard lock(g_directxMutex);
        auto it = g_states.find(ctx.get());
        if (it == g_states.end())
            return false;

        auto& state = it->second;
        if (!update_size(*ctx, state) || !state.renderTarget)
            return false;

        const auto clearColor = epochnamespace::previewgrid::kClearColor;
        const float clear[] = {
            clearColor[0],
            clearColor[1],
            clearColor[2],
            clearColor[3]
        };
        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        state.immediate->ClearRenderTargetView(state.renderTarget, clear);

        const D3D11_VIEWPORT viewport = full_window_viewport(state);
        state.immediate->RSSetViewports(1, &viewport);
        state.immediate->RSSetState(state.rasterizer);
        state.immediate->IASetInputLayout(state.inputLayout);
        state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.pixelShader, nullptr, 0);

        queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());

        const auto sceneViewport = ctx->scene_viewport();
        if (ctx->scene_preview_mode() == epochnamespace::core::ScenePreviewMode::Editor
            && sceneViewport.valid())
        {
            std::vector<DirectXVertex> solid{};
            std::vector<DirectXVertex> lines{};
            solid.reserve(256);
            lines.reserve(512);
            build_preview_geometry(*ctx, state, solid, lines);
            const D3D11_VIEWPORT previewViewport = scene_viewport_for(*ctx, state);
            state.immediate->RSSetViewports(1, &previewViewport);
            state.immediate->IASetInputLayout(state.inputLayout);
            state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
            state.immediate->PSSetShader(state.pixelShader, nullptr, 0);
            draw_vertices(state, solid, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            draw_vertices(state, lines, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }

        (void)state.swapchain->Present(1, 0);

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
        if (!sprite.is_valid())
            return;

        auto ctx = core::get_current_render_context();
        if (!ctx)
            return;

        std::lock_guard lock(g_directxMutex);
        auto it = g_states.find(ctx.get());
        if (it == g_states.end())
            return;

        auto& state = it->second;
        if (!state.immediate || !state.renderTarget || !state.spriteVertexShader || !state.spritePixelShader)
            return;

        const auto atlasIndex = static_cast<std::size_t>(sprite.atlasIndex);
        if (atlasIndex >= atlases.size())
            return;

        const TextureAtlas* atlas = atlases[atlasIndex];
        if (!atlas)
            return;

        AtlasRegion region{};
        if (!atlas->try_get_entry_info(static_cast<int>(sprite.localIndex), region))
            return;

        if (!ensure_gui_atlas(state, *atlas))
            return;

        const auto atlasIt = state.guiAtlases.find(atlas);
        if (atlasIt == state.guiAtlases.end() || !atlasIt->second.view)
            return;

        const float fbW = static_cast<float>((std::max)(1, state.width));
        const float fbH = static_cast<float>((std::max)(1, state.height));
        const bool widthNormalized = w > 0.0f && w <= 1.0f;
        const bool heightNormalized = h > 0.0f && h <= 1.0f;
        const float drawW = widthNormalized ? (std::max)(1.0f, w * fbW) : w;
        const float drawH = heightNormalized ? (std::max)(1.0f, h * fbH) : h;
        const float drawX = (widthNormalized && x >= 0.0f && x <= 1.0f) ? x * fbW : x;
        const float drawY = (heightNormalized && y >= 0.0f && y <= 1.0f) ? y * fbH : y;

        const float x0 = (drawX / fbW) * 2.0f - 1.0f;
        const float x1 = ((drawX + drawW) / fbW) * 2.0f - 1.0f;
        const float y0 = 1.0f - (drawY / fbH) * 2.0f;
        const float y1 = 1.0f - ((drawY + drawH) / fbH) * 2.0f;
        const float u0 = region.u1;
        const float u1 = region.u2;
        const float v0 = 1.0f - region.v2;
        const float v1 = 1.0f - region.v1;

        const DirectXSpriteVertex vertices[] = {
            { x0, y0, 0.0f, 1.0f, u0, v0 },
            { x1, y0, 0.0f, 1.0f, u1, v0 },
            { x1, y1, 0.0f, 1.0f, u1, v1 },
            { x1, y1, 0.0f, 1.0f, u1, v1 },
            { x0, y1, 0.0f, 1.0f, u0, v1 },
            { x0, y0, 0.0f, 1.0f, u0, v0 },
        };

        constexpr std::size_t vertexCount = sizeof(vertices) / sizeof(vertices[0]);
        if (!ensure_sprite_vertex_capacity(state, vertexCount))
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = state.immediate->Map(state.spriteVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (!succeeded(hr) || !mapped.pData)
            return;
        std::memcpy(mapped.pData, vertices, sizeof(vertices));
        state.immediate->Unmap(state.spriteVertexBuffer, 0);

        D3D11_VIEWPORT viewport{};
        viewport.Width = fbW;
        viewport.Height = fbH;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        state.immediate->RSSetViewports(1, &viewport);
        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        const float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        state.immediate->OMSetBlendState(state.spriteBlend, blendFactor, 0xffffffffu);

        constexpr UINT stride = sizeof(DirectXSpriteVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* buffer = state.spriteVertexBuffer;
        state.immediate->IASetInputLayout(state.spriteInputLayout);
        state.immediate->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
        state.immediate->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        state.immediate->VSSetShader(state.spriteVertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.spritePixelShader, nullptr, 0);
        ID3D11ShaderResourceView* view = atlasIt->second.view;
        state.immediate->PSSetShaderResources(0, 1, &view);
        state.immediate->PSSetSamplers(0, 1, &state.spriteSampler);
        state.immediate->Draw(static_cast<UINT>(vertexCount), 0);

        ID3D11ShaderResourceView* nullView = nullptr;
        state.immediate->PSSetShaderResources(0, 1, &nullView);
        state.immediate->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
    }

    void directx_cleanup(std::shared_ptr<core::Context> ctx)
    {
        if (!ctx)
            return;

        std::lock_guard lock(g_directxMutex);
        const auto it = g_states.find(ctx.get());
        if (it == g_states.end())
            return;

        release_state(it->second);
        g_states.erase(it);
    }
}
#else
namespace epochnamespace::directxcontext
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

    void directx_cleanup(std::shared_ptr<core::Context>)
    {
    }
}
#endif
