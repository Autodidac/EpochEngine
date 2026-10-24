// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
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
            (void)synchronize_gui_document(editor);
            const std::optional<authoring::gui::GuiDocumentSnapshot>
                preservedGuiDocument =
                    editor.guiDocument && editor.guiDocument->valid()
                    ? std::optional<authoring::gui::GuiDocumentSnapshot>{
                        editor.guiDocument->snapshot()}
                    : std::nullopt;
            const auto preservedGuiWidgets =
                editor.guiDocumentWidgets;
            const std::uint64_t preservedGuiSavedRevision =
                editor.guiDocumentSavedRevision;
            const std::vector<EditorEntity> preservedEntities = editor.entities;
            const std::size_t preservedSelected = editor.selectedEntity;
            const auto preservedPreviewMode = editor.previewMode;
            const auto preservedWorkspaceTab = editor.workspaceTab;
            const auto preservedDockStatusTab = editor.dockStatusTab;
            const auto preservedMainSurface = editor.mainSurface;
            const auto preservedCanvas2DProject = editor.canvas2dProject;
            const auto preservedProjectCameraMode = editor.projectCameraMode;
            const auto preservedInputProfile = editor.inputProfilePreset;
            const std::string preservedRunBackend = editor.projectRunBackend;
            const double preservedRunFrameLimit = editor.projectRunFrameLimitFps;
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
                if (targetProject == previousProject)
                {
                    editor.previewMode = preservedPreviewMode;
                    editor.workspaceTab = preservedWorkspaceTab;
                    editor.dockStatusTab = preservedDockStatusTab;
                    editor.mainSurface = preservedMainSurface;
                    editor.canvas2dProject = preservedCanvas2DProject;
                    editor.projectCameraMode = preservedProjectCameraMode;
                    editor.inputProfilePreset = preservedInputProfile;
                    editor.projectRunBackend = preservedRunBackend;
                    editor.projectRunFrameLimitFps = preservedRunFrameLimit;
                    input::set_active_profile(editor.inputProfilePreset);

                    if (!preservedEntities.empty())
                    {
                        editor.entities = preservedEntities;
                        select_editor_entity(editor, (std::min)(preservedSelected, editor.entities.size() - 1u));
                        normalize_editor_entity_ids(editor);
                    }
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
                                editor.guiDocumentSavedRevision =
                                    preservedGuiSavedRevision;
                                editor.guiDocumentLoadAttempted = true;
                                editor.guiDocumentSourcePath =
                                    display_project_path(
                                        project_gui_document_path(editor));
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
                if (!save_project_gui_document(editor)
                    || !save_editor_scene_snapshot(editor))
                {
                    editor.projectStatus =
                        "Project save blocked because GUI and scene source evidence were not both committed.";
                    push_editor_log(
                        editor,
                        "[project] " + editor.projectStatus);
                    return;
                }
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
                if (!published.accepted() && !published.artifact.empty())
                {
                    std::error_code removeError{};
                    std::filesystem::remove(
                        editor.packageModelInstall->stagingRoot
                            / published.artifact,
                        removeError);
                }
                editor.packageInstallStatus = published.accepted()
                    ? "Model add-on installed and verified. Activation remains a separate explicit action."
                    : "Model install failed during verified publication: "
                        + published.diagnostic;
                editor.packageInstallProgress =
                    published.accepted() ? 1.0f : 0.0f;
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
            const package_registry::PackageDescriptor& package)
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
            const std::filesystem::path cache = model_cache_root_path();
            const std::filesystem::path installed =
                ai::model_install::version_root(cache, *plan);
            std::error_code error{};
            if (std::filesystem::exists(installed, error) && !error)
            {
                if (model_snapshot_installed(*plan))
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
            external_mcp
        };

        [[nodiscard]] std::string_view project_ai_provider_name(
            ProjectAiProviderMode mode) noexcept
        {
            switch (mode)
            {
            case ProjectAiProviderMode::disabled:
                return "disabled";
            case ProjectAiProviderMode::epoch_local:
                return "epoch_local_qwen38";
            case ProjectAiProviderMode::external_mcp:
                return "external_mcp";
            }
            return "disabled";
        }

        [[nodiscard]] std::filesystem::path project_ai_profile_path(
            const EditorState& editor)
        {
            if (editor.projectRoot.empty())
                return {};
            return resolve_editor_path(
                std::filesystem::path{editor.projectRoot})
                / "Assets" / "AI" / "project_ai.epochai";
        }

        [[nodiscard]] bool write_project_ai_profile(
            EditorState& editor,
            ProjectAiProviderMode mode)
        {
            const std::filesystem::path profilePath =
                project_ai_profile_path(editor);
            if (profilePath.empty())
            {
                editor.packageInstallStatus =
                    "No active project root for an AI provider selection.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            std::error_code ec;
            std::filesystem::create_directories(
                profilePath.parent_path(), ec);
            if (ec)
            {
                editor.packageInstallStatus =
                    "Could not create the project AI profile directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const bool enabled = mode != ProjectAiProviderMode::disabled;
            const bool local = mode == ProjectAiProviderMode::epoch_local;
            const bool external =
                mode == ProjectAiProviderMode::external_mcp;
            const std::filesystem::path temporary =
                profilePath.string() + ".epoch-managed.tmp";
            {
                std::ofstream out(
                    temporary, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus =
                        "Could not write the project AI profile.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }
                out
                    << "{\n"
                    << "  \"schema\": \"epoch.project.ai.v1\",\n"
                    << "  \"provider\": \""
                    << project_ai_provider_name(mode) << "\",\n"
                    << "  \"enabled\": "
                    << (enabled ? "true" : "false") << ",\n"
                    << "  \"model_binding\": \""
                    << (local
                        ? epochengine::ai::kEpochLocalQwen38ModelPackageId
                        : (external
                            ? std::string_view{"operator_selected_external"}
                            : std::string_view{"none"}))
                    << "\",\n"
                    << "  \"runtime_binding\": \""
                    << (local
                        ? std::string_view{"epoch_shared"}
                        : (external
                            ? std::string_view{"external_mcp"}
                            : std::string_view{"none"}))
                    << "\",\n"
                    << "  \"inference_transport\": \""
                    << (local
                        ? std::string_view{"llama_cpp_cli_child_process"}
                        : (external
                            ? std::string_view{"operator_managed_openai_compatible"}
                            : std::string_view{"none"}))
                    << "\",\n"
                    << "  \"tool_protocol\": \"epoch_mcp_v1\",\n"
                    << "  \"endpoint_binding\": \""
                    << (external
                        ? std::string_view{
                            "operator_selected_runtime_endpoint"}
                        : std::string_view{"none"})
                    << "\",\n"
                    << "  \"self_iteration\": \""
                    << (enabled
                        ? std::string_view{
                            "operator_approved_project_source_only"}
                        : std::string_view{"disabled"})
                    << "\",\n"
                    << "  \"project_source_write\": "
                    << (enabled ? "true" : "false") << ",\n"
                    << "  \"engine_source_write\": false,\n"
                    << "  \"operator_approval_per_iteration\": true,\n"
                    << "  \"auto_start\": false,\n"
                    << "  \"server_or_listener\": false,\n"
                    << "  \"weights_bundled\": false,\n"
                    << "  \"external_provider_preserved\": true\n"
                    << "}\n";
            }

            ec.clear();
            std::filesystem::rename(temporary, profilePath, ec);
            if (ec)
            {
                ec.clear();
                std::filesystem::remove(profilePath, ec);
                ec.clear();
                std::filesystem::rename(temporary, profilePath, ec);
            }
            if (ec)
            {
                editor.packageInstallStatus =
                    "Could not publish the project AI profile atomically.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            editor.packageInstallStatus =
                "Project AI provider set to "
                + std::string{project_ai_provider_name(mode)}
                + "; activation remains explicit per iteration.";
            editor.packageInstallProgress = enabled ? 1.0f : 0.0f;
            push_editor_log(
                editor,
                "[package] Project AI profile: "
                    + display_project_path(profilePath));
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
