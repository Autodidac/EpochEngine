#include "ai.backend_host_request.hpp"
#include <cassert>

using namespace epoch_ai::model;
class RecordingProvider final : public IModelProvider {
public:
    int calls{};
    ModelRequest captured{};
    std::expected<ModelResponse, epoch_ai::core::Error> complete(const ModelRequest& request) override {
        ++calls;
        captured = request;
        return ModelResponse{.text = {}, .tool_calls = {{"call_1", "epoch_propose_source_patch",
            json{{"source_id", 7}, {"replacement", "exact bytes\n"}}}}, .raw = json::object()};
    }
    ProviderCapabilities capabilities() const noexcept override { return {}; }
    std::expected<json, epoch_ai::core::Error> discover() override { return json::object(); }
};
int main() {
    std::stop_source cancellation;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
    const json schema{{"type", "function"}, {"function", {{"name", "epoch_propose_source_patch"},
        {"parameters", {{"type", "object"}, {"required", json::array({"source_id"})}}}}}};
    const json body{{"messages", json::array({{{"role", "system"}, {"content", "Epoch host contract"}},
        {{"role", "user"}, {"content", "reviewed source"}}})}, {"tools", json::array({schema})},
        {"tool_choice", "required"}, {"parallel_tool_calls", false}, {"max_tokens", 32768}};
    RecordingProvider provider;
    const auto response = complete_host_request(provider, body, "loaded-native", cancellation.get_token(), deadline, {});
    assert(response && provider.calls == 1);
    assert(provider.captured.model == "loaded-native" && provider.captured.messages.size() == 2);
    assert(provider.captured.messages[1].content == "reviewed source");
    assert(provider.captured.tools == std::vector<json>{schema});
    assert(provider.captured.tool_choice == "required" && provider.captured.parallel_tool_calls == false);
    assert(provider.captured.max_tokens == 32768 && provider.captured.deadline == deadline);
    cancellation.request_stop();
    assert(provider.captured.stop.stop_requested());
    const auto wire = host_response(*response);
    const auto call = wire.at("choices").at(0).at("message").at("tool_calls").at(0).at("function");
    assert(call.at("name") == "epoch_propose_source_patch");
    assert(json::parse(call.at("arguments").get<std::string>()).at("replacement") == "exact bytes\n");
    auto truncated = *response;
    truncated.raw = {{"choices", json::array({{{"finish_reason", "length"}}})}};
    bool rejected{};
    try { (void)host_response(truncated); } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
}
