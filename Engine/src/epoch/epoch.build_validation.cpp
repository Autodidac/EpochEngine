/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

module epoch.build_validation;

namespace epochengine::build_validation
{
    namespace
    {
        [[nodiscard]] constexpr bool ascii_identifier_byte(char value) noexcept
        {
            return (value >= 'a' && value <= 'z')
                || (value >= 'A' && value <= 'Z')
                || (value >= '0' && value <= '9')
                || value == '-' || value == '_' || value == '.';
        }

        void append_json_string(std::string& output, std::string_view value)
        {
            static constexpr char hex[] = "0123456789abcdef";
            output.push_back('"');
            for (const unsigned char byte : value)
            {
                switch (byte)
                {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (byte < 0x20u)
                    {
                        output += "\\u00";
                        output.push_back(hex[(byte >> 4u) & 0x0fu]);
                        output.push_back(hex[byte & 0x0fu]);
                    }
                    else
                    {
                        output.push_back(static_cast<char>(byte));
                    }
                    break;
                }
            }
            output.push_back('"');
        }

        void append_u64(std::string& output, std::uint64_t value)
        {
            std::array<char, 32> buffer{};
            const auto conversion = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value);
            if (conversion.ec == std::errc{})
                output.append(buffer.data(), conversion.ptr);
            else
                output.push_back('0');
        }

        void append_version(std::string& output, SemanticVersion version)
        {
            output.push_back('"');
            append_u64(output, version.major);
            output.push_back('.');
            append_u64(output, version.minor);
            output.push_back('.');
            append_u64(output, version.revision);
            output.push_back('"');
        }

        [[nodiscard]] AdmissionResult refusal(
            AdmissionCode code,
            std::string_view diagnostic,
            CheckLane lane = CheckLane::invalid)
        {
            return {code, lane, std::string{diagnostic}};
        }

        [[nodiscard]] bool valid_platform_compiler(
            Platform platform,
            Compiler compiler) noexcept
        {
            switch (platform)
            {
            case Platform::windows_x64:
                return compiler == Compiler::msvc || compiler == Compiler::clang;
            case Platform::linux_x64:
                return compiler == Compiler::clang || compiler == Compiler::gcc;
            case Platform::macos_arm64:
                return compiler == Compiler::clang;
            case Platform::invalid:
                break;
            }
            return false;
        }

