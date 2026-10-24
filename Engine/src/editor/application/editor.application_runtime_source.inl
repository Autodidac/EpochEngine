// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::filesystem::path editor_runtime_root()
        {
            if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
                return runtimeRoot.lexically_normal();

            if (const auto exeRoot = epochengine::core::path::find_epoch_repo_root(epochengine::core::path::executable_dir()); !exeRoot.empty())
                return exeRoot.lexically_normal();

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            return ec ? std::filesystem::path{} : cwd.lexically_normal();
        }

        [[nodiscard]] std::uint64_t editor_steady_tick_ns() noexcept
        {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        [[nodiscard]] std::filesystem::path resolve_editor_path(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};
            if (path.is_absolute())
                return path.lexically_normal();
            if (const auto data = core::path::candidate_data_root();
                !data.empty() && *path.begin() == "Projects")
                return editor_resolve_candidate_project_path(path, true);
            return (editor_runtime_root() / path).lexically_normal();
        }

        [[nodiscard]] std::filesystem::path active_editor_scene_path(const EditorState& state)
        {
            if (state.projectScenePath.empty())
                return {};
            return resolve_editor_path(std::filesystem::path{ state.projectScenePath });
        }

        [[nodiscard]] bool save_editor_scene_snapshot(EditorState& state)
        {
            const auto scenePath = active_editor_scene_path(state);
            if (scenePath.empty())
                return false;

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
            if (state.outlinerToolTab == OutlinerToolTab::TileMap
                && (state.tileMapWorkspace
                    || state.mainSurface == EditorMainSurface::Game2D))
            {
                auto* tileMap = tilemap_workspace_controller(state);
                if (!tileMap)
                {
                    state.projectStatus = state.tileMapStatus;
                    push_editor_log(state, "[tilemap] " + state.tileMapStatus);
                    return false;
                }
                const auto published = tileMap->save_and_publish();
                state.tileMapStatus = std::string(tileMap->status());
                if (!published)
                {
                    state.projectStatus = "Tile-map save blocked: "
                        + state.tileMapStatus;
                    push_editor_log(state, "[tilemap] " + state.projectStatus);
                    return false;
                }
            }
#endif

            synchronize_editor_selection(state);
            const scene::SceneSnapshot projection = capture_editor_scene_projection(state);
            const auto saved = scene::persistence::save_scene_snapshot_atomic(
                scenePath,
                projection,
                scene::persistence::ScenePersistenceOptions{
                    .requirement = editor_application_profile(state.applicationKind).allow_project_run
                        ? scene::SceneDocumentRequirement::runnable
                        : scene::SceneDocumentRequirement::authoring
                });
            if (!saved)
            {
                state.projectStatus = epochengine::format_text(
                    "Scene save blocked ({}): {}",
                    scene::persistence::scene_persistence_status_name(saved.status),
                    saved.error.empty() ? std::string("no committed scene evidence") : saved.error);
                push_editor_log(state, "[scene] " + state.projectStatus);
                return false;
            }

            const bool committed = saved.evidence.atomic_replace_committed
                && saved.evidence.temporary_bytes_verified;
            if (!committed
                || !state.projectLifecycle.record_scene_commit(saved.evidence.revision))
            {
                state.projectStatus = "Scene bytes were written, but lifecycle evidence could not be committed.";
                push_editor_log(state, "[scene] " + state.projectStatus);
                return false;
            }

            state.sceneDocumentRevision = saved.evidence.revision;
            state.projectStatus = epochengine::format_text(
                "Saved scene revision {} ({} bytes).",
                saved.evidence.revision,
                saved.evidence.bytes);
            return true;
        }

        [[nodiscard]] bool load_editor_scene_snapshot(EditorState& state)
        {
            const auto scenePath = active_editor_scene_path(state);
            if (scenePath.empty())
                return false;

            auto loaded = scene::persistence::load_scene_snapshot(
                scenePath,
                scene::persistence::ScenePersistenceOptions{
                    .requirement = scene::SceneDocumentRequirement::authoring
                });
            if (!loaded.result)
                return false;

            scene::SceneSnapshot& snapshot = loaded.snapshot;
            if (!snapshot.project_id.empty()
                && !state.projectId.empty()
                && snapshot.project_id != state.projectId)
            {
                push_editor_log(state, "[scene] Saved scene belongs to another project and was not loaded.");
                return false;
            }

            std::vector<EditorEntity> entities{};
            entities.reserve(snapshot.objects.size());
            for (const scene::SceneObjectSnapshot& source : snapshot.objects)
            {
                entities.push_back(EditorEntity{
                    .sceneObjectId = source.id,
                    .name = source.name,
                    .type = source.type,
                    .category = source.category,
                    .position = source.position,
                    .rotation = source.rotation,
                    .scale = source.scale,
                    .visible = source.visible,
                    .editorOnly = source.editor_only,
                    .textureMaterial = source.texture_material
                });
            }

            state.entities = std::move(entities);
            if (!rebuild_editor_scene_document_from_snapshot(
                    state,
                    snapshot,
                    "Load saved project scene"))
            {
                return false;
            }

            state.sceneDocumentRevision = snapshot.revision;
            state.sceneLightingDirty = true;
            state.sceneInteractionDirty = true;
            normalize_editor_entity_ids(state);
            select_editor_entity(
                state,
                state.entities.empty() ? 0u : (std::min)(state.selectedEntity, state.entities.size() - 1u));
            return true;
        }

        [[nodiscard]] std::filesystem::path project_entry_source_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "source" / "epoch.main.cpp";
        }

        [[nodiscard]] std::filesystem::path project_windows_build_script_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "build_project.ps1";
        }

        [[nodiscard]] std::filesystem::path project_linux_build_script_path(
            std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot })
                / "build_project.sh";
        }

        [[nodiscard]] std::string project_artifact_stem(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            const std::string stem = root.filename().string();
            return stem == "Sandbox" ? std::string{ "EpochEngine" } : stem;
        }

        [[nodiscard]] std::filesystem::path project_windows_vcxproj_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / (project_artifact_stem(projectRoot) + ".vcxproj");
        }

        [[nodiscard]] std::filesystem::path project_output_exe_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / "build" / "bin" / "windows" / "Debug" / "x64" / (project_artifact_stem(projectRoot) + ".exe");
        }

        [[nodiscard]] std::vector<std::filesystem::path> project_output_exe_candidates(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            const std::filesystem::path outputDir = root / "build" / "bin" / "windows" / "Debug" / "x64";
            std::vector<std::filesystem::path> candidates;
            const auto add_candidate = [&](std::string name) {
                if (name.empty())
                    return;
                if (!name.ends_with(".exe"))
                    name += ".exe";
                const auto path = outputDir / name;
                for (const auto& candidate : candidates)
                {
                    if (candidate == path)
                        return;
                }
                candidates.push_back(path);
            };

            add_candidate(project_artifact_stem(projectRoot));
            add_candidate(root.filename().string());
            add_candidate("EpochEngine");
            add_candidate("EpochEditor");
            return candidates;
        }

        [[nodiscard]] std::filesystem::path project_existing_output_exe_path(std::string_view projectRoot)
        {
            std::error_code ec;
            for (const auto& candidate : project_output_exe_candidates(projectRoot))
            {
                if (std::filesystem::exists(candidate, ec) && !ec)
                    return candidate;
                ec.clear();
            }
            return project_output_exe_path(projectRoot);
        }

        [[nodiscard]] std::filesystem::path project_build_log_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "build" / "logs" / "build-debug-x64.log";
        }

        [[nodiscard]] std::filesystem::path project_notes_path(std::string_view projectRoot)
        {
            auto root = resolve_editor_path(std::filesystem::path{projectRoot});
            if (!core::path::candidate_data_root().empty())
                root = editor_resolve_candidate_project_path(root);
            return root.empty() ? std::filesystem::path{} : root / "PROJECT_NOTES.md";
        }

        [[nodiscard]] std::string read_project_notes(std::string_view projectRoot)
        {
            if (projectRoot.empty())
                return "No active project notes yet.";

            const auto notesPath = project_notes_path(projectRoot);
            std::ifstream in(notesPath, std::ios::binary);
            if (!in)
                return "No project notes have been written yet. Build, run, script, and AI sandbox actions will append notes here.";

            std::ostringstream buffer;
            buffer << in.rdbuf();
            std::string text = buffer.str();
            if (text.empty())
                return "Project notes file exists but is empty.";
            return normalize_editor_text_for_gui(text);
        }

        [[nodiscard]] std::string tail_text(std::string text, std::size_t maxChars)
        {
            if (text.size() <= maxChars)
                return text;
            return std::string("...") + text.substr(text.size() - maxChars);
        }

        void append_project_note(
            const EditorState& editor,
            std::string_view action,
            std::string_view summary,
            std::string_view usage)
        {
            if (editor.projectRoot.empty())
                return;

            const auto notesPath = project_notes_path(editor.projectRoot);
            if (notesPath.empty()) return;
            std::error_code ec;
            std::filesystem::create_directories(notesPath.parent_path(), ec);

            const bool hadNotes = std::filesystem::exists(notesPath, ec)
                && std::filesystem::file_size(notesPath, ec) > 0u;
            std::ofstream out(notesPath, std::ios::app | std::ios::binary);
            if (!out)
                return;

            if (!hadNotes)
            {
                out << "# Epoch Project Notes\n\n";
                out << "These notes are generated by the editor so ProjectLauncher actions, script runs, and AI sandbox passes leave visible operator evidence.\n";
            }

            out << "\n## " << action << "\n\n";
            out << "- Project: " << editor.projectName << " (" << editor.projectId << ")\n";
            out << "- Summary: " << summary << "\n";
            if (!usage.empty())
                out << "- Usage: " << usage << "\n";
            out << "- Manifest: " << editor.projectManifest << "\n";
            out << "- Scene: " << editor.projectScenePath << "\n";
            out << "- Active script: " << editor.activeScript << "\n";
            out << "- Build log: " << resolve_editor_path(project_build_log_path(editor.projectRoot)).generic_string() << "\n";
        }

        [[nodiscard]] std::string ai_build_path_stamp(const std::filesystem::path& path)
        {
            if (path.empty())
                return "(empty)#missing";

            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            const auto normalized = std::filesystem::absolute(resolvedPath, ec).lexically_normal().generic_string();
            ec.clear();
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return normalized + "#missing";

            const auto writeTime = std::filesystem::last_write_time(resolvedPath, ec);
            if (ec)
                return normalized + "#time-error";

            std::string stamp = normalized + "#" + std::to_string(writeTime.time_since_epoch().count());
            ec.clear();
            if (std::filesystem::is_regular_file(resolvedPath, ec) && !ec)
            {
                ec.clear();
                const auto size = std::filesystem::file_size(resolvedPath, ec);
                if (!ec)
                    stamp += "#" + std::to_string(size);
            }
            return stamp;
        }

        [[nodiscard]] std::string ai_continuous_build_fingerprint(
            const EditorState& editor,
            const std::string& activeScriptSource,
            const std::filesystem::path& pathsManifest)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            std::string fingerprint = editor.projectId + "|" + editor.projectRoot + "|" + editor.activeScript;
            fingerprint += "|" + ai_build_path_stamp(project_entry_source_path(editor.projectRoot));
            fingerprint += "|" + ai_build_path_stamp(activeScriptSource);
            fingerprint += "|" + ai_build_path_stamp(editor.projectManifest);
            fingerprint += "|" + ai_build_path_stamp(pathsManifest);
            fingerprint += "|" + ai_build_path_stamp(project_windows_build_script_path(editor.projectRoot));
            fingerprint += "|" + ai_build_path_stamp(root / "scripts");
            return fingerprint;
        }

        [[nodiscard]] std::string bounded_file_tail(
            const std::filesystem::path& path,
            std::size_t maximumBytes = 64u * 1024u)
        {
            if (path.empty() || maximumBytes == 0u)
                return {};
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input.is_open())
                return {};
            const std::streampos end = input.tellg();
            if (end <= std::streampos{0})
                return {};
            const auto available = static_cast<std::uint64_t>(
                static_cast<std::streamoff>(end));
            const std::uint64_t requested = (std::min)(
                available,
                static_cast<std::uint64_t>(maximumBytes));
            input.seekg(
                static_cast<std::streamoff>(available - requested),
                std::ios::beg);
            if (!input.good())
                return {};
            std::string bytes(static_cast<std::size_t>(requested), '\0');
            input.read(
                bytes.data(),
                static_cast<std::streamsize>(bytes.size()));
            if (input.gcount()
                != static_cast<std::streamsize>(bytes.size()))
            {
                return {};
            }
            return bytes;
        }

        // MSBuild /verbosity:normal emits enormous cl.exe module command
        // lines. Displaying its raw 64 KiB tail inside the chat can force the
        // editor's text layout to process a single unbounded command line.
        // Keep the full evidence on disk; surface bounded errors only.
        [[nodiscard]] EditorProjectBuildResult test_ai_project_output(
            std::filesystem::path executable,
            std::string projectId,
            std::stop_token cancellation)
        {
            EditorProjectBuildResult result{};
            std::error_code error{};
            if (projectId.empty() || executable.empty()
                || !executable.is_absolute())
            {
                result.summary =
                    "The approved project test requires one active project and an absolute accepted output path.";
                return result;
            }

            executable = std::filesystem::weakly_canonical(
                executable,
                error);
            if (error)
            {
                result.summary =
                    "The accepted project output could not be canonicalized for testing.";
                return result;
            }
            const auto status =
                std::filesystem::symlink_status(executable, error);
            if (error
                || !std::filesystem::is_regular_file(status)
                || std::filesystem::is_symlink(status))
            {
                result.summary =
                    "The accepted project output is missing, linked, or not a regular executable.";
                return result;
            }

            const std::filesystem::path logPath =
                executable.parent_path()
                / "epoch_project_self_test.output.log";
            platform::child_process::LaunchRequest request{};
            request.executable = executable;
            request.working_directory = executable.parent_path();
            request.merged_output_path = logPath;
            request.arguments = {"--project-self-test"};
            request.correlation_key =
                "epoch.ai.project.test." + projectId;
            request.exclusive_group = "epoch.ai.project.test";
            request.display_name =
                "Epoch approved project contract test";
            request.window_mode =
                platform::child_process::WindowMode::hidden;
            const auto launched =
                platform::child_process::launch_or_focus(request);
            if (launched.code
                != platform::child_process::LaunchCode::started)
            {
                result.summary =
                    "The approved project contract test did not start: "
                    + launched.message;
                return result;
            }

            constexpr std::uint64_t timeoutNs =
                5ull * 60ull * 1'000'000'000ull;
            const auto waited = platform::child_process::wait(
                launched.handle,
                cancellation,
                timeoutNs);
            (void)platform::child_process::release(launched.handle);
            result.output_path = executable.generic_string();
            result.log_path = logPath.generic_string();
            result.cancelled =
                waited.code
                    == platform::child_process::WaitCode::cancelled;
            if (result.cancelled)
            {
                result.summary =
                    "The approved project contract test was cancelled.";
                return result;
            }
            if (waited.code
                    != platform::child_process::WaitCode::exited
                || !waited.process
                || !waited.process->exit_code_valid)
            {
                result.summary =
                    "The approved project contract test did not produce trusted exit evidence: "
                    + waited.message;
                return result;
            }

            result.succeeded = waited.process->exit_code == 0;
            result.summary = result.succeeded
                ? "The approved generated-project contract test passed."
                : epochengine::format_text(
                    "The approved generated-project contract test failed (exit {}).",
                    waited.process->exit_code);
            if (!result.succeeded)
            {
                const std::string diagnostics =
                    bounded_file_tail(logPath);
                if (!diagnostics.empty())
                {
                    result.summary +=
                        "\nVERIFIED_PROJECT_TEST_DIAGNOSTICS_BEGIN\n";
                    result.summary += diagnostics;
                    result.summary +=
                        "\nVERIFIED_PROJECT_TEST_DIAGNOSTICS_END";
                }
            }
            return result;
        }

