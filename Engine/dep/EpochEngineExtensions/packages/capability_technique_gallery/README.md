# Capability Technique Gallery

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

`capability_technique_gallery` is an opt-in, renderer-neutral extension scene
plan. It gives each staged rendering technique a stable catalog entry and a
labeled station in a deterministic gallery layout.

The package does not import OpenGL platform, context, resource, shader, or asset
code. EpochEngine remains responsible for resolving a logical provider contract
to an implementation and for deciding whether build/runtime evidence is strong
enough to run that provider.

## Tier Meaning

- `T0-CPU` is CPU/headless execution only. It never implies presentation.
- `T1-GLES` and `T1-GL` are the first portable presentation tiers.
- T1 compute, storage buffers, indirect drawing, tessellation, timing, and
  bindless features require separate validated feature evidence.
- `T2-VK` and `T2-DX` are equivalent explicit graphics/compute baselines.
- `T3-VK/DX` adds individually proven advanced explicit capabilities.
- `T4-VK/DX` adds inline hardware ray queries.
- `T5-VK/DX` adds full hardware ray-tracing pipelines.

A tier is not an API preference. `select_route` follows each technique's
authored provider alternatives and rejects unvalidated profiles or missing
feature bits.

## Evidence Rules

`Reference only` means the technique is implemented in
[`tiered_gfx_OpenGL_modular_context_demo`](https://github.com/Autodidac/tiered_gfx_OpenGL_modular_context_demo).
It does not mean the technique works in EpochEngine.

`Deferred` means the gallery creates a labeled station and fallback plan, but
will not activate a provider.

`Capability gated` means activation requires a matching tier, explicit feature
evidence, and a separately proven EpochEngine implementation.

No technique in this package is marked `Present`. That status belongs to
EpochEngine's runtime evidence system.

## Scene Contract

`make_scene_plan()` produces one stable station per catalog entry. Stations are
grouped by foundation, materials, lighting, geometry, simulation, post process,
diagnostics, and spatial work. Each station contains:

- a stable technique index and ID;
- deterministic world position;
- owning engine context;
- eligible provider routes and minimum tiers;
- dependency and fallback descriptions;
- evidence scope and status;
- a scene intent suitable for a future live preview.

The disposition is `Profile eligible`, `Fallback only`, or `Deferred`.
Profile eligibility only proves that the supplied profile can satisfy the
route. It does not promote reference code into an EpochEngine implementation.

## Technique Coverage

The catalog preserves all 44 reference-lab entries and adds two architecture
stations for the T4 and T5 contracts.

| Technique | Owner | Minimum routes | Status | Fallback |
|---|---|---|---|---|
| `platform.desktop.gl45` | `context.driver` | T1-GL, T2-VK/DX | Reference only | Selected Epoch context or T0-CPU headless |
| `resource.explicit` | `render.resource` | T0-CPU reference, T1-GLES/GL, T2-VK/DX | Reference only | Logical CPU resources and disposable caches |
| `target.hdr` | `render.frame_graph` | T1-GLES/GL or T2-VK/DX plus HDR-target evidence | Reference only | Clamped LDR target |
| `target.rtt` | `render.frame_graph` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Direct primary-view render |
| `texture.srgb` | `render.material` | T0-CPU reference, T1-GLES/GL, T2-VK/DX | Reference only | Explicit linear RGBA |
| `texture.mip.aniso` | `render.material` | T1-GLES/GL, T2-VK/DX | Reference only | Bilinear mip or nearest sampling |
| `material.pbr` | `render.material` | T1-GLES/GL, T2-VK/DX | Reference only | Blinn-Phong or unlit |
| `material.blinn` | `render.material` | T0-CPU reference, T1-GLES/GL, T2-VK/DX | Reference only | Lambert or unlit |
| `material.normal` | `render.material` | T1-GLES/GL, T2-VK/DX | Reference only | Geometric normals |
| `material.parallax` | `render.material` | T1-GL, T2-VK/DX | Reference only | Normal mapping |
| `material.clearcoat` | `render.material` | T1-GL, T2-VK/DX | Reference only | Fold coat into base specular |
| `material.transmission` | `render.material` | T1-GL, T2-VK/DX | Reference only | Alpha or opaque dielectric |
| `material.alpha` | `render.material` | T1-GLES/GL, T2-VK/DX | Reference only | Opaque geometry or sorted alpha |
| `light.directional` | `render.lighting` | T0-CPU reference, T1-GLES/GL, T2-VK/DX | Reference only | Ambient or unlit |
| `light.point` | `render.lighting` | T1-GLES/GL, T2-VK/DX | Reference only | Nearest light or baked emissive |
| `light.spot` | `render.lighting` | T1-GLES/GL, T2-VK/DX | Reference only | Point light or emissive fixture |
| `light.projector` | `render.lighting` | T1-GL, T2-VK/DX | Reference only | Plain spot light |
| `shadow.directional.soft` | `render.shadow` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Bounded PCF, blob, or none |
| `environment.sky` | `render.environment` | T1-GL, T2-VK/DX | Reference only | Gradient and ambient probe |
| `geometry.billboard` | `render.geometry` | T1-GLES/GL, T2-VK/DX | Reference only | CPU quads or crossed planes |
| `geometry.instancing` | `render.geometry` | T1-GLES/GL, T2-VK/DX | Reference only | Bounded draws or merged geometry |
| `geometry.indirect` | `render.geometry` | T1-GL or T2-VK/DX plus indirect evidence | Reference only | Ordinary instancing |
| `geometry.tessellation` | `render.terrain` | T1-GL or T2-VK/DX plus tessellation evidence | Reference only | CPU or vertex-displaced grid |
| `geometry.pn` | `render.terrain` | T1-GL or T2-VK/DX plus tessellation evidence | Reference only | Source or pre-subdivided mesh |
| `simulation.object` | `simulation.timeline` | T0-CPU | Reference only | Static transform |
| `simulation.cloth` | `simulation.physics` | T0-CPU | Reference only | Static or skeletal cloth |
| `simulation.water` | `simulation.water` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Opaque animated water |
| `simulation.particles` | `simulation.particles` | T0-CPU | Reference only | Bounded CPU emitter or static card |
| `post.hdr` | `render.post` | T1-GLES/GL or T2-VK/DX plus HDR-target evidence | Reference only | Gamma-correct LDR |
| `post.bloom` | `render.post` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Emissive color without bloom |
| `post.fxaa` | `render.post` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | MSAA, native resolution, or none |
| `post.ssao` | `render.post` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Material/baked AO or none |
| `post.fog` | `render.environment` | T1-GLES/GL, T2-VK/DX | Reference only | Background and LOD fade |
| `post.toon` | `render.material` | T1-GLES/GL, T2-VK/DX | Reference only | Standard lit or unlit |
| `debug.views` | `editor.diagnostics` | T0-CPU reference, T1-GLES/GL, T2-VK/DX | Reference only | Text values and CPU samples |
| `debug.gl` | `context.diagnostics` | T1-GL | Reference only | Backend-neutral visible validation |
| `debug.gpuquery` | `performance.evidence` | T1-GL or T2-VK/DX plus timing evidence | Reference only | CPU frame spans |
| `shadow.point` | `render.shadow` | T1-GLES/GL or T2-VK/DX plus render-target evidence | Reference only | Unshadowed point light |
| `shadow.cascade` | `render.shadow` | Staged T1-GLES/GL, T2-VK/DX | Deferred | Single directional map |
| `render.deferred` | `render.frame_graph` | Staged T1-GLES/GL, T2-VK/DX | Deferred | Forward or forward-plus |
| `animation.skinning` | `simulation.animation` | Staged T1-GLES/GL, T2-VK/DX | Deferred | CPU skinning or static pose |
| `particles.compute` | `simulation.particles` | T1-GLES/GL or T2-VK/DX plus compute and storage evidence | Deferred | CPU particles |
| `selection.outline` | `editor.selection` | T0-CPU query or staged T1/T2 presentation | Deferred | CPU ray/AABB and inspector state |
| `bindless.texture` | `render.resource` | T1-GL bindless or T3-VK/DX descriptor indexing | Capability gated | Bounded tables, atlases, or ordinary bindings |
| `spatial.ray_query` | `spatial.query` | T4-VK/DX plus ray-query evidence | Capability gated | CPU ray, software BVH, or compute traversal |
| `render.ray_pipeline` | `render.ray_pipeline` | T5-VK/DX plus RT-pipeline evidence | Deferred | Raster, compute, or CPU reference |

Dependencies and full scene intents are canonical in the C++ catalog rather
than duplicated into a second machine-readable data file.

## Build And Test

```powershell
cmake -S packages/capability_technique_gallery `
  -B cache/packages/capability_technique_gallery/build `
  -DBUILD_TESTING=ON
cmake --build cache/packages/capability_technique_gallery/build `
  --config Release `
  --target epoch_extension_capability_technique_gallery_self_test
ctest --test-dir cache/packages/capability_technique_gallery/build `
  -C Release --output-on-failure
```

The self-test checks catalog completeness, stable IDs, route/tier consistency,
feature-gated T1 compute, rejection of unvalidated profiles, deterministic
route selection, deterministic station placement, and the rule that deferred
techniques never activate.

## Safety

- Package Manager activation is required.
- No server, listener, network surface, or download path exists.
- No native code auto-runs.
- No backend owns canonical authoring state.
- No reference shader, platform shell, GL resource owner, or curated asset is
  copied into this package.
