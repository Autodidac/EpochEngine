# Legacy Feature Map

The old `legacy/` tree has been retired. This file records where its real,
still-useful functionality lives in the active Epoch source so we do not lose
important behavior while cleaning up the repo.

## Preserved in active source

- Allocator utilities:
  `Engine/modules/autility.allocator.ixx`
- Application module registry:
  `Engine/modules/aapplicationmodule.ixx`
- Code inspection helpers:
  `Engine/modules/autility.codeinspector.ixx`
- Script compiler and reload flow:
  `Engine/modules/aengine.scripting.compiler.ixx`,
  `Engine/src/aengine.scripting.compiler.cpp`,
  `Engine/modules/ascripting.system.ixx`
- File watching:
  `Engine/modules/autility.filewatch.ixx`,
  `Engine/src/afilewatch.cpp`
- Image writing:
  `Engine/modules/aimage.writer.ixx`,
  `Engine/modules/aimageatlaswriter.ixx`
- Mipmap atlas support:
  `Engine/modules/amipmapatlas.ixx`
- Movement events:
  `Engine/modules/aengine.event.movement.ixx`
- String conversion:
  `Engine/modules/autility.string.converter.ixx`
- Retry-once utilities:
  `Engine/modules/aengine.core.utilities.ixx`
- Task graph with DOT output:
  `Engine/modules/aengine.taskgraph.dotsystem.ixx`
- Linux OpenGL / multiplexer ownership:
  `Engine/src/aengine.context.multiplexer.linux.cpp`,
  `Engine/modules/acontext.opengl.context.ixx`

## Explicitly deferred

- Scene snapshot persistence:
  the old `SceneSnapshot`, serializer, and save/load code was only a partial
  archive surface and does not map cleanly onto the current module-first
  `ascene` API yet. It should come back only as a modern Epoch scene/history
  service, not as a direct legacy header transplant.

## Removed as archive noise

- The retired `legacy/` headers and source snapshots.
- One-off README capture helper:
  `Tools/capture_readme_screens.ps1`

This keeps the active engine honest: real features stay in Epoch-owned modules,
while unfinished archive stubs do not masquerade as supported systems.
