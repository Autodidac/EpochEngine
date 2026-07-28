import epoch.gui.font;

namespace font = epochengine::gui_lib::font;

int main()
{
    constexpr font::BitmapGlyph glyph = font::default_glyph('a');
    static_assert(glyph.rows[0] == 0x0e);
    static_assert(font::pixel_on(glyph, 1, 0));
    static_assert(!font::pixel_on(glyph, 0, 0));

    constexpr epochengine::gui_lib::Vec2 measured = font::measure_text("AB\nC", 2.0f);
    static_assert(measured.x == 22.0f);
    static_assert(measured.y == 32.0f);

    constexpr font::BitmapGlyph fallback = font::default_glyph('\x01');
    static_assert(font::pixel_on(fallback, 1, 0));
    return 0;
}
