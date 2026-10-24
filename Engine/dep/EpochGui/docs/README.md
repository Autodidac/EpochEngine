
## v0.90.54 chat surface styling

Reusable chat spacing, composer radius and run-detail metrics live in the
`epoch.gui` module's `gui_lib::chat_surface` namespace. The existing GUI bridge
provides a platform-neutral per-message run-information struct and an optional
collapsed/expanded presentation for ordinary assistant messages; it receives
only data the editor supplies. Engine model routing remains editor-owned.
