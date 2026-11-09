# Package Security And Import Policy

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

EpochEngineExtensions exists to keep optional package source out of the minimal
EpochEngine clone while still making those packages reviewable and repeatable.

## Server And Network Packages

- Server-capable code must be package-gated.
- A package that can bind a port, host, listen, or expose a network/control
  surface must set `requiresExplicitNetworkApproval` to `true`.
- Package install must not start a server.
- Package build must not start a server.
- Package tests may run local non-network self-tests by default, but any
  listener or port bind requires an explicit human run action.
- Authoritative dedicated server is optional and headless-capable. It is not the
  default multiplayer path.
- Client listen/nondedicated and future competitive client-predicted paths are
  separate packages.

## Source Import Rules

- Record provenance before copying source.
- Keep license status explicit.
- Import the smallest useful source slice.
- Prefer adapter layers over wholesale source-tree dumps.
- Do not promote extension source into EpochEngine core until the API boundary,
  build/test evidence, and package ownership are documented.
- Visual code extensions are compiled package scripts first. Voxel terrain, planetary
  terrain, FFT ocean, and demo previews should materialize under
  `cache/packages/<package-id>/`, build from reviewed source, and register a
  live editor preview through the Package Manager rather than shipping as JSON
  only.
- Forest Factory remains a core EpochEngine feature. Heavy forest datasets,
  demos, or external L-system reference imports may live here, but activation
  should still be project opt-in.

## Cache Layout

Consuming EpochEngine builds should place downloaded or materialized package
payloads under:

```text
cache/packages/<package-id>/
```

Generated atlases and update payloads are separate concerns owned by the engine
runtime cache policy.
