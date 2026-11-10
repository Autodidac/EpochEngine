// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] bool ai_project_package_request_current(const EditorState& editor) noexcept;
        void finish_ai_project_package_request(EditorState& editor, std::string status, bool succeeded = false);
        [[nodiscard]] bool ai_package_generation_awaiting_attachment(const EditorState& editor) noexcept;
        void continue_ai_project_package_after_attachment(EditorState& editor);
        [[nodiscard]] bool ai_project_package_generation_current(const EditorState& editor) noexcept;

        [[nodiscard]] static SeedObjectSummary summarize_seed_objects(std::span<const EditorSceneSeedEntity> seeds)
        {
            SeedObjectSummary summary{};
            summary.total = seeds.size();

            auto append_unique = [](std::vector<std::string>& values, std::string_view value)
            {
                if (value.empty())
                    return;

                const std::string owned{ value };
                if (std::find(values.begin(), values.end(), owned) == values.end())
                    values.push_back(owned);
            };

            for (const auto& seed : seeds)
            {
                if (seed.category == "World")
                    ++summary.worldCount;
                else if (seed.category == "Gameplay")
                    ++summary.gameplayCount;
                else if (seed.category == "Editor")
                    ++summary.editorCount;

                if (seed.visible)
                    ++summary.visibleCount;
                if (seed.editor_only)
                    ++summary.editorOnlyCount;

                append_unique(summary.types, seed.type);
                append_unique(summary.categories, seed.category);
            }

            return summary;
        }

        [[nodiscard]] static std::string summarize_seed_category_mix(const SeedObjectSummary& summary)
        {
            return "World "
                + std::to_string(summary.worldCount)
                + " | Gameplay "
                + std::to_string(summary.gameplayCount)
                + " | Editor "
                + std::to_string(summary.editorCount);
        }

        [[nodiscard]] static std::string summarize_seed_type_list(const std::vector<std::string>& values)
        {
            if (values.empty())
                return "None";

            std::string result;
            const std::size_t count = (std::min)(values.size(), std::size_t{ 5 });
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!result.empty())
                    result += " | ";
                result += values[i];
            }
            if (values.size() > count)
                result += " | ...";
            return result;
        }

        [[nodiscard]] bool path_exists(const std::filesystem::path& path) noexcept
        {
            if (path.empty())
                return false;
            std::error_code ec;
            return std::filesystem::exists(resolve_editor_path(path), ec);
        }

        void repair_project_evidence(EditorState& editor, std::string_view projectId)
        {
            if (projectId.empty())
            {
                editor.projectStatus =
                    "No project is open. Open a project.epoch.json manifest before saving.";
                push_editor_log(editor, "[project] Save blocked: no project is open.");
                return;
            }
            const std::string requestedProject{projectId};
            const std::string previousProject = editor.projectId;
            if (project_gui_document_is_active(editor))
                (void)synchronize_gui_document(editor);
            const std::optional<authoring::gui::GuiDocument>
                preservedGuiDocument =
                    editor.guiDocument && editor.guiDocument->valid()
                    ? std::optional<authoring::gui::GuiDocument>{
                        *editor.guiDocument}
                    : std::nullopt;
            // Shell refresh reloads disk state. Retain the authoritative owner,
            // including semantic history, rather than only its visible vector.
            const auto preservedSceneDocument = editor.sceneDocument;
            const auto preservedSceneRevision = editor.sceneDocumentRevision;
            const auto preservedProjectRoot = editor.projectRoot;
            const auto preservedScenePath = editor.projectScenePath;
            const auto preservedGuiWidgets =
                editor.guiDocumentWidgets;
            const auto preservedGuiTombstones = editor.guiProjectionTombstones;
            const auto preservedGuiSelectedWidget =
                gui_widget_identity_matches(editor, editor.guiSelectedWidget,
                    editor.guiSelectedWidget.widget)
                ? editor.guiSelectedWidget.widget : authoring::gui::WidgetHandle{};
            const auto preservedGuiContentWidget =
                gui_widget_identity_matches(editor, editor.guiContentEditorWidget,
                    editor.guiContentEditorWidget.widget)
                ? editor.guiContentEditorWidget.widget : authoring::gui::WidgetHandle{};
            const auto preservedGuiDocumentTab = editor.guiDocumentTab;
            const auto preservedGuiCanvasView = editor.guiCanvasView;
            const auto preservedGuiContentPrimary = editor.guiContentPrimary;
            const auto preservedGuiContentSecondary = editor.guiContentSecondary;
            const auto preservedGuiAction = editor.guiActionEditor;
            const auto preservedGuiTooltip = editor.guiTooltipEditor;
            const std::uint64_t preservedGuiSavedRevision =
                editor.guiDocumentSavedRevision;
            const std::vector<EditorEntity> preservedEntities = editor.entities;
            const std::size_t preservedSelected = editor.selectedEntity;
            const auto preservedSelectedId = editor.selectedEntityId;
            const auto preservedSelectedIds = editor.selectedEntityIds;
            const auto preservedPreviewMode = editor.previewMode;
            const auto preservedWorkspaceTab = editor.workspaceTab;
            const auto preservedProjectWorkspaceSection = editor.projectWorkspaceSection;
            const auto preservedDockStatusTab = editor.dockStatusTab;
            const auto preservedMainSurface = editor.mainSurface;
            const auto preservedForestTerrainPainting = editor.forestTerrainPainting;
            const auto preservedCanvas2DProject = editor.canvas2dProject;
            const auto preservedProjectCameraMode = editor.projectCameraMode;
            const auto preservedViewportCameraMode = editor.viewportCameraMode;
            const auto preservedGuiWorkspace = editor.game2DGuiWorkspace;
            const auto preservedOutlinerTab = editor.outlinerToolTab;
            const auto preservedInspectorTab = editor.inspectorToolTab;
            const auto preservedCompanionsPending = editor.workspaceCompanionsPending;
            const auto preservedShowOutliner = editor.showOutliner;
            const auto preservedShowInspector = editor.showInspector;
            const auto preservedShowConsole = editor.showConsoleDock;
            const auto preservedShowAssistant = editor.showAiChat;
            const auto preservedHelpers = editor.helpersVisible;
            const auto preservedAiSurface = editor.aiAuthoringSurface;
            const auto preservedAiSurfaceRestore = editor.aiAuthoringSurfaceRestoreQueued;
            const auto preservedInputProfile = editor.inputProfilePreset;
            const std::string preservedRunBackend = editor.projectRunBackend;
            const double preservedRunFrameLimit = editor.projectRunFrameLimitFps;
            const auto preservedProjectFrameLimitEdit = editor.projectFrameLimitEdit;
            const auto preservedPlanetarySettings = editor.planetarySettings;
            const auto preservedPlanetarySeedDraft = editor.planetarySeedDraft;
            const auto preservedPlanetaryRadiusDraft = editor.planetaryRadiusDraft;
            const auto preservedPlanetaryRadiusSynced = editor.planetaryRadiusSynced;
            const auto preservedPlanetaryRadiusReferenceSynced = editor.planetaryRadiusReferenceSynced;
            const auto preservedPlanetaryCheckedRoot = editor.planetaryCheckedRoot;
            const auto preservedPlanetaryAttachment = editor.planetaryAttachment;
            const auto preservedPlanetaryMeshPolicy = editor.planetaryMeshPolicy;
            const auto preservedPlanetaryBodySelection = editor.planetaryBodySelection;
            const auto preservedPlanetaryInitialFramePending = editor.planetaryInitialFramePending
                && editor.planetaryInitialFrameGeneration
                    == editor.projectLifecycle.evidence().generations.selected_project.generation;
            // Refreshing the same project's shell is not a goal cancellation.
            // Retain progress only for its exact current selection; approvals
            // and pending model requests deliberately remain reset below.
            const bool preservedToolGoalCurrent = editor.aiGoalActive && editor.aiGoalRunning
                && editor.aiGoalTools.enabled && !editor.aiGoalTools.stopped
                && !editor.aiGoalTools.operation().empty()
                && !editor.projectId.empty() && !editor.projectRoot.empty()
                && editor.aiGoalTools.project_id == editor.projectId
                && editor.aiGoalTools.project_root == editor.projectRoot
                && editor.aiGoalTools.project_generation
                    == editor.projectLifecycle.evidence().generations.selected_project.generation;
            const auto preservedToolGoal = editor.aiGoal;
            const auto preservedToolSequence = editor.aiGoalTools;
            const auto preservedToolGoalNextQueued = editor.aiGoalPlanNextQueued;
            const auto preservedToolGoalReceipt = editor.aiGoalLastMilestoneReceipt;
            const bool isSandbox = requestedProject == "sandbox";
            const auto ensured = editor_ensure_project_shell(requestedProject);
            editor.projectStatus = ensured.summary;
            editor.aiContinuousBuildFingerprint.clear();
            if (ensured.succeeded)
            {
                const std::string targetProject = ensured.project_id.empty() ? requestedProject : ensured.project_id;
                set_project(editor, targetProject, true);
                if (!editor.projectLifecycle.record_shell_materialized())
                {
                    editor.projectStatus = "Project shell was materialized, but its lifecycle generation could not be recorded.";
                    push_editor_log(editor, "[project] " + editor.projectStatus);
                    return;
                }
                if (targetProject == previousProject
                    && editor.projectRoot == preservedProjectRoot
                    && editor.projectScenePath == preservedScenePath)
                {
                    editor.previewMode = preservedPreviewMode;
                    editor.workspaceTab = preservedWorkspaceTab;
                    editor.projectWorkspaceSection = preservedProjectWorkspaceSection;
                    editor.dockStatusTab = preservedDockStatusTab;
                    editor.mainSurface = preservedMainSurface;
                    editor.forestTerrainPainting = preservedForestTerrainPainting;
                    editor.canvas2dProject = preservedCanvas2DProject;
                    editor.projectCameraMode = preservedProjectCameraMode;
                    // Saving refreshes project metadata; it is not an explicit
                    // workspace choice. Do not queue default companion panes or
                    // replace the user's GUI view with the application's start view.
                    editor.viewportCameraMode = preservedViewportCameraMode;
                    editor.game2DGuiWorkspace = preservedGuiWorkspace;
                    editor.outlinerToolTab = preservedOutlinerTab;
                    editor.inspectorToolTab = preservedInspectorTab;
                    editor.workspaceCompanionsPending = preservedCompanionsPending;
                    editor.showOutliner = preservedShowOutliner;
                    editor.showInspector = preservedShowInspector;
                    editor.showConsoleDock = preservedShowConsole;
                    editor.showAiChat = preservedShowAssistant;
                    editor.helpersVisible = preservedHelpers;
                    editor.aiAuthoringSurface = preservedAiSurface;
                    editor.aiAuthoringSurfaceRestoreQueued = preservedAiSurfaceRestore;
                    editor.inputProfilePreset = preservedInputProfile;
                    editor.projectRunBackend = preservedRunBackend;
                    editor.projectRunFrameLimitFps = preservedRunFrameLimit;
                    editor.projectFrameLimitEdit = preservedProjectFrameLimitEdit;
                    editor.planetarySettings = preservedPlanetarySettings;
                    editor.planetarySeedDraft = preservedPlanetarySeedDraft;
                    editor.planetaryRadiusDraft = preservedPlanetaryRadiusDraft;
                    editor.planetaryRadiusSynced = preservedPlanetaryRadiusSynced;
                    editor.planetaryRadiusReferenceSynced = preservedPlanetaryRadiusReferenceSynced;
                    editor.planetaryCheckedRoot = preservedPlanetaryCheckedRoot;
                    editor.planetaryAttachment = preservedPlanetaryAttachment;
                    editor.planetaryMeshPolicy = preservedPlanetaryMeshPolicy;
                    editor.planetaryBodySelection = preservedPlanetaryBodySelection;
                    // Reloading metadata for Save is not another initial open.
                    // A new orbit request would replace the context-owned view
                    // and its precise navigation origin on the next frame.
                    editor.planetaryInitialFramePending = preservedPlanetaryInitialFramePending;
                    editor.planetaryInitialFrameGeneration = preservedPlanetaryInitialFramePending
                        ? editor.projectLifecycle.evidence().generations.selected_project.generation : 0u;
                    if (preservedToolGoalCurrent
                        && editor.projectLifecycle.evidence().generations.selected_project.generation
                            == preservedToolSequence.project_generation)
                    {
                        editor.aiGoal = preservedToolGoal;
                        editor.aiGoalDraft = preservedToolGoal;
                        editor.aiGoalTools = preservedToolSequence;
                        editor.aiGoalActive = editor.aiGoalRunning = true;
                        editor.aiGoalCompletedMilestones = preservedToolSequence.stage;
                        editor.aiGoalPlanNextQueued = preservedToolGoalNextQueued;
                        editor.aiGoalLastMilestoneReceipt = preservedToolGoalReceipt;
                    }
                    input::set_active_profile(editor.inputProfilePreset);

                    editor.sceneDocument = preservedSceneDocument;
                    editor.sceneDocumentRevision = preservedSceneRevision;
                    editor.entities = preservedEntities;
                    editor.selectedEntity = preservedSelected;
                    editor.selectedEntityId = preservedSelectedId;
                    editor.selectedEntityIds = preservedSelectedIds;
                    synchronize_editor_selection(editor);
                    editor.sceneInteractionDirty = editor.sceneLightingDirty = true;
                    if (preservedGuiDocument)
                    {
                        try
                        {
                            auto restored = std::make_unique<
                                authoring::gui::GuiDocument>(
                                    *preservedGuiDocument);
                            if (restored->valid()
                                && !restored->validate()
                                && restored->roots().size() == 1u)
                            {
                                editor.guiDocument = std::move(restored);
                                editor.guiDocumentRoot =
                                    editor.guiDocument->roots().front();
                                editor.guiDocumentWidgets =
                                    preservedGuiWidgets;
                                editor.guiProjectionTombstones = preservedGuiTombstones;
                                editor.guiDocumentSavedRevision =
                                    preservedGuiSavedRevision;
                                editor.guiDocumentLoadAttempted = true;
                                editor.guiDocumentSourcePath =
                                    display_project_path(
                                        project_gui_document_path(editor));
                                editor.guiDocumentTab = preservedGuiDocumentTab;
                                editor.guiCanvasView = preservedGuiCanvasView;
                                editor.guiCanvasView.drag = {};
                                if (editor.guiDocument->widget(preservedGuiSelectedWidget))
                                    editor.guiSelectedWidget = gui_widget_identity(
                                        editor, preservedGuiSelectedWidget);
                                if (editor.guiDocument->widget(preservedGuiContentWidget))
                                {
                                    editor.guiContentEditorWidget = gui_widget_identity(
                                        editor, preservedGuiContentWidget);
                                    editor.guiContentPrimary = preservedGuiContentPrimary;
                                    editor.guiContentSecondary = preservedGuiContentSecondary;
                                    editor.guiActionEditor = preservedGuiAction;
                                    editor.guiTooltipEditor = preservedGuiTooltip;
                                }
                            }
                        }
                        catch (...)
                        {
                            editor.guiDocumentStatus =
                                "GUI document could not be restored after project materialization.";
                        }
                    }
                }
                (void)project_gui_document_layout(editor);
                if (!save_project_gui_document(editor, false)
                    || !save_editor_scene_snapshot(editor))
                {
                    editor.projectStatus =
                        "Project save blocked because GUI and scene source evidence were not both committed.";
                    push_editor_log(
                        editor,
                        "[project] " + editor.projectStatus);
                    return;
                }
                if (!persist_project_run_settings(editor)) return;
                editor.projectStatus = ensured.summary + " Active project saved.";
                editor.aiContinuousBuildStatus = isSandbox
                    ? "Sandbox evidence saved; queue a engine development build to stage verifier evidence."
                    : "Project evidence saved; queue a build/run or explicit engine development handoff.";
                push_editor_log(editor, "[project] Saved active project evidence: " + ensured.summary);
                append_project_note(
                    editor,
                    isSandbox ? "Save Engine Development Sandbox Evidence" : "Save Active Project",
                    ensured.summary,
                    isSandbox
                        ? "Sandbox evidence is ready for a controlled engine engine development build pass."
                        : "Project evidence is ready for a manual run/build or an explicit AI handoff.");
            }
            else
            {
                editor.aiContinuousBuildStatus = "Project evidence save failed; inspect project status.";
                push_editor_log(editor, "[project] Save active project evidence failed: " + ensured.summary);
            }
        }

        void repair_active_project_evidence(EditorState& editor)
        {
            repair_project_evidence(editor, editor.projectId);
        }

        [[nodiscard]] std::string editor_json_escape(std::string_view text)
        {
            std::string escaped;
            escaped.reserve(text.size() + 8u);
            for (const char ch : text)
            {
                switch (ch)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20u)
                        escaped += ' ';
                    else
                        escaped += ch;
                    break;
                }
            }
            return escaped;
        }

        [[nodiscard]] std::string safe_package_artifact_id(std::string_view id)
        {
            std::string safe;
            safe.reserve(id.size());
            for (const char ch : id)
            {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.')
                    safe += ch;
                else
                    safe += '_';
            }
            return safe.empty() ? std::string{ "package" } : safe;
        }

        [[nodiscard]] std::string read_bounded_package_file(
            const std::filesystem::path& path,
            const std::size_t maximumBytes)
        {
            std::error_code error{};
            if (!std::filesystem::is_regular_file(path, error) || error
                || std::filesystem::is_symlink(path, error) || error)
                return {};
            const std::uintmax_t bytes = std::filesystem::file_size(path, error);
            if (error || bytes == 0u || bytes > maximumBytes)
                return {};
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return {};
            std::string value(static_cast<std::size_t>(bytes), '\0');
            input.read(value.data(), static_cast<std::streamsize>(value.size()));
            return input && input.peek() == std::char_traits<char>::eof()
                ? value : std::string{};
        }

        [[nodiscard]] std::filesystem::path model_cache_root_path()
        {
            return resolve_editor_path(std::filesystem::path{
                epochengine::ai::local_model_root()});
        }

        [[nodiscard]] bool model_snapshot_installed(
            const ai::model_install::InstallPlan& plan)
        {
            const std::filesystem::path root =
                ai::model_install::version_root(model_cache_root_path(), plan);
            if (root.empty()
                || !ai::model_install::receipt_matches(
                    plan,
                    read_bounded_package_file(
                        root / ai::model_install::receipt_filename,
                        128u * 1024u)))
                return false;

            std::error_code error{};
            for (const ai::model_install::ArtifactSpec& artifact : plan.artifacts)
            {
                const std::filesystem::path path = root / artifact.file;
                if (!std::filesystem::is_regular_file(path, error) || error
                    || std::filesystem::is_symlink(path, error) || error
                    || std::filesystem::file_size(path, error) != artifact.bytes
                    || error)
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::optional<std::filesystem::path>
            find_model_transfer_executable()
        {
#if defined(_WIN32)
            constexpr std::string_view executableName = "curl.exe";
            constexpr char separator = ';';
#else
            constexpr std::string_view executableName = "curl";
            constexpr char separator = ':';
#endif
            const auto pathValue = core::env::get("PATH");
            if (!pathValue)
                return std::nullopt;
            const std::string pathsStorage{
                pathValue->data(), pathValue->size()};
            const std::string_view paths{pathsStorage};
            std::size_t first = 0u;
            while (first <= paths.size())
            {
                const std::size_t end = paths.find(separator, first);
                std::string_view item = paths.substr(
                    first,
                    end == std::string_view::npos
                        ? std::string_view::npos : end - first);
                if (item.size() >= 2u && item.front() == '"'
                    && item.back() == '"')
                {
                    item.remove_prefix(1u);
                    item.remove_suffix(1u);
                }
                if (!item.empty())
                {
                    const std::filesystem::path candidate =
                        std::filesystem::path{std::string{item}}
                        / std::string{executableName};
                    std::error_code error{};
                    if (std::filesystem::is_regular_file(candidate, error)
                        && !error)
                        return std::filesystem::absolute(candidate, error);
                }
                if (end == std::string_view::npos)
                    break;
                first = end + 1u;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::uint64_t model_plan_bytes(
            const ai::model_install::InstallPlan& plan) noexcept
        {
            std::uint64_t total{};
            for (const auto& artifact : plan.artifacts)
            {
                if (artifact.bytes > (std::numeric_limits<std::uint64_t>::max)()
                    - total)
                    return 0u;
                total += artifact.bytes;
            }
            return total;
        }

        [[nodiscard]] bool launch_next_model_artifact(EditorState& editor)
        {
            if (!editor.packageModelInstall)
                return false;
            EditorModelInstallState& install = *editor.packageModelInstall;
            std::error_code error{};
            if (install.artifactIndex < install.plan.artifacts.size())
            {
                std::filesystem::create_directories(
                    install.stagingRoot, error);
                if (error)
                {
                    editor.packageInstallStatus =
                        "Model install failed: the owned staging directory could not be created.";
                    editor.packageInstallProgress = 0.0f;
                    editor.packageModelInstall.reset();
                    return false;
                }
            }

            while (install.artifactIndex < install.plan.artifacts.size())
            {
                const auto& artifact =
                    install.plan.artifacts[install.artifactIndex];
                const std::filesystem::path finalPath =
                    install.stagingRoot / artifact.file;
                error.clear();
                if (std::filesystem::is_regular_file(finalPath, error)
                    && !error
                    && !std::filesystem::is_symlink(finalPath, error)
                    && !error
                    && std::filesystem::file_size(finalPath, error)
                        == artifact.bytes
                    && !error)
                {
                    install.completedBytes += artifact.bytes;
                    ++install.artifactIndex;
                    continue;
                }
                if (std::filesystem::exists(finalPath, error) && !error)
                {
                    std::filesystem::remove(finalPath, error);
                    if (error)
                    {
                        editor.packageInstallStatus =
                            "Model install stopped: an invalid staged artifact could not be replaced.";
                        editor.packageModelInstall.reset();
                        return false;
                    }
                }

                const auto transferTool = find_model_transfer_executable();
                if (!transferTool)
                {
                    editor.packageInstallStatus =
                        "Model install requires the operating system HTTPS transfer tool (curl).";
                    editor.packageInstallProgress = 0.0f;
                    editor.packageModelInstall.reset();
                    return false;
                }
                const std::filesystem::path partialPath =
                    finalPath.string() + ".partial";
                std::filesystem::create_directories(
                    install.outputLog.parent_path(), error);
                if (error)
                {
                    editor.packageInstallStatus =
                        "Model install failed: the transfer evidence directory could not be created.";
                    editor.packageModelInstall.reset();
                    return false;
                }

                platform::child_process::LaunchRequest request{};
                request.executable = *transferTool;
                request.working_directory = install.stagingRoot;
                request.merged_output_path = install.outputLog;
                request.arguments = {
                    "--proto", "=https",
                    "--proto-redir", "=https",
                    "--tlsv1.2",
                    "--fail",
                    "--location",
                    "--retry", "4",
                    "--retry-all-errors",
                    "--continue-at", "-",
                    "--output", partialPath.string(),
                    artifact.source_url};
                request.correlation_key = "epoch.model.install."
                    + install.plan.package_id + "."
                    + std::to_string(install.artifactIndex);
                request.exclusive_group = "epoch.model.install";
                request.display_name = "Epoch model artifact transfer";
                request.window_mode =
                    platform::child_process::WindowMode::hidden;
                request.append_output = true;
                const auto launched =
                    platform::child_process::launch_or_focus(request);
                if (launched.code
                    != platform::child_process::LaunchCode::started)
                {
                    editor.packageInstallStatus =
                        "Model transfer did not start: " + launched.message;
                    editor.packageInstallProgress = install.totalBytes == 0u
                        ? 0.0f
                        : static_cast<float>(install.completedBytes)
                            / static_cast<float>(install.totalBytes);
                    editor.packageModelInstall.reset();
                    return false;
                }
                install.process = launched.handle;
                editor.packageInstallStatus = epochengine::format_text(
                    "Downloading verified model artifact {} of {}: {}",
                    install.artifactIndex + 1u,
                    install.plan.artifacts.size(),
                    artifact.file);
                return true;
            }

            if (!install.publishPending)
            {
                ai::model_install::PublishRequest request{
                    .plan = install.plan,
                    .models_cache_root = install.cacheRoot,
                    .staging_root = install.stagingRoot};
                install.publishPending.emplace(schedule_editor_task(
                    editor,
                    "Verify and publish model add-on",
                    [request = std::move(request)]
                    {
                        return ai::model_install::publish_verified_snapshot(
                            request);
                    }));
                editor.packageInstallStatus =
                    "Verifying exact model bytes and publishing the immutable cache snapshot...";
                editor.packageInstallProgress = 0.99f;
            }
            return true;
        }

        void poll_model_package_install(EditorState& editor)
        {
            if (!editor.packageModelInstall)
                return;
            if (editor.packageModelInstall->publishPending)
            {
                auto& pending =
                    *editor.packageModelInstall->publishPending;
                if (pending.wait_for(std::chrono::milliseconds{0})
                    != std::future_status::ready)
                    return;
                ai::model_install::PublishResult published{};
                try
                {
                    published = pending.get();
                }
                catch (const std::exception& error)
                {
                    published.diagnostic = error.what();
                }
                catch (...)
                {
                    published.diagnostic =
                        "unknown background verification failure";
                }
                if (!published.accepted() && !published.artifact.empty()
                    && !editor.packageModelInstall->stagingRoot.empty())
                {
                    std::error_code removeError{};
                    std::filesystem::remove(
                        editor.packageModelInstall->stagingRoot
                            / published.artifact,
                        removeError);
                }
                const auto& operation = *editor.packageModelInstall;
                if (!operation.registrationOfferId.empty())
                {
                    namespace catalog = epoch_ai::model::catalog;
                    const auto* offer = catalog::find(operation.registrationOfferId);
                    const auto expected = offer && catalog::download_allowed(*offer)
                        ? ai::model_install::plan_for(offer->package_id) : std::nullopt;
                    const auto expectedRoot = expected
                        ? ai::model_install::version_root(operation.cacheRoot, *expected)
                        : std::filesystem::path{};
                    const bool exact = expected && *expected == operation.plan
                        && !expectedRoot.empty() && published.accepted()
                        && published.installed_root == expectedRoot
                        && published.receipt_path == expectedRoot / ai::model_install::receipt_filename;
                    const bool queued = !operation.cancelled && exact
                        && ai::native_runtime::register_model(operation.registrationOfferId,
                            display_project_path(expectedRoot / expected->artifacts.front().file), true);
                    editor.packageInstallStatus = operation.cancelled
                        ? "Registration cancelled. Cached files were left in place; no registration was queued."
                        : queued ? "Verified model registration queued with initial 8K context and one slot. No model was loaded or selected."
                        : "Model registration was not queued: "
                            + (exact ? std::string{"the native queue is busy; retry when idle."}
                                : "snapshot verification or identity failed. " + published.diagnostic);
                    editor.packageInstallProgress = queued ? 1.0f : 0.0f;
                }
                else editor.packageInstallStatus = operation.cancelled
                    ? "Download request cancelled. Verified files may have finished publishing; no model was registered or loaded."
                    : published.accepted()
                    ? "Model add-on installed and verified. Activation remains a separate explicit action."
                    : "Model install failed during verified publication: "
                        + published.diagnostic;
                if (operation.registrationOfferId.empty())
                    editor.packageInstallProgress = !operation.cancelled && published.accepted() ? 1.0f : 0.0f;
                push_editor_log(
                    editor,
                    std::string{"[package] "}
                        + editor.packageInstallStatus);
                editor.packageModelInstall.reset();
                return;
            }
            if (!editor.packageModelInstall->process.valid())
                return;
            platform::child_process::poll();
            EditorModelInstallState& install = *editor.packageModelInstall;
            const auto observed =
                platform::child_process::snapshot(install.process);
            if (!observed)
            {
                editor.packageInstallStatus =
                    "Model transfer evidence disappeared; the partial cache remains resumable.";
                editor.packageModelInstall.reset();
                return;
            }

            const auto& artifact = install.plan.artifacts[install.artifactIndex];
            const std::filesystem::path finalPath =
                install.stagingRoot / artifact.file;
            const std::filesystem::path partialPath =
                finalPath.string() + ".partial";
            if (observed->active())
            {
                std::error_code error{};
                const std::uint64_t partialBytes =
                    std::filesystem::is_regular_file(partialPath, error) && !error
                    ? static_cast<std::uint64_t>(
                        std::filesystem::file_size(partialPath, error))
                    : 0u;
                const std::uint64_t current = install.completedBytes
                    + (std::min)(partialBytes, artifact.bytes);
                editor.packageInstallProgress = install.totalBytes == 0u
                    ? 0.0f
                    : static_cast<float>(current)
                        / static_cast<float>(install.totalBytes);
                editor.packageInstallStatus = epochengine::format_text(
                    "Downloading artifact {} of {}: {} ({} / {} bytes)",
                    install.artifactIndex + 1u,
                    install.plan.artifacts.size(),
                    artifact.file,
                    partialBytes,
                    artifact.bytes);
                return;
            }

            const bool exitedCleanly = observed->exit_code_valid
                && observed->exit_code == 0;
            (void)platform::child_process::release(observed->handle);
            install.process = {};
            if (install.cancelled)
            {
                editor.packageInstallStatus = "Model download cancelled; partial bytes remain resumable. No model was loaded.";
                editor.packageInstallProgress = 0.0f;
                editor.packageModelInstall.reset();
                return;
            }
            if (!exitedCleanly)
            {
                editor.packageInstallStatus =
                    "Model transfer stopped before verification; Install resumes the retained partial file.";
                editor.packageModelInstall.reset();
                return;
            }

            std::error_code error{};
            const bool completeDownload =
                std::filesystem::is_regular_file(partialPath, error)
                && !error
                && !std::filesystem::is_symlink(partialPath, error)
                && !error
                && std::filesystem::file_size(partialPath, error)
                    == artifact.bytes
                && !error;
            if (!completeDownload)
            {
                std::filesystem::remove(partialPath, error);
                editor.packageInstallStatus =
                    "Downloaded model artifact had the wrong type or size; its partial file was rejected.";
                editor.packageInstallProgress = 0.0f;
                editor.packageModelInstall.reset();
                return;
            }
            error.clear();
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    partialPath, finalPath, error))
            {
                editor.packageInstallStatus =
                    "Verified model artifact could not be published into owned staging.";
                editor.packageModelInstall.reset();
                return;
            }
            install.completedBytes += artifact.bytes;
            ++install.artifactIndex;
            (void)launch_next_model_artifact(editor);
        }

        [[nodiscard]] bool start_model_package_install(
            EditorState& editor,
            const package_registry::PackageDescriptor& package,
            bool forceBackgroundVerification = false,
            const std::filesystem::path& cacheOverride = {})
        {
            if (editor.packageModelInstall)
            {
                editor.packageInstallStatus =
                    "A model add-on transfer is already active.";
                return false;
            }
            const auto plan = ai::model_install::plan_for(package.id);
            if (!plan)
            {
                editor.packageInstallStatus =
                    "This build has no compiled install plan for the selected model.";
                return false;
            }
            const std::filesystem::path cache = cacheOverride.empty()
                ? model_cache_root_path() : cacheOverride;
            const std::filesystem::path installed =
                ai::model_install::version_root(cache, *plan);
            std::error_code error{};
            if (std::filesystem::exists(installed, error) && !error)
            {
                if (!forceBackgroundVerification && model_snapshot_installed(*plan))
                {
                    editor.packageInstallStatus =
                        "The exact model add-on is already installed and verified.";
                    editor.packageInstallProgress = 1.0f;
                    return true;
                }
                editor.packageModelInstall.emplace(EditorModelInstallState{
                    .plan = *plan,
                    .cacheRoot = cache,
                    .artifactIndex = plan->artifacts.size(),
                    .totalBytes = model_plan_bytes(*plan),
                    .completedBytes = model_plan_bytes(*plan)});
                editor.packageInstallStatus =
                    "Verifying the existing versioned model snapshot...";
                return launch_next_model_artifact(editor);
            }
            const std::filesystem::path staging =
                ai::model_install::staging_root(cache, *plan, "package-manager");
            if (staging.empty())
            {
                editor.packageInstallStatus =
                    "The model add-on staging path failed local safety validation.";
                return false;
            }
            editor.packageModelInstall.emplace(EditorModelInstallState{
                .plan = *plan,
                .cacheRoot = cache,
                .stagingRoot = staging,
                .outputLog = cache / ".install-logs"
                    / (plan->package_id + ".download.log"),
                .totalBytes = model_plan_bytes(*plan)});
            editor.packageInstallProgress = 0.0f;
            return launch_next_model_artifact(editor);
        }

        [[nodiscard]] bool start_model_offer_install(EditorState& editor, std::string_view offerId)
        {
            const auto* offer = epoch_ai::model::catalog::find(offerId);
            if (!offer || !epoch_ai::model::catalog::download_allowed(*offer))
            {
                editor.packageInstallStatus = "This researched model is not offered for installation on this target.";
                return false;
            }
            const auto cache = ai::native_runtime::model_install_cache_root();
            if (cache.empty())
            {
                editor.packageInstallStatus = "Native model downloads are read-only in this runtime. Use the owning Editor.";
                return false;
            }
            package_registry::PackageDescriptor package{};
            package.id = std::string{offer->package_id};
            return start_model_package_install(editor, package, true, cache);
        }

        [[nodiscard]] bool start_model_offer_registration(EditorState& editor, std::string_view offerId)
        {
            const auto* offer = epoch_ai::model::catalog::find(offerId);
            const auto plan = offer && epoch_ai::model::catalog::download_allowed(*offer)
                ? ai::model_install::plan_for(offer->package_id) : std::nullopt;
            if (editor.packageModelInstall || !plan || plan->artifacts.size() != 1u
                || plan->revision != offer->revision || plan->artifacts.front().file != offer->filename
                || plan->artifacts.front().bytes != offer->bytes || plan->artifacts.front().sha256 != offer->sha256)
            {
                editor.packageInstallStatus = "Finish the active transfer or choose an eligible pinned model before registration.";
                return false;
            }
            const auto cache = ai::native_runtime::model_install_cache_root();
            if (cache.empty())
            {
                editor.packageInstallStatus = "Native model registration is read-only in this runtime. Use the owning Editor.";
                return false;
            }
            EditorModelInstallState operation{};
            operation.plan = *plan;
            operation.cacheRoot = cache;
            operation.registrationOfferId = std::string{offerId};
            try
            {
                operation.publishPending.emplace(schedule_editor_task(editor,
                    "Verify candidate model snapshot before registration",
                    [cache, plan = *plan] { return ai::model_install::verify_snapshot(cache, plan); }));
                editor.packageModelInstall.emplace(std::move(operation));
            }
            catch (const std::exception& error)
            {
                editor.packageInstallStatus = "Model verification could not be queued: " + std::string{error.what()};
                return false;
            }
            editor.packageInstallStatus = "Checking the exact receipt and complete weight SHA-256 in the background; no model will be loaded.";
            editor.packageInstallProgress = 0.0f;
            return true;
        }

        void cancel_model_offer_operation(EditorState& editor)
        {
            if (!editor.packageModelInstall) return;
            editor.packageModelInstall->cancelled = true;
            if (editor.packageModelInstall->process.valid())
                (void)platform::child_process::stop(editor.packageModelInstall->process,
                    platform::child_process::StopMode::graceful);
            editor.packageInstallStatus = "Cancellation requested. Verification may finish safely; registration will not be queued.";
        }

        [[nodiscard]] bool package_source_task_busy(const EditorState& editor) noexcept
        {
            return editor.packageSourceTask && editor.packageSourceTask->busy();
        }

        void cancel_package_source_task(EditorState& editor)
        {
            editor.packageSourceReviewCurrent = false;
            if (editor.planetaryFeatureRequest)
            {
                editor.planetaryFeatureRequest.reset();
                if (editor.taskScheduler && editor.planetaryCancellation.valid())
                    (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
                editor.planetaryStatus = "Water review cancelled. Requested settings and existing project source are retained; nothing will build.";
                if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
            }
            if (editor.aiPackageRequest
                && editor.aiPackageRequest->phase == EditorState::AiPackageRequest::Phase::review)
                finish_ai_project_package_request(editor, "Package review cancelled. No terrain build was started.");
            else if (editor.aiPackageRequest
                && editor.aiPackageRequest->phase == EditorState::AiPackageRequest::Phase::attaching
                && !editor.aiPackageRequest->cancelled)
            {
                editor.aiPackageRequest->cancelled = true;
                if (editor.taskScheduler && editor.planetaryCancellation.valid())
                    (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
                editor.aiToolStatus = "Cancelling the approved source copy; terrain will not be built.";
            }
            if (!package_source_task_busy(editor)) return;
            editor.packageSourceTask->cancellation.request_stop();
            editor.packageSourceStatus = "Cancelling the local source operation...";
        }

        [[nodiscard]] bool package_source_task_current(const EditorState& editor,
            const EditorPackageSourceTask& task) noexcept
        {
            return !task.cancellation.stop_requested()
                && task.expectedPackageId == editor.packageSourceExpectedId
                && task.projectGeneration
                    == editor.projectLifecycle.evidence().generations.selected_project.generation;
        }

        void poll_package_source_task(EditorState& editor)
        {
            if (!editor.packageSourceTask) return;
            auto& task = *editor.packageSourceTask;
            try
            {
                if (task.cachedInspection && task.cachedInspection->wait_for(0ms) == std::future_status::ready)
                {
                    auto cached = task.cachedInspection->get();
                    task.cachedInspection.reset();
                    if (package_source_task_current(editor, task) && cached.review_ready()
                        && cached.inspection.registry_package_id == editor.packageSourceExpectedId)
                    {
                        editor.packageSourceFileLines.clear();
                        const auto path = cached.inspection.manifest_path.generic_u8string();
                        editor.packageSourcePathDraft.assign(path.begin(), path.end());
                        for (const auto& file : cached.inspection.files)
                            editor.packageSourceFileLines.push_back(file.relative_path + " (" + std::to_string(file.bytes) + " bytes)");
                        editor.packageSourceReview = std::move(cached.inspection);
                        editor.packageSourceStaged = std::move(cached.staged);
                        editor.packageSourceReviewCurrent = true;
                        editor.packageSourceStatus = "Local package verified. Review the files, then add it to your project or create a terrain project. Nothing has been built or run.";
                    }
                    else
                    {
                        editor.packageSourceReviewCurrent = false;
                        editor.packageSourceStatus = cached.inspection.review_ready()
                            ? cached.staged.message : cached.inspection.message;
                        if (editor.packageSourceReview && editor.packageSourceReview->review_ready()
                            && editor.packageSourceReview->registry_package_id == editor.packageSourceExpectedId
                            && editor.packageSourceStaged.source_staged())
                            editor.packageSourceStatus += " Previous reviewed source is retained; this review did not update it.";
                        else
                        {
                            editor.packageSourceReview.reset();
                            editor.packageSourceFileLines.clear();
                            editor.packageSourceStaged = {};
                        }
                    }
                    push_editor_log(editor, "[package] " + editor.packageSourceStatus);
                }
                if (task.inspection && task.inspection->wait_for(0ms) == std::future_status::ready)
                {
                    auto review = task.inspection->get();
                    task.inspection.reset();
                    const auto* descriptor = package_registry::find(review.registry_package_id);
                    if (!package_source_task_current(editor, task) || (review.review_ready() && (!descriptor
                        || package_registry::catalog_section(review.registry_package_id) != package_registry::CatalogSection::ProjectPackages
                        || (!editor.packageSourceExpectedId.empty() && editor.packageSourceExpectedId != review.registry_package_id))))
                    {
                        editor.packageSourceStatus = "This manifest does not match the selected project package. Choose its matching manifest or use Local Source for another package.";
                        editor.packageSourceReview.reset();
                        editor.packageSourceReviewCurrent = false;
                    }
                    else
                    {
                        editor.packageSourceStatus = review.review_ready()
                            ? "Source checked. Review its purpose, license and files before approving a local copy."
                            : review.message;
                        editor.packageSourceFileLines.clear();
                        if (review.review_ready())
                            for (const auto& file : review.files)
                                editor.packageSourceFileLines.push_back(file.relative_path + " (" + std::to_string(file.bytes) + " bytes)");
                        editor.packageSourceReview = std::move(review);
                        editor.packageSourceReviewCurrent = editor.packageSourceReview->review_ready();
                    }
                }
                if (task.staging && task.staging->wait_for(0ms) == std::future_status::ready)
                {
                    auto staged = task.staging->get();
                    task.staging.reset();
                    if (!package_source_task_current(editor, task))
                    {
                        editor.packageSourceReviewCurrent = false;
                        editor.packageSourceStatus = "The package, project or source operation changed. Review the source again before approval.";
                    }
                    else
                    {
                        editor.packageSourceStaged = std::move(staged);
                        editor.packageSourceStatus = editor.packageSourceStaged.source_staged()
                            ? "Source staged in the local package cache. It has not been built, run or activated in a project."
                            : editor.packageSourceStaged.message;
                    }
                    push_editor_log(editor, "[package] " + editor.packageSourceStatus);
                }
            }
            catch (...)
            {
                editor.packageSourceReviewCurrent = false;
                task.cancellation.request_stop();
                task.inspection.reset();
                task.cachedInspection.reset();
                task.staging.reset();
                editor.packageSourceStatus = "The local source operation failed. Its source and existing staged copies were not replaced.";
                push_editor_log(editor, "[package] " + editor.packageSourceStatus);
            }
            if (!task.busy()) editor.packageSourceTask.reset();
        }

        void open_package_source_review(EditorState& editor, std::string expected_package_id = {})
        {
            if (package_source_task_busy(editor)) return;
            editor.packageSourceReviewCurrent = false;
            editor.packageSourceView = true;
            if (editor.packageSourceExpectedId != expected_package_id)
            {
                editor.packageSourceReview.reset();
                editor.packageSourceFileLines.clear();
                editor.packageSourceStaged = {};
            }
            editor.packageSourceExpectedId = std::move(expected_package_id);
            if (editor.packageSourceExpectedId.empty()) return;
            auto root = core::path::candidate_data_root();
            if (root.empty()) root = core::path::executable_dir();
            if (root.empty())
            {
                editor.packageSourceStatus = "The local package cache is unavailable.";
                return;
            }
            editor.packageSourceStatus = "Checking this extension's local package files...";
            try
            {
                editor.packageSourceTask = std::make_unique<EditorPackageSourceTask>();
                editor.packageSourceTask->expectedPackageId = editor.packageSourceExpectedId;
                editor.packageSourceTask->projectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
                const auto cancellation = editor.packageSourceTask->cancellation.get_token();
                const auto repository = core::path::find_epoch_repo_root(core::path::executable_dir());
                editor.packageSourceTask->cachedInspection.emplace(std::async(std::launch::async,
                    [cache = root / "cache" / "packages",
                        extensions = repository.empty() ? std::filesystem::path{}
                            : repository / "Engine" / "dep" / "EpochEngineExtensions",
                        id = editor.packageSourceExpectedId, cancellation] {
                        return package_source::inspect_available_source(cache, extensions, id, cancellation);
                    }));
            }
            catch (...)
            {
                editor.packageSourceTask.reset();
                editor.packageSourceStatus = "The local package could not be checked. No files were added.";
            }
        }

        void start_package_source_inspection(EditorState& editor)
        {
            if (package_source_task_busy(editor)) return;
            editor.packageSourceReviewCurrent = false;
            editor.packageSourceView = true;
            if (editor.packageSourcePathDraft.empty())
            {
                editor.packageSourceStatus = "Enter the path to a .epoch-package.json manifest first.";
                return;
            }
            editor.packageSourceReview.reset();
            editor.packageSourceFileLines.clear();
            editor.packageSourceStaged = {};
            editor.packageSourceTechnicalDetails = false;
            editor.packageSourceStatus = "Checking the selected manifest and its source files...";
            try
            {
                const std::filesystem::path manifest{std::u8string{editor.packageSourcePathDraft.begin(), editor.packageSourcePathDraft.end()}};
                editor.packageSourceTask = std::make_unique<EditorPackageSourceTask>();
                editor.packageSourceTask->expectedPackageId = editor.packageSourceExpectedId;
                editor.packageSourceTask->projectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
                const auto cancellation = editor.packageSourceTask->cancellation.get_token();
                editor.packageSourceTask->inspection.emplace(std::async(std::launch::async,
                    [manifest, cancellation] {
                        return package_source::inspect_local_source(manifest, cancellation);
                    }));
            }
            catch (...)
            {
                editor.packageSourceTask.reset();
                editor.packageSourceStatus = "Could not start the local source check. Check the path and try again. No files were staged.";
            }
        }

        void browse_package_source(EditorState& editor)
        {
            if (package_source_task_busy(editor)) return;
            const auto chosen = platform::filesystem::open_file_dialog(
                L"Review a local add-on source package", L"Epoch package manifest", L"*.epoch-package.json");
            if (!chosen.accepted)
            {
                if (!chosen.cancelled) editor.packageSourceStatus = chosen.diagnostic;
                return;
            }
            const auto utf8 = chosen.path.generic_u8string();
            editor.packageSourcePathDraft.assign(utf8.begin(), utf8.end());
            start_package_source_inspection(editor);
        }

        [[nodiscard]] bool planetary_project_busy(const EditorState& editor) noexcept
        {
            return editor.planetaryAttachmentPending.has_value() || editor.planetaryGeneratePending.has_value()
                || editor.planetaryStreamPending.has_value() || editor.planetaryFeatureReviewPending.has_value()
                || editor.planetaryFeatureAdmissionPending.has_value() || editor.waterAttachmentPending.has_value();
        }

        void cancel_planetary_project_task(EditorState& editor)
        {
            editor.planetaryFeatureRequest.reset();
            if (planetary_project_busy(editor) && editor.taskScheduler)
            {
                (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
                editor.planetaryStreamContinue = false;
                editor.planetaryStreamFailed = true;
                editor.planetaryStatus = "Cancelling the owned planetary operation...";
                if (editor.aiPackageRequest)
                {
                    editor.aiPackageRequest->cancelled = true;
                    editor.aiToolStatus = "Cancelling the package/terrain request; no late result will be imported.";
                }
            }
        }

        [[nodiscard]] std::uint64_t planetary_project_key(const EditorState& editor) noexcept
        {
            return editor.projectLifecycle.evidence().generations.selected_project.project_key;
        }

        void start_admitted_planetary_generation(EditorState& editor);

        void save_admitted_planetary_settings(EditorState& editor)
        {
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            const bool saved = project_planetary::save_settings(root, editor.planetarySettings)
                && project_planetary::load_settings(root) == editor.planetarySettings;
            if (saved) refresh_editor_planetary_mesh_policy(editor);
            editor.planetaryStatus = saved
                ? "Requested planetary settings saved. The displayed mesh may still be from the previous generation. Choose Build & Generate to apply them."
                : "Could not save the planetary settings. Existing files were retained.";
        }

        [[nodiscard]] bool planetary_feature_request_current(const EditorState& editor)
        {
            if (!editor.planetaryFeatureRequest) return false;
            const auto& request = *editor.planetaryFeatureRequest;
            return request.project_root == editor.projectRoot && request.project_key == planetary_project_key(editor)
                && request.project_generation == editor.projectLifecycle.evidence().generations.selected_project.generation
                && request.owner_hash == editor.planetaryAttachment.content_hash
                && request.settings == editor.planetarySettings && !editor.projectBuildPending && !editor.scriptBuildPending;
        }

        void approve_planetary_water_source(EditorState& editor)
        {
            if (planetary_project_busy(editor) || !planetary_feature_request_current(editor)
                || !editor.planetaryFeatureRequest->reviewed)
            {
                editor.packageSourceStatus = "Project, source or settings changed. Choose Save Settings or Build & Generate to review Water again.";
                return;
            }
            const auto reviewed = *editor.planetaryFeatureRequest->reviewed;
            const auto hash = reviewed.needs_approval() ? reviewed.dependency.inspection.content_hash : std::string{};
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, "Include reviewed Water source for planetary ocean",
                    [reviewed, hash](std::stop_token cancellation) {
                        return package_source::admit_project_feature(reviewed, hash, cancellation);
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.planetaryFeatureAdmissionPending.emplace(std::move(submitted.completion));
                editor.packageSourceReviewCurrent = false;
                editor.packageSourceStatus = "Including the approved Water source automatically. Cancel remains available; existing source is never replaced.";
            }
            catch (...) { editor.packageSourceStatus = "Water source admission could not start. Settings and preview are retained."; }
        }

        void request_planetary_feature_action(EditorState& editor, bool generate)
        {
            if (planetary_project_busy(editor) || package_source_task_busy(editor))
            { editor.planetaryStatus = "Settings have not been saved: finish or cancel the active terrain/package task, then retry. Your requested settings are retained."; return; }
            if (editor.projectRoot.empty())
            { editor.planetaryStatus = "Settings have not been saved: open a project first."; return; }
            if (!editor.planetarySettings.ocean_surface)
            {
                editor.planetaryFeatureRequest.reset();
                if (generate) start_admitted_planetary_generation(editor);
                else save_admitted_planetary_settings(editor);
                return;
            }
            if (editor.projectBuildPending || editor.scriptBuildPending)
            { editor.planetaryStatus = "Finish or cancel the current project/script build before reviewing Water source. Requested settings are retained."; return; }
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            auto cacheRoot = core::path::candidate_data_root();
            if (cacheRoot.empty()) cacheRoot = core::path::executable_dir();
            const auto repository = core::path::find_epoch_repo_root(core::path::executable_dir());
            if (cacheRoot.empty()) { editor.planetaryStatus = "Water source cache is unavailable. Settings are retained."; return; }
            editor.planetaryFeatureRequest = EditorState::PlanetaryFeatureRequest{
                .project_root=editor.projectRoot, .owner_hash=editor.planetaryAttachment.content_hash,
                .project_key=planetary_project_key(editor),
                .project_generation=editor.projectLifecycle.evidence().generations.selected_project.generation,
                .settings=editor.planetarySettings, .generate=generate};
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, "Check Water prerequisite for planetary ocean",
                    [root, cache=cacheRoot/"cache"/"packages", extensions=repository.empty()?std::filesystem::path{}
                        :repository/"Engine"/"dep"/"EpochEngineExtensions"](std::stop_token cancellation) {
                        return package_source::review_project_feature(root, "research_planetary_terrain",
                            package_source::SourceFeature::planetary_ocean, true, cache, extensions, cancellation);
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.planetaryFeatureReviewPending.emplace(std::move(submitted.completion));
                editor.planetaryStatus = "Ocean is On: Water (FFT Ocean) will be included automatically after its source approval. Checking this project's dependencies...";
            }
            catch (...) { editor.planetaryFeatureRequest.reset(); editor.planetaryStatus = "Could not check Water source. Nothing was saved or built."; }
        }

        void poll_planetary_feature_task(EditorState& editor)
        {
            if (editor.planetaryFeatureReviewPending && editor.planetaryFeatureReviewPending->wait_for(0ms) == std::future_status::ready)
            {
                auto reviewed = editor.planetaryFeatureReviewPending->get();
                editor.planetaryFeatureReviewPending.reset();
                if (!planetary_feature_request_current(editor))
                {
                    editor.planetaryFeatureRequest.reset();
                    editor.planetaryStatus = "Water review discarded because the project, source or settings changed. Nothing was saved or built.";
                    if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
                    return;
                }
                if ((!reviewed.needs_approval() && !reviewed.sources_admitted())
                    || reviewed.owner.content_hash != editor.planetaryFeatureRequest->owner_hash)
                {
                    editor.planetaryFeatureRequest.reset();
                    editor.planetaryStatus = "Water is required with Ocean On: " + reviewed.message + " Choose Ocean Off for a dry planet.";
                    if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
                    return;
                }
                editor.planetaryFeatureRequest->reviewed = reviewed;
                if (reviewed.sources_admitted()) { approve_planetary_water_source(editor); return; }
                editor.packageSourceExpectedId = "research_fft_ocean";
                editor.packageSourceReview = reviewed.dependency.inspection;
                editor.packageSourceStaged = reviewed.dependency.staged;
                editor.packageSourceFileLines.clear();
                for (const auto& file : reviewed.dependency.inspection.files)
                    editor.packageSourceFileLines.push_back(file.relative_path + " (" + std::to_string(file.bytes) + " bytes)");
                editor.packageSourceReviewCurrent = true;
                editor.packageSourceView = true;
                editor.showPackageManagerModal = true;
                editor.packageSourceStatus = "Planet with Ocean On requires Water (FFT Ocean). Approve these exact files once; Water will be included automatically before your requested save or build. Cancel keeps the existing project.";
            }
            if (editor.planetaryFeatureAdmissionPending && editor.planetaryFeatureAdmissionPending->wait_for(0ms) == std::future_status::ready)
            {
                const auto admitted = editor.planetaryFeatureAdmissionPending->get();
                editor.planetaryFeatureAdmissionPending.reset();
                if (!planetary_feature_request_current(editor) || !admitted.sources_admitted())
                {
                    editor.planetaryFeatureRequest.reset();
                    editor.planetaryStatus = admitted.sources_admitted()
                        ? "Approved Water source retained; project or settings changed, so nothing was saved or built."
                        : "Water admission refused: " + admitted.message;
                    editor.packageSourceStatus = editor.planetaryStatus;
                    if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
                    return;
                }
                const bool generate = editor.planetaryFeatureRequest->generate;
                editor.planetaryFeatureRequest.reset();
                editor.showPackageManagerModal = false;
                if (generate)
                {
                    start_admitted_planetary_generation(editor);
                    if (editor.aiPackageRequest && !editor.planetaryGeneratePending)
                        finish_ai_project_package_request(editor, "Water source admitted, but terrain generation could not start: " + editor.planetaryStatus);
                }
                else save_admitted_planetary_settings(editor);
            }
        }

        void start_reviewed_water_attachment(EditorState& editor)
        {
            if (planetary_project_busy(editor) || package_source_task_busy(editor) || editor.projectRoot.empty()
                || editor.projectBuildPending || editor.scriptBuildPending || !editor.packageSourceReviewCurrent
                || !editor.packageSourceReview || !editor.packageSourceReview->review_ready()
                || !editor.packageSourceStaged.source_staged()
                || editor.packageSourceReview->registry_package_id != "research_fft_ocean") return;
            if (editor.planetaryFeatureRequest)
            {
                const auto& feature = editor.planetaryFeatureRequest->reviewed;
                if (!feature || !feature->needs_approval()
                    || feature->dependency.inspection.content_hash != editor.packageSourceReview->content_hash)
                { editor.packageSourceStatus = "Displayed Water source changed. Review the planetary action again before approval."; return; }
                approve_planetary_water_source(editor);
                return;
            }
            const auto reviewed = *editor.packageSourceReview;
            const auto staged = editor.packageSourceStaged;
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            editor.planetaryOperationRoot = editor.projectRoot;
            editor.planetaryOperationProjectKey = planetary_project_key(editor);
            editor.planetaryOperationProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, "Add reviewed Water source to project",
                    [reviewed, staged, root](std::stop_token cancellation) {
                        return package_source::attach_project_source(reviewed, staged, reviewed.content_hash, root, cancellation);
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.waterAttachmentPending.emplace(std::move(submitted.completion));
                editor.packageSourceReviewCurrent = false;
                editor.packageSourceStatus = "Copying the approved Water source. This does not build or run the project.";
            }
            catch (...) { editor.packageSourceStatus = "Water source copy could not start. Existing project source is retained."; }
        }

        void start_planetary_project_attachment(EditorState& editor,bool update_source=false)
        {
            if (planetary_project_busy(editor) || editor.projectRoot.empty()
                || package_source_task_busy(editor) || !editor.packageSourceReviewCurrent
                || !editor.packageSourceReview || !editor.packageSourceReview->review_ready()
                || !editor.packageSourceStaged.source_staged()
                || editor.packageSourceReview->registry_package_id != "research_planetary_terrain") return;
            const auto reviewed = *editor.packageSourceReview;
            const auto staged = editor.packageSourceStaged;
            if(editor.projectBuildPending||editor.scriptBuildPending)
            {editor.packageSourceStatus="Finish or cancel the current project/script build before changing its source dependency.";return;}
            const std::string previousSource=update_source?editor.planetaryAttachment.content_hash:std::string{};
            if(update_source&&(!editor.planetaryAttachment.source_attached()||previousSource==reviewed.content_hash||editor.aiPackageRequest))
            {editor.packageSourceStatus="Review the current installed version and explicitly choose Update Project Source.";return;}
            if (editor.aiPackageRequest)
            {
#if EPOCH_EDITION_PUBLIC
                if (!project_ai_scope_allowed(editor.projectRoot))
                {
                    finish_ai_project_package_request(editor, "Package attachment refused: this is no longer an admitted application project.");
                    return;
                }
#endif
                if (!ai_project_package_request_current(editor) || editor.aiPackageRequest->cancelled
                    || editor.aiPackageRequest->phase != EditorState::AiPackageRequest::Phase::review)
                {
                    finish_ai_project_package_request(editor, "The package request changed before source approval. Nothing was copied.");
                    return;
                }
                editor.aiPackageRequest->source_hash = reviewed.content_hash;
                editor.aiPackageRequest->phase = EditorState::AiPackageRequest::Phase::attaching;
            }
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            editor.planetaryOperationRoot = editor.projectRoot;
            editor.planetaryOperationProjectKey = planetary_project_key(editor);
            editor.planetaryOperationProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
            editor.planetaryAttachmentIsInstall = !update_source;
            editor.planetaryAttachmentIsUpdate = update_source;
            editor.planetaryCheckedRoot = editor.projectRoot;
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, update_source?"Update reviewed Planetary Terrain source":"Add Planetary Terrain source to project",
                    [reviewed, staged, root, previousSource](std::stop_token cancellation) {
                        return previousSource.empty()?package_source::attach_project_source(reviewed, staged, reviewed.content_hash, root, cancellation)
                            :package_source::update_project_source(reviewed,staged,reviewed.content_hash,root,previousSource,cancellation);
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.planetaryAttachmentPending.emplace(std::move(submitted.completion));
                editor.planetaryStatus = update_source?"Preparing the reviewed source update; previous source, scenes and settings will be retained..."
                    :"Copying the approved Planetary Terrain source into this project...";
                editor.packageSourceStatus = editor.planetaryStatus;
            }
            catch (...)
            {
                editor.planetaryStatus = "Could not start the project source copy.";
                if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
            }
        }

        void create_planetary_example_project(EditorState& editor,
            const std::shared_ptr<core::Context>& context)
        {
            if (package_source_task_busy(editor) || planetary_project_busy(editor)
                || !editor.packageSourceReviewCurrent
                || !editor.packageSourceReview || !editor.packageSourceReview->review_ready()
                || !editor.packageSourceStaged.source_staged()
                || editor.packageSourceReview->registry_package_id != "research_planetary_terrain") return;
            const auto reviewed = *editor.packageSourceReview;
            const auto staged = editor.packageSourceStaged;
            const auto created = editor_create_project_shell(
                EditorProjectKind::Game, EditorProjectInputProvision::project_default);
            editor.packageSourceStatus = created.summary;
            if (!created.succeeded) return;
            const auto admitted = editor_admit_project_manifest(created.manifest_path);
            if (!admitted.succeeded || !editor_open_project(context, admitted.project_id))
            {
                editor.packageSourceStatus = "Terrain project created at " + created.root_path
                    + ", but it could not be opened. " + admitted.summary;
                return;
            }
            editor.packageSourceReview = reviewed;
            editor.packageSourceStaged = staged;
            editor.projectCreationOpen = false;
            editor.mainSurface = EditorMainSurface::Project;
            start_planetary_project_attachment(editor);
        }

        [[nodiscard]] std::string planetary_generation_block_reason(const EditorState& editor)
        {
            if (editor.projectBuildPending)
            {
                const auto& configuration = editor.projectBuildRequestedConfiguration.empty()
                    ? editor.projectBuildConfiguration : editor.projectBuildRequestedConfiguration;
                return "Generate is unavailable during the " + configuration
                    + " project build. Finish it or use Cancel Project Build, then choose Build & Generate again. Requested settings and the current preview are retained.";
            }
            if (editor.scriptBuildPending)
                return "Generate is unavailable during the script build. Finish or cancel it, then choose Build & Generate again. Requested settings and the current preview are retained.";
            if (planetary_project_busy(editor))
                return "Terrain work is active. Wait or use its Cancel control, then choose Build & Generate again. Requested settings and the current preview are retained.";
            if (!editor.planetaryAttachment.source_attached())
                return "Add and review the Planetary Terrain source in Package Manager before generating. The current preview is retained.";
            return {};
        }

        void start_planetary_project_generation(EditorState& editor)
        {
            if (const auto blocked = planetary_generation_block_reason(editor); !blocked.empty())
            { editor.planetaryStatus = blocked; return; }
            request_planetary_feature_action(editor, true);
        }

        [[nodiscard]] bool save_ai_planetary_creation_recipe(EditorState& editor, const std::filesystem::path& root)
        {
            if (!editor.aiPackageRequest || !editor.aiPackageRequest->save_recipe_before_generation)
                return true;
            if (!ai_project_package_generation_current(editor))
            {
                editor.planetaryStatus = "The approved creation request changed. No recipe was saved or terrain generation started.";
                return false;
            }
            if (project_planetary::save_settings(root, editor.planetarySettings)
                && project_planetary::load_settings(root) == editor.planetarySettings)
                return true;
            editor.planetaryStatus = "The approved planetary recipe could not be saved and verified. No terrain generation was started.";
            return false;
        }

        void start_admitted_planetary_generation(EditorState& editor)
        {
            if (const auto blocked = planetary_generation_block_reason(editor); !blocked.empty())
            {
                editor.planetaryStatus = blocked;
                return;
            }
            std::uint32_t seed{};
            const auto parsed = std::from_chars(editor.planetarySeedDraft.data(),
                editor.planetarySeedDraft.data() + editor.planetarySeedDraft.size(), seed);
            if (parsed.ec != std::errc{} || parsed.ptr != editor.planetarySeedDraft.data() + editor.planetarySeedDraft.size())
            {
                editor.planetaryStatus = "Seed must be a whole number between 0 and 4294967295.";
                return;
            }
            editor.planetarySettings.seed = seed;
            editor.planetaryStreamFailed = false;
            editor.planetaryStreamContinue = true;
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            if (!save_ai_planetary_creation_recipe(editor, root)) return;
            const auto source = root / editor.planetaryAttachment.relative_source_root;
            const auto engine = core::path::find_epoch_repo_root(core::path::executable_dir());
            if (engine.empty())
            {
                editor.planetaryStatus = "Generation needs this edition's admitted Engine source checkout.";
                return;
            }
            const auto settings = editor.planetarySettings;
            editor.planetaryBuildLog.clear();
            editor.planetaryLogLines.clear();
            editor.planetaryBuildSeconds = 0.0;
            editor.planetaryGenerationSeconds = 0.0;
            editor.planetaryOperationRoot = editor.projectRoot;
            editor.planetaryOperationProjectKey = planetary_project_key(editor);
            editor.planetaryOperationProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, "Build and generate planetary terrain",
                    [root, source, engine, settings](std::stop_token cancellation) {
                        return project_planetary::generate(root, source, engine, settings, cancellation);
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.planetaryGeneratePending.emplace(std::move(submitted.completion));
                editor.planetaryStatus = "Building the approved planetary generator, then generating terrain...";
            }
            catch (...) { editor.planetaryStatus = "Could not start Build & Generate."; }
        }

        void poll_planetary_project_task(EditorState& editor)
        {
            const auto current_operation = [&] {
                return editor.planetaryOperationRoot == editor.projectRoot
                    && editor.planetaryOperationProjectKey == planetary_project_key(editor)
                    && editor.planetaryOperationProjectGeneration == editor.projectLifecycle.evidence().generations.selected_project.generation;
            };
            try
            {
                poll_planetary_feature_task(editor);
                if (editor.waterAttachmentPending && editor.waterAttachmentPending->wait_for(0ms) == std::future_status::ready)
                {
                    const auto attached = editor.waterAttachmentPending->get();
                    editor.waterAttachmentPending.reset();
                    if (current_operation()) editor.packageSourceStatus = attached.message;
                }
                if (editor.planetaryAttachmentPending && editor.planetaryAttachmentPending->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
                {
                    auto attached = editor.planetaryAttachmentPending->get();
                    editor.planetaryAttachmentPending.reset();
                    if (current_operation())
                    {
                        editor.planetaryAttachment = std::move(attached);
                        editor.planetaryMeshPolicy.reset();
                        editor.planetaryStatus = editor.planetaryAttachment.message;
                        if (editor.planetaryAttachment.source_attached())
                        {
                            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
                            // Replacing an explicitly reviewed package source must
                            // not replace this project's unsaved authoring draft.
                            // Ordinary open/install still restores the disk recipe.
                            if (!editor.planetaryAttachmentIsUpdate)
                            {
                                const auto savedSettings = project_planetary::load_settings(root);
                                editor.planetarySettings = savedSettings.value_or(project_planetary::Settings{});
                                if (!savedSettings && editor.planetaryAttachmentIsInstall)
                                {
                                    if(const auto detailed=project_planetary::with_detail_preset(
                                        editor.planetarySettings,project_planetary::DetailPreset::detailed))
                                        editor.planetarySettings=*detailed;
                                    editor.planetarySettings.size_reference_body = editor.planetarySettings.body_index;
                                    editor.planetarySettings.reference_radius_meters = 25000.0;
                                    editor.planetarySettings.ocean_surface = project_planetary::supports_ocean_surface(editor.planetarySettings);
                                }
                                editor.planetarySeedDraft = std::to_string(editor.planetarySettings.seed);
                            }
                            if (editor.planetaryAttachmentIsInstall||editor.planetaryAttachmentIsUpdate)
                            {
                                if (editor.planetaryAttachmentIsUpdate)
                                    editor.planetaryStatus=editor.planetaryAttachment.message;
                                else if (editor.planetarySettings.ocean_surface)
                                {
                                    if (!editor.aiPackageRequest || !editor.aiPackageRequest->generate)
                                        request_planetary_feature_action(editor, false);
                                }
                                else if (!project_planetary::save_settings(root, editor.planetarySettings))
                                    editor.planetaryStatus = "Package source added, but settings could not be saved. Open Project and retry Save Settings or Build & Generate.";
                                else editor.planetaryStatus = "Planetary Terrain added. Open Project to change settings, then choose Build & Generate.";
                                editor.packageSourceStatus = editor.planetaryStatus;
                                editor.mainSurface = EditorMainSurface::Project;
                                editor.showPackageManagerModal = false;
                            }
                            refresh_editor_planetary_mesh_policy(editor);
                        }
                        continue_ai_project_package_after_attachment(editor);
                    }
                    editor.planetaryAttachmentIsInstall = false;
                    editor.planetaryAttachmentIsUpdate = false;
                }
                if (editor.planetaryGeneratePending && editor.planetaryGeneratePending->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
                {
                    const auto generated = editor.planetaryGeneratePending->get();
                    editor.planetaryGeneratePending.reset();
                    if (current_operation())
                    {
                        if (!ai_project_package_generation_current(editor))
                        {
                            finish_ai_project_package_request(editor,
                                "Terrain result retained without import: its request was cancelled, or project/source/settings changed.");
                            return;
                        }
                        editor.planetaryStatus = generated.message;
                        editor.planetaryBuildLog = generated.build_log;
                        editor.planetaryBuildSeconds = generated.build_seconds;
                        editor.planetaryGenerationSeconds = generated.generation_seconds;
                        editor.planetaryLogLines.clear();
                        const auto previousMeshPolicy=editor.planetaryMeshPolicy;
                        if(generated.ok)refresh_editor_planetary_mesh_policy(editor,true);
                        bool imported{};
                        if (generated.ok && import_project_mesh(editor, generated.artifact_path, "Planetary Terrain",
                            editor_planetary_mesh_budget(editor).artifact, generated.mesh_frame))
                        {
                            imported = true;
                            editor.planetaryPreviewFocusPending = true;
                            editor.planetaryStatus = "Terrain generated. Opening its scene and saving the placement...";
                        }
                        else if (generated.ok) {editor.planetaryMeshPolicy=previousMeshPolicy;editor.planetaryStatus = editor.projectStatus;}
                        push_editor_log(editor, "[planet] " + editor.planetaryStatus);
                        if (editor.aiPackageRequest)
                            finish_ai_project_package_request(editor, imported
                                ? "Terrain generated and imported. The existing Project view owns placement and save confirmation."
                                : "Terrain request did not import a result: " + editor.planetaryStatus, imported);
                    }
                }
                if (!planetary_project_busy(editor) && editor.planetaryCheckedRoot != editor.projectRoot)
                {
                    editor.planetaryCheckedRoot = editor.projectRoot;
                    editor.planetaryAttachment = {};
                    editor.planetarySettings = {};
                    editor.planetaryMeshPolicy.reset();
                    editor.planetarySeedDraft = "1337";
                    editor.planetaryStatus = "Add the optional Planetary Terrain package in Package Manager to generate terrain in this project.";
                    if (!editor.projectRoot.empty())
                    {
                        const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
                        editor.planetaryOperationRoot = editor.projectRoot;
                        editor.planetaryOperationProjectKey = planetary_project_key(editor);
            editor.planetaryOperationProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
                        editor.planetaryAttachmentIsInstall = false;
                        editor.planetaryAttachmentIsUpdate = false;
                        auto submitted = schedule_editor_cancellable_task(editor, "Check project's Planetary Terrain dependency",
                            [root](std::stop_token cancellation) {
                                return package_source::load_project_attachment(root, "research_planetary_terrain", cancellation);
                            });
                        editor.planetaryCancellation = submitted.cancellation;
                        editor.planetaryAttachmentPending.emplace(std::move(submitted.completion));
                    }
                }
            }
            catch (...)
            {
                editor.planetaryAttachmentPending.reset();
                editor.planetaryAttachmentIsInstall = false;
                editor.planetaryAttachmentIsUpdate = false;
                editor.planetaryGeneratePending.reset();
                editor.planetaryFeatureReviewPending.reset();
                editor.planetaryFeatureAdmissionPending.reset();
                editor.waterAttachmentPending.reset();
                editor.planetaryFeatureRequest.reset();
                editor.planetaryStatus = "The planetary operation failed. Existing project files and scene remain available.";
                if (editor.aiPackageRequest) finish_ai_project_package_request(editor, editor.planetaryStatus);
                push_editor_log(editor, "[planet] " + editor.planetaryStatus);
            }
        }

        // Camera-driven residency is disposable render state. It never creates
        // authoring transactions or changes the persisted fallback mesh.
        [[nodiscard]] std::optional<double> planetary_preview_sample_time(bool ocean,
            const epochengine::timeline::TimelineState& timeline) noexcept
        {
            if (!ocean || !std::isfinite(timeline.playhead_seconds)
                || timeline.playhead_seconds < 0.0 || timeline.playhead_seconds >= 1.0e12) return {};
            // Coalesce existing worker requests; the playhead is the only
            // animation time. A paused seek retains its exact authored time.
            return timeline.playing ? std::floor(timeline.playhead_seconds*4.0)/4.0 : timeline.playhead_seconds;
        }

        [[nodiscard]] bool planetary_preview_time_reusable(const project_planetary::StreamView& current,
            const project_planetary::StreamView& requested,
            const epochengine::timeline::TimelineState& timeline, bool requestedPlaying,
            epochengine::timeline::PlaybackDirection requestedDirection, double requestedRate) noexcept
        {
            if (current.simulation_seconds.has_value() != requested.simulation_seconds.has_value()) return false;
            if (!current.simulation_seconds) return true;
            const auto now = *current.simulation_seconds, then = *requested.simulation_seconds;
            if (!std::isfinite(now) || !std::isfinite(then) || timeline.playing != requestedPlaying
                || timeline.direction != requestedDirection || timeline.playback_rate != requestedRate) return false;
            if (!timeline.playing) return now == then;
            // A complete cut remains useful during continuous playback, just
            // as during flight. Pause, seek and reverse cannot publish old time.
            return timeline.direction == epochengine::timeline::PlaybackDirection::reverse ? now <= then : now >= then;
        }

        void pump_planetary_stream(EditorState& editor, const void* context)
        {
            const auto tick = editor_steady_tick_ns();
            const bool covered = editor_project_viewport_input_reserved(editor);
            if (covered && editor.planetaryStreamPending && editor.taskScheduler)
                (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
            const auto entityAt = std::find_if(editor.entities.begin(), editor.entities.end(),
                [](const EditorEntity& entity) { return entity.category == "Planetary Terrain" && entity.meshAsset && entity.visible; });
            const auto owners = std::count_if(editor.entities.begin(), editor.entities.end(),
                [](const EditorEntity& entity) { return entity.category == "Planetary Terrain" && entity.meshAsset && entity.visible; });
            const auto meshBudget=editor_planetary_mesh_budget(editor);
            const auto admittedSettings=meshBudget.artifact.maximum_bytes!=0u
                ?std::optional<project_planetary::Settings>{editor.planetaryMeshPolicy->settings}:std::nullopt;
            const bool generationMatches=entityAt!=editor.entities.end()
                &&editor_planetary_stream_identity_matches(editor,*entityAt);
            const bool enabled = admittedSettings&&generationMatches&&admittedSettings->stream_lod && admittedSettings->mode == "planet"
                && owners == 1;
            bool viewValid = enabled;
            project_planetary::StreamView desiredView;
            bool changed = true;
            if (enabled)
            {
                desiredView.simulation_seconds = planetary_preview_sample_time(admittedSettings->ocean_surface,editor.timelineState);
                const auto camera = previewgrid::camera_for(context);
                // Invert the same Euler basis used by the preview renderer. Translation,
                // rotation and nonuniform scale stay owned by the existing scene object.
                const auto eye=previewgrid::camera_world_eye(camera);
                const std::array<double,3> relative{eye[0]-double(entityAt->position[0]),
                    eye[1]-double(entityAt->position[1]),eye[2]-double(entityAt->position[2])};
                const previewgrid::Vec3 rotation{entityAt->rotation[0], entityAt->rotation[1], entityAt->rotation[2]};
                const std::array bases{previewgrid::rotate_euler_degrees({1,0,0}, rotation),
                    previewgrid::rotate_euler_degrees({0,1,0}, rotation), previewgrid::rotate_euler_degrees({0,0,1}, rotation)};
                const auto direction=previewgrid::normalize(previewgrid::subtract(camera.target,camera.eye));
                std::array<double,3> localForward{},localUp{};
                auto& view = desiredView;
                const auto meshScale = scene::mesh_render_scale(entityAt->scale, entityAt->meshAsset);
                if (!meshScale) viewValid = false;
                for (std::size_t axis = 0; axis < 3u; ++axis)
                {
                    if (!meshScale || !std::isfinite((*meshScale)[axis]) || std::abs((*meshScale)[axis]) < 0.00001f) { viewValid = false; break; }
                    view.camera[axis] = (relative[0]*bases[axis].x+relative[1]*bases[axis].y
                        +relative[2]*bases[axis].z)/(*meshScale)[axis];
                    localForward[axis]=(double(direction.x)*bases[axis].x+double(direction.y)*bases[axis].y
                        +double(direction.z)*bases[axis].z)/(*meshScale)[axis];
                    localUp[axis]=(double(camera.up.x)*bases[axis].x+double(camera.up.y)*bases[axis].y
                        +double(camera.up.z)*bases[axis].z)/(*meshScale)[axis];
                }
                const double directionLength=std::hypot(localForward[0],localForward[1],localForward[2]);
                if(!std::isfinite(directionLength)||directionLength<1e-12)viewValid=false;
                else {for(auto& component:localForward)component/=directionLength;view.forward=localForward;}
                view.up=localUp;
                if(!project_planetary::valid_stream_up(view))viewValid=false;
                view.viewport_height = editor.planetaryViewportHeight;
                view.aspect_ratio=std::clamp(double(editor.planetaryViewportWidth)
                    /double((std::max)(1u,view.viewport_height)),0.1,10.0);
                view.vertical_fov = camera.fovRadians;
                changed = editor.planetaryStreamEntity != entityAt->sceneObjectId
                    || editor.planetaryStreamSettings != *admittedSettings
                    || view.viewport_height != editor.planetaryStreamView.viewport_height
                    || std::abs(view.vertical_fov - editor.planetaryStreamView.vertical_fov) > 0.001
                    || view.forward.has_value()!=editor.planetaryStreamView.forward.has_value()
                    || view.up.has_value()!=editor.planetaryStreamView.up.has_value()
                    || std::abs(view.aspect_ratio-editor.planetaryStreamView.aspect_ratio)>0.001;
                changed = changed || view.simulation_seconds != editor.planetaryStreamView.simulation_seconds
                    || !planetary_preview_time_reusable(view,editor.planetaryStreamView,editor.timelineState,
                        editor.planetaryStreamTimelinePlaying,editor.planetaryStreamTimelineDirection,editor.planetaryStreamTimelineRate);
                if(view.forward&&editor.planetaryStreamView.forward)
                    for(std::size_t axis=0;axis<3u;++axis)
                        changed=changed||std::abs((*view.forward)[axis]-(*editor.planetaryStreamView.forward)[axis])>0.01;
                if(view.up&&editor.planetaryStreamView.up)
                    for(std::size_t axis=0;axis<3u;++axis)
                        changed=changed||std::abs((*view.up)[axis]-(*editor.planetaryStreamView.up)[axis])>0.01;
                const double radius = std::sqrt(view.camera[0]*view.camera[0] + view.camera[1]*view.camera[1]
                    + view.camera[2]*view.camera[2]);
                const double epsilon = std::clamp(std::abs(radius - 10.0) * 0.05, 0.00001, 0.025);
                for (std::size_t axis = 0; axis < 3u; ++axis)
                    changed = changed || std::abs(view.camera[axis] - editor.planetaryStreamView.camera[axis]) > epsilon;
            }
            if (editor.planetaryStreamPending
                && editor.planetaryStreamPending->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                editor.planetaryStreamNextTick = tick + 250'000'000ull;
                try
                {
                    const auto streamed = editor.planetaryStreamPending->get();
                    editor.planetaryStreamPending.reset();
                    const bool current = enabled && editor.planetaryOperationRoot == editor.projectRoot
                        && editor.planetaryOperationProjectKey == planetary_project_key(editor)
                        && editor.planetaryOperationProjectGeneration == editor.projectLifecycle.evidence().generations.selected_project.generation
                        && entityAt->sceneObjectId == editor.planetaryStreamEntity
                        && entityAt->position == editor.planetaryStreamPosition
                        && entityAt->rotation == editor.planetaryStreamRotation && entityAt->scale == editor.planetaryStreamScale
                        && *admittedSettings == editor.planetaryStreamSettings
                        && editor.planetaryMeshPolicy
                        && editor.planetaryMeshPolicy->source.project_root == editor.planetaryStreamSource.project_root
                        && editor.planetaryMeshPolicy->source.project_manifest == editor.planetaryStreamSource.project_manifest
                        && editor.planetaryMeshPolicy->source.content_hash == editor.planetaryStreamSource.content_hash
                        && editor.planetaryMeshPolicy->generated_mesh == editor.planetaryStreamMesh
                        && entityAt->meshAsset->artifact_key == editor.planetaryStreamFallbackKey
                        && viewValid && project_planetary::stream_view_reusable(desiredView,editor.planetaryStreamView)
                        && planetary_preview_time_reusable(desiredView,editor.planetaryStreamView,editor.timelineState,
                            editor.planetaryStreamTimelinePlaying,editor.planetaryStreamTimelineDirection,editor.planetaryStreamTimelineRate);
                    if (current && (streamed.waiting_for_worker||streamed.operation.cancelled))
                    {
                        editor.planetaryStreamStatus = streamed.operation.message;
                        editor.planetaryStreamContinue = true;
                    }
                    else if (current && streamed.operation.ok && streamed.request_id == editor.planetaryStreamView.request_id
                        &&streamed.request_id==editor.planetaryStreamSequence)
                    {
                        if (streamed.snapshot_ready && streamed.snapshot)
                        {
                            std::uint64_t bytes{}, indices{};
                            for (const auto& entity : editor.entities)
                                if (entity.sceneObjectId != entityAt->sceneObjectId && entity.meshSnapshot)
                                {
                                    bytes += entity.meshSnapshot->vertices().size() * sizeof(asset::mesh::Vertex)
                                        + entity.meshSnapshot->indices().size() * sizeof(std::uint32_t) + 64u;
                                    if (entity.visible) indices += entity.meshSnapshot->indices().size();
                                }
                            bytes += streamed.snapshot->vertices().size() * sizeof(asset::mesh::Vertex)
                                + streamed.snapshot->indices().size() * sizeof(std::uint32_t) + 64u;
                            indices += streamed.snapshot->indices().size();
                            const previewgrid::ObjectMarker candidate{
                                .position{entityAt->position[0],entityAt->position[1],entityAt->position[2]},
                                .scale{entityAt->scale[0],entityAt->scale[1],entityAt->scale[2]},
                                .rotationDegrees{entityAt->rotation[0],entityAt->rotation[1],entityAt->rotation[2]},
                                .primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=streamed.snapshot};
                            std::vector<previewgrid::ObjectMarker> candidateFrame{candidate};
                            if(streamed.patch_snapshot&&streamed.patch_snapshot->metadata.cloud
                                &&streamed.patch_snapshot->metadata.cloud->artifact
                                &&!streamed.patch_snapshot->metadata.cloud->density)
                            {
                                const auto& cloud=streamed.patch_snapshot->metadata.cloud->artifact;
                                bytes+=cloud->vertices().size()*sizeof(asset::mesh::Vertex)
                                    +cloud->indices().size()*sizeof(std::uint32_t)+64u;
                                indices+=cloud->indices().size();
                                candidateFrame.push_back({.primitive=previewgrid::ObjectPreviewPrimitive::IndexedMesh,.mesh=cloud});
                            }
                            // The worker verified the exact source/recipe before and
                            // after execution; current above binds that immutable request
                            // to this project's present generation and mesh policy.
                            if (editor_planetary_stream_identity_matches(editor,*entityAt)
                                &&streamed.snapshot->vertices().size()<=meshBudget.artifact.maximum_vertices
                                &&streamed.snapshot->indices().size()<=meshBudget.artifact.maximum_indices
                                &&48u+std::uint64_t(streamed.snapshot->vertices().size())*44u
                                    +std::uint64_t(streamed.snapshot->indices().size())*4u<=meshBudget.artifact.maximum_bytes
                                &&bytes <= meshBudget.cache_bytes && indices <= meshBudget.frame_indices
                                &&previewgrid::mesh_frame_admitted(candidateFrame,
                                    {meshBudget.frame_indices,meshBudget.frame_bytes}))
                            {
                                editor.planetaryStreamSnapshot = streamed.snapshot;
                                editor.planetaryStreamCloud = streamed.patch_snapshot
                                    ? streamed.patch_snapshot->metadata.cloud : std::nullopt;
                                editor.planetaryStreamFrame = streamed.operation.mesh_frame;
                                entityAt->meshSnapshot = streamed.snapshot;
                                entityAt->meshSnapshotFrame = streamed.operation.mesh_frame;
                                entityAt->meshLoadAttempted = true;
                                editor.sceneInteractionDirty = true;
                            }
                            else
                            {
                                editor.planetaryStreamFailed = true;
                                editor.planetaryStreamStatus = "Planet LOD paused: the scene geometry or mesh memory budget is full.";
                            }
                        }
                        if (!editor.planetaryStreamFailed) editor.planetaryStreamStatus = streamed.operation.message;
                        editor.planetaryStreamContinue = streamed.pending != 0u || changed;
                        // A successful bounded step still has known work. Keep
                        // one owned job in flight without an extra idle delay;
                        // contention, cancellation and failures retain backoff.
                        if (!editor.planetaryStreamFailed && (streamed.pending != 0u || changed))
                            editor.planetaryStreamNextTick = tick;
                    }
                    else if (current && !streamed.operation.ok)
                    {
                        editor.planetaryStreamFailed = true;
                        editor.planetaryStreamStatus = streamed.operation.message;
                        if (!streamed.operation.build_log.empty()) editor.planetaryBuildLog = streamed.operation.build_log;
                        push_editor_log(editor, "[planet] " + editor.planetaryStreamStatus);
                    }
                    else if (enabled && viewValid && streamed.operation.ok && !streamed.operation.cancelled
                        && streamed.request_id == editor.planetaryStreamView.request_id
                        && streamed.request_id == editor.planetaryStreamSequence)
                    {
                        // No stale snapshot is published. The old worker has
                        // retired, so request the current camera without an
                        // additional contention/failure delay.
                        editor.planetaryStreamContinue = true;
                        editor.planetaryStreamNextTick = tick;
                    }
                }
                catch (const editor_tasks::TaskCancelled&)
                {
                    editor.planetaryStreamPending.reset();
                    editor.planetaryStreamContinue = true;
                    editor.planetaryStreamStatus = "Terrain detail cancelled. The previous mesh is retained.";
                }
                catch (...)
                {
                    editor.planetaryStreamPending.reset();
                    editor.planetaryStreamFailed = true;
                    editor.planetaryStreamStatus = "Planet LOD stopped after a worker failure. The saved terrain remains available.";
                }
            }
            // Poll and retire the previous authoring job above, but never
            // submit another LOD build behind an owned project viewport.
            if (covered) return;
            if(owners==1&&!generationMatches)
            {
                if(editor.planetaryStreamPending&&editor.taskScheduler)
                    (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
                editor.planetaryStreamContinue=false;
                const auto progress=editor_planetary_restore_progress(editor);
                editor.planetaryStreamStatus=progress.empty()
                    ? "Live LOD paused: the current mesh has no matching generated recipe/coordinate evidence. Choose Build & Generate; the previous mesh is retained."
                    : std::string{progress};
                return;
            }
            if (!enabled)
            {
                if (owners > 1) editor.planetaryStreamStatus = "Camera LOD supports one visible planet. Hide the other planets to resume.";
                else if (admittedSettings && generationMatches)
                    editor.planetaryStreamStatus = !admittedSettings->stream_lod
                        ? "Camera LOD is off in this project's terrain settings."
                        : admittedSettings->mode == "universe"
                            ? "System overview. Run the project to explore worlds with camera LOD."
                            : "This terrain coverage uses its generated overview in the editor.";
                if (editor.planetaryStreamPending && editor.taskScheduler)
                    (void)editor.taskScheduler->cancel(editor.planetaryCancellation);
                if (editor.planetaryStreamEntity)
                    for (auto& entity : editor.entities)
                        if (entity.sceneObjectId == editor.planetaryStreamEntity)
                        { entity.meshSnapshot.reset(); entity.meshSnapshotFrame.reset(); entity.meshLoadAttempted = false; editor.sceneInteractionDirty = true; }
                editor.planetaryStreamEntity = 0u;
                editor.planetaryStreamSnapshot.reset(); editor.planetaryStreamCloud.reset();
                editor.planetaryStreamFrame.reset();
                return;
            }
            if (editor.planetaryStreamSnapshot && !entityAt->meshSnapshot
                && entityAt->sceneObjectId == editor.planetaryStreamEntity
                && entityAt->meshAsset->artifact_key == editor.planetaryStreamFallbackKey)
            {
                entityAt->meshSnapshot = editor.planetaryStreamSnapshot;
                entityAt->meshSnapshotFrame = editor.planetaryStreamFrame;
                entityAt->meshLoadAttempted = true;
                editor.sceneInteractionDirty = true;
            }
            if (planetary_project_busy(editor) || editor.planetaryStreamFailed || tick < editor.planetaryStreamNextTick
                || editor.projectBuildPending || editor.scriptBuildPending
                || !(editor.mainSurface==EditorMainSurface::Scene
                    ||(editor.mainSurface==EditorMainSurface::ForestFactory&&editor.forestTerrainPainting))) return;
            if (!viewValid) return;
            // Capture known request identity here. Full filesystem/source hashing
            // belongs to the existing cancellable worker below, not the frame thread.
            if(!editor_planetary_stream_identity_matches(editor,*entityAt))
            {
                editor.planetaryStreamStatus="Live LOD paused because its generated mesh no longer matches the saved recipe. Generate again; the previous mesh is retained.";
                return;
            }
            if (entityAt->meshAsset->artifact_key != editor.planetaryStreamFallbackKey)
            { editor.planetaryStreamSnapshot.reset(); editor.planetaryStreamCloud.reset(); editor.planetaryStreamFrame.reset(); changed = true; }
            if (!changed && !editor.planetaryStreamContinue) return;
            auto view = desiredView;
            view.request_id = ++editor.planetaryStreamSequence;
            editor.planetaryStreamView = view;
            editor.planetaryStreamTimelinePlaying = editor.timelineState.playing;
            editor.planetaryStreamTimelineDirection = editor.timelineState.direction;
            editor.planetaryStreamTimelineRate = editor.timelineState.playback_rate;
            editor.planetaryStreamSettings = *admittedSettings;
            editor.planetaryStreamSource = editor.planetaryMeshPolicy->source;
            editor.planetaryStreamMesh = *editor.planetaryMeshPolicy->generated_mesh;
            editor.planetaryStreamEntity = entityAt->sceneObjectId;
            editor.planetaryStreamFallbackKey = entityAt->meshAsset->artifact_key;
            editor.planetaryStreamPosition = entityAt->position;
            editor.planetaryStreamRotation = entityAt->rotation;
            editor.planetaryStreamScale = entityAt->scale;
            editor.planetaryOperationRoot = editor.projectRoot;
            editor.planetaryOperationProjectKey = planetary_project_key(editor);
            editor.planetaryOperationProjectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            const auto settings = *admittedSettings;
            try
            {
                auto submitted = schedule_editor_cancellable_task(editor, "Stream planetary terrain detail",
                    [root, settings, view, source=editor.planetaryStreamSource,
                        mesh=editor.planetaryStreamMesh](std::stop_token cancellation) {
                        project_planetary::StreamResult result;
                        if (cancellation.stop_requested()) {
                            result.operation.cancelled = true;
                            result.operation.message = "Terrain streaming cancelled before source admission.";
                            return result;
                        }
                        if (!project_planetary::mesh_generation_is_current(root, source, settings, mesh)) {
                            result.operation.message = "Planet LOD paused: its source or saved recipe changed before the worker started. The previous mesh is retained.";
                            return result;
                        }
                        result = project_planetary::stream_step(root, settings, view, cancellation);
                        if (result.operation.ok
                            && !project_planetary::mesh_generation_is_current(root, source, settings, mesh)) {
                            result.operation.ok = false;
                            result.snapshot_ready = false;
                            result.snapshot.reset();
                            result.operation.message = "Planet LOD paused: its source or saved recipe changed during the worker. This snapshot was refused; the previous mesh is retained.";
                        }
                        return result;
                    });
                editor.planetaryCancellation = submitted.cancellation;
                editor.planetaryStreamPending.emplace(std::move(submitted.completion));
                editor.planetaryStreamNextTick = tick + 500'000'000ull;
            }
            catch (...) { editor.planetaryStreamFailed = true; editor.planetaryStreamStatus = "Could not queue planetary terrain detail."; }
        }

        void approve_and_stage_package_source(EditorState& editor)
        {
            if (package_source_task_busy(editor) || !editor.packageSourceReview
                || !editor.packageSourceReviewCurrent || !editor.packageSourceReview->review_ready()) return;
            const auto review = *editor.packageSourceReview;
            if (!package_registry::find(review.registry_package_id)
                || (!editor.packageSourceExpectedId.empty() && editor.packageSourceExpectedId != review.registry_package_id))
            {
                editor.packageSourceStatus = "Package identity changed. Review the correct source manifest first.";
                return;
            }
            auto mutable_root = core::path::candidate_data_root();
            if (mutable_root.empty()) mutable_root = core::path::executable_dir();
            if (mutable_root.empty())
            {
                editor.packageSourceStatus = "The executable-local cache location could not be resolved. No source was staged.";
                return;
            }
            const auto cache_root = mutable_root / "cache" / "packages";
            try
            {
                editor.packageSourceTask = std::make_unique<EditorPackageSourceTask>();
                editor.packageSourceTask->expectedPackageId = editor.packageSourceExpectedId;
                editor.packageSourceTask->projectGeneration = editor.projectLifecycle.evidence().generations.selected_project.generation;
                const auto cancellation = editor.packageSourceTask->cancellation.get_token();
                // Approval is captured once and binds these exact reviewed bytes.
                editor.packageSourceTask->staging.emplace(std::async(std::launch::async,
                    [review, approved_hash = review.content_hash, cache_root, cancellation] {
                        return package_source::stage_local_source(review, approved_hash, cache_root, cancellation);
                    }));
                editor.packageSourceStatus = "Copying the approved source into the local package cache...";
            }
            catch (...)
            {
                editor.packageSourceTask.reset();
                editor.packageSourceStatus = "Could not start source staging. No package was built or activated.";
            }
        }

        void request_site_package_catalog(EditorState& editor, bool force)
        {
            if (editor.packageCatalogPending
                || (!force && editor.packageCatalogRequested))
                return;
            editor.packageCatalogRequested = true;
            editor.packageCatalogStatus = "Loading package catalog from the Epoch Site...";
            editor.packageCatalogPending.emplace(std::async(
                std::launch::async,
                [] { return package_catalog::fetch(); }));
        }

        void poll_site_package_catalog(EditorState& editor)
        {
            if (!editor.packageCatalogPending
                || editor.packageCatalogPending->wait_for(0ms)
                    != std::future_status::ready)
                return;
            package_catalog::Snapshot snapshot =
                editor.packageCatalogPending->get();
            editor.packageCatalogPending.reset();
            if (!snapshot.ok)
            {
                editor.packageCatalogEntries.clear();
                editor.packageCatalogRevision.clear();
                editor.selectedPackageId.clear();
                editor.packageCatalogStatus = snapshot.message;
                editor.packageInstallStatus =
                    "Catalog unavailable. Retry after the Site publishes a valid catalog.";
                editor.packageInstallProgress = 0.0f;
                return;
            }

            // Repository/library authorities and kernel-owned Arcade are not
            // user-installable package rows. The Site publishes actual project
            // add-ons and model artifacts beneath those authorities.
            std::erase_if(snapshot.entries, [](const package_catalog::Entry& entry)
            {
                return entry.id == package_registry::kEngineArcadePackageId
                    || package_registry::is_catalog_authority(entry.id);
            });
            // Canonical labels for exact known IDs do not change catalog
            // availability, source identity or any download/activation gate.
            for (auto& entry : snapshot.entries)
            {
                if (const auto* package = package_registry::find(entry.id))
                {
                    entry.display_name = package->displayName;
                    entry.summary = package->summary;
                }
            }
            editor.packageCatalogEntries = std::move(snapshot.entries);
            editor.packageCatalogRevision = std::move(snapshot.revision);
            editor.packageCatalogStatus = epochengine::format_text(
                "Site catalog {} loaded ({} package{}).",
                editor.packageCatalogRevision,
                editor.packageCatalogEntries.size(),
                editor.packageCatalogEntries.size() == 1u ? "" : "s");
            if (editor.packageCatalogEntries.empty())
                editor.selectedPackageId.clear();
            else if (std::none_of(
                editor.packageCatalogEntries.begin(),
                editor.packageCatalogEntries.end(),
                [&](const package_catalog::Entry& entry)
                {
                    return entry.id == editor.selectedPackageId;
                }))
                editor.selectedPackageId = editor.packageCatalogEntries.front().id;
        }
        enum class ProjectAiProviderMode : std::uint8_t
        {
            disabled,
            epoch_local,
            external_mcp,
            engine_selected
        };

        [[nodiscard]] bool write_project_ai_profile(
            EditorState& editor,
            ProjectAiProviderMode mode)
        {
            namespace fs = std::filesystem;
            namespace profile = epochengine::ai::project_profile;
            const auto fail = [&](std::string message)
            {
                editor.packageInstallStatus = "Project AI: " + std::move(message);
                editor.packageInstallProgress = 0.0f;
                return false;
            };
            if (editor.projectId.empty() || editor.projectRoot.empty())
                return fail("Open or create a project before changing its AI setting.");
            if (editor.projectBuildPending || editor.projectRuntimeProcess.valid())
                return fail("Stop the project build or runtime before changing its AI setting.");
            const fs::path root = resolve_editor_path(fs::path{editor.projectRoot});
            const fs::path manifest = root / "project.epoch.json";
            const auto manifestBefore = read_bounded_package_file(manifest, 1024u * 1024u);
            const auto identity = editor_inspect_project_manifest_identity(
                manifest.generic_string(), editor.projectId, root.generic_string());
            if (!identity.succeeded || manifestBefore.empty())
                return fail("The active project identity could not be verified.");
            profile::Provider provider{};
            switch (mode)
            {
            case ProjectAiProviderMode::disabled: provider = profile::Provider::disabled; break;
            case ProjectAiProviderMode::epoch_local: provider = profile::Provider::epoch_local_qwen38; break;
            case ProjectAiProviderMode::external_mcp: provider = profile::Provider::external_mcp; break;
            case ProjectAiProviderMode::engine_selected: provider = profile::Provider::engine_selected; break;
            default: return fail("This project AI provider is not supported.");
            }
            const auto encoded = profile::serialize_profile(profile::make_profile(provider));
            if (!encoded) return fail(encoded.status);
            std::error_code error;
            const fs::path canonicalRoot = fs::canonical(root, error);
            if (error) return fail("The project directory is unavailable.");
            fs::path directory = canonicalRoot;
            for (const auto* part : {"Assets", "AI"})
            {
                directory /= part;
                auto status = fs::symlink_status(directory, error);
                if (error == std::errc::no_such_file_or_directory) error.clear();
                if (error || fs::is_symlink(status))
                    return fail("The project AI directory must be an ordinary project folder.");
                if (!fs::exists(status))
                {
                    if (!fs::create_directory(directory, error) || error)
                        return fail("The project AI directory could not be created.");
                }
                else if (!fs::is_directory(status))
                    return fail("The project AI directory is not a folder.");
                const auto canonical = fs::canonical(directory, error);
                const auto relative = error ? fs::path{} : canonical.lexically_relative(canonicalRoot);
                if (error || relative.empty() || relative.is_absolute()
                    || *relative.begin() == fs::path{".."})
                    return fail("The project AI directory is outside the active project.");
                directory = canonical;
            }
            const fs::path profilePath = directory / "project_ai.epochai";
            auto status = fs::symlink_status(profilePath, error);
            if (error == std::errc::no_such_file_or_directory) error.clear();
            if (error || fs::is_symlink(status)
                || (fs::exists(status) && !fs::is_regular_file(status)))
                return fail("The existing project AI profile is not an ordinary file.");
            const bool existed = fs::exists(status);
            const auto original = existed ? read_bounded_package_file(profilePath, 32u * 1024u) : std::string{};
            if (existed && !profile::parse_profile(original))
                return fail("The existing project AI profile is invalid; it was retained.");
            const fs::path temporary = directory / ("project_ai.write."
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            const auto bytes = std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(encoded.canonical_bytes.data()), encoded.canonical_bytes.size()};
            if (!platform::filesystem::exclusive_create_and_write(temporary, bytes, error))
                return fail("The project AI profile could not create its temporary file.");
            const auto cleanup = [&]
            {
                std::error_code ignored;
                (void)fs::remove(temporary, ignored);
            };
            const auto currentStatus = fs::symlink_status(profilePath, error);
            if (error == std::errc::no_such_file_or_directory) error.clear();
            if (error || fs::is_symlink(currentStatus) || fs::exists(currentStatus) != existed
                || (existed && read_bounded_package_file(profilePath, 32u * 1024u) != original)
                || read_bounded_package_file(manifest, 1024u * 1024u) != manifestBefore
                || !platform::filesystem::atomic_replace_same_filesystem(temporary, profilePath, error))
            {
                cleanup();
                return fail("The profile could not be saved atomically; the previous setting was retained.");
            }
            editor.packageInstallStatus = provider == profile::Provider::disabled
                ? "Project AI disabled. No models were stopped or removed."
                : provider == profile::Provider::engine_selected
                    ? "Project AI enabled using the engine-selected model. Confirm a model for each session; no weights were copied or loaded."
                    : provider == profile::Provider::epoch_local_qwen38
                        ? "Project AI enabled using the installed local Qwen provider. No model was loaded or weights copied."
                        : "Project AI enabled using the operator-selected external provider. Confirm its model and connection; no connection was started.";
            editor.packageInstallProgress = provider == profile::Provider::disabled ? 0.0f : 1.0f;
            push_editor_log(editor, "[package] " + editor.packageInstallStatus);
            return true;
        }

        [[nodiscard]] bool stage_model_package_opt_in(
            EditorState& editor,
            const epochengine::package_registry::PackageDescriptor& package)
        {
            if (editor.projectRoot.empty())
            {
                editor.packageInstallStatus = "No active project root for model package opt-in.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string safeId = safe_package_artifact_id(package.id);
            const auto installPlan = ai::model_install::plan_for(package.id);
            if (!installPlan)
            {
                editor.packageInstallStatus =
                    "This build has no pinned model artifact plan for that package.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }
            const std::filesystem::path projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            const std::filesystem::path packageDir = projectRoot / "assets" / "packages";
            const std::filesystem::path manifestPath = packageDir / (safeId + ".model.package.json");
            const std::filesystem::path modelCacheDir =
                resolve_editor_path(std::filesystem::path{ epochengine::ai::local_model_root() }) / safeId;
            const std::filesystem::path downloadPlanPath = modelCacheDir / "install.request.json";

            std::error_code ec;
            std::filesystem::create_directories(packageDir, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not create project package directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            ec.clear();
            std::filesystem::create_directories(modelCacheDir, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not create executable-local model cache directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string packageId = editor_json_escape(package.id);
            const std::string displayName = editor_json_escape(package.displayName);
            const std::string repo = editor_json_escape(package.externalSourceRepo);
            const std::string cacheRoot = editor_json_escape(epochengine::ai::local_model_root());
            const std::string cacheDir = editor_json_escape(modelCacheDir.generic_string());

            {
                std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write project model package manifest.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.model.package.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"display_name\": \"" << displayName << "\",\n"
                    << "  \"type\": \"os_model_asset\",\n"
                    << "  \"source_repo\": \"" << repo << "\",\n"
                    << "  \"revision\": \"" << installPlan->revision << "\",\n"
                    << "  \"cache_root\": \"" << cacheRoot << "\",\n"
                    << "  \"download_policy\": \"operator_demand_only\",\n"
                    << "  \"include_in_project\": true,\n"
                    << "  \"weights_bundled\": false,\n"
                    << "  \"requires_license_notice_review\": true\n"
                    << "}\n";
            }

            {
                std::ofstream out(downloadPlanPath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write model install request.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.model.install.request.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"source_repo\": \"" << repo << "\",\n"
                    << "  \"target_dir\": \"" << cacheDir << "\",\n";
                out << "  \"artifacts\": [\n";
                for (std::size_t index = 0u;
                    index < installPlan->artifacts.size(); ++index)
                {
                    const auto& artifact = installPlan->artifacts[index];
                    out
                        << "    {\"file\": \""
                        << editor_json_escape(artifact.file)
                        << "\", \"source_url\": \""
                        << editor_json_escape(artifact.source_url)
                        << "\", \"bytes\": " << artifact.bytes
                        << ", \"sha256\": \"" << artifact.sha256 << "\"}"
                        << (index + 1u == installPlan->artifacts.size()
                            ? "\n" : ",\n");
                }
                out
                    << "  ],\n"
                    << "  \"transfer\": \"operator_requested\",\n"
                    << "  \"human_approval_required\": true,\n"
                    << "  \"engine_iteration_clone\": false,\n"
                    << "  \"weights_bundled_with_project\": false,\n"
                    << "  \"notes\": \"The Package Manager transfers only this compiled pinned plan, verifies exact bytes, and never activates or runs the model.\"\n"
                    << "}\n";
            }

            editor.packageInstallStatus =
                "Model add-on plan staged; verified transfer is starting.";
            editor.packageInstallProgress = 0.0f;
            append_project_note(
                editor,
                "Stage OS Model Package",
                std::string("Staged ") + std::string(package.displayName) + " as an explicit project model package opt-in.",
                "Weights remain outside source and are downloaded only after an operator-approved package download step.");
            push_editor_log(editor, "[package] Wrote model package manifest: " + display_project_path(manifestPath));
            return true;
        }

        [[nodiscard]] bool stage_local_ai_runtime_opt_in(
            EditorState& editor,
            const epochengine::package_registry::PackageDescriptor& package)
        {
            const std::filesystem::path modelRoot =
                resolve_editor_path(std::filesystem::path{ epochengine::ai::local_model_root() });
            const std::filesystem::path packageRoot =
                modelRoot.parent_path() / "packages" / safe_package_artifact_id(package.id);
            const std::filesystem::path planPath =
                packageRoot / "install.request.json";
            const std::filesystem::path downloadRoot =
                packageRoot / "downloads";
            const std::filesystem::path binaryRoot =
                resolve_editor_path(std::filesystem::path{
                    epochengine::ai::epoch_local_llama_cpp_executable()})
                    .parent_path();

            std::error_code ec;
            std::filesystem::create_directories(downloadRoot, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not stage executable-local llama.cpp package directories.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            std::ofstream out(planPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                editor.packageInstallStatus = "Could not write the llama.cpp install request.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            out
                << "{\n"
                << "  \"schema\": \"epoch.local_ai.runtime.install.request.v1\",\n"
                << "  \"package_id\": \"" << editor_json_escape(package.id) << "\",\n"
                << "  \"runtime_source\": \"https://github.com/ggml-org/llama.cpp\",\n"
                << "  \"runtime_license\": \"MIT\",\n"
                << "  \"release\": \""
                << epochengine::ai::kEpochLocalLlamaCppRelease << "\",\n"
                << "  \"revision\": \""
                << epochengine::ai::kEpochLocalLlamaCppRevision << "\",\n"
                << "  \"artifact\": \""
                << epochengine::ai::kEpochLocalLlamaCppArtifact << "\",\n"
                << "  \"artifact_sha256\": \""
                << epochengine::ai::kEpochLocalLlamaCppArtifactSha256
                << "\",\n"
                << "  \"download_dir\": \"" << editor_json_escape(downloadRoot.generic_string()) << "\",\n"
                << "  \"binary_dir\": \"" << editor_json_escape(binaryRoot.generic_string()) << "\",\n"
                << "  \"model_dir\": \"" << editor_json_escape(modelRoot.generic_string()) << "\",\n"
                << "  \"installer\": \"Tools/models/install_epoch_qwen38.ps1\",\n"
                << "  \"network_action\": \"human_approval_required\",\n"
                << "  \"invocation\": \"epoch_owned_child_process\",\n"
                << "  \"auto_run\": false,\n"
                << "  \"server_or_listener\": false,\n"
                << "  \"external_provider_preserved\": true,\n"
                << "  \"status\": \"awaiting_operator_approved_transfer\"\n"
                << "}\n";
            out.close();

            const auto installed =
                epochengine::ai::epoch_local_ai_install_status();
            editor.packageInstallStatus = installed.ready()
                ? "Epoch-local llama.cpp and Qwen3.8 are installed."
                : "Pinned llama.cpp install request staged; transfer awaits explicit operator approval.";
            editor.packageInstallProgress =
                installed.ready() ? 1.0f : 0.05f;
            push_editor_log(editor, "[package] Wrote local AI runtime plan: " + display_project_path(planPath));
            return true;
        }
        [[nodiscard]] bool stage_forest_factory_package_opt_in(
            EditorState& editor,
            const epochengine::package_registry::PackageDescriptor& package)
        {
            if (editor.projectRoot.empty())
            {
                editor.packageInstallStatus =
                    "No active project root for Forest Factory package activation.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }
            if (!ensure_plant_lab_compiled_asset(editor))
            {
                editor.packageInstallStatus =
                    "Plant Lab asset compilation failed: "
                    + editor.plantLabStatus;
                editor.packageInstallProgress = 0.0f;
                return false;
            }
            if (!publish_plant_lab_project_asset(editor))
            {
                editor.packageInstallStatus = editor.plantLabAssetStatus;
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const auto& document = editor.plantLabDocument;
            const auto& profile = document.profile;
            const auto& compiled = editor.plantLabCompiled;
            const auto& occupancy = compiled.voxelOccupancy;
            const std::string safeId = safe_package_artifact_id(package.id);
            const std::filesystem::path projectRoot =
                resolve_editor_path(std::filesystem::path{editor.projectRoot});
            const std::filesystem::path packageDir =
                projectRoot / "assets" / "packages";
            const std::filesystem::path forestDir = packageDir / safeId;
            const std::filesystem::path manifestPath =
                packageDir / (safeId + ".package.json");
            const std::filesystem::path profilePath =
                forestDir / "default.forest.json";

            std::error_code ec;
            std::filesystem::create_directories(forestDir, ec);
            if (ec)
            {
                editor.packageInstallStatus =
                    "Could not create Forest Factory package directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string packageId = editor_json_escape(package.id);
            const std::string displayName =
                editor_json_escape(package.displayName);
            const std::string sourceRepo =
                editor_json_escape(package.externalSourceRepo);
            const std::string referenceRepo =
                editor_json_escape(
                    epochengine::forest::kForestFactoryReferenceRepo);
            const std::string profileFile =
                editor_json_escape(profilePath.generic_string());
            const std::string canonicalSourceFile = editor_json_escape(
                std::string{kDefaultPlantLabLogicalPath});
            const std::string compiledArtifactFile = editor_json_escape(
                editor.plantLabPublishedArtifact.storage_path
                    .lexically_relative(projectRoot)
                    .generic_string());

            {
                std::ofstream out(
                    manifestPath,
                    std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus =
                        "Could not write Forest Factory package manifest.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.core.package.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"display_name\": \"" << displayName << "\",\n"
                    << "  \"type\": \"core_opt_in\",\n"
                    << "  \"source_repo\": \"" << sourceRepo << "\",\n"
                    << "  \"reference_repo\": \"" << referenceRepo << "\",\n"
                    << "  \"editor_workspace\": \"Forest Factory\",\n"
                    << "  \"authoring_workspace\": \"Plant Lab\",\n"
                    << "  \"activation\": "
                       "\"main_scene_use_or_explicit_package_install\",\n"
                    << "  \"project_payload_policy\": "
                       "\"emit compiled descriptors only after visible "
                       "package activation\",\n"
                    << "  \"default_profile\": \"" << profileFile << "\",\n"
                    << "  \"canonical_source\": \"" << canonicalSourceFile << "\",\n"
                    << "  \"compiled_artifact\": \"" << compiledArtifactFile << "\",\n"
                    << "  \"runtime_outputs\": ["
                       "\"preview_skeleton\", \"voxel_lod_plan\", "
                       "\"voxel_occupancy\"],\n"
                    << "  \"requires_human_build_gate\": true,\n"
                    << "  \"server_or_listener_allowed\": false\n"
                    << "}\n";
                out.flush();
                if (!out)
                {
                    editor.packageInstallStatus =
                        "Forest Factory package manifest write was incomplete.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }
            }

            {
                std::ofstream out(
                    profilePath,
                    std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus =
                        "Could not write the current Plant Lab profile.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.forest.profile.v2\",\n"
                    << "  \"name\": \""
                    << editor_json_escape(document.name) << "\",\n"
                    << "  \"genome_hash\": "
                    << document.genome.stableHash << ",\n"
                    << "  \"source_revision\": "
                    << document.revision << ",\n"
                    << "  \"source_content_hash\": "
                    << document.contentHash << ",\n"
                    << "  \"history_cursor\": "
                    << document.historyCursor << ",\n"
                    << "  \"history_operations\": "
                    << document.journal.size() << ",\n"
                    << "  \"preset\": \""
                    << epochengine::forest::preset_name(profile.preset)
                    << "\",\n"
                    << "  \"mode\": \""
                    << (profile.previewMode ==
                            epochengine::forest::ForestPreviewMode::Mode3D
                        ? "3d"
                        : "2d")
                    << "\",\n"
                    << "  \"seed\": " << profile.seed.value << ",\n"
                    << "  \"technique\": "
                       "\"temporal_graph_parametric_l_system\",\n"
                    << "  \"profile\": {\n"
                    << "    \"target_height_m\": "
                    << profile.config.targetHeightMeters << ",\n"
                    << "    \"trunk_radius_m\": "
                    << profile.config.trunkRadiusMeters << ",\n"
                    << "    \"branch_levels\": "
                    << profile.branch.levels << ",\n"
                    << "    \"children_per_node\": "
                    << profile.branch.childrenPerNode << ",\n"
                    << "    \"branch_start_height_m\": "
                    << profile.branch.startHeightMeters << ",\n"
                    << "    \"branch_length_m\": "
                    << profile.branch.branchLengthMeters << ",\n"
                    << "    \"branch_angle_degrees\": "
                    << profile.branch.angleDegrees << ",\n"
                    << "    \"branch_spread_degrees\": "
                    << profile.branch.spreadDegrees << ",\n"
                    << "    \"growth_time_s\": "
                    << profile.temporal.timeSeconds << ",\n"
                    << "    \"growth_duration_s\": "
                    << profile.temporal.durationSeconds << "\n"
                    << "  },\n"
                    << "  \"compiled\": {\n"
                    << "    \"graph_content_hash\": "
                    << compiled.graph.content_hash << ",\n"
                    << "    \"preview_segments\": "
                    << compiled.preview.segmentCount << ",\n"
                    << "    \"preview_leaves\": "
                    << compiled.preview.leafCount << ",\n"
                    << "    \"voxel_lod_levels\": "
                    << compiled.voxelLods.levels.size() << ",\n"
                    << "    \"voxel_active_cells\": "
                    << occupancy.activeCells << ",\n"
                    << "    \"voxel_dense_bytes\": "
                    << occupancy.denseBytes << ",\n"
                    << "    \"voxel_trunk_cells\": "
                    << occupancy.trunkCells << ",\n"
                    << "    \"voxel_branch_cells\": "
                    << occupancy.branchCells << ",\n"
                    << "    \"voxel_foliage_cells\": "
                    << occupancy.foliageCells << "\n"
                    << "  },\n"
                    << "  \"source_repo\": \"" << sourceRepo << "\",\n"
                    << "  \"reference_repo\": \"" << referenceRepo << "\",\n"
                    << "  \"outputs\": {\n"
                    << "    \"preview_skeleton\": true,\n"
                    << "    \"voxel_lod_plan\": true,\n"
                    << "    \"voxel_occupancy\": true,\n"
                    << "    \"mesh_lod\": false,\n"
                    << "    \"impostor\": false\n"
                    << "  },\n"
                    << "  \"project_inclusion\": "
                       "\"explicit_package_activation_only\"\n"
                    << "}\n";
                out.flush();
                if (!out)
                {
                    editor.packageInstallStatus =
                        "Plant Lab profile write was incomplete.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }
            }

            ec.clear();
            const auto manifestBytes =
                std::filesystem::file_size(manifestPath, ec);
            if (ec || manifestBytes == 0u)
            {
                editor.packageInstallStatus =
                    "Forest Factory manifest verification failed.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }
            ec.clear();
            const auto profileBytes =
                std::filesystem::file_size(profilePath, ec);
            if (ec || profileBytes == 0u)
            {
                editor.packageInstallStatus =
                    "Plant Lab profile verification failed.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }
            ec.clear();
            const auto artifactBytes = std::filesystem::file_size(
                editor.plantLabPublishedArtifact.storage_path, ec);
            if (ec || artifactBytes == 0u)
            {
                editor.packageInstallStatus =
                    "Compiled Plant Lab Library artifact verification failed.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            editor.packageInstallStatus = epochengine::format_text(
                "Forest Factory package staged from Plant Lab revision {} "
                "with {} voxel LOD levels.",
                compiled.sourceRevision,
                compiled.voxelLods.levels.size());
            editor.packageInstallProgress = 0.65f;
            append_project_note(
                editor,
                "Stage Forest Factory Package",
                epochengine::format_text(
                    "Staged Plant Lab revision {} and its verified compiled "
                    "forest descriptor.",
                    compiled.sourceRevision),
                "Plant Lab owns temporal source edits; Forest Factory "
                "consumes the compiled preview and voxel LOD descriptor only "
                "after explicit project activation.");
            push_editor_log(
                editor,
                epochengine::format_text(
                    "[package] Staged Plant Lab revision {} (content "
                    "{:016x}) from canonical source {} and Library artifact {}.",
                    compiled.sourceRevision,
                    compiled.sourceContentHash,
                    std::string{kDefaultPlantLabLogicalPath},
                    display_project_path(
                        editor.plantLabPublishedArtifact.storage_path)));
            return true;
        }

        [[nodiscard]] bool builtin_project_package_ready(std::string_view packageId) noexcept
        {
            return packageId == package_registry::kEngineForestFactoryPackageId
                || packageId == package_registry::kEngineArcadePackageId;
        }

        void add_builtin_project_package(EditorState& editor,
            const std::shared_ptr<core::Context>& context,
            const package_registry::PackageDescriptor& package)
        {
            if (!builtin_project_package_ready(package.id)
                || package_source_task_busy(editor) || planetary_project_busy(editor)) return;
            if (editor.projectRoot.empty())
            {
                const auto created = editor_create_project_shell(
                    EditorProjectKind::Game, EditorProjectInputProvision::project_default);
                editor.packageInstallStatus = created.summary;
                if (!created.succeeded) return;
                const auto admitted = editor_admit_project_manifest(created.manifest_path);
                if (!admitted.succeeded || !editor_open_project(context, admitted.project_id))
                {
                    editor.packageInstallStatus = "Project created at " + created.root_path
                        + ", but it could not be opened. " + admitted.summary;
                    return;
                }
            }
            if (package.id == package_registry::kEngineForestFactoryPackageId)
            {
                if (!stage_forest_factory_package_opt_in(editor, package)) return;
                editor.mainSurface = EditorMainSurface::ForestFactory;
                editor.workspaceCompanionsPending = true;
                editor.projectStatus = "Forest Factory added. Edit the plant preview, then choose Add to Scene and Save Project.";
            }
            else
            {
                const auto staged = editor_stage_engine_arcade_project(editor.projectRoot);
                editor.packageInstallStatus = staged.summary;
                if (!staged.succeeded) return;
                editor.projectStatus = staged.summary;
            }
            editor.packageInstallProgress = 1.0f;
            editor.showPackageManagerModal = false;
            push_editor_log(editor, "[package] " + editor.packageInstallStatus);
        }

        [[nodiscard]] bool ai_project_package_request_current(const EditorState& editor) noexcept
        {
            return editor.aiPackageRequest && editor.aiPackageRequest->project_id == editor.projectId
                && editor.aiPackageRequest->project_root == editor.projectRoot
                && editor.aiPackageRequest->project_key == planetary_project_key(editor)
                && editor.aiPackageRequest->project_generation
                    == editor.projectLifecycle.evidence().generations.selected_project.generation;
        }

        void finish_ai_project_package_request(EditorState& editor, std::string status, bool succeeded)
        {
            if (editor.aiGoalTools.enabled && editor.aiGoalTools.waiting
                && editor.aiGoalTools.operation() == "project.create")
                editor.aiGoalPackageReceipt = succeeded;
            editor.aiToolStatus = std::move(status);
            editor.aiPackageRequest.reset();
            push_editor_log(editor, "[ai-package] " + editor.aiToolStatus);
        }

        [[nodiscard]] bool ai_package_generation_awaiting_attachment(const EditorState& editor) noexcept
        {
            return ai_project_package_request_current(editor)
                && editor.aiPackageRequest->generate
                && editor.aiPackageRequest->phase == EditorState::AiPackageRequest::Phase::review;
        }

        enum class AiProjectPackageOperation : unsigned char { none, catalog, install, generate };
        struct AiProjectPackageIntent final
        {
            AiProjectPackageOperation operation{AiProjectPackageOperation::none};
            bool clear_request{};
        };

        [[nodiscard]] std::string ai_project_package_lower(std::string_view value)
        {
            std::string result;result.reserve(value.size());
            for(const unsigned char ch:value)result.push_back(static_cast<char>(ch>='A'&&ch<='Z'?ch-'A'+'a':ch));
            return result;
        }
        [[nodiscard]] std::string ai_project_package_unquoted(std::string_view value)
        {
            std::string result;result.reserve(value.size());char quote{};
            for(const char ch:value)
            {
                if(quote){if(ch==quote)quote=0;result.push_back(' ');}
                else if(ch=='\''||ch=='"'){quote=ch;result.push_back(' ');}
                else result.push_back(ch);
            }
            return result;
        }
        [[nodiscard]] bool ai_project_package_word(std::string_view text,std::string_view word) noexcept
        {
            if(word.empty())return false;
            const auto wordChar=[](char ch){return (ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_';};
            for(auto at=text.find(word);at!=std::string_view::npos;at=text.find(word,at+1u))
                if((at==0u||!wordChar(text[at-1u]))&&(at+word.size()==text.size()||!wordChar(text[at+word.size()])))return true;
            return false;
        }
        [[nodiscard]] AiProjectPackageIntent ai_project_package_intent(std::string_view request,bool registered_target=false)
        {
            const auto lower=ai_project_package_lower(request);
            std::string_view command=lower;
            const auto trim=[](std::string_view value){
                while(!value.empty()&&(value.front()==' '||value.front()=='\t'||value.front()=='\r'||value.front()=='\n'))value.remove_prefix(1u);
                return value;
            };
            command=trim(command);
            for(unsigned pass=0;pass<2u;++pass)
                for(const auto prefix:{"please ","can you ","could you ","would you ","i want you to "})
                    if(command.starts_with(prefix)){command=trim(command.substr(std::string_view{prefix}.size()));break;}
            for(const auto phrase:{"list packages","list available packages","list extensions","show package catalog",
                "show me the package catalog","show available packages","what packages are available","which packages are available"})
                if(command.starts_with(phrase))return {AiProjectPackageOperation::catalog,true};
            const auto end=command.find_first_of(" \t\r\n");
            const auto verb=command.substr(0,end);
            const auto any=[&](std::initializer_list<std::string_view> words){
                return std::ranges::any_of(words,[&](auto word){return ai_project_package_word(lower,word);});
            };
            const bool package=verb=="install"||(any({"install","add","enable","activate"})
                &&(registered_target||any({"package","packages","extension","extensions","planetary_terrain","forest_factory","engine_arcade",
                    "planetary terrain","forest factory","plant lab"})));
            const bool terrain=any({"generate","create","make"})
                &&(any({"terrain","planetary"})||(any({"generate"})&&any({"planet","universe"})));
            const bool leadingPackage=verb=="install"||verb=="add"||verb=="enable"||verb=="activate";
            const auto operation=package&&leadingPackage?AiProjectPackageOperation::install
                :terrain?AiProjectPackageOperation::generate:package?AiProjectPackageOperation::install:AiProjectPackageOperation::none;
            if(operation==AiProjectPackageOperation::none)return {};
            // Classification recognizes mentions for routing, while semantic
            // admission constrains only an affirmative, leading operation.
            // Negated clauses and quoted examples must not turn Save into Add.
            const bool leading=operation==AiProjectPackageOperation::install
                ?leadingPackage
                :verb=="generate"||verb=="create"||verb=="make";
            auto object=end==std::string_view::npos?std::string_view{}:trim(command.substr(end));
            const auto unquotedObject=ai_project_package_unquoted(object);
            object=trim(unquotedObject);
            for(unsigned pass=0;pass<3u;++pass)
                for(const auto article:{"a ","an ","the ","new ","optional ","bundled ","registered "})
                    if(object.starts_with(article)){object=trim(object.substr(std::string_view{article}.size()));break;}
            const bool projectPackage=object.starts_with("project package")||object.starts_with("project extension");
            bool otherObject{};
            if(!projectPackage)
                for(const auto noun:{"text","label","button","cube","object","note","comment","script","file","document","theme","scene","world","project","words","string","example","message","log","description","title","caption"})
                    if(object.starts_with(noun)&&(object.size()==std::string_view{noun}.size()
                        ||object[std::string_view{noun}.size()]==' '))otherObject=true;
            bool operationObject{};
            for(const auto word:operation==AiProjectPackageOperation::install
                ?std::initializer_list<std::string_view>{"package","packages","extension","extensions","planetary_terrain","forest_factory","engine_arcade","planetary terrain","forest factory","plant lab"}
                :std::initializer_list<std::string_view>{"terrain","planetary","planet","universe"})
                operationObject=operationObject||ai_project_package_word(object,word);
            return {operation,leading&&!otherObject&&(operationObject||registered_target||(verb=="install"&&!object.empty()))};
        }

        [[nodiscard]] const package_registry::PackageDescriptor* ai_project_requested_package(std::string_view request)
        {
            const auto lower=ai_project_package_lower(request);
            const auto end=lower.find_first_of(".;\n");
            const std::string_view primary{lower.data(),end==std::string::npos?lower.size():end};
            const package_registry::PackageDescriptor* found{};
            for(const auto& package:package_registry::known_packages())
            {
                if(package_registry::catalog_section(package.id)!=package_registry::CatalogSection::ProjectPackages)continue;
                bool match=ai_project_package_word(primary,package.id)
                    ||ai_project_package_word(primary,ai_project_package_lower(package.displayName));
                for(const auto& binding:package_registry::kSourceManifestBindings)
                    if(binding.registry_id==package.id&&ai_project_package_word(primary,binding.manifest_id))match=true;
                if(!match)continue;
                if(found&&found->id!=package.id)return nullptr;
                found=&package;
            }
            return found;
        }

        // No value means keep the host-generated name. An empty value means
        // an explicit name was present but could not be read safely.
        [[nodiscard]] std::optional<std::string_view> ai_requested_project_display_name(std::string_view request)
        {
            const auto lower = ai_project_package_unquoted(ai_project_package_lower(request));
            if (const auto at = lower.rfind("\nuser clarification:"); at != std::string::npos)
            {
                auto answer = ai::authoring_plan_detail::trim(request.substr(at + 20u));
                const auto folded = ai_project_package_lower(answer);
                if (folded == "use the generated name" || folded == "use the generated name."
                    || folded == "use an automatic name" || folded == "use an automatic name.") return std::nullopt;
                if (folded.starts_with("name:")) return ai::authoring_plan_detail::trim(answer.substr(5u));
            }
            auto clause = std::string_view{lower}.substr(0u, lower.find_first_of(";\n"));
            clause = clause.substr(0u, clause.find(". "));
            auto marker = clause.find(" named ");
            if (const auto called = clause.find(" called "); called < marker) marker = called;
            if (marker == std::string_view::npos)
            {
                if (clause.ends_with(" named") || clause.ends_with(" called")) return std::string_view{};
                return std::nullopt;
            }

            auto name = ai::authoring_plan_detail::trim(request.substr(marker + 7u, clause.size() - marker - 7u));
            if (name.empty()) return std::string_view{};
            if (name.front() == '\'' || name.front() == '"')
            {
                const auto end = name.find(name.front(), 1u);
                if (end == std::string_view::npos) return std::string_view{};
                return name.substr(1u, end - 1u);
            }
            auto remainder = clause.substr(static_cast<std::size_t>(name.data() - request.data()));
            for (const auto suffix : {" in a new project", " with ", " using ", " as "})
                remainder = remainder.substr(0u, remainder.find(suffix));
            name = ai::authoring_plan_detail::trim(name.substr(0u, remainder.size()));
            if (name.ends_with('.')) name.remove_suffix(1u);
            return name;
        }

        [[nodiscard]] bool ai_project_creation_uses_defaults(std::string_view request)
        {
            if (ai::project_lifecycle_request_tool(request) != "project.create") return false;
            auto lower = ai_project_package_lower(request);
            if (const auto name = ai_requested_project_display_name(request); name && !name->empty())
                lower.replace(static_cast<std::size_t>(name->data() - request.data()), name->size(), name->size(), ' ');
            for (const auto marker : {"\nattachment", "\nreference", "\nfile content"})
                if (const auto at = lower.find(marker); at != std::string::npos) lower.resize(at);
            for (const auto refusal : {"without defaults", "without default settings", "no defaults", "not defaults",
                "do not use defaults", "do not use the defaults", "do not use default settings",
                "don't use defaults", "don't use the defaults", "don't use default settings",
                "do not choose for me", "don't choose for me"})
                if (lower.find(refusal) != std::string::npos) return false;
            const auto text = ai_project_package_unquoted(lower);
            for (const auto permission : {"using defaults", "using the defaults", "use defaults", "use the defaults",
                "with defaults", "with default settings", "using default settings", "use default settings", "choose for me"})
                if (ai_project_package_word(text, permission)) return true;
            return false;
        }

        [[nodiscard]] std::array<ai::McpArgument, 4> ai_project_creation_defaults()
        {
            const project_planetary::Settings settings{};
            std::string detail;
            constexpr std::array choices{
                std::pair{project_planetary::DetailPreset::compact, std::string_view{"compact"}},
                std::pair{project_planetary::DetailPreset::fast, std::string_view{"fast"}},
                std::pair{project_planetary::DetailPreset::detailed, std::string_view{"balanced"}},
                std::pair{project_planetary::DetailPreset::fine, std::string_view{"fine"}}};
            for (const auto& [preset, name] : choices)
                if (const auto candidate = project_planetary::with_detail_preset(settings, preset);
                    candidate && *candidate == settings)
                { detail = name; break; }
            // An unrepresentable future default remains invalid and needs a
            // choice; never substitute a different preset behind the user.
            return {{{"seed", std::to_string(settings.seed)},
                {"radius_m", std::to_string(settings.reference_radius_meters)},
                {"detail", std::move(detail)}, {"ocean", settings.ocean_surface ? "on" : "off"}}};
        }

        [[nodiscard]] std::string_view ai_project_coverage_for_target(std::string_view target)
        {
            target = target.substr(0u, target.find_first_of(".;\n"));
            for (const auto clause : {" not ", " but ", " then "})
                if (const auto at = target.find(clause); at != std::string_view::npos)
                    target = target.substr(0u, at);
            if (const auto at = target.find("coverage="); at != std::string_view::npos)
            {
                const auto value = target.substr(at + 9u);
                const auto token = value.substr(0u, value.find_first_of(" \t,"));
                if (token == "none") return "none";
                if (token == "planet") return "planet";
                if (token == "universe") return "universe";
                return "ambiguous";
            }
            const bool planet = ai_project_package_word(target,"planet")
                || ai_project_package_word(target,"planetary");
            const bool universe = ai_project_package_word(target,"universe");
            const bool empty = ai_project_package_word(target,"empty")
                || ai_project_package_word(target,"blank");
            const auto choices = static_cast<unsigned>(planet) + static_cast<unsigned>(universe)
                + static_cast<unsigned>(empty);
            if (choices > 1u) return "ambiguous";
            return planet ? "planet" : universe ? "universe" : empty ? "none" : "";
        }

        [[nodiscard]] std::string ai_adjacent_project_generation_clause(std::string_view tail)
        {
            using ai::authoring_plan_detail::trim;
            tail = trim(tail);
            std::string clause;
            while (!tail.empty())
            {
                const auto lineEnd = tail.find('\n');
                auto line = trim(tail.substr(0u, lineEnd));
                if (line.empty() || std::ranges::any_of(std::array{
                    "attachment", "reference", "quoted text:", "file content", "example:",
                    "user clarification:", "user correction:", "original project request:", "prior clarification"},
                    [&](auto prefix) { return line.starts_with(prefix); })) break;
                if (!clause.empty() && !ai::project_lifecycle_request_tool(line).empty()) break;
                const auto end = (std::min)(line.find(". "), line.find(';'));
                const bool finished = end != std::string_view::npos || line.ends_with('.');
                line = line.substr(0u, end);
                if (!clause.empty()) clause += ' ';
                clause += line;
                if (finished || lineEnd == std::string_view::npos) break;
                tail.remove_prefix(lineEnd + 1u);
            }
            if (!clause.starts_with("generate ")) return {};
            return clause;
        }

        [[nodiscard]] std::string_view ai_project_creation_coverage(std::string_view request)
        {
            if (ai::project_lifecycle_request_tool(request) != "project.create") return {};
            using ai::authoring_plan_detail::trim;
            using ai::project_request_detail::representation_object;
            using ai::project_request_detail::subject;
            auto text = ai_project_package_unquoted(ai_project_package_lower(request));
            if (const auto name = ai_requested_project_display_name(request); name && !name->empty())
            {
                const auto offset = static_cast<std::size_t>(name->data() - request.data());
                const auto before = trim(std::string_view{text}.substr(0u, offset));
                const auto begin = before.ends_with(" named") || before.ends_with(" called")
                    ? static_cast<std::size_t>(before.data() - text.data()) + before.size() - 6u : offset;
                text.replace(begin, offset + name->size() - begin, offset + name->size() - begin, ' ');
            }
            const auto view = trim(text);
            const auto firstEnd = view.find_first_of(".;\n");
            auto coverage = ai_project_coverage_for_target(representation_object(view.substr(0u, firstEnd)));

            // A compound game request may declare the world in its next
            // instruction. Only a direct terrain-generation subject qualifies.
            if (coverage.empty() && firstEnd != std::string_view::npos)
            {
                const auto next = ai_adjacent_project_generation_clause(view.substr(firstEnd + 1u));
                if (!next.empty() && !ai::explicit_generated_asset_request(next))
                {
                    auto target = representation_object(next);
                    while (target.starts_with("one ") || target.starts_with("single ")
                        || target.starts_with("custom ") || target.starts_with("seeded "))
                        target = trim(target.substr(target.find(' ') + 1u));
                    if (subject(target, "planet") || subject(target, "universe"))
                        coverage = ai_project_coverage_for_target(target);
                }
            }
            if (!coverage.empty() && coverage != "ambiguous") return coverage;

            constexpr std::string_view clarification = "\nuser clarification:";
            if (const auto at = view.rfind(clarification); at != std::string_view::npos)
            {
                auto answer = trim(view.substr(at + clarification.size()));
                if (answer.starts_with("use ")) answer.remove_prefix(4u);
                if (answer.starts_with("coverage=") || subject(answer,"empty project")
                    || subject(answer,"one planet") || subject(answer,"single planet")
                    || subject(answer,"a universe") || subject(answer,"universe"))
                    return ai_project_coverage_for_target(answer);
            }
            // The clarification router can retain the original operator text
            // below a corrected leading action. Honor its explicit field.
            constexpr std::string_view original = "\noriginal project request:";
            if (coverage.empty())
                if (const auto at = view.find(original); at != std::string_view::npos)
                {
                    auto objective = view.substr(at + original.size());
                    objective = objective.substr(0u, objective.find_first_of(".;\n"));
                    if (const auto field = objective.find(" coverage="); field != std::string_view::npos)
                        return ai_project_coverage_for_target(objective.substr(field + 1u));
                }
            return coverage;
        }

        [[nodiscard]] std::string_view ai_project_creation_ocean(std::string_view request)
        {
            if (ai::project_lifecycle_request_tool(request) != "project.create") return {};
            auto text = ai_project_package_unquoted(ai_project_package_lower(request));
            if (const auto name = ai_requested_project_display_name(request); name && !name->empty())
                text.replace(static_cast<std::size_t>(name->data() - request.data()), name->size(), name->size(), ' ');
            if (const auto at = text.rfind("\nuser clarification:"); at != std::string::npos)
                text.erase(0u, at + 20u);
            // Reference descriptions and attached prose are not project settings.
            for (const auto marker : {"\nattachment", "\nreference", "\nfile content", "\nread prompt.txt"})
                if (const auto at = text.find(marker); at != std::string::npos) text.resize(at);
            std::string_view choice;
            std::size_t begin{};
            while (begin < text.size())
            {
                const auto end = text.find_first_of(".;\n", begin);
                auto clause = text.substr(begin, end == std::string::npos ? end : end - begin);
                begin = end == std::string::npos ? text.size() : end + 1u;
                if (std::ranges::any_of(std::array{"reference", "photo", "image", "example", "attachment"},
                    [&](auto word) { return ai_project_package_word(clause, word); })) continue;
                // Consume longer negations before their shorter positive forms.
                constexpr std::pair<std::string_view, std::string_view> phrases[]{
                    {"do not disable the ocean","on"}, {"do not disable ocean","on"},
                    {"do not enable the ocean","off"}, {"do not enable ocean","off"},
                    {"ocean not enabled","off"}, {"ocean not disabled","on"},
                    {"ocean=on","on"}, {"ocean=off","off"},
                    {"ocean enabled","on"}, {"ocean disabled","off"},
                    {"ocean on","on"}, {"ocean off","off"},
                    {"enable the ocean","on"}, {"disable the ocean","off"},
                    {"enable ocean","on"}, {"disable ocean","off"},
                    {"without an ocean","off"}, {"without ocean","off"},
                    {"no ocean","off"}, {"with an ocean","on"}, {"with ocean","on"}};
                for (const auto& [phrase, value] : phrases)
                    for (auto at = clause.find(phrase); at != std::string::npos; at = clause.find(phrase))
                    {
                        const bool boundary = (at == 0u || !ai::project_request_detail::word_character(clause[at - 1u]))
                            && (at + phrase.size() == clause.size()
                                || !ai::project_request_detail::word_character(clause[at + phrase.size()]));
                        if (boundary)
                        {
                            if (!choice.empty() && choice != value) return "conflicting";
                            choice = value;
                        }
                        clause.replace(at, phrase.size(), phrase.size(), ' ');
                    }
            }
            return choice;
        }

        [[nodiscard]] bool ai_project_initial_landing_forest(std::string_view request)
        {
            if (ai_project_creation_coverage(request) != "planet") return false;
            auto text = ai_project_package_unquoted(ai_project_package_lower(request));
            text.erase(std::remove(text.begin(),text.end(),'\r'),text.end());
            // Only the initial world-generation paragraph can request this
            // stage. The separate custom tree and later reference prose
            // cannot choose the recipe for the original forest.
            if (const auto end = text.find("\n\n"); end != std::string::npos) text.resize(end);
            std::replace(text.begin(),text.end(),'\n',' ');
            using ai::authoring_plan_detail::trim;
            auto clause = std::string_view{text};
            const auto openingEnd = clause.find(". ");
            if (openingEnd == std::string_view::npos) return false;
            clause = trim(clause.substr(openingEnd + 2u));
            const auto terrainEnd = clause.find(". ");
            if (terrainEnd == std::string_view::npos) return false;
            const auto terrain = ai_adjacent_project_generation_clause(clause);
            if (terrain.empty() || ai::explicit_generated_asset_request(terrain)
                || ai_project_coverage_for_target(terrain) != "planet") return false;
            clause = trim(clause.substr(terrainEnd + 2u));
            for (const std::string_view prefix : {"as part of that generation, ",
                "use plant lab recipes to create native plants and forest factory to "})
                if (clause.starts_with(prefix)) clause.remove_prefix(prefix.size());
            clause = trim(clause.substr(0u,clause.find_first_of(".;?!")));
            return clause == "plant a procedural forest near the landing area"
                || clause == "plant a forest near the landing area";
        }

        constexpr std::array<std::string_view, 3u> ai_project_terrain_fields{"seed", "radius_m", "detail"};

        [[nodiscard]] std::string ai_project_terrain_value(std::string_view field, std::string_view value, bool kilometers = false)
        {
            if (value.empty()) return "?";
            if (field == "detail")
                return value == "compact" || value == "fast" || value == "balanced" || value == "fine"
                    ? std::string{value} : "?";
            if (field == "seed")
            {
                if (value == "random") return "random";
                std::uint32_t seed{};
                const auto end = value.data() + value.size();
                const auto parsed = std::from_chars(value.data(), end, seed);
                return parsed.ec == std::errc{} && parsed.ptr == end ? std::to_string(seed) : "?";
            }
            if (value == "earth") return "6371000";
            if (value == "moon") return "1737400";
            double radius{};
            const auto end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end, radius);
            if (kilometers) radius *= 1000.0;
            if (parsed.ec != std::errc{} || parsed.ptr != end || !std::isfinite(radius)
                || radius < 250.0 || radius > 100000000.0) return "?";
            std::array<char, 32u> normalized{};
            const auto written = std::to_chars(normalized.data(), normalized.data() + normalized.size(), radius, std::chars_format::fixed);
            return written.ec == std::errc{} ? std::string{normalized.data(), written.ptr} : "?";
        }

        using AiProjectTerrainChoices = std::array<std::string, ai_project_terrain_fields.size()>;

        void ai_read_project_terrain_choices(std::string_view text, AiProjectTerrainChoices& choices)
        {
            using ai::authoring_plan_detail::trim;
            const auto next_token = [](std::string_view& rest) {
                constexpr std::string_view separators = " \t\r\n,;";
                const auto start = rest.find_first_not_of(separators);
                if (start == std::string_view::npos) { rest = {}; return std::string_view{}; }
                rest.remove_prefix(start);
                const auto end = rest.find_first_of(separators);
                auto token = rest.substr(0u, end);
                rest.remove_prefix(token.size());
                if (token.ends_with('.') || token.ends_with(':')) token.remove_suffix(1u);
                return token;
            };
            text = trim(text);
            for (const auto stop : {" then ", " with reference ", " using a reference ", " based on ", " for example "})
                text = text.substr(0u, text.find(stop));
            if (text.starts_with("example") || text.starts_with("attachment") || text.starts_with("reference")) return;

            const auto record = [&](std::size_t index, std::string_view raw, bool ambiguous, bool kilometers = false) {
                auto value = ambiguous ? "?" : ai_project_terrain_value(ai_project_terrain_fields[index], raw, kilometers);
                auto& choice = choices[index];
                choice = choice.empty() || choice == value ? std::move(value) : "?";
            };
            std::string_view previous;
            auto token = next_token(text);
            auto next = next_token(text);
            while (!token.empty())
            {
                auto lookahead = text;
                const auto unit = next_token(lookahead);
                const bool alternative = previous == "or" || next == "or" || unit == "or";
                if (const auto equals = token.find('='); equals != std::string_view::npos)
                {
                    const auto field = std::ranges::find(ai_project_terrain_fields, token.substr(0u, equals));
                    if (field != ai_project_terrain_fields.end())
                        record(static_cast<std::size_t>(field - ai_project_terrain_fields.begin()),
                            token.substr(equals + 1u), previous == "or" || next == "or");
                }
                else if (token == "seed")
                    record(0u, previous == "random" ? previous : next, alternative);
                else if (token == "radius")
                {
                    const bool alias = previous == "earth" || previous == "moon";
                    const bool kilometers = unit == "km" || unit == "kilometers" || unit == "kilometres";
                    const bool meters = unit == "m" || unit == "meters" || unit == "metres";
                    record(1u, alias ? previous : next,
                        alternative || (!alias && ((!kilometers && !meters) || next_token(lookahead) == "or")), kilometers);
                }
                else if (token == "detail")
                {
                    const auto preset = ai_project_terrain_value("detail", previous);
                    record(2u, preset == "?" ? next : previous, alternative);
                }
                previous = token;
                token = next;
                next = next_token(text);
            }
        }

        [[nodiscard]] AiProjectTerrainChoices ai_requested_project_terrain(std::string_view request)
        {
            auto text = ai_project_package_lower(request);
            if (const auto name = ai_requested_project_display_name(request); name && !name->empty())
                text.replace(static_cast<std::size_t>(name->data() - request.data()), name->size(), name->size(), ' ');
            text = ai_project_package_unquoted(text);
            for (const auto stop : {"\nattachment", "\nreference", "\nfile content"})
                if (const auto at = text.find(stop); at != std::string::npos) text.resize(at);

            using ai::authoring_plan_detail::trim;
            const auto sentence_end = [](std::string_view line) { return (std::min)(line.find(". "), line.find('\n')); };
            const auto first = trim(text);
            const auto end = sentence_end(first);
            AiProjectTerrainChoices choices{};
            ai_read_project_terrain_choices(first.substr(0u, end), choices);
            if (end != std::string_view::npos)
            {
                const auto clause = ai_adjacent_project_generation_clause(first.substr(end + 1u));
                const auto coverage = ai_project_coverage_for_target(ai::project_request_detail::representation_object(clause));
                if ((coverage == "planet" || coverage == "universe")
                    && !ai::explicit_generated_asset_request(clause))
                    ai_read_project_terrain_choices(clause, choices);
            }
            constexpr std::string_view original = "\noriginal project request:";
            if (const auto at = first.find(original); at != std::string_view::npos)
            {
                const auto recovered = trim(first.substr(at + original.size()));
                ai_read_project_terrain_choices(recovered.substr(0u, sentence_end(recovered)), choices);
            }
            for (auto line = std::string_view{text}; !line.empty();)
            {
                const auto endOfLine = line.find('\n');
                const auto current = line.substr(0u, endOfLine);
                line = endOfLine == std::string_view::npos ? std::string_view{} : line.substr(endOfLine + 1u);
                for (const std::string_view prefix : {"user clarification:", "user correction:"})
                    if (current.starts_with(prefix))
                    {
                        AiProjectTerrainChoices revised{};
                        ai_read_project_terrain_choices(current.substr(prefix.size()), revised);
                        for (std::size_t index{}; index < revised.size(); ++index)
                            if (!revised[index].empty()) choices[index] = std::move(revised[index]);
                        break;
                    }
            }
            return choices;
        }

        enum class AiForestLocation { current_camera, needs_landing_location, ocean_shoreline };

        [[nodiscard]] AiForestLocation ai_forest_location(std::string_view request)
        {
            const auto unquoted = ai_project_package_unquoted(ai_project_package_lower(request));
            auto opening = ai::authoring_plan_detail::trim(std::string_view{unquoted}.substr(0u,unquoted.find_first_of(".;?!\r\n")));
            if (opening.starts_with("please ")) opening.remove_prefix(7u);
            if (ai::project_request_detail::subject(opening,"paint a forest at a verified ocean shoreline")
                || ai::project_request_detail::subject(opening,"paint forest at a verified ocean shoreline"))
                return AiForestLocation::ocean_shoreline;
            if (!ai_project_package_word(opening,"near the landing area")
                && !ai_project_package_word(opening,"at the landing area")) return AiForestLocation::current_camera;
            constexpr std::string_view marker = "\nuser clarification:";
            auto history = std::string_view{unquoted};
            while (true)
            {
                const auto at = history.rfind(marker);
                if (at == std::string_view::npos) break;
                auto answer = ai::authoring_plan_detail::trim(history.substr(at + marker.size()));
                if (answer.ends_with('.')) answer.remove_suffix(1u);
                if (answer == "use the current camera location") return AiForestLocation::current_camera;
                if (answer == "use a verified ocean shoreline") return AiForestLocation::ocean_shoreline;
                if (ai_project_package_word(answer,"camera") || ai_project_package_word(answer,"shoreline")) break;
                history = history.substr(0u,at);
            }
            return AiForestLocation::needs_landing_location;
        }

        [[nodiscard]] std::string ai_project_creation_question(std::string_view request)
        {
            const auto operation = ai::project_lifecycle_request_tool(request);
            if (operation == "forest.paint" && ai_forest_location(request) == AiForestLocation::needs_landing_location)
                return "The generated planet has no verified lakeside landing location for this forest. Choose 'Use a verified ocean shoreline' to search the ocean-bearing body, or move the camera over an inspected site and reply 'Use the current camera location'.";
            if (operation != "project.create" && operation != "forest.configure") return {};
            auto text = ai_project_package_lower(request);
            const auto name = operation == "project.create" ? ai_requested_project_display_name(request) : std::nullopt;
            if (name && !ai::valid_project_display_name(*name))
                return "Which display name should I use? Reply with 'Name: Your Project' (up to 64 letters, digits, spaces, hyphens or underscores), or 'Use the generated name'. Nothing was created.";
            if (name)
                text.replace(static_cast<std::size_t>(name->data() - request.data()), name->size(), name->size(), ' ');
            const auto unquoted = ai_project_package_unquoted(text);
            const auto has = [&](std::initializer_list<std::string_view> words) {
                return std::ranges::any_of(words,[&](auto word){return ai_project_package_word(unquoted,word);});
            };
            const auto answerAt = unquoted.rfind("\nuser clarification:");
            const std::string_view choices = answerAt == std::string::npos ? std::string_view{unquoted}
                : std::string_view{unquoted}.substr(answerAt + 20u);
            const auto chosen = [&](std::string_view phrase) { return ai_project_package_word(choices, phrase); };
            const bool approximation = choices.find("do not") == std::string_view::npos
                && choices.find("don't") == std::string_view::npos
                && (chosen("use the approximation") || chosen("use white bark with oak leaves")
                    || chosen("oak leaves are fine") || chosen("approximation is fine"));
            if (ai_project_package_word(unquoted, "birch") && !approximation)
                return "The default Plant Lab tree uses oak leaves. For exact birch, first choose an admitted birch foliage texture through the tree texture action; or explicitly use white bark with oak leaves as an approximation. Which should I use? Nothing has changed.";
            if (operation == "forest.configure") return {};
            if (ai_project_creation_ocean(request) == "conflicting")
                return "Should the new planet have an ocean on or off? The request contains both choices. Nothing was created.";
            const auto primaryObject = ai::project_request_detail::representation_object(unquoted);
            const bool desktopScale = ai_project_package_word(primaryObject,"desk")
                || ai_project_package_word(primaryObject,"tabletop");
            if (desktopScale && !chosen("procedural world") && !chosen("procedural planet")
                && !chosen("explorable world") && !chosen("explorable planet"))
                return "Should this be an explorable procedural planet with Plant Lab trees, or a miniature/generated prop for a desk scene? Please choose the representation and scale before I create anything.";
            // Explicit permission to choose defaults is still shown as concrete
            // settings in the reviewed call. It is not permission to execute.
            const bool defaults = ai_project_creation_uses_defaults(request);
            const auto coverage = ai_project_creation_coverage(request);
            const bool planet = coverage == "planet" || coverage == "universe";
            std::string missing;
            const auto need = [&](bool present,std::string_view choice) {
                if (present) return;
                if (!missing.empty()) missing += "; ";
                missing += choice;
            };
            need(planet || has({"game","tool","software","application"}),"game or software project");
            need(planet || coverage == "none","empty project, one planet, or a universe");
            if (planet)
            {
                const auto terrain = ai_requested_project_terrain(request);
                constexpr std::array descriptions{"fixed or random terrain seed", "planet size", "Compact, Fast, Balanced, or Fine detail"};
                for (std::size_t index{}; index < terrain.size(); ++index)
                    need(terrain[index] != "?" && (defaults || !terrain[index].empty()), descriptions[index]);
                need(defaults || has({"ocean","water","dry"}),"ocean on or off");
            }
            return missing.empty() ? std::string{}
                : "Before I create anything, please choose: " + missing
                    + ". You can answer in normal words or explicitly ask for defaults.";
        }

        [[nodiscard]] ai::McpValidation validate_ai_project_tool_selection(std::string_view request,const ai::McpToolCall& call)
        {
            const auto lifecycle = ai::project_lifecycle_request_tool(request);
            if (lifecycle == "asset.image.generate")
                return call.tool==lifecycle ? ai::validate_project_tool_arguments(call)
                    : ai::McpValidation{ai::McpErrorCode::invalid_argument,"This requests an image or texture. Use the image generator; do not replace it with a tree, terrain, scene object or generated 3D mesh."};
            if (lifecycle == "asset.model.generate")
                return call.tool == lifecycle ? ai::validate_project_tool_arguments(call)
                    : ai::McpValidation{ai::McpErrorCode::invalid_argument,"This requests a 3D model from the approved reference. Use the reviewed 3D generation action."};
            if (ai::asset_generation_tool(call.tool))
                return {ai::McpErrorCode::invalid_argument,"No matching generation request was made. Nothing was generated."};
            if (ai::explicit_generated_asset_request(request)
                && (call.tool == "forest.configure" || call.tool == "forest.material" || call.tool == "forest.paint"
                    || call.tool == "terrain.generate" || call.tool == "project.create"))
                return {ai::McpErrorCode::invalid_argument,"This requests a generated prop/model representation. Review a PNG in Asset Generation, then ask to generate 3D from the approved reference."};
            if (lifecycle == "forest.configure")
            {
                if (call.tool != lifecycle)
                    return {ai::McpErrorCode::invalid_argument,"Tree design uses the current Plant Lab recipe through forest.configure; it is not a generic generated mesh or terrain action."};
                if (const auto question = ai_project_creation_question(request); !question.empty())
                    return {ai::McpErrorCode::invalid_argument,question};
                return ai::validate_project_tool_arguments(call);
            }
            if (lifecycle == "forest.material")
                return call.tool == lifecycle ? ai::validate_project_tool_arguments(call)
                    : ai::McpValidation{ai::McpErrorCode::invalid_argument,"A tree texture request uses forest.material with an exact admitted project texture; scalar configuration or painting cannot substitute for it."};
            if (call.tool == "forest.material")
                return {ai::McpErrorCode::invalid_argument,"No explicit current-tree texture request was made. Nothing was bound."};
            if (call.tool == "forest.configure")
                return {ai::McpErrorCode::invalid_argument,"No explicit current-tree design request was made. Nothing was configured."};
            if (lifecycle == "forest.paint")
            {
                if (call.tool != lifecycle)
                    return {ai::McpErrorCode::invalid_argument, "Painting a forest requires forest.paint, not regenerating terrain or creating individual objects."};
                if (const auto question = ai_project_creation_question(request); !question.empty())
                    return {ai::McpErrorCode::invalid_argument,question};
                return ai::validate_project_tool_arguments(call);
            }
            if (call.tool == "forest.paint")
                return {ai::McpErrorCode::invalid_argument, "Ask explicitly to paint a forest area before proposing this operation."};
            if (lifecycle == "project.create")
            {
                if (call.tool != lifecycle)
                    return {ai::McpErrorCode::invalid_argument,"Creating a new project requires project.create, not an action on the current project."};
                if (const auto question = ai_project_creation_question(request); !question.empty())
                    return {ai::McpErrorCode::invalid_argument,question};
                const auto requestedName = ai_requested_project_display_name(request);
                const auto* name = ai::authoring_plan_detail::argument(call,"name");
                if (requestedName ? !name || name->value != *requestedName : name != nullptr)
                    return {ai::McpErrorCode::invalid_argument,"The project display name changed after review. Nothing was created."};
                const auto* coverage = ai::authoring_plan_detail::argument(call,"coverage");
                const auto requestedCoverage = ai_project_creation_coverage(request);
                if (coverage && !requestedCoverage.empty() && coverage->value != requestedCoverage)
                    return {ai::McpErrorCode::invalid_argument,"The requested project coverage is "
                        + std::string{requestedCoverage} + "; the proposal must use that one exact value. Nothing was created."};
                const auto requestedOcean = ai_project_creation_ocean(request);
                const auto* ocean = ai::authoring_plan_detail::argument(call,"ocean");
                if (ocean && !requestedOcean.empty() && ocean->value != requestedOcean)
                    return {ai::McpErrorCode::invalid_argument,"The request requires ocean="
                        + std::string{requestedOcean} + "; the proposal selected ocean=" + ocean->value
                        + ". Revise that setting before approval. Nothing was created."};
                if (requestedCoverage == "planet" || requestedCoverage == "universe")
                {
                    const auto terrain = ai_requested_project_terrain(request);
                    for (std::size_t index{}; index < terrain.size(); ++index)
                    {
                        if (terrain[index].empty()) continue;
                        const auto field = ai_project_terrain_fields[index];
                        const auto* proposed = ai::authoring_plan_detail::argument(call,field);
                        if (!proposed || ai_project_terrain_value(field, proposed->value) != terrain[index])
                            return {ai::McpErrorCode::invalid_argument,"The request requires " + std::string{field} + "="
                                + terrain[index] + ". Revise that setting before approval. Nothing was created."};
                    }
                }
                return ai::validate_project_tool_arguments(call);
            }
            if (call.tool == "project.create")
                return {ai::McpErrorCode::invalid_argument,"No affirmative new-project request was made. Nothing was created."};
            if (!lifecycle.empty() && call.tool != lifecycle)
                return {ai::McpErrorCode::invalid_argument,
                    "This request requires " + std::string{lifecycle}
                    + " as its next operation. Revise the proposal before approval."};
            const auto* requestedPackage=ai_project_requested_package(request);
            const auto intent=ai_project_package_intent(request,
                ai_project_requested_package(ai_project_package_unquoted(request))!=nullptr);
            if(!intent.clear_request)return {};
            const std::string_view expected=intent.operation==AiProjectPackageOperation::catalog?"package.catalog"
                :intent.operation==AiProjectPackageOperation::install?"package.install":"terrain.generate";
            if(call.tool!=expected)
                return {ai::McpErrorCode::invalid_argument,"The proposed "+call.tool+" does not perform the requested "+std::string{expected}
                    +" operation. Nothing ran and this proposal cannot be approved. Revise the plan or ask one critical clarification."};
            if(intent.operation==AiProjectPackageOperation::install)
            {
                const auto* package=requestedPackage;
                if(!package)return {ai::McpErrorCode::invalid_argument,"The package request does not identify one registered package. Ask one critical clarification; no package was selected or installed."};
                const auto* target=ai::authoring_plan_detail::argument(call,"package");
                if(!target||target->value!=package->id)
                    return {ai::McpErrorCode::invalid_argument,"The proposed package is different from the requested registered package. Nothing ran and this proposal cannot be approved."};
            }
            return ai::validate_project_tool_arguments(call);
        }

        [[nodiscard]] std::string ai_project_tool_selection_prompt(std::string_view request)
        {
            if (ai_project_creation_uses_defaults(request))
            {
                std::string text = "\nHOST_PROJECT_CREATION_DEFAULTS_V1\n";
                for (const auto& field : ai_project_creation_defaults())
                    text += field.name + "=" + field.value + "\n";
                text += "The user explicitly chose defaults. These host values fill only omitted generation fields before approval. "
                    "Keep kind, coverage and every requested override explicit. Invalid explicit values, duplicate or unknown fields remain errors. "
                    "Nothing executes until the user approves the resolved full CALL.\nEND_HOST_PROJECT_CREATION_DEFAULTS_V1\n";
                if (const auto coverage = ai_project_creation_coverage(request);
                    coverage == "planet" || coverage == "universe")
                {
                    const auto ocean = ai_project_creation_ocean(request);
                    const auto terrain = ai_requested_project_terrain(request);
                    std::string explicitTerrain;
                    for (std::size_t index{}; index < terrain.size(); ++index)
                        if (!terrain[index].empty() && terrain[index] != "?")
                            explicitTerrain += " " + std::string{ai_project_terrain_fields[index]} + "=" + terrain[index];
                    text += "\nREQUEST_OPERATION_CONSTRAINT_V1\nThe user requested coverage=" + std::string{coverage}
                        + ". Use this one literal coverage value, not a list of choices.\n"
                        "Concrete request-scoped defaults proposal:\nEPOCH_TOOL_PLAN_V1\nTITLE Create new "
                        + std::string{coverage} + " project\nSUMMARY Request approval for a new game project with the chosen coverage.\n"
                        "CALL project.create kind=game coverage=" + std::string{coverage}
                        + explicitTerrain
                        + (ocean == "on" || ocean == "off" ? " ocean=" + std::string{ocean} : std::string{})
                        + "\nEND\nThis CALL includes recognized user choices and omits the remaining defaultable generation fields. "
                        "Copy every user-specified seed, radius_m, detail or ocean override into it; Epoch fills only the remaining omitted fields from the host defaults above. "
                        "Do not copy grammar alternatives or angle brackets. Do not save, build or run yet.\nEND_REQUEST_OPERATION_CONSTRAINT_V1\n";
                }
                return text;
            }
            const auto* requestedPackage=ai_project_requested_package(request);
            const auto intent=ai_project_package_intent(request,
                ai_project_requested_package(ai_project_package_unquoted(request))!=nullptr);
            if(!intent.clear_request)return {};
            const std::string_view expected=intent.operation==AiProjectPackageOperation::catalog?"package.catalog"
                :intent.operation==AiProjectPackageOperation::install?"package.install":"terrain.generate";
            std::string text="\nREQUEST_OPERATION_CONSTRAINT_V1\nThe requested operation is "+std::string{expected}+".\n";
            if(intent.operation==AiProjectPackageOperation::install)
            {
                if(const auto* package=requestedPackage)
                    text+="Valid proposal for this registered package:\nEPOCH_TOOL_PLAN_V1\nTITLE Review package addition\n"
                        "SUMMARY Request approval to add the selected registered project package.\nCALL package.install package="
                        +std::string{package->id}+"\nEND\n";
                else text+="The target is missing or ambiguous. Ask for it:\nEPOCH_TOOL_QUESTION_V1\n"
                    "QUESTION Which registered project package should I add?\nEND\n";
            }
            else if(intent.operation==AiProjectPackageOperation::catalog)
                text+="Valid proposal:\nEPOCH_TOOL_PLAN_V1\nTITLE List project packages\n"
                    "SUMMARY Inspect the host's registered project packages.\nCALL package.catalog\nEND\n";
            else text+="CALL syntax: terrain.generate coverage=<requested coverage> seed=<unsigned seed>.\n";
            text+="Return one proposal or one critical question. The catalog supplies availability; if it is unavailable, ask how to proceed. "
                "A proposal requests approval; completion requires later host evidence.\nEND_REQUEST_OPERATION_CONSTRAINT_V1\n";
            return text;
        }

        [[nodiscard]] ai::ToolPlanParseResult admit_ai_project_tool_reply(std::string_view request,std::string_view reply)
        {
            const bool useDefaults = ai_project_creation_uses_defaults(request);
            auto defaults = ai_project_creation_defaults();
            if (useDefaults)
            {
                const auto terrain = ai_requested_project_terrain(request);
                for (std::size_t index{}; index < terrain.size(); ++index)
                    if (!terrain[index].empty()) defaults[index].value = terrain[index];
                if (const auto ocean = ai_project_creation_ocean(request); ocean == "on" || ocean == "off")
                    defaults.back().value = ocean;
            }
            auto result=ai::parse_tool_plan(reply, useDefaults
                ? std::span<const ai::McpArgument>{defaults} : std::span<const ai::McpArgument>{});
            if(result)
            {
                if (result.plan.call.tool == "project.create")
                    if (const auto name = ai_requested_project_display_name(request); name && ai::valid_project_display_name(*name))
                        result.plan.call.arguments.push_back({"name", std::string{*name}});
                if(const auto selected=validate_ai_project_tool_selection(request,result.plan.call);!selected)
                {result.code=ai::ToolPlanCode::invalid_argument;result.message=selected.message;}
                else if (result.plan.call.tool == "forest.paint"
                    && ai_forest_location(request) == AiForestLocation::ocean_shoreline)
                    result.plan.summary = "Search the selected ocean-bearing body for dry ground within 100 meters of verified ocean water, then paint the reviewed forest area.";
            }
            return result;
        }

        [[nodiscard]] std::string ai_project_package_catalog(const EditorState& editor, bool modelEvidence = false)
        {
            std::string text = modelEvidence ? "REGISTERED_PROJECT_PACKAGES_V1\n" : "Project packages\n";
            for (const auto& package : package_registry::known_packages())
            {
                if (package_registry::catalog_section(package.id) != package_registry::CatalogSection::ProjectPackages)
                    continue;
                std::string status, revision = "not verified";
                if (package_registry::is_network_sensitive(package))
                    status = "Separate network approval required; unavailable to this tool";
                else if (builtin_project_package_ready(package.id))
                {
                    status = "Ready to add from the bundled engine";
                    revision = "bundled";
                }
                else if (package.id == "research_planetary_terrain")
                {
                    if (editor.planetaryCheckedRoot == editor.projectRoot
                        && editor.planetaryAttachment.source_attached())
                    {
                        status = "Source attached; generation available";
                        revision = editor.planetaryAttachment.content_hash;
                    }
                    else if (editor.packageSourceReview && editor.packageSourceReview->review_ready()
                        && editor.packageSourceStaged.source_staged()
                        && editor.packageSourceReview->registry_package_id == package.id)
                    {
                        status = "Cached source checked; approval required before adding";
                        revision = editor.packageSourceReview->content_hash;
                    }
                    else status = "Needs source review; local source must be verified";
                }
                else status = "Reference only; no project installer yet";
                text += modelEvidence
                    ? std::string{package.id} + " | " + std::string{package.displayName}
                        + " | revision=" + revision + " | " + status + "\n"
                    : std::string{package.displayName} + " — " + status + "\n";
            }
            if (modelEvidence)
                text += "END_REGISTERED_PROJECT_PACKAGES_V1\nOnly these exact IDs are recognized. A descriptor is not installed. No automatic downloads or network services.\n";
            return text;
        }

        [[nodiscard]] bool dispatch_ai_project_package_tool(EditorState& editor,
            const ai::McpToolCall& call, const std::shared_ptr<core::Context>& context,
            const project_planetary::Settings* requestedSettings = nullptr)
        {
            if (const auto arguments = ai::validate_project_tool_arguments(call); !arguments)
            {
                editor.aiToolStatus = arguments.message;
                return false;
            }
            if (call.tool == "package.catalog")
            {
                editor.aiToolStatus = ai_project_package_catalog(editor);
                return true;
            }
#if EPOCH_EDITION_PUBLIC
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                editor.aiToolStatus = "Package tools need an ordinary game or application project. Engine source is outside this scope.";
                return false;
            }
#endif
            if (editor.projectId.empty() || editor.projectRoot.empty())
            {
                editor.projectCreationOpen = true;
                editor.aiToolStatus = "Create or open a project, then request this package operation again. No source was installed or built.";
                return false;
            }
            if (editor.aiPackageRequest || package_source_task_busy(editor) || planetary_project_busy(editor)
                || editor.projectBuildPending || editor.scriptBuildPending
                || editor.projectLifecycle.evidence().runtime_active)
            {
                editor.aiToolStatus = "Finish or cancel the active project/package operation before approving another.";
                return false;
            }
            const bool generate = call.tool == "terrain.generate";
            const auto* argument = ai::authoring_plan_detail::argument(call, "package");
            const std::string packageId = generate ? "research_planetary_terrain"
                : (argument ? argument->value : std::string{});
            const auto* package = package_registry::find(packageId);
            if (!package || package_registry::catalog_section(packageId) != package_registry::CatalogSection::ProjectPackages
                || package_registry::is_network_sensitive(*package))
            {
                editor.aiToolStatus = "The requested package is unknown, is not a project package, or needs separate network approval.";
                return false;
            }
            if (builtin_project_package_ready(packageId) && !generate)
            {
                editor.packageInstallProgress = 0.0f;
                add_builtin_project_package(editor, context, *package);
                editor.aiToolStatus = editor.packageInstallProgress == 1.0f
                    ? editor.projectStatus : editor.packageInstallStatus;
                return editor.packageInstallProgress == 1.0f;
            }
            if (packageId != "research_planetary_terrain")
            {
                editor.aiToolStatus = "This source library has no project activation route yet. A catalog entry does not mean it is installed.";
                return false;
            }
            EditorState::AiPackageRequest request{};
            request.project_id = editor.projectId;
            request.project_root = editor.projectRoot;
            request.project_key = planetary_project_key(editor);
            request.project_generation = editor.projectLifecycle.evidence().generations.selected_project.generation;
            request.generate = generate;
            request.save_recipe_before_generation = requestedSettings && requestedSettings->ocean_surface;
            if (generate)
            {
                if (requestedSettings) request.settings = *requestedSettings;
                request.settings.mode = ai::authoring_plan_detail::argument(call, "coverage")->value;
                const auto& seed = ai::authoring_plan_detail::argument(call, "seed")->value;
                (void)std::from_chars(seed.data(), seed.data() + seed.size(), request.settings.seed);
                if (!project_planetary::valid_settings(request.settings))
                {
                    editor.aiToolStatus = "The requested terrain settings exceed the current provider budget. Nothing was queued.";
                    return false;
                }
            }
            editor.aiPackageRequest = std::move(request);
            if (editor.planetaryCheckedRoot == editor.projectRoot && editor.planetaryAttachment.source_attached())
            {
                if (!generate)
                {
                    finish_ai_project_package_request(editor, "Planetary Terrain source is already attached to this project.");
                    return true;
                }
                editor.aiPackageRequest->source_hash = editor.planetaryAttachment.content_hash;
                editor.aiPackageRequest->phase = EditorState::AiPackageRequest::Phase::generating;
                editor.planetarySettings = editor.aiPackageRequest->settings;
                editor.planetarySeedDraft = std::to_string(editor.planetarySettings.seed);
                start_planetary_project_generation(editor);
                if (!editor.planetaryGeneratePending && !editor.planetaryFeatureReviewPending && !editor.planetaryFeatureAdmissionPending)
                {
                    finish_ai_project_package_request(editor, "Terrain generation was not queued: " + editor.planetaryStatus);
                    return false;
                }
                editor.aiToolStatus = "The approved terrain request is building with this project's admitted source. Progress and Cancel are in Project.";
                return true;
            }
            editor.selectedPackageId = packageId;
            editor.packageCatalogModelsOnly = false;
            editor.showPackageManagerModal = true;
            open_package_source_review(editor, packageId);
            editor.aiToolStatus = generate
                ? "Review the exact Planetary Terrain source, then choose Add & Generate. No source, build or generation is approved by model text alone."
                : "Review the exact Planetary Terrain source, then choose Add to Project. No source was copied yet.";
            return true;
        }

        [[nodiscard]] bool create_ai_project_from_tool(EditorState& editor,
            const ai::McpToolCall& call, const std::shared_ptr<core::Context>& context)
        {
            if (call.tool != "project.create")
            { editor.aiToolStatus = "This action requires the exact project.create operation."; return false; }
            if (const auto arguments = ai::validate_project_tool_arguments(call); !arguments)
            { editor.aiToolStatus = arguments.message; return false; }
            const auto& kind = ai::authoring_plan_detail::argument(call,"kind")->value;
            const auto& coverage = ai::authoring_plan_detail::argument(call,"coverage")->value;
            std::optional<project_planetary::Settings> terrain;
            if (coverage != "none")
            {
                project_planetary::Settings settings;
                settings.mode = coverage;
                const auto& seed = ai::authoring_plan_detail::argument(call,"seed")->value;
                if (seed == "random")
                    settings.seed = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
                else (void)std::from_chars(seed.data(),seed.data()+seed.size(),settings.seed);
                const auto& radius = ai::authoring_plan_detail::argument(call,"radius_m")->value;
                (void)std::from_chars(radius.data(),radius.data()+radius.size(),settings.reference_radius_meters);
                settings.ocean_surface = ai::authoring_plan_detail::argument(call,"ocean")->value == "on";
                const auto& detail = ai::authoring_plan_detail::argument(call,"detail")->value;
                const auto preset = detail == "compact" ? project_planetary::DetailPreset::compact
                    : detail == "fast" ? project_planetary::DetailPreset::fast
                    : detail == "balanced" ? project_planetary::DetailPreset::detailed
                    : project_planetary::DetailPreset::fine;
                terrain = project_planetary::with_detail_preset(settings,preset);
                if (!terrain)
                { editor.aiToolStatus = "The requested terrain settings do not fit the provider budget. No project was created."; return false; }
            }
            const auto* name = ai::authoring_plan_detail::argument(call,"name");
            const auto created = editor_create_project_shell(kind == "game" ? EditorProjectKind::Game : EditorProjectKind::Tool,
                EditorProjectInputProvision::project_default, name ? std::string_view{name->value} : std::string_view{});
            if (!created.succeeded)
            { editor.aiToolStatus = "project.create failed: " + created.summary; return false; }
            const auto admitted = editor_admit_project_manifest(created.manifest_path);
            if (!admitted.succeeded || admitted.project_id != created.project_id
                || !editor_open_project(context,created.project_id))
            { editor.aiToolStatus = "Created project " + created.project_id + " at " + created.root_path
                + ", but could not open it. " + admitted.summary; return false; }
            editor.projectCreationOpen = false;
            editor.requestedMainSurface = EditorMainSurface::Project;
            if (!terrain)
            {
                editor.aiToolStatus = "project.create completed: " + created.project_id
                    + ". Empty project created and opened; nothing was built or run.";
                return true;
            }
            const auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            // An ocean recipe needs the admitted Water attachment. The
            // existing package flow saves it after review and before generation.
            if (!terrain->ocean_surface && (!project_planetary::save_settings(root,*terrain)
                || project_planetary::load_settings(root) != terrain))
            {
                editor.aiToolStatus = "Created project " + created.project_id
                    + ", but its terrain settings could not be saved. No terrain was generated; retry Save Settings in Project.";
                return false;
            }
            editor.planetarySettings = *terrain;
            editor.planetarySeedDraft = std::to_string(terrain->seed);
            // Reuse the existing exact-source review and cancellation path.
            // This approval never approves optional source/dependency bytes.
            auto generation = call;
            generation.tool = "terrain.generate";
            generation.arguments = {{"coverage",terrain->mode},{"seed",std::to_string(terrain->seed)}};
            const bool queued = dispatch_ai_project_package_tool(editor,generation,context,&*terrain);
            editor.aiToolStatus = "Created and opened " + created.project_id
                + (terrain->ocean_surface ? "; requested an ocean recipe (seed " : "; saved requested terrain settings (seed ")
                + std::to_string(terrain->seed) + "). " + editor.aiToolStatus;
            return queued;
        }

        [[nodiscard]] std::string ai_project_package_tool_description(const ai::McpToolCall& call)
        {
            if (call.tool == "package.catalog") return "Find the project packages this host can actually add or review.";
            if (call.tool == "package.install")
            {
                const auto* id = ai::authoring_plan_detail::argument(call, "package");
                const auto* package = id ? package_registry::find(id->value) : nullptr;
                return package ? "Review " + std::string{package->displayName}
                    + " for this project. Downloaded source still needs its exact file approval."
                    : "The proposed package is not in this host's project catalog. Nothing can be installed.";
            }
            const auto* coverage = ai::authoring_plan_detail::argument(call, "coverage");
            const auto* seed = ai::authoring_plan_detail::argument(call, "seed");
            return coverage && seed ? "Generate " + coverage->value + " terrain (seed " + seed->value
                + "). Missing Planetary Terrain source must be reviewed and approved before its local build."
                : "The terrain request is missing its coverage or seed.";
        }

        void continue_ai_project_package_after_attachment(EditorState& editor)
        {
            if (!editor.aiPackageRequest
                || editor.aiPackageRequest->phase != EditorState::AiPackageRequest::Phase::attaching)
                return;
            if (!ai_project_package_request_current(editor)
                || editor.aiPackageRequest->cancelled)
            {
                finish_ai_project_package_request(editor, "Package request cancelled or its project changed. No terrain build was started.");
                return;
            }
#if EPOCH_EDITION_PUBLIC
            if (!project_ai_scope_allowed(editor.projectRoot))
            {
                finish_ai_project_package_request(editor, "Package continuation refused: this is no longer an admitted application project.");
                return;
            }
#endif
            if (!editor.planetaryAttachment.source_attached())
            {
                finish_ai_project_package_request(editor, "Package attachment failed; no terrain build was started. "
                    + editor.planetaryAttachment.message);
                return;
            }
            if (editor.planetaryAttachment.registry_package_id != "research_planetary_terrain"
                || editor.planetaryAttachment.content_hash != editor.aiPackageRequest->source_hash)
            {
                finish_ai_project_package_request(editor, "The approved package attachment did not complete with the exact reviewed source. No terrain build was started.");
                return;
            }
            if (!editor.aiPackageRequest->generate)
            {
                finish_ai_project_package_request(editor, "Planetary Terrain source added to the active project. Nothing was built or run.");
                return;
            }
            editor.planetarySettings = editor.aiPackageRequest->settings;
            editor.planetarySeedDraft = std::to_string(editor.planetarySettings.seed);
            editor.aiPackageRequest->phase = EditorState::AiPackageRequest::Phase::generating;
            start_planetary_project_generation(editor);
            if (!editor.planetaryGeneratePending && !editor.planetaryFeatureReviewPending && !editor.planetaryFeatureAdmissionPending)
                finish_ai_project_package_request(editor, "The source was added, but terrain generation could not start: " + editor.planetaryStatus);
            else
                editor.aiToolStatus = "Exact approved source attached. Building and generating the requested terrain; progress and Cancel are in Project.";
        }

        [[nodiscard]] bool ai_project_package_generation_current(const EditorState& editor) noexcept
        {
            return !editor.aiPackageRequest
                || (ai_project_package_request_current(editor)
                    && !editor.aiPackageRequest->cancelled
                    && editor.aiPackageRequest->phase == EditorState::AiPackageRequest::Phase::generating
                    && editor.aiPackageRequest->settings == editor.planetarySettings
                    && editor.aiPackageRequest->source_hash == editor.planetaryAttachment.content_hash);
        }
