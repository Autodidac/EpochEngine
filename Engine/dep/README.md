# Dependency ownership and synchronization

These directories are tracked snapshots/fixtures inside EpochEngine, not nested
Git repositories or submodules. Running `git status` here reports Engine's tree,
not an independent dependency checkout. `dependencies.json` records the roles.

| Directory | Authority | Sync rule |
| --- | --- | --- |
| EpochGui | Standalone `Autodidac/EpochGui`; exact bundled mirror | Publish/test portable library changes in the standalone repository, verify identical tree bytes, then update the revision/tree receipt together |
| EpochEngineExtensions | External `Autodidac/EpochEngineExtensions` owns optional package source; bundled catalog/terrain files are engine-owned compatibility fixtures | Do not replace the standalone package tree with this small fixture or copy all optional packages into core. Check package/API compatibility and update each owner intentionally |
| EpochPackageDescriptors | Engine-owned model/package metadata | Review source/identity/license/admission changes in Engine; never bundle weights or infer execution permission |

`Engine/src/epochgui` and `gui.engine` are engine adapters, not portable EpochGui
library code. A change there alone does not require rewriting the standalone GUI
repository. EngCoder currently remains an explicitly selected external endpoint;
there is no silently bundled dependency or full-agent source authority.

Run `Tools/ai/validate_dependency_sync.ps1` for the offline clean-mirror gate.
Add `-Remote` to check the recorded GitHub authority. This never pushes, downloads
packages, updates refs or starts a service. Upstream drift requires review, not
automatic replacement. A dirty mirror requires standalone verification and an
updated exact receipt before an Engine checkpoint is called synchronized.

October 7 audit: EpochGui's bundled and upstream trees both equal
`4f8fd154151cace2ecaea4aa69532daf4fff73bb` at
`c52e91293749a7a34959bfc042c582c3ac182504`. Extensions upstream main is
`eccef139f982a9e6b64295cd9c8b976905ef0990`; its tree differs from the deliberately
smaller Engine fixture. No dependency source edits or upstream pushes are needed
for the Engine-only folder/input/AI-status repairs in this pass.

October 10 reconciliation: Extensions v0.2.1 upstream is now
`a003bcab58a859e170edb79ca8e63ed15707b2d5`, tree
`986d824a88a5d2b2bdcc24fc81b3a94fe76e1df4`. It joins both GitHub eccef139 and
Site 16007d4b histories, preserving all ten package source trees and the Site
tiered-terrain module/contract/notices. All 13 CPU contracts pass in Debug and
Release with authoritative current Engine modules. Existing v0.1.0/v0.2.0
tags and archives remain immutable; publication is recorded in
`Changes/source_sync_2026-10-08.md`.

The `engine_owned_descriptor_fixture` record's revision identifies the reviewed
external source; its tree identifies the deliberately smaller, unchanged Engine
fixture, not the upstream repository tree. No optional source trees are copied
into core, and source publication does not activate packages or grant execution.
