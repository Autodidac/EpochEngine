# GUI Library Architecture

Epoch GUI is an engine-internal library surface. It is not a one-off editor
overlay, and it is not the Console Dock. The editor, launcher, project hub,
systems view, AI sandbox, package manager, scripting view, and future popout
windows should compose shared GUI primitives from the same layer.

## Current Boundary

- Public module API: `Engine/modules/gui.engine.ixx`
- Current implementation: `Engine/src/epochgui/gui.engine.cpp`
- Primary consumer: `Engine/src/editor/editor.application.cpp`
- Backend replay consumers: renderer/context code that drains deferred GUI
  batches after scene rendering

The current implementation is still physically compact, but the ownership rule
is already library-like: reusable controls are added to `gui.engine` first, then
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

When a top menu is open, normal toolbar and pane controls behind it must be
input-muted until the top-layer menu body is rendered. It is not enough to draw
the menu above the scene; click ownership must also prevent toolbar tabs,
context selectors, scene widgets, and dock controls from consuming the same
press/release before the menu item sees it.

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
  scoped theme application belong to `gui.engine`; editor surfaces may store the
  selected preference but must not recreate theme tables in domain code. Current
  exposed choices are `System Light/Dark`, `Light`, and `Dark`. System mode must
  resolve through platform app-theme preference when available and fall back to
  the dark palette on platforms without an OS light/dark bridge. More branded
  Epoch themes such as a future professional dark style are palette extensions,
  not replacements for the three basic choices.
- Cross-backend visual parity starts in `engine.visuals`. Frame clears, scene
  clears, object colors, selection colors, look markers, and editor-only opacity
  factors are shared there so OpenGL, Vulkan, DirectX, Raylib, SDL, and SFML can
  converge on one visual profile. Future GUI theme tables and graph palettes
  should read from that same spine instead of duplicating constants per context.
- Editor composition: scene/game, assets, project/build, systems, AI sandbox,
  scripting, and package manager workspaces choose domain data and layout, but
  do not own generic widget behavior.

## New Control Rule

Every new editor control starts as a reusable GUI primitive unless it is truly
domain-specific. Dropdown/select-box state and segmented-selection geometry
belong in EpochGui controllers; the local AI model selector, scene-mode switch,
package selection, backend selection, project settings, asset selection, and
script selection should reuse those paths instead of adding one-off controls.

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
  Console Dock or domain code when `gui.engine` can own the behavior
- loading screens are shared GUI primitives too. `EpochGui` owns the
  backend-neutral `LoadingScreenLayout`; `gui.engine` owns drawing, input
  capture, and theme integration; launcher/editor/update domains provide only
  title, message, progress, status, and action text. Loading surfaces are valid
  for mode handoff, updater handoff, package/cache work, and project build
  waits, but they are not a replacement for the launcher command grid or the
  editor command menu.
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
- AI controls belong in the AI workspace and Inspector, with compact
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

Desktop editor/tool builds may opt into native routed pane popouts. A valid
popout starts from real pane chrome with a title-bar drag/release gesture,
clones the current editor pane state into a detached context route, captures its
own input, hides the docked source pane while the route is open, restores that
source pane on Dock Back, Close, or native-window close, and keeps GUI overlay
priority active in that routed context. The routed context must refresh GUI/font
resource upload state before its first panel frame so cloned panes do not inherit
stale atlas bindings. The Window menu manages pane visibility and layout reset;
it is not the primary pane-detach surface. Games,
mobile apps, console targets, and headless tools must be able to omit the native
floating/detached host while still linking the portable GUI primitives.

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
must be normalized before display or explicit session capture, and hidden reasoning text
must never be promoted to chat output.

## Script Editing Gate

The Assets workspace owns the first visible Script Source Editor surface. It
loads the active `.ascript.cpp` through the shared `gui.engine` source-editor
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
`gui.engine` first. Editor domains compose those primitives and should not draw
their own ad hoc copies.

## Safe Split Plan

Do not split files only for aesthetics. The safe code split is:

1. Keep `gui.engine` as the public module name.
2. Move implementation chunks into owned source files only when CMake, MSVC
   project files, and filters are updated together.
3. Prefer slices that match responsibility: primitives, layout/docking, text,
   runtime surfaces, theme/rendering, and backend replay.
4. Build MSBuild Debug/Release and the active CMake preset before marking the
   split complete.

