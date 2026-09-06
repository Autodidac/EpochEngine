/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <string>
#include <string_view>
#include <initializer_list>

module ai.development_proposal_codec;

namespace epochengine::ai::development_proposal_codec
{
    namespace
    {
        [[nodiscard]] std::string valid_packet()
        {
            return
                "EPOCH_SOURCE_PROPOSAL_V1\n"
                "title: Repair two bounded files\n"
                "rationale: Exercise ordered multi-file source ingestion\n"
                "lifetime_seconds: 900\n"
                "operation_count: 2\n"
                "begin_operation\n"
                "area: engine\n"
                "path: Engine/src/ai/example.cpp\n"
                "summary: Replace the engine source example\n"
                "final_newline: true\n"
                "begin_content\n"
                "|int answer = 42;\n"
                "||literal_bar\n"
                "end_content\n"
                "end_operation\n"
                "begin_operation\n"
                "area: project\n"
                "path: Projects/demo/main.cpp\n"
                "summary: Replace the project entry source\n"
                "final_newline: false\n"
                "begin_content\n"
                "|int main() { return 0; }\n"
                "end_content\n"
                "end_operation\n"
                "end_proposal\n";
        }

        [[nodiscard]] std::string valid_patch_packet()
        {
            return
                "EPOCH_SOURCE_PATCH_PROPOSAL_V1\n"
                "title: Repair one exact parser block\n"
                "rationale: Repair Parser bounds without regenerating its file\n"
                "lifetime_seconds: 900\n"
                "operation_count: 1\n"
                "begin_operation\n"
                "area: engine\n"
                "path: Engine/src/ai/parser.cpp\n"
                "summary: Repair Parser::validate bounds\n"
                "search_final_newline: true\n"
                "begin_search\n"
                "|int validate() { return 0; }\n"
                "end_search\n"
                "replacement_final_newline: true\n"
                "begin_replacement\n"
                "|int validate() { return 1; }\n"
                "end_replacement\n"
                "end_operation\n"
                "end_proposal\n";
        }

        [[nodiscard]] bool exact_patch_decode_contract()
        {
            const DecodeResult decoded = decode(valid_patch_packet());
            return decoded
                && decoded.proposal.changes.size() == 1u
                && decoded.proposal.changes[0u].edit_kind
                    == SourceEditKind::replace_exact_block
                && decoded.proposal.changes[0u].match_bytes
                    == "int validate() { return 0; }\n"
                && decoded.proposal.changes[0u].replacement_bytes
                    == "int validate() { return 1; }\n";
        }

        [[nodiscard]] bool ordered_decode_contract()
        {
            const DecodeResult decoded = decode(valid_packet());
            return decoded
                && decoded.proposal.title == "Repair two bounded files"
                && decoded.proposal.lifetime_seconds == 900u
                && decoded.proposal.changes.size() == 2u
                && decoded.proposal.changes[0u].area == SourceArea::engine
                && decoded.proposal.changes[0u].relative_path
                    == "Engine/src/ai/example.cpp"
                && decoded.proposal.changes[0u].replacement_bytes
                    == "int answer = 42;\n|literal_bar\n"
                && decoded.proposal.changes[1u].area == SourceArea::project
                && decoded.proposal.changes[1u].replacement_bytes
                    == "int main() { return 0; }";
        }

        [[nodiscard]] bool strict_envelope_contract()
        {
            if (decode("prose before\n" + valid_packet()).code
                    != DecodeCode::invalid_header
                || decode(valid_packet() + "extra\n").code
                    != DecodeCode::trailing_data)
            {
                return false;
            }
            DecodeLimits tiny{};
            tiny.maximum_reply_bytes = 256u;
            tiny.maximum_file_bytes = 128u;
            tiny.maximum_total_replacement_bytes = 256u;
            return tiny.valid()
                && decode(valid_packet(), tiny).code
                    == DecodeCode::size_limit_exceeded;
        }

