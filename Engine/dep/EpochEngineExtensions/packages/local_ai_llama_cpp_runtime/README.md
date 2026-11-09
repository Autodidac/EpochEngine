# Local AI llama.cpp Runtime

This optional C++23 package plans and validates an operator-selected
`llama-cli` process. It is not a downloader, installer, model trainer, or
autonomous builder.

Epoch can discover an existing `llama-cli` executable and a separately licensed
GGUF model through executable-local cache paths or the
`EPOCH_LLAMA_CPP_EXECUTABLE` and `EPOCH_AI_MODEL_PATH` environment variables.
Epoch invokes the selected CLI as one captured child process with bounded output
and timeout handling. It never starts `llama-server`.

The package's current source-build support is deliberately plan-only. A future
executor must:

1. obtain visible operator approval for network access and compilation;
2. record and verify an immutable 40-character llama.cpp git revision;
3. retain the upstream MIT license and source provenance;
4. configure with `LLAMA_BUILD_SERVER=OFF` and `LLAMA_CURL=OFF`;
5. build only the required `llama-cli` target and retain build evidence.

Model weights are never included automatically. Each GGUF model has its own
license, provenance, and project opt-in requirements.