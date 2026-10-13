#include "ai.backend_models_model_registry.hpp"
#include <filesystem>
#include <string>

// Pure registry persistence; no provider, process, model load or training job.
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const std::filesystem::path root{argv[1]};
    epoch_ai::core::Config config{};
    config.model="foundation-b";
    config.controller_model="controller-a";
    config.addons.push_back({.id="cpp",.display_name="C++",.model="specialist-c"});
    epoch_ai::models::ModelRegistry models{config,root};
    if(!models.set_foundation_model("foundation-b")
        ||!models.set_role_model("controller","foundation-b")
        ||!models.set_role_model("cpp","specialist-c"))return 2;
    if(models.set_role_model("controller","unregistered")
        ||models.set_role_model("foundation","controller-a"))return 3;
    epoch_ai::models::ModelRegistry reloaded{config,root};
    if(reloaded.controller_model()!="foundation-b"
        ||reloaded.role_model("cpp")!="specialist-c"
        ||reloaded.selected_foundation_model()!="foundation-b")return 4;
    if(!reloaded.set_role_model("controller","")
        ||reloaded.controller_model()!="controller-a")return 5;
    reloaded.refresh();
    if(!reloaded.role_model("controller").empty()
        ||reloaded.role_model("cpp")!="specialist-c"
        ||reloaded.selected_foundation_model()!="foundation-b")return 6;
    return 0;
}
