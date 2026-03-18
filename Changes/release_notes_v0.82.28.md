# Epoch v0.82.28 Release Notes

## Highlights

- Cleaned up the editor update confirmation modal with separate actions for
  packaged runtime updates and source-snapshot downloads.
- Added a deliberate source-snapshot path for advanced testing when `main` is
  ahead of the latest packaged release.
- Source snapshot downloads now extract beside the runtime and do not replace or
  rebuild the running binary.
