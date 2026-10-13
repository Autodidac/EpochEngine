# EpochEngine local 0.90.41 — AI guidance / observability source candidate

Parent: exact `EpochEngine-0.90.40-full-source.zip`, no `.git` history.
Published runtime channel remains 0.90.35; this source is not released.

## Implemented in this source candidate

- `Engine/docs/engine/ai_task_recipes.md` names source owners and bounded
  edit/build patterns for common theme, self-coding, model, chat and build tasks.
- `editor.ai_documentation_guide.hpp` reads only one fixed source-owned document
  under the admitted source root and extracts only the matching named section,
  with canonical containment, symlink and input-size checks. It is a read-only
  preflight; exact source evidence still grants edit authority.
- The AI development panel reserves a small amount of prompt space and appends
  this guide for applicable Engine-source coding requests. It neither changes
  the source budget’s safety authority nor disables the discovery watchdog.
- Epoch's existing `AiChat` now retains distinct status blocks, coalescing
  only identical consecutive statuses. It preserves a bounded separate history
  of completed raw model responses and exposes a native `Raw Output` / `Chat View`
  toggle. No raw response can act as a tool call without protocol validation.
- The existing EpochGui role colors now distinguish Agent errors/warnings/
  successes from regular agent statuses. Native chat/user retry behavior remains.
- `Changes/roadmap.md`, `mission_cache.md`, `active_pass.md` and Engine/docs
  record future native-model and agentic parity work without closing it.
- Only the local source and Windows editor resource version advance to 0.90.41;
  WinResource.rc stays ASCII/BOM-free.

## Not yet implemented or accepted

- Native Qwen Next Flash in-process runtime and verified model-family adapters.
- Actual loaded GPU offload telemetry. Current offload is not considered broken.
- Real-time raw token streaming, durable paginated chat history, all requested
  composer features and full semantic action-color coverage throughout the UI.
- Project-independent AI SDK consumer builds and EngCoder-equivalent task
  harness/subagent features.
- MSVC complete solution build or a Qwen-driven theme edit / sandbox successor.

## Verification required before reporting success

1. Debug, Release and ReleaseWithDebugInfo MSVC builds; full pure contract
   suite, resource compiler, and actual native EpochGui display.
2. Start a fresh GUI-theme objective; confirm recipe selection, actual source
   edit in the sandbox, compilation and an independent candidate preview.
3. Verify raw-history toggle, ordered retained statuses, source warning/error
   colors, Stop/Cancel/Send and unchanged user model/GPU settings.
4. Separate true native model-load and GPU telemetry tasks from this source
   candidate; do not claim model support from an HTTP connection.

The complete provider-neutral design and staged acceptance matrix are in
`Engine/docs/engine/ai_model_runtime_and_chat_plan.md`.
