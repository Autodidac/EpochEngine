# Renderer Feature Matrix And Backlog

This document folds the external OpenGL / Vulkan / Direct3D feature inventory
into Epoch's active renderer roadmap. It intentionally removes capabilities that
Epoch already has in some usable form from the "new work" list, while still
calling out partial coverage that needs hardening before it can be treated as a
full renderer feature.

## Policy

- C++23 remains the default engine baseline.
- OpenGL is the proving backend for editor/runtime renderer work right now.
- Vulkan remains an explicit graphics migration path, not the default runtime
  renderer.
- DirectX/D3D11 is the first active Windows-native renderer slice for the old
  software-as-product-renderer role. It is screenshot-proven for context,
  swapchain, clear/present, basic shader preview rendering, scene-preview
  gating, GUI replay, and whole-primitive clipping for preview line/triangle
  buffers. It now has real split implementation units for context bridging,
  state/lifetime, device/shader setup, preview geometry, and GUI atlas/sprite
  replay, but still needs the formal renderer resource API and depth ownership
  before it is feature-complete. It is disabled on Linux/WSL.
- D3D12 remains planned until its own build, context, swapchain, shader, and
  resource layers are intentionally promoted and validated.
- The software renderer is a safe-launch/debug GUI and headless-validation
  fallback, not the long-term Windows production renderer target.
- The abstraction should follow the Vulkan/D3D model: explicit buffers,
  textures, samplers, pipelines, bindings, command submission, render targets,
  synchronization, and debug/profiling hooks. OpenGL should emulate that model
  instead of pushing the engine toward legacy implicit OpenGL state.
- Each feature must be validated by at least one stable backend before it is
  marked complete. Cross-backend parity follows after the engine-facing API is
  stable.
- Capability status is strict:
  - `Present` means a native backend implementation exists and is build-verified.
  - `Partial` means a contract exists or one backend works, but parity/proof is
    incomplete.
  - `Missing` means no real implementation exists.
  - `Deferred` means intentionally not part of the current backend/phase.
- Contract-only code is not a feature. Do not mark all backends true because a
  descriptor, enum, null device, or planning row exists.

## Renderer Capability Truth Table

This table is the source of truth for renderer feature status. Keep status
language honest: if a backend-native resource path, build proof, or parity gate
is missing, the row stays `Partial`, `Missing`, or `Deferred`.

