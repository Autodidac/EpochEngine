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

The primary editor surface cannot be undocked. Secondary contexts are reserved
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

1. acquire the frame and drain scene submissions;
2. render the scene through the active backend;
3. compose EpochGui menus, tool windows, modal layers, and diagnostics above it;
4. present exactly once.

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

## Acceptance

A context is accepted only when the regression plan proves startup, canonical
scene orientation, alpha and sampling, resize, save/reopen, generated project
Build and Run, replacement teardown, and bounded resource behavior. Build
success alone proves availability, not visual parity.

See `renderer_regression_smoke_plan.md` for the seven-backend matrix and
evidence artifacts. See `renderer_feature_matrix.md` for current capability
status.
