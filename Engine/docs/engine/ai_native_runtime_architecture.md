# Epoch native AI runtime

The operator owns the incorporated backend and authorizes Epoch source ownership.
Implementation and private headers live under Engine/src/ai/native, lifecycle
integration under ai.native_runtime, and model-worker resources under
Engine/assets/ai/native. Epoch's root license governs this first-party source;
there is no separate backend project/license or frontend HTTP server. Existing
legacy inference/GPU environment values remain read-only compatibility inputs.
Epoch AI owns controls, storage, model lifecycle, raw tokens, task/tool dispatch
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

The chat pane owns one method picker: Native Epoch AI, Local API, direct GGUF
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
