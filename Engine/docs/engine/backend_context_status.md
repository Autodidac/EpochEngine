# Backend Context Ownership

This document defines renderer-context topology and lifetime ownership. It does
not duplicate renderer feature evidence.

- `renderer_feature_matrix.md` owns capability truth.
- `renderer_regression_smoke_plan.md` owns acceptance and evidence policy.
- `runtime_and_editor_workflows.md` owns launcher and editor behavior.
- `Changes/changelog.txt` owns version chronology.

## Active Context Families

| Family | Role |
| --- | --- |
| OpenGL | First Tier 1 reference implementation and shared GL behavior baseline. |
| SDL3 | Specialized GL-derived desktop context using Epoch-owned scene and GUI contracts. |
| SFML3 | Specialized GL-derived desktop context with explicit host/context activation ownership. |
| Raylib3 | Specialized GL-derived context with Raylib-specific window and texture lifecycle rules. |
| Vulkan | Explicit GPU backend with backend-owned synchronization and retirement. |
| Direct3D 11 | Active Windows-native GPU backend. D3D12 remains a separate future backend. |
| Software | Tier 0 CPU fallback, deterministic reference, diagnostics, and headless-compatible path. |
| No-op/headless | Minimal lifecycle host for tests that must not initialize a renderer. |

Every active renderer consumes shared semantic scene, Canvas2D, texture,
material, camera, selection, and GUI data. A backend may adapt those contracts;
it must not invent a parallel authoring model.

## Normal Editor Topology

The normal editor assigns one live renderer context logical active-editor
authority at a time. On Windows, that authority is independent of the physical
context's creation order, parent-grid side, or dock state. A missing-target
context replacement is a single serialized transaction:

1. capture editor and scene state;
2. stop submissions to the source backend;
3. retire and join backend-owned work;
4. destroy source native and graphics resources;
5. create the selected backend in the primary slot;
6. restore the captured state;
7. acknowledge one restored frame before another replacement may begin.

On the implemented Windows host, physical renderer surfaces can undock/redock
without transferring logical active-editor authority. Backend-specific ownership
and unsupported-host restrictions still apply. Secondary contexts are reserved
for explicitly requested diagnostics, previews, and floating tool surfaces;
they are not cloned editors and must not continue rendering after closure.
Multicontext mode is a diagnostic topology, not the normal editor or a source
of runtime backend-scoring evidence.

Linux and WSL default to one OpenGL editor context. Additional Linux backend
builds remain valid production targets, but context replacement is not claimed
until the Linux host owns the same complete retirement and restoration
transaction.

## Protected Frame And GUI Order

The established scene and GUI draw model is protected:

Preserve each backend's existing frame acquisition, GUI preparation, scene
submission/drain, top-layer GUI replay and single-present path. Some GUI work is
prepared or batched before the scene and replayed above it; this document does
not prescribe moving all GUI execution after scene rendering. The detailed
ordering guardrails are owned by `gui_library_architecture.md` and each backend.

Backends must not perform a second clear or present, mutate native windows from
an unowned thread, replay GUI below scene content, or keep backend work alive
after retirement. Changes to this order require a dedicated draw-model mission,
build proof, and operator eye-test evidence.

## Backend Ownership Notes

- OpenGL owns the reference winding, alpha, texture sampling, resize, and
  Canvas2D behavior used to compare specialized GL-derived contexts.
- SDL3 and SFML3 own their native window and GL activation details while using
  Epoch scene and GUI queues. Proxy ownership must be explicit during teardown.
- The Windows parent host may inspect context registry state under its mutex, but
  native placement, owner-thread commands, and resize callbacks execute after that
  mutex is released because window procedures reenter context bookkeeping.
- Raylib3 owns Raylib window/texture lifetime. It must preserve the same scene
  orientation and must not create a second editor window during replacement.
- Vulkan owns queues, fences, swapchain images, and deferred destruction.
  Replacement cannot complete until backend work and resources are retired.
- Direct3D 11 owns its device, immediate context, swapchain, render targets, and
  Windows-native retirement. D3D12 must not be represented as D3D11 capability.
- Software owns CPU rasterization and reference output without silently
  initializing a GPU context.

## Universal Context Interface: Foundation And Planned Migration

This section is an implementation plan, not a claim that a new platform library,
portable custom-context API, or complete backend parity has shipped. The target
is one reusable host contract for CLI tools, ordinary platform-window software,
rendered applications, editor panes, and explicitly launched sandbox candidates.
An application must not instantiate an editor, an AI session, or a GPU merely to
use a window, a timer, or a supervised child process.

