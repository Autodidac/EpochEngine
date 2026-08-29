# EpochEngineExtensions

EpochEngineExtensions is the public descriptor repository for optional Epoch
packages and demonstrations that do not belong in the EpochEngine core source
tree.

The current checkpoint is intentionally descriptor-only. It records stable
package identities, capability areas, platform intent, and reconstruction keys.
It does not contain installable package payloads, native plugins, model weights,
servers, listeners, or generated assets. Consumers must treat every entry as
planned until a later immutable payload is separately admitted with its own
license, checksum, build evidence, and local operator approval.

`catalog/extensions.json` is the canonical machine-readable surface. It mirrors
the descriptor identities owned by EpochEngine's `extension.catalog` module.
The companion repository may grow reviewed payloads later without moving bulky
optional source into EpochEngine mainline.

## Validate

```powershell
cmake -S . -B build
ctest --test-dir build --output-on-failure
```

The validator proves that all eight expected descriptors are present, remain
non-installable, and cannot request automatic execution.

## License

Repository metadata is covered by the included `LICENSE`. Future package
payloads must include their own complete license and notice evidence before
publication.
