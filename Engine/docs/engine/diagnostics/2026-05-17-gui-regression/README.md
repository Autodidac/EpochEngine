# 2026-05-17 GUI Regression Evidence

This folder preserves the visual evidence for the current editor GUI cleanup
batch. Keep these captures with the docs until the corresponding acceptance
gates are verified in a fresh local runtime run.

| Evidence | File | What it shows |
| --- | --- | --- |
| Codex capture before closing the left pane | [codex-before-close-missing-panes.png](codex-before-close-missing-panes.png) | Scene is alive, but Perspective title/chrome, Inspector, and AI Chat are missing or hidden. |
| Codex capture after the operator closed the left pane | [codex-after-outliner-close-title-visible.png](codex-after-outliner-close-title-visible.png) | Closing World Outliner exposes the Perspective title, proving at least part of the issue is layout/composition rather than dead scene rendering. Inspector and AI Chat remain blank. |
| Operator console AI duplication report | [operator-console-ai-duplicate-controls.png](operator-console-ai-duplicate-controls.png) | Console Dock AI tab still exposes control UI that should live in main GUI/Inspector surfaces. |
| Operator outliner popout report | [operator-outliner-popout-button.png](operator-outliner-popout-button.png) | World Outliner still shows a Popout button that should be removed from this pass. |
| Operator central AI and console duplication report | [operator-central-ai-and-console-duplication.png](operator-central-ai-and-console-duplication.png) | Central AI Sandbox and Console Dock duplicate AI visual/control surfaces. |
| Operator scrollbar extents smear report | [operator-scrollbar-extents-smear.png](operator-scrollbar-extents-smear.png) | Scrollbar/text smearing appears when a scrollable view is resized at an extremity. |
| Operator Systems controls in Console Dock report | [operator-systems-controls-in-console-dock.png](operator-systems-controls-in-console-dock.png) | Systems time/control widgets still appear in the Console Dock instead of only in the central Systems workspace. |

Acceptance notes:

- The Perspective title must remain visible with World Outliner open.
- Inspector and AI Chat must draw and respond in the normal editor layout.
- Console Dock tabs must be compact output/evidence views, not duplicate
  control surfaces.
- Scrollbar extents must not smear text or vertical rules at min/max scroll.
- Any future screenshot replacement should be captured from an asset-bearing
  local runtime output, preferably `x64/Debug` or `x64/Release`.
