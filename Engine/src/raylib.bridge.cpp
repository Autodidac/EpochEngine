module;

#include "../include/engine.config.hpp"

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

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

#include <cstdint>
#include <string>
#include <vector>
 // IMPORTANT:
 // This TU is the *only* place that should include <raylib.h>.
 // Keep Win32 headers out of here to avoid symbol collisions:
 //   - winuser.h : CloseWindow / ShowCursor / LoadImageW / DrawTextW
 //   - wingdi.h  : Rectangle()
 //
 // If you truly need Win32 types here, include raylib.h first and then include
 // your minimal Win32 shims that undef/avoid collisions.

#include <raylib.h>
#include <rlgl.h>

module raylib.api;
namespace epochnamespace::raylib_api
{
    namespace
    {
        struct LoadedModelEntry
        {
            std::string path{};
            ::Model model{};
            bool loaded = false;
        };

        std::vector<LoadedModelEntry>& loaded_models()
        {
            static std::vector<LoadedModelEntry> entries{};
            return entries;
        }

        static ::Vector3 to_rl(Vector3 v)
        {
            return ::Vector3{ v.x, v.y, v.z };
        }

        static ::Camera3D to_rl(Camera3D c)
        {
            return ::Camera3D{
                to_rl(c.position),
                to_rl(c.target),
                to_rl(c.up),
                c.fovy,
                c.projection
            };
        }
    }

    const Color raywhite = Color{ ::RAYWHITE.r, ::RAYWHITE.g, ::RAYWHITE.b, ::RAYWHITE.a };
    const Color white = Color{ ::WHITE.r, ::WHITE.g, ::WHITE.b, ::WHITE.a };

    const unsigned int flag_msaa_4x_hint = ::FLAG_MSAA_4X_HINT;
    const unsigned int flag_vsync_hint = ::FLAG_VSYNC_HINT;
    const int log_warning = ::LOG_WARNING;

    const int key_a = ::KEY_A;
    const int key_b = ::KEY_B;
    const int key_c = ::KEY_C;
    const int key_d = ::KEY_D;
    const int key_e = ::KEY_E;
    const int key_f = ::KEY_F;
    const int key_g = ::KEY_G;
    const int key_h = ::KEY_H;
    const int key_i = ::KEY_I;
    const int key_j = ::KEY_J;
    const int key_k = ::KEY_K;
    const int key_l = ::KEY_L;
    const int key_m = ::KEY_M;
    const int key_n = ::KEY_N;
    const int key_o = ::KEY_O;
    const int key_p = ::KEY_P;
    const int key_q = ::KEY_Q;
    const int key_r = ::KEY_R;
    const int key_s = ::KEY_S;
    const int key_t = ::KEY_T;
    const int key_u = ::KEY_U;
    const int key_v = ::KEY_V;
    const int key_w = ::KEY_W;
    const int key_x = ::KEY_X;
    const int key_y = ::KEY_Y;
    const int key_z = ::KEY_Z;

    const int key_zero = ::KEY_ZERO;
    const int key_one = ::KEY_ONE;
    const int key_two = ::KEY_TWO;
    const int key_three = ::KEY_THREE;
    const int key_four = ::KEY_FOUR;
    const int key_five = ::KEY_FIVE;
    const int key_six = ::KEY_SIX;
    const int key_seven = ::KEY_SEVEN;
    const int key_eight = ::KEY_EIGHT;
    const int key_nine = ::KEY_NINE;

    const int key_space = ::KEY_SPACE;
    const int key_enter = ::KEY_ENTER;
    const int key_escape = ::KEY_ESCAPE;
    const int key_tab = ::KEY_TAB;
    const int key_backspace = ::KEY_BACKSPACE;

    const int key_left = ::KEY_LEFT;
    const int key_right = ::KEY_RIGHT;
    const int key_up = ::KEY_UP;
    const int key_down = ::KEY_DOWN;

