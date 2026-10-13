// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
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
    for(const auto& message:body.at("messages"))
        request.messages.push_back({message.at("role").get<std::string>(),
            message.at("content").get<std::string>()});
    if(body.contains("tools"))request.tools=body.at("tools").get<std::vector<json>>();
    if(body.contains("tool_choice"))request.tool_choice=body.at("tool_choice");
    if(body.contains("parallel_tool_calls"))request.parallel_tool_calls=body.at("parallel_tool_calls").get<bool>();
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
    return json{{"choices",json::array({json{{"message",std::move(message)}}})}};
}
inline std::expected<ModelResponse, core::Error> complete_host_request(
    IModelProvider& provider, const json& body, std::string model,
    std::stop_token stop, std::chrono::steady_clock::time_point deadline,
    const StreamCallback& callback) {
    return provider.complete_stream(host_request(body, std::move(model), stop, deadline), callback);
}
}
