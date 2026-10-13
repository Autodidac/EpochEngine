# Epoch AI / EngCoder 0.7.1+ Qwen3.8-Flash-Next hosting integration

## Basis and actual boundary

- Latest available EngCoder release notes and patches in the user's Library: 0.7.1, October 9, 2026. EngCoder 0.7.1 adds a **50% new/reset GPU offload default** without changing existing profiles or the mapping; it maintains Qwen Live retention, chat streaming and native Qwen runtime management. A separately supplied live-output retention patch exists. **No entire EngCoder source checkout or server binary was available for this Epoch patch**.
- Epoch owns the sandbox, exact source edits, builds and operator review. EngCoder owns Qwen Next Flash GGUF runtime and weights. Epoch talks to EngCoder's OpenAI-compatible `http://127.0.0.1:14321/v1` using streaming `/v1/chat/completions`. Epoch does **not** invoke EngCoder `/api/tasks`, switch on training, override a saved GPU percentage, or start a listener on its own.
- EngCoder offers Qwen native serving when the operator has installed, configured, and selected the model. That is **not** proved merely by installing this Epoch source.

## Changes

1. Epoch's Model / Options panel now identifies the EngCoder 0.7.1+ / Qwen Next Flash hosting path and accepts an existing `EngCoderServer.exe`/`EngCoderServer` path, including a server installed outside Epoch. Startup remains explicit. `ENGCODER_SERVER_EXECUTABLE` can preselect the server path.
2. Existing per-user EngCoder cache/runtime location overrides are honored when they exist. Unavailable guessed files/directories are *not* passed to a new backend process; EngCoder can use its own runtime discovery. No weights are copied.
3. The model suggestion scoring prefers discovered Qwen Next Flash IDs to old coding models. The actual returned `/v1/models` ID must be selected and confirmed. The previous explicit selection is preserved; the default Epoch-local Qwen3.8 27B CLI remains available.
4. The coding-provider manifest now identifies Qwen3.8-Flash-Next GGUF as the preferred EngCoder-hosted lane and records its `qwen-community-1.0` license. Nemotron remains in the legacy optional list and existing package registry for backward compatibility.
5. `install_engcoder_backend.ps1` now refuses EngCoder source versions older than 0.7.1. `check_engcoder_qwen_next.ps1` inspects `GET /api/health` and `GET /v1/models` on loopback without starting a generation.

## Windows setup

1. Install a built EngCoder **0.7.1 or newer** `EngCoderServer.exe`, with the native Qwen Flash runtime, or build it from a complete local EngCoder checkout by running `Tools/ai/install_engcoder_backend.ps1 -SourceRoot <path-to-full-EngCoder-source>`. Existing model weights and GPU profile should stay where EngCoder already stores them.
2. In Epoch's AI Controls, point **EngCoder server executable** at the installed server, then use **Start Backend :14321**. If the server is already running independently, choose **Connect EngCoder** without starting a second process.
3. Load/configure Qwen3.8-Flash-Next in EngCoder if its model is not already available. Use `Tools/ai/check_engcoder_qwen_next.ps1` to verify the model ID the server advertises.
4. In Epoch: Connect EngCoder → Rescan Models → choose the returned Qwen ID → Confirm/Use Model. Streaming is the recommended EngCoder endpoint setting; keep previous explicitly saved settings unless changed by the operator.
5. First end-to-end acceptance: request a small isolated source change, and verify proposal, staged candidate edit, build, tests, human review, plus a Qwen Live view on the EngCoder host. `/v1/models` availability alone is not proof of successful inference or source changes.

## Not claimed

- No bundled native server, downloaded GGUFs, installer run, Windows/MSVC full compile, or live Qwen generation in this Linux inspection container.
- No automatic routing to `/api/tasks`, no training/learning/agent execution expansion, no inherited filesystem authority from EngCoder.
- Model-admission and editor launch still require explicit operator action.
