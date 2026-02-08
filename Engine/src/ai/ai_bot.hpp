#pragma once
#include "ai_types.hpp"
#include <string_view>

namespace epoch::ai {

class Bot {
public:
    struct Config {
        std::string backend;
        std::string endpoint;
        std::string model;
        std::size_t best_of = 1;
    };

    explicit Bot(Config cfg);
    BotReply submit(std::string_view user_input);

private:
    Config m_cfg;
};

} // namespace epoch::ai