### Implemented First Layer (Local v0.89.35)

`context.admission` now supplies dependency-light profile preflight, explicit
known/unknown support, backend activation/multiplicity constraints, and
generation-bound process/window/thread readiness checks. Attachment, child-
reported presentation, and final capture remain different evidence states.
The module creates no native resources. Its owner-queue declaration does not
implement a queue, and caller-provided evidence is not OS confinement or an
executable-hash attestation. Existing renderer and multicontext hosts have not
been migrated to it.

`Epoch::SoftwareBase` is the first production preflight consumer. Its public
`epoch.software_application.hpp` reuses `app_callbacks_v1` for an editor-free
CLI or single owned Win32 window. It links only the small lifecycle, logging,
time, context-profile and existing platform-window closure. Initialization,
tick, failure cleanup and shutdown stay on the calling thread; CLI does not
call a native-window factory. Unsupported native platforms fail explicitly.
See `Engine/examples/SoftwareBase/README.md` for focused build/test commands.

This window-only foundation does not yet provide keyboard/pointer/text routing,
EpochGui, drawing, capture or presentation. Built-in GUI for finished non-CLI
application profiles remains required; the foundation is not the completed
software template or an EpochPlatformEngine release. Backend-specific native
proof and generated-template integration remain gates before stable-base
advancement.

### Reuse The Existing Owners

| Responsibility | Existing owner to extend or extract | Boundary to preserve |
| --- | --- | --- |
| Backend identity and render callbacks | `context.type`, `core.context`, and the `BackendMap` registration in `src/renderers/core/engine.context.cpp` | Keep `ContextType` as the renderer identity. `Custom` is not permission to load an engine plugin or to claim a GPU capability. |
| Native lifetime and owner-thread work | `context.window::WindowData`, `context.commandqueue`, and the platform implementations of `context.multiplexer::MultiContextManager` | Preserve native/DC/GL ownership, proxy ownership, retirement, and separate owner-thread/render queues. Do not copy native handles when cloning callback configuration. |
| Resize, visibility, and frame intent | `render.context_frame::WindowObservation`, `WindowState`, `FrameRequest`, and `FramePlan` | Reuse logical/framebuffer sizes, DPI, resize generations, activity, clipping, and presentability. A platform event is an observation, not a second renderer. |
| Platform-window clients | `platform.window::WindowHandle`, `WindowDesc`, `WindowEvent`, and `IWindowSystem` | Extend the existing window seam rather than introducing another native window registry. Its current implementation is not evidence of cross-platform parity. |
| Platform policy and timing | `platform.engine::RuntimePolicy`, `core.time`, and existing `perf.tier` frame-pacing capabilities | Keep platform topology, monotonic time, simulation time, and backend pacing distinct. Hidden/minimized windows need not advance an application simulation. |
| Child lifetime | `platform.child_process::ProcessHandle`, launch/wait results, and process snapshots | The supervisor owns child identity, cancellation, exit evidence, and admitted execution; the context host owns placement only. A PID alone is not durable identity or sandbox confinement. |
| Application/session composition | `epoch.runtime::LaunchOptions` and `ContextSession` / `RunContextSessionLoop` in `src/epoch.engine_legacy.cpp` | Extract reusable lifecycle below the current editor/menu/scene composition. Preserve its simulation and restoration ownership instead of cloning that loop into every client. |
| Docking and input projection | `DetachedContextWindowRequest`, `RoutedPanelDockDragProjection`, routed tab targets, and existing GUI/input ownership | A layout operation changes placement, not editor authority, process ownership, or backend frame order. |

Two existing seams need reconciliation before they can become public platform
building blocks. `platform.context::IGraphicsContext` currently has only a
`NullGraphicsContext` factory implementation; its Vulkan/OpenGL/D3D12 enum
entries do not register the live renderer adapters. `platform.runtime` also
derives some capabilities from those enum values. Do not put a second rendering
stack over that skeleton or publish the enum-derived values as device evidence.
Adapt the live registry into the existing seam, or narrow the seam to its actual
window/surface role, with explicit unsupported results for absent operations.

### Proposed Contract Shape

Universal means shared lifecycle and requests, not identical native behavior.
Implement the following small responsibilities through the existing owners:

1. **Identity and purpose.** Add a host-issued, generation-checked identity for
   each admitted surface/session. Keep purpose (ordinary application, editor,
   routed tool, or sandbox preview), native ownership (owned or borrowed), and
   optional render backend separate. Do not use a raw `Context*`, HWND, or PID
   as a persistent identity after closure or replacement.
