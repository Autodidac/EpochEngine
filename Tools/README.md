# Shared repository tooling

Every build uses these checkout-relative sources. Do not maintain copies in an
output folder, a candidate's cache, or a second tooling tree.

| Location | Owner |
| --- | --- |
| `Tools/build` | Shared version, packaging, provenance and release staging helpers |
| `Tools/source` | Source names/layout/filter/dependency validation and icon maintenance |
| `Tools/models` | Shared native model/runtime installation and health checks |
| `Tools/CMake`, `Tools/MSVC` | Build-system adapters over the same source tree |
| `Tests/build`, `Tests/editor`, `Tests/gui`, `Tests/packages`, `Tests/windows`, `Tests/logging` | Test sources and fixtures |


Run all Python checks on Windows or Linux with `python Tools/run_tests.py`, from
any current directory when using an absolute script path. CTest registers this
same runner when a Python3 interpreter is available. Compiler-dependent tests
report a skip when their required compiler is absent; that is not compiler proof.
The engine's build-safe `--engine-contract-self-test` remains a separate lane.

Run `python Tests/packages/test_forest_materialized_codec.py` for the standalone
Forest persistence contract. It compiles the actual library and module owners
with the installed Windows x64 MSVC toolchain in Debug and Release, then checks
the v1 golden data and explicit v2 reads. It starts no renderer or model and
installs nothing. Successful scratch builds are removed; failures retain their
logs under `build/contract-tests`. Other toolchains are not claimed by this lane.

`Engine/ai/manifests/` holds owned model metadata, rather than a second tools or
tests directory. Model weights and runtime caches are separate disposable data.
Generated build/test results belong in checkout-local `Artifacts/` and are excluded
from source packages; `Tools/build` is tracked tooling and must be included.

`zip_snapshot.bat` makes a numbered local source ZIP. It prunes generated outputs,
models/runtime state, saved projects and local agent folders before walking them.
It preserves real source folders such as `Tools/build` and verifies ZIP entries.
This does not count lines of code, remove every local secret, or produce the
physically stripped Public edition. Public exports must use the canonical
`Tools/build/package_epoch_source.py --edition public` and both source guards.
