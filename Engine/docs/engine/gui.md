# GUI Library Architecture

EpochGui is the reusable portable library. `gui.engine` adapts it to Engine
rendering, input, theme and native hosts. The editor, launcher, project hub,
systems view, Assistant, package manager, scripting view and popout windows
compose the same shared primitives.

Asset Generation keeps reference approval directly beneath the image preview,
outside the settings scroll when the image has its own column. Approving an
image does not generate a model or import anything into a project.

Forest plant sliders show their values and units. A drag changes a temporary
value; releasing it updates the plant once and adds one Undo step. Canceling a
drag leaves the plant unchanged. Standard ranges cover ordinary plants; Extended
ranges use the Forest system's existing limits. Loading a larger plant selects
the extended range without changing it. A refused preview can still be reduced
or undone. Source/CPU and native acceptance are recorded in the active pass.
Forest controls occupy a bounded scroll area beside the plant preview, or above
it in a narrow dock. Scrolling the controls must not consume the tree's viewport.
Zero branch levels means a trunk without branches. Branch length is independent
after it is edited; old saved plants retain their implicit geometry until then.

MSVC follows the same ownership as CMake: EpochGui compiles its portable module
interfaces and implementations once. Engine imports that project's module output;
Editor links the separately owned library. Do not add portable GUI compile items
to Engine's shared item list to work around a missing module reference.

The Engine adapter requires the rounded-rectangle module. Bundled MSVC builds
enable it by default, matching Engine CMake; an explicit incompatible override
fails early with the required setting. Standalone EpochGui can still disable the
optional feature. Debug/Release archive, compiler-import and final-link evidence
plus regression tests verify this boundary. This does not establish native
desktop responsiveness: the reported 120 FPS input/desktop stall remains open.

## October29 interface repairs — source and CPU evidence

Assistant code excerpts identify their file in each Before/After heading, using
the filename and repository-relative path from that request's admitted source
catalog. A later request cannot relabel an earlier event. Unresolved source IDs
remain visibly unresolved; raw tool arguments remain unchanged and available.
The displayed excerpt stays a snippet, rather than expanding to the whole file.

Code coloring follows source-byte tokens, independently of window width, visual
lines and frame count. The display scanner distinguishes C++ keywords, complete
identifiers, numeric literals, comments and quoted/raw strings. Number separators
such as `1'000` do not start strings; quotes inside comments do not recolor later
code. A malformed ordinary string stops at its source newline. Debug/Release
tests inspect actual colored glyph submissions in light/dark themes, wrapping,
scaling and growing transport prefixes. This is display classification, not a
C++ parser or evidence of native visual acceptance. Full integration and native
review are tracked in the active pass.

### Interface review checklist

Preserve the current dock layout and visual style. Review each workspace against
these concrete behaviors rather than replacing it with a different interface:

- Show what is selected, what is running and which project a command changes.
- Keep viewport status short. Put full errors, build output and paths in Output,
  with a direct action to reveal the relevant messages.
- Make every message in All discoverable in a named category. Search should
  work with category filters; Clear and Copy must state their scope.
- Menus stay usable while the pointer travels from their owner to a command.
  Outside click, Escape and focus loss dismiss them without activating content
  behind them. Tasks commands must open their real destination.
- Give the World Outliner a right-click Deselect command. Generated universe
  containers remain selectable in the list without blocking viewport picking.
- Show transport direction and speed; support forward/reverse, fast transport,
  pause, seek and endpoints through one time owner.
- Check authoring during playback, simulation and project Run separately.
  Pause/stop before unsafe script or scene replacement, explain refusals, and
  preserve safe navigation, input focus, cancellation and project identity.
- Keep readable theme contrast, labels that fit, consistent hit targets,
  keyboard navigation, scrolling, selection/copy and responsive resizing.

These are acceptance checks, not a statement that every workspace passes them.

Ordinary section headings use a readable version of the selected theme's own
accent. Objects, Create, Selection and Scripts no longer force informational
blue. Explicit success, warning and error headings keep their status meaning.
Heading fonts are prepared only for themes actually used, through the existing
bounded font atlas. Their accent sheets contain the built-in labels' Basic Latin
glyphs; other characters use the complete normal text font, including its
kerning, instead of turning into question marks. A missing optional heading
font also falls back to normal text.
No glyph recoloring or font baking occurs on each normal drawing frame.

