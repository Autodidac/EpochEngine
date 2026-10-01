# Epoch AI Project Assistant routing repair - v0.90.19

## Problem

Project Assistant conversation, project authoring, guarded project tools, and Engine Self-Coding shared one completion counter and inferred reply ownership from pending booleans or response text. A reply could therefore be consumed by the wrong parser, goal invalidation could mark unrelated work obsolete, and ordinary Project Assistant chat was sent without the active-project evidence implied by the UI.

## Repair

- Carry an explicit host-owned request channel (`Chat`, `Tooling`, `Authoring`, `SourceIteration`) from deferred admission through the `AiChat` worker and completion pump.
- Parse a completion only with the parser belonging to its originating channel. Ordinary chat never enters authoring/tool/source parsing even if the model mentions an Epoch protocol marker.
- Tag local HTTP and external MCP source completions explicitly as `SourceIteration`.
- Build a bounded Project Assistant context envelope containing active project/build status, scene revision, world/GUI counts, GUI-document state, active script, and selected object. The envelope is informational only and grants no mutation authority.
- Route detached and docked Project Assistant chat through the same canonical context builder.
- Goal pause/edit/delete cancels only the exact active/deferred goal-authoring request. It does not cancel or obsolete ordinary chat, tooling, or self-coding work.
- Surface Project Assistant queue/start/wait state in the transcript so retained work no longer appears inert.

## Validation

- C++23 request-channel/project-context contract compiled and passed with Clang 17 and GCC 14 under Epoch-like string overload pressure.
- GCC 14 `epoch_ci_headless` build passed.
- `epoch_texture_artifact_contract` built and both registered headless CTests passed.
- Full Windows/MSVC editor compile and live local-model exercise remain operator validation.
