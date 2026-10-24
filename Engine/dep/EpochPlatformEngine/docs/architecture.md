# Architecture

## Ownership

`Runtime` is the public composition object. Its implementation owns:

- driver factories
- one `DriverHost` per active `DriverKind`
- context records
- lifecycle state
- per-context event queues

A `DriverHost` owns:

- one `IPlatformDriver`
- the driver's native event pump
- its command queue
- its owner thread when the driver permits dedicated threading
- frame callback execution for every context assigned to that driver

This keeps native APIs thread-affine without creating one operating-system event loop per renderer context.

## Context lifecycle

```text
pending
  -> initializing
  -> ready
  -> stopping
  -> stopped

initializing
  -> failed
  -> stopping
  -> stopped
```

Initialization runs after the native window exists and before the frame callback can execute. Shutdown runs before the native window is destroyed.

## Multi-driver concurrency

Contexts using the same platform driver share that driver's owner thread. Contexts using different drivers can run concurrently:

```text
Win32 host thread
    editor.opengl
    editor.vulkan
    floating.inspector

Headless host thread
    build.preview
    server.simulation
```

Renderer-specific worker threads may still exist above this layer. The platform owner thread remains the authority for native handles and native event processing.

## ABI boundary

The public classes use a PIMPL for `Runtime`. The shared library exports the stable public surface while keeping synchronization containers and operating-system implementation types private.

The internal entrypoint is always a static archive. A shared library cannot independently provide the executable's process bootstrap in a portable way. The entrypoint archive provides `main()` or `wWinMain()` and calls the selected static or shared runtime library.

## Custom drivers

A project can register a custom `IPlatformDriver` factory before creating a custom context. Factories are non-capturing function pointers so ownership and lifetime remain explicit.