Classic uses square raised and pressed controls, neutral gray faces, white
inset fields and navy captions. The visual reference is Irrlicht's documented
[Windows Classic skin](https://irrlicht.sourceforge.io/docu/namespaceirr_1_1gui.html),
not its metallic gradient skin. Epoch's numeric palette is an interpretation;
exact upstream color matching is not claimed. Existing theme IDs and the
System default are unchanged. Native aesthetic review remains open.

The generic scrollbar now uses the existing ordered pointer-drag owner. A
press, movement and release received within one GUI frame must still move the
thumb; a held drag continues outside the thumb's original bounds and releases
on its matching up or cancellation. Actual-owner Debug/Release cases pass.
The separately reported panel-divider fault came from idle runtime reconciliation
clearing the drag every frame. Full-screen exit is now idempotent. Matching full
Debug/Release contracts pass, and the operator confirms physical dividers work.

## Queued pointer ownership and theme styles — October28 source

A context that sends mouse-button events owns its button state through those
events. A slower platform sample must not cancel a held drag or create another
press after release. Focus/pointer cancellation clears held buttons; context
cleanup clears their producer identity. Callers using only sampled left-button
input keep that path. The existing per-context GUI state owns this distinction;
there is no second input manager. Build/native acceptance lives in the active pass.

The visible names are Codex Black and Classic Theme; existing enum names and
saved IDs remain compatible. Classic uses gray faces, raised borders that reverse
when pressed, inset white fields and navy captions with white caption text. Normal
control text stays dark. Choosing it does not rewrite the saved rounding preference
for other themes. Indigo Quartz uses saved ID16 and Signal Plum17; retired ID13
stays retired. New settings start with System Light/Dark; saved choices still load.

The jump-to-latest control uses the same circular button and hit test with an
opaque high-contrast face and inverse outline. Dark themes use a black face and
white arrow; light themes use a white face and dark arrow. Its drawing remains
in the existing top layer; background code must not show through the circle.

Physical divider acceptance is separate from CPU gesture tests. The existing
`--trace-menu-button0` option enables bounded native delivery, GUI admission and
divider decision evidence (120 interesting records per owner, no text or keys).
It changes no input, frame or overlay order. Default operation records none of it.

## Project keyboard input — October28, local source

Project GUI uses the existing `project.gui_runtime` session for widget focus and
the shared `gui.engine` text controller for caret, selection and text history.
Click a project control first; Tab then moves through its enabled, visible
controls and Shift+Tab moves back. Enter or Space activates a focused button.
Arrow keys adjust a focused slider. Plain Tab remains text in multiline fields;
Shift+Tab leaves the field and Ctrl+Tab retains dock-group navigation. A scene
without project GUI focus keeps its own keys.

The shared text owner handles multiline Tab from its key event; it does not
depend on a platform sending Tab as a character. A paired Tab character is
consumed once. Read-only fields and rejected edits keep their existing guards.
When an actual project GUI loads, the debug Project Controls panel starts hidden
behind a small Controls button. Show/Hide changes drawing and pointer capture
together. Projects without GUI, including the planet controls, start expanded.
This is a temporary view choice, not a saved project setting.

Text, Tab and later text are processed in their original order. Modal/popup
capture, focus loss, clipped controls and outside clicks retain their existing
input gates. Drawing an authoring canvas does not activate its project actions.
Controls that disappear release their current context's keyboard claim at the
existing end of frame; their text history and other contexts stay intact.

Focus changes and button activation publish their event before committing state.
If event storage fails, focus, action counts and pending events stay unchanged.
Actual-owner Debug/Release tests cover ordered input, hidden controls and payload
allocation failure. MSVC Debug iterator-bookkeeping allocation is a separate STL
limitation, explicitly excluded from the payload-failure fixture. Native acceptance
and the matching combined build are recorded in the active pass.

Tile Map uses the existing `image_box` rectangular control for cells, palette
items and map objects. Text-button minimum sizes must not enlarge a tile's draw
or click area. Item and viewport clips also apply to queued presses; the owned
workspace background covers the gaps. Global Undo/Redo routes to the Tile Map's
existing document journal while it is active. This does not close the complete
playable 2D or wider GUI Editor missions.

Tile collision belongs to a palette tile backed by a real project texture. The
Collision inspector explains that prerequisite before editing. Clicking a painted
cell selects its actual tile; an empty cell cannot toggle a palette-wide collider.
Stage the shape in the inspector, then Apply. That change affects every placement
of the selected tile. Texture-free cell colliders need a separate data/runtime
contract; the current control must not invent a fake texture to enable them.

Global Editor Undo/Redo uses the existing ordered GUI key events and their saved
modifiers, so a quick press and release is not lost between polling frames. It
claims no text focus and respects modal, menu and context ownership. Native
verification of this follow-up remains in the active pass.

## Tool-pane readability — October27, local source

Section headings now use the current theme's readable accent color and wrap to
the available width. Their measured height is shared with modal layout so the
next controls and footer do not overlap. Package titles use the existing menu
row's selected state and actual font height; summaries wrap underneath.

3D Objects keeps a compact Create / filter / More toolbar above its hierarchy.
Create and More open the existing command-menu component; object creation,
selection actions, Undo/Redo and Ambient Lighting remain available there.
The tree uses the remaining pane height, with indented rows and disclosure
arrows. Its root shows the project name, or Scene, without changing stored IDs.
Filtering, expansion, selection and generated-body access keep their existing
controller. These October30 changes have focused Debug/Release control proof;
matching Editor builds and narrow/wide native checks remain separate gates.

Asset Browser, GUI Hierarchy and Script Browser group related actions.
Buttons fit their labels, center as a group and wrap in narrow panes.
File and widget rows remain left aligned for scanning. Asset search filters the
visible list. Project, Timeline, Assets and GUI Overlay reuse these controls;
diagnostic prefixes are removed from ordinary property labels.

Dock/section tabs measure whole titles, close reservations and padding. Narrow
dock headers wrap complete draggable tabs and reserve the resulting height before
painting pane contents. Close glyphs sit in the upper-right corner without a
separate button background; hover/press feedback changes the glyph ink, while
paint and hit geometry share one owner. Floating caption/resize painting restores
its body clip afterward, and queued short drags use the existing pointer owner.
These controls have CPU/build proof; native acceptance remains separate.

Focused control/layout tests and the combined Release build/contracts pass.
Native normal/compressed acceptance remains in the active pass; this does not
close the wider GUI Editor, floating-window or 2D authoring missions.

## Command menus — October27, local source

`epoch.gui.command_menu` is a compiled C++23 component in EpochGui. Give it one
list of commands, measured text widths and the available window size. It places
the popup and its rows, tracks focus and presses, and returns the chosen command.
The Editor no longer keeps separate row counts or label-width lists.

```text
Editor command list → EpochGui layout and input → Engine drawing → Editor action
```

The component reuses EpochGui's rectangle, popup and scrollbar owners. A menu
that fits has no scrollbar. A clipped menu has wheel scrolling, a draggable
thumb and bounded scroll position. Arrow, Home, End and Page keys move focus;
disabled commands are skipped. Enter chooses once; Escape, outside presses and
focus loss close the menu. A press on one row cannot activate another row.

The Editor command bar requires a click to open its first menu. While a menu is
open, hovering over another command-bar button switches menus. Menus stay open
while the pointer is over a header, popup, submenu or the gap connecting them.
Continuous absence for 750 ms dismisses them; returning resets the delay.
EpochGui owns the elapsed-time helper, and the Engine adapter supplies frame
time and the combined keep-open region. Standalone Debug/Release tests cover
the timer and menu navigation (`Artifacts/Reports/command-menu-nov06/`);
the rebuilt Editor still needs native acceptance of this November6 change.

`gui.engine` supplies the existing font measurements, theme, clipping and drawing.
Only the component activates commands; the row painter does not dispatch again.
The Editor supplies actions and availability, reserves space below its command
bar, and closes the menu before calling an action. Existing early input capture
and overlay order are retained. Standalone Debug/Release component tests and the
Editor descriptor tests pass; see `Artifacts/Reports/command-menu-object-oct27/`.
This adoption covers the Editor command bar. Other context menus and selectors
still need individual migration and acceptance; no whole-GUI completion claim.

The operator-approved Graphite Copper theme extends the existing palette,
selector, cycle, native chrome and settings paths. Its persisted ID is14;
retired13 continues to load as System. The original candidate reused13 and
therefore failed reopen. Actual codec/resolver Debug/Release tests now cover
the new theme, old IDs and invalid15. Fire Engine Red remains test-only.

## Current portable input owner — local, unpublished

Engine's58 portable Gui files and the Demo/Media working pins now bind local
checkpoint `28d81396a4b1e782264712d97bbe7c8aefc05748`. Published Gui0.90.1 at
`7d5d5514a1443c0ef340a626fa163569858ca362` remains unchanged. Portable hashes
bind the exact committed blobs; this is a local source checkpoint, not a new
published version or full-tree mirror.

`epoch.gui.input` now compiles the popup/floating pointer-ownership decisions.
The two existing Engine policy headers only delegate. Adapters still latch
popup capture before background controls, give floating-window children body
input before absorbing unused clicks, and retain existing frame/draw order.
The module does not create a host, scheduler or new input manager.

Standalone Gui plus actual Engine-wrapper consumers pass19/19 in Debug and
Release, including all16 popup,32 chrome and four absorption combinations.
Demo Platform passes5/5 and Media's native composition6/6 in each configuration
against the explicit local Gui root. These are CPU/build results; native input,
full Engine edition integration and the broader GUI ownership migration need
their own acceptance. Receipt: `build/validation/gui-input-ownership-oct26/result.json`.

## Shared frame and presentation contract

Docked/tiled window backgrounds and splitter fills are rectangular so shared
edges do not expose corner cutouts. Floating/top-layer windows retain rounded
corners and shadows. This is surface geometry, not a change to frame ordering or
overlay replay. The Editor badge and native titles measure the same completed
native render frames: the badge samples one second, while titles show a weighted
five-second average. UI update throughput belongs only in diagnostics; neither
counter claims monitor scanout FPS. The status strip reports the effective cap
separately from the measured frame rate.
UI/session production uses the shared deadline limiter and the fastest live
context's foreground/background/minimized policy, bounded by CPU/build pressure.
Do not add fixed post-frame sleeps: they add work time to the intended interval.
Render workers combine pressure and normal pacing in one deadline; queue drain
and frame composition order remain unchanged. Worker polling, shutdown and
explicit capture-settle waits are separate lifecycle operations.
Genuinely uncapped producer/render turns use no additional timer or presentation
frequency ceiling. A nominal sub-microsecond Windows timer measured1–1.7ms in
the frame loop and was removed from those call sites. Native OpenGL completion
fences bound unfinished GPU work, not frames per second. Render and monitor
scanout rates remain distinct; desktop responsiveness needs runtime evidence.
The October30 pacing repair preserves positive finite manual values such as
1500/5000Hz and cooperatively yields during fine waits, late turns and uncapped
turns. This adds no fixed sleep or FPS ceiling. The subsequent uncapped test
still froze the operator's desktop: these changes are not responsiveness proof.

Deferred GUI snapshots remain immutable while replay consumers hold them.
Prepare their vectors, compare content and retire replaced storage outside the
publication mutex. Under the mutex, copy the observed pointer or revalidate it
before the short swap/generation update. An unchanged pending frame is compared
before allocating replacement storage; it retains its existing capacity.
Preserve unchanged/empty generation
semantics, existing base/top-layer publication order and renderer replay order.
Actual Debug/Release CPU checks measured zero allocations/frees across256 warm
unchanged publications after this repair, versus768 each before it. This is
proof at the publication boundary, not a whole-Editor or desktop performance
measurement. OpenGL stream-storage ownership is recorded in
[renderer evidence](renderers.md#october30-gui-submission-repair--source-and-cpu-proof).

The compact timeline reserves measured heading/readout/control heights before
allocating scene space. Time text sits outside the thin progress track; the
scrubber owns its full hit area, and playback actions fit their labels and wrap.
Instructions above a viewport reserve their full wrapped height plus spacing.

## Assistant composer ownership (0.90.54 source candidate)

Portable text-controller undo/redo, caret persistence and history bounds belong to `Engine/dep/EpochGui/`; text-input placement, the responsive two-row chat composer and bottom-following scroll geometry belong to the `gui.engine` EpochGUI adapter. The editor owns AI transport selection, Candidate Lab Pause/Resume/Stop state, operator continuation prompts and authorization, not reusable text controls. An automatic follow occurs only while at the bottom or after a deliberate Bottom command, and disengages on user scroll-up. Live Windows/renderer proof is still pending.

## October22 navigation and chrome

The shared status strip measures labels before layout. Optional per-item
compression priority abbreviates CPU/RAM tags to percentages first, then task
counts, then lower-priority identity fields only when width requires it. Full
Thread/Queued/Idle labels are retained whenever they fit, including default
windows. CPU/RAM appears before version and is the last retained field under
extreme compression. Abbreviated fields expose their full label on hover.
The Editor reserves measured menu widths before assigning status space, rather
than sacrificing menus while a long status label remains uncompressed.

Tab/ShiftTab addresses a hovered tab strip; CtrlTab/CtrlShiftTab navigates its
containing dock group or workspace, including from a text field. Text and scene
own plain Tab away from tab strips. Modal/popup/drag ownership still wins; a GUI
navigation event is consumed by one group. This is tab-group navigation, not a
claim of complete keyboard-only control focus traversal. Native Windows titles
are generic backend/count/FPS; per-segment title color remains a custom-chrome
follow-up because native caption text has only one color.

## Portable scrollbar ownership (October24)

`epoch.gui` now owns compiled vertical scrollbar layout, thumb grab offsets and
pointer-to-scroll mapping in `Engine/dep/EpochGui/src/epochgui/gui.scrollbar_layout.cpp`.
Generic scroll areas and chat views consume the same functions for hit geometry,
drag mapping and drawing. Tiny tracks clamp the visible thumb to the viewport,
matching hit geometry. The engine adapter retains pointer/clip admission, input
consumption, per-context state, follow-tail policy and render submission order.
The standalone `EpochGui.ScrollbarLayout` consumer imports the real library
module and checks scrolling endpoints, grab preservation, translated/scaled
coordinates, short/empty tracks and non-finite extents. This establishes one
portable behavior boundary; it does not prove GUI runtime or complete extraction.

## Portable editable-text layout (October24)

The existing text-control API now owns visual-line construction, soft/hard wrap
caret affinity and pointer-to-byte hit mapping in `gui.text_layout.cpp`. The
engine edit-box adapter supplies a synchronous glyph metric callback using its
existing UTF-8 decoder, drawable-character substitution, kerning and whitespace
policy. It retains atlas measurement, local-coordinate conversion, selection,
input ownership and drawing. No second decoder or text controller is introduced.
`EpochGui.TextLayout` imports the compiled module and covers UTF-8 byte boundaries,
soft versus hard breaks, trailing/empty lines, proportional widths, narrow wraps,
and pointer midpoint/endpoints. Runtime text-input acceptance remains pending.

## Current Boundary

- Portable static-library API: `Engine/dep/EpochGui/include/gui/`
- Portable implementation: `Engine/dep/EpochGui/src/epochgui/`
- Engine adapter API/implementation: `Engine/modules/gui/gui.engine.ixx` and
  `Engine/src/epochgui/gui.engine.cpp`
- Primary editor consumer: `Engine/src/editor/editor.application.cpp`
- Backend replay consumers: renderer/context code that drains deferred GUI
  batches after scene rendering

This document describes current local portable/controller composition. Release
authorities and immutable history are recorded in `Changes/active_pass.md` and
the changelog, not independently versioned by this architecture. Source and
contract integration below does not claim GUI screenshots, responsiveness, or
operator eye proof.

The latest real Epoch Editor screenshots are the default shell/layout
authority. Earlier polished concepts are broad feature-density and workflow
inspiration only; they are not implementation evidence, and neither their scene
content nor unimplemented controls may be copied into product claims. Defect
captures identify repair gates and are not accepted eye-test evidence.

The current implementation is still physically compact, but the ownership rule
is already library-like: reusable controls are added to `gui.engine` first, then
editor domains consume them. Editor workspaces should not reimplement generic
buttons, tabs, dropdowns, scroll areas, text inputs, modal chrome, or clipping
logic.

Direct tab insertion applies to internal and native routed-pane drags. EpochGui
owns variable-width strip geometry, insertion slots, markers, and ghosts. The
editor translates the hovered slot into an exact pane route, logical dock
region, and tab index; the Win32 host consumes that handoff only on button
release. A native pane remains a separate top-level window while moving, becomes
50% opaque for held-window feedback, and never reparents merely because its
cursor crossed the parent. If no compatible strip owns the cursor, existing
outer guides and ordinary floating behavior remain available.

Any active native-pane move, internal title drag, splitter drag, or floating GUI
move/resize is an input-capture boundary. Scene selection, entity dragging,
camera navigation, and global shortcuts behind that grab must not receive the
same input. This is the same ownership rule used by modal layers; visual
top-layer order alone is not sufficient.

Native Windows input admission uses the foreground thread's actual focused
surface, not a renderer thread's stale `GetFocus()` or remembered GUI owner.
Unknown HWNDs are never equivalent merely because both have no context lookup.
Pointer polling additionally requires the context under the cursor, or its
own active native capture for a drag. A covered/inactive context discards queued
pointer events and held-button state; keyboard/text events are discarded when
its context loses focus. `FocusLost` cancels older queued interactions without
manufacturing a release/click, including loss and reacquisition between frames.
Logical text selection is retained, but input cannot edit it while inactive.
Launcher arrow/Enter navigation uses context-safe edge queries as well.
These checks do not pause background AI/build work or change draw/replay order.
Pure GUI contracts cover queue cancellation and inactive/covered admission;
native Alt-Tab, overlapping windows and captured-drag behavior require operator
eye testing.

Large workflow dialogs such as Package Manager derive their bounded size from
the current viewport. Their content regions grow and scroll within one measured
body budget, while progress and action controls remain anchored and reachable.
Fixed proof-of-concept dialog dimensions are not the product layout contract.

Application workspaces use real tab ownership rather than toolbar mode buttons.
The shared workspace-layout contract assigns document, structure, inspector,
and operation-dock providers separately for Standard Editor, Plant Lab, and GUI
Editor. The primary renderer view is externally supplied by the engine context;
the GUI library does not create or own a renderer. Standard Editor and the
projectless launcher GUI Editor consume the same canonical GUI document and
projection adapter. Standard Editor presents the interactive placement canvas;
GUI Editor presents widget creation, Canvas, Runtime Preview, Component Graph,
Styles, verified `.epochgui` I/O, and reusable templates. Both share stable
scene-projected widget identities with Hierarchy and Properties. A scratch
document can contain widgets without scene projections; those rows currently
report that selection is unavailable. Complete document-only selection and
floating-window behavior still need native acceptance.

Authored canvases shrink their text with their widget rectangles. The Engine
adapter's `ScopedTextScale` keeps glyph drawing and measurement on the same
scale, then restores the previous scale. `ContentClipScope` intersects each
widget with its parent's visible bounds and restores those bounds afterward.
Container names appear in the hierarchy; visible titles belong to text widgets,
so a container cannot paint an extra title over its children. CPU tests cover
nested scopes, separate threads, invalid scales and restoration after errors.
Windows Development Release checks confirm the title-overlap repair at normal
size. Small-window readability, canvas zoom and the complete authoring workflow
remain open. This check does not establish Linux or other-backend behavior.

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

`gui.engine::status_strip_preferred_width` measures full labels using the same
font, spacing and padding as the strip. The Editor allocates that width when
available before selecting shorter labels, instead of reserving a fixed share
of the viewport. Its update checking/action control uses the right chrome zone,
immediately before the status group. Task counts remain visible during model
loading; the model's own activity surface carries loading status.

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
- Nested clipping is intersection-preserving. Entering a child content scope
  keeps clipping active even when the child and parent are disjoint; an empty
  intersection is still a clip, never a signal to fall back to the host/window
  extent. Leaving the child scope restores the exact parent bounds. The
  build-safe nested-clip contract covers the disjoint intersection and parent
  restoration that prevent wrapped AI/status text from bleeding across docks.
- Theme and rendering: palette ownership, font/glyph metrics, clipping,
  runtime-surface atlas use, deferred GUI replay, and backend-safe present
  ordering. Theme preference labels, option data, preference resolution, and
  scoped theme application belong to `gui.engine`; editor surfaces may store the
  selected preference but must not recreate theme tables in domain code. The
  launcher, its transitions and updater surfaces inherit this selected appearance;
  they must not force a separate Classic palette. The Editor appearance query reads
  live context settings or the existing saved layout without opening Editor services.
  Theme resolution and Theme Cycle stay in the existing GUI owner.
  The baseline choices are `System Light/Dark`, `Light`, and `Dark`; named Epoch
  palettes currently include Classic Launcher, Midnight Blue, Ember Forge,
  Forest Terminal, and Aurora Steel. System mode resolves through platform
  app-theme preference when available and falls back to the dark palette on
  platforms without an OS light/dark bridge. Light mode is a real high-luminance
  palette with a dark glyph atlas, not a recolored dark surface. Rounded controls
  default on; new palettes extend this table rather than replacing the baseline.
  EpochGui also owns semantic UI tones independent of named themes: neutral,
  muted, info, success, warning, error, assistant, and engine. Each tone resolves
  to a readable light/dark surface plus an accent strip; editor domains select a
  semantic meaning but do not define their own color tables. `gui.engine` exposes
  those tokens through reusable message roles, `semantic_block`, semantic console
  tasks, and opt-in semantic console actions.
- Cross-backend visual parity starts in `visuals.engine`. Frame clears, scene
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

Editor startup follows that same factory instead of generating an empty root
as a product default: game projects select Game HUD and other project kinds
select Desktop App. The migration path is deliberately narrower than “one root
widget.” It serializes and compares the complete snapshot to Epoch's exact
historical generated `MainCanvas`-only document, replaces only that byte-
equivalent legacy shape, and leaves the replacement dirty until an explicit
GUI/Project save. Authored Blank Canvas documents and every other valid source
are not migration candidates. Starter round-trip and exact legacy-refusal/
replacement behavior belong to `authoring.gui_document` contract evidence.

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
  document. Assistant Chat remains a separate dockable conversation tool so the scene
  can stay visible. Reusable console windows own clipped transcript, input, and
  optional pinned/footer action rows; approval-sensitive actions stay pinned
  above scrollable content so compact docks cannot hide authority controls.
  Text rows accept optional semantic roles so user, assistant, system, info,
  success, warning, error, and engine-work messages use shared contrasting
  surfaces without editor-side drawing. Project-assistant mode uses the assistant
  semantic tone; isolated engine self-coding uses the engine tone, so authority
  domains remain visually distinct in every console using the shared primitive.
  Chat labels are User, Assistant and Agent, with Agent reserved for engine
  self-iteration status. Opt-in message bubbles use existing rounded semantic
  backgrounds, bounded wrapping, content-sized rows and right-aligned user
  messages; ordinary Output/text lists keep their existing presentation.
  Console hosts may place the action row beside the prompt, show a selectable
  two-line activity block, and expose flat inline controls in a bounded scroll
  area. Multiline prompts reserve three text lines. The Assistant uses these shared
  controls for Model / Options, EngCoder lifecycle and endpoint/model settings
  inside the existing conversation pane. Consecutive engine-host status updates
  replace one activity message; the candidate journal retains detailed evidence.
  Send first publishes the user row and presents a frame before routing/model
  preparation. Queued requests block duplicate submission. Right-click Retry
  Request resubmits the original operator input through normal routing and fresh
  approval, never an assistant/protocol packet or a previous approval. The menu
  binds the exact row text and closes if transcript trimming replaces that row;
  busy requests disable Retry. Selection/copy controls remain available.
  Font line metrics reserve raster/descender padding, and focused text controls
  own a visible high-contrast caret across chat, script, and ordinary edit fields.
  Partially clipped glyphs are omitted rather than destination-scaled because
  atlas UVs remain immutable during GUI clipping.
- Scripting needs a real code/text editor surface, not a Console Dock submenu.

The single `Output` tool owns selectable log text and compact Project, Assets,
Automation, and Systems evidence filters. These status categories are not
independent dock routes. Do not reintroduce property-row blocks, action buttons,
selection tables, or progress widgets into Output filters; those controls
belong in central workspaces, modal windows, or Inspector-owned panels.

Docking defaults to in-host tool-tab stacks over one live editor projection.
World Outliner, Asset Browser, GUI Hierarchy, Script Browser, Tile Map,
Properties, Global Settings, Output, and Assistant Chat each own an independent tool
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
Left, and Bottom Right tab groups. Output and Assistant Chat default to the two bottom
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
Assets, AI Engine Dev, Systems, or other non-World surface to mutate World by
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

`EpochGui` is a real linkable static-library target with backend-neutral source,
interfaces and consumers under `Engine/dep/EpochGui`, mirrored from
`Autodidac/EpochGui`. Engine-specific composition remains in `Engine/src/epochgui`.
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
GUI proof routes are infrastructure only. Modal/action sizing, scroll bodies,
text adapters and Package Manager rows have production consumers; their remaining
integration, narrow/high-DPI and native interaction evidence belongs to the
roadmap. Do not recreate those controls from this older conversion sequence or
touch protected top-layer composition to compensate for an untested layout.

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
  configuration outside project and release data. Output and Assistant Chat initially
  occupy Bottom Left and Bottom Right respectively, but both are ordinary routes;
  an empty bottom group collapses and the occupied group consumes the available
  width. Restore is schema-bounded; malformed files fall back to application
  defaults, and writes use same-filesystem atomic replacement. User
  tab reordering/direct insertion have production controllers; arbitrary
  compatible stack creation and native integration coverage remain follow-up.
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


## Multicontext routed-pane ownership

Native floating contexts are alternate hosts for canonical editor panes, not independent copies of every subsystem.

- Mouse/keyboard state is accepted only from the focused owning Epoch native context.
- A floating context's top row remains normal GUI hit-test space; host-window movement must use the explicit host-drag gesture instead of stealing ordinary clicks.
- Docking guides are computed in the parent editor coordinate space after converting/scaling routed-host mouse coordinates.
- Closing a detached pane returns it to its remembered dock. Window/command-menu restore actions make the pane active so a closed native host cannot make a tool permanently unreachable.
- Assistant Chat is strictly single-owner: the canonical editor owns the AI/model/session state. A detached Assistant Chat owns only projection/input-buffer/mailbox state. It must never construct a second model/session owner during Play/Run or undock/redock.

These are lifecycle and ownership contracts, not backend-specific visual tricks.

## Font and surface shadowing

EpochGui owns reusable text/window shadow intent. Font shadowing uses a GUI-owned glyph/atlas layer so editor domains do not render a second ad-hoc font pass. Window/chrome shadowing likewise belongs to shared GUI composition. Backends such as SDL3 may have their own native rendering mechanisms, but they consume the shared visual intent rather than defining a competing editor-only shadow system.

### Interval-based theme cycling (development candidate)

`Editor Settings > Theme > Theme Cycle` cycles through the seven fixed
palettes (Dark, Light, Classic Launcher, Midnight Blue, Ember Forge, Forest
Terminal, Aurora Steel). `System Light/Dark` is excluded because it resolves to
one of those palettes. The adjacent interval selector offers 30 seconds
(default), 1, 3, 5, 8, 10, 15, 20, 30 minutes, and 1 hour. A monotonic
`ThemeCycleClock` in the shared GUI implementation computes the current slot
without blocking the draw loop. Selecting another theme disables cycling;
returning to Theme Cycle starts at Dark. Changing the interval restarts the
cycle. The editor persists theme choice and interval in version 6 of its layout
settings and accepts legacy versions 1-5 with the 30-second default. Other
editor-snapshot restoration includes the interval. This is a source candidate;
Windows runtime/appearance acceptance is not yet complete.
