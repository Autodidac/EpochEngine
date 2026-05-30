# Epoch OS AI Content

This folder stores repo-safe AI integration assets for Epoch. The current
direction is OS/open-source model integration, not an internal bundled model.
Epoch owns the editor harness, evidence gates, prompts, manifests, captures,
tool schemas, and package lanes. Model weights stay external, operator-selected,
and license-reviewed before use.

Tracked here:

- curated JSONL datasets
- dataset schemas
- eval cases and deterministic smoke gates
- control-loop contracts
- provider manifests
- prompt templates

Do not commit normal Git history with:

- raw checkpoints
- quantized weights
- downloaded model files
- cache files
- compiled local model outputs

Downloaded model/package artifacts should live outside tracked source, usually
under executable-local cache buckets such as `cache/models/`,
`cache/packages/`, or `cache/updates/`.

Raw/staging capture can live under:

- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`

Those capture files are Git-safe JSON/JSONL, but they are still staging data.
Review them, promote only intentional pieces into curated repo datasets or
evals, and delete outdated/bad artifacts when the model direction changes.

## Canonical OS Model Lanes

Epoch documents these Hugging Face pages as the current OS-model source lanes:

| Lane | Repo | Purpose | License Metadata |
| --- | --- | --- | --- |
| Fast coding/review | `nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16` | Fast local helper for status, review, and bounded planning | NVIDIA Open Model License |
| Heavy coding/planning | `Qwen/Qwen3.6-27B` | Larger local coding/planning helper | Apache-2.0 |
| Preferred image package | `prism-ml/bonsai-image-ternary-4B-mlx-2bit` | Default local image generation/editing lane; best quality/footprint balance | Apache-2.0 |
| Low-memory image package | `prism-ml/bonsai-image-binary-4B-mlx-1bit` | Optional local image lane; smallest local footprint | Apache-2.0 |
| Higher-memory image fallback | `black-forest-labs/FLUX.2-klein-4B` | Optional image generation/editing fallback when memory budget allows | Apache-2.0 |
| Video/editing package | `Wan-AI/Wan2.1-VACE-1.3B` | Future video generation/editing package lane | Apache-2.0 |
| 3D asset package | `microsoft/TRELLIS.2-4B` | Future textured/PBR 3D asset package lane | MIT |

Source pages:

- `https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16`
- `https://huggingface.co/Qwen/Qwen3.6-27B`
- `https://huggingface.co/prism-ml/bonsai-image-ternary-4B-mlx-2bit`
- `https://huggingface.co/prism-ml/bonsai-image-binary-4B-mlx-1bit`
- `https://huggingface.co/black-forest-labs/FLUX.2-klein-4B`
- `https://huggingface.co/Wan-AI/Wan2.1-VACE-1.3B`
- `https://huggingface.co/microsoft/TRELLIS.2-4B`

These pages are metadata and download targets, not permission to bundle weights
blindly. Package/release work must preserve license files, notices, provenance,
and any model-specific redistribution requirements.

Image-lane Apache-2.0 handling is tracked under
`Engine/third_party/licenses/`: the shared Apache-2.0 license text lives at
`apache-2.0/LICENSE.txt`, and image model attribution/NOTICE requirements live
under `model-assets/`. Generated projects or releases that redistribute model
weights must carry those notices plus any upstream NOTICE or modification records.

## On-Demand Model Cache And Project Inclusion

Epoch does not clone or bundle model weights for engine self-iteration. The
engine harness uses a selected local OpenAI-compatible endpoint when one is
already running, and Package Manager model lanes stage on-demand downloads into
the executable-local `cache/models/` bucket when the operator explicitly asks.

The coding/review model lanes are:

- `nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16` for fast local review and bounded planning
- `Qwen/Qwen3.6-27B` for heavier coding, planning, and review passes

The preferred local image-generation lane is
`prism-ml/bonsai-image-ternary-4B-mlx-2bit`. Use
`prism-ml/bonsai-image-binary-4B-mlx-1bit` when footprint matters more than
quality, and keep `black-forest-labs/FLUX.2-klein-4B` as an optional
higher-memory fallback. All three image lanes are metadata/download recipes by
default; weights stay in executable-local `cache/models/` after explicit
operator action.

