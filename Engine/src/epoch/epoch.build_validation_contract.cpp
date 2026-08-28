/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

module epoch.build_validation;

namespace epochengine::build_validation
{
    namespace
    {
        constexpr std::string_view digest{
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
        constexpr std::string_view commit{
            "0123456789abcdef0123456789abcdef01234567"};

        [[nodiscard]] CheckEvidence passed(CheckLane lane)
        {
            return {
                .lane = lane,
                .status = CheckStatus::passed,
                .duration_milliseconds = 1u,
                .evidence_sha256 = std::string{digest},
                .diagnostic = "proven"};
        }

        [[nodiscard]] ValidationReceipt windows_receipt()
        {
            ValidationReceipt receipt{};
            receipt.source_version = {0u, 89u, 32u};
            receipt.packaged_version = {0u, 89u, 30u};
            receipt.source_commit = commit;
            receipt.source_tree_sha256 = digest;
            receipt.platform = Platform::windows_x64;
            receipt.compiler = Compiler::msvc;
            receipt.configuration = Configuration::release;
            receipt.target = "EpochEditor";
            receipt.toolchain = "msvc-19.44-vcpkg-static";
            receipt.artifact = {
                "epoch_win10_x64_v0.89.32.zip", std::string{digest}, 4096u};
            receipt.checks = {
                passed(CheckLane::source_names),
                passed(CheckLane::compile),
                passed(CheckLane::engine_contract),
                passed(CheckLane::headless_ci),
                passed(CheckLane::package_inventory),
                passed(CheckLane::dependency_resolution),
                CheckEvidence{
                    .lane = CheckLane::renderer_smoke,
                    .status = CheckStatus::skipped,
                    .duration_milliseconds = 0u,
                    .evidence_sha256 = {},
                    .diagnostic = "operator eye evidence not claimed"}};
            return receipt;
        }
    }

    ContractFailure run_contract() noexcept
    {
        try
        {
            const SemanticVersion expected{0u, 89u, 32u};
            const ValidationReceipt valid = windows_receipt();
            if (!admit(valid, expected))
                return ContractFailure::valid_windows;

            ValidationReceipt linux = valid;
            linux.platform = Platform::linux_x64;
            linux.compiler = Compiler::clang;
            linux.toolchain = "clang-22-vcpkg-static";
            linux.artifact.name = "epoch_linux_x64_v0.89.32.tar.gz";
            linux.checks.push_back(passed(CheckLane::shared_library_resolution));
            if (!admit(linux, expected))
                return ContractFailure::valid_linux_without_pixels;

            auto candidate = valid;
            candidate.source_version.revision = 30u;
            if (admit(candidate, expected).code != AdmissionCode::version_mismatch)
                return ContractFailure::version_mismatch;

            candidate = valid;
            candidate.source_commit.front() = 'G';
            if (admit(candidate, expected).code
                != AdmissionCode::invalid_source_commit)
            {
                return ContractFailure::invalid_commit;
            }

            candidate = valid;
            candidate.artifact.sha256.pop_back();
            if (admit(candidate, expected).code != AdmissionCode::invalid_artifact)
                return ContractFailure::invalid_digest;

            candidate = valid;
            candidate.checks.push_back(passed(CheckLane::compile));
            if (admit(candidate, expected).code != AdmissionCode::duplicate_check)
                return ContractFailure::duplicate_lane;

            candidate = valid;
            auto* compile = const_cast<CheckEvidence*>(
                find_check(candidate, CheckLane::compile));
            compile->status = CheckStatus::failed;
            compile->evidence_sha256.clear();
            if (admit(candidate, expected).code != AdmissionCode::failed_check)
                return ContractFailure::failed_lane;

            candidate = valid;
            auto* contract = const_cast<CheckEvidence*>(
                find_check(candidate, CheckLane::engine_contract));
            contract->status = CheckStatus::skipped;
            contract->evidence_sha256.clear();
            if (admit(candidate, expected).code
                != AdmissionCode::skipped_required_check)
            {
                return ContractFailure::skipped_required;
            }

            candidate = valid;
            std::erase_if(candidate.checks, [](const CheckEvidence& check)
            {
                return check.lane == CheckLane::headless_ci;
            });
            if (admit(candidate, expected).code
                != AdmissionCode::missing_required_check)
            {
                return ContractFailure::missing_lane;
            }

            candidate = valid;
            candidate.configuration = Configuration::debug;
            if (admit(candidate, expected).code
                != AdmissionCode::release_configuration_required)
            {
                return ContractFailure::debug_release;
            }

            AdmissionPolicy strictPixels{};
            strictPixels.permit_explicit_renderer_skip = false;
            if (admit(valid, expected, strictPixels).code
                != AdmissionCode::renderer_evidence_unclaimed)
            {
                return ContractFailure::renderer_claim;
            }

            candidate = valid;
            std::reverse(candidate.checks.begin(), candidate.checks.end());
            if (canonical_json(candidate) != canonical_json(valid))
                return ContractFailure::canonical_order;

            candidate = valid;
            candidate.checks.front().diagnostic = "line\n\"quoted\"\\path";
            const std::string escaped = canonical_json(candidate);
            if (escaped.find("line\\n\\\"quoted\\\"\\\\path")
                == std::string::npos)
            {
                return ContractFailure::canonical_escape;
            }

            candidate = valid;
            candidate.checks.resize(maximum_checks + 1u);
            if (admit(candidate, expected).code != AdmissionCode::invalid_receipt
                || valid_lower_hex("ABCDEF", 3u)
                || valid_identifier(".hidden")
                || valid_identifier(std::string(maximum_identifier_bytes + 1u, 'a')))
            {
                return ContractFailure::bounds;
            }
            return ContractFailure::none;
        }
        catch (...)
        {
            return ContractFailure::bounds;
        }
    }
}

#if defined(EPOCH_BUILD_VALIDATION_CONTRACT_MAIN)
int main()
{
    return epochengine::build_validation::run_contract()
            == epochengine::build_validation::ContractFailure::none
        ? 0 : 1;
}
#endif
