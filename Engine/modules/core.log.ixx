module;

#include "../include/_epoch.stl_types.hpp"

export module core.log;

export namespace epoch::core::log
{
    enum class level : std::uint32_t
    {
        trace = 0,
        info = 1,
        warn = 2,
        error = 3,
        off = 4,
    };

    struct kv
    {
        epoch::string_view key{};
        epoch::string_view value{};
    };

    // Configuration (process-global, but explicit).
    void set_level(level min_level) noexcept;
    level get_level() noexcept;

    void enable_console(bool on) noexcept;   // stdout
    void enable_debugger(bool on) noexcept;  // OutputDebugString on Windows, no-op elsewhere
    bool set_file(epoch::string_view utf8_path) noexcept; // append mode (UTF-8 path)
    void close_file() noexcept;

    // Log entry points.
    void write(level lvl, epoch::string_view tag, epoch::string_view msg);
    void write_kv(level lvl, epoch::string_view tag, epoch::string_view msg, epoch::array_view<const kv> fields);

    inline void trace(epoch::string_view tag, epoch::string_view msg) { write(level::trace, tag, msg); }
    inline void info(epoch::string_view tag, epoch::string_view msg) { write(level::info, tag, msg); }
    inline void warn(epoch::string_view tag, epoch::string_view msg) { write(level::warn, tag, msg); }
    inline void error(epoch::string_view tag, epoch::string_view msg) { write(level::error, tag, msg); }

    // C ABI bridge for non-module translation units (App project, tools, etc.)
    // Implemented in src/core.log.cpp.
    extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8);
}