    const int mouse_button_left = ::MOUSE_BUTTON_LEFT;
    const int mouse_button_right = ::MOUSE_BUTTON_RIGHT;
    const int mouse_button_middle = ::MOUSE_BUTTON_MIDDLE;
    const int mouse_button_side = ::MOUSE_BUTTON_SIDE;
    const int mouse_button_extra = ::MOUSE_BUTTON_EXTRA;

    const int pixelformat_rgba8 = ::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    const int camera_perspective = ::CAMERA_PERSPECTIVE;

    static ::Color to_rl(Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }
    static ::Vector2 to_rl(Vector2 v) { return ::Vector2{ v.x, v.y }; }
    static Vector2 from_rl(::Vector2 v) { return Vector2{ v.x, v.y }; }
    static ::Rectangle to_rl(Rectangle r) { return ::Rectangle{ r.x, r.y, r.width, r.height }; }

    static ::Texture2D to_rl(Texture2D t)
    {
        ::Texture2D out{};
        out.id = t.id;
        out.width = t.width;
        out.height = t.height;
        out.mipmaps = t.mipmaps;
        out.format = t.format;
        return out;
    }

    static Texture2D from_rl(::Texture2D t)
    {
        Texture2D out{};
        out.id = t.id;
        out.width = t.width;
        out.height = t.height;
        out.mipmaps = t.mipmaps;
        out.format = t.format;
        return out;
    }

    static ::Image to_rl(Image img)
    {
        ::Image out{};
        out.data = img.data;
        out.width = img.width;
        out.height = img.height;
        out.mipmaps = img.mipmaps;
        out.format = img.format;
        return out;
    }

    static ::RenderTexture2D to_rl(RenderTexture2D rt)
    {
        ::RenderTexture2D out{};
        out.id = rt.id;
        out.texture = to_rl(rt.texture);
        out.depth = to_rl(rt.depth);
        return out;
    }

    static RenderTexture2D from_rl(::RenderTexture2D rt)
    {
        RenderTexture2D out{};
        out.id = rt.id;
        out.texture = from_rl(rt.texture);
        out.depth = from_rl(rt.depth);
        return out;
    }

    void set_config_flags(unsigned int flags) { ::SetConfigFlags(flags); }
    void set_trace_log_level(int level) { ::SetTraceLogLevel(level); }
    void init_window(int w, int h, const char* title) { ::InitWindow(w, h, title); }
    void close_window() { ::CloseWindow(); }
    void set_window_size(int w, int h) { ::SetWindowSize(w, h); }

    bool window_should_close() { return ::WindowShouldClose(); }
    bool is_window_ready() { return ::IsWindowReady(); }
    void* get_window_handle() { return ::GetWindowHandle(); }

    int get_render_width() { return ::GetRenderWidth(); }
    int get_render_height() { return ::GetRenderHeight(); }
    int get_screen_width() { return ::GetScreenWidth(); }
    int get_screen_height() { return ::GetScreenHeight(); }

    void begin_drawing() { ::BeginDrawing(); }
    void end_drawing() { ::EndDrawing(); }
    void clear_background(Color c) { ::ClearBackground(to_rl(c)); }
    void begin_texture_mode(const RenderTexture2D& target) { ::BeginTextureMode(to_rl(target)); }
    void end_texture_mode() { ::EndTextureMode(); }
    void begin_scissor_mode(int x, int y, int width, int height) { ::BeginScissorMode(x, y, width, height); }
    void end_scissor_mode() { ::EndScissorMode(); }
    void draw_rectangle_rec(const Rectangle& rec, Color color) { ::DrawRectangleRec(to_rl(rec), to_rl(color)); }
    void draw_line_v(Vector2 start, Vector2 end, Color color) { ::DrawLineV(to_rl(start), to_rl(end), to_rl(color)); }
    void draw_triangle(Vector2 a, Vector2 b, Vector2 c, Color color) { ::DrawTriangle(to_rl(a), to_rl(b), to_rl(c), to_rl(color)); }
    void set_viewport(int x, int y, int width, int height) { ::rlViewport(x, y, width, height); }
    void begin_mode_3d(const Camera3D& camera) { ::BeginMode3D(to_rl(camera)); }
    void end_mode_3d() { ::EndMode3D(); }
    void draw_grid(int slices, float spacing) { ::DrawGrid(slices, spacing); }

