// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Reusable project starter: copy into scripts/timeline_translation.ascript.cpp,
// edit these keys in your project, then compile/run the project-owned source.
#include "scripting.epoch_api.h"

EPOCH_SCRIPT_EXPORT int run_script_v2(const EpochScriptHostV2* host)
{
    if(!host||host->abi_version!=EPOCH_SCRIPT_HOST_ABI_V2
        ||host->struct_size<sizeof(EpochScriptHostV2)||!host->copy_selected_translation_track)return -1;
    // Offsets from the object's CURRENT authored position, in scene units.
    // This example moves, holds, and returns; it does not loop automatically.
    const EpochScriptTranslationKey keys[]{
        {0.0,{0.0,0.0,0.0}},
        {2.0,{0.0,1.0,0.0}},
        {4.0,{2.0,1.0,0.0}},
        {6.0,{2.0,1.0,0.0}},
        {8.0,{0.0,0.0,0.0}}
    };
    return host->copy_selected_translation_track(host->user_data,keys,
        static_cast<uint32_t>(sizeof(keys)/sizeof(keys[0])));
}
