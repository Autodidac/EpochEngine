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

The practical rule is strict: if an editor surface needs a control that normal
software would also need, create or extend the shared GUI primitive first. A
static text box is not an edit box, and one-off buttons are not a clipboard or
context-menu system. This keeps Epoch GUI reusable for future projects that may
ship mostly or entirely as GUI software, possibly with a custom renderer behind
the same control library.

## Draw Model Guardrail

The current working draw model is a protected contract, not a playground. The
OpenGL editor path is stable only when it builds the normal GUI/backend batch
before the scene, renders the scene preview once, drains follow-up work, replays
only the explicit GUI top-layer batch for command menus/modal chrome above it,
and captures/presents afterward. Overlay-priority state must not change the
OpenGL frame order by itself; top-layer replay owns scene-over menu composition.
That order is the baseline for menu, pane, and scene composition work.

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

Menu hit regions must match the exact drawn dropdown size. A stale row count can
make the outside-click guard close a menu while its drawn body is still being
interacted with, which looks like command-menu flicker even when the renderer
order is correct.

Outside-click dismissal is input-modal, not left-button-only. Command menus and
select boxes must drop focus on any click outside their active bounds, including
right-clicks used to open context menus elsewhere, so stale menu capture cannot
slow text editing or make toolbar buttons flash through inactive states.

On OpenGL, top-layer sprites are replay-only. They must not also be queued in
the normal deferred GUI batch, and the OpenGL renderer must not switch
pre-scene/post-scene GUI drain order when a dropdown is open. Either condition
can make command menus slowly flip between scene-under and scene-over
composition. Backends that do not yet consume the explicit top-layer replay path
keep their existing normal-batch behavior until their presenter owns a matching
replay pass.

## Intended Layers

- Primitive widgets: labels, buttons, connected tabs, dropdown/select boxes,
  progress bars, text inputs, scrollable text panels, image/runtime-surface
  views, checkable/action list rows, and future sliders, tree views, and richer
  list views.
- Composite modal bodies: package manager, settings, source-update prompts, and
  other dense control surfaces should put changing details inside clipped shared
  scroll areas. Progress bars, action buttons, and modal-level status chrome stay
  outside the scrolling body so scroll extents cannot bleed them into the scene
  viewport or debugger area.
- Button state: use `gui::button_selected` for active/open toolbar, menu, tab,
  and window-chrome buttons so selection is explicit and does not flicker
  through transient hover/press state while top-layer GUI is replayed.
- Pressed state is release-frame stable. Reusable buttons, image buttons, tabs,
  titlebar close controls, and select-box options keep their active color until
  the click release has been processed, so GUI replay cannot flash controls back
  through hover/default colors on the same frame that an action fires.
- Layout and docking: windows, splitters, resize handles, scroll extents,
  focus routing, z-order, modal scrims, context menus, and future popout hosts.
- Theme and rendering: palette ownership, font/glyph metrics, clipping,
  runtime-surface atlas use, deferred GUI replay, and backend-safe present
  ordering. Theme preference labels, option data, preference resolution, and
  scoped theme application belong to `engine.gui`; editor surfaces may store the
  selected preference but must not recreate theme tables in domain code. Current
  exposed choices are Follow System Dark Mode, Professional Dark, and Classic
  Launcher; Follow System Dark Mode resolves to the dark tool palette until a
  real platform light/dark palette bridge lands.
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
- dropdown/select boxes close on outside click and align the scroll position near
  the selected option when opened; editor surfaces should not duplicate this
  behavior in domain code
- progress bars are shared GUI primitives for package installs, workspace
  loading, updater/cache operations, generated-project builds, and any future
  visible long-running editor action; do not draw one-off progress rows in
  Console Dock or domain code when `engine.gui` can own the behavior
- Package Manager uses a reusable list/action/detail shape. Package selection
  should not be a cramped one-line combo box when rows need per-package status,
  Install/Remove/Review Gate actions, provenance, and bounded progress. Package
  names and row summaries use selectable `text_link` rows, not fake buttons;
  only true commands remain buttons.
- automatic content containers are a GUI-library responsibility. Lists,
  details panes, source editors, progress rows, graph canvases, and modal bodies
  should resize from viewport/content constraints through reusable primitives so
  editor workspaces do not keep reinventing brittle row math.
- dynamic word wrap is the default for reusable text surfaces. Logs, chat
  transcripts, inspector rows, package details, update modals, source previews,
  and other scrollable panels should measure against their actual viewport width
  and keep scrollbar extents in pixel space so resizing cannot smear stale glyph
  columns or truncate important status text unless a control explicitly opts out.
