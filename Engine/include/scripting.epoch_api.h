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
#pragma once
#include <stdint.h>

#ifdef _WIN32
#  define EPOCH_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#  define EPOCH_SCRIPT_EXPORT extern "C"
#endif

struct EpochScriptHost
{
    void* user_data;
    void (*log)(void* user_data, const char* message);
    void (*rotate_all_entities_yaw)(void* user_data, float delta_degrees);
    int (*queue_model_load)(void* user_data, const char* debug_name, const char* path);
    const char* project_model_asset;
    int (*request_engine_scene)(void* user_data, const char* scene_id);
    int (*attach_selected_entity_oscillator)(
        void* user_data,
        int axis,
        float amplitude,
        float frequency_hz);
    int (*arrange_cubes)(void* user_data, unsigned columns,
        float spacing_x, float spacing_z, float origin_x, float origin_y, float origin_z);
    int (*attach_cube_oscillators)(void* user_data, unsigned count,
        int axis, float amplitude, float frequency_hz);
};

// A separate entry symbol prevents a new script from reading past an old host.
// Legacy run_script(EpochScriptHost*) and its exact structure remain unchanged.
struct EpochScriptTranslationKey
{
    double seconds;
    double offset[3];
};
struct EpochScriptHostV2
{
    uint32_t abi_version;
    uint32_t struct_size;
    EpochScriptHost* legacy;
    void* user_data;
    // Host/user_data are borrowed only during this synchronous entry. Scripts
    // must not retain them or invoke these callbacks from additional threads.
    // Copies at most 256 finite, ordered keys during run_script_v2. The host
    // binds the selected entity/project/scene and commits only on entry success.
    int (*copy_selected_translation_track)(void*,const EpochScriptTranslationKey*,uint32_t);
};
enum { EPOCH_SCRIPT_HOST_ABI_V2=2 };
