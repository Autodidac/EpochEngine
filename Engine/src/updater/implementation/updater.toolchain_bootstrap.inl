        [[nodiscard]] inline std::filesystem::path git_executable_in_root(
            const std::filesystem::path& root)
        {
#if defined(_WIN32)
            const std::array<std::filesystem::path, 4> candidates{
                root / "cmd/git.exe",
                root / "bin/git.exe",
                root / "mingw64/bin/git.exe",
                root / "usr/bin/git.exe"
            };

            for (const auto& candidate : candidates)
            {
                if (std::filesystem::exists(candidate))
                    return candidate;
            }
#else
            const auto candidate = root / "bin/git";
            if (std::filesystem::exists(candidate))
                return candidate;
#endif

            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_or_prepare_git(
            const std::filesystem::path& log_path)
        {
#if defined(_WIN32)
            const auto configured = env_path("GIT_EXE_PATH");
            if (!configured.empty() && std::filesystem::exists(configured))
            {
                append_log_line(log_path, "[INFO] Using configured git: " + configured.string());
                return configured;
            }

            if (const auto from_path = find_executable_on_path("git.exe");
                !from_path.empty())
            {
                append_log_line(log_path, "[INFO] Using git from PATH: " + from_path.string());
                return from_path;
            }

            const auto program_files = env_path("ProgramFiles");
            const auto program_files_x86 = env_path("ProgramFiles(x86)");
            const std::array<std::filesystem::path, 6> common_candidates{
                program_files / "Git/cmd/git.exe",
                program_files / "Git/bin/git.exe",
                program_files / "Git/mingw64/bin/git.exe",
                program_files_x86 / "Git/cmd/git.exe",
                program_files_x86 / "Git/bin/git.exe",
                program_files_x86 / "Git/mingw64/bin/git.exe"
            };

            for (const auto& candidate : common_candidates)
            {
                if (!candidate.empty() && std::filesystem::exists(candidate))
                {
                    append_log_line(log_path, "[INFO] Using installed git: " + candidate.string());
                    return candidate;
                }
            }

            const auto tools_root = managed_tools_root();
            const auto managed_root = tools_root / "g";
            if (const auto managed_git = git_executable_in_root(managed_root);
                !managed_git.empty())
            {
                append_log_line(log_path, "[INFO] Using managed git: " + managed_git.string());
                return managed_git;
            }

            std::error_code ec;
            std::filesystem::create_directories(tools_root, ec);
            if (ec)
            {
                append_log_line(log_path, "[ERROR] Failed to create updater tools directory for git.");
                return {};
            }

            const auto archive_url = query_latest_github_asset_url(
                GIT_WINDOWS_RELEASE_API_URL(),
                "MinGit-",
                "-64-bit.zip",
                "busybox",
                log_path);
            if (archive_url.empty())
                return {};

            const auto archive_path =
                make_temp_download_path("git_mingit").replace_extension(".zip");
            const auto staging_dir = tools_root / "gx";

            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove_all(managed_root, ec);

            append_log_line(log_path, "[INFO] Downloading managed git.");
            if (!download_file(archive_url, archive_path.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to download managed git archive.");
                return {};
            }

            if (!extract_archive(archive_path.string(), staging_dir.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to extract managed git archive.");
                std::filesystem::remove(archive_path, ec);
                return {};
            }

            std::filesystem::create_directories(managed_root.parent_path(), ec);
            if (!ec)
                std::filesystem::rename(staging_dir, managed_root, ec);

            if (ec)
            {
                ec.clear();
                std::filesystem::copy(
                    staging_dir,
                    managed_root,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);
            }

            std::filesystem::remove(archive_path, ec);
            std::filesystem::remove_all(staging_dir, ec);

            if (ec)
            {
                append_log_line(log_path, "[ERROR] Failed to stage managed git.");
                return {};
            }

            if (const auto managed_git = git_executable_in_root(managed_root);
                !managed_git.empty())
            {
                append_log_line(log_path, "[INFO] Managed git ready at: " + managed_git.string());
                return managed_git;
            }

            append_log_line(log_path, "[ERROR] Managed git executable is missing after extraction.");
            return {};
#else
            return find_executable_on_path("git");
#endif
        }

        [[nodiscard]] inline std::string ensure_vcpkg_git_head(
            const std::filesystem::path& git_exe,
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            int git_exit = -1;
            std::string head;
            const auto local_git_dir = vcpkg_root / ".git";

            const auto resolve_local_head = [&]() -> std::string
            {
                if (!std::filesystem::exists(local_git_dir))
                    return {};

                const auto resolved_git_dir = trim_ascii(capture_process_output(
                    git_exe,
                    { "rev-parse", "--absolute-git-dir" },
                    vcpkg_root,
                    &git_exit));

                if (git_exit != 0 || resolved_git_dir.empty())
                {
                    append_log_line(
                        log_path,
                        "[WARN] Managed vcpkg git-dir probe failed with exit code "
                        + std::to_string(git_exit) + ".");
                    return {};
                }

                std::error_code local_ec;
                const auto normalized_local_git_dir =
                    std::filesystem::weakly_canonical(local_git_dir, local_ec);

                std::error_code resolved_ec;
                const auto normalized_resolved_git_dir =
                    std::filesystem::weakly_canonical(
                        std::filesystem::path{ resolved_git_dir },
                        resolved_ec);

                if (local_ec || resolved_ec
                    || normalized_local_git_dir != normalized_resolved_git_dir)
                {
                    append_log_line(
                        log_path,
                        "[WARN] Managed vcpkg git resolution escaped the sandboxed repo: "
                        + resolved_git_dir);

                    return {};
                }

                std::string local_head = capture_process_output(
                    git_exe,
                    {
                        "--git-dir=" + local_git_dir.string(),
                        "--work-tree=" + vcpkg_root.string(),
                        "rev-parse",
                        "--verify",
                        "HEAD"
                    },
                    vcpkg_root,
                    &git_exit);

                if (git_exit == 0 && !local_head.empty())
                    return lower_ascii(trim_ascii(std::move(local_head)));

                append_log_line(
                    log_path,
                    "[WARN] Managed vcpkg HEAD probe failed with exit code "
                    + std::to_string(git_exit) + ".");

                return {};
            };

            if (head = resolve_local_head(); !head.empty())
                return head;

            append_log_line(log_path, "[INFO] Initializing managed vcpkg git registry snapshot.");

            int init_exit = -1;
            if (!run_process_hidden(
                git_exe,
                { "-C", vcpkg_root.string(), "init" },
                vcpkg_root,
                log_path,
                true,
                &init_exit) || init_exit != 0)
            {
                append_log_line(log_path, "[ERROR] Failed to initialize the managed vcpkg git registry.");
                return {};
            }

            if (!std::filesystem::exists(local_git_dir))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg git init did not create a local .git directory.");
                return {};
            }

            const std::array<std::vector<std::string>, 4> setup_steps{
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "user.name",
                    "Epoch Updater"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "user.email",
                    "updater@epoch.local"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "config",
                    "core.longpaths",
                    "true"
                },
                std::vector<std::string>{
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "add",
                    "--",
                    "ports",
                    "versions",
                    "scripts",
                    "triplets",
                    ".vcpkg-root"
                }
            };

            for (const auto& args : setup_steps)
            {
                int step_exit = -1;
                if (!run_process_hidden(
                    git_exe,
                    args,
                    vcpkg_root,
                    log_path,
                    true,
                    &step_exit) || step_exit != 0)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare the managed vcpkg git registry.");
                    return {};
                }
            }

            int commit_exit = -1;
            log_info("Committing managed vcpkg registry snapshot.");
            if (!run_process_hidden(
                git_exe,
                {
                    "--git-dir=" + local_git_dir.string(),
                    "--work-tree=" + vcpkg_root.string(),
                    "commit",
                    "--no-gpg-sign",
                    "-m",
                    "Managed vcpkg registry snapshot"
                },
                vcpkg_root,
                log_path,
                true,
                &commit_exit))
            {
                append_log_line(log_path, "[ERROR] Failed to launch git commit for the managed vcpkg registry.");
                return {};
            }

            if (commit_exit != 0)
            {
                if (head = resolve_local_head(); !head.empty())
                    return head;

                append_log_line(log_path, "[ERROR] Managed vcpkg git registry did not produce a usable HEAD revision.");
                return {};
            }

            if (head = resolve_local_head(); !head.empty())
            {
                log_info("Managed vcpkg registry snapshot ready.");
                return head;
            }

            append_log_line(log_path, "[ERROR] Failed to resolve the managed vcpkg git HEAD revision.");
            return {};
        }

        [[nodiscard]] inline std::filesystem::path find_or_prepare_vcpkg(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& log_path)
        {
            if (const auto installed = find_installed_vcpkg(manifest_root, log_path);
                !installed.empty())
            {
                return installed;
            }

            const std::string baseline =
                read_manifest_builtin_baseline(manifest_root);
            const std::string resolved_ref =
                baseline.empty() ? std::string{ VCPKG_DEFAULT_REF } : baseline;
            const std::string safe_ref = shorten_token(resolved_ref);

            const auto tools_root = managed_tools_root();
            const auto managed_root = tools_root / ("v-" + safe_ref);
            const auto managed_exe = managed_root / VCPKG_EXECUTABLE_NAME();

            if (const auto prepared_vcpkg = validated_vcpkg_executable_in_root(managed_root);
                !prepared_vcpkg.empty())
            {
                log_info("Using managed vcpkg toolchain.");
                append_log_line(log_path, "[INFO] Using managed vcpkg: " + managed_root.string());
                return prepared_vcpkg;
            }

            std::error_code ec;
            std::filesystem::create_directories(tools_root, ec);
            if (ec)
            {
                log_error("Failed to create updater tools directory.");
                append_log_line(log_path, "[ERROR] Failed to create updater tools directory: " + tools_root.string());
                return {};
            }

            const auto archive_path =
                make_temp_download_path("vcpkg_" + safe_ref).replace_extension(".zip");
            const auto staging_dir = tools_root / ("vx-" + safe_ref);
            const auto bootstrap_script = managed_root / VCPKG_BOOTSTRAP_SCRIPT_NAME();

            log_info("Downloading managed vcpkg toolchain.");
            append_log_line(log_path, "[INFO] Preparing managed vcpkg ref: " + resolved_ref);
            append_log_line(log_path, "[INFO] Managed vcpkg target: " + managed_root.string());

            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove_all(managed_root, ec);

            if (!download_file(VCPKG_ARCHIVE_URL(resolved_ref), archive_path.string()))
            {
                if (resolved_ref == VCPKG_DEFAULT_REF)
                {
                    append_log_line(log_path, "[ERROR] Failed to download managed vcpkg archive.");
                    return {};
                }

                append_log_line(
                    log_path,
                    "[WARN] Managed vcpkg ref " + resolved_ref
                    + " was not downloadable; falling back to " + std::string{ VCPKG_DEFAULT_REF } + ".");

                std::filesystem::remove(archive_path, ec);
                if (!download_file(VCPKG_ARCHIVE_URL(VCPKG_DEFAULT_REF), archive_path.string()))
                {
                    append_log_line(log_path, "[ERROR] Failed to download fallback managed vcpkg archive.");
                    return {};
                }
            }

            if (!extract_archive(archive_path.string(), staging_dir.string()))
            {
                append_log_line(log_path, "[ERROR] Failed to extract managed vcpkg archive.");
                std::filesystem::remove(archive_path, ec);
                return {};
            }

            std::filesystem::path bootstrap_candidate;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(staging_dir, ec))
            {
                if (ec)
                    break;

                if (entry.is_regular_file()
                    && entry.path().filename() == VCPKG_BOOTSTRAP_SCRIPT_NAME())
                {
                    bootstrap_candidate = entry.path();
                    break;
                }
            }

            if (bootstrap_candidate.empty())
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg archive did not contain " + VCPKG_BOOTSTRAP_SCRIPT_NAME() + ".");
                std::filesystem::remove(archive_path, ec);
                std::filesystem::remove_all(staging_dir, ec);
                return {};
            }

            const std::filesystem::path extracted_root = bootstrap_candidate.parent_path();
            const auto normalized_for_compare = [](const std::filesystem::path& path)
                {
                    std::error_code path_ec;
                    auto absolute = std::filesystem::absolute(path, path_ec);
                    if (path_ec)
                    {
                        path_ec.clear();
                        absolute = path;
                    }

                    auto text = absolute.lexically_normal().string();
#if defined(_WIN32)
                    text = lower_ascii(std::move(text));
#endif
                    return text;
                };
            ec.clear();
            const bool direct_staging_root =
                normalized_for_compare(extracted_root)
                == normalized_for_compare(staging_dir);
            if (direct_staging_root)
            {
                std::filesystem::create_directories(managed_root, ec);
                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare managed vcpkg destination directory.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }

                std::filesystem::copy(
                    staging_dir,
                    managed_root,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);

                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to copy managed vcpkg files into place.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }
            }
            else
            {
                std::filesystem::create_directories(managed_root, ec);
                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to prepare managed vcpkg destination directory.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }

                std::filesystem::copy(
                    extracted_root,
                    managed_root,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);

                if (ec)
                {
                    append_log_line(log_path, "[ERROR] Failed to copy managed vcpkg files into place.");
                    std::filesystem::remove(archive_path, ec);
                    std::filesystem::remove_all(staging_dir, ec);
                    return {};
                }
            }

            std::filesystem::remove(archive_path, ec);
            std::filesystem::remove_all(staging_dir, ec);

            if (!std::filesystem::exists(bootstrap_script))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg bootstrap script is missing.");
                return {};
            }

