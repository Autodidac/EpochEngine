# EpochEngine integration

`EpochPlatformEngine` should be introduced as the native host beneath EpochEngine's existing renderer contexts, not as a renderer rewrite.

## Existing concepts preserved

EpochEngine already separates context types and backend implementations. `EpochPlatformEngine::ContextType` intentionally mirrors the existing `epochengine::core::ContextType` values:

```text
OpenGL, SDL, SFML, Raylib, Vulkan, DirectX, Software, Custom, Noop
```

The renderer modules remain in EpochEngine. The platform library only supplies the native host and thread-affinity contract.

## Mapping

| EpochEngine responsibility | EpochPlatformEngine replacement |
|---|---|
| Native parent/child window handles | `NativeWindowHandle` |
| `WindowData::guiRoute` | `ContextDesc::route` |
| Backend lifecycle atomics | `LifecycleState` |
| Owner-thread command queue | `Runtime::dispatch` |
| `MultiContextManager::AddWindow` | `Runtime::create_context` |
| `RemoveWindow` / retirement | `destroy_context` and stopped-context collection |
| `OpenDetachedContextWindow` | top-level/detached `WindowMode` context |
| Dock back / `SetParent` | `Runtime::reparent` |
| Win32/X11 event pump | Win32/X11 platform driver |
| Global active window pointer | explicit `ContextId` lookup |

## Renderer attachment sequence

1. EpochEngine requests a platform context with a route and renderer `ContextType`.
2. The platform driver creates the native window on its owner thread.
3. `ContextCallbacks::initialize` receives a published native handle.
4. EpochEngine creates its OpenGL/Vulkan/DirectX/other renderer context.
5. `ContextCallbacks::frame` invokes the existing backend frame path.
6. Native events arrive through the context event queue and optional callback.
7. Shutdown destroys renderer resources first.
8. The platform driver destroys the native window last.

## Driver separation

Do not merge renderer drivers into `EpochPlatformEngine`.

- Win32, X11, and Cocoa are platform drivers.
- OpenGL, Vulkan, DirectX, SDL, SFML, Raylib, and software are renderer/context drivers.
- One platform driver can host several renderer types simultaneously.

This preserves the engine's multi-context diagnostic grids and detached editor panes while removing duplicated native-window code from each renderer.

## Initial migration order

1. Add `EpochPlatformEngine` as a normal repository/project dependency without package-manager registration.
2. Route headless contexts first.
3. Route detached Win32 editor hosts.
4. Route normal Win32 parent/child context windows.
5. Route X11 windows.
6. Remove native handles and static global window state from the old platform layer only after parity tests pass.

The old and new paths can coexist during migration because renderer code receives opaque native handles rather than depending on the platform implementation type.

## Canonical module reconciliation (local validation)

`platform.native` re-exports the existing public header declarations and links
exactly the existing native implementation. CMake targets
`EpochPlatformEngine::ModuleStatic` and `ModuleShared` select the respective
static/shared ABI. Header users and existing entrypoint archives remain supported.
The module CPU consumers exercise the actual headless driver without starting a
second Runtime scheduler. Native desktop behavior is not established by this test.

The Engine extraction is not a replacement for this repository. Before importing
both APIs into Engine, move its distribution/archive/topology policy into its
existing `epochengine::platform::policy` adapter namespace. The canonical public
`RuntimePlatform` remains `uint8_t` with Unknown=0, Windows=1, Linux=2, MacOS=3.
The Engine extraction previously used different values and size: that adaptation
requires recompiling consumers and must not reinterpret persisted ordinals. Its
`epochengine::core::ContextType` is a separate API and must remain unchanged.

The current Engine registry and per-context render threads remain the owners.
Native reuse should start at `IPlatformDriver`, not by starting this repository's
`Runtime::DriverHost` beside Engine's multiplexer. Existing-window admission and
scoped pumping need explicit contracts before adopting SDL/SFML/Raylib surfaces.
The present Win32 driver pumps the current thread queue and owns created windows;
it is not yet a safe non-owning adapter for those existing surfaces.
