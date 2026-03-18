# Epoch v0.82.17

## Highlights

- Consolidated the older versioned release-note markdowns into a single archive
  file so the `Changes/` folder keeps one historical notes surface while new
  releases continue as individual version files.
- Removed the accidentally tracked Windows `.exp` script-build artifact from
  the repo and added ignore coverage for `.exp` files and the local
  `Temp Script Test/` workspace.

## Verification

- Confirmed the accidental `.exp` artifact was removed from Git tracking
- Added ignore rules so the same class of local script-build outputs stops
  reappearing
- Refreshed the active release/version surfaces to `v0.82.17`
