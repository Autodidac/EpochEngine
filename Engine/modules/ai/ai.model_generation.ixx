// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
export module ai.model_generation;
export import platform.child_process;

export namespace epochengine::ai::model_generation
{
    inline constexpr std::string_view model_package = "os_model_trellis_2_cpp_q4_512";
    struct Request final
    {
        std::filesystem::path reference_path{};
        std::string approved_reference_sha256{};
        std::uint32_t seed{42u};
        std::uint32_t resolution{512u};
        std::uint32_t steps{12u};
        std::uint32_t face_budget{60'000u};
        std::uint32_t atlas{1024u};
    };
    struct Configuration final
    {
        std::filesystem::path executable{};
        platform::child_process::ExecutableIdentity expected_executable{};
        std::filesystem::path models_cache_root{};
        // Captured by an explicit human license/access boundary. Never inferred
        // from model presence, an ungated mirror or general package consent.
        bool dinov3_terms_accepted{};
    };
    struct Invocation final
    {
        platform::child_process::LaunchRequest launch{};
        std::filesystem::path output{};
        std::string diagnostic{};
        bool ready{};
        // v0.8.1 has no exact triangle-budget flag. The job owner must enforce
        // this bound on independently parsed output before approval/import.
        std::uint32_t required_face_budget{};
        std::string reference_sha256{};
    };
    [[nodiscard]] std::string request_diagnostic(const Request& request);
    // Read-only preparation only: no process, downloads, state changes or new
    // scheduler. The job owner retains cancellation and actual output validation.
    [[nodiscard]] Invocation prepare(const Request& request,
        const Configuration& configuration,
        const std::filesystem::path& owned_work_directory);
}