        [[nodiscard]] bool path_and_operation_contract()
        {
            std::string traversal = valid_packet();
            traversal.replace(
                traversal.find("Engine/src/ai/example.cpp"),
                std::string{"Engine/src/ai/example.cpp"}.size(),
                "Engine/../outside.cpp");
            if (decode(traversal).code != DecodeCode::invalid_path)
                return false;

            std::string duplicate = valid_packet();
            duplicate.replace(
                duplicate.find("Projects/demo/main.cpp"),
                std::string{"Projects/demo/main.cpp"}.size(),
                "Engine/src/ai/example.cpp");
            duplicate.replace(
                duplicate.find("area: project"),
                std::string{"area: project"}.size(),
                "area: engine");
            if (decode(duplicate).code != DecodeCode::duplicate_path)
                return false;

            std::string malformed = valid_packet();
            malformed.replace(
                malformed.find("|int answer = 42;"),
                std::string{"|int answer = 42;"}.size(),
                "int answer = 42;");
            return decode(malformed).code == DecodeCode::malformed_content;
        }

        [[nodiscard]] std::string valid_context_request()
        {
            return
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1\n"
                "reason: Inspect the parser and its public contract\n"
                "path_count: 2\n"
                "path: Engine/src/ai/ai.development_proposal_codec.cpp\n"
                "path: Engine/modules/ai.development_proposal_codec.ixx\n"
                "end_request\n";
        }

