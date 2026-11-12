# Legacy Feature Map

The old `legacy/` tree has been retired. This file records where its real,
still-useful functionality lives in the active Epoch source so we do not lose
important behavior while cleaning up the repo.

## Preserved in active source

- Allocator utilities:
  `Engine/modules/autility.allocator.ixx`
- Application module registry:
  `Engine/modules/application/application.registry.ixx`
- Code inspection helpers:
  `Engine/modules/utility/utility.codeinspector.ixx`
- Script compiler and reload flow:
  `Engine/modules/scripting/scripting.compiler.ixx`,
  `Engine/src/scripts/scripting.compiler_engine.cpp`,
  `Engine/modules/scripting/scripting.system.ixx`
- File watching:
  `Engine/modules/utility/utility.filewatch.ixx`,
  `Engine/src/platform/utility.filewatch.cpp`
- Image writing:
  `Engine/modules/image/image.writer.ixx`,
  `Engine/modules/image/image.atlas_writer.ixx`
- Mipmap atlas support:
  `Engine/modules/atlas/atlas.mipmap.ixx`
- Movement events:
  `Engine/modules/event/event.movement.ixx`
- String conversion:
  `Engine/modules/utility/utility.string_converter.ixx`
- Retry-once utilities:
  `Engine/modules/core/core.utilities.ixx`
- Task graph with DOT output:
  `Engine/modules/taskgraph/taskgraph.dotsystem.ixx`
- Linux OpenGL / multiplexer ownership:
  `Engine/src/renderers/host/host.context_linux.cpp`,
  `Engine/modules/opengl/opengl.context.ixx`

## Explicitly deferred

- Scene snapshot persistence:
  the old `SceneSnapshot`, serializer, and save/load code was only a partial
  archive surface and does not map cleanly onto the current module-first
  `scene` API yet. It should come back only as a modern Epoch scene/history
  service, not as a direct legacy header transplant.

## Removed as archive noise

- The retired `legacy/` headers and source snapshots.
- One-off README capture helper:
  `Tools/capture_readme_screens.ps1`

This keeps the active engine honest: real features stay in Epoch-owned modules,
while unfinished archive stubs do not masquerade as supported systems.
