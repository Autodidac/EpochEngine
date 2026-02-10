#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_API_VERSION 2u

#if defined(_WIN32)
#  define APP_CALL __cdecl
#else
#  define APP_CALL
#endif

    struct app_callbacks_v1
    {
        uint32_t version;   // must be APP_API_VERSION
        uint32_t size;      // sizeof(struct app_callbacks_v1)
        void* user;      // host-defined context pointer

        int  (APP_CALL* on_init)(void* user);
        void (APP_CALL* on_tick)(void* user, uint64_t frame, double dt_seconds);
        bool (APP_CALL* should_quit)(void* user);
        void (APP_CALL* on_shutdown)(void* user);
    };

    const struct app_callbacks_v1* APP_CALL app_get_callbacks(void);

#ifdef __cplusplus
}
#endif
