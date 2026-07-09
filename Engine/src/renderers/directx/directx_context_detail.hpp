/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
#pragma once

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)

namespace epochnamespace::directxcontext::detail
{
    inline constexpr const char* kLogDirectX = "Context.DirectX";

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
        std::unordered_map<const TextureAtlas*, DirectXAtlasGPU> guiAtlases{};
    };

    extern std::recursive_mutex g_directxMutex;
    extern std::unordered_map<const core::Context*, DirectXState> g_states;

    bool succeeded(HRESULT hr) noexcept;
    void log_failure(const char* action, HRESULT hr);
    bool client_size(HWND hwnd, int& width, int& height) noexcept;
    void release_render_target(DirectXState& state) noexcept;
    bool create_render_target(DirectXState& state);
    void release_state(DirectXState& state) noexcept;
    bool create_device(DirectXState& state);
    bool update_size(core::Context& ctx, DirectXState& state);

    D3D11_VIEWPORT full_window_viewport(const DirectXState& state) noexcept;
    D3D11_VIEWPORT scene_viewport_for(const core::Context& ctx, const DirectXState& state) noexcept;

    void build_preview_geometry(
        const core::Context& ctx,
        const DirectXState& state,
        std::vector<DirectXVertex>& solid,
        std::vector<DirectXVertex>& lines);

    void draw_vertices(
        DirectXState& state,
        const std::vector<DirectXVertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology);

    void draw_gui_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h) noexcept;
}

#endif
