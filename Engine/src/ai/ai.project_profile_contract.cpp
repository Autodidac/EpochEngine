/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
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
                Provider::external_mcp})
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

        return !parse_profile(engine_write)
            && !parse_profile(duplicate)
            && !parse_profile(listener)
            && !parse_profile(unknown)
            && !serialize_profile(Profile{
                .provider = Provider::disabled,
                .enabled = true});
    }
}
