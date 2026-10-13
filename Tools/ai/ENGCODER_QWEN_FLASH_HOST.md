# Epoch / EngCoder 0.7.1+ Qwen Flash Next host boundary

## Current integration

- Epoch owns the editor UI, AI self-iteration pipeline, sandbox/source safety, and
  OpenAI-compatible client requests (`http://127.0.0.1:14321/v1`).
- A user-selected, headless **EngCoderServer** owns the Qwen Flash Next model
  download location, installed manifests, special native llama-server runtime,
  layer offload, KV/context settings, load/unload, and inference lifecycle.
- EngCoder's GUI is **not** bundled or used. An Epoch-hosted model does not use
  Epoch's generic local Qwen3.8 27B GGUF installer, Nemotron loader, or the
  Epoch Package Manager model cache.
- `Start Backend` launches a trusted EngCoderServer using the existing OS user
  profile and environment; it does not supply `--workspace` or substitute
  `USERPROFILE`/`LOCALAPPDATA`, `ENGCODER_DATA_ROOT`,
  `ENGCODER_MODEL_CACHE_ROOT`, `ENGCODER_QWEN_FLASH_SERVER`, or GPU values.
- `Connect EngCoder` can attach to an already-running backend (recommended).
  Model loading, downloads, and configuration still happen through EngCoder.
- An existing native model must be **loaded and advertised in `/v1/models`** before
  Epoch can select it. Model selection is explicit; do not guess Qwen model IDs.

## Operator quick check (Windows)

1. Run/configure your installed EngCoder 0.7.1+ instance and load Qwen Flash Next
   using EngCoder's model library and native loader.
2. Run `Tools/ai/check_engcoder_qwen_next.ps1` to inspect host health and model IDs.
3. In Epoch AI Controls, select `Connect EngCoder`, `Rescan Models`, and confirm
   the Qwen Flash Next model ID returned by `/v1/models`.
4. Run a short inference, then check normal and streaming completion before
   entrusting source edits to self-iteration.

## Deliberate limitations

This source package **does not contain EngCoder itself** or recreate its native
Qwen Flash runtime inside Epoch. Native hosting requires an installed headless
EngCoderServer 0.7.1+ and a Qwen Flash model loaded there. Full end-to-end
Windows Qwen load/inference/self-iteration validation is still required.

Epoch's legacy opt-in Qwen3.8 27B and Nemotron local packages are separate,
not fallback automatic downloads for Qwen Flash Next.
