# Full-frame scene rendering

## October30 policy and acceptance

The operator approved removal of cached/partial scene rendering and cancelled
the proposed multi-frame animation cache. Source now uses the existing OpenGL
conventional scene path and removes software whole-frame skip/reuse logic. Each
admitted frame reconstructs its scene viewport and follows the existing backend
queue, base-GUI, scene, top-layer, capture and presentation order.

The OpenGL scene color/depth cache, regional redraw policy, exclusive pending
object/lighting damage queues, live cache diagnostics, dirty/vacated overlays and
SELECTIVE/FULL A/B benchmark have been removed from source. There is no remaining
runtime toggle to restore them. Geometry/lighting publication and revisions remain.

Focused CPU contracts and combined Debug/Release builds/full pure contracts pass;
see `Artifacts/Reports/full-frame-rendering-oct30/Debug-Release-result.json`.
The same combined Release still lagged in the operator's actual 120 retry.
Native responsiveness and GUI/image acceptance remain open; follow the
[active pass](../../../Changes/active_pass.md).

## Preserved ownership

Ordinary asset/font/texture residency, immutable GUI draw snapshots, geometry
preparation and Canvas2D derived raster/texture preparation remain. Those caches
serve resource preparation; they do not authorize skipping final native frame
composition. This policy does not claim that no cached pixels exist anywhere.

The central event bus, explicit simulation time, task/job services, temporal
identity and generic neuromorphic/event-camera/invalidation primitives remain.
They are not replaced by a renderer or a new scheduler. Future event/perception
consumers remain separately scoped in [neuromorphic design](neuromorphic.md) and
[world integration](voxels.md#central-event-rendering-and-system-one); no learned
policy or automatic performance gain is implied.