| Feature family | Current Epoch status | Notes |
| --- | --- | --- |
| Window/context bootstrap | Present | OpenGL, Raylib, SDL, SFML, Vulkan, DirectX/D3D11, software fallback, and noop/headless paths exist. Normal editor use is converging toward explicit first-class context panes with inactive backends torn down rather than hidden. D3D12 is planned, not active. |
| Frame begin / clear / present | Partial | Core paths exist, but OpenGL composition, GUI replay, resize, modal z-order, and backend parity remain regression-sensitive and must stay in smoke coverage. |
| Basic primitives | Present | Triangles, quads, grid/marker primitives, cubes, lights, Canvas2D, and editor helper geometry exist. DirectX/D3D11 now clips preview lines and triangles as complete primitives so one clipped endpoint cannot corrupt later line-list pairs. |
| Shader pipeline | Partial | OpenGL shader setup, Vulkan SPIR-V assets, and DirectX/D3D11 first-pass HLSL preview shaders exist. Formal cross-backend shader/pipeline ownership remains backlog. |
| Uniforms / UBO-style data | Partial | OpenGL uniforms and Vulkan uniform/descriptors exist; the engine-facing binding model still needs formalization. |
| Vertex buffers / indexed drawing / VAO-equivalent | Partial | OpenGL, Vulkan, and DirectX/D3D11 carry first-pass buffer/index paths; the shared render-device API is still early. |
| Transforms / projection / camera | Present | Perspective camera, Canvas2D camera, preview camera math, object transform data, and editor camera controls exist. |
| Quaternion/API-neutral camera math | Partial | Camera behavior exists; stronger math ownership and test coverage are still needed before calling this complete. |
| Texture mapping / samplers / atlases | Partial | Atlas, upload, GUI font, Vulkan texture, and backend texture modules exist; sampler policy and filtering choices need a renderer-level contract. |
| Materials / basic lighting | Partial | Ambient-solid primitives and basic light entities exist. Material handles and named texture slots now flow through the render graph/device binding contract; backend-native material shading, specular paths, and multiple light types remain backlog. |
| Debugging support | Partial | Logging, Systems diagnostics, Vulkan validation messaging, host FPS title diagnostics, screenshots, and smoke docs exist. GPU debug markers/query plumbing remain backlog. |
| 3D picking | Partial | Editor object selection exists, but full ID-target/depth/ray picking is not complete. |
| Framebuffers / render targets / capture | Partial | OpenGL/Vulkan/DirectX swapchain or framebuffer paths, capture bridges, and runtime surfaces exist. The shared renderer API now describes sampled render-target assets plus backend requirements for color/depth attachments, sampler use, offscreen target ownership, and presentable-surface use. Raylib allocates native sampled render textures only when the Raylib runtime renderer is live and now fails closed during build-only/no-runtime validation; OpenGL owns a native FBO/color texture/depth renderbuffer/sampler hook factory in `opengl.textures`, and that real factory is now imported by the engine contract harness instead of existing only as an unused export; SDL3/SFML3 now have concrete backend-native target-texture/render-texture device modules with safe no-runtime build gates. The OpenGL-family logical gate still proves the common `engine_arcade.screen` graph and arcade-cabinet material/model consumption without touching live GUI order. Live device hookup and presentation parity remain active gates. |
| Renderer resource spine | Partial | `render.device` owns formal handles/descriptors for buffers, textures, samplers, shaders, pipelines, materials, render targets, binding sets, command lists, mesh/model resources, render-pass/FrameGraph-ready targets, sampled render-texture asset plans, backend attachment/sampler/presentation requirements, default sampled-target handle allocation helpers, material texture slot descriptors, and command resource bindings. `render.graph` compiles sampled render-texture assets as one owner of color texture, sampler, and render target handles, declares material and mesh/model descriptor resources, resolves pass read/write bindings into backend handles including the sampler required for sampled render-texture/material reads, refuses `MaterialTextureSlot::render_surface` bindings unless the texture is owned by a sampled render-texture asset, asks the device for backend binding-set handles per pass, and resolves graph-level model draw intents into command-context `draw_model` submissions. `render.arcade` owns the reusable Engine Arcade screen/cabinet graph builders, including a two-pass proof where the populate pass draws a tiny scene model into `engine_arcade.screen` and the cabinet pass samples that render surface through a material slot. The build-only engine contract self-test compiles `engine_arcade.screen` through the null device and OpenGL-family logical devices, proves the mini-arcade cabinet graph reaches material/model binding plus model draw submission, verifies the OpenGL native hook seam with a fake allocator, verifies the real OpenGL hook factory can be installed on the OpenGL-family device and drive the shared descriptor/record cleanup path, proves a plain texture cannot masquerade as a render-surface material binding, checks SDL3/SFML3 native render-texture devices can safely refuse allocation without a live renderer/window, verifies Raylib refuses native RTT allocation when no live Raylib renderer exists, and proves SDL3/SFML3 graph-native cabinet model handles submit and release correctly. OpenGL, Raylib, SDL3, and SFML3 have concrete sampled-RTT device paths at different proof levels; SDL3, SFML3, and Raylib now report native sampled-RTT capability only when their runtime renderer/window is actually live. Vulkan/DirectX remain resource-model gates. System Info reports declared descriptors separately from backend-native mesh/model and sampled-RTT allocation so the UI does not overclaim renderer parity. |
| Runtime-mini render assets | Partial | Render-to-texture remains a core engine renderer primitive. Engine Arcade is a separate runtime-mini/game package consumer that records its default mini-runtime scene, scene inventory, render-to-texture asset role, renderer-resource requirements, and `engine_arcade.screen` 512x512 sampled target through `package.registry`; the package gate now asks for sampled render targets and binding sets explicitly. `render.arcade` provides the shared source builder for that sampled target, an offscreen mini-scene populate pass, and a cabinet material/model graph that samples the result. The engine contract harness proves that sampled target can compile as a texture/sampler/render-target/binding-set asset through the shared graph, can receive a model draw in its render pass, can be consumed as a cabinet-screen material/model draw by the OpenGL-derived logical devices and SDL3/SFML3 native devices, and has runtime-gated native-device allocation lanes in Raylib, SDL3, and SFML3 plus an OpenGL real hook-factory contract. Build-only tests cannot accidentally mark those native lanes present or create/destroy GPU objects without a live runtime. Actual arcade-cabinet presentation and Vulkan/DirectX resource-model implementations remain follow-up work. |
| Text and UI rendering | Partial | Engine-owned GUI, font atlas, scroll views, tab bars, splitters, and runtime-surface textures exist. Professional dock/window polish, selectable/editable text parity, context menus, and floating GUI windows remain active work. |
| Platform window layer | Present | Win32 and Linux/X11 host paths exist with backend-specific context ownership. |

## Missing Renderer Feature Backlog

The source feature list maps 61 OpenGL families to Vulkan and Direct3D
equivalents. After removing the present/partial foundations above, the missing
work should be grouped this way instead of tackled as an unstructured checklist.

### Baseline Renderer Completion

