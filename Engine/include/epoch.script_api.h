#pragma once

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
};
