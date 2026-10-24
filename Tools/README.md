# Shared repository tooling

Every build uses these checkout-relative sources. Do not maintain copies in an
output folder, a candidate's cache, or a second tooling tree.

| Location | Owner |
| --- | --- |
| `Tools/build` | Shared version, packaging, provenance and release staging helpers |
| `Tools/source` | Source names/layout/filter/dependency validation and icon maintenance |
| `Tools/models` | Shared native model/runtime installation and health checks |
| `Tools/CMake`, `Tools/MSVC` | Build-system adapters over the same source tree |
| `Tests/build`, `Tests/editor`, `Tests/gui`, `Tests/windows`, `Tests/logging` | Test sources and fixtures |


Run all Python checks on Windows or Linux with `python Tools/run_tests.py`, from
any current directory when using an absolute script path. CTest registers this
same runner when a Python3 interpreter is available. Compiler-dependent tests
report a skip when their required compiler is absent; that is not compiler proof.
The engine's build-safe `--engine-contract-self-test` remains a separate lane.

`Engine/ai/manifests/` holds owned model metadata, rather than a second tools or
tests directory. Model weights and runtime caches are separate disposable data.
Generated build/test results belong in checkout-local `build/` and are excluded
from source packages; `Tools/build` is tracked tooling and must be included.
