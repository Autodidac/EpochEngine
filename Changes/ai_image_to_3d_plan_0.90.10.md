# Epoch AI text -> image -> 3D asset plan

Status: design plan only for v0.90.10. The current reliability pass fixes AI Assistant and source self-iteration; it does not yet vendor or execute image/3D generators.

## Goal

After the required local model packages have been installed and accepted once, the operator should be able to type a single request such as:

> make me a bike model

Epoch AI should then complete the entire asset job without a follow-up prompt:

1. interpret the request as a 3D asset-generation intent;
2. synthesize an image suitable for image-to-3D conditioning;
3. reconstruct a textured 3D asset from that image;
4. validate and post-process the mesh/materials;
5. import it into the active Epoch project;
6. return the generated asset, preview image, and generation metadata to the conversation.

The assistant may report failures or missing prerequisites, but normal asset jobs must not stop to ask for seed, resolution, format, topology target, or intermediate-image approval. Those are policy defaults with optional advanced overrides.

## Architecture

Keep orchestration in EpochEngine, not EngCoder. EngCoder can remain a coding/planning provider, but asset generation belongs beside Epoch's package manager, GPU/runtime selection, job progress, asset database, renderer, and import pipeline.

Add platform-neutral C++23 boundaries under `epochengine::ai`:

- `epochengine::ai::assets` - intent, job graph, policy/defaults, progress, cancellation, provenance, result bundle.
- `epochengine::ai::image_generation` - text/image generation interface and `stable-diffusion.cpp` adapter.
- `epochengine::ai::model_generation` - image-to-3D interface and `trellis.cpp` adapter.
- `epochengine::assets` - final GLB/material/texture validation and project import; generation code must not own renderer/editor state directly.

Backend-specific implementation files stay separate from the interfaces. CUDA, Vulkan, HIP/ROCm, Metal, and CPU capability selection must be runtime/configuration policy rather than preprocessor logic leaking into editor code.

## Production backend

### Text -> image

Use `stable-diffusion.cpp` as the native C/C++ inference runtime. It supports FLUX.2 Klein and CPU/CUDA/HIP/Metal/Vulkan backends. Epoch already has `os_model_flux_2_klein_4b` package metadata, so make that the first cross-platform image-generation package instead of depending on the current MLX-only Bonsai weight variants on Windows/Linux.

The image policy should create a reconstruction-friendly object shot automatically: centered complete object, uncluttered/neutral background, no cropping, useful three-quarter perspective, consistent lighting, no text/logos unless requested, deterministic seed recorded in metadata.

### Image -> 3D

Use `trellis.cpp`, the native C++/GGML port of Microsoft TRELLIS.2, as the initial production backend. It performs background removal, conditioning, 3D generation, mesh extraction, UV/texturing and GLB export without Python at runtime. Prefer direct library integration over starting its HTTP server so Epoch owns lifecycle and progress.

TRELLIS.2 and its model are MIT-licensed upstream at the time of this plan, while every vendored/source dependency and model revision still needs to be captured in Epoch's existing attribution/license inventory before distribution.

### DVR C++23 port

Port the linked Differentiable Volumetric Rendering implementation as an optional `research_dvr` module, not the default asset generator. The port is useful for learning, regression tests, implicit-surface research and future engine-native inverse-rendering work, but a category-trained 2020 single-view DVR implementation is the wrong production path for arbitrary prompt-to-asset generation.

The research port should replace PyTorch/Python infrastructure with explicit C++23 components:

- dataset/camera loader;
- ray generation;
- positional encoding;
- image encoder abstraction;
- occupancy/texture MLP abstraction;
- surface ray marcher + secant refinement;
- implicit-gradient attachment;
- losses/training driver;
- GLB/mesh extraction utility for inspecting learned fields.

Do not make this port a hard dependency of the production TRELLIS path.

## One-command asset job

`Epoch AI` classifies `make me a bike model` into an `AssetGenerationRequest` with defaults such as:

