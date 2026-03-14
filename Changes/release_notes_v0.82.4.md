# Epoch v0.82.4 Release Notes

- Restored the Vulkan editor scene draw so the viewport now renders the editor grid instead of a flat clear pass.
- Kept the Vulkan editor preview aligned with the OpenGL editor look using the same dark palette and a stable fixed preview camera.
- Tightened the SFML shutdown path so GPU atlas cleanup runs before the context dies, reducing close-time activation failures in mixed backend sessions.
