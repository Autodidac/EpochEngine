# Backend Menu Overlay Status

This note tracks practical GUI parity across the currently active backends.

| Backend | Current status | Main caution |
| --- | --- | --- |
| OpenGL | Strong baseline | Keep context activation and atlas upload paths stable. |
| SDL | Usable | Present/renderer fault handling still deserves targeted regression coverage. |
| Raylib | Strong | Docking and framebuffer/logical-size interactions should keep being exercised. |
| SFML | Usable but delicate | OpenGL/state ownership is easier to disturb than in the other backends. |
| Software | Useful fallback | Best used for validation and smoke coverage, not primary shipping visuals. |
| Vulkan | Not parity-ready | Treat as experimental until the runtime path is fully finished. |

## Guidance

- Fix backend-specific GUI/render issues locally where possible.
- Keep atlas upload, clear/present behavior, and input scaling aligned across the active backends.
- Audit default shell backend behavior as well as parity: editor OpenGL,
  launcher software, explicit switching, and full teardown of inactive
  backends should all stay visible in backend review notes.
- Do not let retired archive-era UI code become the accidental source of truth again.
