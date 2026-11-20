#if __has_include(<scripting.epoch_api.h>)
#  include <scripting.epoch_api.h>
#elif __has_include(<include/scripting.epoch_api.h>)
#  include <include/scripting.epoch_api.h>
#else
#  error "Epoch script API header not found. Add Engine/include (preferred) or Engine/ to your include paths."
#endif

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

    host_log(host, "tool_bootstrap: project shell ready.");
}
