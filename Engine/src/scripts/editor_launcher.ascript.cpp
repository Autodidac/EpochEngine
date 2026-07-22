/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
#include "epoch.script_api.h"

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

    host_log(host, "editor_launcher: legacy launcher script active.");

    if (host->project_model_asset && host->project_model_asset[0] != '\0')
    {
        const std::string message =
            std::string("editor_launcher: active demo model -> ") + host->project_model_asset;
        host_log(host, message.c_str());

        if (host->queue_model_load)
        {
            const int result = host->queue_model_load(
                host->user_data,
                "mini_sponza_v2",
                host->project_model_asset);
            if (result >= 0)
                host_log(host, "editor_launcher: queued demo model load.");
            else
                host_log(host, "editor_launcher: backend rejected demo model load.");
        }
        else
        {
            host_log(host, "editor_launcher: model-load callback unavailable.");
        }
    }
    else
    {
        host_log(host, "editor_launcher: no demo model declared for this project.");
    }

    if (!host->rotate_all_entities_yaw)
    {
        host_log(host, "editor_launcher: rotate callback unavailable.");
        return;
    }

    host->rotate_all_entities_yaw(host->user_data, 6.0f);
    host_log(host, "editor_launcher: applied +6 yaw to scene entities.");
}
