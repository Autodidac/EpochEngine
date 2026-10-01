# EpochEngine v0.90.28

Adds a repeatable renderer comparison harness for the OpenGL event-driven scene path.

- A/B mode alternates normal selective reconstruction and forced full-cache reconstruction every frame.
- Both lanes use the same persistent scene cache and final presentation blit.
- Benchmark timing is GPU-synchronized at the scene boundary and excludes optional dirty/vacated debug overlays.
- Per-lane sample count, accumulated time, last/average time, and comparison speedup are visible in World Settings.
- `Oscillate Selected Entity` attaches a transient X-axis +/-2 unit, 0.5 Hz preview oscillator to the selected movable entity for deterministic localized motion.

The legacy Event renderer toggle remains available to force the separate conventional path, but that path is not used by the alternating benchmark because bypassing cache updates would make the following selective frame invalid.