Generated projects do not inherit model weights by default. A project may carry
model metadata/download recipes, but copying or packaging weights into a project
requires an explicit package opt-in plus license and notice review. This keeps
normal games/software lean while still allowing large OS models to be included
when a project deliberately needs them. The first editor gate writes a
project-local `*.model.package.json` manifest and a cache-local
`download.plan.json`; actual weight transfer remains a separate
operator-approved Package Manager step.

## Current AI Architecture

Epoch keeps three AI/control pieces:

1. an engine-owned OS-model harness for working memory, retrieval, planning,
   tool use, verification, evidence metrics, and dataset/eval gates
2. local MCP/control/tool harnesses that operate the editor and collect proof
3. operator-selected OS model lanes for inference, review, and future creative
   package workflows

External local OpenAI-compatible providers such as LM Studio or Ollama are
runtime/helper providers. They can speed up testing, evaluation, curation, and
iteration, but they are not hidden authority. Epoch does not target bundled
runtime model weights as the practical path.

Model discovery is inventory only. Epoch may list available local models, but it
must not auto-name or activate one from discovery. The operator-selected model is
the only active runtime/helper target for in-editor calls unless a phase
explicitly allows extra helper lanes for drafting or review.

## AI Technique Policy

Epoch's AI loop should follow current practical agent guidance:

- Treat AI code, datasets, evals, tools, and UI surfaces as production engine
  work. Experimental learning loops must still be bounded, reviewable,
  evidence-captured, and reversible.
- Keep agent workflows simple, composable, and inspectable. The live loop is a
  staged workflow, not a swarm of hidden autonomous agents.
- Treat model/tool integration as a controlled tool-calling loop: the model can
  request or propose work, Epoch executes the editor/build/tool action, and the
  resulting output is fed back as evidence.
- Treat games, tools, apps, and servers as valid project outputs, but keep
  bypass-capable runtime activation human-gated.
- Keep tracing/capture artifacts for model replies, tool actions, build logs,
  scene changes, and staged packets so failures are debuggable.
- Build evals early and use them before prompt, model, dataset, or tool-harness
  changes are treated as improvements.
- Do not train recursively on unverified generated output. Model-written
  suggestions are raw material only; they need real tool/build/scene evidence,
  labels, and review before curated promotion.
- Keep training and eval material separate. Raw captures stay in the workspace,
  curated datasets live under `Engine/ai/datasets/curated/`, and eval cases live
  under `Engine/ai/evals/`.

## Closed-Loop OS Model Target

Epoch AI is not meant to become useful by adding more prompt text to a stateless
chat path. The target is an always-running, evidence-gated control loop around
selected OS model lanes:

1. base model for language/reasoning
2. working memory for current goal, task stack, observations, files, tools,
   assumptions, errors, and recent actions
3. persistent semantic, episodic, and procedural memory
4. retrieval/ranking that injects only relevant memory into context
5. explicit goal stack with constraints, success conditions, and failure states
6. planner that continuously replans from new evidence
7. executor that performs visible tool/editor/build actions
8. verifier that checks compilers, tests, screenshots, logs, diffs, and evals
9. metrics that reward verified progress and penalize constraint violations
10. self-state tracker for known unknowns, confidence, tools, failures, and mode
11. attention controller for interrupts, priorities, and tool requirements
12. real-time loop: observe, update memory, retrieve, evaluate goals, plan, act,
    verify, commit memory, and replan

Long-term memory alone is not enough. The practical intelligence jump comes from
closed-loop agency against reality: goal, action, evidence, correction, next
action. Epoch must not promote memories, datasets, code changes, or self-status
claims without visible evidence from that loop.

## Engine Self-Iteration Loop

The current foundation is an evidence-gated engine self-iteration lane, not
blind self-modifying autonomy and not the normal game/software editor:

1. planner produces or updates an explicit staged packet
2. executor proposes work only from the staged packet and current operator goal
3. builder creates fresh build evidence
4. verifier checks build/runtime/capture/eval output
5. gate promotes curated training/eval records or discards the attempt

The live contract is stored in:

- `Engine/ai/control/continuous_build_loop.json`

