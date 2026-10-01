# Input focus, renderer labels, and AI protocol UI - v0.90.25

## Input ownership

Win32 `GetAsyncKeyState` is global OS state. Epoch now samples it only while an Epoch window owns the foreground process. Per-context input additionally requires that the focused native HWND belongs to that exact context, so a detached viewport, second monitor window, browser, terminal, or other Epoch pane cannot drive an unfocused scene/game context. Held state is cleared when Epoch loses foreground ownership.

The previous mouse-safe path had a fail-open fallback: after a failed focus test it could still return the global mouse state. That fallback is removed. Camera and project-play action handling now uses context-safe action queries instead of global `input::action_*`.
Mouse pressed-edge state now comes from the input engine instead of re-polling `GetAsyncKeyState`, preventing a held mouse button from becoming a new press every frame. The standalone Vulkan camera also requires its GLFW window to own focus before accepting mouse or WASD camera input.

## Event renderer diagnostics

`Frame totals` now spells out `Cached`, `Partial`, `Full`, and `Conventional`. These are cumulative frame-path counters, not frame titles.

## AI protocol privacy

Structured `EPOCH_*` authoring/tool/source packets are host protocol. They are never normal chat output. Project Assistant now suppresses a structured packet even if a transport/routing defect incorrectly delivers it on the conversation channel, including packets preceded by short token-progress/debug text.
