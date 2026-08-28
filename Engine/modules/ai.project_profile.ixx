/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>

export module ai.project_profile;

export namespace epochengine::ai::project_profile
{
    enum class Provider : std::uint8_t
    {
        disabled,
        epoch_local_qwen38,
        external_mcp
    };

    struct Profile final
    {
        Provider provider{Provider::disabled};
        bool enabled{};
        std::string model_binding{};
        std::string runtime_binding{};
        std::string inference_transport{};
        std::string tool_protocol{};
        std::string endpoint_binding{};
        std::string self_iteration{};
        bool project_source_write{};
        bool engine_source_write{};
        bool operator_approval_per_iteration{true};
        bool auto_start{};
        bool server_or_listener{};
        bool weights_bundled{};
        bool external_provider_preserved{true};

        friend bool operator==(const Profile&, const Profile&) = default;
    };

    struct CodecResult final
    {
        bool accepted{};
        Profile profile{};
        std::string canonical_bytes{};
        std::string sha256{};
        std::string status{};
        bool migrated_legacy{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
    };

    [[nodiscard]] Profile make_profile(Provider provider);
    [[nodiscard]] CodecResult serialize_profile(const Profile& profile);
    [[nodiscard]] CodecResult parse_profile(std::string_view bytes);
    [[nodiscard]] bool run_contract();
}
