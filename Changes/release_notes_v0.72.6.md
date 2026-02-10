# AlmondShell v0.72.6 Release Notes

## Highlights
- Bumped the engine metadata so `aversion.hpp`, runtime banners, and documentation all advertise v0.72.6 consistently.
- Cleaned up the README snapshot, engine analysis brief, and configuration flag guide to remove stale references to earlier snapshots and better describe the current module/back-end layout.
- Recorded the documentation refresh in the changelog and release notes to keep downstream packagers aligned on the new version.

## Upgrade Notes
- Reconfigure builds after pulling to pick up the updated version helpers and ensure cached artifacts do not report the older revision.
- Regenerate documentation (Doxygen or static site) if you publish the reference so the refreshed snapshot metadata appears in the output.