        [[nodiscard]] bool strict_context_request_contract()
        {
            const auto decoded = decode_context_request(
                valid_context_request(), SourceArea::engine);
            if (!decoded
                || decoded.request.reason
                    != "Inspect the parser and its public contract"
                || decoded.request.paths.size() != 2u
                || decoded.request.paths[0u]
                    != "Engine/src/ai/ai.development_proposal_codec.cpp"
                || decoded.request.paths[1u]
                    != "Engine/modules/ai.development_proposal_codec.ixx"
                || !decoded.request.reads.empty())
            {
                return false;
            }

            std::string unknown = valid_context_request();
            unknown.insert(
                unknown.find("path_count:"), "unknown: rejected\n");
            if (decode_context_request(unknown, SourceArea::engine).code
                != DecodeCode::invalid_field)
            {
                return false;
            }

            std::string duplicateCount = valid_context_request();
            duplicateCount.insert(
                duplicateCount.find("path: "), "path_count: 2\n");
            if (decode_context_request(
                    duplicateCount, SourceArea::engine).code
                != DecodeCode::invalid_field)
            {
                return false;
            }

            std::string missingReason = valid_context_request();
            const std::string reasonLine =
                "reason: Inspect the parser and its public contract\n";
            missingReason.erase(
                missingReason.find(reasonLine), reasonLine.size());
            const auto inferredReason = decode_context_request(
                missingReason, SourceArea::engine);
            if (!inferredReason
                || inferredReason.request.reason
                    != "Model-selected verified source context.")
            {
                return false;
            }

            const auto compact = decode_context_request(
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1\n"
                "PATH Engine/src/ai/ai.development_proposal_codec.cpp\n"
                "Engine/modules/ai.development_proposal_codec.ixx\n",
                SourceArea::engine);
            if (!compact || compact.request.paths.size() != 2u
                || compact.request.paths[0u]
                    != "Engine/src/ai/ai.development_proposal_codec.cpp"
                || compact.request.paths[1u]
                    != "Engine/modules/ai.development_proposal_codec.ixx")
            {
                return false;
            }

            std::string duplicatePath = valid_context_request();
            duplicatePath.replace(
                duplicatePath.find(
                    "Engine/modules/ai.development_proposal_codec.ixx"),
                std::string{
                    "Engine/modules/ai.development_proposal_codec.ixx"}.size(),
                "Engine/src/ai/ai.development_proposal_codec.cpp");
            if (decode_context_request(
                    duplicatePath, SourceArea::engine).code
                != DecodeCode::duplicate_path)
            {
                return false;
            }

            std::string traversal = valid_context_request();
            traversal.replace(
                traversal.find(
                    "Engine/src/ai/ai.development_proposal_codec.cpp"),
                std::string{
                    "Engine/src/ai/ai.development_proposal_codec.cpp"}.size(),
                "Engine/../outside.cpp");
            if (decode_context_request(traversal, SourceArea::engine).code
                != DecodeCode::invalid_path)
            {
                return false;
            }

            if (decode_context_request(
                    valid_context_request(), SourceArea::project).code
                != DecodeCode::invalid_path)
            {
                return false;
            }

            return decode_context_request(
                valid_context_request() + "trailing\n",
                SourceArea::engine).code == DecodeCode::trailing_data;
        }
        [[nodiscard]] bool context_read_contract()
        {
            constexpr std::string_view firstPath{
                "Engine/src/ai/ai.development_proposal_codec.cpp"};
            constexpr std::string_view secondPath{
                "Engine/modules/ai.development_proposal_codec.ixx"};
            const std::string prefix =
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1\npath_count: 2\npath: "
                + std::string{firstPath} + "\n";
            const auto selected = decode_context_request(prefix
                + "first_line: 120\nquery:   literal \xc3\xa9 symbol  \npath: "
                + std::string{secondPath} + "\nquery: ContextRead\nend_request\n",
                SourceArea::engine);
            if (!selected || selected.request.reads.size() != 2u
                || selected.request.reads[0u].path != firstPath
                || selected.request.reads[0u].first_line != 120u
                || selected.request.reads[0u].query != "  literal \xc3\xa9 symbol  "
                || selected.request.reads[1u].path != secondPath
                || selected.request.reads[1u].first_line != 0u
                || selected.request.reads[1u].query != "ContextRead")
                return false;

            const std::string single = "EPOCH_SOURCE_CONTEXT_REQUEST_V1\npath: "
                + std::string{firstPath} + "\n";
            for (const auto line : {"0", "1", "1000000"})
            {
                const auto bounds = decode_context_request(single
                    + "first_line: " + line + "\nquery: \nend_request\n",
                    SourceArea::engine);
                if (!bounds || bounds.request.reads.size() != 1u
                    || !bounds.request.reads.front().query.empty())
                    return false;
            }
            constexpr std::string_view invalidFields[] = {
                "first_line: -1\n", "first_line: +1\n", "first_line: 1.5\n",
                "first_line: 1000001\n", "first_line: 999999999999999999999999\n",
                "first_line: \n", "first_line: 1\nfirst_line: 2\n",
                "query: first\nquery: second\n", "query: bad\rquery\n",
                "query: valid\nunknown: invalid\n"};
            for (const auto fields : invalidFields)
            {
                if (decode_context_request(single + std::string{fields}
                        + "end_request\n", SourceArea::engine))
                    return false;
            }
            if (decode_context_request(single + "query: " + std::string(257u, 'x')
                    + "\nend_request\n", SourceArea::engine)
                || decode_context_request(single + "query: "
                    + std::string{"bad\0query", 9u} + "\nend_request\n",
                    SourceArea::engine)
                || decode_context_request(
                    "EPOCH_SOURCE_CONTEXT_REQUEST_V1\nfirst_line: 1\npath: "
                    + std::string{firstPath} + "\n", SourceArea::engine)
                || decode_context_request(
                    "EPOCH_SOURCE_CONTEXT_REQUEST_V1\nquery: orphan\npath: "
                    + std::string{firstPath} + "\n", SourceArea::engine))
                return false;

            std::string complete = "EPOCH_SOURCE_CONTEXT_REQUEST_V1\n";
            for (std::size_t index = 0u; index < 12u; ++index)
            {
                complete += "path: Engine/src/ai/ai.read_" + std::to_string(index)
                    + ".cpp\nfirst_line: " + std::to_string(index + 1u)
                    + "\nquery: bounded\n";
            }
            const auto maximum = decode_context_request(complete, SourceArea::engine);
            return maximum && maximum.request.paths.size() == 12u
                && maximum.request.reads.size() == 12u
                && !decode_context_request(complete
                    + "path: Engine/src/ai/ai.read_12.cpp\nquery: extra\n",
                    SourceArea::engine);
        }

