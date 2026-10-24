# EpochPlatformEngine v0.1.1

## DPI contract

- The Win32 driver now sizes the initial native window using the system DPI instead of a hard-coded 96 DPI.
- Every new Win32 context emits an initial `dpi_changed` event before normal frame processing.
- `dpi_changed` explicitly reports horizontal and vertical DPI in `WindowEvent::width` and `WindowEvent::height`.
- Existing per-monitor `WM_DPICHANGED` handling and suggested-window repositioning remain intact.

This allows renderer and GUI libraries to resolve logical font sizes correctly on the first frame, including 125%, 150%, 175%, and 200% Windows scaling.
