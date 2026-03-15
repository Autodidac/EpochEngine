# Epoch v0.82.15

## Highlights

- Replaced the old filewatch-oriented editor scripting loop with an engine-owned
  `Run` action that compiles and executes scripts against a stable host API.
- Added reusable shared preview cameras with `Editor` and `FPS` modes,
  keyboard travel, and right-mouse look for the multicontext editor scene.
- Fixed updater version parsing so remote checks can read either plain semver
  text or the full `aengine.version.ixx` module without dumping banner/BOM
  garbage into the console.

## Active code surfaces

- [Engine/include/epoch.script_api.h](../Engine/include/epoch.script_api.h)
- [Engine/modules/ascripting.system.ixx](../Engine/modules/ascripting.system.ixx)
- [Engine/modules/aengine.scripting.compiler.ixx](../Engine/modules/aengine.scripting.compiler.ixx)
- [Engine/src/scripts/rotate_all_entities.ascript.cpp](../Engine/src/scripts/rotate_all_entities.ascript.cpp)
- [Engine/modules/epoch.render.preview_grid.ixx](../Engine/modules/epoch.render.preview_grid.ixx)
- [Engine/src/aengine.cpp](../Engine/src/aengine.cpp)
- [Engine/src/aeditor.cpp](../Engine/src/aeditor.cpp)
- [Engine/modules/aengine.updater.system.ixx](../Engine/modules/aengine.updater.system.ixx)

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`
- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Compiled and invoked the default `rotate_all_entities` script against the new
  host API
- Launched the debug editor/runtime from `x64/Debug`
