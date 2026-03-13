module;

#define EPOCH_APPLICATION_MODULE(NAME)                                       \
    static void NAME##_init    () noexcept;                                  \
    static void NAME##_update  (float) noexcept;                             \
    static void NAME##_shutdown() noexcept;                                  \
    static epochnamespace::application_module NAME##_desc {                 \
        &NAME##_init, &NAME##_update, &NAME##_shutdown                       \
    };                                                                       \
    static epochnamespace::_module_registrar NAME##_auto { &NAME##_desc };  \
    static void NAME##_init() noexcept

#ifndef ALMOND_APPLICATION_MODULE
#define ALMOND_APPLICATION_MODULE(NAME) EPOCH_APPLICATION_MODULE(NAME)
#endif

export module aapplicationmodule;

import <cstdint>;
import <vector>;

export namespace epochnamespace
{
    // â”€â”€â”€ Contract â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    struct application_module {
        void (*init)    () noexcept = nullptr;
        void (*update)  (float dt)  noexcept = nullptr;
        void (*shutdown)() noexcept = nullptr;
    };

    // â”€â”€â”€ Internal registry (header-only, hidden from public API) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    namespace detail {
        inline auto& get_modules() {
            static std::vector<application_module*> list;
            return list;
        }
    }

    // â”€â”€â”€ Registrar helper (automatic push into the list) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    struct _module_registrar {
        explicit _module_registrar(application_module* m) noexcept {
            detail::get_modules().push_back(m);
        }
    };
}
