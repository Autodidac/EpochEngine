# Editor-free software base

`Epoch::SoftwareBase` is a small C++23 application composition using the existing
`app_callbacks_v1`, engine logger and `platform.window` abstraction. It does not
link the engine/editor entry, EpochGui, renderers, updater, projects, AI or child
process services. This is a foundation for software applications, not a rendered
editor or a completed EpochPlatformEngine extraction.

The application explicitly selects one profile:

- `cli`: callbacks only; no window factory, GUI or graphics context is called.
- `platform-window`: one owned Win32 native window, with created/resize/close
  notifications and callbacks on the calling thread. Other platforms return
  unsupported rather than substituting the existing null window system. This
  profile does not expose keyboard/pointer/text routing, drawing, native present
  or capture. Only one window session may be active in this software base.

The created event echoes requested outer window dimensions; resize events report
native client dimensions. Neither is framebuffer or presentation evidence.

Both profiles use the shared context-profile preflight contract. No process,
thread, attachment, rendering or pixel evidence is invented by preflight.

## Build

Use a fresh build directory with an installed module-capable MSVC or Clang/CMake
toolchain. For a Windows Visual Studio generator:

```powershell
cmake -S . -B build/software-base -G "Visual Studio 17 2022" -A x64 -DEPOCH_SOFTWARE_BASE_ONLY=ON -DBUILD_TESTING=ON
cmake --build build/software-base --config Release --target epoch_software_app epoch_software_application_contract epoch_context_admission_contract --parallel 1
ctest --test-dir build/software-base -C Release -R "epoch_(software_|context_admission_contract)" --output-on-failure
```

The generated executable is `build/software-base/Engine/Release/EpochSoftware.exe`.
The CMake-generated Visual Studio targets own these new sources; they are not
added to the legacy shared engine source list.

```powershell
EpochSoftware.exe --profile cli --ticks 3
EpochSoftware.exe --profile platform-window --ticks 180 --interval-ms 16
```

The second command opens a native window and requires the operator's native-run
authorization. It is intentionally not an automatic CTest. Closing the window
also exits cleanly; omitting `--ticks` leaves this profile running until closed.
The base has no asset-directory requirement because it loads no engine assets.

## Lifecycle and evidence

The first tick runs without a warmup gate. Successful callbacks receive
monotonic elapsed seconds and zero-based tick indices. `on_shutdown`, if supplied,
is called once after attempted initialization, including partial initialization
failure. Callback exceptions are contained; shutdown precedes owned-window
release. A window observer cannot throw through the native noexcept event pump.

The focused contract uses an in-memory window-system implementation to test
ordering, failure cleanup, foreign-window event rejection and overlapping-session
refusal. It creates no native windows and does not prove native pixels or input.
`window_released` reports release of the owned lease, not an independent native
handle-destruction or presentation attestation. Native visibility, resize, close
and process/handle cleanup remain separate exact-build tests.

The separate Windows target `epoch_platform_window_native_contract` is excluded
from the default build and is never registered with CTest. After explicit native
test authorization, build it and run:

```powershell
epoch_platform_window_native_contract.exe --native-window-contract
```

It creates hidden real Win32 windows and checks initial/default/UTF-8 captions,
title changes, actual client resize events, callback-driven resize deferred to
the next pump, no duplicate notifications, cancellation during delivery, deferred
close and complete owned handle retirement/recreation. Running without the
explicit argument creates no window. This test uses no renderer and does not
attest visual/pixel output.
