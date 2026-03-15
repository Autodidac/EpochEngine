#include "epoch.script_api.h"

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

    host_log(host, "rotate_all_entities: compiling on demand.");

    if (!host->rotate_all_entities_yaw)
    {
        host_log(host, "rotate_all_entities: rotate callback unavailable.");
        return;
    }

    host->rotate_all_entities_yaw(host->user_data, 12.0f);
    host_log(host, "rotate_all_entities: applied +12 yaw to scene entities.");
}
