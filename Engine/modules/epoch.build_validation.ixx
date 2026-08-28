/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module epoch.build_validation;

export namespace epochengine::build_validation
{
    inline constexpr std::string_view receipt_schema{
        "epoch.build-validation/v1"};
    inline constexpr std::uint32_t maximum_checks{64u};
    inline constexpr std::uint32_t maximum_identifier_bytes{96u};
    inline constexpr std::uint32_t maximum_diagnostic_bytes{2048u};

    enum class Platform : std::uint8_t
    {
        invalid,
        windows_x64,
        linux_x64,
        macos_arm64
    };

    enum class Compiler : std::uint8_t
    {
        invalid,
        msvc,
        clang,
        gcc
    };

    enum class Configuration : std::uint8_t
    {
        invalid,
        debug,
        release
    };

    enum class CheckLane : std::uint8_t
    {
        invalid,
        source_names,
        compile,
        engine_contract,
        headless_ci,
        package_inventory,
        dependency_resolution,
        shared_library_resolution,
        renderer_smoke,
        count
    };

    enum class CheckStatus : std::uint8_t
    {
        invalid,
        passed,
        failed,
        skipped
    };

    enum class AdmissionCode : std::uint8_t
    {
        admitted,
        invalid_receipt,
        version_mismatch,
        invalid_source_commit,
        invalid_artifact,
        invalid_check,
        duplicate_check,
        failed_check,
        missing_required_check,
        skipped_required_check,
        release_configuration_required,
        renderer_evidence_unclaimed,
        unsupported_platform
    };

    struct SemanticVersion final
    {
        std::uint16_t major{};
        std::uint16_t minor{};
        std::uint16_t revision{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return major <= 999u && minor <= 999u && revision <= 9999u;
        }

        friend constexpr auto operator<=>(
            const SemanticVersion&,
            const SemanticVersion&) noexcept = default;
    };

    struct ArtifactEvidence final
    {
        std::string name{};
        std::string sha256{};
        std::uint64_t size_bytes{};

        friend bool operator==(
            const ArtifactEvidence&,
            const ArtifactEvidence&) noexcept = default;
    };

    struct CheckEvidence final
    {
        CheckLane lane{CheckLane::invalid};
        CheckStatus status{CheckStatus::invalid};
        std::uint64_t duration_milliseconds{};
        std::string evidence_sha256{};
        std::string diagnostic{};

        friend bool operator==(
            const CheckEvidence&,
            const CheckEvidence&) noexcept = default;
    };

    struct ValidationReceipt final
    {
        std::string schema{receipt_schema};
        SemanticVersion source_version{};
        SemanticVersion packaged_version{};
        std::string source_commit{};
        std::string source_tree_sha256{};
        Platform platform{Platform::invalid};
        Compiler compiler{Compiler::invalid};
        Configuration configuration{Configuration::invalid};
        std::string target{};
        std::string toolchain{};
        ArtifactEvidence artifact{};
        std::vector<CheckEvidence> checks{};

        friend bool operator==(
            const ValidationReceipt&,
            const ValidationReceipt&) noexcept = default;
    };

    struct AdmissionPolicy final
    {
        bool require_release_configuration{true};
        bool require_headless_ci{true};
        bool require_package_inventory{true};
        bool require_dependency_resolution{true};
        bool require_renderer_smoke{false};
        bool permit_explicit_renderer_skip{true};

        friend constexpr auto operator<=>(
            const AdmissionPolicy&,
            const AdmissionPolicy&) noexcept = default;
    };

    struct AdmissionResult final
    {
        AdmissionCode code{AdmissionCode::invalid_receipt};
        CheckLane lane{CheckLane::invalid};
        std::string diagnostic{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == AdmissionCode::admitted;
        }
    };

    [[nodiscard]] constexpr std::string_view platform_name(
        Platform value) noexcept
    {
        switch (value)
        {
        case Platform::windows_x64: return "windows-x64";
        case Platform::linux_x64: return "linux-x64";
        case Platform::macos_arm64: return "macos-arm64";
        case Platform::invalid: break;
        }
        return "invalid";
    }