        [[nodiscard]] bool required_lane(
            CheckLane lane,
            Platform platform,
            const AdmissionPolicy& policy) noexcept
        {
            if (lane == CheckLane::source_names
                || lane == CheckLane::compile
                || lane == CheckLane::engine_contract)
            {
                return true;
            }
            if (lane == CheckLane::headless_ci)
                return policy.require_headless_ci;
            if (lane == CheckLane::package_inventory)
                return policy.require_package_inventory;
            if (lane == CheckLane::dependency_resolution)
                return policy.require_dependency_resolution;
            if (lane == CheckLane::shared_library_resolution)
                return platform != Platform::windows_x64;
            if (lane == CheckLane::renderer_smoke)
                return policy.require_renderer_smoke;
            return false;
        }
    }

    bool valid_lower_hex(
        std::string_view value,
        std::size_t exactBytes) noexcept
    {
        if (exactBytes == 0u || exactBytes > 128u
            || value.size() != exactBytes * 2u)
        {
            return false;
        }
        return std::all_of(value.begin(), value.end(), [](char byte)
        {
            return (byte >= '0' && byte <= '9')
                || (byte >= 'a' && byte <= 'f');
        });
    }

    bool valid_identifier(std::string_view value) noexcept
    {
        return !value.empty() && value.size() <= maximum_identifier_bytes
            && value.front() != '.' && value.back() != '.'
            && std::all_of(value.begin(), value.end(), ascii_identifier_byte);
    }

    const CheckEvidence* find_check(
        const ValidationReceipt& receipt,
        CheckLane lane) noexcept
    {
        const auto found = std::find_if(
            receipt.checks.begin(), receipt.checks.end(),
            [lane](const CheckEvidence& check)
            {
                return check.lane == lane;
            });
        return found == receipt.checks.end() ? nullptr : &*found;
    }

    AdmissionResult admit(
        const ValidationReceipt& receipt,
        SemanticVersion expectedSource,
        const AdmissionPolicy& policy) noexcept
    {
        try
        {
            if (receipt.schema != receipt_schema
                || !receipt.source_version.valid()
                || !receipt.packaged_version.valid()
                || !expectedSource.valid()
                || !valid_identifier(receipt.target)
                || !valid_identifier(receipt.toolchain)
                || receipt.checks.empty()
                || receipt.checks.size() > maximum_checks)
            {
                return refusal(
                    AdmissionCode::invalid_receipt,
                    "receipt schema, identity, or bounded collection is invalid");
            }
            if (receipt.source_version != expectedSource)
            {
                return refusal(
                    AdmissionCode::version_mismatch,
                    "receipt source version does not match requested publication");
            }
            if (!valid_lower_hex(receipt.source_commit, 20u)
                || !valid_lower_hex(receipt.source_tree_sha256, 32u))
            {
                return refusal(
                    AdmissionCode::invalid_source_commit,
                    "source commit or committed-tree digest is malformed");
            }
            if (!valid_platform_compiler(receipt.platform, receipt.compiler))
            {
                return refusal(
                    AdmissionCode::unsupported_platform,
                    "platform and compiler do not form a supported validation lane");
            }
            if (receipt.configuration == Configuration::invalid
                || (policy.require_release_configuration
                    && receipt.configuration != Configuration::release))
            {
                return refusal(
                    AdmissionCode::release_configuration_required,
                    "publication requires an exact Release configuration receipt");
            }
            if (!valid_identifier(receipt.artifact.name)
                || receipt.artifact.size_bytes == 0u
                || !valid_lower_hex(receipt.artifact.sha256, 32u))
            {
                return refusal(
                    AdmissionCode::invalid_artifact,
                    "artifact identity, size, or SHA-256 is invalid");
            }

            std::array<bool, static_cast<std::size_t>(CheckLane::count)> seen{};
            for (const CheckEvidence& check : receipt.checks)
            {
                const auto index = static_cast<std::size_t>(check.lane);
                if (check.lane <= CheckLane::invalid
                    || check.lane >= CheckLane::count
                    || check.status == CheckStatus::invalid
                    || check.diagnostic.size() > maximum_diagnostic_bytes
                    || (check.status == CheckStatus::passed
                        && !valid_lower_hex(check.evidence_sha256, 32u))
                    || (check.status != CheckStatus::passed
                        && !check.evidence_sha256.empty()
                        && !valid_lower_hex(check.evidence_sha256, 32u)))
                {
                    return refusal(
                        AdmissionCode::invalid_check,
                        "validation check fields are malformed",
                        check.lane);
                }
                if (seen[index])
                {
                    return refusal(
                        AdmissionCode::duplicate_check,
                        "validation lane appears more than once",
                        check.lane);
                }
                seen[index] = true;
                if (check.status == CheckStatus::failed)
                {
                    return refusal(
                        AdmissionCode::failed_check,
                        "a reported validation lane failed",
                        check.lane);
                }
            }

            for (std::uint8_t raw = static_cast<std::uint8_t>(CheckLane::source_names);
                raw < static_cast<std::uint8_t>(CheckLane::count); ++raw)
            {
                const auto lane = static_cast<CheckLane>(raw);
                if (!required_lane(lane, receipt.platform, policy))
                    continue;
                const CheckEvidence* check = find_check(receipt, lane);
                if (check == nullptr)
                {
                    return refusal(
                        AdmissionCode::missing_required_check,
                        "required validation lane is absent",
                        lane);
                }
                if (check->status != CheckStatus::passed)
                {
                    return refusal(
                        AdmissionCode::skipped_required_check,
                        "required validation lane was not proven",
                        lane);
                }
            }

            const CheckEvidence* renderer = find_check(
                receipt, CheckLane::renderer_smoke);
            if (renderer != nullptr && renderer->status == CheckStatus::skipped
                && !policy.permit_explicit_renderer_skip)
            {
                return refusal(
                    AdmissionCode::renderer_evidence_unclaimed,
                    "policy does not permit an explicitly skipped renderer smoke",
                    CheckLane::renderer_smoke);
            }

            return {
                AdmissionCode::admitted,
                CheckLane::invalid,
                "receipt satisfies local build-admission policy"};
        }
        catch (...)
        {
            return refusal(
                AdmissionCode::invalid_receipt,
                "receipt admission failed without mutating publication state");
        }
    }

    std::string canonical_json(const ValidationReceipt& receipt) noexcept
    {
        try
        {
            std::string output{};
            output.reserve(1024u + receipt.checks.size() * 256u);
            output += "{\"schema\":";
            append_json_string(output, receipt.schema);
            output += ",\"source_version\":";
            append_version(output, receipt.source_version);
            output += ",\"packaged_version\":";
            append_version(output, receipt.packaged_version);
            output += ",\"source_commit\":";
            append_json_string(output, receipt.source_commit);
            output += ",\"source_tree_sha256\":";
            append_json_string(output, receipt.source_tree_sha256);
            output += ",\"platform\":";
            append_json_string(output, platform_name(receipt.platform));
            output += ",\"compiler\":";
            append_json_string(output, compiler_name(receipt.compiler));
            output += ",\"configuration\":";
            append_json_string(output, configuration_name(receipt.configuration));
            output += ",\"target\":";
            append_json_string(output, receipt.target);
            output += ",\"toolchain\":";
            append_json_string(output, receipt.toolchain);
            output += ",\"artifact\":{\"name\":";
            append_json_string(output, receipt.artifact.name);
            output += ",\"sha256\":";
            append_json_string(output, receipt.artifact.sha256);
            output += ",\"size_bytes\":";
            append_u64(output, receipt.artifact.size_bytes);
            output += "},\"checks\":[";

            std::vector<CheckEvidence> ordered = receipt.checks;
            std::sort(ordered.begin(), ordered.end(),
                [](const CheckEvidence& left, const CheckEvidence& right)
                {
                    return left.lane < right.lane;
                });
            for (std::size_t index = 0u; index < ordered.size(); ++index)
            {
                if (index != 0u)
                    output.push_back(',');
                const CheckEvidence& check = ordered[index];
                output += "{\"lane\":";
                append_json_string(output, check_lane_name(check.lane));
                output += ",\"status\":";
                append_json_string(output, check_status_name(check.status));
                output += ",\"duration_ms\":";
                append_u64(output, check.duration_milliseconds);
                output += ",\"evidence_sha256\":";
                append_json_string(output, check.evidence_sha256);
                output += ",\"diagnostic\":";
                append_json_string(output, check.diagnostic);
                output.push_back('}');
            }
            output += "]}";
            return output;
        }
        catch (...)
        {
            return {};
        }
    }
}
