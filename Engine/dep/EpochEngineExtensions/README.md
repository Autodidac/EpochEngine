# EpochEngineExtensions

EpochEngineExtensions is the public companion source repository for optional
Epoch packages and demonstrations that do not belong in the EpochEngine core
source tree.

The repository remains descriptor-only except for the bounded local source
package at `demonstrations/tiered_terrain`. That package generates deterministic
tiered local heightfields through EpochEngine's authoritative
`terrain.foundation` module. It does not implement a renderer, native plugin,
planetary terrain, server, listener, generated asset bundle, or automatic
execution path.

Local source availability is not Site package availability. The tiered terrain
source must still receive an immutable revision, archive checksum, license
evidence, build evidence, and explicit local admission before a host can claim
an installable published package.

`catalog/extensions.json` is the canonical machine-readable surface. It mirrors
the stable identities owned by EpochEngine's `extension.catalog` module while
distinguishing the one local source package from seven planned descriptors.
EpochEngine's embedded catalog remains the publication/admission boundary; this
source checkpoint alone does not make the package available through the Site.

## Validate

```powershell
cmake -S . -B build `
  -DEPOCH_ENGINE_MODULE_DIR=C:/path/to/EpochEngine/Engine/modules
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

The explicit module directory is mandatory. The build consumes
`render.math.ixx` and `terrain.foundation.ixx` from that directory and never
defines substitute terrain identities. The contracts prove repeatability,
changed-seed divergence, discrete terraces, exact bounds, package capacity
limits, propagation of core rejection, and the absence of automatic execution,
native-renderer, or planetary claims.

## License

Repository metadata is covered by the included `LICENSE`. Future package
payloads must include their own complete license and notice evidence before
publication.