#if defined(_WIN32)
            auto cmd = env_path("ComSpec");
            if (cmd.empty())
                cmd = std::filesystem::path{ "C:\\Windows\\System32\\cmd.exe" };
#endif

            int bootstrap_exit = -1;
            append_log_line(log_path, "[INFO] Bootstrapping managed vcpkg.");

#if defined(_WIN32)
            if (!run_process_hidden(
                cmd,
                {
                    "/C",
                    bootstrap_script.string(),
                    "-disableMetrics"
                },
                managed_root,
                log_path,
                true,
                &bootstrap_exit) || bootstrap_exit != 0)
#else
            if (!run_process_hidden(
                std::filesystem::path{ "/bin/bash" },
                {
                    bootstrap_script.string(),
                    "-disableMetrics"
                },
                managed_root,
                log_path,
                true,
                &bootstrap_exit) || bootstrap_exit != 0)
#endif
            {
                append_log_line(
                    log_path,
                    "[ERROR] Managed vcpkg bootstrap failed with exit code " + std::to_string(bootstrap_exit));
                log_error("Managed vcpkg bootstrap failed. See logs/epoch_source_update.log for details.");
                return {};
            }

            if (!std::filesystem::exists(managed_exe))
            {
                append_log_line(log_path, "[ERROR] Managed vcpkg executable is missing after bootstrap.");
                return {};
            }

            append_log_line(log_path, "[INFO] Managed vcpkg ready at: " + managed_exe.string());
            log_info("Managed vcpkg ready.");
            return managed_exe;
        }

        [[nodiscard]] inline bool prepare_manifest_for_managed_vcpkg_registry(
            const std::filesystem::path& manifest_root,
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            const auto manifest_file = manifest_root / "vcpkg.json";
            if (!std::filesystem::exists(manifest_file))
            {
                append_log_line(log_path, "[ERROR] Source manifest file is missing for vcpkg preparation.");
                return false;
            }

            const auto config_file = manifest_root / "vcpkg-configuration.json";
            std::error_code ec;
            std::filesystem::remove(config_file, ec);

            const auto git_exe = find_or_prepare_git(log_path);
            if (git_exe.empty())
            {
                append_log_line(log_path, "[ERROR] Could not locate or prepare git for the managed vcpkg registry.");
                return false;
            }

            auto manifest_text = read_text_file(manifest_file);
            if (manifest_text.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to read the source manifest before vcpkg preparation.");
                return false;
            }

            const auto registry_head = ensure_vcpkg_git_head(git_exe, vcpkg_root, log_path);
            if (registry_head.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to prepare the managed vcpkg registry revision.");
                return false;
            }

            static const std::regex k_baseline_regex(
                R"REGEX("builtin-baseline"\s*:\s*"[0-9A-Fa-f]+")REGEX",
                std::regex::optimize);
            const std::string baseline_line =
                "\"builtin-baseline\": \"" + registry_head + "\"";

            if (std::regex_search(manifest_text, k_baseline_regex))
            {
                manifest_text = std::regex_replace(
                    manifest_text,
                    k_baseline_regex,
                    baseline_line,
                    std::regex_constants::format_first_only);
            }
            else
            {
                const auto brace = manifest_text.find('{');
                if (brace == std::string::npos)
                {
                    append_log_line(log_path, "[ERROR] Source manifest is malformed and could not be updated.");
                    return false;
                }

                manifest_text.insert(brace + 1, "\n  " + baseline_line + ",");
            }

            {
                std::ofstream manifest_out(manifest_file, std::ios::binary | std::ios::trunc);
                if (!manifest_out)
                {
                    append_log_line(log_path, "[ERROR] Failed to rewrite the source manifest for managed vcpkg.");
                    return false;
                }

                manifest_out << manifest_text;
            }

            append_log_line(log_path, "[INFO] Reconfigured the source snapshot to use the managed vcpkg git registry.");
            return true;
        }

        [[nodiscard]] inline bool patch_cmake_policy_overlay_portfile(
            const std::filesystem::path& portfile,
            const std::filesystem::path& log_path,
            std::string_view port_name)
        {
            auto portfile_text = read_text_file(portfile);
            if (portfile_text.empty())
            {
                append_log_line(log_path, std::string{ "[WARN] Failed to read the staged " }
                    + std::string{ port_name } + " portfile.");
                return false;
            }

            portfile_text = std::regex_replace(portfile_text, std::regex{ "\r\n?" }, "\n");
            if (portfile_text.find("CMAKE_POLICY_VERSION_MINIMUM=3.5") != std::string::npos)
            {
                append_log_line(log_path, std::string{ "[INFO] Managed vcpkg " }
                    + std::string{ port_name } + " port already carries modern CMake policy handling.");
                return true;
            }

            const std::string option_line = "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n";
            const auto options_pos = portfile_text.find("\n    OPTIONS\n");
            if (options_pos != std::string::npos)
            {
                portfile_text.insert(options_pos + std::string{ "\n    OPTIONS\n" }.size(), option_line);
            }
            else
            {
                const auto configure_pos = portfile_text.find("vcpkg_cmake_configure(");
                if (configure_pos == std::string::npos)
                {
                    append_log_line(log_path, std::string{ "[WARN] Could not patch the staged " }
                        + std::string{ port_name } + " portfile.");
                    return false;
                }

                const auto line_break = portfile_text.find('\n', configure_pos);
                if (line_break == std::string::npos)
                {
                    append_log_line(log_path, std::string{ "[WARN] Could not patch the staged " }
                        + std::string{ port_name } + " portfile.");
                    return false;
                }

                portfile_text.insert(
                    line_break + 1,
                    "    OPTIONS\n"
                    "        -DCMAKE_POLICY_VERSION_MINIMUM=3.5\n");
            }

            std::ofstream out(portfile, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                append_log_line(log_path, std::string{ "[WARN] Failed to write the staged " }
                    + std::string{ port_name } + " overlay port.");
                return false;
            }

            out << portfile_text;
            append_log_line(log_path, std::string{ "[INFO] Prepared a " }
                + std::string{ port_name } + " overlay port for modern CMake policy handling.");
            return true;
        }

        [[nodiscard]] inline std::filesystem::path prepare_vcpkg_overlay_ports(
            const std::filesystem::path& vcpkg_root,
            const std::filesystem::path& log_path)
        {
            const auto overlay_root = managed_tools_root() / "ov";
            std::error_code ec;
            std::filesystem::create_directories(overlay_root, ec);
            if (ec)
            {
                append_log_line(log_path, "[WARN] Failed to create the updater overlay-ports directory.");
                return {};
            }

            bool staged_any = false;
            constexpr std::array<std::string_view, 11> k_policy_ports{
                "freetype",
                "glad",
                "glfw3",
                "libogg",
                "libvorbis",
                "raylib",
                "sdl3",
                "sfml",
                "shaderc",
                "spirv-tools",
                "zlib"
            };

            for (const auto port_name : k_policy_ports)
            {
                const auto source_port_dir = vcpkg_root / "ports" / std::string{ port_name };
                if (!std::filesystem::exists(source_port_dir))
                {
                    append_log_line(log_path, std::string{ "[WARN] Managed vcpkg " }
                        + std::string{ port_name } + " port was not found; skipping overlay patch.");
                    continue;
                }

                const auto overlay_port_dir = overlay_root / std::string{ port_name };
                ec.clear();
                std::filesystem::remove_all(overlay_port_dir, ec);
                ec.clear();
                std::filesystem::copy(
                    source_port_dir,
                    overlay_port_dir,
                    std::filesystem::copy_options::recursive
                    | std::filesystem::copy_options::overwrite_existing,
                    ec);
                if (ec)
                {
                    append_log_line(log_path, std::string{ "[WARN] Failed to stage the " }
                        + std::string{ port_name } + " overlay port.");
                    continue;
                }

                staged_any = patch_cmake_policy_overlay_portfile(
                    overlay_port_dir / "portfile.cmake",
                    log_path,
                    port_name)
                    || staged_any;
            }

            if (!staged_any)
                return {};

            append_log_line(log_path, "[INFO] Prepared updater overlay ports for modern CMake policy handling.");
            return overlay_root;
        }

