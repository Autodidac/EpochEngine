# Epoch native AI runtime

## Shared downloaded weights — October28

Public and Development share one weight cache:
`%LOCALAPPDATA%/EpochEngine/AI/model-cache` on Windows. Linux uses the matching
`EpochEngine/AI/model-cache` below XDG data home or `~/.local/share`.
`ai.epoch_native_storage.hpp` owns this default. The two editions still keep
separate conversations, approvals, model selections, workspaces and learning
state. A shared model file does not grant Public access to Development source
or private state, and no weights enter source/release archives by default.

The operator's two Flash repositories were fully SHA256-verified before cleanup.
They were duplicate copies of one three-shard model plus its vision projection,
not three model versions. One canonical repository remains. Legacy Development
and Public weight paths are compatibility links, so saved paths still resolve
without copying the payload again. About74.3GiB was recovered. Settings and
saved work were untouched. Evidence:
`Artifacts/Reports/shared-flash-cache-oct28/`.
The shared-path and separate-state contract passes actual Debug/Release
consumers with both physical edition markers. Integrated Development Debug and
Release builds and full contracts now pass, including child binding and ownership
(`e6139aea…968091` / `27c125d4…f089de`). The operator confirms the folders now show about
74 GiB. Cache size must count each physical payload once, including when snapshot
links and legacy aliases reach it; the actual-owner accounting tests pass.

Never remove a numbered GGUF shard as though it were an old version. Admit a
complete set, reuse its actual cache identity and verify inactive ownership
before a user-authorized migration. Discover known legacy weight locations
without copying private profiles or downloading a second complete set.

### Editor and project model ownership

One user-level weight store serves the Editor, generated projects and candidate
previews. A private child home is for its settings and conversations; it must not
become another model download location. The trusted parent captures the shared
inputs before preparing that private home. The child borrows the existing SDK
and admitted library paths. Missing inputs lead back to the parent Model Manager;
they do not trigger child downloads, runtime copies or alternate-cache searches.
A model-free project can still launch when no model or SDK is installed.
These are input and lifecycle rules, not OS confinement or a new shared server.

Sharing files does not share a conversation or prove that memory was freed.
The native manager's ownership lease covers all its resident model processes,
and another application must wait until those processes retire. Merely pausing
inference does not release the weights or attention cache. Preserve active jobs,
selection and actual loaded settings. Never kill another application's model to
claim the lease.

The existing `run_asset_stage` transaction has passed one real headless 9B test:
the old worker retired before the callback, the actual loaded configuration was
restored in a new worker, one completion returned `OK`, and all owned processes
retired. The saved profile remained unchanged and no weights or SDK were copied.
Receipt: `Artifacts/Reports/native-asset-handoff-runtime-oct28/`.
The subsequent real shared-user roundtrip also passes against the final Release:
parent model retires, an owned headless child loads the same existing 9B file and
SDK with separate private state, the child retires, and the parent restores its
exact loaded configuration. A separate CPU process first proves that the lease
is busy under the original live model. Original profile bytes are unchanged;
two tiny completions total 34 input / 4 output tokens, with no paid calls. Total
runtime was 11.891 seconds, all owned processes retired, and no model/SDK copies
were created. Receipt: `shared-child-roundtrip/` below the same report root.
This does not yet prove automatic Editor → project → Editor handoff. That requires
the project lifecycle to request ownership, preserve the coding session, and
restore it only after the child model workers have retired. Projects with AI
disabled must not evict the Editor's models. Native GUI acceptance remains open.

The operator owns the incorporated backend and authorizes Epoch source ownership.
Implementation and private headers live under Engine/src/ai/native, lifecycle
integration under ai.native_runtime, and model-worker resources under
Engine/assets/ai/native. Epoch's root license governs this first-party source;
there is no separate backend project/license or frontend HTTP server. Existing
legacy inference/GPU environment values remain read-only compatibility inputs.
The engine owns controls, storage, model lifecycle, raw tokens, task/tool dispatch
and the native process lifecycle. Qwen and external dependency licenses remain.
Epoch inference explicitly disables model-weight training; source self-iteration
does not authorize training or ingestion of runtime conversations as training data.

Controlled source, project authoring and evidence-review requests make one native
model completion through Epoch's existing request/schema adapter. The native
backend does not enter its independent task/controller loop for these requests.
Epoch retains source context, validates proposals, stages exact edits and owns
build, repair and approval. Ordinary chat retains the native agent capabilities.
Cancellation, deadlines and output budgets reach the provider; incomplete output
at the token limit is rejected. Raw views aggregate decoded token deltas without
event/function wrappers. Legacy wire identifiers remain compatible cache inputs.

Final ownership/routing MSVC Release build, isolated pure engine contracts and
the one-completion/schema/cancellation adapter regression pass October 12.
The candidate-copy regression retains native implementation and private headers
inside Engine, fixing the operator's v0.90.45 missing-addons compiler failure.
Model provision/load, inference, GPU telemetry and native succession are still
operator acceptance. Native Qwen's specialized worker remains a prerequisite.

## Native setup and chat controls (v0.90.47)

The chat pane owns one method picker: Local Models, Local API, direct GGUF
runtime, or external MCP for engine tasks. Selecting Native queues cache reuse,
required runtime/model preparation, loading and persisted foundation selection.
Selecting Local API discovers the chosen endpoint and confirms the selected model
without a second approval popup. Required setup failures remain visible; an absent
specialized Qwen installer/runtime is an actual prerequisite, not ready state.
The direct lane reuses its installed runtime discovery and package configuration.
Method/model changes are disabled while an owned request is queued or running.

