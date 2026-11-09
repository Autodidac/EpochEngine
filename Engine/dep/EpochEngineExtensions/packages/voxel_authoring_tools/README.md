# Voxel Authoring Tools

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

Renderer-neutral voxel authoring support for Epoch package experiments.

This package owns editor-side authoring primitives: sparse density edits,
material stamping, brush replay, bounds tracking, and deterministic smoke tests.
It intentionally does not own renderer handles, GPU upload paths, or a runtime
backend. Those belong to Epoch renderer adapters after the package produces
visible build and preview evidence.
