# EpochEngine v0.90.27

Adds a separate vacated-region diagnostic overlay to the event-driven OpenGL editor renderer.

- Previous object bounds are retained as their own diagnostic region set.
- Old and current bounds are still merged into the authoritative reconstruction damage set.
- Vacated regions render in cyan when enabled.
- The setting never disables cleanup of old pixels/depth.