2. **Declared needs and supported operations.** Describe whether a client needs
   a native window, input, rendering, capture, parent placement, or child-process
   presentation before creating resources. A backend adapter reports support
   and the required owner thread. Include maximum live instances per process,
   event-pump ownership, borrowed/owned handles, embedding mode, capture origin
   and finality, and known/unknown presentation evidence. The initial
   `context.admission` contract describes these constraints, but integration
   with each actual backend remains planned. Do not
   silently substitute OpenGL, an editor, or a null renderer for an unavailable
   requested operation.
3. **Requests and immutable observations.** Route resize, title, close, focus,
   placement, and capture requests to the existing owner queues. Publish a
   generation-bound snapshot of lifecycle, `WindowState`, input ownership,
   supported operations, and errors. Never invoke reentrant native callbacks
   while the manager registry lock is held.
4. **Session execution.** A platform-only client pumps events and its own work;
   a render client submits to the existing `Context::process` path. The common
   host must not add an unconditional clear/present or reorder any backend's
   queue drains. Use existing clocks and frame-pacing contracts rather than a
   new per-facade time source.
5. **Readiness and retirement.** Distinguish native attachment, backend-ready,
   first successful presentation, captured-frame availability, and application
   readiness. Closing stops admission, drains or rejects owned work, retires
   resources, joins owned threads, and invalidates the identity. Backend-specific
   fence/activation/proxy cleanup stays in its adapter.

The current Windows `AddExternalProcessWindow` verifies that the HWND belongs
to the supplied PID and records attachment/lifecycle readiness without inventing
presentation counters. Attachment is separate from child-reported readiness
and actual frame/capture evidence. An externally hosted candidate keeps its own
executable, runtime, backend, and process-local resources; it is not an in-process
clone of the parent editor. Linux currently rejects external-window attachment
and replacement through this manager, so unsupported must remain visible until
a native implementation and proof exist.

### Client Needs And Backend Constraints

| Client or backend | Minimum needs | Ownership and acceptance requirement |
| --- | --- | --- |
| CLI/headless tool | Core lifecycle, logging, time, paths, optional explicitly admitted child execution; no window or renderer | No GUI/AI/editor initialization, graphics loader, listener, or native-window side effect. A no-op render contract is optional, not required for a CLI. |
| Platform-window software | Native window, event pump, focus/resize/close, optional EpochGui | Ordinary app callbacks and window ownership must work without scene/editor state. Non-CLI GUI support remains built in, not a downloadable engine plugin. |
| Software-rendered application | Native presentation when requested; CPU buffers and capture | No implicit GL/Vulkan/D3D initialization. The current `software.state` uses process-global state, so a successful single instance does not admit arbitrary same-backend concurrency. Prove buffer bounds, resize, presentation, and closure independently of GPU results. |
| OpenGL | Native drawable, correct current context, render owner, reference frame path | Preserve context activation, shared-resource rules, one presentation, and source/destination retirement. |
| SDL3 / SFML3 | Their native window/event integration and GL activation adapters | SDL's adapter has a process-global runtime lease and separate hosted/standalone event ownership. SFML owns activation through `setActive`. Respect proxy/native ownership and teardown order; a generic host must not destroy borrowed windows twice. |
| Raylib3 | Raylib-specific window, thread, renderer, and texture ownership | Its current runtime is process-global and initialization requires the exact existing owner/thread on re-entry. Preserve that constraint; a common interface does not grant concurrent-instance safety. |
| Vulkan / Direct3D 11 | Their native surface plus explicit backend device/presentation ownership | Resize and close must respect queues, in-flight work, swapchain resources, and retirement. Direct3D 12 is not covered by Direct3D 11 acceptance. |
| Custom in-process application surface | A declared host/window contract and only the capabilities it supplies | Reuse the registered callback seam; optional rendering may be absent. No dynamic engine-plugin system or inherited capabilities from a label. |
| Sandbox candidate from another executable | Supervised process identity plus an admitted presentation/placement adapter | Bind launch, working directory, executable identity, window identity, and session generation. Parent placement never grants source-write authority or arbitrary process control. |

`Keep Current` / `Choose` remains an application-level sandbox decision above
this host contract. Selecting a candidate changes the sandbox lineage and the
owned preview process set, not the normal source tree or unrelated projects.
The parent shell stays available to make that choice. A context API must not
infer promotion, start listeners, or terminate processes it did not launch.

### Floating Interaction And Readable Guides

