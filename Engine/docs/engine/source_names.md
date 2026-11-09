# Source Naming Architecture

This contract applies to first-party C++ sources, headers, implementation parts
and module interfaces in Engine and engine-owned dependencies, including
`Engine/dep/EpochPlatformEngine` and EpochGui-owned source. Exact third-party
mirrors, generated products and installed dependency trees retain their own
upstream naming. Validation counts and dated receipts belong in active_pass or
the changelog, not this ownership rule.

Use C++23 modules first for domain/public APIs. Keep textual headers where an
actual compiler/global-module-fragment, native OS/library, C ABI or template
consumer requires them. The existing `epoch.app_api.h` callback ABI is a genuine
foreign/plugin boundary. A compatibility header forwards to one owner; it must
not copy an API implementation, state or an entire dependency. Module facades may
expose that existing single ABI during migration. C++26 is an upgrade path after
compiler/standard-library/module-consumer checks, not a premature minimum.
Within each configuration, module producers and consumers must agree on toolset,
language mode, CRT/debug and standard-header/security compilation settings. Keep
Debug/Release IFC and object outputs separate; do not suppress mismatch checks.

## Filename Grammar

Every first-party C++ source, module, header and textual implementation part must
stay at or below 4,000 physical lines. `Tools/source/validate_source_size.py` is
the portable check; its PowerShell entry is `validate_source_size.ps1`. Shared
tests enforce the same limit, including engine-owned dependency code. Exact
third-party mirrors and generated build/runtime output are outside this rule.
Feature-owned `.inl` files keep private state and definition order in their
existing translation unit and appear under **Implementation Files** in MSVC.
They improve physical ownership; they do not by themselves establish a separate
library, repository or independently validated platform target.

Prefer real module implementation units or owned `.cpp` files for new behavior.
Do not split a large function into more textual includes just to pass the line
limit. Existing fragments are migration debt where they depend on surrounding
local variables, anonymous types or declaration order. Extract a cohesive typed
boundary first, then compile its implementation independently and remove the old
fragment in the same validated change. Templates, genuine global-module-fragment
requirements and the current physically private edition extraction may retain
textual inclusion where a reviewed alternative is not yet available. Preserve
the private/public source boundary during that extraction; renaming `.inl` to
`.cpp` while still including it does not complete the work.

Every first-party C++ filename has exactly one ownership separator:

```text
<owner>.<subject_role>.<extension>
```

Rules:

- `owner` identifies the namespace, subsystem, backend, or product surface;
- `subject_role` uses lowercase words separated by underscores;
- the filename stem, excluding its final extension, contains one ownership dot;
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
domain that owns the behavior. `engine.updater` remains a compatibility identity; updater/channel changes follow
the explicit distribution and verification contract rather than cosmetic renames.

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

- Engine external interfaces: `Engine/include` and `Engine/modules`;
- dependency interfaces: that dependency's `include` and `modules` directories;
- internal implementation: the owning component's `src/<owner>`;
- no first-party implementation/header files remain directly at `Engine/src`;
  Engine-only primitives use `src/core`, composition uses `src/epoch`, Engine
  build helpers use `src/build`, platform adapters use `src/platform`, and backend-
  neutral rendering helpers use `src/render`;
- renderer implementation: `Engine/src/renderers/<backend>`;
- reusable Platform implementation/interfaces: `Engine/dep/EpochPlatformEngine`;
  the shared C++ feature probe belongs to its `include` directory;
- reusable GUI implementation: `Engine/dep/EpochGui`; Engine-specific adapters
  and not-yet-extracted GUI integration remain under `Engine/src/epochgui`;
- project services: `Engine/src/project`;
- authoring implementation: `Engine/src/authoring/<domain>`;
- scripts and scripting compiler: `Engine/src/scripts`.

Moves update CMake, MSVC projects, shared items, filters, includes, imports,
scripts, and documentation in the same commit.
Visual Studio filters follow the physical implementation, owning component
module or public-interface directory. `Tools/source/update_source_filters.ps1 -Write`
regenerates this view without changing compilation conditions; omit `-Write`
for a check. `Tools/source/validate_source_layout.ps1` rejects new flat implementations
and missing CMake/MSVC source references. `Engine/dep/dependencies.json` distinguishes exact upstream mirrors from
engine-owned library targets. `Tools/source/validate_dependency_sync.ps1` checks
that authority; an Engine adapter edit does not prove a standalone repository or
Site was synchronized. Platform/GUI extractions require their own standalone
consumer and Engine-integration proof, not only a folder or project reference.

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
.\Tools\source\validate_source_names.ps1
```

The naming/layout/size validators together check first-party ownership, with
engine-owned dependency sources included in the size gate. The filename rule is
lowercase stems with one ownership dot, underscore subject separation, and exact
module/file identity. A rename is not
complete until Debug and Release builds, the engine contract, and the active
CMake contract lane pass.
