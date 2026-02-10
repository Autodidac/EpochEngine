#pragma once
#include <string>
#include <vector>

namespace epoch::ai {

struct Candidate {
    std::string text;
    double score = 0.0;
};

struct BotReply {
    std::string text;
    double score = 0.0;
    std::vector<Candidate> alternatives;
};

} // namespace epoch::ai