- Backend-native implementations behind the formal renderer resource model:
  buffer, texture, sampler, shader, material, mesh, model, pipeline, binding
  set, render target, pass, command list, and synchronization handles. The
  engine-facing handles/descriptors exist first; Raylib has the first native
  model-handle and runtime-guarded sampled-RTT lane, OpenGL/SDL3/SFML3 share the first logical
  sampled-RTT plus arcade-cabinet material/model draw contract, OpenGL has the
  first FBO-backed hook factory, SDL3/SFML3 now own native target-texture/
  render-texture device modules, and broader backend allocation/feature parity
  is the active gate.
- Formal material system with diffuse/specular parameters and texture slots.
- Multiple point lights and spot lights.
- Model import through an explicit chosen importer path, such as Assimp or a
  hardened glTF stack, with asset-browser evidence and project/runtime handoff.
- Normal mapping with tangent data and material binding.
- Cubemap / skybox support.
- Billboarding for sprites, icons, particles, and debug helpers.
- Instanced rendering.
- OpenGL Direct State Access style cleanup where available.
- Backend-native sampled render-target allocation, binding, and presentation for
  in-game surfaces such as arcade cabinets and editor previews.

### Shadows And Lighting

- Basic shadow mapping.
- PCF soft shadows.
- Point-light shadow maps.
- Directional-light shadow maps.
- Cascaded shadow maps.
- Polygon offset/depth-bias policy including clamp support where available.

### Deferred And Post Processing

- Deferred shading / G-buffer.
- Screen-space ambient occlusion.
- SSAO depth reconstruction.
- Object motion blur through velocity buffers.
- Toon shading and rim lighting.
- Silhouette/edge detection.
- Stencil shadow volumes.

### Animation, Geometry, And Particles

- Skeletal animation with joint matrices and skinned vertex data.
- Geometry shader path where useful, but do not require it for normal runtime.
- Tessellation control/evaluation or hull/domain shader equivalents.
- PN-triangle tessellation.
- Particle system.
- Transform-feedback particles where OpenGL makes sense; prefer compute/storage
  buffer designs for Vulkan/D3D-style backends.

### GPU-Driven And Diagnostics

- Indirect drawing / GPU-driven batches.
- Pipeline statistics and GPU query plumbing.
- Transform-feedback overflow or stream-output statistics where supported.
- Anisotropic texture filtering.
- Backend debug markers and external tool integration such as Vulkan debug
  utils, PIX-ready D3D markers when D3D is promoted, and OpenGL debug output.

## Cross-API Mapping Rule

When a feature is selected for implementation, record the active backend mapping
before coding:

| Engine feature | OpenGL shape | Vulkan shape | Direct3D shape |
| --- | --- | --- | --- |
| Resource binding | Program uniforms, UBOs, texture units, DSA where available | Descriptor sets, push constants, UBO/SSBO/image descriptors | Root signature, CBV/SRV/UAV/samplers |
| Pipeline | Program + VAO + GL state object discipline | Graphics/compute pipeline objects | PSO + input layout + root signature |
| Render target | FBO/color/depth attachments | Image views + render pass/dynamic rendering/framebuffer | RTV/DSV resources |
| Draw submission | glDraw*, glDraw*Instanced, indirect variants | vkCmdDraw*, vkCmdDrawIndirect* | Draw*, DrawInstanced, ExecuteIndirect |
| Debug/profiling | GL debug output + timer/stat queries | Validation layers + debug utils + query pools | Debug layer + PIX markers + query heaps |

Do not add a feature to only one backend without also documenting the intended
equivalent in this table or in the feature's implementation note.

## Recommended Implementation Order

1. Finish the explicit render-device abstraction and material/light resource
   model around the features Epoch already partially has.
2. Promote render-to-texture assets and model import so editor/project workflows
   can use real assets instead of placeholder surfaces.
3. Add instancing, normal maps, skybox/cubemap, and multiple-light support.
4. Add the first shadow-map path, then PCF and directional/point variants.
5. Add deferred G-buffer and SSAO once render targets and materials are stable.
6. Add skeletal animation and particle systems after model import and buffers
   are settled.
7. Add advanced GPU-driven, query, tessellation, and post-processing features
   only after smoke coverage can prove they do not destabilize the editor.

## Acceptance Gates

- The Systems workspace can report which renderer features are present, partial,
  or missing for each active backend.
- A feature cannot be marked complete until OpenGL and at least one non-OpenGL
  path either pass or explicitly document why they are deferred.
- Renderer features must not regress the editor GUI composition contract: scene
  preview first, latest GUI overlay after it, stable panes, and no hidden
  backend ownership surprises.
- Feature work must update this matrix, the roadmap, and the relevant smoke
  plan before a release note claims support.
