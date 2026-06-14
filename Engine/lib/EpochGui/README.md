# EpochGui

EpochGui is the C++23 module/static-library surface for the reusable Epoch GUI
layout library. The current mirror metadata tracks the EpochEngine `v0.87.29`
source line. The canonical EpochEngine source still lives in the engine tree:

- `Engine/lib/EpochGui/modules/epoch.gui.ixx`
- `Engine/include/epoch/gui/*.hpp`
- `Engine/src/gui/*.cpp`

This directory owns the mirror build and documentation files for
`Autodidac/EpochGui`; it must not fork, duplicate, or wrap editor/runtime code.

## Current Payload

The first bounded payload is a versioned static library with backend-neutral
geometry and layout helpers exported by `epoch.gui`:

- `modules/epoch.gui.ixx` as the module interface
- `epoch/gui/floating_window.hpp` plus `src/gui/floating_window.cpp`
- `epoch/gui/layout_primitives.hpp` backed by `src/gui/floating_window.cpp`
- `epoch/gui/popup_layout.hpp` plus `src/gui/popup_layout.cpp`
- `epoch/gui/dock_layout.hpp` plus `src/gui/dock_layout.cpp`
- `epoch/gui/dockable_window.hpp` plus `src/gui/dockable_window.cpp`
- `epoch/gui/version.hpp` for the mirror name and `0.87.29` version constants

The public namespace is `epochnamespace::gui_lib`. New integrations should
prefer:

```cpp
import epoch.gui;
```

Compatibility headers remain for the current engine adapter while call sites
move over. The implementation is module-owned and includes OOP controllers such
as `FloatingWindowController`, `PopupLayoutController`,
`LayoutPrimitiveController`, `DockLayoutController`, and
`DockableWindowController`, all deriving from `LayoutController`. It does not
include `editor.cpp`, `engine.cpp`, context sources, renderer backends, runtime
assets, generated output, or `Engine/src/engine.gui.cpp`.

Native popout windows, desktop docking hosts, editor tool panes, and routed
contexts are integration-layer features outside this library. Games, mobile
apps, console targets, and headless tools can link only the portable
layout/state primitives and omit floating GUI host routes entirely.

## Supported Layouts

The CMake and MSVC files support two source layouts:

- In-tree EpochEngine checkout: this directory is `Engine/lib/EpochGui`, and the
  source root is detected two directories above it.
- Standalone mirror checkout: place `modules`, `include/epoch/gui`, and
  `src/gui` beside this README, and the source root is the repository root.

If neither layout applies, pass an explicit source root to CMake:

```powershell
cmake -S Engine/lib/EpochGui -B build/EpochGui -DEPOCHGUI_SOURCE_ROOT=Engine
```

## CMake

From an EpochEngine checkout:

```powershell
cmake -S Engine/lib/EpochGui -B build/EpochGui
cmake --build build/EpochGui --target EpochGui --config Debug
```

From a standalone `Autodidac/EpochGui` checkout:

```powershell
cmake -S . -B build
cmake --build build --target EpochGui --config Debug
```

The CMake target is `EpochGui`. Compatibility aliases are also provided as
`epoch_gui` and `Autodidac::EpochGui`.

The CMake project version is `0.87.29`, matching the engine source line that
adds OpenGL-first sampled render-surface preview proof while keeping reusable
GUI layout state separate from editor/runtime context ownership.

## Visual Studio

From an EpochEngine checkout:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine/lib/EpochGui/EpochGui.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
```

From a standalone mirror checkout, open or build `EpochGui.vcxproj` from the
repository root after placing `include/epoch/gui` and `src/gui` beside it.

## Mirror Rules

- Keep the mirror source-only. Do not check in build output, packages, caches,
  captures, generated projects, or editor/runtime assets.
- Keep `.gitattributes`, `.gitignore`, `LICENSE`, CMake, MSVC, and this README
  with the standalone `Autodidac/EpochGui` repository.
- Keep reusable GUI implementation in `include/epoch/gui` and `src/gui`.
- Keep the module interface in `modules/epoch.gui.ixx`.
- Keep this directory focused on standalone build metadata and mirror docs.
- Promote new reusable controls through EpochEngine first, then update the
  mirror payload once the source and build evidence are real.
- Promotion is mechanical and must move as one focused batch: update public
  headers under `Engine/include/epoch/gui`, implementation files under
  `Engine/src/gui`, `modules/epoch.gui.ixx`, in-tree CMake, standalone CMake,
  MSVC `.vcxproj`/`.filters`, the engine adapter imports/wrappers, this README,
  and the standalone `Autodidac/EpochGui` mirror. Build the in-tree and
  standalone Debug/Release library targets before claiming the payload changed.
- Carry the EpochEngine license terms into the standalone mirror root.
