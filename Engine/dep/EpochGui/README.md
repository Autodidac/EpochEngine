# EpochGui

MPL-2.0 permits commercial use, including proprietary larger works subject to its covered-file obligations. See [Commercial use and giving back](#commercial-use-and-giving-back) for licensing, origin links and voluntary credit requests.

EpochGui is a portable C++23 GUI layout, input-adapter, raster-data, and geometry library used by EpochEngine and standalone applications.

Bundled library version: `0.90.0`. The portable library code is checked against
the pinned standalone revision in `Engine/dep/dependencies.json`. Engine build
integration and documentation have their own changes; this is not a full-tree mirror.

It owns reusable GUI state, layout calculations, hit testing, text-control behavior, docking metadata, an embedded fallback bitmap font, bounded PPM decoding, and optional renderer-neutral helpers. It does not own editor/runtime code, platform windows, OpenGL, or another rendering backend.

## Modules

### `epoch.gui`

The core module provides:

- `Vec2` and `Rect`
- Floating-window state and layout
- Splitters and progress bars
- Loading-screen layout
- Selectable rows, segmented controls, and responsive tab-strip planning that
  preserves the active route behind a bounded overflow selector, plus
  deterministic wrapped keyboard navigation that skips disabled routes
- Popup placement and state
- Docking, dock guides, context insertion grids, and dockable-window state
- Panel-host state
- Reusable node-graph, system, tile, and virtualized asset-grid workspaces
- A reusable high-density hierarchy-tree controller with stable IDs,
  ancestor-retaining filtering, range/toggle selection, keyboard traversal,
  virtualized row planning, scroll-to-visible, locked/disabled rows, and
  deterministic context-action routing
- Text editing, selection, navigation, and scrolling

```cpp
import epoch.gui;
```

### `gui/hierarchy_tree.hpp`

The renderer-neutral hierarchy controller is a public header/source API for
dense trees such as outliners, GUI/component hierarchies, script browsers, and
evidence explorers. It provides stable numeric identities, transactional tree
replacement, deterministic ordering, expand/collapse state, ancestor-retaining
filtering, single/toggle/range selection, keyboard traversal, virtualized row
planning, scroll-to-visible calculations, locked and disabled row policy, and
deterministic context-action routing.

```cpp
#include <gui/hierarchy_tree.hpp>

namespace tree = epochengine::gui_lib::hierarchy_tree;

tree::Controller hierarchy{};
const tree::ReplaceResult admitted = hierarchy.replace_nodes(nodes);
const tree::LayoutPlan visible = hierarchy.plan_rows(viewport);
```

EpochGui owns the portable state and calculations. A consuming application
still supplies its domain nodes, draws the planned rows, feeds input commands,
and executes accepted context routes. This standalone release does not claim
that a particular EpochEngine editor surface has adopted the control yet.

### `epoch.gui.tile_workspace`

The tile-workspace module provides reusable authoring control state without
owning project documents, render backends, native windows, or files:

- tool and palette selection
- virtualized palette rows
- ordered layer selection and visibility state
- bounded grid layout and cell hit testing
- cursor-anchored zoom and clamped pan
- renderer-neutral layout snapshots and metrics

```cpp
import epoch.gui.tile_workspace;
```

Applications bind this portable controller to their own temporal map document,
persistence, rendering, and input adapters.

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

### `epoch.gui.input`

Most applications should continue using their existing engine, platform, or window-system input layer. `epoch.gui.input` provides backend-neutral per-frame normalization and modal arbitration for small tools, standalone demos, tests, and integrations. It is part of the core library so every host can enforce the same modal-input boundary.

Import it directly:

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
- Modal keyboard capture plus bounded or fully blocked pointer arbitration

It contains no Win32, X11, Cocoa, SDL, GLFW, rendering, or operating-system calls. Native hosts remain responsible for feeding events and performing requested native window actions.

## CMake

Build the core library:

```powershell
cmake -S . -B build
cmake --build build --target EpochGui --config Release
```

Build the optional rounded-geometry feature and every test:

```powershell
cmake -S . -B build \
  -DEPOCHGUI_ENABLE_ROUNDED_RECT=ON \
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
  /p:EpochGuiEnableRoundedRect=true
```

## Repository layout

```text
modules/epoch.gui.ixx                  Core public C++23 module
modules/epoch.gui.font.ixx             Embedded fallback bitmap font
modules/epoch.gui.image.ixx            Raster image API and layout
modules/epoch.gui.rounded_rect.ixx     Optional rounded-geometry module
modules/epoch.gui.input.ixx            Core input and modal-arbitration module
include/gui/                           Compatibility headers
src/epochgui/                          Backend-neutral implementations
tests/font_tests.cpp                   Embedded-font tests
tests/image_tests.cpp                  PPM decoder and image-layout tests
tests/dock_layout_tests.cpp            Dock-context grid and insertion tests
tests/node_graph_workspace_tests.cpp   Node-graph workspace tests
tests/system_workspace_tests.cpp       Systems workspace tests
tests/asset_grid_tests.cpp             Virtualized asset-grid tests
tests/text_control_tests.cpp           Core text-control tests
tests/rounded_rect_tests.cpp           Optional rounded-geometry tests
tests/input_tests.cpp                  Input and modal-arbitration tests
```

## Boundaries

EpochGui remains backend-neutral. It owns reusable font bits, decoded raster data, and raster layout, but not font shaping, GPU texture creation, shaders, draw submission, native event translation, operating-system windows, native frame presentation, or host commands. Existing engine or platform input should be preferred when available.

## Commercial use and giving back

This is a simple guide to [MPL-2.0](LICENSE). The full license is the legal text.

### Allowed

- [x] Use, change and sell this library, by itself or inside your app.
- [x] Keep changes to this library private while the software stays with you
  or inside your organization. Giving copies to others triggers the rules below.
- [x] Keep your own app's code private if it does not contain code copied from
  this library. Using or linking to the library does not make your whole app
  open source.

### Rules For Distribution

*Required when you give copies to people outside your organization, or publicly.*

- [ ] Give those people access to this library's source code, including your
  changes. Tell them where and how to get the matching source without undue delay.
- [ ] If you copied this library's code into another source file, make that whole
  file available too. You do not have to share your other, separate app files.
- [ ] Keep those shared library files under MPL-2.0. Include the license, or tell
  people how to get it.
- [ ] Keep the existing copyright, patent, license and warranty notices in the
  source. Correcting a notice that is factually wrong is allowed.
- [ ] Let recipients use, change and share those library files under MPL-2.0.
  Your app's terms must not take those rights away.
- [ ] If you charge for providing the source, charge no more than it costs you
  to provide it. This does not limit what you charge for your app.

You do not have to send changes to us or post them for the whole world. The
source-sharing duty is to the people who receive your copy of the software.

### Not allowed

- [ ] Do not give people the software while denying them the library source
  they are entitled to under the rules above.
- [ ] Do not erase the original authors' notices or pretend you wrote their code.
- [ ] Do not assume MPL lets you use our names or logos as your own brand, or
  claim we approved your product. See the separate [branding rules](TRADEMARKS.md).

### Recommended, entirely optional

- [ ] Make the original author and [source link](https://github.com/Autodidac/EpochGui) easy for other
  developers to find. Use your dependency list, credits, notices or technical docs;
  no particular README or About-screen placement is requested.
- [ ] If you list similar libraries or tools your project uses, list this one
  alongside them. A link to [EpochEngine](https://github.com/Autodidac/EpochEngine)
  also helps support the work.
- [ ] Say what you added and what came from this library. A new name, wrapper or
  small extra feature does not make the underlying library your invention.
- [ ] Share useful fixes, features, tests and documentation through a
  [pull request](https://github.com/Autodidac/EpochGui/pulls); accepted contributions receive author credit.
- [ ] Follow [CONTRIBUTING.md](CONTRIBUTING.md) when choosing to contribute.

**MPL does not require an upstream pull request. But it would be a lot cooler if
you shared!** Contributions help these libraries support themselves and continued
work on EpochEngine. These recommendations add no copyright-license conditions.

See [Mozilla's MPL FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/) for examples.

## License

This library's own code uses **MPL-2.0**. [LICENSE](LICENSE) contains the unchanged
legal terms. Other authors' code keeps its own license, and earlier copies keep
the rights already granted for them.

See [NOTICE](NOTICE) for the original author/source, [TRADEMARKS.md](TRADEMARKS.md)
for names and logos, and [CONTRIBUTING.md](CONTRIBUTING.md) for sending improvements.
