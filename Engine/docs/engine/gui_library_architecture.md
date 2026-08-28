# GUI Library Architecture

Epoch GUI is an engine-internal library surface. It is not a one-off editor
overlay, and it is not the Console Dock. The editor, launcher, project hub,
systems view, AI sandbox, package manager, scripting view, and future popout
windows should compose shared GUI primitives from the same layer.

## Current Boundary

- Portable static-library API: `Engine/dep/EpochGui/include/gui/`
- Portable implementation: `Engine/dep/EpochGui/src/epochgui/`
- Engine adapter API/implementation: `Engine/modules/gui.engine.ixx` and
  `Engine/src/epochgui/gui.engine.cpp`
- Primary editor consumer: `Engine/src/editor/editor.application.cpp`
- Backend replay consumers: renderer/context code that drains deferred GUI
  batches after scene rendering

Source `v0.89.28` documents the current portable/controller composition. The
published `v0.89.06` packaged runtime and updater remain sealed. Source and
contract integration below does not claim GUI screenshots, responsiveness, or
operator eye proof.

The current implementation is still physically compact, but the ownership rule
is already library-like: reusable controls are added to `gui.engine` first, then
editor domains consume them. Editor workspaces should not reimplement generic
buttons, tabs, dropdowns, scroll areas, text inputs, modal chrome, or clipping
logic.

Application workspaces use real tab ownership rather than toolbar mode buttons.
The shared workspace-layout contract assigns document, structure, inspector,
and operation-dock providers separately for Standard Editor, Plant Lab, and GUI
Editor. The primary renderer view is externally supplied by the engine context;
the GUI library does not create or own a renderer. Standard Editor and the
projectless launcher GUI Editor consume the same canonical GUI document and
projection adapter. Standard Editor presents the interactive placement canvas;
GUI Editor presents widget creation, Canvas, Runtime Preview, Component Graph,
Styles, verified `.epochgui` I/O, and reusable templates. Both share stable
selection with Hierarchy and Properties.

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

## Adaptive Professional Chrome

The primary editor command row now composes one shared three-zone chrome
contract rather than positioning menus, run controls, update controls, and one
unbounded status sentence independently. `gui_lib::make_chrome_bar_layout`
owns deterministic left, centered-command, and right-status planning from
preferred widths, compact widths, pinning, priority, and explicit overflow.
It returns exact item and overflow-button bounds without backend or editor
dependencies. Invalid or non-finite bounds fail closed.

The `gui.engine::chrome_bar` adapter measures labels with the active EpochGui
font, draws menu/command controls and read-only status chips, and maps overflow
selection back to the original stable item index. The editor supplies only real
application state: application identity, command-menu state, admitted run and
update actions, engine version/build tag, live and hardware thread counts,
active renderer context, and preview zoom. The chrome does not synthesize FPS,
GPU, memory, build, or validation evidence.

The standard editor uses a 32-pixel command row and a 36-pixel responsive
document row inside the existing fixed toolbar allocation. Menus, commands, and
status fields compact or move into explicit per-zone overflow while the primary
run/update commands remain pinned by policy. The existing responsive workspace
tab strip still owns document selection and overflow. This is a composition
change only: backend frame order, queue drains, scene drawing, and top-layer
menu replay remain unchanged.

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
  selected preference but must not recreate theme tables in domain code. The
  baseline choices are `System Light/Dark`, `Light`, and `Dark`; named Epoch
  palettes currently include Classic Launcher, Midnight Blue, Ember Forge,
  Forest Terminal, and Aurora Steel. System mode resolves through platform
  app-theme preference when available and falls back to the dark palette on
  platforms without an OS light/dark bridge. Light mode is a real high-luminance
  palette with a dark glyph atlas, not a recolored dark surface. Rounded controls
  default on; new palettes extend this table rather than replacing the baseline.
