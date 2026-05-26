# GUI Library Architecture

Epoch GUI is an engine-internal library surface. It is not a one-off editor
overlay, and it is not the Console Dock. The editor, launcher, project hub,
systems view, AI sandbox, package manager, scripting view, and future popout
windows should compose shared GUI primitives from the same layer.

## Current Boundary

- Public module API: `Engine/modules/engine.gui.ixx`
- Current implementation: `Engine/src/engine.gui.cpp`
- Primary consumer: `Engine/src/editor.cpp`
- Backend replay consumers: renderer/context code that drains deferred GUI
  batches after scene rendering

The current implementation is still physically compact, but the ownership rule
is already library-like: reusable controls are added to `engine.gui` first, then
editor domains consume them. Editor workspaces should not reimplement generic
buttons, tabs, dropdowns, scroll areas, text inputs, modal chrome, or clipping
logic.

## Draw Model Guardrail

The current working draw model is a protected contract, not a playground. The
OpenGL editor path is stable only when normal GUI/backend work is drained, the
scene preview renders once, follow-up work is drained, only the explicit GUI
top-layer batch for command menus/modal chrome is replayed above it, and
capture/present happens afterward. That order is the baseline for menu, pane,
and scene composition work.

Fix command-menu z-order, modal layering, resize chrome, and scene/pane
composition by improving the GUI library or the explicit top-layer/draw-model
batch. Do not skip queue drains, move the whole GUI into a deferred batch, or
change backend frame order as a shortcut unless the task is specifically a
draw-model change with build evidence and operator eye-test proof across the
affected contexts.

Command menus, dropdowns, and modal chrome that must sit above the scene
viewport should wrap their window draw in `gui::begin_top_layer()` /
`gui::end_top_layer()` so the renderer can replay only those sprites after the
scene pass. Do not rely on menu creation order alone for z-order.

On OpenGL, top-layer sprites are replay-only. They must not also be queued in
the normal deferred GUI batch, or command menus can slowly flip between
scene-under and scene-over composition while a dropdown is open. Backends that
do not yet consume the explicit top-layer replay path keep their existing
normal-batch behavior until their presenter owns a matching replay pass.

## Intended Layers

- Primitive widgets: labels, buttons, connected tabs, dropdown/select boxes,
  progress bars, text inputs, scrollable text panels, image/runtime-surface
  views, and future checkboxes, sliders, tree views, and list views.
- Button state: use `gui::button_selected` for active/open toolbar, menu, tab,
  and window-chrome buttons so selection is explicit and does not flicker
  through transient hover/press state while top-layer GUI is replayed.
- Layout and docking: windows, splitters, resize handles, scroll extents,
  focus routing, z-order, modal scrims, context menus, and future popout hosts.
- Theme and rendering: palette ownership, font/glyph metrics, clipping,
  runtime-surface atlas use, deferred GUI replay, and backend-safe present
  ordering.
- Editor composition: scene/game, assets, project/build, systems, AI sandbox,
  scripting, and package manager workspaces choose domain data and layout, but
  do not own generic widget behavior.

## New Control Rule

Every new editor control starts as a reusable GUI primitive unless it is truly
domain-specific. The first reusable dropdown/select-box is the local AI model
selector; package selection, backend selection, project settings, asset
selection, and script selection should follow that same path instead of adding
new one-off rows of buttons.

Before a control is considered ready, it needs:

- stable clipping and hit testing inside scrollable/resizable panes
- dropdown/select boxes must keep their own mouse-wheel focus while open instead
  of letting a parent scroll pane consume the wheel first
- progress bars are shared GUI primitives for package installs, workspace
  loading, updater/cache operations, generated-project builds, and any future
  visible long-running editor action; do not draw one-off progress rows in
  Console Dock or domain code when `engine.gui` can own the behavior
- input profiles are shared engine/editor contracts, not per-surface hacks.
  Camera reset begins with the `Home` preview hotkey, but rebinding, project
  export, and package opt-in must flow through a reusable input-configuration
  surface before being promoted to generated projects
- text inputs must support basic desktop editing affordances before promotion:
  first-pass whole-field copy, cut, paste, select-all, and visible domain
  clipboard actions are acceptable, but true ranged text selection/caret
  movement remains the next gate
