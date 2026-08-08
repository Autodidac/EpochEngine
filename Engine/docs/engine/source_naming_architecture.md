# Source Naming Architecture

This contract applies to every first-party C++ source, header, inline
implementation, and module interface under `Engine/src`, `Engine/include`,
and `Engine/modules`. Vendor sources, generated output, build products, and
installed dependency trees are excluded.

## Filename Grammar

Every first-party C++ filename has exactly one ownership separator:

```text
<owner>.<subject_role>.<extension>
```

Rules:

- `owner` identifies the namespace, subsystem, backend, or product surface;
- `subject_role` uses lowercase words separated by underscores;
- the basename contains exactly one dot;
- hyphens and additional ownership dots are not permitted;
- a file moves into the directory owned by its subsystem as that subsystem
  matures.

Examples:

```text
opengl.canvas2d_scene.cpp
opengl.context_process_impl.hpp
render.canvas2d_scene.ixx
project.texture_resources.ixx
editor.application.cpp
core.context_backends.hpp
```

A filename does not repeat its full directory path. The directory establishes
physical ownership; the prefix establishes searchable logical ownership.

## Module Identity

A primary module uses the same one-dot grammar as its interface filename:

```text
export module render.canvas2d_scene;
render.canvas2d_scene.ixx
```

A module partition uses a colon in C++ and an underscore in its filename:

```text
export module vulkan.context:runtime;
vulkan.context_runtime.ixx
```

The filename validator converts `:` to `_` and requires an exact match.
Imports always name the exported module, never a compatibility filename.

## Owner Vocabulary

- `core`: dependency-light primitives, lifecycle contracts, handles, logging,
  errors, math, environment, and command infrastructure.
- `epoch`: public engine composition, runtime entry, version, and CLI facade.
- `platform`: operating-system policy and portable host abstractions.
- `render`: backend-neutral rendering contracts, resources, graphs, lighting,
  rays, visibility, and presentation.
- `opengl`, `vulkan`, `directx`, `software`, `raylib`, `sdl`,
  `sfml`: backend-owned implementation and capability adapters.
- `gui`: reusable EpochGui-facing controls and engine GUI composition.
- `editor`: editor application, workspaces, commands, and editor-only
  orchestration.
- `project`: project identity, persistence, materialization, build, run, and
  runtime resource admission.
- `asset`: compiled, portable runtime artifacts.
- `authoring`: canonical editable documents and semantic history.
- `scene`, `ecs`, `physics`, `audio`, `simulation`, `game`,
  `scripting`, `ai`, and `network`: their named domain contracts.

The old `engine.*` prefix is not a general-purpose owner. New code chooses the
domain that owns the behavior. `engine.updater` remains a sealed compatibility
identity until the published updater gate is explicitly reopened.

## Core And Epoch

`core` can be consumed without constructing the engine product. It must not
silently acquire editor, renderer, updater, project, or package policy.

`epoch` composes public engine behavior from domain modules. It may coordinate
`core`, platform, project, renderer, and editor services, but it does not absorb
their implementation merely to create a central name.

This gives the dependency direction:

```text
core and domain contracts
        -> subsystem implementations
        -> epoch composition
        -> application/editor entry
```

## Temporal Layer Ownership

The temporal architecture is reflected directly in names:

```text
authoring.*
    canonical editable document and semantic operations

asset.*
    compiled portable artifact

project.*
    project identity, source revision admission, and runtime packaging

render.*
    disposable physical residency and backend execution
```

For textures this means:

```text
authoring.texture
    editable layers, tiles, operations, branches, checkpoints

asset.texture_artifact
    deterministic compiled mips, serialization, hashes, validation

project.texture_resources
    authenticates project source revisions and publishes runtime references

render.texture_artifact / render.texture_residency
    creates and retires backend-local physical resources
```

Descriptor indices, atlas coordinates, sparse pages, backend handles, and upload
buffers never become canonical authoring state.

## Directory Ownership

- public external headers: `Engine/include`;
- module interfaces: `Engine/modules`;
- internal implementation: `Engine/src/<owner>`;
- renderer implementation: `Engine/src/renderers/<backend>`;
- reusable GUI implementation: `Engine/src/epochgui` and
  `Engine/dep/EpochGui`;
- project services: `Engine/src/project`;
- authoring implementation: `Engine/src/authoring/<domain>`;
- scripts and scripting compiler: `Engine/src/scripts`.

Moves update CMake, MSVC projects, shared items, filters, includes, imports,
scripts, and documentation in the same commit.

## Compatibility Policy

A compatibility surface is allowed only when an external caller still needs it.
It must:

- name the legacy role honestly;
- forward into one canonical owner;
- contain no duplicate state or implementation;
- have a removal condition in the active plan.

Generic `bridge` names are not ownership. Use the actual role: adapter,
transfer, facade, host, serializer, capture, presentation, or legacy runtime.

## Enforcement

Run:

```powershell
.\Tools\ai\validate_source_names.ps1
```

The validator checks all first-party C++ files for lowercase one-dot names,
underscore subject separation, and exact module/file identity. A rename is not
complete until Debug and Release builds, the engine contract, and the active
CMake contract lane pass.
