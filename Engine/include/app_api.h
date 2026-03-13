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
