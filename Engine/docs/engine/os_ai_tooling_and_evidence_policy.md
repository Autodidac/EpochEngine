# OS AI, MCP, Harness, And Evidence Policy

Epoch does not own or train an internal LLM. The engine hosts an
operator-selected model connection and owns the bounded tools, context,
permissions, evidence, and editor integration around that model.

## Model Boundary

Supported inference transports are:

- an operator-started OpenAI-compatible endpoint using `/v1/models` and
  `/v1/chat/completions`;
- a directly selected `llama-cli` executable plus an operator-licensed GGUF
  model.

Direct CLI inference is one captured child process with an argument vector,
bounded output, timeout, termination, and offline/no-server flags. Epoch does
not start `llama-server`, bind a port, silently choose a discovered model, ship
model weights, mutate model weights, fine-tune a model, or maintain a hidden
internal persona.

Model discovery is inventory. Activation requires an explicit operator choice.
Executable and model selections persist only in executable-local cache state.

## MCP Boundary

`ai.mcp` owns the model/tool input-output contract. It is separate from the
model provider. The current contract provides:

- tool descriptors and capability negotiation;
- structured tool calls, arguments, results, errors, and evidence attachments;
- read, author, build, execute, capture, diagnose, engine-source, and network
  capability classes;
- operator-approval gates for every write, child process, source edit, or
  network-sensitive action;
- host-owned session authority containing granted capabilities and approved
  call IDs; model/tool-call payloads cannot grant themselves either one;
- cancellation plus argument, result, evidence, step, and time budgets;
- fail-closed evidence records whose default state is failed/missing until an
  explicit executor supplies a terminal result;
- an in-process project tool registry covering inspect, create, save, document
  edit, script edit, build, run, test, capture, and diagnostics.

The protocol and registry are build-proven. Model-generated tool-call parsing
and the complete multi-step dispatcher remain unfinished. Until that dispatcher
lands, editor actions are operator-invoked and the existing Tool Harness is the
real executor.

Epoch starts no MCP network server or hidden listener. A future external MCP
adapter may use bounded stdio or an explicitly approved host, but it must route
through the same registry, validation, and evidence contracts.

## Harness Responsibilities

The engine/editor harness must be able to:

1. inspect the active project, scene, scripts, diagnostics, and capability state;
2. create a project through the validated project shell;
3. apply reviewable document or script changes inside allowlisted roots;
4. save canonical authoring state;
5. build with the project lifecycle service;
6. run an approved child project or script;
7. execute contract tests;
8. capture editor/runtime evidence and return structured results to the model;
9. stop or cancel bounded work;
10. preserve a visible session trace.

Project creation is a required AI/tool capability because the editor itself must
create usable games, tools, and applications. A model request never bypasses the
same project admission, path, package, build, run, and network gates used by
human controls.

Engine-source development is a separate developer capability. It requires a
workspace allowlist, patch preview, explicit approval, build/test evidence, and
human review. The harness never commits, pushes, releases, changes updater
state, starts a service, or grants itself approval.

## Session And Evidence Storage

Runtime model exchanges and tool traces are evidence, not training data:

- `workspace/model_exchange.jsonl` records explicitly retained model exchanges;
- `workspace/tool_trace.jsonl` records structured tool calls/results;
- `workspace/ai/sessions/` stores bounded iteration packets;
- `cache/models/` stores operator-managed model assets;
- `cache/ai/` stores disposable runtime cache;
- `Engine/ai/evals/` stores deterministic behavior/evidence cases;
- `Engine/ai/datasets/curated/` may store reviewed fixtures or eval material,
  but Epoch does not automatically turn them into model training.

Normal chat is not automatically written to disk. A visible Capture/Trace
action or harness execution creates evidence. Hidden reasoning is never
displayed or harvested.

Every retained session identifies the selected model, project revision, tool
registry/schema, calls, results, errors, evidence paths, budgets, cancellation,
and terminal state. A model's confidence is never completion evidence.

## Extensions Runtime Package

`EpochEngineExtensions/packages/local_ai_llama_cpp_runtime` is currently a
plan and validation package. It does not fetch or build llama.cpp. Any future
executor must obtain explicit network/build approval, pin and verify an immutable
upstream revision, retain the MIT provenance, disable server/curl targets, and
keep each GGUF license separate.

## Safety Invariant

Generated games, tools, apps, and server source may be authored after an
operator request. Any listener, port bind, server process, model-accessible
control surface, downloaded native extension, or hidden service remains inert
until a human explicitly enables it.

The final invariant is:

> The model proposes structured work; Epoch validates and executes bounded
> tools; evidence returns to the model and operator; authority stays with the
> operator.