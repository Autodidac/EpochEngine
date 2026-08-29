# Tiered Terrain Demonstration

This package generates a deterministic, bounded local heightfield with discrete
terrace levels. It extends EpochEngine's existing terrain foundation; it does
not replace or duplicate `TerrainAssetId`, `HeightfieldDescriptor`,
`Heightfield`, surface queries, mesh plans, or terrain limits.

The package imports `terrain.foundation`. Its standalone CMake build requires
the host to select the authoritative EpochEngine module directory through
`EPOCH_ENGINE_MODULE_DIR`. The build also consumes the authoritative
`render.math` dependency from that same directory.

The generator is data-only:

- fixed-size integer coordinate hashing and integer terrace selection;
- bounded 5..129 sample axes and at most 16,641 samples;
- 2..32 discrete terraces with a caller-selected positive step height;
- a zero-height border and exact peak sample for stable authored bounds;
- propagation of the core terrain validator's rejection status;
- no renderer, native upload, collision solver, planetary terrain, service,
  listener, asset download, or automatic execution.

`source_available_local` in the companion catalog means only that these source
files exist in this repository. It is not a Site availability claim. A future
published package must have an immutable revision, archive checksum, license,
and independent build/admission evidence.
