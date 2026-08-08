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
module;

#include <cstdint>
#include <vector>

#define EPOCH_APPLICATION_MODULE(NAME)                                       \
    static void NAME##_init    () noexcept;                                  \
    static void NAME##_update  (float) noexcept;                             \
    static void NAME##_shutdown() noexcept;                                  \
    static epochengine::application_module NAME##_desc {                 \
        &NAME##_init, &NAME##_update, &NAME##_shutdown                       \
    };                                                                       \
    static epochengine::_module_registrar NAME##_auto { &NAME##_desc };  \
    static void NAME##_init() noexcept

#ifndef EPOCH_APPLICATION_MODULE
#define EPOCH_APPLICATION_MODULE(NAME) EPOCH_APPLICATION_MODULE(NAME)
#endif

export module application.registry;

export namespace epochengine
{
    // Contract
    struct application_module {
        void (*init)    () noexcept = nullptr;
        void (*update)  (float dt)  noexcept = nullptr;
        void (*shutdown)() noexcept = nullptr;
    };

    // Internal registry (header-only, hidden from public API)
    namespace detail {
        inline auto& get_modules() {
            static std::vector<application_module*> list;
            return list;
        }
    }

    // Registrar helper (automatic push into the list)
    struct _module_registrar {
        explicit _module_registrar(application_module* m) noexcept {
            detail::get_modules().push_back(m);
        }
    };
}
