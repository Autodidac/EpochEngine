module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "../../platform.framework.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

module directx.context;

import core.context;
import core.logger;
import atlas.texture;
import sprite.handle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "directx.context_detail.hpp"

namespace epochengine::directxcontext::detail
{
    namespace
    {
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
                logger::get(kLogDirectX).log(
                    logger::LogLevel::Error,
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
}
#endif