Options use flat Models, Roles, Training, Tools and Guidance pages in the existing
chat scroll surface. Model roles persist across restart; clearing a role restores
automatic routing. Registered Qwen profiles and available internal weights can be
assigned to the controller or specialists. This does not promise simultaneous
residency. Epoch's owned project/engine request protocols still govern edits,
builds, child processes and approval; registered native tools retain their policy.

Explicit Training initializes and trains internal controller/conversation/specialist
weights, with scope, step count, CUDA requirement, progress, pause/resume and stop.
Ordinary requests keep automatic weight training disabled. Setup cancellation
reaches model loading and training environment/initialization workers. Python,
PyTorch and safetensors are required for training; missing dependencies are
reported by the bundled worker. Downloaded Qwen weights are not rewritten.

Raw Output retains decoded model tokens only in the chat. Activity is a separate
chat view: System 1 routing, tools, runtime readiness and Epoch-owned project or
engine work. Activity records do not enter raw token blocks. Epoch owns conversation
history; independent native tasks use distinct session IDs to avoid sharing a
single task history between editor projects.

Windows speech enumerates installed SAPI providers without opening a microphone.
Dictate/Voice explicitly starts capture for the visible editor context. Transcripts
remain editable in the composer and require Send; speech cannot authorize tools.
Voice adds speech playback for assistant replies. Stop, project/context teardown
and application shutdown stop capture/playback. Other platforms report that no
native speech provider is connected. No device/model/GPU run is claimed here.

MSVC Debug/Release compile successfully. Release's isolated pure suite passes
182 checks, including fourth-read admission, token/activity separation and default
unrestricted work admission. Model-role persistence also passes an independent
CPU-only test. Live GUI, microphone, real training and useful self-coded successor
acceptance remain operator checks.
# October22 local tuning contract

The GPU slider is a percentage of the model's layer-count hint, not GPU
utilization or dedicated VRAM. The former universal30-layer ceiling is removed.
Recommended/automatic resolution caps weights against the smaller detected and
configured VRAM budget with desktop/compute headroom. Imported local models use
the existing complete-shard byte inspection when their saved size is unknown;
unknown sizes retain a conservative30-layer fallback. Resident sets retain their
separate weight-byte allocation. This estimate does not predict exact KV/compute
allocations; startup fallbacks and actual loaded context remain authoritative.

Model Manager separates saved settings from the runtime's effective profile:
loaded GPU layers, decode/batch threads, KV types/placement and batch/microbatch.
Settings persist through the validated whitelist and require explicit reload.
Context reserve, automatic compaction and trigger/target controls are available.
Direct loads and saved sets share the existing RAM-headroom admission heuristic;
failure retains other models and explains the requirement. This is model-load
admission, not an inference permission prompt or queue/scheduler replacement.
Do not claim maximum TPS from a slider, GPU utilization or passing fixtures.
Validate identical prompt/output lengths, prompt latency and measured decode
tokens separately. Full GPU KV tuning, precise GGUF layer/KV metadata sizing and
same-prompt hardware benchmarks remain acceptance work.


## Model Manager redesign — October26 operator references

Use the supplied LM Studio and Bionic screenshots as layout references, not as backend capability evidence or instructions to copy their system prompt. Keep one Model Manager with a readable model library and selected-model details; allow resizing and independent scrolling, with compact, consistently aligned action buttons. Preserve explicit local/provider/Codex selection and show the destination before Send.

Group settings into Info, Load and Inference. Within them use clear sections: Context and performance; Generation; Reasoning; Memory; Speculative decoding; Advanced. Include short help descriptions and units. Distinguish saved defaults from live settings; explain when reload is required and never reload during an active task. Show model identity, quantization/file size, supported versus actually loaded context, image/projector status, resource estimates and measured usage. Do not use project-package models as if they were host Assistant installations.

Automatic hardware settings is the intended simple default, with manual overrides available. Auto must compute within known model limits, usable RAM/VRAM after renderer and other residents, KV/cache/intermediate costs, task needs and response reserve. Show the resolved settings and reason. Do not equate largest context, highest GPU percentage or most threads with fastest performance. Current runtime automatic policy does NOT yet automatically size context: saved context remains explicit (normally81920). Adding an Auto label without implementing/admitting that policy is not acceptance. Never silently shrink an active coding session or its admitted source.

Existing controls to retain and make readable: GPU automatic fit/exact layers, context/slots/reserve/compaction, decode and batch CPU threads, logical/physical batch sizes, Flash Attention, K/V cache formats and placement, prompt/expert/host memory budgets and compatible draft-model settings. Add requested generation/reasoning/memory controls only through verified backend options and profile persistence: temperature, optional response limit, stop strings, supported thinking/budget, seed, sampling/penalties, mmap/keep-resident,20-minute idle timeout, cache checkpoints, RoPE overrides and speculative decoding. Unsupported items must be unavailable with a reason, not cosmetic switches. Show experimental status only where justified. Advanced argument overrides must not defeat process ownership, paths, approval or network boundaries.

Context overflow must preserve task/source integrity: visible compaction/checkpoint or a reported limit, never silently discard code from the middle merely because a reference UI offers that mode. Keep model-specific settings separate from conversation/system instructions and prevent prompt text from granting tool or own-source permissions.

Acceptance: normal/compressed windows, keyboard/focus/scroll, selectable readable values, draft cancel/save/reload, saved/live differences, Auto/manual transitions, unsupported options, insufficient memory, idle expiry and a real27B image/file request with correct image receipt. Screenshots alone and CPU fixtures do not establish native GUI acceptance.
