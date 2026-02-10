/**************************************************************
 *   Epoch Engine - AI Integration (LM Studio)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/_epoch.stl_types.hpp"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winhttp.h>
#   pragma comment(lib, "winhttp.lib")
#endif

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>

export module epoch.ai;

import core.log;

export namespace epoch::ai
{
    struct Candidate
    {
        std::string text;
        double score = 0.0;
    };

    struct BotReply
    {
        std::string text;
        double score = 0.0;
        std::vector<Candidate> alternatives;
    };

    class Bot
    {
    public:
        struct Config
        {
            std::string backend = "lmstudio_chat";     // currently only lmstudio_chat
            std::string endpoint = "http://localhost:1234"; // base or full
            std::string model = "arliai_glm-4.5-air-derestricted";
            std::size_t best_of = 1;
        };

        explicit Bot(Config cfg);
        [[nodiscard]] BotReply submit(std::string_view user_input);

    private:
        Config m_cfg{};
        std::string m_endpoint_full{}; // normalized to /api/v1/chat
    };

    // Engine-global service wrapper (simple singleton)
    void init_bot();
    void shutdown_bot();
    void append_training_sample(std::string_view prompt, std::string_view answer, std::string_view source = "win32_chat_panel");
    [[nodiscard]] std::string send_to_bot(const std::string& user_text);
    [[nodiscard]] std::string default_workspace_root();
}