- Cross-backend visual parity starts in `engine.visuals`. Frame clears, scene
  clears, object colors, selection colors, look markers, and editor-only opacity
  factors are shared there so OpenGL, Vulkan, DirectX, Raylib, SDL, and SFML can
  converge on one visual profile. Future GUI theme tables and graph palettes
  should read from that same spine instead of duplicating constants per context.
- Editor composition: scene/game, assets, project/build, systems, AI sandbox,
  scripting, and package manager workspaces choose domain data and layout, but
  do not own generic widget behavior.

## Image, Tab, And Graph Controls

EpochGui now owns portable geometry for image boxes and tab buttons. Image boxes
take stable image identity, source extent, contain/stretch fit, selection,
enabled/interactive state, and optional caption; the engine adapter resolves the
runtime sprite and draws it. Image buttons require an explicit stable identity
through the same contract; the legacy overload remains compatibility-only. Tab
bars take stable IDs, active/disabled/dirty/closeable state, accept explicit
geometry when the application requires it, and otherwise derive readable tool-tab
width from EpochGui font metrics and close/dirty affordances. They return
selection/close intent without owning editor documents.

Active tool tabs use the active palette fill; the close affordance changes its
own hover color without adding an underline that can be mistaken for selection.
Rounded controls are the editor default and remain a user preference. Static
palette sprites are packed with an edge-clamped gutter and expose only the inner
atlas rectangle, preventing linear filtering from sampling neighboring rounded
button or modal pixels.

`gui_lib::AssetGrid` owns bounded filtering, virtualized tile layout, stable
selection and activation, scroll clamping, optional image metadata, and explicit
invalid/duplicate identity rejection. It returns visible stable IDs for
virtualization without owning thumbnail lifetime. The central Asset Manager
matches each browser entry to canonical texture source identity and hydrates one
bounded catalog atlas before drawing the grid, so a first-visible card does not
depend on a previous frame's visibility result. Decoded thumbnails, atlas
entries, and renderer handles remain disposable projection state.

`gui.engine::register_runtime_surface_atlas` accepts a bounded batch of stable
runtime-surface descriptors, validates the whole request before mutation, updates
the shared physical atlas under one lock, and uploads once after all accepted
entries are packed. The Assets adapter uses this path for project thumbnails and
retains only aligned `SpriteHandle` projections; neither EpochGui nor the atlas
owns canonical asset identity or source bytes. Its build-safe contract proves
stable handles for unchanged registration and same-size pixel replacement.

`gui_lib::node_graph_workspace::Controller` is the reusable graph interaction
surface. It transactionally accepts bounded stable node, pin, and edge layouts;
owns pan, anchored zoom, fit, projection, hit testing, selection, keyboard
navigation, and pointer intents; and never imports engine scheduler or authoring
state. The engine `node_graph_canvas` adapter preserves view and node offsets and
returns drag, connect, disconnect, and selection intent. `editor.systems_panel`
maps those intents into `authoring.task_graph`; GUI Editor maps parent-output to
child-input connections into validated temporal widget reparenting and maps
disconnect back to the document root. Domain documents remain mutation
authorities, so GUI intent cannot rewrite scheduler or document internals.

`authoring.gui_document` is the typed temporal meaning behind GUI authoring. It
also owns the shared semantic template factory for Blank Canvas, Desktop App,
Dashboard, Mobile App, and Game HUD documents; editor application choices and
project materialization call the same factory. Generated 2D projects declare
`Assets/Gui/main.epochgui` in their manifest, preserve existing valid source,
and create the shared Game HUD only when canonical source is absent. Source is
compiled and persisted through `project.gui_library`, then restored through
`project.gui_runtime` before generated child acceptance may continue.

The reusable GUI boundary also requires:
- global editor Delete, Escape, and history shortcuts query
  `gui::keyboard_input_captured()` so active text/select controls retain keyboard
  ownership rather than triggering scene or document commands
- native hosts forward printable Unicode text separately from editing keys;
  Backspace, Delete, Enter, Escape, and other control codepoints must not arrive
  twice as both a key command and inserted text