    [[nodiscard]] constexpr std::string_view compiler_name(
        Compiler value) noexcept
    {
        switch (value)
        {
        case Compiler::msvc: return "msvc";
        case Compiler::clang: return "clang";
        case Compiler::gcc: return "gcc";
        case Compiler::invalid: break;
        }
        return "invalid";
    }

    [[nodiscard]] constexpr std::string_view configuration_name(
        Configuration value) noexcept
    {
        switch (value)
        {
        case Configuration::debug: return "debug";
        case Configuration::release: return "release";
        case Configuration::invalid: break;
        }
        return "invalid";
    }

    [[nodiscard]] constexpr std::string_view check_lane_name(
        CheckLane lane) noexcept
    {
        switch (lane)
        {
        case CheckLane::source_names: return "source_names";
        case CheckLane::compile: return "compile";
        case CheckLane::engine_contract: return "engine_contract";
        case CheckLane::headless_ci: return "headless_ci";
        case CheckLane::package_inventory: return "package_inventory";
        case CheckLane::dependency_resolution: return "dependency_resolution";
        case CheckLane::shared_library_resolution:
            return "shared_library_resolution";
        case CheckLane::renderer_smoke: return "renderer_smoke";
        case CheckLane::invalid:
        case CheckLane::count: break;
        }
        return "invalid";
    }

    [[nodiscard]] constexpr std::string_view check_status_name(
        CheckStatus status) noexcept
    {
        switch (status)
        {
        case CheckStatus::passed: return "passed";
        case CheckStatus::failed: return "failed";
        case CheckStatus::skipped: return "skipped";
        case CheckStatus::invalid: break;
        }
        return "invalid";
    }

    [[nodiscard]] constexpr std::string_view admission_code_name(
        AdmissionCode code) noexcept
    {
        switch (code)
        {
        case AdmissionCode::admitted: return "admitted";
        case AdmissionCode::invalid_receipt: return "invalid_receipt";
        case AdmissionCode::version_mismatch: return "version_mismatch";
        case AdmissionCode::invalid_source_commit:
            return "invalid_source_commit";
        case AdmissionCode::invalid_artifact: return "invalid_artifact";
        case AdmissionCode::invalid_check: return "invalid_check";
        case AdmissionCode::duplicate_check: return "duplicate_check";
        case AdmissionCode::failed_check: return "failed_check";
        case AdmissionCode::missing_required_check:
            return "missing_required_check";
        case AdmissionCode::skipped_required_check:
            return "skipped_required_check";
        case AdmissionCode::release_configuration_required:
            return "release_configuration_required";
        case AdmissionCode::renderer_evidence_unclaimed:
            return "renderer_evidence_unclaimed";
        case AdmissionCode::unsupported_platform:
            return "unsupported_platform";
        }
        return "invalid_receipt";
    }

    [[nodiscard]] bool valid_lower_hex(
        std::string_view value,
        std::size_t exactBytes) noexcept;
    [[nodiscard]] bool valid_identifier(std::string_view value) noexcept;
    [[nodiscard]] const CheckEvidence* find_check(
        const ValidationReceipt& receipt,
        CheckLane lane) noexcept;
    [[nodiscard]] AdmissionResult admit(
        const ValidationReceipt& receipt,
        SemanticVersion expectedSource,
        const AdmissionPolicy& policy = {}) noexcept;
    [[nodiscard]] std::string canonical_json(
        const ValidationReceipt& receipt) noexcept;

    enum class ContractFailure : std::uint8_t
    {
        none,
        valid_windows,
        valid_linux_without_pixels,
        version_mismatch,
        invalid_commit,
        invalid_digest,
        duplicate_lane,
        failed_lane,
        skipped_required,
        missing_lane,
        debug_release,
        renderer_claim,
        canonical_order,
        canonical_escape,
        bounds
    };

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
