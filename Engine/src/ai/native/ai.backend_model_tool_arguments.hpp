// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace epoch_ai::model {

// Tool arguments are data, not a recoverable program. Reject duplicate object
// members as soon as their key finishes, before a looping generation can spend
// the entire output allowance or the JSON parser can silently keep its last value.
// This scanner only enforces member uniqueness; the normal JSON/schema parsers
// remain responsible for complete syntax and tool authority.
class ToolArgumentGuard final {
public:
    void append(std::string_view fragment) {
        for (const char c : fragment) {
            if (in_string_) {
                if (key_string_) {
                    key_.push_back(c);
                    if (key_.size() > 65'536u)
                        throw std::runtime_error("Tool argument object key exceeds the protocol limit.");
                }
                if (escaped_) { escaped_ = false; continue; }
                if (c == '\\') { escaped_ = true; continue; }
                if (c != '"') continue;
                in_string_ = false;
                if (key_string_) {
                    const auto decoded = nlohmann::json::parse(key_).get<std::string>();
                    auto& object = frames_.back();
                    if (!object.keys.insert(decoded).second)
                        throw std::runtime_error("Repeated JSON key in tool arguments: "
                            + decoded.substr(0u, 96u)
                            + ". Generation stopped; no tool was executed.");
                    object.expects_key = false;
                    key_.clear();
                }
                continue;
            }
            if (c == '"') {
                in_string_ = true;
                key_string_ = !frames_.empty() && frames_.back().object
                    && frames_.back().expects_key;
                if (key_string_) key_ = "\"";
            } else if (c == '{' || c == '[') {
                if (frames_.size() >= 512u)
                    throw std::runtime_error("Tool argument JSON nesting exceeds the protocol limit.");
                frames_.push_back(Frame{.object = c == '{', .expects_key = c == '{'});
            } else if (c == '}' || c == ']') {
                if (frames_.empty() || frames_.back().object != (c == '}'))
                    throw std::runtime_error("Unbalanced JSON in tool arguments.");
                frames_.pop_back();
            } else if (c == ',' && !frames_.empty() && frames_.back().object) {
                frames_.back().expects_key = true;
            }
        }
    }

private:
    struct Frame final {
        bool object{};
        bool expects_key{};
        std::unordered_set<std::string> keys{};
    };
    std::vector<Frame> frames_{};
    bool in_string_{};
    bool key_string_{};
    bool escaped_{};
    std::string key_{};
};

[[nodiscard]] inline nlohmann::json parse_tool_arguments(std::string_view arguments) {
    if (arguments.empty()) return nlohmann::json::object();
    ToolArgumentGuard guard;
    guard.append(arguments);
    return nlohmann::json::parse(arguments);
}

} // namespace epoch_ai::model
