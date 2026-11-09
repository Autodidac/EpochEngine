# Extensions 0.2.3 — local release preparation

This source update preserves the existing optional FFT ocean and planetary
terrain work from the current Extensions owner. It includes bounded ocean
admission/evaluation, the reviewed EpochPlanet subset and existing planetary
module, export and controller sources. Importing these sources adds no service,
model, renderer or network listener, and does not activate any package.

Packaging now requires this repository's own physical `.git` directory and
the exact Site origin. Git calls select that directory and work tree explicitly;
a detached Extensions folder cannot discover an enclosing Engine repository.
Archive generation preserves exact committed bodies and file modes regardless of the user's
Git newline settings; the reviewed upstream subset retains its provenance
bytes and line endings. Source receipts and package source links identify the Site
repository. The manifest validation wrapper now preserves its validator's exit.
Six ownership/archive regression tests cover those paths.

Windows Debug and Release standalone builds and all 20 CPU CTest contracts pass
against the selected current Engine 0.90.185 `render.math` and
`terrain.foundation` owners. The exact source identities and build receipts are
retained by the preparing checkout. This is source/CPU evidence; hosted checks,
Linux/macOS and native Package Manager/renderer activation remain separate.

The existing forest sampler and its contract are retained. The incomplete
optional Forest mesher CMake/README claims are omitted because their module and
consumer are absent from this source owner. The original working source and
unfinished extraction mission remain preserved by the Engine checkout. This
release does not complete Forest Factory or Plant Lab extraction.

Prior tags and published archives remain immutable. This preparation creates no
GitHub push, Site upload, activation or publication. No SelfCoding source,
credentials, private state, model weights or build products belong in its source
archives.
