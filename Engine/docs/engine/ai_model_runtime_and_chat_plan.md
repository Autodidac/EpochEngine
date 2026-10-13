# Epoch AI — Native agent harness, model adapters and project reuse

Source design updated October 11, 2026 (local v0.90.41). This is the
**implementation and acceptance contract** for future AI missions. The
[active pass](../../../Changes/active_pass.md) owns the current gate and the
[roadmap](../../../Changes/roadmap.md) owns priority. This design must not be
mistaken for completed runtime implementation.

## Product ownership

EpochEngine owns its AI session identities, permissions, sandbox source
transactions, task queue, tool registry, model routing, service interfaces,
cancellation, host evidence, runtime resources, native chat and project exports.
EpochGui owns its native controls and rendering. No EngCoder HTML/web, SDL
window stack, secondary GUI, or standalone server management UI is imported.
Reuse **behavior and tested runtime algorithms**, not duplicate editors.

The independent EngCoder 0.7.1+ backend is currently only an explicit hosted
inference option. Epoch's current `/v1/models` and `/v1/chat/completions` link
does not establish an in-engine Qwen Next Flash loader or full agent-task API.
A complete engine-owned loader needs the actual QwenFlashRuntime implementation,
its compatible runtime dependencies and contracts; route through an adapter
with explicit version/ABI, without guessing model loader flags. Hosted mode
remains a compatible fallback until native loading is proved.

Projects select and instantiate only needed AI services. Standalone generated
programs must not require EpochEditor, its workspace/cache, the engine's GUI,
EngCoder's GUI, or imported project/private Engine source. They may opt into a
shared model-service process with explicit lifecycle/permissions; they never
silently copy weights, settings, or private session histories.

## Provider-agnostic API (target; not yet shipped as this API)

- `IAiModelBackend`: probe/capabilities, installed-model discovery, load,
  readiness, generate with streaming event sink, cancellation, unload and
  telemetry. The host owns identities/permissions; one adapter owns one loader.
- `AiModelDescriptor`: stable provider/model/revision, format/runtime family,
  tokenizer and context limits; no model ID guesses from display text.
- `AiSession`: selected model plus loaded-generation lease, independent request,
  conversation, actor/tool and cancellation IDs. Server and project share the
  same protocol but never implicit mutable session state.
- `AiEvent`: ordered, timestamped stage events (selected, loading, ready,
  request, received, tool call, source read, patch, compilation, test, review,
  stopped/error) and optional bounded text deltas. Provider progress bytes are
  NOT token counts. Every published event has an owning generation.
- `AiToolExecutor`: named capability, exact path/root, argument/approval
  validation, deadline, stop token and immutable evidence receipt. LLM text
  alone never performs filesystem/process/Git operations.
- `AiTaskCheckpoint`: explicit objective, completed source edits and receipts,
  unresolved steps, next action and owned sandbox. Compact and resume, don't
  rerun exploration or replay approvals. A model response is not a completed edit.

Use static/value-oriented data and composition where helpful; interface/RAII
ownership is justified for asynchronously loaded backends and process leases.
Implement portable core interfaces separately from Windows/GPU-specific
adapters. All publicly reusable symbols/headers/modules are versioned, exported
properly and tested by a standalone consumer.

## Backend matrix and ownership

| Lane | Loader family | Storage and offload owner | Required verification |
| --- | --- | --- | --- |
| Qwen3.8 Flash Next | Dedicated QwenFlashRuntime/manifest, not an ordinary GGUF model assumption | Current EngCoder installed profile in hosted mode; native Epoch adapter after backend integration | Existing install discovered, clean async load/stream/stop, manifest/context/MTP and real GPU use |
| Qwen3.8 27B dense | Matching GGUF-compatible llama.cpp backend/profile, or operator-selected external API | Backend-specific verified model path and saved offload, not shared Flash loader assumptions | Installed revision, loaded layers, context, live output and safe reload |
| Qwen3.6 27B dense | Actual model format/backend determined from installed artifact; do not invent flags | Its backend adapter | Same |
| Qwen3.6 35B-A3B MoE | Actual MoE-capable runtime with supported quantization; independent context/KV budget | Its backend adapter | Same, plus MoE resource telemetry |
| Nemotron 9B | Verified supported format/backend | Its backend adapter | Same |
| Other model or remote endpoint | Capability-based local/remote adapter | Explicit operator profile | Unsupported capabilities hidden/disabled, not silently emulated |

Do not claim the latest model name, backend support, revision or performance
without a real installed manifest and inference receipt. All choices should
appear through **one** Epoch AI Controls UI with the same load, cancel, model,
chat and tool experience, while differing backend-specific options are shown
only where supported.

### GPU-offload diagnostics (investigative; not a regression claim)

