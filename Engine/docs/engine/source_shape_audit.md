# Source Shape Audit

This document records the current organization status. The normative naming and
ownership rules are in `source_naming_architecture.md`; the product sequence is
in `capability_tier_architecture.md` and `Changes/roadmap.md`.

## Current State

The first-party C++ tree now follows one filename grammar across
`Engine/modules`, `Engine/src`, and `Engine/include`:

```text
<owner>.<subject_role>.<extension>
```

The basename has exactly one ownership dot. Additional subject words use
underscores. Module partitions use `:` in the module identity and `_` in the
filename. `Tools/ai/validate_source_names.ps1` enforces the rule and verifies
module-interface identity.

Mature implementation areas are physically owned:

- renderer code is under `src/renderers/<backend>`;
- reusable GUI code is under `src/epochgui` and `dep/EpochGui`;
- editor, project, authoring, asset, AI, platform, script, physics, and audio
  code live in their named folders;
- public headers remain under `include`;
- module interfaces remain under `modules`.

CMake, Visual Studio shared items, projects, and filters are equal build
surfaces. A source move updates all of them in the same pass.

## Architectural Boundaries

`core` owns dependency-light primitives and lifecycle contracts. `epoch` owns
public engine composition and runtime entry. Domain owners retain their own
implementation and state.

The temporal resource direction is:

```text
authoring document and semantic history
-> portable compiled asset
-> project revision admission
-> disposable renderer residency
```

Current texture ownership follows that direction through
`authoring.texture`, `asset.texture_artifact`,
`project.texture_resources`, `render.texture_artifact`, and
`render.texture_residency`.

`project.lifecycle` centralizes Save, project-shell materialization, Build, Run,
wait, and focus-existing-runtime decisions. Editor widgets remain callers of
that policy rather than independent project state machines.

## Compatibility

Compatibility entry points may remain when external callers require them, but
they must be thin and explicitly named. Generic bridge terminology is retired
in favor of adapter, transfer, facade, host, capture, presentation, serializer,
or legacy-runtime roles.

The published updater implementation remains sealed. Its compatibility module
name is not permission to reorganize updater behavior during ordinary source
work.

## Verification Gate

A source-shape pass is complete only when:

1. the naming validator passes for the entire first-party C++ tree;
2. no CMake, MSVC project, shared-item, filter, include, or import path is stale;
3. Debug and Release editor targets compile;
4. build-safe engine and focused subsystem contracts pass;
5. documentation names the canonical owner and remaining delivery work.

Generated dependency/install trees are disposable. If a mechanical operation
touches one, regenerate it from the manifest before trusting compiler output.