`EpochGui` is now a real linkable static-library target for backend-neutral
layout primitives under `Engine/include/gui` and `Engine/src/epochgui`, with
standalone mirror metadata in `Engine/dep/EpochGui` for `Autodidac/EpochGui`.
Keep `gui.engine` as the engine module/API adapter around rendering, input,
theme, text, and atlas/backend replay; keep portable math/control state in
`EpochGui` first.

The current reusable payload includes floating-window layout, popup/dropdown
layout, dock-layout math, dockable-window host/action state, splitters,
progress-bar layout, selectable-list row math, segmented-selection geometry,
rounded-rectangle mesh/style policy, toggle-switch layout, and portable
text-control state. `SelectionControlController` owns clamped segment sizing,
gap-aware item placement, aggregate bounds, toggle geometry, and hit testing;
`gui.engine` supplies rendering, cached rounded control corners, theme, font,
focus, and translated input.
`TextControlController` provides UTF-8-safe caret boundaries, anchor/range
selection, line/document/word/multiline navigation, edit and clipboard intent,
read-only and maximum-byte policy, and metric-driven scrolling. The adapter
still owns native clipboard calls, glyph measurement, wrapping, rendering, and
input-event translation. The editor exposes rounded controls as an opt-in
Settings toggle, disabled by default and preserved during context snapshot
handoff. The current production editor route uses real pane title bars for
detach requests; the baked primary renderer surface is never detachable, while
secondary routed panes retain native popout/redock. Old generic Floating GUI
proof routes are infrastructure only. The next safe conversion batch is modal
sizing/action rows, closable panels with scroll bodies, text adapter integration,
and Package Manager action rows before touching top-layer menu composition.

Floating/native GUI hosts are optional integration features, not required
`EpochGui` payload. Games, mobile apps, console targets, headless tools, and
other constrained products can link only the backend-neutral layout/state
library and omit detached-window routes, desktop docking chrome, and editor
panel hosts entirely. The engine/editor adapter owns route ids such as
`floating.gui` and any native window/context driver needed to present them; the
static library must remain usable when those host capabilities are absent.

## Portable Library Contract

`EpochGui` is the reusable library, not the editor shell. Its public payload is
allowed to know about rectangles, focus ids, layout controllers, theme-neutral
state, window modes, dock slots, scroll offsets, selected rows, text edit state,
and user-intent actions. It must not require an engine runtime, a renderer
backend, a Win32/X11 native window, editor project state, package manager state,
AI tooling, or a particular input system in order to compile.

This split gives future products a clear choice:

| Target kind | May link `EpochGui` | May use `gui.engine` adapter | May include native floating hosts | Default expectation |
| --- | --- | --- | --- | --- |
| Epoch desktop editor | Yes | Yes | Yes | Full panes, routed popouts, modal/top-layer replay, context handoff |
| Desktop tool/software app | Yes | Usually | Optional | App chooses whether popouts/docking are worth the platform cost |
| Game runtime | Yes | Optional | Usually no | In-game HUD/menu primitives without editor panels or detached windows |
| Mobile app/game | Yes | Optional, platform-gated | No by default | Single-surface touch UI, no desktop window assumptions |
| Console game/app | Yes | Optional, platform-gated | No | Controller-safe menus, no mouse/window chrome dependency |
| Headless/server/test | Usually no, but buildable | No | No | CLI/log/test evidence only |

Portable GUI code should therefore expose intent rather than perform host work.
For example, a dockable-window controller can report `dock_requested`,
`float_requested`, `detach_requested`, `focus_requested`, and `close_requested`.
The editor adapter decides whether `detach_requested` maps to a native top-level
window, an in-app floating panel, a no-op with status text, or a hidden build
flag in a product that does not support it.

## Host Capability Tiers

There are three different concepts that must not be collapsed into one:

1. **Portable control state**: layout, focus, hit-test decisions, scroll state,
   selected rows, text edit state, dock/float action state, and reusable
   controller classes. This belongs in `EpochGui`.
2. **Engine GUI adapter**: renderer submission, font atlas access, input event
   translation, theme tables, deferred GUI batches, top-layer replay, and editor
   bridge functions. This belongs in `gui.engine`.
3. **Native/application host**: OS windows, parented backend child panes,
   detached routed contexts, redock/undock window movement, app lifecycle,
   platform permission checks, and product-specific inclusion flags. This
   belongs in the editor/runtime host layer, not in `EpochGui`.

