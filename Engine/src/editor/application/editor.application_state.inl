// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
            None = 0,
            File,
            Edit,
            Asset,
            Window,
            Tools,
            Tasks,
            Help
        };

        enum class EditorAutomationCommand : unsigned char
        {
            None = 0,
            SmartUpdate,
            SourceUpdate,
            AssetsInteractionProof,
            CandidateLabSmokeChoose,
            CandidateLabSmokeKeep,
            SelfCodingLocalSmoke
        };

        enum class EditorUpdateState : unsigned char
        {
            Idle = 0,
            Checking,
            Available,
            RestartReady,
            SourceWorkerRunning,
            Canceled,
            Failed
        };

        enum class AiWorkspaceDomain : unsigned char
        {
            Control = 0,
            Tooling,
            Engine,
            Software,
            Evidence,
            Visualizer,
            Ops
        };

        enum class AiDeferredRequestKind : unsigned char
        {
            None = 0,
            Chat,
            Tooling,
            Authoring,
        };

        enum class AiChatIntent : unsigned char
        {
            ProjectChat = 0,
            ProjectAuthoring,
            ProjectTooling,
            EngineSource,
            Ambiguous
        };

        enum class EditorLayoutDrag : unsigned char
        {
            None = 0,
            Outliner,
            Inspector,
            Dock,
            BottomColumns
        };

        enum class EditorToolDockRegion : std::uint8_t
        {
            Left = 0,
            Right,
            BottomLeft,
            BottomRight
        };

        enum class OutlinerToolTab : unsigned char
        {
            World = 0,
            Assets,
            Gui,
            Scripting,
            TileMap
        };

        enum class InspectorToolTab : unsigned char
        {
            Properties = 0,
            WorldSettings,
            AiControls,
            AiDevelopment
        };

        enum class EditorToolPane : std::uint8_t
        {
            WorldOutliner = 0,
            AssetBrowser,
            GuiHierarchy,
            ScriptBrowser,
            TileMap,
            Properties,
            WorldSettings,
            AiControls,
            Output,
            AiChat,
            AiDevelopment,
            Count
        };

        constexpr std::size_t kEditorToolPaneCount =
            static_cast<std::size_t>(EditorToolPane::Count);

        [[nodiscard]] constexpr std::size_t tool_pane_index(
            EditorToolPane pane) noexcept
        {
            return static_cast<std::size_t>(pane);
        }

        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>
            editor_tool_pane_routes() noexcept
        {
            return {
                "pane.world_outliner",
                "pane.asset_browser",
                "pane.gui_hierarchy",
                "pane.script_browser",
                "pane.tile_map",
                "pane.properties",
                "pane.world_settings",
                "pane.ai_controls",
                "pane.output",
                "pane.ai_chat",
                "pane.ai_development"
            };
        }

        [[nodiscard]] constexpr std::array<std::string_view, kEditorToolPaneCount>
            editor_tool_pane_labels() noexcept
        {
            return {
                "World Outliner",
                "Asset Browser",
                "GUI Hierarchy",
                "Script Browser",
                "Tile Map",
                "Properties",
                "Global Settings",
                "AI Settings",
                "Output",
                "Assistant",
                "Assistant"
            };
        }

        [[nodiscard]] constexpr std::array<EditorToolDockRegion, kEditorToolPaneCount>
            default_tool_pane_regions() noexcept
        {
            return {
                EditorToolDockRegion::Left,
                EditorToolDockRegion::Left,
                EditorToolDockRegion::Left,
                EditorToolDockRegion::Left,
                EditorToolDockRegion::Left,
                EditorToolDockRegion::Right,
                EditorToolDockRegion::Right,
                EditorToolDockRegion::Left, // AI Settings belongs to the left dock.
                EditorToolDockRegion::BottomLeft,
                EditorToolDockRegion::Right, // AI Chat remains on the right.
                EditorToolDockRegion::BottomRight
            };
        }

        [[nodiscard]] constexpr std::array<bool, kEditorToolPaneCount>
            default_tool_pane_open_state() noexcept
        {
            std::array<bool, kEditorToolPaneCount> result{};
            result.fill(true);
            return result;
        }

        [[nodiscard]] constexpr std::array<std::uint32_t, kEditorToolPaneCount>
            default_tool_pane_tab_order() noexcept
        {
            std::array<std::uint32_t, kEditorToolPaneCount> result{};
            for (std::uint32_t index = 0u; index < result.size(); ++index)
                result[index] = index;
            return result;
        }

        [[nodiscard]] constexpr std::optional<EditorToolPane>
            editor_tool_pane_from_route(std::string_view route) noexcept
        {
            constexpr auto routes = editor_tool_pane_routes();
            for (std::size_t index = 0; index < routes.size(); ++index)
            {
                if (routes[index] == route)
                    return static_cast<EditorToolPane>(index);
            }

            // Accept the previous coarse identities during snapshot and host
            // migration, then immediately restore the corresponding real tab.
            if (route == "pane.ai_raw")
                return EditorToolPane::AiChat;
            if (route == "pane.outliner")
                return EditorToolPane::WorldOutliner;
            if (route == "pane.inspector")
                return EditorToolPane::Properties;
            if (route == "pane.console"
                || route == "pane.project_status"
                || route == "pane.asset_status"
                || route == "pane.ai_status"
                || route == "pane.systems_status")
            {
                return EditorToolPane::Output;
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr bool is_outliner_tool_pane(
            EditorToolPane pane) noexcept
        {
            return pane >= EditorToolPane::WorldOutliner
                && pane <= EditorToolPane::TileMap;
        }

        [[nodiscard]] constexpr bool is_inspector_tool_pane(
            EditorToolPane pane) noexcept
        {
            return pane == EditorToolPane::Properties
                || pane == EditorToolPane::WorldSettings
                || pane == EditorToolPane::AiControls;
        }

        [[nodiscard]] constexpr bool is_console_tool_pane(
            EditorToolPane pane) noexcept
        {
            return pane == EditorToolPane::Output;
        }

        using EditorMainSurface = EditorApplicationSurface;
        constexpr std::size_t kEditorMainSurfaceCount =
            static_cast<std::size_t>(EditorMainSurface::Count);

        [[nodiscard]] constexpr std::size_t editor_main_surface_index(
            EditorMainSurface surface) noexcept
        {
            const std::size_t index = static_cast<std::size_t>(surface);
            return index < kEditorMainSurfaceCount ? index : 0u;
        }

        template <typename Value>
        [[nodiscard]] constexpr std::array<Value, kEditorMainSurfaceCount>
            filled_workspace_values(Value value) noexcept
        {
            std::array<Value, kEditorMainSurfaceCount> values{};
            values.fill(value);
            return values;
        }

        struct SystemsSurfaceState
        {
            std::unique_ptr<editor_systems_panel::Panel> panel{};
            float renderZoom{ 1.15f };
            float taskZoom{ 1.15f };
            int renderPan{};
            int taskPan{};
            int graphInputCooldownFrames{};
            SpriteHandle renderSurface{};
            SpriteHandle taskSurface{};
            SpriteHandle supportSurface{};
            SpriteHandle aiLoopSurface{};
        };

        [[nodiscard]] static bool is_ws_only(std::string_view s) noexcept
        {
            for (unsigned char c : s)
                if (c > ' ') return false;
            return true;
        }

        [[nodiscard]] static std::string normalize_editor_text_for_gui(std::string_view text)
        {
            return editor_gui_text::sanitize(text);
        }

        std::recursive_mutex aiModelSelectionMutex{};
        std::size_t aiModelSelectionUsers{};

        // A queued request owns its model selection until its worker retires.
        // The same mutex covers selection actions and lease acquisition; no
        // context reads another context's mutable chat/editor state.
        struct AiModelSelectionLease final
        {
            AiModelSelectionLease()
            {
                std::scoped_lock lock(aiModelSelectionMutex);
                ++aiModelSelectionUsers;
            }
            ~AiModelSelectionLease()
            {
                std::scoped_lock lock(aiModelSelectionMutex);
                --aiModelSelectionUsers;
            }
            AiModelSelectionLease(const AiModelSelectionLease&) = delete;
            AiModelSelectionLease& operator=(const AiModelSelectionLease&) = delete;
        };

        struct AiChatRequestState
        {
            std::shared_ptr<AiModelSelectionLease> selectionLease{
                std::make_shared<AiModelSelectionLease>()};
            std::string reply{};
            std::string error{};
            ai::ModelTerminalFailure terminalFailure{ai::ModelTerminalFailure::none};
            std::stop_source cancellation{};
            std::atomic<ai::ModelRequestStage> stage{ai::ModelRequestStage::queued};
            mutable std::mutex progressMutex{};
            ai::ModelRequestProgress progress{};
            std::chrono::steady_clock::time_point lastResponseAt{};
            std::atomic_bool ready{false};

            void observe_progress(const ai::ModelRequestProgress& update)
            {
                std::scoped_lock lock(progressMutex);
                if (update.attempt != progress.attempt)
                    lastResponseAt = {};
                if (update.response_bytes > progress.response_bytes
                    || (update.attempt != progress.attempt && update.response_bytes))
                    lastResponseAt = std::chrono::steady_clock::now();
                progress = update;
            }
        };

        [[nodiscard]] bool ai_reply_is_internal_protocol(std::string_view reply) noexcept
        {
            while (!reply.empty()
                && (reply.front() == ' ' || reply.front() == '\t'
                    || reply.front() == '\r' || reply.front() == '\n'))
            {
                reply.remove_prefix(1u);
            }

            constexpr std::string_view headers[]{
                "EPOCH_AUTHORING_PLAN_V1",
                "EPOCH_AUTHORING_QUESTION_V1",
                "EPOCH_PROJECT_SCRIPT_V1",
                "EPOCH_TOOL_PLAN_V1",
                "EPOCH_TOOL_QUESTION_V1",
            };
            for (const std::string_view header : headers)
            {
                if (reply.starts_with(header))
                    return true;
            }

            // Some local runtimes/debug adapters have prepended a token-progress
            // sentence before the actual structured packet. Treat a line-framed
            // protocol header near the front as internal too, rather than ever
            // painting the plan/reasoning into the user-facing chat transcript.
            constexpr std::size_t kProtocolProbeBytes = 512u;
            const std::string_view probe = reply.substr(
                0u, (std::min)(reply.size(), kProtocolProbeBytes));
            for (const std::string_view header : headers)
            {
                const std::size_t pos = probe.find(header);
                if (pos != std::string_view::npos
                    && (pos == 0u || probe[pos - 1u] == '\n'
                        || probe[pos - 1u] == ' '))
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string recover_prefixed_ai_protocol(
            std::string_view reply,
            std::string_view firstHeader,
            std::string_view secondHeader = {})
        {
            constexpr std::size_t kProtocolProbeBytes = 512u;
            const std::string_view probe = reply.substr(
                0u, (std::min)(reply.size(), kProtocolProbeBytes));
            const auto find_header = [probe](std::string_view header) noexcept
            {
                if (header.empty())
                    return std::string_view::npos;
                return probe.find(header);
            };
            const std::size_t first = find_header(firstHeader);
            const std::size_t second = find_header(secondHeader);
            std::size_t offset = first;
            if (offset == std::string_view::npos
                || (second != std::string_view::npos && second < offset))
            {
                offset = second;
            }
            if (offset == std::string_view::npos)
                return std::string{reply};
            return std::string{reply.substr(offset)};
        }

        [[nodiscard]] std::optional<bool> ai_chat_confirmation(std::string_view input)
        {
            while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1u);
            while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1u);
            std::string lower{input};
            for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            constexpr std::array approveTerms{
                std::string_view{"yes"}, std::string_view{"y"}, std::string_view{"ok"},
                std::string_view{"okay"}, std::string_view{"approve"}, std::string_view{"apply"},
                std::string_view{"do it"}, std::string_view{"go ahead"}, std::string_view{"proceed"},
                std::string_view{"continue"}, std::string_view{"use it"}, std::string_view{"run it"}};
            constexpr std::array rejectTerms{
                std::string_view{"no"}, std::string_view{"n"}, std::string_view{"nope"},
                std::string_view{"reject"}, std::string_view{"discard"}, std::string_view{"cancel"},
                std::string_view{"never mind"}, std::string_view{"nevermind"}};
            if (std::ranges::find(approveTerms, lower) != approveTerms.end()) return true;
            if (std::ranges::find(rejectTerms, lower) != rejectTerms.end()) return false;
            return std::nullopt;
        }

        struct AiChat
        {
            static constexpr std::size_t kMaxLines = 500;
            static constexpr std::size_t kMaxRawRecords = 256;
            static constexpr std::size_t kMaxRawBytesPerRecord = 128u * 1024u;
            std::vector<std::string> lines{};
            // Parallel to visible lines; only operator requests are retryable.
            // Protocol packets, approvals and status messages carry no action.
            std::vector<std::optional<std::string>> retryInputs{};
            std::vector<gui::ChatMessageRunInfo> lineRunDetails{};
            gui::ChatMessageRunInfo pendingRunInfo{};
            std::vector<ai::ReferenceImage> draftReferenceImages{}, queuedReferenceImages{}, activeReferenceImages{};
            std::vector<gui::ChatFileAttachment> draftReferenceFiles{}, queuedReferenceFiles{}, activeReferenceFiles{};
            std::string queuedDraftText{}, activeDraftText{};
            std::string activeReferencePrompt{};
            std::unordered_map<std::string, gui::ChatImageAttachment> referencePreviews{};
            std::uint64_t nextImageRequestId{}, queuedImageRequestId{}, activeImageRequestId{};
            std::string queuedInput{};
            std::chrono::steady_clock::time_point queuedInputAt{};
            unsigned queuedInputFrames{};
            std::string queuedBlockReported{};
            std::uint64_t queuedCompletionGeneration{};
            bool queuedInputRetryable{};
            bool suppressUserEcho{};
            AiDeferredRequestKind statusRequestKind{AiDeferredRequestKind::Chat};
            std::string input{};
            std::string pendingPrompt{};
            std::string latestRawReply{};
            std::vector<std::string> rawOutputHistory{};
            std::uint64_t nativeRawSequence{};
            bool nativeStreamCapture{};
            bool showActivities{};
            std::uint64_t nativeActivitySequence{};
            std::uint64_t inlineActivityRunSequence{};
            static constexpr std::size_t kNoActivityLine = (std::numeric_limits<std::size_t>::max)();
            std::size_t inlineHostActivityLine{kNoActivityLine};
            std::uint64_t inlineHostActivityId{};
            // One host disclosure per submitted user turn; setup/restoration
            // chatter stays in the dedicated Activity history, not the chat.
            bool hostActivityRequested{};
            bool sourceOutcomeEmitted{};
            bool sourceTaskOpen{}; // Only a real source task may emit a terminal verdict.
            std::size_t lastHostActivityLine{kNoActivityLine};
            std::vector<std::string> activityHistory{};
            std::string lastWorkObservation{};
            std::uint64_t externalReplySequence{}, externalReplyId{};
            std::string externalReplyText{};
            bool externalReplyOpen{};

            void begin_external_reply()
            {
                externalReplyId = (std::uint64_t{1} << 58u) | ++externalReplySequence;
                externalReplyText.clear();
                externalReplyOpen = true;
            }

            void update_external_reply(std::string_view delta, std::string_view completed,
                gui::ChatMessageRunInfo info)
            {
                if (externalReplyId == 0u) return;
                if (!completed.empty()) externalReplyText.assign(completed);
                else if (!delta.empty()) externalReplyText.append(delta.substr(0u,
                    (std::min)(delta.size(), 32768u - (std::min)(externalReplyText.size(), std::size_t{32768u}))));
                if (externalReplyText.empty()) return;
                info.run_id = externalReplyId;
                for (std::size_t index = 0u; index < lineRunDetails.size(); ++index)
                    if (lineRunDetails[index].run_id == externalReplyId)
                    {
                        lines[index] = "Assistant: " + externalReplyText;
                        lineRunDetails[index] = std::move(info);
                        return;
                    }
                append_line("Assistant: " + externalReplyText, std::nullopt, std::move(info));
            }

            void finish_source_task(std::string_view reason,
                bool hostTerminalFailure = false)
            {
                if (sourceOutcomeEmitted || !sourceTaskOpen)
                    return;
                // No activity record is required to show the operator a real
                // terminal host verdict; it may be missing after a rejected
                // source request. Never cross into a newer user turn.
                const std::size_t activityBoundary = lastHostActivityLine < lines.size()
                    ? lastHostActivityLine : lines.size();
                for (std::size_t index = activityBoundary;
                    index < lines.size(); ++index)
                    if (lines[index].starts_with("User: ")) return;
                const std::string status = normalize_editor_text_for_gui(reason);
                if (status.empty()) return;
                // Do not infer terminal success/failure by scanning message
                // text. A successful repair may quote "failed" compiler output;
                // the panel's typed terminal verdict is the authority.
                const bool failed = hostTerminalFailure;
                const bool reviewReady = status.find("review") != std::string::npos
                    || status.find("Review") != std::string::npos
                    || status.find("approved") != std::string::npos;
                const std::string summary = (failed
                    ? "Self-iteration stopped before approval. "
                    : reviewReady ? "Candidate awaits human review. "
                    : "Self-iteration ended without a verified result. ")
                    + status.substr(0u, (std::min)(status.size(), std::size_t{384u}))
                    + " Live source remains unchanged.";
                // The host already owns its own nested events. The terminal
                // assistant reply is separate and must not be copied again into
                // the Worked disclosure, where it otherwise appears twice.
                end_host_activity(failed);
                hostActivityRequested = false;
                append_line("Assistant: " + summary);
                sourceOutcomeEmitted = true;
                sourceTaskOpen = false;
            }

            void end_host_activity(bool failed = false, bool stopped = false) noexcept
            {
                if (inlineHostActivityLine < lineRunDetails.size()
                    && lineRunDetails[inlineHostActivityLine].run_id == inlineHostActivityId)
                {
                    auto& info = lineRunDetails[inlineHostActivityLine];
                    info.stream_live = false;
                    info.status = stopped ? "Stopped" : failed ? "Failed" : "Completed activity";
                    for (auto& item : info.activity_events)
                        if (item.group == "Model" && item.status == "Streaming")
                            item.status = (failed || stopped) ? "Interrupted" : "Captured";
                    lastHostActivityLine = inlineHostActivityLine;
                }
                inlineHostActivityLine = kNoActivityLine;
                inlineHostActivityId = 0u;
                hostActivityRequested = false;
            }

            // Each activity record is inserted with its user turn. No late
            // rotation of the transcript or parallel arrays is permitted.

            void complete_activity_metadata(const gui::ChatMessageRunInfo& runInfo)
            {
                if (!runInfo.present()) return;
                const std::size_t index = inlineHostActivityLine < lineRunDetails.size()
                    ? inlineHostActivityLine : lastHostActivityLine;
                if (index >= lineRunDetails.size() || index >= lines.size()
                    || !lineRunDetails[index].stream_block
                    || lineRunDetails[index].route != "Activity") return;
                // Never attach a delayed model completion to a newer user turn.
                for (std::size_t i = index + 1u; i < lines.size(); ++i)
                    if (lines[i].starts_with("User: ")) return;
                auto& activity = lineRunDetails[index];
                activity.model = runInfo.model;
                activity.duration = runInfo.duration;
                activity.tps = runInfo.tps;
                activity.stream_live = false;
                activity.status = runInfo.status;
                // Closing a request also closes each raw-stream disclosure.
                // Preserve truncated/capture-limit evidence as a distinct
                // status; never leave old token sections marked Streaming.
                for (auto& event : activity.activity_events)
                {
                    const bool rawStream = event.group == "Model"
                        && (event.summary == "Reasoning tokens"
                            || event.summary == "Tool arguments"
                            || event.summary == "Response tokens");
                    if (!rawStream || event.status != "Streaming") continue;
                    event.status = runInfo.status == "Cancelled" ? "Stopped"
                        : runInfo.status == "Failed" ? "Failed" : "Complete";
                }
                std::string telemetry = "Model: "
                    + (runInfo.model.empty() ? std::string{"Not reported"} : runInfo.model)
                    + "\nElapsed: " + (runInfo.duration.empty()
                        ? std::string{"Not reported"} : runInfo.duration)
                    + "\nGeneration rate: " + (runInfo.tps.empty()
                        ? std::string{"not reported"} : runInfo.tps);
                if (!runInfo.evidence.empty())
                    telemetry += "\n" + runInfo.evidence;
                if (!activity.evidence.empty()) activity.evidence += "\n";
                // Usage belongs to a separate nested event, not appended as
                // undifferentiated prose to the visible host message stream.
                if (activity.activity_events.size() < 512u)
                    activity.activity_events.push_back(gui::ChatActivityEvent{
                        .group = "Model",
                        .summary = "Inference completed",
                        .detail = std::string{"Model: "}
                            + (runInfo.model.empty() ? "Not reported" : runInfo.model)
                            + "\nElapsed: "
                            + (runInfo.duration.empty() ? "Not reported" : runInfo.duration)
                            + "\nGeneration rate: "
                            + (runInfo.tps.empty() ? "Not reported" : runInfo.tps),
                        .status = runInfo.status});
                activity.evidence += std::move(telemetry);
            }

            [[nodiscard]] gui::ChatMessageRunInfo* ensure_host_activity()
            {
                if (!hostActivityRequested) return nullptr;
                if (inlineHostActivityLine >= lineRunDetails.size()
                    || inlineHostActivityId == 0u
                    || lineRunDetails[inlineHostActivityLine].run_id != inlineHostActivityId)
                {
                    gui::ChatMessageRunInfo info{};
                    info.stream_block = true;
                    info.stream_live = true;
                    info.route = "Activity";
                    info.run_id = (std::uint64_t{1} << 61u)
                        | ++inlineActivityRunSequence;
                    inlineHostActivityId = info.run_id;
                    append_line("Agent: Activity", std::nullopt, std::move(info));
                    inlineHostActivityLine = lineRunDetails.size() - 1u;
                    lastHostActivityLine = inlineHostActivityLine;
                }
                return &lineRunDetails[inlineHostActivityLine];
            }

            void append_host_activity(std::string_view group, std::string_view event)
            {
                // Do not manufacture visible rows for startup, model restore,
                // unrelated background telemetry, or a completed request.
                if (!hostActivityRequested) return;
                if (group == "Assistant") group = "Assistant";
                std::string detail = normalize_editor_text_for_gui(event);
                for (std::size_t at{}; (at = detail.find("Assistant", at)) != std::string::npos;)
                { detail.replace(at, 13u, "Assistant"); at += 9u; }
                if (detail.empty()) return;
                // Host producers sometimes include their own group prefix.
                // Canonicalize at the boundary so "Assistant: Assistant:"
                // cannot appear in the chat or exported run evidence.
                const std::string prefix = std::string{group} + ": ";
                while (detail.starts_with(prefix)) detail.erase(0u, prefix.size());
                if (detail.empty()) return;
                auto* entry = ensure_host_activity();
                if (!entry) return;
                auto& info = *entry;
                const std::string record = std::string{group} + ": " + detail;
                // Distinct host events remain inspectable but cannot multiply
                // chat rows. Bound the evidence just like native token streams.
                if (!info.evidence.empty()
                    && info.evidence.size() >= record.size()
                    && info.evidence.compare(info.evidence.size() - record.size(),
                        record.size(), record) == 0)
                    return;
                if (info.evidence.size() + record.size() + 1u > kMaxRawBytesPerRecord)
                {
                    info.evidence.erase(0u, (std::min)(info.evidence.size(),
                        kMaxRawBytesPerRecord / 4u));
                    info.evidence.insert(0u, "[Earlier host events are retained in the activity log]\n");
                }
                if (!info.evidence.empty()) info.evidence += '\n';
                info.evidence += record;
                // Keep the disclosure label brief like EngCoder; the complete
                // observed host message remains inspectable one level below.
                std::string summary = detail;
                if (detail.starts_with("Request submitted"))
                    summary = "Request submitted";
                else if (detail.starts_with("Intent routed"))
                    summary = "Assistant preparing engine work";
                else if (detail.starts_with("Preparing a buildable"))
                    summary = "Preparing candidate workspace";
                else if (detail.find("Digest-bound proposal request admitted")
                    != std::string::npos)
                    summary = "Model request admitted";
                else if (detail.find("source request is queued")
                    != std::string::npos)
                    summary = "Queued for model";
                else if (detail.find("Host started the Assistant request")
                    != std::string::npos)
                    summary = "Waiting for model endpoint";
                else if (detail.find("Epoch host model request;") != std::string::npos)
                    summary = "Model request dispatched";
                else if (detail.find("host completion:") != std::string::npos)
                    summary = "Model transport completed";
                else if (detail.find("Model response completed; Epoch validates") != std::string::npos)
                    summary = "Validating response";
                else if (detail.find("Model response received")
                    != std::string::npos)
                    summary = "Model response received";
                else if (detail.starts_with("Exact campaign proposal staged")
                    || detail.starts_with("Model-generated changes staged"))
                    summary = "Changes applied in candidate";
                else if (group == "Workflow")
                    summary = detail.substr(0u, detail.find('\n'));
                else if (detail.size() > 80u)
                    summary = detail.substr(0u, 80u) + "...";
                // Full evidence above is retained for diagnostics. Ordinary users
                // only need meaningful workflow transitions and model output;
                // queue/transport chatter is one expandable technical trace.
                const bool technical = editor::ai_activity_presentation::technical_setup_event(
                    group, detail);
                if (technical)
                {
                    // Raw host evidence above remains available locally; status updates below still apply.
                }
                else
                {
                    const bool repeatedMilestone = !info.activity_events.empty()
                        && !info.activity_events.back().tool
                        && info.activity_events.back().summary == summary;
                    if (repeatedMilestone)
                    {
                        auto& existing = info.activity_events.back();
                        if (existing.detail != detail
                            && existing.detail.find(detail) == std::string::npos
                            && existing.detail.size() + detail.size() + 1u <= 4096u)
                            existing.detail += "\n" + detail;
                    }
                    else if (info.activity_events.size() < 512u)
                        info.activity_events.push_back(gui::ChatActivityEvent{
                            .group = std::string{group}, .summary = std::move(summary),
                            .detail = detail, .status = "Observed"});
                }
                // This is the most recent *observed host milestone*, not a
                // model-authored progress claim. Never show "Coding" merely
                // because a model request was queued or a worker was created.
                if (group.compare("System") == 0
                    && detail.starts_with("Request submitted"))
                    info.status = "Submitted";
                else if (group.compare("System 1") == 0
                    && detail.starts_with("Intent routed"))
                    info.status = "Assistant preparing engine work";
                else if (group.compare("Workflow") == 0)
                {
                    // Stage names originate in the panel's host-owned
                    // snapshot, not in untrusted model output.
                    const auto end = detail.find('\n');
                    info.status = detail.substr(0u, end);
                }
                else if (group.compare("Assistant") == 0)
                {
                    if (detail.starts_with("Preparing a buildable"))
                        info.status = "Preparing candidate";
                    else if (detail.find("Digest-bound proposal request admitted")
                        != std::string::npos)
                        info.status = "Preparing model request";
                    else if (detail.find("source request is queued")
                        != std::string::npos)
                        info.status = "Waiting for model";
                    else if (detail.find("Host started the Assistant request")
                        != std::string::npos)
                        info.status = "Waiting for model response";
                    else if (detail.find("Model response received")
                        != std::string::npos)
                        info.status = "Reviewing model response";
                    else if (detail.starts_with("Coding the candidate"))
                        info.status = "Coding candidate";
                    else if (detail.starts_with("Reading the required declaration"))
                        info.status = "Reading source";
                    else if (detail.find("compiler") != std::string::npos
                        && detail.find("started") != std::string::npos)
                        info.status = "Build started";
                }
            }

            // Tool calls are first-class chronological entries, not prose inside
            // the controller's combined Activity disclosure. Each can be
            // inspected independently without exposing model reasoning/tokens.
            void append_tool_activity(std::string_view group, std::string_view event)
            {
                if (!hostActivityRequested) return;
                // Native host emits id/name/status/reason as four separate
                // fields. Treat arbitrary model text only as a bounded label.
                // The same call_id updates one card rather than showing
                // review, approval and execution as unrelated tool calls.
                const std::string_view separator{"\n"};
                const auto first = event.find(separator);
                const auto second = first == std::string_view::npos
                    ? first : event.find(separator, first + 1u);
                const auto third = second == std::string_view::npos
                    ? second : event.find(separator, second + 1u);
                const bool structured = group == "Tool call"
                    && first != std::string_view::npos
                    && second != std::string_view::npos
                    && third != std::string_view::npos;
                if (!structured)
                {
                    append_host_activity(group, event);
                    return;
                }
                const std::string id = structured
                    ? std::string{event.substr(0u, first)} : std::string{};
                const std::string name = structured
                    ? normalize_editor_text_for_gui(event.substr(first + 1u,
                        second - first - 1u)) : std::string{group};
                const std::string status = structured
                    ? normalize_editor_text_for_gui(event.substr(second + 1u,
                        third - second - 1u)) : "Reviewed";
                const std::string reason = structured
                    ? normalize_editor_text_for_gui(event.substr(third + 1u))
                    : normalize_editor_text_for_gui(event);
                if (name.empty() || reason.size() > kMaxRawBytesPerRecord) return;
                const auto is_blocked = status == "denied" || status == "blocked"
                    || status == "failed" || status == "rejected";
                auto* info = ensure_host_activity();
                if (!info) return;
                if (structured && !id.empty())
                {
                    // Update the same nested tool entry as review progresses.
                    // The stable native id also protects against cross-tool
                    // merging when multiple calls share the same name.
                    for (auto& item : info->activity_events)
                    {
                        if (!item.tool || item.id != id) continue;
                        item.status = is_blocked ? "Blocked" : status;
                        item.detail += "\n" + status + ": " + reason;
                        if (item.detail.size() > 4096u)
                            item.detail.erase(0u, item.detail.size() - 4096u);
                        return;
                    }
                }
                if (info->activity_events.size() >= 512u) return;
                gui::ChatActivityEvent item{};
                item.group = "Tool call";
                item.summary = editor::ai_activity_presentation::tool_action_title(name);
                item.id = id;
                item.status = is_blocked ? "Blocked" : status;
                item.detail = "Host checked " + name + "\n" + status + ": " + reason;
                item.tool = true;
                info->activity_events.push_back(std::move(item));
            }

            void remember_activity(std::string group,std::string text)
            {
                const auto entry="Agent: ["+group+"] "+normalize_editor_text_for_gui(text);
                if(!activityHistory.empty()&&activityHistory.back()==entry)return;
                activityHistory.push_back(entry);
                if(activityHistory.size()>kMaxLines)activityHistory.erase(activityHistory.begin());
                if (group == "Tools" || group == "Tool call")
                    append_tool_activity(group, text);
                else if (group == "System 1" || group == "Runtime"
                    || group == "Model")
                    append_host_activity(group, text);
            }
            ai::native_runtime::OutputKind nativeRawKind{ai::native_runtime::OutputKind::boundary};
            std::uint64_t inlineNativeRunId{};
            std::size_t inlineNativeLine{(std::numeric_limits<std::size_t>::max)()};

            void end_inline_native_block() noexcept
            {
                if (inlineNativeLine < lineRunDetails.size()
                    && lineRunDetails[inlineNativeLine].run_id == inlineNativeRunId)
                    lineRunDetails[inlineNativeLine].stream_live = false;
                inlineNativeLine = (std::numeric_limits<std::size_t>::max)();
                inlineNativeRunId = 0u;
            }

            void remember_native_output(const ai::native_runtime::EventBlock& event)
            {
                nativeRawSequence=(std::max)(nativeRawSequence,event.sequence);
                if(event.kind==ai::native_runtime::OutputKind::boundary) {
                    nativeRawKind=event.kind;
                    return;
                }
                if(event.text.empty()) return;
                if(rawOutputHistory.empty() || nativeRawKind!=event.kind
                    || rawOutputHistory.back().size()+event.text.size()>kMaxRawBytesPerRecord)
                    rawOutputHistory.emplace_back("Agent: ");
                rawOutputHistory.back()+=normalize_editor_text_for_gui(event.text);
                // One activity disclosure per user turn, including streamed
                // reasoning, tool arguments and response events. Never create
                // separate conversation rows for each native stream kind.
                if (hostActivityRequested || lastHostActivityLine < lineRunDetails.size())
                {
                    if (hostActivityRequested && (inlineHostActivityLine >= lineRunDetails.size()
                        || inlineHostActivityId == 0u))
                        append_host_activity("Model", "Streaming response");
                    const std::size_t ownerLine = hostActivityRequested
                        ? inlineHostActivityLine : lastHostActivityLine;
                    bool newerUserTurn = false;
                    for (std::size_t line = ownerLine + 1u; line < lines.size(); ++line)
                        if (lines[line].starts_with("User: "))
                        { newerUserTurn = true; break; }
                    if (!newerUserTurn && ownerLine < lineRunDetails.size()
                        && lineRunDetails[ownerLine].route == "Activity"
                        && (!hostActivityRequested ||
                            lineRunDetails[ownerLine].run_id == inlineHostActivityId))
                    {
                        auto& activity = lineRunDetails[ownerLine];
                        // Only a real native output chunk proves generation
                        // has begun. Queue/admission messages are not tokens.
                        if (hostActivityRequested)
                        {
                            activity.status = "Generating";
                            activity.model = !pendingRunInfo.model.empty()
                                ? pendingRunInfo.model : ai::native_runtime::snapshot(false).model;
                        }
                        const char* label = event.kind == ai::native_runtime::OutputKind::reasoning
                            ? "Reasoning tokens" : event.kind == ai::native_runtime::OutputKind::tool_arguments
                            ? "Tool arguments" : "Response tokens";
                        const std::string chunk = normalize_editor_text_for_gui(event.text);
                        // The Activity tree is the raw-stream owner. Each kind
                        // gets one independently expandable, retained entry
                        // under the CURRENT user turn, never a second console.
                        if ((nativeRawKind != event.kind || activity.activity_events.empty()
                            || activity.activity_events.back().group != "Model"
                            || activity.activity_events.back().summary != label)
                            && activity.activity_events.size() < 512u)
                        {
                            activity.activity_events.push_back(gui::ChatActivityEvent{
                                .group = "Model", .summary = label,
                                .detail = {}, .status = activity.stream_live
                                    ? "Streaming" : "Complete"});
                        }
                        if (!activity.activity_events.empty())
                        {
                            auto& stream = activity.activity_events.back();
                            if (stream.group == "Model" && stream.summary == label)
                            {
                                if (stream.detail.size() < kMaxRawBytesPerRecord)
                                    stream.detail.append(chunk, 0u, (std::min)(
                                        chunk.size(), kMaxRawBytesPerRecord - stream.detail.size()));
                                if (stream.detail.size() == kMaxRawBytesPerRecord)
                                    stream.status = "Capture limit reached";
                            }
                        }
                        // Keep the full, bounded transcript export compatible
                        // with existing histories and diagnostics.
                        if (nativeRawKind != event.kind)
                        {
                            if (!activity.evidence.empty()) activity.evidence += '\n';
                            activity.evidence += label;
                            activity.evidence += ":\n";
                        }
                        if (activity.evidence.size() < kMaxRawBytesPerRecord)
                            activity.evidence.append(chunk, 0u, (std::min)(
                                chunk.size(), kMaxRawBytesPerRecord - activity.evidence.size()));
                    }
                }
                nativeRawKind=event.kind;
                if(rawOutputHistory.size()>kMaxRawRecords)
                    rawOutputHistory.erase(rawOutputHistory.begin());
            }

            void remember_raw_output(std::string_view)
            {
                // Deliberately no raw provider response in the user-facing chat.
            }
            ai::ModelTerminalFailure latestTerminalFailure{ai::ModelTerminalFailure::none};
            std::string latestTerminalStatus{};
            ai::InferenceWorkload latestCompletionWorkload{ai::InferenceWorkload::chat};
            AiDeferredRequestKind latestCompletionRequestKind{AiDeferredRequestKind::None};
            AiDeferredRequestKind pendingRequestKind{AiDeferredRequestKind::None};
            std::uint64_t completionGeneration{};
            std::shared_ptr<AiChatRequestState> pending{};
            std::chrono::steady_clock::time_point requestStartedAt{};
            epochengine::ai::InferenceWorkload pendingWorkload{
                epochengine::ai::InferenceWorkload::chat};
            std::jthread worker{};

            AiChat() = default;

            AiChat(const AiChat&) = delete;
            AiChat& operator=(const AiChat&) = delete;
            AiChat(AiChat&&) noexcept = default;
            AiChat& operator=(AiChat&&) noexcept = default;
            bool requestRetirementFailed{};

            void clear_completion_payload() noexcept
            {
                latestRawReply.clear();
                latestTerminalFailure = ai::ModelTerminalFailure::none;
                latestTerminalStatus.clear();
                latestCompletionRequestKind = AiDeferredRequestKind::None;
            }

            void pump()
            {
                if (pending && activeImageRequestId)
                {
                    const auto stage = pending->stage.load(std::memory_order_acquire);
                    if (stage == ai::ModelRequestStage::sending) image_status("Waiting for vision response");
                    else if (stage == ai::ModelRequestStage::request_sent
                        || stage == ai::ModelRequestStage::awaiting_response
                        || stage == ai::ModelRequestStage::receiving) image_status("Image sent");
                }
                // Capture host-reviewed tool events before retiring the model
                // request, otherwise fast replies close the owning turn before
                // its tool calls reach the UI. This runs even with Chat hidden.
                const auto drain_tools = [&]() {
                    for (const auto& event : ai::native_runtime::activities_since(nativeActivitySequence))
                    {
                        nativeActivitySequence = event.sequence;
                        remember_activity(event.group, event.text);
                    }
                };
                drain_tools();
                if (!ai::native_runtime::raw_stream_capture_enabled())
                {
                    end_inline_native_block();
                    nativeRawKind = ai::native_runtime::OutputKind::boundary;
                }
                // Native stream events belong in inline chat disclosures.
                // Source iteration and authoring run outside the chat worker:
                // drain them even when no chat request is currently pending.
                // The monotonic sequence prevents duplicate replay each frame.
                const auto drain_raw_stream = [&]() {
                    if (!ai::native_runtime::raw_stream_capture_enabled()) return;
                    for (const auto& event : ai::native_runtime::events_since(nativeRawSequence))
                        remember_native_output(event);
                };
                drain_raw_stream();
                if (!pending
                    || !pending->ready.load(std::memory_order_acquire))
                    return;
                // A worker may have completed between the first drain and the
                // ready check. Its published tool events still belong before
                // the final assistant response, never to the next user turn.
                drain_tools();
                // Drain events emitted just as the worker completed before
                // closing the owning Activity disclosure.
                drain_raw_stream();
                end_inline_native_block();
                latestCompletionWorkload = pendingWorkload;
                latestCompletionRequestKind = pendingRequestKind;
                statusRequestKind = pendingRequestKind;
                if (pendingRequestKind == AiDeferredRequestKind::Chat)
                    end_host_activity();
                latestTerminalFailure = pending->terminalFailure;
                latestTerminalStatus.clear();
                if (activeImageRequestId)
                    image_status(pending->cancellation.stop_requested()
                            || pending->terminalFailure == ai::ModelTerminalFailure::cancelled
                        ? "Cancelled" : pending->terminalFailure != ai::ModelTerminalFailure::none
                            || !pending->error.empty() ? "Image request failed" : "Response received");
                if (activeImageRequestId && pending->terminalFailure == ai::ModelTerminalFailure::none
                    && pending->error.empty() && !pending->cancellation.stop_requested())
                {
                    std::scoped_lock lock(pending->progressMutex);
                    image_status(pending->progress.input_image_tokens > 0u
                        ? "Image input confirmed" : "Response received · image usage not reported");
                }
                if ((activeImageRequestId || !activeReferenceFiles.empty()) && (pending->terminalFailure != ai::ModelTerminalFailure::none
                    || !pending->error.empty() || pending->cancellation.stop_requested()))
                    retain_failed_reference_images();

                if (pending->stage.load(std::memory_order_acquire)
                        == ai::ModelRequestStage::retirement_failed
                    || pending->terminalFailure == ai::ModelTerminalFailure::retirement_failed)
                {
                    if (worker.joinable())
                        worker.join();
                    requestRetirementFailed = true;
                    ++completionGeneration;
                    latestRawReply.clear();
                    latestTerminalFailure = ai::ModelTerminalFailure::retirement_failed;
                    latestTerminalStatus = "Model request ended, but HTTP handle retirement could not be confirmed. Restart the engine before another HTTP request. No response was applied.";
                    const auto info = completed_run_info("Failed");
                    pendingPrompt.clear();
                    pendingRequestKind = AiDeferredRequestKind::None;
                    pending.reset();
                    append_status(latestTerminalStatus, info);
                    return;
                }
                if (pending->cancellation.stop_requested()
                    || pending->terminalFailure == ai::ModelTerminalFailure::cancelled)
                {
                    if (worker.joinable())
                        worker.join();
                    ++completionGeneration;
                    latestRawReply.clear();
                    latestTerminalFailure = ai::ModelTerminalFailure::cancelled;
                    latestTerminalStatus = "Request cancelled. Its response was discarded.";
                    const auto info = completed_run_info("Cancelled");
                    pendingPrompt.clear();
                    pendingRequestKind = AiDeferredRequestKind::None;
                    pending.reset();
                    append_status(latestTerminalStatus, info);
                    return;
                }

                if (pending->terminalFailure != ai::ModelTerminalFailure::none)
                {
                    ++completionGeneration;
                    latestRawReply.clear();
                    latestTerminalStatus = !pending->error.empty() ? pending->error : pending->reply;
                    if (latestTerminalStatus.empty())
                        latestTerminalStatus = "The model request exhausted its time budget; no automatic repeat was started.";
                    pendingPrompt.clear();
                    append_status(latestTerminalStatus, completed_run_info("Failed"));
                }
                else if (pending->error.empty())
                {
                    latestRawReply = std::move(pending->reply);
                    remember_raw_output(latestRawReply);
                    ++completionGeneration;
                    // Only ordinary conversation is rendered verbatim. Structured
                    // authoring/tool/source payloads are host protocol, not user-facing
                    // chat. Their validated summaries and controls are appended by the
                    // owning completion handler after parsing.
                    if (latestCompletionRequestKind == AiDeferredRequestKind::Chat)
                    {
                        if (ai_reply_is_internal_protocol(latestRawReply))
                        {
                            append_status(
                                "The model returned an internal structured protocol on the conversation channel. "
                                "Epoch hid it instead of exposing host instructions or reasoning in chat; retry the change request if no staged action appears.");
                        }
                        else
                        {
                            std::string reply =
                                normalize_editor_text_for_gui(latestRawReply);
                            if (reply.empty()) reply = "(empty reply)";
                            append_status(reply, completed_run_info("Completed"));
                        }
                    }
                    pendingPrompt.clear();
                }
                else
                {
                    ++completionGeneration;
                    latestTerminalFailure = ai::ModelTerminalFailure::request_failed;
                    latestTerminalStatus = pending->error;
                    append_status(std::string("(error) ") + pending->error,
                        completed_run_info("Failed"));
                    latestRawReply.clear();
                    pendingPrompt.clear();
                }

                if (worker.joinable())
                    worker.join();
                pendingRequestKind = AiDeferredRequestKind::None;
                pending.reset();
            }

            [[nodiscard]] gui::ChatMessageRunInfo completed_run_info(
                std::string status) const
            {
                // Snapshot before retiring the request. All counts below are
                // transport observations, not estimated tokens or fabricated
                // System 1/Architect activity.
                gui::ChatMessageRunInfo info = pendingRunInfo;
                info.status = std::move(status);
                info.run_id = completionGeneration;
                if (requestStartedAt.time_since_epoch().count() != 0)
                {
                    const auto elapsed = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - requestStartedAt).count();
                    const auto elapsedSeconds = static_cast<std::uint64_t>(
                        (std::max)(std::int64_t{0}, static_cast<std::int64_t>(elapsed)) / 1000);
                    if (elapsedSeconds >= 3600u)
                        info.duration = epochengine::format_text("{}h {}m {}s",
                            elapsedSeconds / 3600u, (elapsedSeconds / 60u) % 60u,
                            elapsedSeconds % 60u);
                    else if (elapsedSeconds >= 60u)
                        info.duration = epochengine::format_text("{}m {}s",
                            elapsedSeconds / 60u, elapsedSeconds % 60u);
                    else
                        info.duration = std::to_string(elapsedSeconds) + "s";
                }
                // Prefer the measured t/s for this exact request, when the
                // provider reported completion-token usage. Historical means
                // are strictly labeled and never called live measurements.
                if (pending)
                {
                    std::scoped_lock lock(pending->progressMutex);
                    if (pending->progress.tps_known
                        && std::isfinite(pending->progress.measured_tps))
                        info.tps = epochengine::format_text("{:.1f} t/s (measured)",
                            pending->progress.measured_tps);
                }
                if (info.tps.empty() && ai::native_runtime::selected())
                {
                    const auto inventory = ai::native_runtime::snapshot(true);
                    for (const auto& model : inventory.models)
                    {
                        if (!model.tps_known || !std::isfinite(model.reported_tps)
                            || model.reported_tps <= 0.0) continue;
                        if (model.id == info.model || model.uri == info.model
                            || model.label == info.model)
                        {
                            info.tps = epochengine::format_text(
                                "model historical avg {:.1f} t/s", model.reported_tps);
                            break;
                        }
                    }
                }
                if (pending)
                {
                    std::scoped_lock lock(pending->progressMutex);
                    const auto& p = pending->progress;
                    if (p.attempt > 0u)
                    {
                        const auto decodeRate = p.token_usage_reported
                            ? ai::metrics::tokens_per_second(p.completion_tokens, p.generation_ms)
                            : std::nullopt;
                        if (decodeRate)
                            info.tps = epochengine::format_text(
                                "{:.1f} t/s ({} output tokens; {})", *decodeRate,
                                p.completion_tokens,
                                p.provider_generation_timing ? "provider decode" : "stream-observed");
                        const auto requestMs = requestStartedAt.time_since_epoch().count() != 0
                            ? std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - requestStartedAt).count()
                            : 0.0;
                        const auto effectiveRate = p.token_usage_reported
                            ? ai::metrics::tokens_per_second(p.completion_tokens, requestMs)
                            : std::nullopt;
                        info.evidence = "Transport attempt  " + std::to_string(p.attempt)
                            + "\nPrompt bytes       " + std::to_string(p.prompt_bytes)
                            + "\nResponse bytes " + std::to_string(p.response_bytes)
                            + "\nStream events     " + std::to_string(p.response_events)
                            + "\nTool arg bytes     " + std::to_string(p.tool_argument_bytes)
                            + "\nReasoning bytes " + std::to_string(p.reasoning_bytes)
                            + "\nOutput tokens     " + (p.token_usage_reported
                                ? std::to_string(p.completion_tokens) : "Not reported")
                            + "\nDecode interval   " + (p.generation_ms > 0.0
                                ? epochengine::format_text("{:.0f} ms ({})",
                                    p.generation_ms, p.provider_generation_timing
                                        ? "provider" : "stream-observed") : "Not reported")
                            + "\nDecode rate       " + info.tps
                            + "\nEffective rate    " + (effectiveRate
                                ? epochengine::format_text("{:.1f} t/s (request incl. prefill)", *effectiveRate)
                                : std::string{"Not available"})
                            + "\nTransport bytes   Wire/payload bytes; not token counts";
                    }
                    else
                        info.evidence = "Transport counters not reported";
                }
                return info;
            }

            [[nodiscard]] bool cancel_pending()
            {
                if (!pending)
                    return false;
                pending->cancellation.request_stop();
                pendingPrompt.clear();
                return true;
            }

            [[nodiscard]] std::string request_phase() const
            {
                if (!pending) return {};
                std::scoped_lock lock(pending->progressMutex);
                return pending->progress.phase;
            }

            [[nodiscard]] std::string request_activity() const
            {
                if (!pending)
                    return {};
                if (pending->cancellation.stop_requested())
                    return "Stopping this request and waiting for its worker to finish.";
                const auto stage = pending->stage.load(std::memory_order_acquire);
                {
                    std::scoped_lock lock(pending->progressMutex);
                    const auto& progress = pending->progress;
                    if (progress.attempt != 0u && (stage == ai::ModelRequestStage::queued
                        || stage == ai::ModelRequestStage::sending || stage == ai::ModelRequestStage::request_sent
                        || stage == ai::ModelRequestStage::awaiting_response || stage == ai::ModelRequestStage::receiving))
                    {
                        const auto header = epochengine::format_text(
                            "{} | attempt {}/2 | prompt {} bytes. ",
                            progress.phase, progress.attempt, progress.prompt_bytes)
                            + (progress.timeout_seconds == 0u ? std::string{}
                                : epochengine::format_text("Request deadline {} min. ",
                                    progress.timeout_seconds / 60u));
                        if (progress.response_bytes == 0u)
                            return header + (progress.streaming_requested
                                ? "Waiting for the model response; no stream bytes received yet. Loading, prompt processing and generation cannot be distinguished without provider telemetry."
                                : "Waiting for the complete reply; this transport gives no live output. Streaming shows incoming activity when the provider supports it. Neither mode applies an unfinished response.");
                        const auto silence = pending->lastResponseAt.time_since_epoch().count() == 0
                            ? 0 : std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - pending->lastResponseAt).count();
                        return header + epochengine::format_text(
                            "Received {} bytes | {} stream events | text {} bytes | tool arguments {} bytes | reasoning {} bytes | last response {}s ago. Counts are bytes/events, not tokens; a partial reply is not an applied change.",
                            progress.response_bytes, progress.response_events,
                            progress.content_bytes, progress.tool_argument_bytes,
                            progress.reasoning_bytes, (std::max)(std::int64_t{0}, silence));
                    }
                }
                switch (stage)
                {
                case ai::ModelRequestStage::queued:
                    return "Queued for the model connection. No response is available yet.";
                case ai::ModelRequestStage::sending:
                    return "Starting the model request.";
                case ai::ModelRequestStage::request_sent:
                    return "Request sent. Waiting for the model response.";
                case ai::ModelRequestStage::awaiting_response:
                    return "Waiting for the model response. No response activity has been observed yet.";
                case ai::ModelRequestStage::receiving:
                    return "Receiving the model response.";
                case ai::ModelRequestStage::completed:
                    return "Response returned. Finishing request cleanup.";
                case ai::ModelRequestStage::cancelled:
                    return "Request cancelled. Finishing worker cleanup.";
                case ai::ModelRequestStage::failed:
                    return "Request failed. Finishing worker cleanup.";
                case ai::ModelRequestStage::retirement_failed:
                    return "HTTP retirement was not confirmed. Restart the engine before another HTTP request.";
                }
                return {};
            }

            [[nodiscard]] std::uint64_t elapsed_milliseconds() const noexcept
            {
                if (!pending || requestStartedAt.time_since_epoch().count() == 0)
                    return 0u;
                const auto elapsed = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - requestStartedAt);
                return elapsed.count() <= 0
                    ? 0u
                    : static_cast<std::uint64_t>(elapsed.count());
            }

            void append_status(std::string text,
                gui::ChatMessageRunInfo runInfo = {})
            {
                text = editor_gui_text::host_status(text);
                if (text.empty())
                    return;
#if EPOCH_EDITION_PUBLIC
                remember_activity("Project assistant", text);
#endif
                // Protocol/transport details belong to the turn's single system
                // disclosure, never a second run-details footer under the answer.
                // The full original counters remain in its expandable evidence.
                if (runInfo.present())
                {
                    const std::size_t index = inlineHostActivityLine < lineRunDetails.size()
                        ? inlineHostActivityLine : lastHostActivityLine;
                    if (index < lineRunDetails.size()
                        && lineRunDetails[index].stream_block
                        && lineRunDetails[index].route == "Activity")
                    {
                        bool newerUserTurn = false;
                        for (std::size_t i = index + 1u; i < lines.size(); ++i)
                            newerUserTurn |= lines[i].starts_with("User: ");
                        if (!newerUserTurn)
                        {
                            complete_activity_metadata(runInfo);
                            runInfo = {};
                        }
                    }
                }
#if EPOCH_EDITION_PUBLIC
                constexpr bool engineWork = false;
#endif
                const bool failed = text.find("failed") != std::string::npos
                    || text.find("rejected") != std::string::npos
                    || text.find("stopped") != std::string::npos
                    || text.find("exhausted") != std::string::npos
                    || text.find("could not") != std::string::npos
                    || text.find("No code") != std::string::npos
                    || text.find("[ERROR]") != std::string::npos;
                const bool modelSummary = runInfo.present()
                    && runInfo.status == "Awaiting validation";
                const bool routineChatStatus = !runInfo.present()
                    && (text.find("Message queued") != std::string::npos
                        || text.find("request started") != std::string::npos
                        || text.find("request queued") != std::string::npos
                        || text.find("model is still loading") != std::string::npos
                        || text.find("Restoring the saved model set") != std::string::npos
                        || text.find("Restoring saved model set") != std::string::npos
                        || text.find("Enter a goal objective") != std::string::npos
                        || text.find("New goal creation cancelled") != std::string::npos
                        || text.find("request has not been sent") != std::string::npos
                        || text.find("Queued message has not been sent") != std::string::npos
                        || text.find("Model readiness") != std::string::npos);
                // A queued request has not run or completed. Preserve those
                // notices in the activity log, but do not manufacture a
                // completed run disclosure containing only "queued" text.
                if (routineChatStatus)
                    return;
                // Routing is a host event, not a conversational reply. Never
                // print its technical handoff line as a separate assistant card.
                if (!runInfo.present() && hostActivityRequested
                    && text.starts_with("Intent routed to "))
                {
                    append_host_activity("System 1", text);
                    return;
                }
                // The MCP validator rejected this scene plan before applying
                // it. Report one actionable failure below the user's request,
                // with the exact host diagnostic in the associated run details.
                if (!runInfo.present() && hostActivityRequested
                    && text.starts_with("scene.arrange has invalid"))
                {
                    append_host_activity("Project Assistant", text);
                    append_line("Assistant: Couldn't arrange the scene. The proposed grid arguments "
                        "failed validation, so no scene change was applied. Open run details "
                        "for the exact reason.");
                    end_host_activity(true);
                    return;
                }
                if (engineWork)
                {
                    // A failed *gate* is not a failed *task*: the verifier may
                    // immediately queue a compiler/test/visual repair. Preserve
                    // the owning turn and its live native stream across those
                    // repairs. Only the host supervisor may end the task after
                    // every worker, test and deferred dispatch has retired.
                    append_host_activity("Assistant", text);
                    return;
                }
                const std::string entry = (engineWork
                    ? "Agent: " : "Assistant: ") + std::move(text);
                if (!runInfo.present() && !lines.empty() && lines.back() == entry
                    && (lineRunDetails.empty() || !lineRunDetails.back().present()))
                    return;
                // Append the human-facing reply after its existing activity.
                // Neither async completion nor retry may reorder another turn.
                append_line(entry, std::nullopt, std::move(runInfo));
            }

            void append_user_message(std::string text, std::string retryInput = {},
                gui::ChatMessageRunInfo runInfo = {})
            {
                if (suppressUserEcho) return; // Existing queued turn owns activity.
                sourceOutcomeEmitted = false;
                sourceTaskOpen = false;
                end_host_activity();
                lastHostActivityLine = kNoActivityLine;
                hostActivityRequested = true;
                lastWorkObservation.clear();
                text = normalize_editor_text_for_gui(text);
                if (text.empty() && runInfo.images.empty() && runInfo.files.empty()) return;
                append_line("User: " + std::move(text), retryInput.empty()
                    ? std::nullopt : std::optional<std::string>{std::move(retryInput)}, std::move(runInfo));
                // Always allocate exactly one system disclosure in chronological
                // order, immediately after the initiating user message.
                append_host_activity("System", "Request submitted");
            }

            [[nodiscard]] bool queue_input(std::string text, bool retryable)
            {
                if (pending || worker.joinable() || requestRetirementFailed
                    || !queuedInput.empty() || ((text.empty() || is_ws_only(text))
                        && draftReferenceFiles.empty())) return false;
                gui::ChatMessageRunInfo imageInfo{};
                for (const auto& file : draftReferenceFiles)
                    text += "\n\nUser-attached reference file: " + file.name
                        + "\nContent-Length: " + std::to_string(file.content.size())
                        + "\n```text\n" + file.content + "\n```\n";
                if (text.size() > 65536u) return false;
                std::string display = text;
                // Fold the importer-owned text envelope for presentation only.
                // The exact request/retry input below remains byte-for-byte intact.
                constexpr std::string_view fileMarker = "\n\nUser-attached reference file: ";
                for (std::size_t offset = 0u; imageInfo.files.size() < 16u;)
                {
                    const auto start = display.find(fileMarker, offset);
                    if (start == std::string::npos) break;
                    const auto nameStart = start + fileMarker.size();
                    const auto nameEnd = display.find('\n', nameStart);
                    constexpr std::string_view fence = "\n```text\n";
                    if (nameEnd == std::string::npos || nameEnd == nameStart)
                    { offset = nameStart; continue; }
                    std::size_t fenceStart = nameEnd;
                    std::optional<std::size_t> countedBytes{};
                    constexpr std::string_view lengthMarker = "\nContent-Length: ";
                    if (display.compare(nameEnd, lengthMarker.size(), lengthMarker) == 0)
                    {
                        const auto first = nameEnd + lengthMarker.size();
                        const auto last = display.find('\n', first);
                        std::size_t count{};
                        if (last == std::string::npos) { offset = first; continue; }
                        const auto parsed = std::from_chars(display.data() + first, display.data() + last, count);
                        if (parsed.ec != std::errc{} || parsed.ptr != display.data() + last || count > 32768u)
                        { offset = first; continue; }
                        countedBytes = count;
                        fenceStart = last;
                    }
                    if (display.compare(fenceStart, fence.size(), fence) != 0)
                    { offset = nameStart; continue; }
                    const auto bodyStart = fenceStart + fence.size();
                    const auto end = countedBytes ? bodyStart + *countedBytes : display.find("\n```\n", bodyStart);
                    if (end == std::string::npos || end - bodyStart > 32768u)
                    { offset = bodyStart; continue; }
                    if (end > display.size() || display.compare(end, 5u, "\n```\n") != 0)
                    { offset = bodyStart; continue; }
                    imageInfo.files.push_back({display.substr(nameStart, nameEnd - nameStart),
                        display.substr(bodyStart, end - bodyStart)});
                    display.erase(start, end + 5u - start);
                    offset = start;
                }
                if (!imageInfo.files.empty())
                    imageInfo.run_id = (std::uint64_t{1} << 59u) | ++nextImageRequestId;
                if (!draftReferenceImages.empty())
                {
                    imageInfo.image_request_id = ++nextImageRequestId;
                    for (const auto& image : draftReferenceImages)
                    {
                        auto preview = referencePreviews.contains(image.client_id)
                            ? referencePreviews.at(image.client_id) : gui::ChatImageAttachment{};
                        preview.id = image.client_id;
                        preview.name = image.name;
                        preview.status = "Queued";
                        imageInfo.images.push_back(std::move(preview));
                    }
                }
                queuedImageRequestId = imageInfo.image_request_id;
                queuedDraftText = display;
                queuedReferenceFiles = imageInfo.files;
                append_user_message(display, retryable && draftReferenceImages.empty() ? text : std::string{}, std::move(imageInfo));
                queuedReferenceImages = std::move(draftReferenceImages);
                draftReferenceFiles.clear();
                queuedInput = std::move(text);
                queuedInputAt = std::chrono::steady_clock::now();
                queuedCompletionGeneration = completionGeneration;
                queuedInputRetryable = retryable;
                // The first subsequent frame draws the message. Routing and
                // model preparation start only after that frame was presented.
                queuedBlockReported.clear();
                queuedInputFrames = 2u;
                return true;
            }

            [[nodiscard]] std::optional<std::string> take_queued_input()
            {
                if (queuedInput.empty() || pending || worker.joinable()
                    || requestRetirementFailed) return std::nullopt;
                if (queuedInputFrames > 0u && --queuedInputFrames > 0u)
                    return std::nullopt;
                queuedInputAt = {};
                queuedBlockReported.clear();
                activeReferenceImages = std::move(queuedReferenceImages);
                activeReferenceFiles = std::move(queuedReferenceFiles);
                activeDraftText = std::move(queuedDraftText);
                activeReferencePrompt = activeReferenceImages.empty() ? std::string{} : queuedInput;
                activeImageRequestId = std::exchange(queuedImageRequestId, 0u);
                return std::exchange(queuedInput, {});
            }

            void image_status(std::string_view status, std::uint64_t requestId = 0u)
            {
                const auto id = requestId ? requestId : activeImageRequestId;
                if (!id) return;
                for (auto& line : lineRunDetails)
                    if (line.image_request_id == id)
                        for (auto& image : line.images)
                            if (image.status.compare(status) != 0) image.status = status;
            }

            void restore_reference_images(std::vector<ai::ReferenceImage>& retained)
            {
                // A provider change, rejected dispatch or cancellation must not
                // discard either the queued attachments or a newer draft.
                for (auto& image : retained) draftReferenceImages.push_back(std::move(image));
                retained.clear();
                if (&retained == &queuedReferenceImages)
                {
                    restore_reference_files(queuedReferenceFiles);
                    if (input.empty()) input = queuedDraftText;
                    image_status("Not sent", queuedImageRequestId);
                    queuedImageRequestId = 0u;
                }
                else if (&retained == &activeReferenceImages)
                {
                    restore_reference_files(activeReferenceFiles);
                    if (input.empty()) input = activeDraftText;
                    image_status("Not sent");
                    activeImageRequestId = 0u;
                }
            }

            void restore_reference_files(std::vector<gui::ChatFileAttachment>& retained)
            {
                for (auto& file : retained) draftReferenceFiles.push_back(std::move(file));
                retained.clear();
            }

            void retain_failed_reference_images()
            {
                // Keep the failed request's receipt, while allowing a corrected
                // resend with the same bytes alongside any newer unsent draft.
                for (auto& image : activeReferenceImages) draftReferenceImages.push_back(std::move(image));
                activeReferenceImages.clear();
                restore_reference_files(activeReferenceFiles);
                activeImageRequestId = 0u;
                if (input.empty()) input = activeDraftText;
                activeReferencePrompt.clear();
            }

            [[nodiscard]] std::vector<std::uint8_t> retryable_lines() const
            {
                std::vector<std::uint8_t> result(lines.size());
                for (std::size_t i = 0u; i < lines.size() && i < retryInputs.size(); ++i)
                    result[i] = retryInputs[i].has_value() ? 1u : 0u;
                return result;
            }

            [[nodiscard]] std::optional<std::string> retry_input(std::size_t line) const
            {
                return line < lines.size() && line < retryInputs.size()
                    ? retryInputs[line] : std::nullopt;
            }

            [[nodiscard]] std::string copyable_line(std::size_t index) const
            {
                if (index >= lines.size()) return {};
                if (index < lineRunDetails.size()
                    && lineRunDetails[index].stream_block)
                {
                    const auto& info = lineRunDetails[index];
                    const std::string speaker = info.model.empty()
                        ? "Epoch Host" : info.model;
                    std::string exportText = speaker + " · " + info.route;
                    if (info.route == "Activity" && !info.activity_events.empty())
                    {
                        for (const auto& event : info.activity_events)
                        {
                            exportText += "\n  " + event.group + " · " + event.summary;
                            if (event.tool && !event.status.empty())
                                exportText += " · " + event.status;
                            exportText += "\n    " + event.detail;
                        }
                    }
                    else exportText += "\n" + info.evidence;
                    return exportText;
                }
                std::string_view text{lines[index]};
                if (text.starts_with("Assistant: ")) text.remove_prefix(11u);
                else if (text.starts_with("User: ")) text.remove_prefix(6u);
                else if (text.starts_with("Agent: ")) text.remove_prefix(7u);
                return std::string{text};
            }

            [[nodiscard]] std::string copyable_transcript() const
            {
                std::string result{};
                for (std::size_t index = 0; index < lines.size(); ++index)
                {
                    if (!result.empty()) result += "\n\n";
                    if (index < lineRunDetails.size()
                        && lineRunDetails[index].stream_block)
                    {
                        result += copyable_line(index); // Includes model and disclosure label.
                    }
                    else if (lines[index].starts_with("User: "))
                        result += "You: " + copyable_line(index);
                    else if (lines[index].starts_with("Assistant: "))
                    {
                        const auto& info = lineRunDetails[index];
                        result += (info.model.empty() ? "Epoch Assistant" : info.model)
                            + ": " + copyable_line(index);
                    }
                    else if (lines[index].starts_with("Agent: "))
                        result += "Epoch Host: " + copyable_line(index);
                    else result += copyable_line(index);
                }
                return result;
            }

            void submit(std::string text)
            {
                (void)submit_with_display(std::move(text), {});
            }

            [[nodiscard]] bool submit_with_display(
                std::string text,
                std::string displayText,
                epochengine::ai::InferenceWorkload workload =
                    epochengine::ai::InferenceWorkload::chat,
                AiDeferredRequestKind requestKind =
                    AiDeferredRequestKind::Chat,
                bool echoRequest = true)
            {
                std::scoped_lock selectionLock(aiModelSelectionMutex);
                if (text.empty() || is_ws_only(text))
                    return false;
                displayText = normalize_editor_text_for_gui(
                    displayText.empty() ? text : displayText);
                if (displayText.empty())
                    displayText = "(bounded local-model request)";

                if (pending || worker.joinable() || requestRetirementFailed)
                {
                    append_status("(busy)");
                    return false;
                }

                if (echoRequest) append_user_message(displayText);
                clear_completion_payload();
                pendingPrompt = displayText;

                pending = std::make_shared<AiChatRequestState>();
                nativeStreamCapture=ai::native_runtime::selected();
                // Keep the global raw-output cursor; no reset on another chat.
                pendingWorkload = workload;
                pendingRequestKind = requestKind;
                requestStartedAt = std::chrono::steady_clock::now();
                pendingRunInfo = {};
                if (ai::native_runtime::selected())
                {
                    const auto runtime = ai::native_runtime::snapshot(false);
                    pendingRunInfo.model = !runtime.model.empty()
                        ? runtime.model : "Native model (ID not reported)";
                }
                else
                {
                    pendingRunInfo.model = epochengine::ai::active_model_name();
                    if (pendingRunInfo.model.empty())
                        pendingRunInfo.model = "Provider-selected (ID not reported)";
                }
                switch (requestKind)
                {
                case AiDeferredRequestKind::Authoring:
                    pendingRunInfo.route = "Project Authoring"; break;
                case AiDeferredRequestKind::Tooling:
                    pendingRunInfo.route = "Project Tooling"; break;
                case AiDeferredRequestKind::Chat:
                    pendingRunInfo.route = "Conversation"; break;
                default:
                    pendingRunInfo.route = "Host request"; break;
                }
                // Epoch's intent dispatcher runs on the host. Do not invent an
                // Architect-model call merely because a System 1 policy exists.
                pendingRunInfo.architect =
                    "System 1: host intent dispatch; separate Architect model not reported";
                try
                {
                    worker = std::jthread(
                        [request = pending,
                         t = std::move(text), images = activeReferenceImages,
                         workload]() mutable
                        {
                            try
                            {
                                epochengine::systems::threading::ScopedThreadActivity
                                    threadActivity{};
                                // A queued worker may first run after its
                                // context closed. Do not send an already
                                // cancelled request. The same owned token also
                                // interrupts queued or in-flight transport work.
                                if (!request->cancellation.stop_requested())
                                    request->reply =
                                        epochengine::ai::send_to_engine_ai(
                                            t, workload,
                                            request->cancellation.get_token(),
                                            [request](ai::ModelRequestStage stage) noexcept
                                            {
                                                request->stage.store(stage,
                                                    std::memory_order_release);
                                            }, &request->terminalFailure,
                                            [request](const ai::ModelRequestProgress& progress)
                                            { request->observe_progress(progress); }, images);
                            }
                            catch (const std::exception& e)
                            {
                                request->error = e.what();
                            }
                            catch (...)
                            {
                                request->error =
                                    "The local-model request failed with an unknown error.";
                            }
                            request->ready.store(
                                true, std::memory_order_release);
                        });
                }
                catch (const std::exception& exception)
                {
                    // The earlier busy guard guarantees this is only the
                    // newly allocated request, never an existing operation.
                    pending.reset();
                    pendingPrompt.clear();
                    pendingRequestKind = AiDeferredRequestKind::None;
                    requestStartedAt = {};
                    append_status(std::string{"The local-model worker could not start: "}
                        + exception.what());
                    return false;
                }
                catch (...)
                {
                    pending.reset();
                    pendingPrompt.clear();
                    pendingRequestKind = AiDeferredRequestKind::None;
                    requestStartedAt = {};
                    append_status("The local-model worker could not start because of an unknown scheduling failure.");
                    return false;
                }
                return true;
            }
        private:
            void append_line(std::string text,
                std::optional<std::string> retryInput = std::nullopt,
                gui::ChatMessageRunInfo runInfo = {})
            {
                retryInputs.resize(lines.size());
                lineRunDetails.resize(lines.size());
                lines.emplace_back(std::move(text));
                retryInputs.emplace_back(std::move(retryInput));
                lineRunDetails.emplace_back(std::move(runInfo));
                trim_lines();
            }

            void trim_lines()
            {
                if (lines.size() > kMaxLines)
                {
                    const auto removed = static_cast<std::ptrdiff_t>(lines.size() - kMaxLines);
                    lines.erase(
                        lines.begin(),
                        lines.begin() + removed);
                    retryInputs.erase(retryInputs.begin(), retryInputs.begin() + removed);
                    lineRunDetails.erase(
                        lineRunDetails.begin(), lineRunDetails.begin() + removed);
                    if (inlineNativeLine != (std::numeric_limits<std::size_t>::max)())
                    {
                        if (inlineNativeLine < static_cast<std::size_t>(removed))
                        {
                            inlineNativeLine = (std::numeric_limits<std::size_t>::max)();
                            inlineNativeRunId = 0u;
                        }
                        else inlineNativeLine -= static_cast<std::size_t>(removed);
                    }
                    if (inlineHostActivityLine != kNoActivityLine)
                    {
                        if (inlineHostActivityLine < static_cast<std::size_t>(removed))
                        {
                            inlineHostActivityLine = kNoActivityLine;
                            inlineHostActivityId = 0u;
                        }
                        else inlineHostActivityLine -= static_cast<std::size_t>(removed);
                    }
                    if (lastHostActivityLine != kNoActivityLine)
                    {
                        if (lastHostActivityLine < static_cast<std::size_t>(removed))
                            lastHostActivityLine = kNoActivityLine;
                        else lastHostActivityLine -= static_cast<std::size_t>(removed);
                    }
                }
            }
        };

        [[nodiscard]] static std::string last_chat_line_with_prefix(const AiChat& chat, std::string_view prefix)
        {
            for (auto it = chat.lines.rbegin(); it != chat.lines.rend(); ++it)
            {
                if (it->rfind(prefix, 0) == 0)
                    return it->substr(prefix.size());
            }
            return {};
        }

        struct EditorEntity
        {
            scene::SceneObjectId sceneObjectId{ scene::kInvalidSceneObjectId };
            std::string name{};
            std::string type{};
            std::string category{};
            std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
            bool visible{ true };
            bool editorOnly{ false };
            std::optional<scene::SceneTextureMaterialSnapshot>
                textureMaterial{};
        };

        [[nodiscard]] std::optional<EditorWorkspaceTab> requested_editor_workspace_tab() noexcept
        {
            const auto parse_workspace_tab = [](std::string_view value) noexcept
                -> std::optional<EditorWorkspaceTab>
            {
                if (value == "AI" || value == "ai" || value == "Ai")
                    return EditorWorkspaceTab::AI;
                if (value == "Systems" || value == "systems")
                    return EditorWorkspaceTab::Systems;
                if (value == "Scripts" || value == "scripts")
                    return EditorWorkspaceTab::Assets;
                if (value == "Assets" || value == "assets")
                    return EditorWorkspaceTab::Assets;
                if (value == "Output" || value == "output")
                    return EditorWorkspaceTab::Output;
                return std::nullopt;
            };

#if defined(_MSC_VER)
            char* rawValue = nullptr;
            std::size_t rawSize = 0;
            if (_dupenv_s(&rawValue, &rawSize, "EPOCH_EDITOR_START_WORKSPACE") != 0 || !rawValue)
                return std::nullopt;

            const auto tab = parse_workspace_tab(std::string_view{ rawValue });
            std::free(rawValue);
            return tab;
#else
            const char* const rawValue = std::getenv("EPOCH_EDITOR_START_WORKSPACE");
            if (!rawValue)
                return std::nullopt;

            return parse_workspace_tab(std::string_view{ rawValue });
#endif
        }

        [[nodiscard]] EditorWorkspaceTab initial_editor_workspace_tab() noexcept
        {
            return requested_editor_workspace_tab().value_or(EditorWorkspaceTab::Output);
        }

        [[nodiscard]] EditorMainSurface initial_editor_main_surface(EditorWorkspaceTab workspace) noexcept
        {
            switch (workspace)
            {
            case EditorWorkspaceTab::AI:
                return EditorMainSurface::AISandbox;
            case EditorWorkspaceTab::Systems:
                return EditorMainSurface::Systems;
            case EditorWorkspaceTab::Assets:
            case EditorWorkspaceTab::Scripts:
                return EditorMainSurface::Assets;
            case EditorWorkspaceTab::Project:
                return EditorMainSurface::Project;
            case EditorWorkspaceTab::Output:
            default:
                return EditorMainSurface::Scene;
            }
        }

        struct ProjectTextureThumbnailState final
        {
            SpriteHandle sprite{};
            std::array<std::uint64_t, 4> artifact_key{};
            std::uint32_t width{};
            std::uint32_t height{};
            std::uint32_t source_width{};
            std::uint32_t source_height{};
            std::uintmax_t source_bytes{};
            std::filesystem::file_time_type source_write_time{};
        };

        struct GuiProjectionTombstone final
        {
            authoring::gui::WidgetHandle widget{};
            EditorEntity entity{};
        };

        using GuiTemplatePreset = authoring::gui::TemplatePreset;

        struct GuiTemplateDefinition final
        {
            GuiTemplatePreset preset{};
            std::string_view id{};
            std::string_view label{};
            std::string_view purpose{};
        };

        inline constexpr std::array kGuiTemplateCatalog{
            GuiTemplateDefinition{
                GuiTemplatePreset::blank_canvas,
                "blank_canvas",
                "Blank Canvas",
                "Empty 1280 x 720 application canvas."},
            GuiTemplateDefinition{
                GuiTemplatePreset::desktop_app,
                "desktop_app",
                "Desktop App",
                "Desktop shell with header, navigation, content, and actions."},
            GuiTemplateDefinition{
                GuiTemplatePreset::dashboard,
                "dashboard",
                "Dashboard",
                "Metrics dashboard with navigation, cards, and activity feed."},
            GuiTemplateDefinition{
                GuiTemplatePreset::mobile_app,
                "mobile_app",
                "Mobile App",
                "390 x 844 mobile application shell and bottom navigation."},
            GuiTemplateDefinition{
                GuiTemplatePreset::game_hud,
                "game_hud",
                "Game HUD",
                "Responsive game status, objective, and action controls."}
        };

        struct CodeWorkspaceUiState
        {
            editor_code_workspace::Controller controller{};
            editor_code_workspace::WorkspaceAuthority authority{
                .surface_revision = 1u,
                .project_revision = 1u,
                .async_generation = 1u};
            std::string configurationKey{};
            std::string draft{};
            std::optional<editor_code_workspace::DocumentHandle> draftHandle{};
            std::uint64_t draftRevision{};
            std::string status{"No project script workspace is open."};
            std::string gotoLine{"1"};
            std::size_t gotoTarget{1u};
            std::uint64_t gotoGeneration{};
            std::optional<editor_code_workspace::DocumentHandle> discardClose{};
            bool discardReloadArmed{};
            std::string evidenceDigest{};
        };

        struct EditorModelInstallState
        {
            ai::model_install::InstallPlan plan{};
            std::filesystem::path cacheRoot{};
            std::filesystem::path stagingRoot{};
            std::filesystem::path outputLog{};
            std::size_t artifactIndex{};
            std::uint64_t totalBytes{};
            std::uint64_t completedBytes{};
            platform::child_process::ProcessHandle process{};
            std::optional<std::future<ai::model_install::PublishResult>>
                publishPending{};
        };

        enum class TimelineWorkspaceSection : std::uint8_t
        {
            sequence = 0,
            save_restore,
            media,
            diagnostics
        };

        struct ScriptOscillator
        {
            bool enabled{ false };
            scene::SceneObjectId entity_id{ scene::kInvalidSceneObjectId };
            std::array<float, 3> base_position{ 0.0f, 0.0f, 0.0f };
            std::uint8_t axis{ 0u };
            float amplitude{ 2.0f };
            float frequency_hz{ 0.5f };
            double phase_radians{};
        };

        struct AiAuthoringApplyProgress final
        {
            std::atomic<std::size_t> batches{};
            std::atomic<std::size_t> objects{};
            std::atomic<std::size_t> commands{};
            std::atomic<std::size_t> nextBatch{32u};
            std::atomic<std::uint64_t> lastBatchUs{};
            std::atomic<std::uint64_t> peakBatchUs{};
        };

        struct EditorState
        {
            bool initialized{ false };
            TopMenu openMenu{ TopMenu::None };
            std::string projectId{};
            std::string projectPickerId{};
            std::string projectName{};
            std::string projectRoot{};
            std::string projectScenePath{};
            std::string projectManifest{};
            std::string projectTemplate{};
            std::string projectKind{};
            std::string activeScript{};
            std::string activeRuntimeScene{};
            std::string activeWorld{};
            std::string projectStatus{};
            std::string projectBuildStatus{};
            std::string projectBuildDiagnosticTail{};
            std::string scriptBuildStatus{};
            std::optional<std::future<EditorScriptBuildResult>> scriptBuildPending{};
            editor_tasks::CancellationTicket scriptBuildCancellation{};
            std::string scriptBuildTarget{};
            std::uint64_t scriptBuildGeneration{};
            std::uint64_t scriptBuildCompletedGeneration{};
            bool scriptBuildSucceeded{};
            bool aiProjectScriptRunAfterBuild{};
            bool aiProjectScriptRepairQueued{};
            std::string aiProjectScriptCompilerDiagnostics{};
            std::uint64_t aiProjectScriptBuildGeneration{};
            std::string aiProjectScriptRunRoot{}, aiProjectScriptRunPath{}, aiProjectScriptRunSource{}, aiProjectScriptRunObjective{};
            std::string newScriptName{ "sandbox_iteration" };
            CodeWorkspaceUiState projectCodeWorkspace{};
            editor_authoring_history::Controller authoringHistory{};
            std::string scriptEditorPath{};
            std::string scriptEditorText{};
            std::string scriptEditorStatus{ "No script source loaded." };
            bool scriptEditorDirty{ false };
            bool aiTasksDetailsVisible{false};
            std::optional<std::future<EditorProjectBuildResult>> projectBuildPending{};
            editor_tasks::CancellationTicket projectBuildCancellation{};
            project_lifecycle::EvidenceLedger projectLifecycle{};
            platform::child_process::ProcessHandle projectRuntimeProcess{};
            std::optional<platform::child_process::ProcessSnapshot>
                projectRuntimeSnapshot{};
            std::string projectRuntimeStatus{
                "No external project runtime has been launched this session."};
            bool projectRuntimeForceStopAvailable{false};
            std::uint64_t projectBuildLifecycleGeneration{};
            std::uint64_t projectBuildProjectKey{};
            std::string projectBuildProjectId{};
            std::string projectBuildProjectRoot{};
            core::sha256::Digest projectBuildAcceptedOutputDigest{};
            bool projectBuildRunAfterBuild{ false };
            std::string projectBuildRunScene{};
            std::string projectBuildOutputPath{};
            std::string projectBuildRunBackend{};
            double projectBuildRunFrameLimitFps{ 60.0 };
            previewgrid::CameraMode projectBuildRunCameraMode{ previewgrid::CameraMode::Editor };
            input::ProfilePreset projectBuildRunInputProfile{ input::ProfilePreset::EditorDefault };
            std::string projectRunBackend{ "opengl" };
            EditorCanvas2DProjectSnapshot canvas2dProject{};
            double projectRunFrameLimitFps{ 60.0 };
            struct FrameLimitEditState final
            {
                bool initialized{};
                bool manual{};
                std::string text{"1500"};
                std::string error{};
            };
            FrameLimitEditState projectFrameLimitEdit{};
            previewgrid::CameraMode projectCameraMode{ previewgrid::CameraMode::Editor };
            previewgrid::CameraMode viewportCameraMode{ previewgrid::CameraMode::Editor };
            input::ProfilePreset inputProfilePreset{ input::ProfilePreset::EditorDefault };
            gui::ThemePreference themePreference{ gui::ThemePreference::FollowSystemDark };
            std::uint32_t themeCycleIntervalSeconds{30u};
            bool roundedRectangles{ true };
            double editorFrameLimitFps{ 120.0 };
            FrameLimitEditState editorFrameLimitEdit{};
            bool editorFrameVsyncRequested{ false };
            std::string selectedProjectFile{};
            std::string selectedAssetPath{};
            std::unique_ptr<
                editor_project_textures::ProjectTextureController>
                projectTextures{};
            std::unique_ptr<
                editor_project_input_settings::Controller>
                projectInputSettings{};
            std::string projectInputSettingsStatus{
                "Open Input Manager to load the active project's bindings."};
            std::string projectTextureStatus{
                "Create an editable texture or select a BMP, TGA, PPM, or .epoch_texture source."};
            std::string assetSearch{};
            std::string assetBrowserFolder{};
            std::size_t assetCategory{};
            std::size_t assetWorkspaceSection{};
            std::size_t projectWorkspaceSection{};
            bool projectCreationOpen{};
            std::size_t projectCreationKind{};
            float assetGridScroll{};
            std::vector<std::uint64_t> assetGridVisibleIds{};
            std::uint64_t selectedProjectAudioCueId{};
            std::string projectAudioStatus{
                "Open Project Audio to manage decoded cues and buses."};
            std::size_t guiAuthoringSection{};
            std::size_t guiDocumentTab{};
            gui::DragSurfaceState guiCanvasWidgetDrag{};
            authoring::gui::WidgetHandle guiCanvasWidgetDragHandle{};
            float guiCanvasWidgetDragOriginX{};
            float guiCanvasWidgetDragOriginY{};
            float guiCanvasWidgetDragPreviewX{};
            float guiCanvasWidgetDragPreviewY{};
            std::unique_ptr<authoring::gui::GuiDocument> guiDocument{};
            authoring::gui::WidgetHandle guiDocumentRoot{};
            std::unordered_map<scene::SceneObjectId, authoring::gui::WidgetHandle>
                guiDocumentWidgets{};
            std::vector<GuiProjectionTombstone>
                guiProjectionTombstones{};
            std::string guiDocumentStatus{"GUI document is ready to initialize."};
            std::string guiDocumentSourcePath{};
            std::uint64_t guiDocumentSavedRevision{};
            bool guiDocumentLoadAttempted{};
            scene::SceneObjectId guiContentEditorEntityId{
                scene::kInvalidSceneObjectId};
            std::string guiContentPrimary{};
            std::string guiContentSecondary{};
            std::string guiActionEditor{};
            std::string guiTooltipEditor{};
            std::string newGuiTabName{"Tab"};
            std::size_t guiTemplatePreset{};
            bool guiDocumentReplaceArmed{};
            std::string newTextureName{"new_texture"};
            std::size_t newTextureExtentIndex{1u};
            std::size_t newTexturePatternIndex{};
            std::string newTextureLayerName{"Layer"};
            std::size_t textureBrushColorIndex{2u};
            std::size_t textureBrushRadiusIndex{1u};
            std::uint16_t textureBrushOpacity{65'535u};
            std::uint16_t textureBrushHardness{65'535u};
            std::uint8_t textureBrushChannelMask{0x0fu};
            bool textureMaskPaintReveal{};
            authoring::texture::StrokeDescriptor textureBrushStroke{};
            std::string textureBrushLogicalPath{};
            std::uint32_t textureBrushLastX{0xffffffffu};
            std::uint32_t textureBrushLastY{0xffffffffu};
            bool textureBrushActive{};
            scene::SceneTextureFilter textureFilter{
                scene::SceneTextureFilter::linear};
            scene::SceneTextureAddress textureAddressU{
                scene::SceneTextureAddress::clamp_to_edge};
            scene::SceneTextureAddress textureAddressV{
                scene::SceneTextureAddress::clamp_to_edge};
            scene::SceneTextureAlphaMode textureAlpha{
                scene::SceneTextureAlphaMode::straight};
            scene::SceneTextureColorSpace textureColorSpace{
                scene::SceneTextureColorSpace::srgb};
            float textureAlphaCutoff{0.5f};
            SpriteHandle projectTexturePreviewSprite{};
            std::string projectTexturePreviewPath{};
            std::array<std::uint64_t, 4> projectTexturePreviewKey{};
            std::uint32_t projectTexturePreviewWidth{};
            std::uint32_t projectTexturePreviewHeight{};
            std::unordered_map<std::string, ProjectTextureThumbnailState>
                projectTextureThumbnails{};
            std::unordered_map<std::string, ProjectTextureThumbnailState>
                projectSourceTextureThumbnails{};
            std::uint64_t projectTextureThumbnailAtlasSignature{};
            forest::ForestAssetDocument plantLabDocument{};
            forest::CompiledForestAsset plantLabCompiled{};
            project_forests::ForestArtifactLocator plantLabPublishedArtifact{};
            std::uint64_t plantLabCompiledRevision{};
            std::uint64_t plantLabCompiledContentHash{};
            bool plantLabDocumentInitialized{};
            std::vector<EditorEntity> forestPreviewEntities{};
            std::uint64_t forestPreviewRevision{};
            std::uint64_t forestPreviewContentHash{};
            std::optional<previewgrid::CameraRigSnapshot> forestProjectCamera{};
            std::string plantLabStatus{
                "Plant Lab document is ready to initialize."};
            std::string plantLabAssetStatus{
                "No project Plant Lab asset has been opened."};
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            std::unique_ptr<editor_tilemaps::TileMapWorkspaceController>
                tileMapWorkspace{};
            std::string tileMapStatus{"Open the 2D Scene/UI workspace to create a map."};
            std::uint32_t tileMapTileExtent{32u};
#endif
            std::vector<EditorEntity> entities{};
            ScriptOscillator scriptOscillator{};
            std::unordered_map<scene::SceneObjectId, ScriptOscillator> scriptCubeOscillators{};
            authoring::scene::SceneDocument sceneDocument{};
            lighting::LightManager sceneLighting{ 128 };
            std::vector<lighting::LightHandle> sceneLightHandles{};
            bool sceneLightingDirty{ true };
            scene_interaction::SceneInteractionQuery sceneInteraction{};
            bool sceneInteractionDirty{ true };
            std::size_t selectedEntity{ 0 };
            scene::SceneObjectId selectedEntityId{ scene::kInvalidSceneObjectId };
            std::vector<scene::SceneObjectId> selectedEntityIds{};
            editor_hierarchy::Controller worldHierarchy{};
            std::string worldHierarchyFilter{};
            std::string worldHierarchyStatus{"Hierarchy ready."};
            float worldHierarchyScrollY{};
            float worldHierarchyRequestedScrollY{-1.0f};
            scene::SceneObjectId worldHierarchyScrollSelection{
                scene::kInvalidSceneObjectId};
            std::uint64_t sceneDocumentRevision{ 1u };
            std::vector<std::string> logLines{};
            bool helpersVisible{ true };
            bool sceneDragActive{ false };
            bool sceneLeftWasHeld{ false };
            std::size_t sceneDragEntity{ 0 };
            scene::SceneObjectId sceneDragEntityId{ scene::kInvalidSceneObjectId };
            gui::Vec2 sceneDragStartMouse{};
            std::array<float, 3> sceneDragStartPosition{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> sceneDragStartHit{ 0.0f, 0.0f, 0.0f };
            std::vector<std::pair<scene::SceneObjectId, std::array<float, 3>>> sceneDragStartPositions{};
            bool sceneDragHasPlaneHit{ false };
            bool sceneMarqueeActive{ false };
            bool sceneMarqueeAdditive{ false };
            gui::Vec2 sceneMarqueeStart{};
            gui::Vec2 sceneMarqueeCurrent{};
            EditorTimeSnapshot timeSnapshot{};
            // Sample native render completions, independently of UI updates.
            epochengine::renderhost::frame_rate::Sample chromeFrameRate{};
            double chromeFpsSample{};
            bool chromeFpsMeasured{};
            // Display-only path discovery is sampled, never repeated for each
            // rendered status row. Real actions still resolve fresh authority.
            std::chrono::steady_clock::time_point aiStatusPathsSample{};
            epochengine::ai::EvidencePaths aiStatusPaths{};
            bool aiStatusMcpAvailable{};
            EditorTimeControl timeControl{};
            epochengine::saveload::StreamingSaveConfig streamingSaveConfig{};
            epochengine::saveload::StreamingSaveStatus streamingSaveStatus{};
            epochengine::saveload::StreamingCheckpointRecord lastCheckpointRecord{};
            epochengine::timeline::TimelineState timelineState{};
            std::vector<epochengine::timeline::TimelineTrack> timelineTracks{};
            std::vector<epochengine::timeline::TimelineEvent> timelineEvents{};
            epochengine::media::timeline_preview::PreviewState
                mediaTimelinePreview{};
            TimelineWorkspaceSection timelineWorkspaceSection{
                TimelineWorkspaceSection::sequence};
            EditorApplicationKind applicationKind{ EditorApplicationKind::Standard };
            core::ScenePreviewMode previewMode{ core::ScenePreviewMode::Editor };
            bool sceneRunFullscreenInEditor{}; // Optional Run dropdown: full client-area scene, Escape returns.
            EditorWorkspaceTab workspaceTab{ initial_editor_workspace_tab() };
            EditorWorkspaceTab dockStatusTab{ EditorWorkspaceTab::Output };
            EditorMainSurface mainSurface{ initial_editor_main_surface(workspaceTab) };
            // One UI transition requested by an admitted workflow. Consumed
            // once on the frame owner, never inferred from a running task.
            std::optional<EditorMainSurface> requestedMainSurface{};
            float bottomGridSplit{ 0.32f };
            float bottomGridDragOffset{};
            float outlinerSplit{ 0.20f };
            float inspectorSplit{ 0.33f };
            float dockSplit{ 0.28f };
            std::array<float, kEditorMainSurfaceCount> workspaceBottomGridSplits{
                filled_workspace_values(0.32f) };
            std::array<float, kEditorMainSurfaceCount> workspaceDockSplits{
                filled_workspace_values(0.28f) };
            std::array<bool, kEditorMainSurfaceCount> workspaceOutputFollow{
                filled_workspace_values(true) };
            std::array<bool, kEditorMainSurfaceCount> workspaceAiChatFollow{
                filled_workspace_values(true) };
            EditorMainSurface dockPreferenceSurface{
                initial_editor_main_surface(initial_editor_workspace_tab()) };
            bool outputFollowTail{ true };
            // Detached chat uses immutable, rate-limited stream disclosure snapshots.
            std::shared_ptr<const std::vector<gui::ChatMessageRunInfo>>
                detachedChatRunSnapshot{};
            std::chrono::steady_clock::time_point detachedChatRunSnapshotAt{};
            std::uint64_t detachedChatRawSequence{};
            std::uint64_t detachedChatCompletionGeneration{};
            bool aiChatFollowTail{ true };
            unsigned screenshotCaptureAfterFrames{}; // defer until menu is visibly closed
            bool printScreenHeld{}; // edge detection; never capture another app
            bool f12Held{};
            // Detached chat snapshot state uses cross-context immutable data;
            // snapshot. Reuse it while the stream is unchanged; never copy
            // 256 large token records on every editor frame.
            bool aiSetupVisible{ false };
            std::uint64_t outputScrollToEndGeneration{};
            std::uint64_t aiChatScrollToEndGeneration{};
            bool showOutliner{ true };
            bool showInspector{ true };
            bool showConsoleDock{ true };
            bool showAiChat{ true };
            std::string detachedAiChatInput{};
            std::array<bool, kEditorToolPaneCount> toolPaneOpen{
                default_tool_pane_open_state() };
            std::array<EditorToolDockRegion, kEditorToolPaneCount>
                toolPaneDockRegions{ default_tool_pane_regions() };
            std::array<std::uint32_t, kEditorToolPaneCount> toolPaneTabOrder{
                default_tool_pane_tab_order() };
            std::string activeLeftPaneRoute{ "pane.world_outliner" };
            std::string activeRightPaneRoute{ "pane.ai_chat" };
            std::string activeBottomLeftPaneRoute{ "pane.output" };
            std::string activeBottomRightPaneRoute{ "pane.ai_development" };
            bool layoutPreferencesLoaded{ false };
            EditorApplicationKind layoutPreferencesApplication{
                EditorApplicationKind::Standard };
            std::string layoutPreferencesSnapshot{};
            std::chrono::steady_clock::time_point layoutPreferencesNextCheck{};
            bool layoutPreferencesSaveErrorLogged{ false };
            EditorLayoutDrag layoutDrag{ EditorLayoutDrag::None };
            OutlinerToolTab outlinerToolTab{ OutlinerToolTab::World };
            // The document workspace is independent of whichever tool pane
            // happened to render last (including detached/custom dock groups).
            bool game2DGuiWorkspace{};
            bool workspaceCompanionsPending{};
            InspectorToolTab inspectorToolTab{ InspectorToolTab::Properties };
            bool paneTitleDragActive{ false };
            std::string paneTitleDragRoute{};
            std::string paneTitleDragLabel{};
            gui::Vec2 paneTitleDragStart{};
            int surfaceSettleFrames{ 0 };
            gui::Vec2 lastLayoutExtent{};
            gui::DockableWindowHostState dockableGuiHost{};
            gui::DockableWindowState floatingGuiWindow{
                .id = 1,
                .mode = gui::DockableWindowMode::floating,
                .dock_slot = gui::DockSlot::right,
                .floating = { .open = false },
                .visible = false
            };
            std::string detachedPanelHostStatus{ "Pane popouts are routed context panels; whole-editor backend selection stays in Settings." };
            std::string contextSelectionStatus{ "Context follows the active editor window." };
            std::string passiveContextScoreStatus{ "Passive context scoring is waiting for single-context editor timing." };
            std::string passiveContextRecommendation{ "No recommended editor context yet." };
            double passiveContextLastFrameMs{ 0.0 };
            bool passiveContextHasLastFrame{ false };
            bool projectNotesVisible{ false };
            bool showAboutModal{ false };
            bool showSettingsModal{ false };
            bool showPackageManagerModal{ false };
            bool showAiModelManagerModal{ false };
            bool aiModelManagerOpenLogged{}; // Only log lifecycle transitions.
            bool aiModelManagerAutoRescanPending{};
            bool aiChatModelPickerOpen{}; // One read-only scan per opening.
            // Only read-only model INVENTORY discovery is automatic. We never
            // start GGUF inference or restore a saved set without an operator action.
            unsigned aiNativeAutoScanDelayFrames{ 4u };
            bool aiNativeAutoScanAttempted{};
            int aiModelManagerTab{}; // Native / Local API / Direct GGUF
            int aiModelManagerSection{}; // Runtime / System 1 / Routing / Training / Tools
            std::string aiModelPendingUnregister{}; // Inline confirmation; no weight deletion
            std::string aiDirectExecutableDraft{};
            std::string aiDirectModelDraft{};
            std::string aiModelManagerFeedback{};
            std::unordered_map<std::string, unsigned> aiNativeOffloadDraft{};
            std::unordered_map<std::string, epochengine::ai::native_runtime::ModelTuning> aiNativeTuningDraft{};
            bool aiNativeAdvancedTuningOpen{false};
            std::string aiNativeTestPrompt{
                "Explain in two short sentences how a C++23 game engine safely shares GPU memory between rendering and local AI inference."};
            std::array<std::string, 3> aiNativeLoadSetIds{};
            bool showAiModelConsentModal{ false };
            bool showAiModelSettings{ false };
            std::string aiEndpointDraft{ "http://127.0.0.1:1234/v1" };
            std::string aiContextCapacityDraft{};
            std::string aiContextCapacityIdentity{};
            bool showAiEndpointRoutes{};
            std::string aiEndpointStatus{};
            bool showVoiceConsentModal{ false };
            std::string selectedPackageId{};
            bool packageCatalogModelsOnly{};
            bool packageTechnicalDetailsOpen{};
            std::string packageCatalogSearch{};
            std::string packageInstallStatus{ "The Site package catalog has not loaded." };
            float packageInstallProgress{ 0.0f };
            std::vector<package_catalog::Entry> packageCatalogEntries{};
            std::string packageCatalogRevision{};
            std::string packageCatalogStatus{ "Catalog not requested." };
            std::optional<std::future<package_catalog::Snapshot>>
                packageCatalogPending{};
            std::optional<EditorModelInstallState> packageModelInstall{};
            bool packageCatalogRequested{};
            EditorUpdateState updateState{ EditorUpdateState::Idle };
            std::string updateStatus{ "Updates have not been checked." };
            std::optional<std::future<updater::UpdateCommandResult>> updateCheckPending{};
            updater::UpdateCommandResult lastUpdateCheck{};
            bool autoUpdateCheckQueued{ true };
            bool updateCheckPassive{ true };
            bool showUpdateConfirmModal{ false };
            bool showSourceUpdateConfirmModal{ false };
            gui::Vec2 updateConfirmModalStableSize{};
            gui::Vec2 updateConfirmModalStableViewport{};
            gui::Vec2 sourceUpdateConfirmModalStableSize{};
            gui::Vec2 sourceUpdateConfirmModalStableViewport{};
            bool updateInstallPending{ false };
            bool updateSourceInstallPending{ false };
            bool updateSourceCancelRequested{ false };
            bool updateProjectSourceDownloadPending{ false };
            std::chrono::steady_clock::time_point updateOperationStartedAt{};
            std::chrono::steady_clock::time_point updateRetryAfter{};
            bool updateRestartCountdownArmed{ false };
            std::chrono::steady_clock::time_point updateRestartCountdownStartedAt{};
            EditorAutomationCommand automationCommand{ EditorAutomationCommand::None };
            bool automationConsumed{ false };
            std::uint8_t candidateLabSmokePhase{};
            std::uint8_t selfCodingSmokePhase{};
            std::uint8_t selfCodingSmokeWarmupFrames{};
            std::chrono::steady_clock::time_point selfCodingSmokeStartedAt{};
            bool selfCodingWorkingIndicatorObserved{};
            std::uint64_t selfCodingLastHeartbeatSecond{};
            std::string selfCodingLastStatus{};
            bool selfCodingSmokeChooseCandidate{};
            std::string selfCodingSmokeBaselineRoot{};
            std::string selfCodingSmokeCandidateRoot{};
            std::string selfCodingSmokeSelectedParentRoot{};
            std::string selfCodingSmokeSuccessorRoot{};
            std::uint32_t selfCodingSmokeSuccessorGeneration{};
            std::uint64_t selfCodingSmokeFirstCandidateProcessId{};
            std::uint64_t selfCodingSmokeChosenProcessId{};
            bool selfCodingSmokeSuccessorMaterialized{};
            bool selfCodingSmokeSuccessorMaterializationFailed{};
            bool selfCodingSmokeSuccessorPlanRequested{};
            bool selfCodingSmokeSuccessorPlanReviewed{};
            bool selfCodingSmokeSuccessorPreviewRequested{};
            bool selfCodingSmokeOutcomePassed{};
            std::string selfCodingSmokeOutcomeDetail{};
            std::chrono::steady_clock::time_point selfCodingSmokeRetirementStartedAt{};
            SystemsSurfaceState systems{};
            std::unique_ptr<editor_tasks::Scheduler> taskScheduler{};
            AiWorkspaceDomain aiWorkspaceDomain{ AiWorkspaceDomain::Engine };
            EditorMainSurface aiAuthoringSurface{ EditorMainSurface::Scene };
            bool aiAuthoringSurfaceRestoreQueued{};
            bool aiModelInventoryScanned{ false };
            std::optional<std::future<std::vector<std::string>>>
                aiModelInventoryPending{};
            // Direct GGUF probing and provider activation can touch the file
            // system and model lifecycle. Keep both away from the GUI thread.
            std::optional<std::future<std::string>> aiDirectRuntimePending{};
            editor_tasks::CancellationTicket aiDirectRuntimeCancellation{};
            std::string aiPendingModelSelection{};
            // The operator can stage one chat turn before any provider is ready.
            // A queued turn pins the chosen transport, not a different model.
            int aiChatQueuedMethodSelection{-1};
            std::string aiChatQueuedModel{};
            // Only explicit operator selection changes invalidate queued turns.
            // Runtime residency/automatic role routing must not mutate this epoch.
            std::uint64_t aiChatUserModelSelectionRevision{};
            std::uint64_t aiChatQueuedUserModelSelectionRevision{};
            std::chrono::steady_clock::time_point aiLocalModelConfirmedAt{};
            std::string aiDeferredPrompt{};
            std::string aiDeferredDisplay{};
            AiDeferredRequestKind aiDeferredRequestKind{ AiDeferredRequestKind::None };
            std::shared_ptr<AiModelSelectionLease> aiDeferredModelLease{};
            bool aiDeferredDispatchQueued{};
            std::string aiModelConsentStatus{
                "Local model discovery has not been confirmed for use."};
            std::string aiAuthoringRequest{
                "Create a useful starter scene with a ground, light, and one object."};
            std::string aiToolRequest{
                "Inspect the active project and report its current evidence."};
            std::string aiGoal{};
            std::string aiGoalDraft{};
            epochengine::ai::voice::Session aiVoiceSession{};
            epochengine::ai::AuthoringPlanParseResult aiAuthoringPlan{};
            bool aiProjectScriptRequested{};
            std::string aiProjectScriptRoot{}, aiProjectScriptPath{}, aiProjectScriptPreimage{};
            std::string aiProjectScriptDraft{};
            bool aiProjectScriptExisted{};
            std::uint64_t aiProjectScriptSceneRevision{};
            // A worker owns only a detached authoring snapshot, never this
            // live context or its renderer. Publication is revision guarded.
            std::optional<std::future<std::unique_ptr<EditorState>>> aiAuthoringApplyPending{};
            editor_tasks::CancellationTicket aiAuthoringApplyCancellation{};
            std::shared_ptr<AiAuthoringApplyProgress> aiAuthoringApplyProgress{};
            std::uint64_t aiAuthoringApplySceneRevision{};
            std::uint64_t aiAuthoringApplyGuiRevision{};
            bool aiAuthoringApplyCancelled{};
            epochengine::ai::ToolPlanParseResult aiToolPlan{};
            std::string aiAuthoringStatus{
                "Describe scene or GUI objects for the selected local model to plan."};
            std::string aiToolStatus{
                "Use /tool with one active-project operation to request a guarded tool plan."};
            std::uint64_t aiAuthoringRequestedGeneration{};
            std::uint64_t aiToolRequestedGeneration{};
            std::uint64_t aiObservedCompletionGeneration{};
            std::uint64_t aiAuthoringIgnoredThroughGeneration{};
            bool aiToolAwaitingReply{};
            bool aiToolPlanApplied{};
            std::optional<std::future<EditorProjectBuildResult>>
                aiToolTestPending{};
            editor_tasks::CancellationTicket aiToolTestCancellation{};
            std::string aiToolTestProjectId{};
            bool aiToolTestAfterBuild{};
            std::size_t aiGoalCompletedMilestones{};
            std::vector<std::string> aiGoalAttemptedMilestoneSignatures{};
            std::string aiGoalLastMilestoneSignature{};
            std::string aiGoalLastMilestoneReceipt{};
            bool aiGoalActive{};
            bool aiGoalRunning{};
            bool aiGoalEditing{};
            bool aiGoalPlanNextQueued{};
            bool aiIntentClarificationPending{};
            AiChatIntent aiIntentClarificationResumeIntent{AiChatIntent::Ambiguous};
            std::string aiIntentClarificationRequest{};
            std::string aiIntentClarificationQuestion{};
            std::string aiProjectClarificationObjective{};
            std::vector<std::pair<std::string, std::string>> aiProjectClarifications{};
            std::optional<std::size_t> aiQueuedMessageActionIndex{};
            bool aiAuthoringAwaitingReply{};
            bool aiAuthoringPlanApplied{};
            bool aiAuthoringPlanForGoal{};
            bool aiContinuousBuildEnabled{ false };
            std::optional<std::future<EditorProjectBuildResult>> aiContinuousBuildPending{};
            std::string aiContinuousBuildFingerprint{};
            std::string aiContinuousBuildStatus{ "Manual OS AI evidence gate is idle." };
            std::size_t aiContinuousBuildRunCount{ 0 };
            bool aiContinuousBuildLastSucceeded{};
            std::string aiToolHarnessStatus{ "AI tool harness has not run yet." };
            std::size_t aiToolHarnessRunCount{ 0 };
            // Bounded evidence futures retain worker ownership during shutdown;
            // no capture is initiated from a crash handler or candidate teardown.
            std::vector<std::future<void>> aiDiagnosticCaptures{};
            std::unordered_set<std::string> aiDiagnosticCaptureKeys{};
            std::unordered_map<std::string, unsigned> aiDiagnosticCaptureCycles{};
            std::string aiDiagnosticWorkspaceRoot{};
            std::string aiDiagnosticEngineRoot{};
            bool aiDiagnosticWorkspaceReady{false};
            bool aiDiagnosticScreenshotsEnabled{false};
            std::chrono::steady_clock::time_point aiLastManualDiagnosticsExport{};
            std::string aiLastAutoDiagnosticsFailure{};
            // The exact host action is retained while resources settle. A
            // generation change cancels it; polling never reconstructs work.
            // The operator elects to use available resources without a RAM/CPU
            // permission or cooldown. Ownership and candidate-choice barriers
            // remain in the same controller.
            platform::work_admission::Controller aiWorkAdmission{
                platform::work_admission::Policy{.cooldown_ms = 0u, .resource_checks = false}};
            platform::work_admission::ModelMemoryApproval aiModelMemoryApproval{};
            platform::work_admission::ModelMemoryScope aiQueuedModelMemoryScope{};
            int aiMethodSelection{-1};
            int aiSetupTab{};
            bool aiAutomaticModelSelection{};
            bool aiAdvancedSetup{};
            std::string aiRegisteredModelId{};
            std::string aiRegisteredModelPath{};
            int aiTrainingScope{};
            bool aiTrainingRequireCuda{};
            int aiTrainingSteps{1};
            bool aiTrainingPauseRequested{};
            bool aiNativeInventoryRequested{};
            bool aiSuggestionScanStarted{};
            std::optional<std::future<std::vector<epochengine::ai::LocalApiModelSuggestion>>>
                aiSuggestionScanPending{};
            std::vector<epochengine::ai::LocalApiModelSuggestion> aiSuggestedModels{};
            std::size_t aiSelectedSuggestion{};
            std::string aiSuggestionScanStatus{};
            std::uint64_t aiVoiceTranscriptSequence{};
            bool aiVoiceConversation{};
            std::size_t aiVoiceSpokenLines{};
            bool aiWorkLocalHttp{};
            std::uint64_t aiWorkArtifactEpoch{};
            std::uint64_t aiWorkToken{};
            std::uint64_t aiWorkLease{};
            bool aiWorkLeaseIsSource{true};
            bool aiWorkRetirementBarrier{};
            std::uint64_t aiWorkObservedFinish{};
            platform::work_admission::Reason aiWorkReason{
                platform::work_admission::Reason::idle};
            std::string aiWorkStatus{};
            std::optional<std::future<EditorScriptBuildResult>> aiGuardedHarnessPending{};
            std::string aiGuardedHarnessBefore{};
        };

