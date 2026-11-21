# FFT Ocean Extension

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

`fft_ocean` is a renderer-neutral C++23 Epoch code extension. The package owns
a CPU spectral ocean tile that can be staged into `cache/packages/fft_ocean/`,
compiled by the Package Manager, and previewed by any active renderer through a
height/displacement/normal sample buffer.

This package is intentionally not part of the minimal EpochEngine clone. It is
for projects that opt into ocean simulation and accept the package build cost.

## Source

- `include/epoch/extensions/fft_ocean/fft_ocean.hpp`
- `src/fft_ocean.cpp`
- `tests/fft_ocean_self_test.cpp`

The implementation builds a deterministic Phillips-style initial spectrum,
evolves it over time, performs a 2D inverse FFT, and derives displacement,
normals, and foam hints for preview/render integration.

Enable `OceanSettings::spectralDisplacement` for rolling waves. The extension
derives horizontal travel from the direction and phase of each height mode,
so the two faces converge toward the crest. It packs both horizontal components
into one additional inverse FFT and reuses the height transform's scratch space.
Setting choppiness to zero skips the extra transform. The default standalone
mode retains the original slope-based displacement for existing callers.

## Bounded evaluation

`admit(settings, limits)` validates settings before allocation and reports vector
payload bytes for resident state, one evaluation and an equal-size replacement.
Resolution is a power of two from 8 through 512; the absolute working payload cap
is 64MiB. Caller limits can narrow those bounds. Allocator bookkeeping is outside
the reported payload. `reset` also counts the actual retained vector capacities,
so replacing an existing tile can require more than constructing a fresh tile.

`reset` and `evaluate` return `OceanResult` without throwing. Failed settings,
time, memory or output admission retains the previous complete settings/samples.
Construction retains its throwing failure contract. All settings and time must
be finite; physical scalar magnitudes are nonnegative, with positive tile length,
gravity and normal strength. Invalid sampling coordinates return a neutral sample.
The evaluator still uses absolute time and the original spectrum/FFT arithmetic;
it does not accumulate foam history. There is no cancellation callback in this
synchronous bounded API, no GPU compute implementation and no caustic lighting.
The Planetary Terrain extension selects spectral displacement for both initial
mesh generation and timed water updates. Its shared water sampler owns spherical
projection, shoreline clipping inputs and LOD transitions.
