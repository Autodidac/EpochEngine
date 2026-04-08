module;

#include <cstdint>
#include <string>
#include <string_view>

export module ai.runtime;

export namespace epoch::ai
{
    enum class ProviderMode : std::uint8_t
    {
        EmbeddedTiny = 0,
        McpOperations,
        LmStudioOracle
    };

    struct ModelManifest
    {
        std::string id{};
        std::string display_name{};
        ProviderMode provider{ ProviderMode::EmbeddedTiny };
        std::string endpoint{};
        std::string manifest_path{};
        std::string tokenizer_path{};
        std::string checkpoint_path{};
        bool repo_safe_manifest{ true };
        bool local_weights_only{ true };
        bool available{ false };
    };

    [[nodiscard]] inline std::string_view provider_mode_name(ProviderMode mode) noexcept
    {
        switch (mode)
        {
        case ProviderMode::EmbeddedTiny:
            return "embedded-tiny";
        case ProviderMode::McpOperations:
            return "mcp-operations";
        case ProviderMode::LmStudioOracle:
            return "lmstudio-oracle";
        default:
            return "unknown";
        }
    }
}