- keyboard/mouse focus behavior that does not leak across panes or contexts
- backend-safe rendering through the shared GUI replay path
- a documented owner and expected consumers
- build/test evidence before roadmap completion is claimed

## Console Dock Boundary

Console Dock is a compact evidence and diagnostic strip. It may show current
status, build evidence, model selection state, and logs, but central workflows
belong in proper GUI windows:

- Project and package controls belong in Project/Package workspaces or modals.
- Systems graphs and time controls belong in the Systems workspace.
- AI sandbox controls belong in the AI workspace and Inspector, with compact
  status mirrored in the dock only when useful. The World Outliner may expose an
  `OS AI` tab with compact model/loop state, chat transcript, prompt entry,
  and plan controls because that keeps the selected AI model attached to normal editor chrome
  instead of hiding control in a separate floating window.
- Scripting needs a real code/text editor surface, not a Console Dock submenu.

Current bottom-dock non-output tabs should use the same selectable
`scroll_text_panel` presentation as `Output`. Do not reintroduce property-row
blocks, buttons, dropdowns, or progress widgets into Project, Assets, AI, or
Systems dock pages; those controls belong in central workspaces, modal windows,
or Inspector-owned panels.

## Artifact And Smear Guard

Backends consume GUI sprites as pixel-space rectangles. Shared GUI code should
reject non-finite, empty, subpixel, or unreasonable sprite extents before they
enter a deferred backend batch; backend replay code should clip those rectangles
to the active framebuffer before converting to normalized device coordinates.
This prevents scroll/dropdown/resize slivers from turning into vertical
barcode-like smear artifacts during DirectX or multicontext resize tests.

Current text rendering intentionally sanitizes non-ASCII/high-byte input to a
safe visible fallback until the font pipeline owns full UTF-8 shaping. UTF-8
continuation bytes are skipped so unsupported glyphs collapse instead of
turning one source banner into a wall of question marks. That is a stability
rule, not the final typography goal: mojibake in AI notes or local model replies
must be normalized before display/training capture, and hidden reasoning text
must never be promoted to chat output.

## Script Editing Gate

The Assets workspace owns the first visible Script Source Editor surface. It
loads the active `.ascript.cpp`, allows multiline edits through the shared
`engine.gui` text input, exposes explicit Copy Source and Paste Clipboard
actions, and saves/reloads through the same evidence log used by project
actions. This is only the first production-safe step. Promotion to a real code
editor still requires internal scroll/caret positioning, ranged selection,
keyboard navigation, syntax-aware display, and a cleaner split between preview,
editor, and build actions.

## Script Editor And Clipboard Gate

The current script surface is not a finished editor. It can locate and preview
script source, but a production scripting workspace still needs editable code
text, a visible caret and selection, copy/cut/paste/select-all, save/reload
evidence, build/run feedback, and predictable keyboard focus without holding a
mouse button down. Broken high-byte banners or mojibake in source previews must
be sanitized for display without corrupting the saved source.

Script editing should use shared GUI text primitives, not a one-off asset panel
hack. The acceptance gate is a script file that can be opened from Assets or the
Script Editor workspace, edited, saved, reloaded, copied/pasted, built, and run
with visible evidence and no Console Dock-only control path.

## Safe Split Plan

Do not split files only for aesthetics. The safe code split is:

1. Keep `engine.gui` as the public module name.
2. Move implementation chunks into owned source files only when CMake, MSVC
   project files, and filters are updated together.
3. Prefer slices that match responsibility: primitives, layout/docking, text,
   runtime surfaces, theme/rendering, and backend replay.
4. Build MSBuild Debug/Release and the active CMake preset before marking the
   split complete.

Until that split is fully verified, `Engine/src/engine.gui.cpp` remains the
canonical implementation file and this document is the contract for keeping new
GUI work library-shaped.

The next ownership step is a real linkable GUI target, not just more code inside
the editor. Keep `engine.gui` as the public module/API, then split reusable
primitives, layout/docking, text/clipboard, progress bars, modal chrome, and
backend replay support into a separately linked static/shared object target only
when CMake, MSVC project files, filters, and all active backend consumers are
updated and validated together.