- one drag gesture has one stable widget owner until release; sibling widgets may
  not cancel or steal a pending/active canvas drag while the document is iterated
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
- AI authoring and bounded-development controls belong in the movable AI Controls
  tool, with compact status mirrored in the dock only when useful. The World
  Outliner may expose an `OS AI` tab with compact model/loop state, chat
  transcript, prompt entry, and plan controls because that keeps the selected
  AI model attached to normal editor chrome instead of hiding control in a main
  document. AI Chat remains a separate dockable conversation tool so the scene
  can stay visible. Reusable console windows own clipped transcript, input, and
  optional pinned/footer action rows; approval-sensitive actions stay pinned
  above scrollable content so compact docks cannot hide authority controls.
  Text rows accept optional semantic roles so user, assistant, system, and error
  messages can use restrained contrasting backgrounds without editor-side drawing.
  Font line metrics reserve raster/descender padding, and focused text controls
  own a visible high-contrast caret across chat, script, and ordinary edit fields.
  Partially clipped glyphs are omitted rather than destination-scaled because
  atlas UVs remain immutable during GUI clipping.
- Scripting needs a real code/text editor surface, not a Console Dock submenu.

The single `Output` tool owns selectable log text and compact Project, Assets,
AI Output, and Systems evidence filters. These status categories are not
independent dock routes. Do not reintroduce property-row blocks, action buttons,
selection tables, or progress widgets into Output filters; those controls
belong in central workspaces, modal windows, or Inspector-owned panels.

Docking defaults to in-host tool-tab stacks over one live editor projection.
World Outliner, Asset Browser, GUI Hierarchy, Script Browser, Tile Map,
Properties, World Settings, Output, and AI Chat each own an independent tool
route. They may move between the left, right, Bottom Left, and Bottom Right
groups without entering
the main document strip or creating another editor, renderer, project session,
or native context. Tabs retain stable pane identity, focus, selection, document
bindings, and history while the host changes only presentation. Output filter
selection persists with the editor context but never changes pane identity.
One EpochGui-owned guide overlay and placement ghost make four logical tab
stacks, two physical-context destinations, and native float drops visible before
release. Bottom Left and Bottom Right are ordinary tab destinations. The
upper-left and upper-right context targets preserve a physical context on the
selected side instead of retiring it into a logical tab stack. Guides use
translucent orange
idle and stronger orange hover states. Drawing and hit testing consume the same
`DockGuideLayout`; the editor must never draw a competing guide set. Press identity is part of the
closable tab result, so selection commits before movement and the exact pressed route enters drag.
Labels reserve close space once and use readable professional widths. The
reusable workspace state validates group category, destination activation,
source fallback, close fallback, and movement; document and scene tabs are not
legal tool-dock payloads.

Persistent control identity is scoped by GUI host, owning window, and stable
control ID. Two windows may intentionally reuse a local ID without sharing
scroll position, drag capture, popup state, or selection. Scroll-area frames also
record their owner and nesting depth, unwind at the owning window boundary, and
draw their track and thumb inside the area viewport clip. The editor supplies
window identity and content; it must not implement or globalize reusable
scrollbar state.

Reusable console windows own their complete vertical composition: transcript,
input row, command button, and any optional footer actions are measured and
clipped inside the console rectangle. Hosts provide action descriptions and
consume the selected action; they must not reserve a second height or draw an
ad hoc footer outside the console. This invariant applies equally in side,
bottom, floating, and context-backed pane hosts.

Scene and game panes remain associated with the editor's selected renderer. A
DirectX editor creates DirectX scene panes, an OpenGL editor creates OpenGL
scene panes, and mixed-backend grids are reserved for explicit diagnostics or
accurate-preview comparison. That renderer association does not turn every
ordinary GUI pane into a renderer context. GUI primitives stay backend-neutral;
the editor host decides whether a particular view needs a render surface.