        [[nodiscard]] bool context_text_contract()
        {
            const std::string invalidUtf8{
                static_cast<char>(0xc3), static_cast<char>(0x28)};
            return valid_context_text("int answer = 42;\n")
                && !valid_context_text(std::string_view{"bad\0text", 8u})
                && !valid_context_text(invalidUtf8);
        }
        [[nodiscard]] bool protocol_prompt_contract()
        {
            constexpr std::string_view objective{
                "Repair the parser in Engine/src/ai/"
                "ai.development_proposal_codec.cpp."};
            constexpr std::string_view evidence{
                "PATH Engine/src/ai/ai.development_proposal_codec.cpp\n"
                "FILE_CONTENT_SIZE "
                "Engine/src/ai/ai.development_proposal_codec.cpp 13\n"
                "FILE_CONTENT_BEGIN "
                "Engine/src/ai/ai.development_proposal_codec.cpp\n"
                "known source\n"
                "FILE_CONTENT_END "
                "Engine/src/ai/ai.development_proposal_codec.cpp\n"};
            const std::string context = context_request_prompt(
                SourceArea::engine, "fix bugs", evidence);
            const std::string engine = protocol_prompt(
                SourceArea::engine, objective, evidence);
            const std::string project = protocol_prompt(
                SourceArea::project, "Repair Projects/demo/main.cpp.", {});
            const std::string grounded = protocol_prompt(
                SourceArea::engine,
                "replace \"known source\" with \"known reviewed source\" in "
                "Engine/src/ai/ai.development_proposal_codec.cpp",
                evidence);
            const std::string projectContext = context_request_prompt(
                SourceArea::project,
                "EPOCH_SOURCE_EDIT_REQUEST_V1\nInspect a project",
                "PATH Projects/demo/project.main.cpp\n");
            for (const auto* prompt : {&context, &projectContext, &engine, &project, &grounded})
            {
                if (prompt->find("If response_format supplies a JSON schema") == std::string::npos
                    || prompt->find("not a literal response prefix") == std::string::npos
                    || prompt->find("Do not prepend a header or a bare sentinel to JSON") == std::string::npos
                    || prompt->find("If no JSON schema is supplied") == std::string::npos
                    || prompt->find("The first response byte must be E") != std::string::npos
                    || prompt->find("The first byte must be E") != std::string::npos)
                    return false;
            }
            return context.starts_with("EPOCH_SOURCE_SELECTION_V1\n")
                && projectContext.starts_with("EPOCH_SOURCE_SELECTION_V1\n")
                && engine.starts_with("EPOCH_SOURCE_EDIT_REQUEST_V1\n")
                && project.starts_with("EPOCH_SOURCE_EDIT_REQUEST_V1\n")
                && grounded.starts_with("EPOCH_SOURCE_EDIT_REQUEST_V1\n")
                && context.find("nonempty reason, paths=[] and reads=[]") != std::string::npos
                && engine.find("action=insufficient requires a nonempty reason") != std::string::npos
                && engine.find("empty title, rationale, operations, paths and reads") != std::string::npos
                && engine.find("JSON action=context") != std::string::npos
                && engine.find("action=patch") != std::string::npos
                && context.find("EPOCH_SOURCE_CONTEXT_REQUEST_V1")
                    != std::string::npos
                && context.find("path: Engine/") != std::string::npos
                && context.find("fix bugs") != std::string::npos
                && context.find(evidence) != std::string::npos
                && context.find("up to twelve") != std::string::npos
                && context.find("first_line: N") != std::string::npos
                && context.find("query: text") != std::string::npos
                && engine.find("another region of the same file") != std::string::npos
                && context.find("does not add source-file bytes")
                    != std::string::npos
                && context.find("EPOCH_SOURCE_PROPOSAL_V1") == std::string::npos
                && engine.find("path: Engine/") != std::string::npos
                && engine.find("area: engine") != std::string::npos
                && engine.find(
                    "path: Engine/src/ai/ai.development_proposal_codec.cpp\n")
                    != std::string::npos
                && engine.find("Engine/path/to/file.cpp")
                    == std::string::npos
                && project.find("path: Projects/") != std::string::npos
                && project.find("area: project") != std::string::npos
                && project.find(
                    "path: Projects/NO_REVIEWED_PATH_RETURN_INSUFFICIENT\n")
                    != std::string::npos
                && engine.find(objective) != std::string::npos
                && engine.find(evidence) != std::string::npos
                && engine.find("EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1")
                    != std::string::npos
                && engine.find("EPOCH_SOURCE_PATCH_PROPOSAL_V1")
                    != std::string::npos
                && engine.find("FINAL OUTPUT CHECK")
                    != std::string::npos
                && engine.find("begin_search") != std::string::npos
                && engine.find("begin_replacement") != std::string::npos
                && engine.find(
                    "end_replacement\nend_operation\nend_proposal\n")
                    != std::string::npos
                && engine.find("end_replacement\nRepeat")
                    == std::string::npos
                && engine.find("Never regenerate the whole file") != std::string::npos
                && grounded.find(
                    "search_final_newline: false\nbegin_search\n"
                    "|known source\nend_search\n"
                    "replacement_final_newline: false\nbegin_replacement\n"
                    "|known reviewed source\nend_replacement\n"
                    "end_operation\nend_proposal\n")
                    != std::string::npos
                && engine.find("EPOCH_SOURCE_PROPOSAL_V1")
                    == std::string::npos
                && engine.find("EPOCH_SOURCE_CONTEXT_REQUEST_V1")
                    != std::string::npos
                && engine.find("FILE_ABSENT") != std::string::npos
                && engine.find("Never invent a path")
                    != std::string::npos
                && engine.find("objective-specific owner")
                    != std::string::npos
                && engine.find("exact existing symbol") != std::string::npos
                && engine.find("generic logger") != std::string::npos
                && engine.find("FILE_CONTENT_SIZE") != std::string::npos
                && engine.find("one ownership dot") != std::string::npos
                && engine.find("one to four related") != std::string::npos
                && engine.find("set operation_count to that exact integer")
                    != std::string::npos
                && engine.find("at most four operations")
                    != std::string::npos
                && engine.find("exactly one bounded source change")
                    == std::string::npos
                && engine.find("core.log") == std::string::npos;
        }

