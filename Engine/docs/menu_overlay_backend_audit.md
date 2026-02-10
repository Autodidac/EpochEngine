# Menu overlay backend audit

This checklist records where each backend wires menu-sensitive primitives and notes any
mismatches relative to the software renderer baseline.

## Software (baseline)
- [x] Window events keep `ctx->width/height` aligned with the Win32 client size and
      the software framebuffer via the multiplexer and `resize_framebuffer` guard.【F:AlmondShell/src/acontextmultiplexer.cpp†L720-L812】【F:AlmondShell/modules/acontext.softrenderer.context.ixx†L1-L1】
- [x] `get_width/height` forward to the software framebuffer dimensions used by
      `draw_sprite`, so layout queries and rendering share the same pixel grid.【F:AlmondShell/modules/acontext.softrenderer.context.ixx†L1-L1】
- [x] `draw_sprite` consumes pixel-space coordinates without additional scaling,
      matching the cached menu positions.【F:AlmondShell/modules/acontext.softrenderer.context.ixx†L1-L1】
- [x] Mouse polling keeps coordinates global and lets `get_mouse_position_safe`
      project them into the window client space used for hit-testing.【F:AlmondShell/modules/aengine.input.ixx†L1-L1】
- ✅ No parity issues observed; this is the reference behaviour.

## OpenGL
- [x] `opengl_initialize` seeds both the context and GL state with the window
      client size, and runtime resizes propagate through the multiplexer.
      【F:AlmondShell/modules/acontext.opengl.context.ixx†L1-L1】【F:AlmondShell/src/acontextmultiplexer.cpp†L720-L812】
- [x] `opengl_get_width/height` read back the current viewport dimensions so
      layout queries track the drawable surface.【F:AlmondShell/modules/acontext.opengl.context.ixx†L1-L1】
- [x] The OpenGL `draw_sprite` path resolves the live viewport before converting
      menu coordinates into clip-space quads.【F:AlmondShell/modules/acontext.opengl.textures.ixx†L1-L1】
- [x] Mouse input reuses the global polling path and the overlay’s explicit
      vertical flip keeps hit-testing consistent.【F:AlmondShell/src/acontext.cpp†L431-L468】【F:AlmondShell/modules/aengine.gui.menu.ixx†L1-L1】
- ✅ Behaviour matches the software baseline.

## SDL
- [x] SDL’s initialization and resize callback keep `ctx->width/height` in sync
      with the renderer output size collected each frame.【F:AlmondShell/modules/acontext.sdl.context.ixx†L1-L1】
- [x] `sdl_get_width/height` query the renderer output (falling back to the last
      known window size) so layout sees the same dimensions as the blitter.
      【F:AlmondShell/modules/acontext.sdl.context.ixx†L1-L1】
- [x] `draw_sprite` maps menu coordinates through the active renderer viewport
      without inventing extra scaling rules.【F:AlmondShell/modules/acontext.sdl.textures.ixx†L1-L1】
- [x] Mouse input piggybacks on the shared Windows polling path, yielding client
      coordinates compatible with the cached menu rectangles.【F:AlmondShell/src/acontext.cpp†L471-L507】【F:AlmondShell/modules/aengine.input.ixx†L1-L1】
- ✅ Behaviour matches the software baseline.

## Raylib
- [x] `dispatch_resize` forces `ctx->width/height` to the CLI “virtual” canvas
      while tracking the physical framebuffer separately.【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】
- [x] `raylib_get_width/height` currently return the logical window reported by
      Raylib (`GetScreenWidth/Height`).【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】
- [x] The Raylib `draw_sprite` implementation expects menu coordinates in the
      virtual canvas and scales them into the fitted viewport.【F:AlmondShell/modules/acontext.raylib.renderer.ixx†L1-L1】【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】
- [x] Mouse polling switches to Raylib’s per-frame cursor data and disables the
      global-to-client conversion after applying `SetMouseOffset/Scale`.【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】【F:AlmondShell/modules/acontext.raylib.input.ixx†L1-L1】
- [x] Viewport seeding now reads the live framebuffer during context creation so
      atlas-driven overlays stay aligned before the child window finishes
      docking.【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】
- [x] Align `ctx->get_width_safe()/get_height_safe()` with the virtual canvas so
      menu layout positions match the viewport scaling used during draw calls.【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】
- [x] Confirm that hit-testing uses the same scaled coordinates Raylib hands back
      after `SetMouseScale` to avoid menu hover mismatches.【F:AlmondShell/modules/acontext.raylib.context.ixx†L1-L1】【F:AlmondShell/modules/acontext.raylib.input.ixx†L1-L1】

Verification: Launch `--renderer=raylib --scene=fit_canvas --trace-menu-button0`, dock the pane, and confirm the logged GUI bounds for button 0 match the design-canvas dimensions reported by the immediate-mode layer while pointer hover events trigger without offset after resizing and context reacquisition.
