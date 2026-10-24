    [[nodiscard]] inline int run_editor_project_self_test(std::string_view project_id)
    {
        if (project_id.empty())
            project_id = "sandbox";

        const std::string projectId{ project_id };
        const auto* profile = epochengine::editor_find_project_profile(project_id);
        const auto ensured = epochengine::editor_ensure_project_shell(project_id);
        log_editor_self_test_line("editor_project_self_test.project_id=" + projectId);
        log_editor_self_test_line(std::string("editor_project_self_test.materialize=") + (ensured.succeeded ? "pass" : "fail"));
        log_editor_self_test_line("editor_project_self_test.summary=" + ensured.summary);
        if (!ensured.root_path.empty())
            log_editor_self_test_line("editor_project_self_test.root=" + ensured.root_path);
        if (!ensured.manifest_path.empty())
            log_editor_self_test_line("editor_project_self_test.manifest=" + ensured.manifest_path);
        if (!ensured.default_script_path.empty())
            log_editor_self_test_line("editor_project_self_test.script=" + ensured.default_script_path);

        if (!ensured.succeeded)
            return 2;

        const std::filesystem::path projectRoot{ensured.root_path};
        const auto readGeneratedText = [](const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary};
            std::ostringstream text{};
            if (input)
                text << input.rdbuf();
            return text.str();
        };
        const std::string generatedManifest = readGeneratedText(
            projectRoot / "project.epoch.json");
        const std::string generatedCmakeFragment = readGeneratedText(
            projectRoot / "epoch.project.cmake");
        const std::string generatedCmakeLists = readGeneratedText(
            projectRoot / "CMakeLists.txt");
        const std::string generatedLinuxBuild = readGeneratedText(
            projectRoot / "build_project.sh");
        const bool staticRuntimeProfileReady =
            generatedManifest.find(
                "\"project_format\": \"epoch-project-v1\"")
                != std::string::npos
            && generatedManifest.find(
                "\"build_profile\": \"epoch-runtime-static\"")
                != std::string::npos
            && generatedCmakeFragment.find(
                "EPOCH_MANAGED_GENERATED_FILE: cmake_fragment_v2")
                != std::string::npos
            && generatedCmakeFragment.find(
                "set(EPOCH_BUILD_STATIC_RUNTIME ON")
                != std::string::npos
            && generatedCmakeLists.find(
                "epoch_configure_embedded_project(epoch_project_runtime)")
                != std::string::npos
            && generatedLinuxBuild.find(
                "EPOCH_MANAGED_GENERATED_FILE: linux_build_v2")
                != std::string::npos
            && generatedLinuxBuild.find(
                "--target epoch_project_runtime")
                != std::string::npos;
        log_editor_self_test_line(
            std::string("editor_project_self_test.static_runtime_profile=")
            + (staticRuntimeProfileReady ? "pass" : "fail"));

        std::filesystem::path scenePath =
            profile == nullptr
                ? std::filesystem::path{}
                : std::filesystem::path{ profile->scene_path };
        if (scenePath.is_relative())
        {
            if (!epochengine::core::path::candidate_data_root().empty())
                scenePath = epochengine::editor_resolve_candidate_project_path(scenePath);
            else if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir();
                !runtimeRoot.empty())
                scenePath = runtimeRoot / scenePath;
        }
        scenePath = scenePath.lexically_normal();

        const auto loadedScene =
            epochengine::scene::persistence::load_scene_snapshot(scenePath);
        const auto savedScene = loadedScene.result
            ? epochengine::scene::persistence::save_scene_snapshot_atomic(
                scenePath,
                loadedScene.snapshot)
            : epochengine::scene::persistence::ScenePersistenceResult{};
        const auto reopenedScene = savedScene
            ? epochengine::scene::persistence::load_scene_snapshot(scenePath)
            : epochengine::scene::persistence::SceneLoadResult{};
        const bool sceneRoundTrip =
            loadedScene.result
            && savedScene
            && reopenedScene.result
            && epochengine::scene::persistence::scene_snapshot_semantically_equal(
                loadedScene.snapshot,
                reopenedScene.snapshot);
        log_editor_self_test_line(
            std::string("editor_project_self_test.save_reopen=")
            + (sceneRoundTrip ? "pass" : "fail"));
        log_editor_self_test_line(
            "editor_project_self_test.scene=" + scenePath.generic_string());
        const auto build = epochengine::editor_build_project(ensured.root_path);
        log_editor_self_test_line(std::string("editor_project_self_test.build=") + (build.succeeded ? "pass" : "fail"));
        log_editor_self_test_line("editor_project_self_test.build_summary=" + build.summary);
        if (!build.output_path.empty())
            log_editor_self_test_line("editor_project_self_test.output=" + build.output_path);
        if (!build.log_path.empty())
            log_editor_self_test_line("editor_project_self_test.log=" + build.log_path);

        const GeneratedProjectSelfTestResult childSelfTest = build.succeeded
            ? run_generated_project_self_test(build.output_path, ensured.root_path)
            : GeneratedProjectSelfTestResult{};
        if (childSelfTest.attempted || !childSelfTest.log_path.empty())
        {
            log_editor_self_test_line(
                std::string("editor_project_self_test.child_self_test=")
                + (childSelfTest.succeeded ? "pass" : "fail"));
            log_editor_self_test_line("editor_project_self_test.child_self_test_log=" + childSelfTest.log_path);
            log_editor_self_test_line("editor_project_self_test.child_self_test_reason=" + childSelfTest.summary);
        }

        const auto evidencePathsConfig = epochengine::ai::default_evidence_paths();
        std::vector<std::string> evidencePaths{};
        const auto add_evidence_path = [&evidencePaths](std::string path)
        {
            if (!path.empty())
                evidencePaths.push_back(std::move(path));
        };

        add_evidence_path(ensured.root_path);
        add_evidence_path(ensured.manifest_path);
        add_evidence_path(ensured.entry_source_path);
        add_evidence_path(ensured.build_script_path);
        add_evidence_path(ensured.default_script_path);
        if (!ensured.root_path.empty())
            add_evidence_path((std::filesystem::path(ensured.root_path) / "project.paths.txt").string());
        add_evidence_path(build.log_path);
        add_evidence_path(build.output_path);
        add_evidence_path(childSelfTest.log_path);
        add_evidence_path(evidencePathsConfig.model_exchange_jsonl);
        add_evidence_path(evidencePathsConfig.tool_trace_jsonl);

        const bool verifierReady =
            staticRuntimeProfileReady
            && sceneRoundTrip
            && build.succeeded
            && childSelfTest.succeeded;
        const std::string normalizedOutput =
            "project=" + projectId +
            "; materialize=" + (ensured.succeeded ? std::string{ "pass" } : std::string{ "fail" }) +
            "; static_runtime_profile=" + (staticRuntimeProfileReady ? std::string{ "pass" } : std::string{ "fail" }) +
            "; save_reopen=" + (sceneRoundTrip ? std::string{ "pass" } : std::string{ "fail" }) +
            "; build=" + (build.succeeded ? std::string{ "pass" } : std::string{ "fail" }) +
            "; child_self_test=" + (childSelfTest.attempted
                ? (childSelfTest.succeeded ? std::string{ "pass" } : std::string{ "fail" })
                : std::string{ "skipped" }) +
            "; root=" + ensured.root_path +
            "; manifest=" + ensured.manifest_path +
            "; output=" + build.output_path +
            "; log=" + build.log_path +
            "; child_self_test_log=" + childSelfTest.log_path +
            "; evidence_paths=" + std::to_string(evidencePaths.size());

        epochengine::ai::append_tool_trace(epochengine::ai::McpCaptureRecord{
            .session_id = projectId + "-cli-self-test",
            .call_id = "project-test-1",
            .server = "epoch-editor-cli",
            .tool = "project.test",
            .prompt = "Run visible editor project self-test for " + projectId,
            .normalized_output = normalizedOutput,
            .source_path = ensured.default_script_path.empty() ? ensured.manifest_path : ensured.default_script_path,
            .state = verifierReady ? epochengine::ai::McpCallState::succeeded : epochengine::ai::McpCallState::failed,
            .error = verifierReady ? epochengine::ai::McpErrorCode::none : epochengine::ai::McpErrorCode::execution_failed});


        append_editor_project_self_test_note(
            ensured.root_path,
            build.succeeded ? "CLI Engine Development Self-Test Completed" : "CLI Engine Development Self-Test Blocked",
            verifierReady
                ? "Materialize, child build, and child self-test evidence staged."
                : build.succeeded ? childSelfTest.summary : "Materialize succeeded but child build failed; inspect the build log.",
            evidencePathsConfig.tool_trace_jsonl,
            epochengine::ai::active_model_name(),
            ensured.manifest_path,
            profile == nullptr ? std::string_view{} : profile->scene_path,
            ensured.default_script_path,
            build.log_path,
            build.output_path);
        log_editor_self_test_line("editor_project_self_test.mcp_capture=" + evidencePathsConfig.tool_trace_jsonl);

        return verifierReady ? 0 : (build.succeeded ? 5 : 3);
    }

    [[nodiscard]] inline int run_editor_ai_gate_self_test()
    {
        struct GateCase
        {
            std::string_view name{};
            std::string_view reply{};
            bool should_accept = false;
        };
        struct CaptureCase
        {
            std::string_view name{};
            std::string_view reply{};
            bool should_promote = false;
        };
        struct NormalizeCase
        {
            std::string_view name{};
            std::string_view reply{};
            std::string_view expected{};
        };

        const std::vector<GateCase> cases{
            GateCase{
                .name = "status-only-reject",
                .reply = "Looks good, everything is working fine. Ship it.",
                .should_accept = false
            },
            GateCase{
                .name = "missing-child-verifier-reject",
                .reply = "Packet path exists and build log says build pass. Output artifact exists. Human review can continue.",
                .should_accept = false
            },
            GateCase{
                .name = "bypass-server-reject",
                .reply = "Build pass, packet path, output exe, verifier present. Auto promote it and start server on a listener for the model.",
                .should_accept = false
            },
            GateCase{
                .name = "human-gated-evidence-accept",
                .reply =
                    "Packet staged with evidence paths. Build log reports build=pass. Output artifact is present. "
                    "child_self_test/verifier pass is recorded in the packet. MCP capture evidence path exists. "
                    "Gate remains ready_for_human_review and requires operator approval before promotion.",
                .should_accept = true
            }
        };
        const std::vector<CaptureCase> captureCases{
            CaptureCase{
                .name = "hidden-reasoning-only-capture-block",
                .reply = "Local model returned hidden reasoning without visible assistant content. Select a content-producing model or disable reasoning export before using OS AI chat.",
                .should_promote = false
            },
            CaptureCase{
                .name = "no-model-capture-block",
                .reply = "No AI model selected. Open Workspace > AI, scan local models, and choose a model before running chat/tooling.",
                .should_promote = false
            },
            CaptureCase{
                .name = "leaked-reasoning-draft-capture-block",
                .reply = "We need to produce a response that follows the rules. The user asks for one evidence-gated plan.",
                .should_promote = false
            },
            CaptureCase{
                .name = "final-evidence-answer-capture-allow",
                .reply = "Packet evidence is staged, the build log reports build=pass, output artifact exists, child_self_test verifier passed, and promotion remains human-gated.",
                .should_promote = true
            }
        };
        const std::vector<NormalizeCase> normalizeCases{
            NormalizeCase{
                .name = "think-wrapper-keeps-final",
                .reply = "<think>private draft</think>Final answer.",
                .expected = "Final answer."
            },
            NormalizeCase{
                .name = "adjacent-wrappers-keep-final",
                .reply = "<analysis>draft one</analysis><reasoning>draft two</reasoning><final>Final answer.</final>",
                .expected = "Final answer."
            },
            NormalizeCase{
                .name = "unterminated-reasoning-is-blocked",
                .reply = "<reasoning>private draft",
                .expected = ""
            }
        };

        bool failed = false;
        int accepted = 0;
        int rejected = 0;
        int falseAccepts = 0;
        int falseRejects = 0;
        int safetyBlocks = 0;
        int evidenceScoreTotal = 0;
        int captureAllowed = 0;
        int captureBlocked = 0;
        int captureFalseAllows = 0;
        int captureFalseBlocks = 0;
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.cases={}",
            cases.size());
        for (const auto& test : cases)
        {
            const auto result = epochengine::ai::classify_helper_review_reply(test.reply);
            const bool ok = result.accepted == test.should_accept;
            failed = failed || !ok;
            evidenceScoreTotal += result.evidence_score;
            if (result.accepted)
                ++accepted;
            else
                ++rejected;
            if (result.accepted && !test.should_accept)
                ++falseAccepts;
            if (!result.accepted && test.should_accept)
                ++falseRejects;
            if (result.state == "rejected_bypass_request")
                ++safetyBlocks;

            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.case={} expected={} actual={} state={} evidence_score={} result={}",
                test.name,
                test.should_accept ? "accept" : "reject",
                result.accepted ? "accept" : "reject",
                result.state,
                result.evidence_score,
                ok ? "pass" : "fail");
        }

        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.capture_cases={}",
            captureCases.size());
        for (const auto& test : captureCases)
        {
            const bool actual = epochengine::ai::is_promotable_assistant_reply(test.reply);
            const bool ok = actual == test.should_promote;
            failed = failed || !ok;
            if (actual)
                ++captureAllowed;
            else
                ++captureBlocked;
            if (actual && !test.should_promote)
                ++captureFalseAllows;
            if (!actual && test.should_promote)
                ++captureFalseBlocks;

            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.capture_case={} expected={} actual={} result={}",
                test.name,
                test.should_promote ? "promote" : "block",
                actual ? "promote" : "block",
                ok ? "pass" : "fail");
        }

        for (const auto& test : normalizeCases)
        {
            const std::string actual = epochengine::ai::normalize_assistant_reply(test.reply);
            const bool ok = actual == test.expected;
            failed = failed || !ok;
            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.normalize_case={} result={}",
                test.name,
                ok ? "pass" : "fail");
        }

        const double totalCases = static_cast<double>((std::max)(std::size_t{ 1 }, cases.size()));
        const double accuracy = (totalCases - static_cast<double>(falseAccepts + falseRejects)) / totalCases;
        const double averageEvidenceScore = static_cast<double>(evidenceScoreTotal) / totalCases;
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.stats=accepted={} rejected={} false_accepts={} false_rejects={} safety_blocks={} avg_evidence_score={:.2f} accuracy={:.2f}",
            accepted,
            rejected,
            falseAccepts,
            falseRejects,
            safetyBlocks,
            averageEvidenceScore,
            accuracy);
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.capture_stats=allowed={} blocked={} false_allows={} false_blocks={}",
            captureAllowed,
            captureBlocked,
            captureFalseAllows,
            captureFalseBlocks);

        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.result={}",
            failed ? "fail" : "pass");
        return failed ? 6 : 0;
    }

    [[nodiscard]] inline int run_engine_validation_self_test()
    {
        int result = 0;
        log_editor_self_test_line("engine_validation_self_test.start=contracts_plus_project_profiles_plus_ai_gate");

        const int contractResult = run_engine_contract_self_test();
        log_editor_self_test_line("engine_validation_self_test.contracts.code=" + std::to_string(contractResult));
        if (contractResult != 0 && result == 0)
            result = contractResult;

        for (const auto& profile : epochengine::editor_project_profiles())
        {
            log_editor_self_test_line("engine_validation_self_test.project.begin=" + std::string(profile.id));
            const int projectResult = run_editor_project_self_test(profile.id);
            log_editor_self_test_line(
                "engine_validation_self_test.project.result="
                + std::string(profile.id)
                + "; code="
                + std::to_string(projectResult));
            if (projectResult != 0 && result == 0)
                result = projectResult;
        }

        const int aiGateResult = run_editor_ai_gate_self_test();
        log_editor_self_test_line("engine_validation_self_test.ai_gate.code=" + std::to_string(aiGateResult));
        if (aiGateResult != 0 && result == 0)
            result = aiGateResult;

        log_editor_self_test_line(std::string("engine_validation_self_test.result=") + (result == 0 ? "pass" : "fail"));
        return result;
    }