        [[nodiscard]] bool proposal_quality_contract()
        {
            Proposal valid{};
            valid.title = "Repair parser bounds";
            valid.rationale =
                "Preserve Parser ownership while repairing bounds validation";
            valid.changes.push_back(SourceChange{
                .area = SourceArea::engine,
                .relative_path = "Engine/src/ai/parser.cpp",
                .summary = "Repair Parser::validate bounds",
                .replacement_bytes =
                    "/************************************************\n"
                    " * SPDX-License-Identifier: LicenseRef-MIT-NoSell\n"
                    " ************************************************/\n"
                    "module ai.parser;\n"
                    "namespace epochengine::ai { int validate() { return 1; } }\n"});
            const std::string original =
                "/************************************************\n"
                " * SPDX-License-Identifier: LicenseRef-MIT-NoSell\n"
                " ************************************************/\n"
                "module ai.parser;\n"
                "namespace epochengine::ai { int validate() { return 0; } }\n";
            const std::string evidence =
                "FILE_CONTENT_SIZE Engine/src/ai/parser.cpp "
                + std::to_string(original.size()) + "\n"
                "FILE_CONTENT_BEGIN Engine/src/ai/parser.cpp\n"
                + original
                + "FILE_CONTENT_END Engine/src/ai/parser.cpp\n";
            if (!validate_quality(
                    valid, "Repair parser bounds validation", evidence))
            {
                return false;
            }
            for (const std::string_view objective : {
                    "find and fix a bug", "make it better", "improve",
                    "Resolve an incorrect condition", "修正してください"})
            {
                if (!validate_quality(valid, objective, evidence))
                    return false;
            }
            if (validate_quality(valid, {}, evidence))
                return false;

            Proposal noOp = valid;
            noOp.changes[0u].replacement_bytes = original;
            if (validate_quality(
                    noOp, "Repair parser bounds validation", evidence))
            {
                return false;
            }
            if (validate_quality(noOp, "find and fix a bug", evidence))
                return false;

            Proposal moduleInclude = valid;
            moduleInclude.changes[0u].replacement_bytes =
                "#include \"core.log.ixx\"\n";
            if (validate_quality(
                    moduleInclude, "Repair parser bounds validation", evidence))
            {
                return false;
            }

            Proposal unrelatedLogger = valid;
            unrelatedLogger.changes[0u].replacement_bytes =
                "class Logger { static Logger& GetInstance(); };\n";
            if (validate_quality(
                    unrelatedLogger,
                    "Repair parser bounds validation", evidence))
            {
                return false;
            }

            Proposal genericSummary = valid;
            genericSummary.changes[0u].summary =
                "Replace the file with a general implementation";
            if (validate_quality(
                    genericSummary,
                    "Repair parser bounds validation",
                    evidence))
            {
                return false;
            }

            Proposal exactPatch{};
            exactPatch.title = "Repair parser bounds";
            exactPatch.rationale =
                "Preserve Parser ownership while repairing bounds validation";
            exactPatch.changes.push_back(SourceChange{
                .area = SourceArea::engine,
                .relative_path = "Engine/src/ai/parser.cpp",
                .summary = "Repair Parser::validate bounds",
                .edit_kind = SourceEditKind::replace_exact_block,
                .match_bytes =
                    "namespace epochengine::ai { int validate() { return 0; } }\n",
                .replacement_bytes =
                    "namespace epochengine::ai { int validate() { return 1; } }\n"});
            const std::string excerpt =
                "namespace epochengine::ai { int validate() { return 0; } }\n";
            const std::string excerptEvidence =
                "FILE_SOURCE_SIZE Engine/src/ai/parser.cpp 4096\n"
                "FILE_EXCERPT_OFFSET Engine/src/ai/parser.cpp 1024\n"
                "FILE_EXCERPT_SIZE Engine/src/ai/parser.cpp "
                + std::to_string(excerpt.size()) + "\n"
                "FILE_EXCERPT_BEGIN Engine/src/ai/parser.cpp\n"
                + excerpt
                + "FILE_EXCERPT_END Engine/src/ai/parser.cpp\n";
            if (!validate_quality(
                    exactPatch,
                    "Repair parser bounds validation",
                    excerptEvidence))
            {
                return false;
            }

            Proposal excerptWholeFile = valid;
            return !validate_quality(
                excerptWholeFile,
                "Repair parser bounds validation",
                excerptEvidence);
        }

        [[nodiscard]] int contract_failure_code()
        {
            if (!ordered_decode_contract()) return 1;
            if (!exact_patch_decode_contract()) return 8;
            if (!strict_envelope_contract()) return 2;
            if (!path_and_operation_contract()) return 3;
            if (!strict_context_request_contract()) return 4;
            if (!context_read_contract()) return 9;
            if (!context_text_contract()) return 5;
            if (!protocol_prompt_contract()) return 6;
            if (!proposal_quality_contract()) return 7;
            return 0;
        }
    }

    bool run_contract()
    {
        return contract_failure_code() == 0;
    }
}

#if defined(EPOCH_AI_DEVELOPMENT_PROPOSAL_CODEC_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::development_proposal_codec::
        contract_failure_code();
}
#endif
