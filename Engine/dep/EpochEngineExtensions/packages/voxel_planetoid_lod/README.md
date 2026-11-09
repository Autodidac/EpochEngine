# Voxel Planetoid LOD

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

Renderer-neutral voxel planetoid LOD source for Epoch extension review.

This package stages the common planetoid field sampler, chunk key policy, and
LOD shell selection that future renderer adapters can consume. It deliberately
exposes value types and deterministic jobs only; GPU upload, bindless resources,
and backend-specific presentation stay outside this package.
