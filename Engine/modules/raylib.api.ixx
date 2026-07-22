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
module;

// NOTE:
// This is the *interface* module. It must not include <windows.h> or <raylib.h>.
// The implementation lives in a normal TU/module implementation unit.

#include <include/engine.config.hpp>
#include <cstdint>

export module raylib.api;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

export namespace epochengine::raylib_api
{
    struct Color
    {
        std::uint8_t r{};
        std::uint8_t g{};
        std::uint8_t b{};
        std::uint8_t a{};
    };

    struct Vector2
    {
        float x{};
        float y{};
    };

    struct Vector3
    {
        float x{};
        float y{};
        float z{};
    };

    struct Camera3D
    {
        Vector3 position{};
        Vector3 target{};
        Vector3 up{};
        float fovy{};
        int projection{};
    };

    struct Rectangle
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    struct Texture2D
    {
        std::uint32_t id{};
        int width{};
        int height{};
        int mipmaps{};
        int format{};
    };

    struct RenderTexture2D
    {
        std::uint32_t id{};
        Texture2D texture{};
        Texture2D depth{};
    };

    struct Image
    {
        void* data{};
        int width{};
        int height{};
        int mipmaps{};
        int format{};
    };

    extern const Color raywhite;
    extern const Color white;

    extern const unsigned int flag_msaa_4x_hint;
    extern const unsigned int flag_vsync_hint;
    extern const int log_warning;

    extern const int key_a;
    extern const int key_b;
    extern const int key_c;
    extern const int key_d;
    extern const int key_e;
    extern const int key_f;
    extern const int key_g;
    extern const int key_h;
    extern const int key_i;
    extern const int key_j;
    extern const int key_k;
    extern const int key_l;
    extern const int key_m;
    extern const int key_n;
    extern const int key_o;
    extern const int key_p;
    extern const int key_q;
    extern const int key_r;
    extern const int key_s;
    extern const int key_t;
    extern const int key_u;
    extern const int key_v;
    extern const int key_w;
    extern const int key_x;
    extern const int key_y;
    extern const int key_z;

    extern const int key_zero;
    extern const int key_one;
    extern const int key_two;
    extern const int key_three;
    extern const int key_four;
    extern const int key_five;
    extern const int key_six;
    extern const int key_seven;
    extern const int key_eight;
    extern const int key_nine;

    extern const int key_space;
    extern const int key_enter;
    extern const int key_escape;
    extern const int key_tab;
    extern const int key_backspace;

    extern const int key_left;
    extern const int key_right;
    extern const int key_up;
    extern const int key_down;

    extern const int mouse_button_left;
    extern const int mouse_button_right;
    extern const int mouse_button_middle;
    extern const int mouse_button_side;
    extern const int mouse_button_extra;

    extern const int pixelformat_rgba8;
    extern const int camera_perspective;

    void set_config_flags(unsigned int flags);
    void set_trace_log_level(int level);
    void init_window(int w, int h, const char* title);
    void close_window();
    void set_window_size(int w, int h);

    bool window_should_close();
    bool is_window_ready();
    void* get_window_handle();

    int get_render_width();
    int get_render_height();
    int get_screen_width();
    int get_screen_height();
    int get_framebuffer_width();
    int get_framebuffer_height();
    int get_graphics_api_version();
    std::uint32_t get_default_texture_id();

    void begin_drawing();
    void end_drawing();
    void flush_render_batch();
    void clear_background(Color c);
    void begin_texture_mode(const RenderTexture2D& target);
    void end_texture_mode();
    void begin_scissor_mode(int x, int y, int width, int height);
    void end_scissor_mode();
    void draw_rectangle_rec(const Rectangle& rec, Color color);
    void draw_line_v(Vector2 start, Vector2 end, Color color);
    void draw_triangle(Vector2 a, Vector2 b, Vector2 c, Color color);
    void set_viewport(int x, int y, int width, int height);
    void begin_mode_3d(const Camera3D& camera);
    void end_mode_3d();
    void draw_grid(int slices, float spacing);

    void set_target_fps(int fps);
    void set_window_title(const char* title);

    bool is_key_down(int k);
    bool is_mouse_button_down(int b);
    int get_mouse_x();
    int get_mouse_y();
    float get_mouse_wheel_move();
    Vector2 get_mouse_position();
    void set_mouse_offset(int ox, int oy);
    void set_mouse_scale(float sx, float sy);

    Texture2D load_texture_from_image(const Image& img);
    Image load_image_from_screen();
    void unload_image(const Image& img);
    void unload_texture(const Texture2D& tex);

    RenderTexture2D load_render_texture(int w, int h);
    void unload_render_texture(const RenderTexture2D& rt);

    void draw_texture_pro(
        const Texture2D& tex,
        const Rectangle& src,
        const Rectangle& dst,
        Vector2 origin,
        float rotation,
        Color tint);

    int load_model(const char* path);
    bool has_model(int model_id);
    void draw_model(int model_id);
    void unload_model(int model_id);
    bool has_loaded_models();
    void draw_loaded_models();
    void unload_all_models();
}
#endif
