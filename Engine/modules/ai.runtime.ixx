module;

#include <cstdint>
#include <string>
#include <string_view>

export module ai.runtime;

export namespace epochengine::ai
{
    enum class ProviderMode : std::uint8_t
    {
        OpenSourceLocal = 0
    };

    struct ModelManifest
    {
        std::string id{};
        std::string display_name{};
        ProviderMode provider{ProviderMode::OpenSourceLocal};
        std::string endpoint{};
        std::string manifest_path{};
        bool repo_safe_manifest{true};
        bool local_weights_only{false};
        bool available{false};
    };

    [[nodiscard]] inline std::string_view provider_mode_name(ProviderMode) noexcept
    {
        return "operator-selected-local-model";
    }
}