# Compact runtime debug output — 2026-10-11

Based on the 2026-10-11 18:31–18:42 Editor debug capture (351 entries, approximately 103 KiB). 277 entries came from `host.context_win.cpp` window-state or frame-pacing messages, often while dragging a window edge. This is an output/readability correction, not a change to the AI self-iteration pipeline.

## Behavior

- Windows and Linux multiplexer implementations report window state at startup, immediately on important activity/focus/visibility/DPI changes, and once at the end of a resize burst (350 ms quiet period). Render state is still published **every** frame; only logging is deferred.
- Frame pacing configuration and retries operate as before. Identical finalized plans are no longer printed repeatedly. An actual finalized plan change is still logged immediately.
- Core log console output and Windows `OutputDebugStringA` now use `HH:MM:SS [LEVEL] tag: message`. Thread hashes are omitted there. Long INFO-only lines are summarized or shortened. INFO messages about source windows, pacing, and window size get purpose-built compact summaries.
- Warnings and errors **never** have their message text truncated, and preserve the reported source location. Existing full structured records (date, thread ID, message, key-value fields) are retained in the core log file; system-specific log files continue receiving their original full descriptions. No on-disk log schema change.
- The headless logger's output format is unchanged.

### Typical before / after

Before:

`2026-10-11 18:32:05 [INFO] [Context.Multiplexer.Win] [tid=...] - (host.context_win.cpp:5282) - Backend OpenGL pacing requested=target_hz configured_hz=120 effective=target_hz effective_hz=120 activity=foreground native_requested=false native_configured=false native_active=false cpu_wait=true.`

After:

`18:32:05 [INFO] Context.Win: OpenGL pacing target_hz @ 120 Hz`

Before:

`2026-10-11 18:32:05 [INFO] [Context.Multiplexer.Win] [tid=...] - (host.context_win.cpp:5237) - Backend OpenGL window state activity=foreground presentable=true logical=1600x1293 framebuffer=1600x1293 dpi_milli=1000 dpi_known=true visibility_known=true focus_known=true occlusion_known=true resize_generation=132.`

After:

`18:32:05 [INFO] Context.Win: OpenGL window foreground 1600x1293 (fb 1600x1293)`

The new resize gate prevents printing all intermediate sizes while the mouse is moving.

## Regression test

Standalone C++23 test (no engine modules or Windows SDK required):

```sh
g++ -std=c++23 -Wall -Wextra -Werror -pedantic Tests/logging/compact_runtime_logging_test.cpp -o compact_logging_test
./compact_logging_test
```

Covers compact formatting, INFO length capping, intact warnings/errors, suppression of intermediate resize measurements, final settled state, and immediate important transitions.

## MSVC verification still required

Build Debug, Release, and ReleaseWithDebugInfo on Windows. Run the Editor, resize its window rapidly, check that only the final dimensions appear after settling, verify WARN/ERROR text in Output and detailed fields in the retained logfile. No claim of a full Visual Studio build or an AI self-iteration campaign is made from the standalone regression test.
