# EpochGui

EpochGui is a portable C++23 GUI layout, input-adapter, raster-data, and geometry library used by EpochEngine and standalone applications.

It owns reusable GUI state, layout calculations, hit testing, text-control behavior, docking metadata, an embedded fallback bitmap font, bounded PPM decoding, and optional renderer-neutral helpers. It does not own editor/runtime code, platform windows, OpenGL, or another rendering backend.

## Modules

### `epoch.gui`

The core module provides:

- `Vec2` and `Rect`
- Floating-window state and layout
- Splitters and progress bars
- Loading-screen layout
- Selectable rows and segmented controls
- Popup placement and state
- Docking and dockable-window state
- Panel-host state
- Text editing, selection, navigation, and scrolling

```cpp
import epoch.gui;
```

### `epoch.gui.font`

The font module provides the built-in renderer-neutral fallback font:

- Hard-coded 5x7 glyph rows stored as compact bit fields
- Uppercase letters, digits, common punctuation, and lowercase folding
- Constant glyph, character-advance, and line-advance metrics
- Pixel testing without allocation or file I/O
- Text measurement using the same metrics

```cpp
import epoch.gui.font;

namespace font = epochengine::gui_lib::font;

constexpr font::BitmapGlyph glyph = font::default_glyph('A');
if (font::pixel_on(glyph, 2, 0))
{
    // Emit one pixel quad through the application's renderer.
}
```

The embedded font is the always-available default. Applications can still layer a richer font system over EpochGui.

### `epoch.gui.image`

The image module provides bounded, renderer-neutral raster loading and layout:

- ASCII P3 and binary P6 PPM decoding
- 8-bit and 16-bit PPM sample support
- Explicit width, height, pixel-count, and file-size limits
- RGBA8 output with no OpenGL, Vulkan, DirectX, or platform dependency
- Contain and stretch layout inside an EpochGui `Rect`
- Per-pixel rectangle calculation for software or simple batched renderers

```cpp
import epoch.gui.image;

namespace image = epochengine::gui_lib::image;

const image::ImageResult loaded = image::load_ppm_file("assets/example.ppm");
if (loaded)
{
    const image::RasterImageLayout layout = image::make_raster_image_layout(
        loaded.image,
        { { 20.0f, 20.0f }, { 640.0f, 360.0f } },
        image::ImageFit::contain);
}
```

EpochGui decodes and lays out raster data. The consuming renderer remains responsible for uploading a texture or emitting geometry.

### `epoch.gui.rounded_rect` — optional, disabled by default

The optional rounded-rectangle module generates renderer-neutral triangle meshes for:

- Filled rounded rectangles
- Independent radius per corner
- Optional borders with configurable width
- Pill-shaped controls
- Proportional radius normalization when corners overlap
- Configurable tessellation from 1 to 64 segments per corner

It contains no OpenGL, DirectX, Vulkan, platform-window, timing, or animation code.

Enable it with:

```text
-DEPOCHGUI_ENABLE_ROUNDED_RECT=ON
```

Then import it:

```cpp
import epoch.gui.rounded_rect;

namespace rounded = epochengine::gui_lib::rounded_rect;

const rounded::RoundedRectMesh mesh = rounded::make_rounded_rect_mesh({
    .bounds = { { 40.0f, 40.0f }, { 240.0f, 96.0f } },
    .radii = { 18.0f, 18.0f, 18.0f, 18.0f },
    .border_width = 3.0f,
    .segments_per_corner = 12
});
```

### `epoch.gui.input` — optional fallback, disabled by default

Most applications should continue using their existing engine, platform, or window-system input layer. `epoch.gui.input` exists for small tools, standalone demos, tests, and integrations that do not already provide normalized per-frame input.

Enable it explicitly:

```text
-DEPOCHGUI_ENABLE_INPUT=ON
```

Then import it:

```cpp
import epoch.gui.input;

namespace input = epochengine::gui_lib::input;

input::InputTracker tracker;
tracker.set_pointer_position({ mouse_x, mouse_y });
tracker.set_pointer_button(input::PointerButton::left, button_down);

const auto floating_input = input::floating_window_input(tracker.frame());
```

The fallback module provides:

- Per-frame pointer, wheel, key, and modifier transitions
- Conversion into existing EpochGui `FloatingWindowInput` and `PopupInput`
- Right-click context-menu requests
- Borderless replacement-title-bar layout and hit testing
- Resize-edge/corner, caption, minimize, maximize, and close regions
- Window-command detection for custom chrome

It contains no Win32, X11, Cocoa, SDL, GLFW, rendering, or operating-system calls. Native hosts remain responsible for feeding events and performing requested native window actions.

## CMake

Build the core library:

```powershell
cmake -S . -B build
cmake --build build --target EpochGui --config Release
```

Build every optional feature and test:

```powershell
cmake -S . -B build \
  -DEPOCHGUI_ENABLE_ROUNDED_RECT=ON \
  -DEPOCHGUI_ENABLE_INPUT=ON \
  -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The main target is `EpochGui`. Compatibility aliases are available as:

- `epoch_gui`
- `Autodidac::EpochGui`

## Visual Studio project

The checked-in `EpochGui.vcxproj` builds the core library, embedded font, and PPM image support by default.

Enable optional features explicitly:

```powershell
msbuild EpochGui.vcxproj \
  /p:Configuration=Release \
  /p:Platform=x64 \
  /p:EpochGuiEnableRoundedRect=true \
  /p:EpochGuiEnableInput=true
```

## Repository layout

```text
modules/epoch.gui.ixx                  Core public C++23 module
modules/epoch.gui.font.ixx             Embedded fallback bitmap font
modules/epoch.gui.image.ixx            Raster image API and layout
modules/epoch.gui.rounded_rect.ixx     Optional rounded-geometry module
modules/epoch.gui.input.ixx            Optional fallback input module
include/gui/                           Compatibility headers
src/epochgui/                          Backend-neutral implementations
tests/font_tests.cpp                   Embedded-font tests
tests/image_tests.cpp                  PPM decoder and image-layout tests
tests/text_control_tests.cpp           Core text-control tests
tests/rounded_rect_tests.cpp           Optional rounded-geometry tests
tests/input_tests.cpp                  Optional fallback-input tests
```

## Boundaries

EpochGui remains backend-neutral. It owns reusable font bits, decoded raster data, and raster layout, but not font shaping, GPU texture creation, shaders, draw submission, native event translation, operating-system windows, native frame presentation, or host commands. Existing engine or platform input should be preferred when available.
