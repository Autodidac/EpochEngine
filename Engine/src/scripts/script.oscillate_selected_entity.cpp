/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch Project.   *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 ************************************************/
#include "scripting.epoch_api.h"

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

    if (!host->attach_selected_entity_oscillator)
    {
        host_log(host, "oscillate_selected_entity: oscillator host callback unavailable.");
        return;
    }

    // Select a Cube (or another movable scene object) before running this script.
    // X axis, +/-2 world units, one complete cycle every two seconds.
    constexpr int axis_x = 0;
    constexpr float amplitude = 2.0f;
    constexpr float frequency_hz = 0.5f;

    if (host->attach_selected_entity_oscillator(
            host->user_data,
            axis_x,
            amplitude,
            frequency_hz) != 0)
    {
        host_log(host, "oscillate_selected_entity: attach failed; select a movable scene object first.");
        return;
    }

    host_log(host, "oscillate_selected_entity: attached. Press Play, then enable A/B Render Benchmark in Global Settings.");
}
