# Epoch v0.82.50

## Highlights

- Fixed the Windows source updater handoff so the detached worker now resolves the real live executable path, waits for the runtime to exit, replaces it from the rebuilt `x64/Debug` output, and relaunches the updated app cleanly.
- Moved native `vcpkg` and `MSBuild` output into updater log files so restore/build activity no longer dumps raw junk into the live console.
- Replaced hardcoded machine-local sample-project dependency paths with repo-relative vcpkg include/library locations so source snapshot builds are portable across machines.

## Notes

- The packaged updater release is `0.82.50`.
- The active source target on `main` is now `0.82.50`.
