/*
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

export module ai.image_generation;

export import platform.child_process;

export namespace epochengine::ai::image_generation
{
    inline constexpr std::string_view diffusion_package =
        "os_model_flux_2_klein_4b_q4_0";
    inline constexpr std::string_view text_encoder_package =
        "os_model_qwen_3_4b_image_encoder_q4_k_m";
    inline constexpr std::string_view decoder_package =
        "os_model_flux_2_small_decoder";

    struct Request final
    {
        std::string prompt{};
        std::string negative_prompt{};
        std::uint32_t seed{42u};
        std::uint32_t width{1024u};
        std::uint32_t height{1024u};
        std::uint32_t steps{4u};
    };

    struct Configuration final
    {
        std::filesystem::path executable{};
        platform::child_process::ExecutableIdentity expected_executable{};
        std::filesystem::path models_cache_root{};
    };

    struct Invocation final
    {
        platform::child_process::LaunchRequest launch{};
        std::filesystem::path output{};
        std::string diagnostic{};
        bool ready{};
    };

    // Prepares one bounded local request; performs no writes or process launch.
    // The job owner retains scheduling, cancellation and output/image admission.
    [[nodiscard]] Invocation prepare(const Request& request,
        const Configuration& configuration,
        const std::filesystem::path& owned_work_directory);
    // Empty means valid. Reused before scheduling or hardware queries.
    [[nodiscard]] std::string request_diagnostic(const Request& request);
}