    void set_target_fps(int fps) { ::SetTargetFPS(fps); }
    void set_window_title(const char* title) { ::SetWindowTitle(title); }

    bool is_key_down(int k) { return ::IsKeyDown(k); }
    bool is_mouse_button_down(int b) { return ::IsMouseButtonDown(b); }
    int get_mouse_x() { return ::GetMouseX(); }
    int get_mouse_y() { return ::GetMouseY(); }
    float get_mouse_wheel_move() { return ::GetMouseWheelMove(); }
    Vector2 get_mouse_position() { return from_rl(::GetMousePosition()); }
    void set_mouse_offset(int ox, int oy) { ::SetMouseOffset(ox, oy); }
    void set_mouse_scale(float sx, float sy) { ::SetMouseScale(sx, sy); }

    Texture2D load_texture_from_image(const Image& img)
    {
        ::Image rlImg = to_rl(img);
        return from_rl(::LoadTextureFromImage(rlImg));
    }

    Image load_image_from_screen()
    {
        const ::Image image = ::LoadImageFromScreen();
        return Image{
            image.data,
            image.width,
            image.height,
            image.mipmaps,
            image.format
        };
    }

    void unload_image(const Image& img)
    {
        ::UnloadImage(to_rl(img));
    }

    void unload_texture(const Texture2D& tex) { ::UnloadTexture(to_rl(tex)); }

    RenderTexture2D load_render_texture(int w, int h) { return from_rl(::LoadRenderTexture(w, h)); }
    void unload_render_texture(const RenderTexture2D& rt) { ::UnloadRenderTexture(to_rl(rt)); }

    void draw_texture_pro(
        const Texture2D& tex,
        const Rectangle& src,
        const Rectangle& dst,
        Vector2 origin,
        float rotation,
        Color tint)
    {
        ::DrawTexturePro(to_rl(tex), to_rl(src), to_rl(dst), to_rl(origin), rotation, to_rl(tint));
    }

    int load_model(const char* path)
    {
        if (!path || path[0] == '\0')
            return -1;

        auto& entries = loaded_models();
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].path == path)
                return static_cast<int>(i);
        }

        ::Model model = ::LoadModel(path);
        if (model.meshCount <= 0)
        {
            ::UnloadModel(model);
            return -1;
        }

        entries.push_back(LoadedModelEntry{
            .path = path,
            .model = model,
            .loaded = true
        });
        return static_cast<int>(entries.size() - 1);
    }

    bool has_model(int model_id)
    {
        const auto& entries = loaded_models();
        return model_id >= 0 &&
            static_cast<std::size_t>(model_id) < entries.size() &&
            entries[static_cast<std::size_t>(model_id)].loaded;
    }

    void draw_model(int model_id)
    {
        auto& entries = loaded_models();
        if (model_id < 0 || static_cast<std::size_t>(model_id) >= entries.size())
            return;

        const auto& entry = entries[static_cast<std::size_t>(model_id)];
        if (entry.loaded)
            ::DrawModel(entry.model, ::Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, ::WHITE);
    }

    void unload_model(int model_id)
    {
        auto& entries = loaded_models();
        if (model_id < 0 || static_cast<std::size_t>(model_id) >= entries.size())
            return;

        auto& entry = entries[static_cast<std::size_t>(model_id)];
        if (!entry.loaded)
            return;

        ::UnloadModel(entry.model);
        entry.loaded = false;
        entry.path.clear();
    }

    bool has_loaded_models()
    {
        for (const auto& entry : loaded_models())
        {
            if (entry.loaded)
                return true;
        }
        return false;
    }

    void draw_loaded_models()
    {
        for (const auto& entry : loaded_models())
        {
            if (entry.loaded)
                ::DrawModel(entry.model, ::Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, ::WHITE);
        }
    }

    void unload_all_models()
    {
        auto& entries = loaded_models();
        for (auto& entry : entries)
        {
            if (entry.loaded)
                ::UnloadModel(entry.model);
        }
        entries.clear();
    }
}

#endif
