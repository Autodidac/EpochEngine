# v0.90.29 — Multi-selection, bounded lighting damage, EpochGui, and MSVC project pass

## Scope

This release moved several editor behaviors from one-off UI logic into reusable engine contracts.

### 3D multi-selection and batch editing

- The scene editor now owns a selection set plus one primary selection.
- `Shift + Left Click` toggles object membership without discarding the rest of the set.
- Dragging empty 3D space performs marquee/rectangle selection.
- Dragging one selected object moves the complete selected set.
- Group movement commits through one scene-document transaction so Undo treats the move as one authored operation.
- Batch creation can create and select four or eight cubes atomically for repeatable render tests.

### Renderer-neutral lighting invalidation

`render.lighting` now owns lighting damage semantics rather than the OpenGL preview inventing them locally.

- Ambient/environment and directional-light changes remain global invalidation because they can affect the complete visible scene.
- Point and spot lights publish conservative previous/current influence-volume damage.
- The OpenGL event renderer projects those bounded volumes to dirty rectangles.
- Other renderers may consume the same `LightingInvalidationFrame`; full-frame renderers remain correct without implementing partial reuse.

The lighting contract is conservative. It can request more reconstruction than necessary, but it must not omit pixels whose lighting may have changed.

### EpochGui presentation

- EpochGui gained a font-shadow atlas layer for readable text shadowing without domain-owned duplicate glyph code.
- Shared window/chrome shadowing was added at the GUI layer rather than per renderer.
- SDL3 and other backends keep their own backend presentation mechanics; the visual intent belongs to EpochGui.

### Multicontext input routing

The top strip of a detached/native child context is no longer consumed as an unconditional drag surface. Normal clicks are delivered to GUI controls; host movement from that strip requires the explicit host-drag modifier. This prevents top-row controls from becoming unclickable in multicontext mode.

### Oscillator discoverability

The render benchmark oscillator is exposed from the selected object's Properties surface under Movement / Benchmark with attach, detach, and source actions. The editable example also lives under the ProjectLauncher scripts folder so it is visible in normal project navigation.

### MSVC solution usability

`Engine.sln` includes the ProjectLauncher and exposes both `Debug | x64` and `Release | x64` configurations. This is configuration availability, not proof that every native build has completed successfully; build failures remain acceptance evidence for later fixes.
