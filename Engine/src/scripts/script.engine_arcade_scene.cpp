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

    host_log(host, "engine_arcade_scene: package exposes kernel-owned mini-runtime scenes.");
    host_log(host, "engine_arcade_scene: available scenes: snake,tetris,pacman,frogger,sokoban,match3,sliding,minesweeper,2048,sandsim,cellular.");
    host_log(host, "engine_arcade_scene: render target asset engine_arcade.screen is staged as a 512x512 sampled arcade surface.");

    if (!host->request_engine_scene)
    {
        host_log(host, "engine_arcade_scene: engine scene callback unavailable.");
        return;
    }

    const int result = host->request_engine_scene(host->user_data, "snake");
    if (result >= 0)
        host_log(host, "engine_arcade_scene: selected 'snake'; use Run to launch the engine-owned scene.");
    else
        host_log(host, "engine_arcade_scene: engine rejected built-in scene request.");
}