- kind: static textured mesh;
- output: GLB + source PNG + thumbnail + JSON metadata;
- image size: backend-recommended default;
- 3D quality: balanced/local;
- topology: engine-ready postprocessed mesh;
- PBR textures: enabled;
- import: active project generated-assets folder;
- conversation result: asset path + preview + generation summary;
- seed: auto-generated and persisted so the job can be reproduced.

Execution graph:

`natural language -> intent -> image job -> image validation -> TRELLIS job -> mesh/PBR validation -> postprocess -> project import -> preview -> chat result`

No model-facing free-form tool selection is allowed after intent classification. The host constructs this job graph and each backend receives typed arguments.

## Reliability and safety contracts

- Every stage is a cancellable state machine with a stable job ID.
- Generated files first land in an isolated temporary job directory and move into the project only after validation.
- Never overwrite an existing asset implicitly; use deterministic duplicate-safe names.
- Preserve source prompt, resolved prompt, model IDs/revisions, backend/device, seed, settings, hashes, generated image, GLB, textures and validation result.
- Treat model/runtime output as untrusted data: validate dimensions, file sizes, GLB structure, material/texture references, index ranges, finite vertex values and resource budgets before import.
- A failed image or mesh stage reports the concrete failure and keeps useful diagnostics; it does not fabricate a successful asset.
- First-time model download/license consent may require operator action. Once prerequisites are installed/accepted, normal jobs require no follow-up prompting.

## Cross-platform build layout

Add optional CMake features so ordinary Epoch builds remain lightweight:

- `EPOCH_ENABLE_AI_IMAGE_GENERATION`
- `EPOCH_ENABLE_AI_3D_GENERATION`
- `EPOCH_AI_BACKEND_VULKAN`
- `EPOCH_AI_BACKEND_CUDA`
- `EPOCH_AI_BACKEND_HIP`
- `EPOCH_AI_BACKEND_METAL`

Provide matching CMake presets/build wrappers for Windows, Linux and macOS. Do not embed weights in source control. Package descriptors download pinned revisions into Epoch's existing executable-local model cache and keep code/runtime/model licenses separate.

The first supported build matrix should be:

- Windows x64: MSVC/ClangCL + Vulkan default, optional CUDA for NVIDIA and HIP where supported;
- Linux x64: Clang/GCC + Vulkan default, optional CUDA/HIP;
- macOS Apple Silicon: AppleClang + Metal;
- CPU: supported by the image runtime for compatibility; 3D CPU generation may be exposed as a slow fallback only when the selected backend actually supports it acceptably.

## Engine integration phases

### Phase 1 - native image lane

Vendor/pin `stable-diffusion.cpp`, convert the existing FLUX.2 Klein package from metadata-only to executable inference, add typed C++ API, job progress/cancel, image cache/provenance, and an Epoch AI `generate image` intent.

Acceptance: one chat message creates a PNG locally on Windows and imports/displays it without another prompt.

### Phase 2 - native image-to-3D lane

Vendor/pin `trellis.cpp`, map the existing TRELLIS.2 package manifest to its required converted weights, add device capability checks, progress/cancel, GLB output and validation.

Acceptance: a supplied PNG produces a valid textured GLB and Epoch can load/render it.

### Phase 3 - automatic text -> 3D

Add `AssetGenerationRequest` orchestration and policy defaults. The assistant decides from ordinary language that the desired result is a model and automatically chains image generation to 3D generation.

Acceptance: after prerequisites are installed, `make me a bike model` returns an imported bike GLB with preview and metadata with no follow-up prompt.

### Phase 4 - asset quality pass

Add mesh simplification/LOD targets, collision generation, scale/orientation normalization, optional retopology, thumbnail/multi-view QA, material compression, engine naming/tags, and regeneration from recorded metadata.

### Phase 5 - research DVR C++23 module

Port the linked DVR paper implementation behind an optional research build flag with deterministic tests against small fixtures. Keep it independent of the shipping asset-generation path.

## Current-pass boundary

The v0.90.10 reliability patch intentionally stops before these phases. It fixes the AI request/response paths required to safely orchestrate them first: source-protocol recovery, authoring-plan protocol consistency, tolerant single-call recovery, and detached AI Chat routing back to the canonical editor AI session.
