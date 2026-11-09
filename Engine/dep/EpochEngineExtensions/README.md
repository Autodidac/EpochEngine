# Epoch Engine Extensions

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

Optional package source, package manifests and bounded demonstrations for EpochEngine.
Current companion source version: `v0.2.3` (local delivery preparation).

EpochEngine keeps the lean core runtime, editor, package descriptors, security
gates, cache/update plumbing, and stable API contracts. This repository is the
review home for bulky optional packages that should not be copied into every
software project, single-player game, or minimal generated engine clone.

## Core Compatibility

- Selected Public EpochEngine source: `v0.90.183`, commit
  `61c835459f7ed8bc72994376c6966574b0c4949c`.
- This companion release is optional source; it does not select or replace an
  installed Engine runtime. Windows Debug/Release CPU checks use that exact
  Public source's `render.math` and `terrain.foundation` modules.
- These identities describe the selected host, not blanket package/runtime
  compatibility proof. Package-specific API, build and admission checks still apply.
- The optional cloud-density exporter/controller additionally requires the Engine
  `asset.cloud_volume` and v5 planetary-snapshot contracts prepared for 0.90.185.
  They are not supplied by the 0.90.183 compatibility baseline above. CPU transport
  tests do not establish native cloud appearance or support in every renderer.
- Renderer contexts, sampled render-to-texture, EpochGui, updater behavior, and
  release packaging remain engine-owned contracts. Extensions consume those
  contracts and do not replace or fork them.

## Package Boundary

- EpochEngine mainline owns stable interfaces and inert package descriptors.
- This repository owns optional package source, provenance records, and package
  integration manifests.
- JSON manifests are package discovery and policy metadata, not the live
  implementation path for visual/source packages.
- Visual/source packages use C++23 `.ascript.cpp` package runners. Those
  runners describe the human-gated fetch, cache, build, and live-preview path
  that Epoch's Package Manager can compile and invoke.
- Runtime downloads and generated package payloads belong under the consuming
  editor/runtime executable folder at `cache/packages/`.
- Packages that can bind a port, host, listen, expose a server, or create a new
  network/control surface require explicit human approval before build or run.
- Authoritative dedicated server support is optional. It is not the default
  networking model for games, and it must not be injected into unrelated
  software or single-player projects.

## Source-Staged Code Extension Tracks

- `engine_network_runtime`: inert shared network/session contracts and packet
  frame/policy self-tests.
- `engine_authoritative_dedicated_server`: optional headless authoritative
  server planning package for projects that deliberately choose that path.
- `engine_client_listen_server`: client-hosted listen/nondedicated server
  planning path.
- `voxel_planetoid_lod`: renderer-neutral voxel planetoid field and chunk LOD
  package.
- `voxel_authoring_tools`: sparse voxel authoring and brush replay package.
- `forest_temporal_graph`: optional Forest Factory temporal graph package.
- `planetary_terrain`: planetary biome/tile composition package.
- `fft_ocean`: FFT ocean simulation package.
- `capability_technique_gallery`: renderer-neutral labeled scene/catalog for
  reference techniques and T0-CPU through T5-VK/DX provider routes. Reference
  implementations remain reference-only until EpochEngine records its own
  build and runtime evidence.

Each source-staged package has a package-local `CMakeLists.txt`, public C++23
header, source file, and self-test target. Package code is intentionally
renderer-neutral unless the package name says otherwise.

## Models and project assistance

The local-model package is an offline **project runtime plan**, not an Engine
model installer or a bundle of model weights. Install/select an Engine model in
the Engine's Model Manager. Project models have their own licenses and opt-in.
This optional package catalog does not grant access to the Engine's source or
provide its private self-coding component.

## Validate

The reconciled root build includes the preserved ten source packages and the
Site-origin tiered-terrain module. Select the authoritative Engine modules;
substitute terrain definitions are never generated or copied here.

```powershell
cmake -S . -B build -DEPOCH_ENGINE_MODULE_DIR=C:/path/to/EpochEngine/Engine/modules -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Windows, use a short absolute build directory if the checkout is deeply
nested: MSVC's generated module/tracker paths can otherwise exceed its path
limit. A single-node build (`--parallel 1 -- /nodeReuse:false`) also avoids
MSBuild worker-IPC restrictions in constrained build environments.

The Windows CI workflow checks the same two configurations against the pinned
Public source. It does not run an Editor, model, listener or renderer. Linux,
macOS and native Package Manager activation remain separate acceptance gates.

For the manifest-only check:

```powershell
.\run_validate_packages.ps1
```

The validator is intentionally small and standard-library only. It checks that
package manifests are present, deterministic JSON, security-gated where needed,
aligned with EpochEngine package policy, and that source/visual packages point
at a real C++23 package runner script.

## Demonstration Catalog And Provenance

`demonstrations/tiered_terrain` adds deterministic bounded local heightfields
through the host's `terrain.foundation` and `render.math` modules. Its own README,
license, source and contract remain separate from the older package trees.

`catalog/extensions.json` retains the eight stable Engine-facing identities and
their admission states. It is not an exhaustive listing of the ten preserved
package trees: their manifests and source remain under `packages/`. A source
archive does not activate a package, prove native renderer integration, install
a model, or authorize a server/listener.

The GitHub and Site histories are joined, not rewritten; previous `v0.1.0` and
`v0.2.0` tags and published archives remain immutable. See
[source reconciliation](docs/source_reconciliation.md) for the exact parents,
ownership and validation boundary.

## Source packages

After committing the checked source, create matched source ZIP/TAR archives and
checksums with the standard-library packer:

```powershell
python tools/package_source.py --output C:/path/to/checkout-local/package-staging
```

It exports committed files only and verifies every ZIP/TAR file body. Git
history, build outputs, model weights and private runtime data are excluded.
The archive is source, not an installed extension or a runnable Engine release.
Packaging requires this source owner's physical `.git` directory and the exact
Site origin `https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git`.
A detached source folder cannot discover an enclosing Engine repository.

## License

Repository files are licensed under the same `LicenseRef-MIT-NoSell` license as
EpochEngine unless another license is explicitly stated. Package manifests
describe package source provenance separately; a manifest license does not
relicense upstream source.
