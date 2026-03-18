# Epoch v0.82.24 Release Notes

## Highlights

- Fixed the updater to query the latest actual GitHub release instead of the
  `main` branch version file.
- Keeps version detection aligned with the downloadable `main.zip` release
  asset, which prevents `404` mismatches when `main` is ahead of the latest
  packaged release.