Float is an ordinary presentation route for a pane, not a second promotion mode.
A desktop editor/tool may route one stable pane identity to an in-app floating
panel, another same-process application window, or the existing context-backed
native host. The engine adapter supplies native renderer/context integration;
EpochGui owns pane identity, guide targets, placement intent, and tab-group
return. Closing, hiding, or redocking must return the same logical pane without
logging out of the editor, replacing active-editor authority, or cloning
canonical state.

The current editor keeps its main World/GUI/Forest/Plant/Timeline/Project/
Assets/AI/Systems strip document-only. Tool routes use the Left, Right, Bottom
Left, and Bottom Right tab groups. Output and AI Chat default to the two bottom
groups, but neither route nor group is special after initialization.

Document and reviewed-source tab strips use EpochGui's responsive strip
planner. The planner measures requested tab widths against the host content
width, keeps the active route visible whenever one full tab plus the overflow
control can fit, and moves the remaining stable routes into a bounded selectable
overflow list. The same portable contract owns previous/next/first/last route
navigation, skips disabled routes, and makes wrapping explicit. The engine
adapter owns rendering and input for that layout; the primary workspace strip
opts into Ctrl+Tab and Ctrl+Shift+Tab traversal while other strips remain
unchanged by default. Editor workspaces only map returned indices to semantic
routes. No context may solve clipping with editor-local label truncation, magic
last-item widths, or an unreachable off-window tab. Reusable changes in the
bundled tree require a linked, verified checkpoint in the standalone EpochGui
Epoch Site dependency authority before or with the Engine dependency update.

Dragging a tool to Float starts its existing context-backed native host;
dragging that native titlebar over the primary host publishes all four logical
tab targets,
upper-left and upper-right physical-context targets, and a matching placement
ghost. A logical target retires the pane-owned native context and restores the
same pane as an in-host tool tab. A context target preserves and grid-docks the
physical context on the selected side. EpochGui's `DockGuideLayout`
owns the visible guide rectangles, pointer hit testing, hovered target, and
placement preview for every routed pane; engine hosts consume that result and
must not infer separate broad screen regions. Native drag projection supplies
the live native cursor so every visible guide remains selectable. Releasing on
a logical target or closing the native window returns the same route to its
remembered group.

Guide coordinate space follows the dragged surface. An ordinary tool-pane
popout is a routed pane, so its guide overlay remains local to that pane's
context. A detached full renderer context is not a routed pane: EpochGui derives
its left/right targets from the destination preview rectangles, centers each
guide inside the actual destination, and returns the same preview as the hover
ghost. The engine host supplies parent-space context bounds and presents that
model in the canonical parent window. The detached renderer must not draw a
second local guide set. Native window creation, reparenting, backend owner-thread
commands, and renderer frame order remain engine/backend responsibilities and
are not changed by guide projection.

Floating content contains no Dock Back or Close Window command buttons.
Routed-pane garbage collection is deterministic: close/redock detaches the pane
route from its host. Context retirement, completed-thread joining, command-queue
clearing, and native GL/DC/window release occur only when that route owns a
dedicated context; reusing an existing context never retires the host. Full
editor shutdown also resets route maps, the shared detached-pane projection,
and passive-context scoring. Epoch defines no built-in Secondary Map or fixed
Display 2. Application-defined map, radar, and telemetry panes use this same
contract. Native interaction still requires operator eye proof, and automatic
monitor placement remains unfinished.
Games, mobile apps, console targets, and headless tools may omit floating,
detached, and context hosts while linking the portable GUI primitives.

## Pane Hosts And Application-Defined Outputs

A pane has one stable logical identity and one active presentation route. The
host resolves requests in this order:

1. an in-host tab stack, which is the default;
2. an optional in-app floating panel;
3. an optional same-process or native additional window;
4. an explicit renderer/context host when the pane's declared capabilities
   require it.

Application-defined surfaces such as `Game`, `Map`, radar, or telemetry are
ordinary pane identities and may be assigned to a compatible tab stack or
floated through the existing host route. Their application owns visible
show/hide/focus controls. Hiding one suspends or removes its presentation work
according to policy but does not discard the underlying document, selection,
temporal state, or project session.