- source editors are shared GUI primitives. They process text input directly,
  render only visible source lines inside the scroll clip, and expose
  click-to-caret placement, drag ranged selection, focused navigation hotkeys,
  Ctrl+A/C/X/V, and right-click Select All/Copy/Cut/Paste through the GUI
  context-menu path; editor workspaces should not replace them with static text
  dumps or ad hoc clipboard buttons.
- input profiles are shared engine/editor contracts, not per-surface hacks.
  `v0.84.54` keeps movement bindings and look bindings non-overlapping in the
  shipped presets, after `v0.84.53` introduced named
  movement/look/reset/cancel/confirm actions, Win32 navigation-key coverage,
  project runtime camera handoff, and visible Editor Settings/Project workspace
  profile selectors. Rebinding, project export, and package opt-in must flow
  through a reusable input-configuration surface before being promoted to
  generated projects
- text inputs must support basic desktop editing affordances before promotion:
  focused editing, visible caret state, ranged selection, copy, cut, paste,
  select-all, stable scroll focus, and context-menu actions without requiring a
  held mouse button
- keyboard/mouse focus behavior that does not leak across panes or contexts
- backend-safe rendering through the shared GUI replay path
- a documented owner and expected consumers
- build/test evidence before roadmap completion is claimed

## Console Dock Boundary

Console Dock is a compact evidence and diagnostic strip. It may show current
status, build evidence, model selection state, and logs, but central workflows
belong in proper GUI windows:

- Project and package controls belong in Project/Package workspaces or modals.
- System Info graphs belong in the System Info workspace. Shared `core.time` controls,
  timeline graphing, streaming-save cadence, and video-authoring controls belong
  in Video or the bottom scene timeline strip.
- Intelligence controls belong in the Intelligence workspace and Inspector, with compact
  status mirrored in the dock only when useful. The World Outliner may expose an
  `OS AI` tab with compact model/loop state, chat transcript, prompt entry,
  and plan controls because that keeps the selected AI model attached to normal editor chrome
  instead of hiding control in a separate floating window.
- Scripting needs a real code/text editor surface, not a Console Dock submenu.

Current bottom-dock non-output tabs should use compact status-only text inside
`scroll_text_panel`; only `Output` owns selectable log text. Do not reintroduce
property-row blocks, buttons, dropdowns, selection tables, or progress widgets
into Project, Assets, AI, or Systems dock pages; those controls belong in
central workspaces, modal windows, or Inspector-owned panels.

Docking defaults to selected-backend multi-pane composition. A DirectX editor
creates DirectX scene panes, an OpenGL editor creates OpenGL scene panes, and
mixed-backend grids are reserved for explicit diagnostics or accurate-preview
comparison. GUI primitives must therefore stay backend-neutral while the editor
host owns which renderer family a pane belongs to.

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
loads the active `.ascript.cpp` through the shared `engine.gui` source-editor
primitive, not an editor-local clipboard hack. The primitive owns scrollable
multiline editing, click-to-caret placement, drag ranged selection, Ctrl+A/C/X/V,
Left/Right/Home/End navigation, and right-click Select All/Copy/Cut/Paste.
Save/Reload remain editor evidence actions because they touch project files.
Promotion to a real code editor still requires syntax-aware display, line/column
status, search, undo/redo, and a cleaner split between preview, editor, and build
actions.

## Script Editor And Clipboard Gate

The current script surface is not a finished editor. It can locate and edit
script source through the shared source-editor primitive, but a production
scripting workspace still needs syntax-aware code text, line numbers, search,
undo/redo, save/reload evidence, build/run feedback, and predictable keyboard
focus across all docked contexts. Built-in script assets should use ASCII-safe
source headers until the text renderer supports the full banner glyph set; any
remaining high-byte source preview normalization must not corrupt the saved
source.

Script editing should use shared GUI text/source-editor primitives, not a
one-off asset panel hack. The acceptance gate is a script file that can be
opened from Assets or the Script Editor workspace, edited, selected, saved,
reloaded, copied/pasted from a right-click context menu, built, and run with
visible evidence and no Console Dock-only control path.

Window chrome follows the same rule: close buttons, titlebar controls, context
menus, scroll areas, text inputs, and future tabs/splitters belong in
`engine.gui` first. Editor domains compose those primitives and should not draw
their own ad hoc copies.

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
