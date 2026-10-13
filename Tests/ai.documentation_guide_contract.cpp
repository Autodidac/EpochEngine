#include "../Engine/src/editor/ai/editor.ai_documentation_guide.hpp"
#include <cassert>
#include <filesystem>
#include <string_view>
using namespace epochengine::editor_ai_development_panel::documentation_guide;
int main()
{
    assert(recipe_name("Add a fire engine red theme") == "THEME");
    assert(recipe_name("Qwen Next Flash GPU offload") == "MODEL_RUNTIME");
    assert(recipe_name("Repair agentic source discovery") == "SELF_ITERATION");
    assert(recipe_name("Fix chat composer") == "AI_CHAT");
    assert(recipe_name("compile linker") == "BUILD");
    assert(recipe_name("an unrelated query").empty());
    constexpr std::string_view text =
        R"(<!-- EPOCH_RECIPE_THEME_BEGIN -->
Theme owner
<!-- EPOCH_RECIPE_THEME_END -->)";
    assert(extract_recipe(text, "THEME").find("Theme owner") != std::string::npos);
    assert(extract_recipe(text, "BUILD").empty());
    const auto directory = std::filesystem::path{EPOCH_GUIDE_TEST_ROOT};
    const auto guide = load_recipe(directory, "new gui theme palette");
    assert(guide.find("ThemeVariant") != std::string::npos);
    assert(guide.find("exact") != std::string::npos);
    assert(load_recipe(directory, "an unrelated objective").empty());
}