Desktop fullscreen policy may use either one same-process borderless host over
selected display work areas or multiple native windows, depending on monitor
geometry, DPI, platform support, renderer support, and product capability tier.
EpochGui supplies portable pane/output identity and route intent; the engine
host owns monitor enumeration, window placement, native lifecycle, fullscreen,
focus, and presentation throttling. Non-desktop targets may omit this entire
host layer. This section is an architectural contract and does not claim that
multi-monitor routing or arbitrary tab-stack docking is implemented.

## Workspace Command Presentation

EpochGui owns the reusable enabled/disabled control presentation and input
routing. It does not decide which editor document a global action may mutate.
The engine-owned `editor.workspace_commands` catalog resolves that authority
from the active surface and exposes an explicit rejection reason for unsupported
or stale commands. This separation lets Edit menus, shortcuts, outliners, and
future command palettes share one visual vocabulary without allowing a Project,
Assets, AI Development, Systems, or other non-World surface to mutate World by
fallback.

## System Workspace Projection

EpochGui's `SystemWorkspace` is a portable data/controller primitive, not an
engine registry or graph executor. It owns:

- bounded transactional row replacement through an adapter or owning rows;
- hierarchy, expansion, filtering, category/status filters, and stable sorting;
- selection, keyboard navigation, summaries, and selected-row lookup;
- source/view revisions and explicit empty, error, and stale-data states;
- deterministic bounds for rows, depth, text, filters, and categories.

It imports no engine systems, scheduler, authoring graph, renderer, windowing, or
project state. `editor.systems_workspace` converts immutable registry, shared
editor scheduler, and learning-document snapshots into rows. `gui.engine` draws
those rows and graph surfaces, routes input, and owns clipping/theme/font
behavior.

The adapter caches by source/view revision and keeps bounded scheduler rows and
timing history. It does not rebuild graph projection or duplicate owning data
every frame when source revision, viewport, filter, and selection are unchanged.
Live diagnostics sampling is an engine lifetime decision: enable registry timing
only while the central Systems workspace is active and disable it when hidden.
Task scheduler timestamps remain lifecycle evidence independent of tab
visibility. EpochGui only projects supplied snapshots.

The read-only Live Scheduler reports the real shared TaskGraph used by AI
evidence builds, selected script builds, project builds, and the approved tool
harness. The separately labeled Learning Graph owns editable semantic
add/connect/remove, undo/redo, validation, topology, critical-path, and
parallel-wave simulation. Learning actions cannot schedule, cancel, reorder, or
execute live work. The Time view presents bounded registry-update samples and
live scheduler queue/run evidence. Fixed-size runtime chart surfaces update
existing atlas pixels in place, preserving their sprite handle and preventing
per-sample atlas growth.

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

## Script Editing And Build Gate

The Assets/Scripting tool owns the visible `.ascript.cpp` source surface through
shared EpochGui text-editor state and the `gui.engine` adapter. The reusable
control owns multiline indexing, caret/range selection, focus, scrolling,
clipboard intent, navigation, dirty revision, find/replace, and save
acknowledgement. Native clipboard, glyph measurement, syntax presentation,
diagnostics, file I/O, and build execution remain adapter/domain work.

Build Selected Script is a real C++23 shared-library compilation request routed
through the shared editor TaskGraph. The editor saves dirty source first and
starts at most one build. The compiler records source/current-output evidence,
rejects ownership changes during compilation, verifies the bounded candidate,
publishes it by same-filesystem atomic replacement, and verifies the published
artifact before success. The surface reports source, output target, generation,
progress state, completion, and actionable failure; the loader resolves that
same canonical output path.

A selected-script build is not Project Build and cannot authorize external Run.
The strict `project.lifecycle` contract defines selected-project, committed
scene, materialized-shell, build-input, verified-artifact, and runtime
generations. The production editor still uses its legacy evidence adapter and
must populate those stamps before the GUI can claim generation-safe Build/Run.

