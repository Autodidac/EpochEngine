#include "../Engine/src/editor/ai/editor.ai_source_discovery_budget.hpp"

using namespace epochengine::editor_ai_development_panel::discovery_budget;
static_assert(initial_rounds == 3u);
static_assert(!prioritize_edit(0u, 0u));
static_assert(!prioritize_edit(1u, 0u));
static_assert(prioritize_edit(2u, 0u));
static_assert(prioritize_edit(3u, 0u));
static_assert(prioritize_edit(4u, 0u));
static_assert(prioritize_edit(4u, 1u));
static_assert(prioritize_edit(6u, 2u));
static_assert(prioritize_edit(9u, 3u));
static_assert(focus_after_rounds(400u) == 9u);
int main() { return 0; }
