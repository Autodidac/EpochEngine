// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        void start_ai_continuous_project_build(
            EditorState& editor,
            const std::string& activeScriptSource,
            const std::filesystem::path& pathsManifest,
            std::string_view reason,
            bool force)
        {
            if (editor.aiContinuousBuildPending)
            {
                editor.aiContinuousBuildStatus = "Engine-development build already running.";
                return;
            }

            if (editor.projectRoot.empty())
            {
                editor.aiContinuousBuildStatus = "Cannot build: no active project root.";
                return;
            }

            const std::string fingerprint = ai_continuous_build_fingerprint(editor, activeScriptSource, pathsManifest);
            if (!force && fingerprint == editor.aiContinuousBuildFingerprint)
            {
                editor.aiContinuousBuildStatus = "Evidence is unchanged; manual build gate remains idle.";
                return;
            }

            editor.aiContinuousBuildFingerprint = fingerprint;
            editor.aiContinuousBuildStatus = "Queued build: " + std::string(reason);
            push_editor_log(editor, "[ai-build] Queued engine development build: " + std::string(reason));

            editor.aiContinuousBuildPending.emplace(schedule_editor_task(
                editor,
                "AI Evidence Project Build",
                [root = editor.projectRoot]() {
                    return editor_build_project(root);
                }));
        }

        bool save_script_source_editor(EditorState& editor);

        void pump_script_build(EditorState& editor)
        {
            if (!editor.scriptBuildPending
                || editor.scriptBuildPending->wait_for(std::chrono::milliseconds(0))
                    != std::future_status::ready)
            {
                return;
            }

            const std::uint64_t generation = editor.scriptBuildGeneration;
            editor.scriptBuildSucceeded = false;
            try
            {
                const auto result = editor.scriptBuildPending->get();
                editor.scriptBuildStatus = result.summary;
                editor.scriptBuildCompletedGeneration = generation;
                editor.scriptBuildSucceeded = result.succeeded && !result.cancelled;
                push_editor_log(
                    editor,
                    std::string{"[script] "}
                        + (result.cancelled
                            ? "Build cancelled. "
                            : (result.succeeded
                                ? "Build passed. "
                                : "Build failed. "))
                        + result.summary);
            }
            catch (const editor_tasks::TaskCancelled&)
            {
                editor.scriptBuildStatus =
                    "Script build cancelled before its compiler process started.";
                push_editor_log(editor, "[script] " + editor.scriptBuildStatus);
            }
            catch (const std::exception& error)
            {
                editor.scriptBuildStatus = std::string{"Script build failed: "} + error.what();
                push_editor_log(editor, "[script][ERROR] " + editor.scriptBuildStatus);
            }
            catch (...)
            {
                editor.scriptBuildStatus = "Script build failed with an unknown exception.";
                push_editor_log(editor, "[script][ERROR] " + editor.scriptBuildStatus);
            }

            editor.scriptBuildPending.reset();
            editor.scriptBuildCompletedGeneration = generation;
            editor.scriptBuildCancellation = {};
        }

        [[nodiscard]] bool cancel_script_build(EditorState& editor)
        {
            editor.aiProjectScriptRunAfterBuild = false;
            editor.aiProjectScriptRepairQueued = false;
            if (!editor.scriptBuildPending
                || !editor.scriptBuildCancellation.valid()
                || !editor.taskScheduler)
            {
                editor.scriptBuildStatus =
                    "No active script build is available to cancel.";
                return false;
            }
            const taskgraph::CancelCode code = editor.taskScheduler->cancel(
                editor.scriptBuildCancellation);
            if (code == taskgraph::CancelCode::cancelled
                || code == taskgraph::CancelCode::requested)
            {
                editor.scriptBuildStatus = code == taskgraph::CancelCode::cancelled
                    ? "Script build cancelled before compiler start."
                    : "Cancelling script compiler process tree...";
                push_editor_log(editor, "[script] " + editor.scriptBuildStatus);
                return true;
            }
            editor.scriptBuildStatus = code == taskgraph::CancelCode::already_terminal
                ? "Script build is already settling."
                : "Script build cancellation ticket is stale.";
            return false;
        }
        void start_script_build(EditorState& editor, std::string_view reason)
        {
            pump_script_build(editor);
            if (editor.scriptBuildPending)
            {
                editor.scriptBuildStatus = "A script build is already running.";
                return;
            }
            if (editor.activeScript.empty())
            {
                editor.scriptBuildStatus = "Select a script before building.";
                return;
            }
            if (editor.scriptEditorDirty && !save_script_source_editor(editor))
            {
                editor.scriptBuildStatus = "Save the active script before building.";
                return;
            }

            const std::string script = editor.activeScript;
            const std::string projectRoot = editor.projectRoot;
            const std::string scriptSource = editor_resolve_script_source_path(script, projectRoot);
            editor.scriptBuildTarget = compiler::default_script_output_path(
                std::filesystem::path{scriptSource}).generic_string();
            ++editor.scriptBuildGeneration;
            editor.scriptBuildSucceeded = false;
            editor.scriptBuildStatus = "Building " + scriptSource + "...";
            push_editor_log(
                editor,
                "[script] Queued C++23 build: " + std::string{reason});
            auto submitted = schedule_editor_cancellable_task(
                editor,
                "C++23 Script Build: " + script,
                [script, projectRoot](std::stop_token cancellation)
                {
                    return editor_build_script(
                        script,
                        projectRoot,
                        cancellation);
                });
            editor.scriptBuildCancellation = submitted.cancellation;
            editor.scriptBuildPending.emplace(
                std::move(submitted.completion));
        }
        [[nodiscard]] std::string display_project_path(const std::filesystem::path& path)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            return std::filesystem::absolute(resolvedPath, ec).lexically_normal().generic_string();
        }

        bool load_script_source_editor(EditorState& editor, const std::filesystem::path& path, bool force = false)
        {
            if (path.empty())
            {
                editor.scriptEditorStatus = "No script source selected.";
                return false;
            }

            const auto resolvedPath = resolve_editor_path(path);
            const std::string normalizedPath = display_project_path(resolvedPath);
            if (!force && editor.scriptEditorDirty && editor.scriptEditorPath != normalizedPath)
            {
                editor.scriptEditorStatus = "Unsaved script edits; save or reload before switching source files.";
                return false;
            }

            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
            {
                editor.scriptEditorPath = normalizedPath;
                editor.scriptEditorText.clear();
                editor.scriptEditorDirty = false;
                editor.scriptEditorStatus = "Script source is missing.";
                return false;
            }

            ec.clear();
            const auto size = std::filesystem::file_size(resolvedPath, ec);
            if (ec)
            {
                editor.scriptEditorStatus = "Could not read script source size.";
                return false;
            }

            constexpr std::uintmax_t kMaxEditableScriptBytes = 256u * 1024u;
            if (size > kMaxEditableScriptBytes)
            {
                editor.scriptEditorStatus = "Script source is too large for the current editor surface.";
                return false;
            }

            std::ifstream in(resolvedPath, std::ios::binary);
            if (!in)
            {
                editor.scriptEditorStatus = "Could not open script source for reading.";
                return false;
            }

            std::ostringstream text;
            text << in.rdbuf();
            editor.scriptEditorPath = normalizedPath;
            editor.scriptEditorText = text.str();
            editor.scriptEditorDirty = false;
            editor.scriptEditorStatus = "Loaded script source.";
            return true;
        }

        bool save_script_source_editor(EditorState& editor)
        {
            if (editor.scriptEditorPath.empty())
            {
                editor.scriptEditorStatus = "No script source loaded.";
                return false;
            }

            const auto resolvedPath = resolve_editor_path(std::filesystem::path{ editor.scriptEditorPath });
            if (!core::path::candidate_data_root().empty()
                && editor_resolve_candidate_project_path(resolvedPath).empty())
            {
                editor.scriptEditorStatus = "This preview's validated engine source is read-only. Project scripts save in its private Projects folder; engine changes belong to the next sandbox iteration.";
                return false;
            }
            std::error_code ec;
            std::filesystem::create_directories(resolvedPath.parent_path(), ec);
            if (ec)
            {
                editor.scriptEditorStatus = "Could not create script source directory.";
                return false;
            }

            std::ofstream out(resolvedPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                editor.scriptEditorStatus = "Could not open script source for writing.";
                return false;
            }

            out << editor.scriptEditorText;
            out.flush();
            if (!out)
            {
                editor.scriptEditorStatus = "Script source write failed; unsaved edits are retained.";
                return false;
            }
            editor.scriptEditorDirty = false;
            editor.scriptEditorStatus = "Saved script source.";
            return true;
        }

        [[nodiscard]] bool apply_ai_project_script(EditorState& editor)
        {
            if (editor.aiProjectScriptDraft.empty() || editor.scriptEditorDirty || editor.scriptBuildPending)
            {
                editor.aiAuthoringStatus = "The draft is unavailable, or unfinished script work must be saved first.";
                return false;
            }
            std::error_code error{};
            auto root = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            if (!core::path::candidate_data_root().empty()) root = editor_resolve_candidate_project_path(root);
            root = std::filesystem::weakly_canonical(root, error);
            const std::filesystem::path target{editor.aiProjectScriptPath};
            const auto scripts = std::filesystem::weakly_canonical(root / "scripts", error);
            const auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
            const bool existed = std::filesystem::exists(target, error);
            if (error || root.generic_string() != editor.aiProjectScriptRoot
                || scripts.parent_path() != root || canonicalTarget != target
                || target.parent_path() != scripts || !target.filename().string().ends_with(".ascript.cpp")
                || existed != editor.aiProjectScriptExisted
                || editor.sceneDocumentRevision != editor.aiProjectScriptSceneRevision)
            {
                editor.aiAuthoringStatus = "The project, scene or script destination changed. Request a fresh script draft.";
                return false;
            }
            std::string prior{};
            if (existed)
            {
                std::ifstream file{target, std::ios::binary};
                if (std::filesystem::file_size(target, error) > 128u * 1024u || error || !file) return false;
                prior.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
                if (!file) return false;
            }
            if (prior != editor.aiProjectScriptPreimage)
            {
                editor.aiAuthoringStatus = "The script changed on disk. The draft was preserved without overwriting it.";
                return false;
            }
            std::filesystem::create_directories(target.parent_path(), error);
            auto temporary = target;
            temporary += ".ai-" + std::to_string(editor.aiAuthoringRequestedGeneration) + ".tmp";
            const auto source = std::as_bytes(std::span{editor.aiProjectScriptDraft.data(), editor.aiProjectScriptDraft.size()});
            if (error || !platform::filesystem::exclusive_create_and_write(temporary, source, error))
            {
                editor.aiAuthoringStatus = "Could not stage the project script; the draft is retained. " + error.message();
                return false;
            }
            const bool published = existed
                ? platform::filesystem::atomic_replace_same_filesystem(temporary, target, error)
                : platform::filesystem::atomic_publish_new_same_filesystem(temporary, target, error);
            if (!published)
            {
                std::error_code ignored{};
                std::filesystem::remove(temporary, ignored);
                editor.aiAuthoringStatus = "Could not publish the project script; the draft is retained. " + error.message();
                return false;
            }
            if (!existed) { std::error_code ignored{}; std::filesystem::remove(temporary, ignored); }
            std::ifstream publishedFile{target, std::ios::binary};
            const std::string publishedSource{std::istreambuf_iterator<char>{publishedFile}, std::istreambuf_iterator<char>{}};
            if (!publishedFile || publishedSource != editor.aiProjectScriptDraft)
            {
                editor.aiAuthoringStatus = "Script publication could not be verified. The draft is retained; build and Run were skipped.";
                return false;
            }
            editor.scriptEditorPath = target.generic_string();
            editor.scriptEditorText = editor.aiProjectScriptDraft;
            editor.scriptEditorDirty = false;
            auto id = target.filename().string();
            id.resize(id.size() - std::string_view{".ascript.cpp"}.size());
            editor.activeScript = std::move(id);
            editor.aiProjectScriptDraft.clear();
            editor.aiAuthoringStatus = "Saved reviewed project script: " + target.generic_string()
                + ". Use the Scripts controls to build and run it; no compiler or native script was started.";
            return true;
        }

        void sync_script_source_editor(EditorState& editor, const std::filesystem::path& path)
        {
            if (path.empty())
                return;

            const std::string normalizedPath = display_project_path(path);
            if (editor.scriptEditorPath.empty() || (!editor.scriptEditorDirty && editor.scriptEditorPath != normalizedPath))
                (void)load_script_source_editor(editor, path, false);
        }

        void draw_script_source_editor(
            EditorState& editor,
            const std::filesystem::path& sourcePath,
            float width,
            float height,
            bool allowBuild = true)
        {
            sync_script_source_editor(editor, sourcePath);

            const std::string resolvedEditorPath =
                editor.scriptEditorPath.empty()
                    ? display_project_path(sourcePath)
                    : editor.scriptEditorPath;
            const std::string editorFileName =
                std::filesystem::path{resolvedEditorPath}
                    .filename()
                    .string();
            gui::label(
                editorFileName
                + (editor.scriptEditorDirty ? " *" : ""));
            gui::property_row(
                "Path",
                resolvedEditorPath,
                74.0f);

            std::vector<gui::InlineButtonSpec> scriptEditActions{
                gui::InlineButtonSpec{
                    .label = "Save",
                    .width = 72.0f},
                gui::InlineButtonSpec{
                    .label = "Reload",
                    .width = 82.0f}};
            if (allowBuild)
            {
                scriptEditActions.push_back(gui::InlineButtonSpec{
                    .label = "Build",
                    .width = 76.0f});
            }
            scriptEditActions.push_back(gui::InlineButtonSpec{
                .label = "Copy Path",
                .width = 96.0f});
            if (const auto clicked = gui::inline_button_row(
                    scriptEditActions, 29.0f, 5.0f))
            {
                if (*clicked == 0u)
                {
                    if (save_script_source_editor(editor))
                    {
                        push_editor_log(
                            editor,
                            "[script] Saved source: "
                                + editor.scriptEditorPath);
                        if (allowBuild)
                        {
                            append_project_note(
                                editor,
                                "Save Script Source",
                                "Saved script source from the editor surface.",
                                editor.scriptEditorPath);
                        }
                    }
                    else
                    {
                        push_editor_log(
                            editor,
                            "[script] Save failed: "
                                + editor.scriptEditorStatus);
                    }
                }
                else if (*clicked == 1u)
                {
                    if (load_script_source_editor(
                            editor, sourcePath, true))
                    {
                        push_editor_log(
                            editor,
                            "[script] Reloaded source: "
                                + editor.scriptEditorPath);
                    }
                    else
                    {
                        push_editor_log(
                            editor,
                            "[script] Reload failed: "
                                + editor.scriptEditorStatus);
                    }
                }
                else if (allowBuild && *clicked == 2u)
                {
                    if (editor.scriptEditorDirty)
                        (void)save_script_source_editor(editor);
                    start_script_build(
                        editor,
                        "Script source editor build request");
                }
                else
                {
                    (void)gui::set_clipboard_text(
                        resolvedEditorPath);
                    editor.scriptEditorStatus =
                        "Copied script path.";
                }
            }

            const std::string sourceEditorId =
                "script-source-editor:" + resolvedEditorPath;
            const auto sourceEditor = gui::source_editor(
                editor.scriptEditorText,
                gui::SourceEditorOptions{
                    .id = sourceEditorId,
                    .size = { (std::max)(180.0f, width), (std::max)(180.0f, height) },
                    .max_chars = 256u * 1024u,
                    .show_context_menu = true
                });
            if (sourceEditor.edit.changed)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Modified.";
            }
            if (sourceEditor.selected_all)
                editor.scriptEditorStatus = "Selected script source.";
            if (sourceEditor.copied)
            {
                editor.scriptEditorStatus = "Copied script source to clipboard.";
                push_editor_log(editor, "[script] Copied source to clipboard.");
            }
            if (sourceEditor.cut)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Cut script source to clipboard.";
                push_editor_log(editor, "[script] Cut source to clipboard.");
            }
            if (sourceEditor.pasted)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Pasted clipboard text into script source.";
                push_editor_log(editor, "[script] Pasted clipboard text into source editor.");
            }

            gui::property_row(
                "State",
                editor.scriptEditorDirty
                    ? std::string{"Modified"}
                    : editor.scriptEditorStatus,
                74.0f);
        }

        struct ProjectCodeWorkspaceDrawResult
        {
            bool handled{};
            bool returnToScene{};
        };

        [[nodiscard]] std::uint64_t next_workspace_revision(
            const std::uint64_t value) noexcept
        {
            const std::uint64_t next = value + 1u;
            return next == 0u ? 1u : next;
        }

        void synchronize_project_code_draft(
            CodeWorkspaceUiState& state)
        {
            const auto document = state.controller.active_document();
            if (!document)
            {
                state.draft.clear();
                state.draftHandle.reset();
                state.draftRevision = 0u;
                return;
            }
            if (!state.draftHandle
                || *state.draftHandle != document->handle
                || state.draftRevision != document->revision)
            {
                state.draft = document->text;
                state.draftHandle = document->handle;
                state.draftRevision = document->revision;
            }
        }

        [[nodiscard]] bool configure_project_code_workspace(
            EditorState& editor)
        {
            auto& state = editor.projectCodeWorkspace;
            if (editor.projectRoot.empty())
            {
                state.status = "Open a project before entering Project Scripts.";
                return false;
            }

            const std::filesystem::path root = resolve_editor_path(
                std::filesystem::path{editor.projectRoot});
            const std::filesystem::path scriptsRoot = root / "scripts";
            std::vector<editor_code_workspace::PathRequest> paths{};
            std::error_code error{};
            if (std::filesystem::exists(scriptsRoot, error) && !error)
            {
                for (const auto& entry :
                     std::filesystem::directory_iterator(scriptsRoot, error))
                {
                    if (error)
                        break;
                    std::error_code entryError{};
                    if (!entry.is_regular_file(entryError) || entryError)
                        continue;
                    const auto path = entry.path();
                    if (!path.filename().string().ends_with(".ascript.cpp"))
                        continue;
                    const auto relative = path.lexically_relative(root);
                    if (relative.empty() || relative.is_absolute())
                        continue;
                    paths.push_back(editor_code_workspace::PathRequest{
                        .relative_path = relative.generic_string(),
                        .writable = true});
                }
            }
            std::sort(
                paths.begin(), paths.end(),
                [](const auto& left, const auto& right)
                {
                    return left.relative_path < right.relative_path;
                });
            if (paths.empty())
            {
                state.status = "This project has no project-local .ascript.cpp files.";
                return false;
            }

            std::string key = editor.projectId + "\n"
                + root.generic_string();
            for (const auto& path : paths)
                key += "\n" + path.relative_path;
            if (state.configurationKey == key
                && state.controller.snapshot().configured)
            {
                return true;
            }

            auto authority = state.authority;
            authority.surface_revision = next_workspace_revision(
                authority.surface_revision);
            authority.project_revision = next_workspace_revision(
                authority.project_revision);
            authority.async_generation = next_workspace_revision(
                authority.async_generation);
            const auto opened = state.controller.open(
                editor_code_workspace::OpenRequest{
                    .kind = editor_code_workspace::WorkspaceKind::project_scripts,
                    .workspace_id = "project-scripts:" + editor.projectId,
                    .root = root,
                    .authority = authority,
                    .paths = std::move(paths),
                    .maximum_file_bytes = 256u * 1024u,
                    .maximum_tabs = 32u});
            state.status = opened.reason;
            if (!opened)
                return false;
            state.authority = authority;
            state.configurationKey = std::move(key);
            state.draft.clear();
            state.draftHandle.reset();
            state.draftRevision = 0u;
            state.discardClose.reset();
            state.discardReloadArmed = false;
            synchronize_project_code_draft(state);
            return true;
        }

        [[nodiscard]] ProjectCodeWorkspaceDrawResult
        draw_project_code_workspace(
            EditorState& editor,
            const float width,
            const float height)
        {
            ProjectCodeWorkspaceDrawResult result{
                .handled = !editor.projectRoot.empty()};
            auto& state = editor.projectCodeWorkspace;
            editor.showOutliner = false;
            gui::label("Project Scripts");
            const std::array navigation{
                gui::SegmentedButtonSpec{
                    .label = "3D Scene",
                    .width = 104.0f,
                    .active = false},
                gui::SegmentedButtonSpec{
                    .label = "Project Scripts",
                    .width = 144.0f,
                    .active = true}};
            if (const auto selected = gui::segmented_button_row(
                    navigation, 30.0f, 4.0f);
                selected && *selected == 0u)
            {
                result.returnToScene = true;
                return result;
            }

            if (!configure_project_code_workspace(editor))
            {
                gui::wrapped_label(state.status, width);
                return result;
            }

            auto snapshot = state.controller.snapshot();
            gui::property_row("Root", snapshot.canonical_root, 72.0f);
            std::vector<gui::TabButtonSpec> tabs{};
            tabs.reserve(snapshot.documents.size());
            for (const auto& document : snapshot.documents)
            {
                tabs.push_back(gui::TabButtonSpec{
                    .id = document.relative_path,
                    .label = document.label,
                    .width = (std::clamp)(
                        64.0f + static_cast<float>(document.label.size()) * 6.5f,
                        120.0f,
                        260.0f),
                    .active = document.active,
                    .closable = true,
                    .dirty = document.dirty});
            }
            const auto tabResult = gui::responsive_tab_bar_buttons(
                gui::ResponsiveTabBarOptions{
                    .overflow_id = "project-code-tabs-overflow",
                    .overflow_label = "Scripts",
                    .tabs = tabs,
                    .available_width = width,
                    .overflow_width = 132.0f,
                    .height = 30.0f,
                    .gap = 2.0f,
                    .presentation = gui::TabBarPresentation::Document});
            if (tabResult.closed_index
                && *tabResult.closed_index < snapshot.documents.size())
            {
                const auto& closing = snapshot.documents[*tabResult.closed_index];
                const bool discard = state.discardClose
                    && *state.discardClose == closing.handle;
                const auto closed = state.controller.close(
                    closing.handle,
                    state.authority,
                    closing.revision,
                    discard);
                state.status = closed.reason;
                if (closed)
                {
                    state.discardClose.reset();
                    state.draftHandle.reset();
                }
                else if (closed.code
                    == editor_code_workspace::ResultCode::dirty_document)
                {
                    state.discardClose = closing.handle;
                    state.status += " Close the same tab again to discard its edits.";
                }
                synchronize_project_code_draft(state);
                snapshot = state.controller.snapshot();
            }
            else if (tabResult.selected_index
                && *tabResult.selected_index < snapshot.documents.size())
            {
                const auto selected = state.controller.activate(
                    snapshot.documents[*tabResult.selected_index].handle,
                    state.authority);
                state.status = selected.reason;
                state.discardClose.reset();
                synchronize_project_code_draft(state);
                snapshot = state.controller.snapshot();
            }

            const auto active = state.controller.active_document();
            if (!active)
            {
                gui::wrapped_label(
                    "All script tabs are closed. Leave and reopen Project Scripts to restore the project catalog.",
                    width);
                gui::wrapped_label(state.status, width);
                return result;
            }
            synchronize_project_code_draft(state);

            const std::array fileActions{
                gui::InlineButtonSpec{.label = "Save", .width = 72.0f},
                gui::InlineButtonSpec{.label = "Reload", .width = 82.0f},
                gui::InlineButtonSpec{.label = "Copy Path", .width = 96.0f}}
            ;
            if (const auto action = gui::inline_button_row(
                    fileActions, 29.0f, 5.0f))
            {
                if (*action == 0u)
                {
                    const auto saved = state.controller.save(
                        active->handle, state.authority, active->revision);
                    state.status = saved.reason;
                    if (saved)
                    {
                        state.discardReloadArmed = false;
                        push_editor_log(
                            editor,
                            "[script] " + saved.reason + " "
                                + active->relative_path);
                    }
                }
                else if (*action == 1u)
                {
                    if (active->dirty && !state.discardReloadArmed)
                    {
                        state.discardReloadArmed = true;
                        state.status = "Reload again to discard the active tab's edits.";
                    }
                    else
                    {
                        const auto reloaded = state.controller.reload(
                            active->handle,
                            state.authority,
                            active->revision,
                            true);
                        state.status = reloaded.reason;
                        state.discardReloadArmed = false;
                        if (reloaded)
                            state.draftHandle.reset();
                    }
                }
                else
                {
                    const std::filesystem::path absolute =
                        std::filesystem::path{snapshot.canonical_root}
                        / active->relative_path;
                    (void)gui::set_clipboard_text(
                        display_project_path(absolute));
                    state.status = "Copied the active project script path.";
                }
                synchronize_project_code_draft(state);
            }

            gui::property_row(
                "Document",
                active->relative_path + (active->dirty ? " *" : ""),
                82.0f);
            gui::property_row(
                "Access",
                active->writable ? "Writable project source" : "Read-only",
                82.0f);
            gui::property_row(
                "Encoding",
                active->utf8_bom
                    ? "UTF-8 with preserved BOM"
                    : "UTF-8",
                82.0f);
            gui::label("Go To Line");
            (void)gui::edit_box(
                state.gotoLine, {(std::min)(160.0f, width), 28.0f}, 12u, false);
            if (gui::button(
                    "Go To Line",
                    {(std::min)(132.0f, width), 28.0f}))
            {
                std::size_t target{};
                const auto parsed = std::from_chars(
                    state.gotoLine.data(),
                    state.gotoLine.data() + state.gotoLine.size(),
                    target);
                if (parsed.ec != std::errc{}
                    || parsed.ptr != state.gotoLine.data()
                        + state.gotoLine.size()
                    || target == 0u)
                {
                    state.status = "Go To Line requires a positive whole number.";
                }
                else
                {
                    const auto located = state.controller.goto_line(
                        active->handle,
                        state.authority,
                        active->revision,
                        target);
                    state.status = located.reason;
                    if (located)
                    {
                        state.gotoTarget = target;
                        state.gotoGeneration = next_workspace_revision(
                            state.gotoGeneration);
                    }
                }
            }

            const std::string editorId = "project-code-editor:"
                + state.configurationKey + ":" + active->relative_path;
            const auto edited = gui::source_editor(
                state.draft,
                gui::SourceEditorOptions{
                    .id = editorId,
                    .size = {
                        (std::max)(180.0f, width),
                        (std::max)(320.0f, height - 225.0f)},
                    .max_chars = 256u * 1024u,
                    .show_context_menu = true,
                    .read_only = !active->writable,
                    .can_undo = active->can_undo,
                    .can_redo = active->can_redo,
                    .goto_line = state.gotoTarget,
                    .goto_generation = state.gotoGeneration});
            if (edited.undo_requested || edited.redo_requested)
            {
                const auto changed = edited.undo_requested
                    ? state.controller.undo(
                        active->handle,
                        state.authority,
                        state.draftRevision)
                    : state.controller.redo(
                        active->handle,
                        state.authority,
                        state.draftRevision);
                state.status = changed.reason;
                if (changed)
                    state.draftHandle.reset();
                state.discardReloadArmed = false;
                synchronize_project_code_draft(state);
            }
            else if (edited.edit.changed)
            {
                const auto replaced = state.controller.replace_text(
                    active->handle,
                    state.authority,
                    state.draftRevision,
                    state.draft);
                state.status = replaced.reason;
                if (!replaced)
                    state.draftHandle.reset();
                state.discardReloadArmed = false;
                synchronize_project_code_draft(state);
            }
            else if (edited.copied)
            {
                state.status = "Copied the selected UTF-8 project source.";
            }
            if (edited.invalid_utf8_replacements != 0u)
            {
                state.status = epochengine::format_text(
                    "Replaced {} invalid UTF-8 input byte(s) with visible U+FFFD markers; review before saving.",
                    edited.invalid_utf8_replacements);
            }
            gui::property_row(
                "Cursor",
                epochengine::format_text(
                    "line {}, column {}",
                    edited.cursor_line,
                    edited.cursor_column),
                82.0f);
            gui::property_row(
                "Scroll",
                epochengine::format_text(
                    "horizontal {:.0f}px, vertical {:.0f}px",
                    edited.horizontal_scroll,
                    edited.vertical_scroll),
                82.0f);
            gui::wrapped_label(
                state.status
                    + " Mouse-wheel scrolls vertically; Shift+mouse-wheel scrolls horizontally. Up/Down, Page Up/Down, Home/End, and Ctrl+Home/End keep the caret visible.",
                width);
            return result;
        }