When adding a GUI feature, first decide which tier owns each piece. A context
menu's open/close/focus state can be portable. A context menu's glyph rendering
and clipping are adapter work. A context menu that escapes into a separate
desktop window is native-host work and must be optional.

## Optional Feature Flags And Build Shape

The source should keep preparing for these compile-time or target-profile
boundaries even before all flags exist:

- `EpochGui` core: always free of renderer and OS-window dependencies.
- `gui.engine` desktop adapter: enabled for editor/tool builds with renderer
  replay and font atlas ownership.
- Editor dock host: enabled for desktop editor shells that need docked panes,
  splitters, modals, menus, and inspector/workspace windows.
- Detached/native route host: enabled only when the platform/app owns native
  top-level windows and lifecycle evidence. `floating.gui` is one such route.
- Product HUD/menu profile: enabled for games and apps that need buttons,
  lists, tabs, progress, sliders, text entry, and modals but not editor panes.
- Headless profile: may compile shared data structures for tests, but does not
  initialize renderer/GUI host state.

Do not make a game, mobile app, console app, or generated software target pay
for desktop editor windowing just because it uses buttons or tabs. Conversely,
do not make the editor reimplement controls just because a product profile may
exclude the host that presents them.

## Current Floating Panel And Route Contract

World Outliner, Inspector, Console Dock, and AI Chat popouts start from their
real pane title bars with a drag/release gesture. They use explicit pane route
ids and a cloned editor snapshot so the new context presents one current pane
rather than a second editor shell. A successful route marks that pane detached
inside the editor layout, removes the source pane from the docked window, and
restores it when the routed pane uses Dock Back, Close, or native window close.
`Window` menu entries are reserved for show/hide, recovery docking, and layout
reset so menu clicks do not stand in for the primary docking gesture.

The first named optional native GUI route remains `floating.gui`, but it is
infrastructure for future low-level GUI host tests, not the current menu path,
not a second editor shell, and not the context-selection UI. Its contract is:

- The engine host may map `floating.gui` to an "Epoch Floating GUI" title and
  route-specific default size when a desktop product enables native routed
  panels.
- The `WindowData::guiRoute` field tells the session loop to run
  `editor_run_context_panel(ctx, route)` instead of the normal editor surface.
- `editor_run_context_panel` branches on the route id and draws compact panel
  content for concrete pane ids. `floating.gui` remains a low-level host proof,
  not the user-facing pane-popout route.
- A native route captures input inside its own native/context window. It must
  not steal clicks from the original editor window, leave the docked source pane
  visible behind it, or pretend to redock before a real host move exists. The
  safe redock path is close-and-restore until drag/drop host redocking is owned
  by the native application tier.
- The session loop refreshes GUI/font upload state for the routed context before
  its first routed panel frame. Font corruption after spawning a pane means the
  route did not receive an isolated atlas upload and must fail validation.
- Backend selection remains in Editor Settings. A floating GUI
  panel or route may display the active renderer as evidence, but it does not
  own backend switching.

Additional optional routes should follow the same visible status path. Good
candidate names are concrete panel ids such as `asset.browser`, `code.editor`,
`ai.visualizer`, and `build.output`. Avoid names that imply broad engine
authority, hidden automation, or a whole duplicate editor.

## Conversion Checklist For New GUI Work

Before a new GUI surface is considered production progress, record the answers:

- Which pieces belong in `EpochGui`, `gui.engine`, editor domain code, and
  native/application host code?
- Can a game or mobile target use the reusable controls without linking the
  desktop popout host?
- What happens when detach/popout is unsupported: hidden option, disabled
  command, in-app panel fallback, or status log?
- Does the control capture input only inside its drawn bounds?
- Does it use existing font/theme/atlas/replay paths?
- Does it keep OpenGL top-layer replay order unchanged unless the task is
  explicitly a draw-model change?
- Does it provide a useful status/error path rather than silently doing nothing?
- Are CMake, MSVC projects, filters, and standalone `Autodidac/EpochGui`
  metadata updated when the reusable library grows?
- Did Debug and Release builds pass before the changelog claims the feature?

This checklist is intentionally stricter than a visual mockup. Epoch should
prefer one real control with correct ownership over three fake panels that only
work in the current editor frame.