The source editor is still not a finished IDE. Production acceptance needs
syntax-aware display, line/column status, search UI, undo/redo, document tabs,
large-file virtualization, compile/load diagnostics, predictable focus across
docked contexts, and visible external project-run evidence. Controller or build
contracts alone do not prove those GUI behaviors.

The editor now places the selected script in the central Assets/Scripts
workspace with Save, Reload, Build, and Copy Path commands above the full-width
source control. Outliner Scripts remains a compact navigator. The reusable
asset grid owns bounded per-context/window popup state and returns stable target
plus action identity; the engine maps Open, Show Details, and Copy Path to real
project behavior.

Window chrome follows the same ownership rule: close controls, title bars,
context menus, scroll areas, text inputs, tabs, and splitters belong in EpochGui
or `gui.engine` according to the portable/adapter boundary; editor domains
compose them rather than drawing ad hoc copies.
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
rounded-rectangle mesh/style policy, toggle-switch layout, portable text-control
state, and the bounded `SystemWorkspace` row/filter/sort/hierarchy/selection
controller. `SelectionControlController` owns clamped segment sizing,
gap-aware item placement, aggregate bounds, toggle geometry, and hit testing;
`gui.engine` supplies rendering, cached rounded control corners, theme, font,
focus, and translated input.
Rounded atlas replay overlaps adjacent corner/edge slices by at most one
physical pixel so fractional logical-pixel and DPI scaling cannot expose
transparent seams at hover or pressed-state boundaries.
`TextControlController` provides UTF-8-safe caret boundaries, anchor/range
selection, line/document/word/multiline navigation, edit and clipboard intent,
read-only and maximum-byte policy, and metric-driven scrolling. The adapter
still owns native clipboard calls, glyph measurement, wrapping, rendering, and
input-event translation. The editor exposes rounded controls as the default
EpochGui style policy, with an explicit Settings toggle to disable them; the
preference is preserved during context snapshot handoff. The current production editor route uses real tool tabs and pane title bars for
placement requests; scene views and main document tabs are never ordinary
tool-dock payloads. Logical active-editor authority does not pin its physical
renderer context to a grid side. Exact tool routes retain native popout/redock,
with host guide projection and remembered-group restoration. Old generic Floating
GUI proof routes are infrastructure only. The next safe conversion batch is modal
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

Every non-scene tool owns an exact route and starts from its real tool tab or
pane chrome. The route carries pane identity; the engine publishes one bounded
live projection containing project identity, scene revision, entity rows,
selection, logs, and preview state. Detached panes read that projection and do
not create a second editor shell, scene document, or project session. Dragging
a tool tab shows Left, Right, Bottom Left, Bottom Right, and native-float guides;
releasing on a group joins that group while Float starts the context-backed
native route. A native titlebar drag additionally exposes upper-left and
upper-right physical-context guides. A physical-context target preserves and
grid-docks the context on that side; any logical target retires the detached
host and restores the route to the selected in-host stack.
Both paths use the same EpochGui layout for drawing and hit testing, and
native close restores the route to its remembered group. The main document strip and
scene views reject tool routes. The `Window` menu remains available for exact
show/hide recovery, named secondary-output control, and layout reset.

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
  not steal clicks from the original editor window or leave the docked source
  pane visible behind it. Native chrome owns movement and close; routed content
  contains no Dock Back or Close Window commands. Tool-tab and native-titlebar
  drags use visible guide zones and placement ghosts against canonical pane
  state. Standard Editor, Plant Lab, and GUI Editor each persist pane visibility,
  Left/Right/Bottom Left/Bottom Right placement, active tabs, the bottom-column
  split ratio, theme choice, and rounded-control preference in versioned per-user
  configuration outside project and release data. Output and AI Chat initially
  occupy Bottom Left and Bottom Right respectively, but both are ordinary routes;
  an empty bottom group collapses and the occupied group consumes the available
  width. Restore is schema-bounded; malformed files fall back to application
  defaults, and writes use same-filesystem atomic replacement. User
  tab reordering and arbitrary compatible stack creation remain follow-up work.
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