Preserve the existing dock guides, direct tab insertion, and native floating
routes. The next interaction work should make their ownership explicit across
owned windows, library proxies, and admitted external surfaces:

- One gesture owns drag/resize/tab-transfer input until release or cancellation.
  Background viewports, text controls, shortcuts, and camera input must not
  consume that gesture; focus loss and native capture loss must release it.
- Keep a direct tab target stable through small pointer excursions, show the
  exact insertion slot, and restore normal opacity on every terminal path.
  Apply the requested held-window transparency only to the held surface, not
  the entire editor; unsupported native opacity requires an honest visible
  drag preview, not a success flag.
- Reuse EpochGui text measurement, clipping, and DPI-aware layout for guide
  labels and selected-target descriptions. Test narrow docks, large scaling,
  long titles, and missing-glyph fallback. Do not render opaque technical route
  identifiers or blank/icon-only controls as the user's sole instruction.
- Keep logical editor authority independent of physical dock state and preserve
  backend-specific proxy/undocking restrictions.
  Do not convert a floating tool into an editor context or reparent a backend
  window from the wrong thread merely to make a guide appear responsive.

These are interaction acceptance requirements, not a declaration that all
floating paths or backends currently satisfy them.

### Migration Order And Baseline Gate

1. Inventory actual registration, window ownership, activation/retirement,
   capture, and pacing behavior for every backend. Add dependency-light
   contracts for identity, needs negotiation, lifecycle transitions, resize
   observations, and stale-request rejection before moving implementation.
2. Separate ordinary CLI/platform-window composition from editor/menu/scene
   policy in the existing runtime/session entry points. Prove minimal clients
   without constructing the editor, AI systems, renderer, or updater. Keep the
   current editor path intact during this extraction.
3. Adapt no-op and platform-only/software clients first, then OpenGL. Introduce
   the shared request/snapshot seam around existing calls without changing their
   order. Reconcile `platform.context` with the live registry at this stage;
   avoid leaving two authoritative backend enums/capability tables.
4. Move SDL3, SFML3, Raylib3, Vulkan, and Direct3D 11 behind that seam one at a
   time. Keep each backend's native state private and stop migration of a failing
   backend without regressing already working paths or hiding unsupported ones.
5. Bind routed floating tools and external sandbox previews to the same
   identity/input/placement contract. Prove cancellation, reparenting, native
   capture loss, child exit, and repeated candidate selection before calling
   the comparison workflow complete.
6. Package the proven dependency-light platform services gradually as
   `EpochPlatformEngine`. This is a reusable layer of the existing engine, not
   another engine copy, mandatory editor dependency, plugin system, or duplicate
   window/context owner. Keep paths/logging/time/process supervision usable
   independently; window and renderer adapters remain opt-in composition.
7. Update `multicontext-base-stable` only to an exact committed revision whose
   claimed CLI, platform-window, and multicontext paths pass their gates. Record
   its previous tip as recoverable history before moving the branch; never
   rewrite immutable releases or replace failed native evidence with a build
   result. The authorized branch update is a separate integration action, not
   something this design document performs.

Required proof includes build-safe lifecycle/identity/queue/resize contracts;
minimal CLI and platform-window build/link dependency checks; ordinary app
startup/input/resize/minimize/restore/close tests; and backend-specific native
render/presentation/retirement tests. Candidate tests must check wrong/stale
process-window identity, launch failure, child crash/exit, input isolation,
repeated choose/keep transitions, and unchanged live-source/project hashes.
Capture timestamps and generation identities must correspond to the tested
binary. Existing native runtime permission and stop-on-driver-instability rules
still apply. Report each unsupported or untested cell independently; no single
backend run proves a universal context implementation.

Reuse the existing `epoch_render_context_frame_contract`,
`epoch_gui_dock_layout_contract`, and `epoch_editor_workspace_layout_contract`
CTest lanes, plus the pure context/session and SDL ownership/readiness contracts
in `--engine-contract-self-test`. They prove their state/geometry/ownership
rules, not native pixels, cross-process embedding, or driver-loss recovery.
Child-process contracts that launch non-GPU helpers are a separate execution
lane; do not relabel them as the pure renderer-free aggregate.

## Acceptance

A context is accepted only when the regression plan proves startup, canonical
scene orientation, alpha and sampling, resize, save/reopen, generated project
Build and Run, replacement teardown, and bounded resource behavior. Build
success alone proves availability, not visual parity.

See `renderer_regression_smoke_plan.md` for the seven-backend matrix and
evidence artifacts. See `renderer_feature_matrix.md` for current capability
status.