The editor AI workspace has a continuous build lane that watches active
project/script evidence, runs one child-project build at a time, and stages a
fresh packet after successful builds. The compatibility profile id may still be
`sandbox`, but generated child artifacts present as `EpochEngine` because this
is the engine-upgrade lane rather than a separate game project. That packet is
the durable handoff into future replay, verifier metrics, and curated promotion.
It does not grant blind write-through to the repo.

Epoch also exposes `--editor-ai-gate-self-test` as a deterministic guard for
model-review quality. It rejects status-only replies, missing verifier evidence,
bypass/server requests, and any self-iteration reply that tries to promote
without human approval. Keep this check green before allowing selected OS
models such as Nemotron or Qwen to supervise iteration packets.

## Evidence Metrics

Use these metrics for every candidate model, adapter, prompt, tool schema, or
dataset promotion:

- gate accuracy
- false accept rate
- false reject rate
- evidence coverage across packet, build log, output, verifier, and capture
- tool trace coverage across action, arguments, output, errors, file diffs, and
  screenshots when available
- build pass rate
- runtime smoke pass rate
- self-iteration completion rate
- curated promotion rate
- server/bypass block rate
- regression rate after promotion
- time to verified patch

The deterministic seed is `--editor-ai-gate-self-test`, which logs aggregate
accept/reject, false-accept/false-reject, safety-block, average evidence-score,
and accuracy statistics. Keep that gate green before treating Nemotron, Qwen, or
any other local OS model as a reviewer.

Self-iteration is not complete just because a model says it is. A completed
iteration needs the same packet to show: project manifest, build log, output
artifact, generated child self-test/verifier result, visible gate state, no
required `[missing]` markers, and a human-review hold before source or dataset
promotion.

## Running The OS AI Controls

From a developer checkout:

1. Configure/build the editor:
   `cmake --preset windows-msvc-debug`
2. Build after code changes:
   `cmake --build --preset windows-msvc-debug`
3. Launch the editor executable from an asset-bearing output folder:
   `x64/Debug/EpochEditor.exe`
4. Open the central `AI Sandbox` surface for model selection, evidence status,
   chat, and self-iteration controls.
5. Use Package Manager model lanes only when a model needs to be downloaded into
   `cache/models/`; otherwise select an already running local model endpoint.
6. Select an explicit local model from the OS provider inventory before chat or
   tooling is active.
7. Use the Inspector for AI command buttons and keep Bottom Dock AI output as
   compact evidence/status only.
8. Promote only reviewed, evidence-backed captures from the training/eval gate.

The local chat path is explicit: the editor scans `/v1/models` and sends chat
requests to `/v1/chat/completions` on the configured local endpoint. The default
endpoint is `http://localhost:1234`; override it with `EPOCH_AI_ENDPOINT`,
`EPOCH_OPENAI_BASE_URL`, `LM_STUDIO_BASE_URL`, or `OPENAI_BASE_URL` before
launching the editor. CLI/self-iteration runs may also set the explicit model
with `EPOCH_AI_MODEL`, `EPOCH_OPENAI_MODEL`, `LM_STUDIO_MODEL`, or
`OPENAI_MODEL`; this is an operator-selected override, not a first-detected
model fallback.

If a selected model returns blank visible assistant content and only
`reasoning_content`, Epoch rejects that reply as a model/API configuration
failure. Hidden reasoning is never displayed as chat and is never promoted as
curated assistant training data.

## Server And Addon Safety

Epoch can create games, tools, apps, and server projects, but creation is not
permission to run anything that could bypass the gate. Any generated server
code, multiplayer service, local web service, control API, model-accessible app,
or network listener must remain inert until the operator explicitly enables and
launches it from a visible editor or shell action. The AI loop can ask for that
approval and explain why it is needed; it cannot grant approval to itself.

Local game runs, local tool tests, script harness passes, and non-networked
runtime checks are allowed through approved editor/tool-harness controls because
they are the evidence path Epoch needs. The boundary is not "never run local
things"; it is "never let the model create or activate a new service/control
surface it can use to bypass human review."

The repo-local `addons/` folder is intentionally ignored. It may contain starter
projects, experiments, and future Epoch candidates, but those projects must be
reviewed and intentionally promoted before anything from that folder becomes
tracked source.
