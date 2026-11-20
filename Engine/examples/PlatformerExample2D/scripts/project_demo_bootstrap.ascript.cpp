#if __has_include(<scripting.epoch_api.h>)
#  include <scripting.epoch_api.h>
#elif __has_include(<include/scripting.epoch_api.h>)
#  include <include/scripting.epoch_api.h>
#else
#  error "Epoch script API header not found. Add Engine/include (preferred) or Engine/ to your include paths."
#endif

#include <string>

namespace
{
    void host_log(EpochScriptHost* host, const char* message)
    {
        if (host && host->log)
            host->log(host->user_data, message);
    }
}

EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)
{
    if (!host)
        return;

    host_log(host, "project_demo_bootstrap: project shell ready.");
    if (host->project_model_asset && host->project_model_asset[0] != '\0')
    {
        std::string message = std::string("project_demo_bootstrap: demo model asset -> ") + host->project_model_asset;
        host_log(host, message.c_str());
        if (host->queue_model_load)
        {
            const int result = host->queue_model_load(host->user_data, "mini_sponza_v2", host->project_model_asset);
            host_log(host, result >= 0
                ? "project_demo_bootstrap: queued demo model load."
                : "project_demo_bootstrap: backend rejected demo model load.");
        }
        else
        {
            host_log(host, "project_demo_bootstrap: model-load callback unavailable.");
        }
    }
    else
    {
        host_log(host, "project_demo_bootstrap: no demo model asset declared for this project.");
    }

    if (host->rotate_all_entities_yaw)
    {
        host->rotate_all_entities_yaw(host->user_data, 6.0f);
        host_log(host, "project_demo_bootstrap: applied a light +6 yaw demo step.");
    }
    else
    {
        host_log(host, "project_demo_bootstrap: rotate callback unavailable.");
    }
}
