# Context Module Audit

The following table summarises the current state of every file whose name includes "context".  "Earmark" indicates whether the file should be queued for removal in a future cleanup pass.

| Path | Status | Notes | Earmark |
| --- | --- | --- | --- |
| `modules/aengine.core.context.ixx` | Active | Core context abstraction referenced throughout the engine. | No |
| `modules/aengine.context.control.ixx` | Active | Provides command helpers for draw operations; required by the multiplexer. | No |
| `modules/aengine.context.multiplexer.ixx` | Active | Coordinates backend switching; heavily used by `src modules/aengine.context.multiplexer.win.cppm`. | No |
| `src modules/aengine.context.multiplexer.win.cppm` | Active | Implements multiplexer runtime; necessary for desktop builds. | No |
| `modules/acontext.opengl.renderer.ixx` | Legacy Stub | Mostly commented-out scaffolding with alternate renderer hooks; not referenced at runtime. | Maybe |
| `modules/acontext.opengl.state.ixx` | Active | Shares renderer state structs across backends; used by Raylib helpers. | No |
| `modules/aengine.context.type.ixx` | Active | Enumerations for backend selection; consumed by virtually every renderer. | No |
| `modules/aengine.context.window.ixx` | Active | Wraps native window handles; needed by all backends. | No |
| `modules/acontext.opengl.context.ixx` | Active | Fully implemented OpenGL backend. | No |
| `modules/asfmlcontext.ixx` | Active | Used by the SFML backend wiring in `src/acontext.cpp`. | No |
| `modules/acontext.sdl.context.ixx` | Active | Required for the SDL backend and renderer integration. | No |
| `modules/acontext.softrenderer.context.ixx` | Active | Powers the software renderer; still under development but exercised by tooling. | No |
| `modules/acontext.noop.context.ixx` | Minimal Stub | Only compiled for headless builds; harmless placeholder for tests. | No |
| `modules/acontext.raylib.context.ixx` | Active | Primary Raylib backend implementation. | No |
| `modules/acontext.raylib.input.ixx` | Active | Required to translate Raylib input to engine enums. | No |
| `modules/acontext.raylib.state.ixx` | Active | Shared Raylib state container; consumed by renderer and context glue. | No |
| `modules/acontext.sdl.renderer.ixx` | Partial | SDL renderer wrapper is wired in but lacks texture batching; revisit once SDL backend stabilises. | No |
| `src/acontext.cpp` | Active | Registers every backend and owns the dispatch table. | No |

Continue reviewing entries marked "Maybe" before performing destructive changes; the prior "Yes" candidates have been excised from the tree.
