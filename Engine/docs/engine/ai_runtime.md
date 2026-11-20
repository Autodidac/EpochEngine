# Epoch native AI runtime

## Verified runtime evidence — November 15

This index separates measured behavior from the next acceptance gate. The
[model choices](ai_plan.md#november15-measured-model-choices) own role policy and
comparison interpretation; the [active pass](../../../Changes/active_pass.md#november-16-current-execution-order)
owns temporary workload holds and the next run. Flash remains the selected
primary coder. Historical workload holds below do not establish a current hold.
No slower Edge0 candidate has been promoted.

| Subject | Confirmed result and evidence | Limit or remaining acceptance |
| --- | --- | --- |
| Flash streaming profile | Actual Public/Development Debug/Release consumers pass [profile checks](../../../Artifacts/Reports/shared-native-inputs-oct28/1794794734472512600/result.json). The later [calculator request](../../../Artifacts/Reports/epoch-calculator-mission-20261115/mission-launch.json) used 30 GPU layers, CPU experts, a 3072 MiB GPU expert cache, 384 MiB pinned staging and CPU KV. | Preserve this model-specific profile. A layer count alone does not reproduce it. That request stopped under measured paging pressure; useful calculator implementation is not accepted. |
| Flash narrowed Engine proposal — November 16 | The [in-Engine receipt](../../../Artifacts/Reports/epoch-calculator-mission-20261116/flash-engine-result.json) records a completed, untruncated final generation of 369 tokens at 5.426 provider decode tokens/s. Working set was observed at 47.07 GiB; both owned processes retired. | The proposal is retained, unapplied and unbuilt. 47.07 GiB is not a peak. 81,920 is context capacity; actual input tokens are unknown. No paid review or functional calculator acceptance. |
| 27B tuned profile | The installed decoder passed 5/5 fixed tasks at median 9.51 output tokens/s with 56/66 GPU layers, 12 threads, 81,920 context capacity and CPU q4_0 KV. [Comparison](../../../Artifacts/Reports/qwen27b-offload-20261115/comparison.json) and [saved-profile change](../../../Artifacts/Reports/qwen27b-offload-20261115/profile-change.json) preserve exact settings and hashes. | Short prompts, ordinary desktop load and measured minimum free VRAM of 2.23 GiB. This is not full-window throughput or guaranteed headroom under future load. |
| 9B and modified models | [Six-configuration comparison](../../../Artifacts/Reports/model-coding-20261115/comparison.json) records actual scores, failures, timings, memory and worker retirement. The tested Qwen3.5 9B is restricted to chat and advisory planning; modified variants retain separate loading and no automatic fallback. | Role restrictions come from the operator and the owning policy. Small fixture scores do not certify general coding ability or tool safety. |
| Edge0 installed baseline | [Compute run](../../../Artifacts/Reports/edge0-compute-check-20261115/edge0-compute-progress.json) completed a 152-input-token LineDecoder task with 689 output tokens at 3.457 provider decode tokens/s. The [compiled fixture passed](../../../Artifacts/Reports/edge0-compute-check-20261115/edge0-compute-code-results.json), and the [owned job retired](../../../Artifacts/Reports/edge0-compute-check-20261115/edge0-compute-job.json). | 81,920 is capacity, not input length. Background contention was recorded; the CUDA counter windows are bounded samples, not whole-run averages or an isolated ranking. |
| Edge0 full Editor — November 16 | Assistant Chat produced the exact two-line calculator-startup deletion. The provider reported 10,433 prompt tokens in 1,754.312 seconds, then 2,077 output tokens at 4.049 tokens/s. Epoch reached manual review in 37m47s. [Result](../../../Artifacts/Reports/epoch-calculator-mission-20261116/edge-engine-result.json) | The proposal is retained, unapplied and unbuilt. Early resource samples cover prompt processing only. The request ran at 81,920 context capacity and never engaged paid review. |
| Edge0 resident-expert experiment | [Real-pack parity](../../../Artifacts/Reports/edge0-resident-reuse-20261115/expert-parity.log) passed with 90 cache hits / 180 misses and worst relative L2 error 0.026386. The matched generation interval regressed from 31.47 to 108.98 seconds in the [stop receipt](../../../Artifacts/Reports/edge0-resident-reuse-20261115/performance-stop.json). | Unpromoted and retired before a complete coding answer. Reduced transfer bytes did not establish faster generation. Keep it outside the installed runtime. |
| Build/Run retirement | [Focused CPU contracts](../../../Artifacts/Reports/project-runtime-status84-nov07/9b49be151d07/result.json) and the [rebuilt Release contract suite](../../../Artifacts/Reports/editor-reviewed-run-20261115/editor-contract-verification.json) pass. Ordinary and reviewed Run retain their distinct build/artifact contracts below. | The [disposable 9B Editor launch](../../../Artifacts/Reports/live-build-run-9b-20261115/launch.json) proves only Editor startup. No resident 9B retirement-before-compiler or child-runtime acceptance has been observed in that attempt. |
| Windows CUDA control | Named CUDA and compute PDH counters were readable; three idle samples showed 0% CUDA with no model worker. Earlier Edge0 runs retain [raw CUDA samples](../../../Artifacts/Reports/edge0-compute-check-20261115/cuda-decode-samples.json). | Idle availability is not model qualification. The [below-100% solo / below-50% project-plus-bot plan](#windows-cuda-budgets-for-editor-and-project-ai) has no runtime enforcement acceptance yet. |

The Flash paging stop had 165 MiB available RAM and 25,118 page-ins/sec while
system commit remained 38.6 / 65.5 GiB. Its [pressure receipt](../../../Artifacts/Reports/epoch-calculator-mission-20261115/mission-pressure-stop.json)
does not diagnose GPU saturation or justify a blanket ban on Windows paging.
The later narrowed request completed the proposal recorded above. No proposal
from the earlier paging-stopped request was accepted; the new proposal still
needs host build/checks and human acceptance.

Each receipt applies to its recorded executable and inputs. The calculator
attempt used Editor `2EA90D5C…B3BCF9`; the later reviewed-Run suite and disposable
9B launch used `DABC93B0…B5BC`. Full SHA-256 values are in the linked receipts.
Do not transfer a result between those binaries or relabel configured context
capacity as populated input. Local reports can contain private paths, prompts
and candidate data: retain them as local evidence, not public release payloads.
When changing a result, update this index and its owning contract with the new
receipt and limitation instead of appending a second competing status account.

### Edge0 full Editor — November 16

The calculator-startup request had returned no output after 20 minutes.
The installed worker's read-only slot counters confirmed ongoing prompt
processing, with 8,998 tokens processed and zero generated at 06:46:54 UTC.
The provider later reported 10,433 input tokens and first output arrived
after 29m14s. The two-line deletion matches one exact anchor in the retained
source. The later Menu case still initializes the menu.
[`edge-live-slot-samples.jsonl`](../../../Artifacts/Reports/epoch-calculator-mission-20261116/edge-live-slot-samples.jsonl)
[Verified proposal and timing](../../../Artifacts/Reports/epoch-calculator-mission-20261116/edge-engine-result.json)

The earlier 165 two-second samples during this request averaged 19.9% total
board use (33% max) and 2.8% Edge CUDA use (5.3% max), with 8.39 GiB highest
sampled worker RAM and 8.17 GiB worker dedicated VRAM. Samples cover
06:21:01–06:26:30 UTC during prompt processing.
[`resource-samples.jsonl`](../../../Artifacts/Reports/epoch-calculator-mission-20261116/edge-observation/resource-samples.jsonl)

A later read-only 30-second observation of Editor PID 34340 and Edge PID 37152
recorded Edge whole-machine CPU 31.16% mean / 47.75% peak, named CUDA 4.77% /
6.71%, and Copy 12.69% / 18.51%. Worker dedicated GPU memory was 8.36 GiB;
working set averaged 8.17 GiB and private commit 10.77 GiB. Editor CPU averaged
1.92%, 3D 7.52%, CUDA 0%. Whole-board GPU 30.3% / 38% is a separate measurement,
not the worker's CUDA utilization. The workload phase was not independently
identified; these samples do not establish prompt/decode TPS or a causal
bottleneck. [Samples and process identities](../../../Artifacts/Reports/edge0-live-observation-20261116/host-samples.json)
[Summary](../../../Artifacts/Reports/edge0-live-observation-20261116/summary.json)

An isolated stream1 diagnostic candidate retains the admitted expert algorithm
and adds opt-in `EPOCH_EDGE0_PHASE_TIMING=1` CPU wall-time counters for preparation,
read/staging, enqueue, compute calls and readback. Enqueue time is not GPU kernel
time; no extra CUDA synchronization is introduced. Its native build and CPU
counter checks passed, but it is not installed or inference-qualified and no
speed improvement is established. The benchmark now accepts an explicit `--edge0-candidate-manifest` plus
`--edge0-candidate-sha256` only within its existing Edge0 KV qualification
profile. It still verifies the installed stream1 baseline, every candidate SDK
file, and the exact model metadata; only the two decoder DLLs may differ from
the baseline. Missing or wrong hashes, changed bytes and incomplete SDK lists
fail closed in CPU tests. Preserve the current model, four KV layers, context
and other load settings; wait until the active worker has finished and retired.
[Source/check receipt](../../../Artifacts/Reports/edge0-stream1-phases-20261116/source-receipt.json)
[All nine binary hashes](../../../Artifacts/Reports/edge0-stream1-phases-build-20261116/binary-inventory.json)
The diagnostic-only retirement sink retains stdout/stderr tails under the
checkout-local candidate's `observations/retired-*` directory for success,
failure and cancellation. Each stream is capped at 1 MiB; its receipt records
exit/cancel/timeout state and whether that cap was reached (earlier-byte loss
is possible; total produced bytes are not measured). Capture stays on the owned
worker, with no GUI file writes or new listener. CPU Debug/Release sink tests
passed; native model execution remains a separate acceptance gate.

November 17 source review of that exact diagnostic candidate confirms how to
interpret these counters: preparation includes allocation, graph creation and
dense uploads after eviction; read/staging includes expert reads and host copies;
compute/readback include blocking host waits. The expert mutex wait and outer
model graph are not covered. Counters are cumulative and reported every 4,096
layer-tokens, so compare deltas with the accompanying layer-token/upload counts,
not as isolated CUDA kernel timings. One changed expert slot currently stages
all ten selected experts. This is a candidate optimization target, not proof of
the bottleneck; the earlier resident-reuse experiment regressed and stays out.
The four-KV-layer Editor request reached its 45-minute mission deadline with no
answer. Retained backend logs report 15,818 prompt tokens in 2,669.91 seconds
(5.92 prompt tokens/sec); the request journal records 67,502 prompt bytes and
zero response bytes. No decode TPS or coding verdict exists. The Editor closed
normally afterward and both owned processes retired. Its bounded resource sample
is recorded in the active pass; concurrent independent model activity prevents
an isolated ranking. The intact diagnostic tails contain no GPU phase records;
do not infer a phase bottleneck from their absence.
[Terminal receipt](../../../Artifacts/Reports/editor-integration-20261116/admission-tooltip-repair/edge-terminal-result.json)


Resource discipline: retain useful output and diagnostic evidence, then retire
owned workers that have no pending use. Do not leave a finished or rejected run
consuming resources merely to await review; preserve its reviewable artifacts.
Never retire unrelated operator workloads. Clean only verified obsolete generated
data, preserving source, projects, candidate edits and required evidence.

The next requested KV comparison is zero/four/eight layers, subject to actual
backend support and memory admission. Eight is not a verified default or maximum;
show layer counts rather than implying percentages of total GPU offload. Hold
model, actual prompt, context capacity, output budget and other settings constant;
record prompt/decode time separately, CPU time, working-set/private RAM peaks,
process GPU memory and CUDA samples. Samples are not whole-run peaks, and shared
GPU totals cannot establish model-only resource savings. Compare correctness too.

The installed stream1 backend already places all 49 accounted layers on GPU
and streams the routed experts to CUDA0. Model Manager's existing Runtime
Options source now offers the zero-layer baseline and four/eight-layer GPU
attention KV trials (eight remains unqualified; next Editor integration pending). Saving applies to the next explicit load. The trial requires a detected
16 GiB-class CUDA adapter and leaves the single slot, context, batches, expert
placement and model unchanged. Measure actual placement, prompt/decode times,
correctness, RAM, VRAM and CUDA under the operator's 50% Editor target before
choosing a new default. Flash settings are independent.

November 17 bounded source review: the admitted 49-layer/four-GPU-KV profile
already executes experts on CUDA despite its `--cpu-moe` launch flag. In
`Artifacts/Reports/edge0-stream1-phases-build-20261116/source/src/models/qwen.edge0_pack.hpp`,
`prepare_gpu_layer` fixes expert residency at 3 GiB with a 1.5 GiB free reserve;
`evaluate_gpu` serializes per-token staging, graph execution and blocking readback.
A changed slot stages all ten experts. These describe the staging path; the retired measurement below provides its
bounded wall-time evidence, not an energy verdict. More ordinary layer/KV
offload does not remove this path. No additional qualified GPU tuning is established.
Without a causal GPU repair, use Flash next after cloud acceptance, preserving
its working recommended/auto placement and existing independent configuration.

**Retired November 17 measurement:**
`Artifacts/Reports/edge0-stream1-phases-build-20261116/observations/retired-111420936736400/`
retains an interrupted worker (cancelled, exit 1; no diagnostic stream truncation).
At its last cumulative phase sample (56:18), host wall milliseconds were
prepare 915.673, read/stage 2,050,404.071, enqueue 15,896.590,
compute call 705,345.639 and readback 47,333.435. Read/stage represents about
72.7% of those recorded phase times. These are not CUDA-event timings or a
completed coding result. The expert counters report 14,160,340,812,800 upload
bytes and 2,785,591,296 resident bytes; neither is a whole-process VRAM peak.

The same log reports 48 model layers, full-attention interval 4 and 12 owned
attention layers; attention/indexer KV stores are 540/67.5 MiB at 81,920 capacity.
Both report four requested/four resident GPU KV layers. The existing decoder's
`llama-kv-cache.cpp` counts requested owned layers rather than hardcoding four,
so eight is structurally meaningful, **not yet qualified**.

The November 17 source change extends existing Edge-only admission in
`ai.backend_model_runtime_model.hpp`, Model Manager integer choices and
`Tools/models/edge0/benchmark_windows.py` from 0/4 to explicit 0/4/8. Nineteen focused CPU checks pass (23.787s),
including actual profile validation in Debug/Release and both editions plus
placement parsing. The shared native fixture was brought current with the
existing diagnostic-admission atomic; no admission bypass was added. Preserve
baseline/default 0, 49 GPU layers, both cache types, batches, one slot, runtime
identity and the existing 16 GiB-class guard; change neither Flash nor generic
GPU percentages. Extended placement checks in
`Tests/ai/test_edge0_benchmark_contention.py` require eight-of-eight in BOTH
stores and reject ignored/partial placement; actual profile-admission cases cover
0/4/8 and unsupported values. Native placement/resource acceptance remains open: run the same bounded coding workload
one model configuration at a time with the measurements listed above. Eight
must not become the default merely because its launch succeeds.

The separate prompt-progress coding baseline processed 13,468 prompt tokens in
2,166.861 seconds and decoded 292 tokens at 3.840 tokens/s. It requested the
exact stream and observer source owners. The next inference was cancelled at
399.203 seconds with no output. There is no proposal to build from this run.
The 81,920 figure is loaded capacity. Retain the original prompt and source
admission identity for the four-layer trial.
[Baseline receipt](../../../Artifacts/Reports/editor-integration-20261116/regular-edge-072753935/edge-prompt-progress-baseline.json)


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
Ordinary Editor Build and Run use the existing cancellable project task and the
native retirement transaction's leave-unloaded policy. The task waits for
protected inference and verifies retirement of Editor-owned model workers
before it invokes the project build. This applies to AI-disabled projects too.
Once the handoff retires a model, the Editor preserves its saved selection and
tuning and leaves it unloaded through build success, failure, cancellation and
project exit.

Reviewed Assistant Run uses the same model retirement transaction. The
reviewed build generation and accepted executable carry through the handoff.
The Editor rechecks the project inputs, generation and executable digest
before launching that artifact without invoking the compiler.

A build-only result releases its model admission claim when the result is
settled. For Run, the claim passes to the child process supervisor and blocks
Editor model loading across the launch gap and for the lifetime of the owned
project process tree. The supervisor releases it only after verifying
retirement. If compiler, model or project process retirement cannot be
verified, the corresponding guard remains held. Asset generation continues to
use the default policy that restores its prior resident configuration.

Focused CPU fixtures and the Windows Release Editor contract suite passed.
Live acceptance still needs a disposable project Run with an Editor model
already resident, evidence that its worker exits before the compiler starts,
and verification that stopping the compiled project retires the child and
leaves the Editor model unloaded. See
`Artifacts/Reports/editor-reviewed-run-20261115/`.

### Workload-qualified model tuning — November 16

Requested policy; automatic profile selection and all-model acceptance are not
yet proven. Optimize every installed model through its existing backend and
saved-profile owner for actual useful throughput in Epoch. Select a qualified
recipe from hardware capabilities/tier and declared workload: editing, building,
project running, or project-plus-bot, including declared scene complexity.
Capability rank (coding ability) remains separate from machine/resource tier.
Keep settings stable during a request; choose profiles at explicit workload
boundaries rather than continuously retuning layers, threads, batching, caches
or pacing from live utilization. Telemetry may pause/cancel an unsafe run; it is
not an automatic benchmark search or permission to mutate saved tuning.

Qualification records the exact model/quantization/backend, hardware, context
capacity and actual input length, effective settings, workload and concurrent
activity. Measure prompt latency, provider-reported decode tokens/s, task
completion/correctness, memory/commit and project frame-time impact separately.
For non-text models use the modality's actual latency/throughput rather than
inventing TPS. Compare one model at a time; publish the recipe and evidence in
Model Manager through the existing comparison path. Preserve the verified Flash
streaming recipe and explicit operator settings; a 27B tuning result does not
authorize applying its GPU placement to Flash or Edge0. Unknown workload/tier
combinations remain unqualified, not silently promoted from an idle benchmark.

### Windows CUDA budgets for Editor and project AI

The November 16 workload policy above supersedes the earlier proposal for live
feedback retuning. The utilization limits below remain qualification and safety
targets, pending per-model/workload acceptance and runtime enforcement.

| Mode | Admission and scheduling target |
| --- | --- |
| Model alone | Qualify a stable recipe with sustained CUDA activity below 100%; use 95% as a headroom target during qualification, not a live retuning threshold. |
| Project Build or Run | Finish protected work and retire the Editor's large model workers before the compiler or project starts. The current handoff retires all Editor-owned model workers and leaves them unloaded. Keep that behavior while qualifying any concurrent mode. |
| Running project with an explicitly enabled bot | Qualify the combined Epoch-owned AI CUDA activity below 50%, targeting 45% headroom and preserved project frame time. An unsupported workload waits or uses an already qualified CPU profile when the operator selects it. |

1. Extend the existing native/Windows telemetry path. Read WDDM's
   `GPU Engine(*)\Utilization Percentage` counters off the frame thread.
   Track the exact owned PID and start time, adapter LUID and engine identity.
   Prefer `engtype_Cuda`. Accept a compute counter only after verifying that
   it is the driver's equivalent on that adapter. Report stale, missing or
   unattributed data as unavailable. Aggregate owned AI processes on the
   same engine before selecting the busiest CUDA engine. Keep 3D, copy,
   dedicated/shared GPU memory, system commit and frame times separate.
   The adapter's graphics and compute engines may share physical cores.
   [Microsoft's Task Manager GPU explanation](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/)
2. Publish the readings in Model Manager before claiming qualification. Record
   cold load, prompt processing and decode separately. Start with
   one-second samples and rolling five-second observations.
   Show raw peaks separately. A sampled percent cannot guarantee
   a limit on every kernel. Windows does not support NVIDIA MPS.
   [NVIDIA MPS platform support](https://docs.nvidia.com/deploy/mps/latest/when-to-use-mps.html)
3. Apply the selected recipe through the existing scheduler and native worker:
   bounded prompt chunks, decode work and in-flight launches, without a new
   feedback controller. Unsafe live pressure may pause or cancel at an existing
   cooperative boundary; it must not silently retune or repeatedly reload the
   model. Preserve the selected profile and report the safety stop. Changing
   VRAM allocation or CPU offload alone cannot establish CUDA compliance.
4. Qualify the first concurrent mode inside the running project's existing
   bot owner. Keep the child process guard and exclusive native lease while
   proving this path. Editor inference alongside an independently owned
   project worker requires a shared admission protocol and separate proof.
   Queue concurrent GPU work when qualified workload admission or a safe
   cancellation boundary is unavailable. Never stop another application's model or reclaim an
   unverified process. Preserve prompts, candidate evidence and selection.
5. Test one exact model and backend recipe at a time. Verify the existing
   resident-model-to-Build/Run sequence, then collect a solo and a
   project-plus-bot receipt with the same prompt and specified context
   capacity **and** actual input length. Include rolling CUDA activity,
   raw peaks, VRAM, system commit, decoding speed, project frame times
   against its model-free baseline, cancellation, retirement and whether
   the requested task completed. A long over-budget window or project
   regression blocks qualification. Begin with 9B, then qualify 27B and
   Flash Next separately. The slower Edge0 experiment remains outside
   the installed runtime.

## Native backend ownership

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
Those October 12 checks establish adapter behavior only. Later model loading,
inference and telemetry evidence is indexed above; useful self-coded successor
acceptance remains open. Native Qwen's specialized worker remains a prerequisite.

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

### November 17 bounded Edge phase observation

`Artifacts/Reports/edge0-phases-observed-20261117/phase-analysis.json` records
one startup milestone, 48 layer-tokens before the coding request began:
prepare 346.297 ms, read/stage 454.941 ms, enqueue 1.494 ms, compute-call
50.653 ms, readback 2.460 ms (855.845 ms measured total). These are host wall
intervals, not CUDA kernel timings or coding throughput. No request-phase
milestone or completed answer was captured. An external workload appeared;
the reserve guard stopped the owned worker with 1,139,728,384 bytes free VRAM.
The owned job retired with zero active processes. Do not infer an inference
bottleneck or change defaults from this interrupted startup measurement.

The exact diagnostic SDK already accepts `LLAMA_ARG_LOG_VERBOSITY=4` and
`EPOCH_EDGE0_PHASE_TIMING=1` through the existing inherited child environment.
Set them only on the diagnostic Editor launch, retaining hash admission and
normal model selection. This exposes library INFO phase records without a
parallel harness or new transport. The prior long run used verbosity 3, which
filtered those records; it therefore cannot establish phase attribution.


### Native-operation settlement admission (November 17)

`ai.native_runtime.cpp::start_admitted_work` now retains the existing busy gate
through its completion callback and releases it afterward, including a throwing
callback. Previously a successor could observe idle before settlement and replace
the old jthread; its destructor joins on the caller. The only current nonempty
settlement callback publishes the asset-handoff completion condition. This closes
that admission window without changing schedulers or process ownership.
`Tests/ai/test_native_operation_settlement.py` exercises the actual worker wrapper
with a controlled callback: successor admission remains blocked during settlement,
then busy/native-operation ownership clears on success and exception (Debug and
Release, 1.454s). Native Editor integration remains pending. This is not evidence
that the reported long Send freeze is fixed: source preparation, GUI stream
consumption, lock contention and system resource pressure still require timed
native acceptance. Build task-pool workers, chat request threads and the owned
native model process are distinct owners; changing pool membership alone is not
a demonstrated latency repair.


### Status-only source input script resolution (November 17)

Confirmed source location:
`Engine/dep/EpochSelfCoding/integration/fragments/editor.private_editor_source_tick_packet_dbe7e6cd.inl:26`.
Before repair, the `source_iteration_input(width, statusOnly)` lambda called
`editor_resolve_script_source_path` even for `statusOnly=true`. Its owner,
`Engine/src/editor/editor.scene.cpp:3226`, checks file existence and may resolve
the repository root. The mission driver's per-frame session-status call therefore
can perform filesystem work unrelated to status. This is not measured proof of
the reported long Send freeze. `session_activity` itself reads flags/clock;
`native_runtime::snapshot(true)` copies cached inventory under its state mutex,
not a filesystem rescan or the HTTP host lock.

The bounded repair makes the local `script`
initializer `statusOnly ? std::string{} : editor_resolve_script_source_path(...)`.
Keep full-action path resolution, authority admission and dispatch unchanged.
Before omitting other fields, inventory all status-only consumers; do not assume
project identity or readiness fields are dispensable. The CPU check `Tests/ai/test_status_script_resolution.py` extracts the actual
script initializer using a counting resolver port: 100 status calls make zero
resolver calls; the full-action call resolves once and retains its exact path
and arguments (Debug/Release, 1.374s). This tests the changed initializer, not the
entire input lambda; the remaining status/session fields are unchanged by the diff. Native timed Send/scroll/Details acceptance remains a separate gate.

### Remembered model discovery folder

An explicit successful **Scan folder** remembers the canonical library folder in
`native_storage::defaults().data_root/model_library.txt`. Rescan restores this
bounded, versioned UTF-8 preference on its existing worker and scans it alongside
the shared weights cache. Edition/candidate-private state remains separate from
shared immutable weights; a fresh candidate profile intentionally has no saved
folder. An unavailable saved folder remains visible with the scanner diagnostic.
The preference neither registers nor loads models; split-GGUF completeness and
explicit registration admission remain unchanged. Write failures visibly report
session-only selection. Reads reject malformed Windows UTF-8 without escaping the
worker; writes validate a sibling staging file and reuse platform atomic replacement,
keeping the prior preference when staging/publication fails. The focused malformed
encoding and failed-write preservation check passed Debug/Release (3.013s).
`ai.native_model_discovery.hpp` owns the preference and
scanner; `ai.native_runtime.cpp` owns worker integration. The focused
`test_epoch_9099_native_rescan.py` preference/shards/no-host checks passed on
November17 (three checks, Debug/Release CPU fixtures, 8.384s). New-build native
restart/Rescan acceptance is pending; the running request was not interrupted.
