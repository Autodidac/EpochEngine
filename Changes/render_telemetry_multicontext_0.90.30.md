# v0.90.30 — Render telemetry correctness and multicontext recovery

## FPS semantics

Epoch now reports two different rates instead of collapsing them into one misleading number:

- **FPS / presentation FPS** is the actual native context loop/present rate. VSync, frame caps, editor pacing, and benchmark synchronization can limit it.
- **render FPS** is derived from measured scene-render time for the active render path. This is the number that exposes the large cost reduction of a cached frame even when presentation remains capped.

OpenGL titles identify the active cached/partial/full/conventional render path. A/B benchmark titles report selective and full-baseline render throughput while retaining the real presentation FPS.

## Persistent statistics

Cached, partial, full, and conventional timing/count accumulators survive disabling event rendering. Selective/full A/B accumulators likewise survive disabling benchmark mode. Evidence is cleared only by explicit reset commands.

## Multicontext recovery

- Dock-guide projection converts parent-host coordinates into the editor surface before guide placement.
- Routed panes refresh docking guides while floating/native windows move.
- Closing a detached tool returns it to its remembered dock instead of making it unreachable.
- Window/command-menu Show/Return actions reactivate the pane when restoring it.
- Detached AI Chat is a projection/mailbox client of the canonical editor AI session; it does not construct a second model/session owner in the detached context.

That ownership rule removes the Run-mode crash path caused by two AI owners competing for one editor/model lifetime.
