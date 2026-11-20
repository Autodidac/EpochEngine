/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
module;

#include <string>

module ai.project_profile;

namespace epochengine::ai::project_profile
{
    bool run_contract()
    {
        for (const Provider provider : {
                Provider::disabled,
                Provider::epoch_local_qwen38,
                Provider::external_mcp,
                Provider::engine_selected,
                Provider::calculator_coach})
        {
            const CodecResult encoded = serialize_profile(make_profile(provider));
            const CodecResult decoded = parse_profile(encoded.canonical_bytes);
            if (!encoded || !decoded || decoded.profile != make_profile(provider)
                || decoded.canonical_bytes != encoded.canonical_bytes
                || decoded.sha256 != encoded.sha256 || decoded.migrated_legacy)
                return false;
        }

        const std::string legacy =
            "{\n"
            "  \"schema\": \"epoch.project.ai.v1\",\n"
            "  \"provider\": \"disabled\",\n"
            "  \"inference_transport\": \"none\",\n"
            "  \"tool_protocol\": \"epoch_mcp_v1\",\n"
            "  \"self_iteration_scope\": \"project_source_only\",\n"
            "  \"engine_source_write\": false,\n"
            "  \"operator_approval_per_iteration\": true,\n"
            "  \"auto_start\": false,\n"
            "  \"server_or_listener\": false,\n"
            "  \"weights_bundled\": false\n"
            "}\n";
        const CodecResult migrated = parse_profile(legacy);
        if (!migrated || !migrated.migrated_legacy
            || migrated.profile != make_profile(Provider::disabled))
            return false;

        std::string engine_write = serialize_profile(
            make_profile(Provider::epoch_local_qwen38)).canonical_bytes;
        engine_write.replace(
            engine_write.find("\"engine_source_write\": false"),
            std::string{"\"engine_source_write\": false"}.size(),
            "\"engine_source_write\": true");
        std::string duplicate = serialize_profile(
            make_profile(Provider::disabled)).canonical_bytes;
        duplicate.insert(duplicate.find("\n}"),
            ",\n  \"provider\": \"disabled\"");
        std::string listener = serialize_profile(
            make_profile(Provider::external_mcp)).canonical_bytes;
        listener.replace(
            listener.find("\"server_or_listener\": false"),
            std::string{"\"server_or_listener\": false"}.size(),
            "\"server_or_listener\": true");
        std::string unknown = serialize_profile(
            make_profile(Provider::disabled)).canonical_bytes;
        unknown.insert(unknown.find("\n}"), ",\n  \"network\": true");
        std::string auto_start = serialize_profile(
            make_profile(Provider::epoch_local_qwen38)).canonical_bytes;
        auto_start.replace(
            auto_start.find("\"auto_start\": false"),
            std::string{"\"auto_start\": false"}.size(),
            "\"auto_start\": true");
        std::string no_approval = serialize_profile(
            make_profile(Provider::external_mcp)).canonical_bytes;
        no_approval.replace(
            no_approval.find("\"operator_approval_per_iteration\": true"),
            std::string{"\"operator_approval_per_iteration\": true"}.size(),
            "\"operator_approval_per_iteration\": false");
        std::string no_project_write = serialize_profile(
            make_profile(Provider::external_mcp)).canonical_bytes;
        no_project_write.replace(
            no_project_write.find("\"project_source_write\": true"),
            std::string{"\"project_source_write\": true"}.size(),
            "\"project_source_write\": false");
        std::string enabled_legacy = legacy;
        enabled_legacy.replace(
            enabled_legacy.find("\"provider\": \"disabled\""),
            std::string{"\"provider\": \"disabled\""}.size(),
            "\"provider\": \"external_mcp\"");

        const Profile coach = make_profile(Provider::calculator_coach);
        std::string coach_tools = serialize_profile(coach).canonical_bytes;
        coach_tools.replace(
            coach_tools.find("\"tool_protocol\": \"none\""),
            std::string{"\"tool_protocol\": \"none\""}.size(),
            "\"tool_protocol\": \"epoch_mcp_v1\"");
        std::string coach_write = serialize_profile(coach).canonical_bytes;
        coach_write.replace(
            coach_write.find("\"project_source_write\": false"),
            std::string{"\"project_source_write\": false"}.size(),
            "\"project_source_write\": true");

        return !parse_profile(engine_write)
            && !parse_profile(duplicate)
            && !parse_profile(listener)
            && !parse_profile(unknown)
            && !parse_profile(auto_start)
            && !parse_profile(no_approval)
            && !parse_profile(no_project_write)
            && !parse_profile(enabled_legacy)
            && coach.enabled && !coach.project_source_write && !coach.engine_source_write
            && !coach.auto_start && !coach.server_or_listener && !coach.weights_bundled
            && coach.tool_protocol == "none" && coach.endpoint_binding == "none"
            && coach.self_iteration == "disabled"
            && !parse_profile(coach_tools) && !parse_profile(coach_write)
            && !serialize_profile(make_profile(static_cast<Provider>(255)))
            && !serialize_profile(Profile{
                .provider = Provider::disabled,
                .enabled = true});
    }
}
