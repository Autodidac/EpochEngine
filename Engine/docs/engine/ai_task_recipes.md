# AI task recipes — compact, reusable navigation guide

This document is a host-selected source-navigation aid for bounded AI
work. It is not the roadmap, a task approval, a verified code excerpt, a build
receipt, or an instruction to bypass the engine's sandbox/permission gates.
Read `Changes/active_pass.md`, `Changes/roadmap.md` and the owning architecture
contract for task scope. Use the current repository index and exact source bytes
before writing. A recipe names **likely** owners, not guaranteed APIs.

<!-- EPOCH_RECIPE_THEME_BEGIN -->
## GUI theme / palette additions
Likely owners: `Engine/modules/gui.engine.ixx` (`ThemeVariant`, `ThemePreference`),
`Engine/src/epochgui/gui.engine.cpp` (`GuiResources`, `active_palette`, sprite
construction, `registerPaletteControls`, `theme_preference_choices`,
`resolve_theme_preference`), and
`Engine/src/editor/editor.application.cpp` (`snapshot_theme_preference`).
Find all existing theme-switch/persistence sites by identifier, not by repeatedly
reading the entire editor source. Preserve existing enum numeric values; append
new preferences rather than shifting serialized values. Follow one existing
palette pattern. In the FIRST useful proposal include declaration, palette,
selector, resolver and persistence changes together where the exact current
source supports them. Run relevant GUI and editor contracts/builds. Do not
count palette discussion or read-only discovery as an edit.

The following map is executable navigation: each row names an admitted source
file, an inclusive first-line literal, and an exclusive end-line literal. Epoch
resolves these against the current candidate and keeps the whole packet resident
on every coding request. Update this map alongside source refactors. Never store
line numbers or fabricated source bytes here. Changes to this document use the
same exact-source candidate edits and human promotion review as engine code. Add recipes for other engine areas
as their ownership is established; the map is ordinary human-readable source.

SOURCE_REGION | Engine/modules/gui.engine.ixx | export enum class ThemeVariant | export class ScopedTheme
SOURCE_REGION | Engine/src/epochgui/gui.engine.cpp | struct GuiResources | std::array<SemanticSpritePair, gui_lib::semantic_tone_count> semanticDark
SOURCE_REGION | Engine/src/epochgui/gui.engine.cpp | [[nodiscard]] static const GuiResources::PaletteSprites& active_palette() | static_assert(gui_lib::semantic_tone_count
SOURCE_REGION | Engine/src/epochgui/gui.engine.cpp | g_resources.emberForge.windowBackground = | for (std::size_t index = 0u;
SOURCE_REGION | Engine/src/epochgui/gui.engine.cpp | std::span<const ThemePreferenceChoice> theme_preference_choices() | void push_theme(ThemeVariant
SOURCE_REGION | Engine/src/editor/editor.application.cpp | [[nodiscard]] static gui::ThemePreference snapshot_theme_preference | [[nodiscard]] static std::filesystem::path editor_layout_preferences_root
SOURCE_REGION | Engine/src/editor/editor.application.cpp | [[nodiscard]] static bool decode_editor_layout_preferences( | static void load_editor_layout_preferences(

The sprite block includes complete Ember Forge, Forest Terminal and Aurora Steel
examples and the rounded-control registrations. The palette member, lookup,
enums, choices, resolver, snapshot validation and decoder must change together.
The Settings selector consumes theme_preference_choices generically. The decoder
currently bounds the persisted value by ThemePreference::Dark: append new enum
values and update that bound. Do not inspect unrelated editor state or input
headers just to add a palette. Submit the coherent edit using this complete
packet; only request a dependency that is actually absent.
<!-- EPOCH_RECIPE_THEME_END -->

<!-- EPOCH_RECIPE_SELF_ITERATION_BEGIN -->
## Engine self-coding and source edits
Read `Engine/docs/engine/ai_source_discovery_architecture.md` and
`Engine/docs/engine/os_ai_tooling_and_evidence_policy.md`. Likely owners:
`Engine/src/editor/ai/editor.ai_development_panel.cpp`,
`ai.source_workspace`, `ai.source_index`, proposal codec, stager and
`ai.self_iteration_orchestrator`. Reuse retained verified source and the
current task/repair receipt. Batch only genuinely missing dependency windows.
Produce an actual exact-preimage patch, sandbox files and compiler receipts.
Stop with a clear remaining dependency when insufficient; never loop through
new files to impersonate coding progress. Keep source edits, candidate launch,
and promotion as separately proved actions.
<!-- EPOCH_RECIPE_SELF_ITERATION_END -->

<!-- EPOCH_RECIPE_MODEL_RUNTIME_BEGIN -->
## Local inference and model hosting
Read `Engine/docs/engine/ai_model_runtime_and_chat_plan.md` and
`Engine/docs/engine/os_ai_tooling_and_evidence_policy.md`. Use Epoch-owned
model selection, permissions, event/transport and GUI services. Qwen Next Flash
uses EngCoder/QwenFlashRuntime's specific manifest, cache and loader: do not
pretend it is an ordinary GGUF loader or redirect it to Epoch's model cache.
Other Qwen/Nemotron model families use their actual compatible backend adapters.
Separate configured GPU offload from observed loaded-runtime layers/statistics;
never change offload on suspicion alone. Test installed-model discovery,
loading, streaming, cancellation and unload independently.
<!-- EPOCH_RECIPE_MODEL_RUNTIME_END -->

<!-- EPOCH_RECIPE_AI_CHAT_BEGIN -->
## AI chat and activity windows
The existing EpochGui `ConsoleWindowOptions` is the native visual owner. Its
role-aware message bubbles, flat Model / Options controls and multiline prompt
must not be replaced with EngCoder's web or SDL interface. Maintain append-only
meaningful activity and finished-response blocks; coalesce only exact duplicate
statuses. Raw provider output belongs in a separate user-selectable view, not
into validated command execution. Preserve full process/model error details and
never present stream byte counters as generated tokens.
<!-- EPOCH_RECIPE_AI_CHAT_END -->

<!-- EPOCH_RECIPE_BUILD_BEGIN -->
## Build and resource repairs
Read `Engine/docs/build/build_configuration_flags.md` and
`Engine/docs/build/local_build_scripts_and_release_packaging.md`. Respect
existing Debug/Release/ReleaseWithDebugInfo output ownership, C++23 modules,
`Engine/src/build/build.cpp_feature_probe.hpp` and resource-script encoding.
Verify the first causal compiler/RC error before touching implementation;
subsequent missing libraries may only be effects of the initial error.
Preserve source-owned inputs while excluding disposable build artifacts.
<!-- EPOCH_RECIPE_BUILD_END -->