Preserve the user's **saved** offload value. Report requested/offload cap,
launched runtime arguments, actually loaded GPU layers, device/VRAM allocation,
KV cache placement, context and backend execution mode separately. Show
`unknown` when telemetry is unavailable; do not synthesize a percentage from
VRAM or overwrite saved preferences. GPU use is provider-owned when hosted.
Compare before/after load under the same model, quantization, context, KV and
backend before modifying settings.

## In-engine ChatGPT-style surface

Native EpochGui conversation pane remains the owner: multiline composer,
Send, Stop, model picker/selector, flat options, tool approval messages,
scroll-to-last, copy/select and retry of user requests. Extend the same widget,
not an extra webview/window or SDL stack.

- Display separately colored `User`, `Assistant`, `Agent`, `Tool`, `Build`,
  `Success`, `Warning` and `Error` blocks from typed session events.
- **Append** meaningful transitions, completed replies, tool results,
  diagnostics and candidate receipts. Coalesce **only identical** adjacent
  statuses; never replace an earlier completed block with newer activity.
  Retention should be bounded, paged and persisted per conversation/project,
  not merely an arbitrarily growing GUI vector.
- Raw Output is opt-in and isolated from action/validation: show current
  streamed content and tool arguments with boundaries, completion/cancellation
  state and measured TPS only when actual token data is supplied. Never label
  byte/event counts as tokens. Preserve the last completed raw response while
  idle, and prior raw blocks across turns; UI may truncate but host evidence
  and selected session must remain bound.
- Show exact selected backend/model and whether it is starting, ready,
  generating, stopped, errored, or externally managed; no false load spinner.
- Reuse EpochGui native theme, DPI handling, font sizes/shadows, selection,
  accessibility and hover/focus semantics. No new window dependency.

### v0.90.41 scope (partial)

Read-only source-owned task recipes are included in AI source prompts when
applicable; they help the model target source owners early but confer no edit
authority. The native conversation now retains distinct activity blocks and
has a Raw Output toggle for **completed** bounded model responses. Existing
role tone mapping colors source milestones by outcome. This is *not* live raw
token delivery, a durable chat-history store, or a fully integrated model
runtime. Explicit acceptance remains required before claiming those features.

## Self-coding outcome contract

1. Identify task and one verified recipe; inspect the minimal current source
   owners and tests. Do not spend several generation passes explaining paths.
2. Submit one useful exact-current-source edit proposal within a small finite
   discovery budget. If blocked, publish exact missing dependency and stop;
   repeatedly requesting new different files is not progress.
3. Stage every accepted edit into isolated actual candidate files, with a
   manifest of modified paths/preimages. Never mark a patch as applied until
   those files exist and digests match.
4. Compile, test and repair against actual compiler errors in the candidate;
   preserve partial results, full diagnostic receipts and the next handoff.
5. Launch/compare a separate sandbox process, Keep Current / Choose Candidate,
   validate a second build from the chosen parent. No live-source promotion
   without operator authority.

**Native end-to-end acceptance:** Add a fresh named GUI theme with exact enum,
implementation, registration and persistence edits; execute the real build and
unit tests; produce changed sandbox files and independent preview; choose and
build the successor without losing request/status/raw history. Measure source
read rounds, first-byte/first-patch latency, model TPS, build duration and
candidate outcomes. A 40-minute sixth pass just starting a theme patch fails
the performance/usability gate regardless of eventual model output.

## Staged deliveries, dependencies, exit criteria

1. **Source guidance + honest observable UI:** recipes, retained blocks,
   typed activity and completed raw inspector; pass source/helper/pure contract
   checks and MSVC Debug/Release/ReleaseWithDebugInfo. v0.90.41 is a candidate
   here, not a runtime acceptance result.
2. **Real native raw streaming and checkpointed chat:** provider event sink,
   backpressure/cancellation, session persistence and copyable blocks; measured
   output retained across finish/reload; full Windows/GUI tests.
3. **Model adapter foundation and GPU telemetry:** config → launch arguments →
   reported actual offload; independent model-load/delete/stop controls, data
   ownership; fake-backend and real model tests.
4. **Qwen Next Flash native backend:** integrate verified EngCoder QwenFlashRuntime
   source/dependencies into engine-managed adapter, no external EngCoder server
   required; test native local inference on operator hardware.
5. **Other model backends and routing:** dense/MoE/Nemotron profiles, stable
   multi-model leases and scheduling, per-task model choice; real backend tests.
6. **Portable, separately reusable project AI SDK:** example CLI/GUI/game
   consumers from static/core and selected adapters; Editor optional, project
   scope guards complete, versioned public API and cross-platform builds.
7. **Agentic harness parity:** semantic task plans, independent tools/subagents,
   permissions, source/source-control, build/tests, artifact retention and
   coherent native UI; proof of real source edits and sandbox succession.

Do not silently close unfinished missions when a new source version is stamped.
Update `Changes/mission_cache.md` and `Changes/roadmap.md` when decisions change;
only move a mission to the changelog after its actual acceptance receipts exist.
