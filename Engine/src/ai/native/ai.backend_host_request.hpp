// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
// Epoch host owns the workflow, tools, approval and compilation. This adapter
// admits one model completion, without entering the independent agent loop.
#include "ai.backend_model_model_provider.hpp"
#include <stdexcept>

namespace epoch_ai::model {
inline ModelRequest host_request(const json& body, std::string model,
    std::stop_token stop, std::chrono::steady_clock::time_point deadline) {
    ModelRequest request{};
    request.model=std::move(model);
    request.stop=stop;
    request.deadline=deadline;
    request.max_tokens=body.at("max_tokens").get<int>();
    for(const auto& message:body.at("messages")) {
        const auto& content = message.at("content");
        Message admitted{message.at("role").get<std::string>(), {}};
        if (content.is_string()) admitted.content = content.get<std::string>();
        else if (content.is_array() && content.size() <= 5u) {
            for (const auto& part : content) {
                const auto type = part.at("type").get<std::string>();
                if (type == "text") admitted.content += part.at("text").get<std::string>();
                else if (type == "image_url") {
                    const auto url = part.at("image_url").at("url").get<std::string>();
                    if (!url.starts_with("data:image/") || url.size() > 6u * 1024u * 1024u)
                        throw std::invalid_argument("Native image input requires bounded inline image data.");
                } else throw std::invalid_argument("Unsupported native message content part.");
            }
            admitted.content_parts = content;
        } else throw std::invalid_argument("Invalid native message content.");
        request.messages.push_back(std::move(admitted));
    }
    if(body.contains("tools"))request.tools=body.at("tools").get<std::vector<json>>();
    if(body.contains("tool_choice"))request.tool_choice=body.at("tool_choice");
    if(body.contains("parallel_tool_calls"))request.parallel_tool_calls=body.at("parallel_tool_calls").get<bool>();
    // The self-coding host hands its thinking mode through the native adapter.
    // Without this copy, the UI control is silently lost before LM Studio/QFR.
    if (body.contains("chat_template_kwargs")
        && body.at("chat_template_kwargs").is_object()) {
        const auto& flags = body.at("chat_template_kwargs");
        if (flags.contains("enable_thinking") && flags.at("enable_thinking").is_boolean())
            request.enable_thinking = flags.at("enable_thinking").get<bool>();
    }
    return request;
}
inline json host_response(const ModelResponse& response) {
    if (response.raw.contains("choices")) {
        for (const auto& choice : response.raw.at("choices"))
            if (choice.contains("finish_reason") && choice.at("finish_reason") == "length")
                throw std::runtime_error("Native model reached its output-token limit; incomplete output was not admitted");
    }
    json message{{"role","assistant"},{"content",response.text}};
    if(!response.tool_calls.empty()) {
        message["tool_calls"]=json::array();
        for(const auto& call:response.tool_calls)
            message["tool_calls"].push_back({{"id",call.id},{"type","function"},
                {"function",{{"name",call.name},{"arguments",call.arguments.dump()}}}});
    }
    json result{{"choices",json::array({json{{"message",std::move(message)}}})}};
    if (response.raw.is_object())
    {
        if (response.raw.contains("usage") && response.raw.at("usage").is_object())
            result["usage"] = response.raw.at("usage");
        if (response.raw.contains("timings") && response.raw.at("timings").is_object())
            result["timings"] = response.raw.at("timings");
    }
    return result;
}
inline std::expected<ModelResponse, core::Error> complete_host_request(
    IModelProvider& provider, const json& body, std::string model,
    std::stop_token stop, std::chrono::steady_clock::time_point deadline,
    const StreamCallback& callback) {
    return provider.complete_stream(host_request(body, std::move(model), stop, deadline), callback);
}
}
