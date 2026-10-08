#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string_view>

namespace
{
    std::mutex g_headlessLogMutex;

    constexpr std::string_view level_text(std::uint32_t level) noexcept
    {
        switch (level)
        {
        case 0u: return "TRACE";
        case 1u: return "INFO";
        case 2u: return "WARN";
        case 3u: return "ERROR";
        case 4u: return "OFF";
        default: return "UNKNOWN";
        }
    }

    void write_part(std::string_view text)
    {
        if (!text.empty())
        {
            std::fwrite(text.data(), 1u, text.size(), stdout);
        }
    }
}

extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8)
{
    const std::lock_guard lock{ g_headlessLogMutex };
    const std::string_view tag = tag_utf8 ? std::string_view{ tag_utf8 } : std::string_view{ "Epoch.HeadlessCI" };
    const std::string_view msg = msg_utf8 ? std::string_view{ msg_utf8 } : std::string_view{};

    write_part("[");
    write_part(level_text(lvl));
    write_part("] [");
    write_part(tag);
    write_part("] ");
    write_part(msg);
    write_part("\n");
    std::fflush(stdout);
}
