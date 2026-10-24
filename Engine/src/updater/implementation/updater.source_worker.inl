#if defined(_WIN32)
        [[nodiscard]] inline std::wstring to_wide(const std::string& value)
        {
            if (value.empty())
                return {};

            const int required = MultiByteToWideChar(
                CP_UTF8,
                0,
                value.c_str(),
                -1,
                nullptr,
                0);

            if (required <= 0)
                return std::wstring(value.begin(), value.end());

            std::wstring wide(static_cast<std::size_t>(required - 1), L'\0');

            MultiByteToWideChar(
                CP_UTF8,
                0,
                value.c_str(),
                -1,
                wide.data(),
                required);

            return wide;
        }

        [[nodiscard]] inline std::string last_error_message(const char* prefix)
        {
            const DWORD error = GetLastError();
            std::error_code ec(static_cast<int>(error), std::system_category());
            return std::string{ prefix } + ": " + ec.message();
        }

        void append_log_line(
            const std::filesystem::path& log_path,
            const std::string& line)
        {
            if (log_path.empty())
                return;

            std::ofstream out(log_path, std::ios::binary | std::ios::app);
            if (!out)
                return;

            out << line << "\r\n";
        }

        [[nodiscard]] inline std::string build_command_line(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args)
        {
            std::string command = quote_shell_arg(executable.string());

            for (const auto& arg : args)
            {
                command.push_back(' ');
                command += quote_shell_arg(arg);
            }

            return command;
        }

        [[nodiscard]] inline bool run_process_with_visibility(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            const bool hidden,
            int* const exit_code = nullptr)
        {
            if (exit_code != nullptr)
                *exit_code = -1;

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE output_handle = INVALID_HANDLE_VALUE;
            bool close_output_handle = false;
            if (!log_path.empty())
            {
                output_handle = CreateFileW(
                    log_path.wstring().c_str(),
                    FILE_APPEND_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    &sa,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);

                if (output_handle == INVALID_HANDLE_VALUE)
                {
                    log_error(last_error_message("Failed to open updater process log"));
                    return false;
                }
                close_output_handle = true;
            }

            HANDLE input_handle = INVALID_HANDLE_VALUE;
            bool close_input_handle = false;
            const bool use_std_handles = hidden || output_handle != INVALID_HANDLE_VALUE;
            if (use_std_handles)
            {
                if (output_handle == INVALID_HANDLE_VALUE)
                {
                    output_handle = CreateFileW(
                        L"NUL",
                        GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        &sa,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        nullptr);
                    close_output_handle = output_handle != INVALID_HANDLE_VALUE;
                }

                input_handle = CreateFileW(
                    L"NUL",
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    &sa,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
                close_input_handle = input_handle != INVALID_HANDLE_VALUE;

                if (output_handle == INVALID_HANDLE_VALUE || input_handle == INVALID_HANDLE_VALUE)
                {
                    if (close_output_handle)
                        CloseHandle(output_handle);
                    if (close_input_handle)
                        CloseHandle(input_handle);
                    log_error("Failed to prepare updater child-process stdio handles.");
                    return false;
                }
            }

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = hidden ? SW_HIDE : SW_SHOWNORMAL;

            if (use_std_handles)
            {
                si.dwFlags |= STARTF_USESTDHANDLES;
                si.hStdOutput = output_handle;
                si.hStdError = output_handle;
                si.hStdInput = input_handle;
            }

            PROCESS_INFORMATION pi{};

            std::string command_line = build_command_line(executable, args);
            std::wstring command_line_wide = to_wide(command_line);
            std::wstring working_directory_wide =
                working_directory.empty() ? std::wstring{} : working_directory.wstring();

            const BOOL created = CreateProcessW(
                executable.wstring().c_str(),
                command_line_wide.data(),
                nullptr,
                nullptr,
                use_std_handles ? TRUE : FALSE,
                hidden ? CREATE_NO_WINDOW : 0,
                nullptr,
                working_directory.empty() ? nullptr : working_directory_wide.c_str(),
                &si,
                &pi);

            if (close_output_handle)
                CloseHandle(output_handle);
            if (close_input_handle)
                CloseHandle(input_handle);

            if (!created)
            {
                log_error(last_error_message("Failed to launch updater process"));
                return false;
            }

            if (wait_for_exit)
            {
                WaitForSingleObject(pi.hProcess, INFINITE);

                DWORD process_exit_code = 0;
                if (!GetExitCodeProcess(pi.hProcess, &process_exit_code))
                    process_exit_code = static_cast<DWORD>(-1);

                if (exit_code != nullptr)
                    *exit_code = static_cast<int>(process_exit_code);
            }

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }

        [[nodiscard]] inline bool run_process_hidden(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            int* const exit_code = nullptr)
        {
            return run_process_with_visibility(
                executable,
                args,
                working_directory,
                log_path,
                wait_for_exit,
                true,
                exit_code);
        }

        [[nodiscard]] inline bool launch_batch_hidden(const std::filesystem::path& script_path)
        {
            auto cmd = env_path("ComSpec");
            if (cmd.empty())
                cmd = std::filesystem::path{ "C:\\Windows\\System32\\cmd.exe" };

            return run_process_hidden(
                cmd,
                { "/C", script_path.string() },
                script_path.parent_path(),
                {},
                false,
                nullptr);
        }
#else
        void append_log_line(
            const std::filesystem::path& log_path,
            const std::string& line)
        {
            if (log_path.empty())
                return;

            std::ofstream out(log_path, std::ios::binary | std::ios::app);
            if (!out)
                return;

            out << line << '\n';
        }

        [[nodiscard]] inline std::string build_command_line(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args)
        {
            std::string command = quote_shell_arg(executable.string());

            for (const auto& arg : args)
            {
                command.push_back(' ');
                command += quote_shell_arg(arg);
            }

            return command;
        }

        [[nodiscard]] inline bool run_process_hidden(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            const std::filesystem::path& log_path,
            const bool wait_for_exit,
            int* const exit_code = nullptr)
        {
            if (exit_code != nullptr)
                *exit_code = -1;

            std::string shell_command;
            if (!working_directory.empty())
                shell_command += "cd " + quote_shell_arg(working_directory.string()) + " && ";

            shell_command += build_command_line(executable, args);

            if (!log_path.empty())
                shell_command += " >> " + quote_shell_arg(log_path.string()) + " 2>&1";
            else
                shell_command += " >/dev/null 2>&1";

            if (!wait_for_exit)
                shell_command += " &";

            const std::string wrapped = "/bin/sh -lc " + quote_shell_arg(shell_command);
            const int rc = std::system(wrapped.c_str());

            if (!wait_for_exit)
            {
                if (exit_code != nullptr)
                    *exit_code = 0;
                return rc == 0;
            }

            if (rc == -1)
                return false;

            int normalized = rc;
            if (WIFEXITED(rc))
                normalized = WEXITSTATUS(rc);

            if (exit_code != nullptr)
                *exit_code = normalized;

            return true;
        }

        [[nodiscard]] inline bool launch_batch_hidden(const std::filesystem::path& script_path)
        {
            return run_process_hidden(
                std::filesystem::path{ "/bin/sh" },
                { script_path.string() },
                script_path.parent_path(),
                {},
                false,
                nullptr);
        }
#endif

        [[nodiscard]] inline std::filesystem::path current_binary_path()
        {
            std::error_code ec;

#if defined(_WIN32)
            std::wstring module_path(MAX_PATH, L'\0');
            for (;;)
            {
                const DWORD copied = GetModuleFileNameW(
                    nullptr,
                    module_path.data(),
                    static_cast<DWORD>(module_path.size()));

                if (copied == 0)
                    break;

                if (copied < module_path.size() - 1)
                {
                    module_path.resize(copied);
                    return std::filesystem::path{ module_path }.lexically_normal();
                }

                module_path.resize(module_path.size() * 2);
            }
#endif

            if (!epochengine::core::cli::exe_path.empty())
                return std::filesystem::absolute(epochengine::core::cli::exe_path, ec).lexically_normal();

            const auto configured_binary = std::filesystem::path{ RUNTIME_BINARY_NAME() };
            if (!configured_binary.empty())
                return std::filesystem::absolute(configured_binary, ec).lexically_normal();

#if defined(_WIN32)
            return std::filesystem::absolute("EpochEditor.exe", ec).lexically_normal();
#else
            return std::filesystem::absolute("epoch", ec).lexically_normal();
#endif
        }

        [[nodiscard]] inline std::filesystem::path replacement_binary_path(const std::filesystem::path& target_binary)
        {
            const auto parent = target_binary.parent_path();
            const auto stem = target_binary.stem();
            const auto ext = target_binary.extension();
            return parent / (stem.string() + ".update" + ext.string());
        }

        [[nodiscard]] inline std::filesystem::path replacement_package_path(const std::filesystem::path& target_binary)
        {
            const auto install_token =
                shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24);
            return ensure_directory(package_cache_root() / install_token) / "main.update.pkg";
        }

        [[nodiscard]] inline std::string sanitize_cache_file_name(std::string text)
        {
            for (char& ch : text)
            {
                const auto uch = static_cast<unsigned char>(ch);
                if (!std::isalnum(uch) && ch != '-' && ch != '_' && ch != '.')
                    ch = '_';
            }

            while (!text.empty() && (text.back() == '_' || text.back() == '.'))
                text.pop_back();

            if (text.empty() || text == "." || text == "..")
                text = "epoch-update-" + std::string{ PROJECT_PACKAGED_VERSION };

            return text;
        }

        [[nodiscard]] inline std::string update_package_cache_file_name(
            const std::string_view url,
            const std::string_view archive_extension)
        {
            const std::string stripped_url = strip_url_query_and_fragment(std::string{ url });
            const auto last_separator = stripped_url.find_last_of("/\\");
            std::string file_name = last_separator == std::string::npos
                ? stripped_url
                : stripped_url.substr(last_separator + 1);

            file_name = sanitize_cache_file_name(file_name);

            if (!archive_extension.empty())
            {
                const std::string lower_name = lower_ascii(file_name);
                const std::string lower_extension = lower_ascii(std::string{ archive_extension });
                if (!lower_name.ends_with(lower_extension))
                    file_name += archive_extension;
            }

            return file_name;
        }

        [[nodiscard]] inline std::filesystem::path replacement_package_path(
            const std::filesystem::path& target_binary,
            const std::string_view url,
            const std::string_view archive_extension)
        {
            const auto install_token =
                shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24);

            return ensure_directory(package_cache_root() / install_token)
                / update_package_cache_file_name(url, archive_extension);
        }

        [[nodiscard]] inline std::filesystem::path replacement_extract_dir(const std::filesystem::path& target_binary)
        {
            const auto install_root =
                updater_cache_root()
                / shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24)
                / "extract";
            return install_root;
        }

        [[nodiscard]] std::filesystem::path source_work_root(const std::filesystem::path& target_binary)
        {
            return ensure_directory(
                managed_work_root()
                / shorten_token(
                    target_binary.parent_path().filename().string()
                    + "_" + target_binary.stem().string(),
                    24));
        }

        [[nodiscard]] std::filesystem::path source_run_tools_root(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return ensure_directory(
                target_binary.parent_path()
                / CACHE_ROOT_SUBDIR()
                / UPDATER_CACHE_SUBDIR()
                / "t"
                / shorten_token(std::string{ run_token }, 16));
        }

        [[nodiscard]] std::filesystem::path source_archive_path(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "source_snapshot";
        }

        [[nodiscard]] inline std::string archive_extension_from_url(const std::string_view url)
        {
            const auto normalized_url =
                lower_ascii(strip_url_query_and_fragment(std::string{ url }));

            if (normalized_url.ends_with(".tar.gz"))
                return ".tar.gz";
            if (normalized_url.ends_with(".tgz"))
                return ".tgz";
            if (normalized_url.ends_with(".zip"))
                return ".zip";
            return {};
        }

        [[nodiscard]] std::filesystem::path source_archive_path(
            const std::filesystem::path& target_binary,
            const std::string_view source_url)
        {
            auto archive_path = source_archive_path(target_binary);
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] std::string make_source_update_run_token()
        {
            static std::atomic<unsigned long long> s_source_run_counter{ 0 };
            const auto serial = s_source_run_counter.fetch_add(1, std::memory_order_relaxed);
            const auto tick = static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            return std::to_string(tick) + "_" + std::to_string(serial);
        }

        [[nodiscard]] std::filesystem::path source_run_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return ensure_directory(
                source_work_root(target_binary)
                / ("run_" + shorten_token(std::string{ run_token }, 40)));
        }

        [[nodiscard]] std::filesystem::path source_archive_path(
            const std::filesystem::path& target_binary,
            const std::string_view source_url,
            const std::string_view run_token)
        {
            auto archive_path = source_run_dir(target_binary, run_token) / "source_snapshot";
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] inline std::string describe_source_archive(const std::string_view /*source_url*/)
        {
            return PROJECT_SOURCE_ARCHIVE_LABEL();
        }

        [[nodiscard]] std::filesystem::path source_staging_dir(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "sx";
        }

        [[nodiscard]] std::filesystem::path source_final_dir(const std::filesystem::path& target_binary)
        {
            return source_work_root(target_binary) / "src";
        }

        [[nodiscard]] inline std::filesystem::path project_source_cache_root()
        {
            return ensure_directory(runtime_cache_root() / "project_sources" / "epoch_engine");
        }

        [[nodiscard]] inline std::filesystem::path project_source_downloads_dir()
        {
            return ensure_directory(project_source_cache_root() / "downloads");
        }

        [[nodiscard]] inline std::filesystem::path project_source_archive_path(
            const std::string_view source_url)
        {
            auto archive_path = project_source_downloads_dir() / "source_snapshot";
            const auto extension = archive_extension_from_url(source_url);
            if (!extension.empty())
                archive_path += extension;
            return archive_path;
        }

        [[nodiscard]] inline std::filesystem::path project_source_staging_dir()
        {
            return project_source_cache_root() / "staging";
        }

        [[nodiscard]] inline std::filesystem::path project_source_final_dir()
        {
            return project_source_cache_root() / "source";
        }

        [[nodiscard]] std::filesystem::path source_staging_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return source_run_dir(target_binary, run_token) / "sx";
        }

        [[nodiscard]] std::filesystem::path source_final_dir(
            const std::filesystem::path& target_binary,
            const std::string_view run_token)
        {
            return source_run_dir(target_binary, run_token) / "src";
        }

        [[nodiscard]] std::filesystem::path source_cancel_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "epoch_source_update.cancel";
        }

        [[nodiscard]] std::filesystem::path source_active_run_path(const std::filesystem::path& target_binary)
        {
            return target_binary.parent_path() / "epoch_source_update.active";
        }

#if !defined(_WIN32)
        [[nodiscard]] inline std::filesystem::path linux_source_update_lock_path()
        {
            return updater_cache_root() / "epoch_source_update.lock";
        }
#endif

        [[nodiscard]] std::filesystem::path update_handoff_log_path_for(
            const std::filesystem::path& target_binary);

        struct SourceActiveMarker
        {
            std::filesystem::path run_dir{};
            unsigned long process_id{};
            bool has_process_id{ false };
        };

        [[nodiscard]] inline bool source_cancel_requested(const std::filesystem::path& target_binary)
        {
            std::error_code ec;
            return std::filesystem::exists(source_cancel_path(target_binary), ec) && !ec;
        }

        [[nodiscard]] inline std::atomic_bool& in_process_source_update_active() noexcept
        {
            static std::atomic_bool active{ false };
            return active;
        }

        struct ScopedInProcessSourceUpdate final
        {
            bool acquired{ false };

            ScopedInProcessSourceUpdate() noexcept
            {
                bool expected = false;
                acquired = in_process_source_update_active().compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel);
            }

            ~ScopedInProcessSourceUpdate()
            {
                if (acquired)
                    in_process_source_update_active().store(false, std::memory_order_release);
            }
        };

        [[nodiscard]] inline bool stop_source_update_if_canceled(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& log_path,
            const std::string_view checkpoint)
        {
            if (!source_cancel_requested(target_binary))
                return false;

            const std::string message =
                "[WARN] Source update canceled by operator at "
                + std::string{ checkpoint } + ".";
            append_log_line(log_path, message);
            append_log_line(update_handoff_log_path_for(target_binary), message);
            log_info("Source update cancel reached " + std::string{ checkpoint } + ".");
            return true;
        }

        [[nodiscard]] inline std::string normalized_absolute_path_string(
            const std::filesystem::path& path)
        {
            std::error_code ec;
            auto absolute = std::filesystem::absolute(path, ec);
            if (ec)
            {
                ec.clear();
                absolute = path;
            }

            auto normalized = absolute.lexically_normal().string();
#if defined(_WIN32)
            normalized = lower_ascii(std::move(normalized));
#endif
            return normalized;
        }

        [[nodiscard]] inline bool path_is_inside(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            std::string root_text = normalized_absolute_path_string(root);
            std::string candidate_text = normalized_absolute_path_string(candidate);
            if (root_text.empty() || candidate_text.empty())
                return false;

            while (!root_text.empty()
                && (root_text.back() == '\\' || root_text.back() == '/'))
            {
                root_text.pop_back();
            }

            if (candidate_text == root_text)
                return true;
            if (candidate_text.size() <= root_text.size())
                return false;
            if (candidate_text.compare(0u, root_text.size(), root_text) != 0)
                return false;

            const char separator = candidate_text[root_text.size()];
            return separator == '\\' || separator == '/';
        }

        void remove_update_cache_path_best_effort(const std::filesystem::path& path)
        {
            if (path.empty())
                return;

            std::error_code ec;
            if (std::filesystem::is_directory(path, ec))
            {
                std::filesystem::remove_all(path, ec);
                return;
            }

            ec.clear();
            std::filesystem::remove(path, ec);
        }

        void cleanup_stale_source_update_runs(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& keep_run_dir = {})
        {
            const auto work_root = source_work_root(target_binary);
            std::error_code ec;
            if (!std::filesystem::exists(work_root, ec))
                return;

            const std::string keep_text = keep_run_dir.empty()
                ? std::string{}
                : normalized_absolute_path_string(keep_run_dir);

            for (std::filesystem::directory_iterator it{ work_root, ec }, end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto path = it->path();
                if (!keep_text.empty()
                    && normalized_absolute_path_string(path) == keep_text)
                {
                    continue;
                }

                const std::string name = path.filename().string();
                const bool managed_source_cache =
                    name == "sx"
                    || name == "src"
                    || name.starts_with("run_")
                    || name.starts_with(".stale")
                    || name.starts_with("source_snapshot");

                if (managed_source_cache && path_is_inside(work_root, path))
                    remove_update_cache_path_best_effort(path);
            }
        }

        void cleanup_stale_source_tool_runs(
            const std::filesystem::path& target_binary,
            const std::filesystem::path& keep_tools_dir = {})
        {
            const auto tools_root =
                target_binary.parent_path()
                / CACHE_ROOT_SUBDIR()
                / UPDATER_CACHE_SUBDIR()
                / "t";
            std::error_code ec;
            if (!std::filesystem::exists(tools_root, ec))
                return;

            const std::string keep_text = keep_tools_dir.empty()
                ? std::string{}
                : normalized_absolute_path_string(keep_tools_dir);

            for (std::filesystem::directory_iterator it{ tools_root, ec }, end; it != end; it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto path = it->path();
                if (!keep_text.empty()
                    && normalized_absolute_path_string(path) == keep_text)
                {
                    continue;
                }

                if (path_is_inside(tools_root, path) && std::filesystem::is_directory(path, ec))
                    remove_update_cache_path_best_effort(path);
            }
        }

        [[nodiscard]] inline std::optional<SourceActiveMarker> read_source_active_marker(
            const std::filesystem::path& target_binary)
        {
            const auto active_run_path = source_active_run_path(target_binary);
            const auto active_run_marker = read_text_file(active_run_path);
            const auto line_end = active_run_marker.find_first_of("\r\n");
            const auto active_run_text = trim_ascii(active_run_marker.substr(0u, line_end));
            if (active_run_text.empty())
                return std::nullopt;

            const auto work_root = source_work_root(target_binary);
            const std::filesystem::path active_run{ active_run_text };
            if (!path_is_inside(work_root, active_run))
                return std::nullopt;

            SourceActiveMarker marker{};
            marker.run_dir = active_run;

            const auto pid_marker = active_run_marker.find("pid=");
            if (pid_marker != std::string::npos)
            {
                const auto digits_begin = pid_marker + 4u;
                auto digits_end = digits_begin;
                while (digits_end < active_run_marker.size()
                    && std::isdigit(static_cast<unsigned char>(active_run_marker[digits_end])) != 0)
                {
                    ++digits_end;
                }

                if (digits_end > digits_begin)
                {
                    unsigned long parsed_pid{};
                    const auto* begin = active_run_marker.data() + digits_begin;
                    const auto* end = active_run_marker.data() + digits_end;
                    const auto [ptr, ec] = std::from_chars(begin, end, parsed_pid);
                    if (ec == std::errc{} && ptr == end && parsed_pid != 0ul)
                    {
                        marker.process_id = parsed_pid;
                        marker.has_process_id = true;
                    }
                }
            }

            return marker;
        }

        [[nodiscard]] inline bool source_update_active_marker_present(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            const auto marker = read_source_active_marker(target_binary);
            if (!marker)
                return false;
            if (active_run_out)
                *active_run_out = marker->run_dir;
            return true;
        }

        [[nodiscard]] bool source_update_active_run_exists(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            std::filesystem::path active_run;
            if (!source_update_active_marker_present(target_binary, &active_run))
                return false;

            std::error_code ec;
            if (!std::filesystem::exists(active_run, ec) || ec)
                return false;

            if (active_run_out)
                *active_run_out = active_run;
            return true;
        }

        [[nodiscard]] inline bool process_is_running(const unsigned long process_id) noexcept
        {
            if (process_id == 0ul)
                return false;
#if defined(_WIN32)
            HANDLE process = OpenProcess(
                SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                FALSE,
                static_cast<DWORD>(process_id));
            if (process == nullptr)
                return false;

            DWORD exit_code = 0u;
            const bool running =
                GetExitCodeProcess(process, &exit_code) != 0 && exit_code == STILL_ACTIVE;
            CloseHandle(process);
            return running;
#else
            const int result = ::kill(static_cast<pid_t>(process_id), 0);
            return result == 0 || errno == EPERM;
#endif
        }

#if !defined(_WIN32)
        struct ScopedLinuxSourceUpdateLock final
        {
            int descriptor{ -1 };
            bool acquired{ false };
            bool already_locked{ false };

            explicit ScopedLinuxSourceUpdateLock(const std::filesystem::path& lock_path)
            {
                int open_flags = O_RDWR | O_CREAT;
#if defined(O_CLOEXEC)
                open_flags |= O_CLOEXEC;
#endif
                descriptor = ::open(lock_path.c_str(), open_flags, S_IRUSR | S_IWUSR);
                if (descriptor < 0)
                    return;

                const int descriptor_flags = ::fcntl(descriptor, F_GETFD);
                if (descriptor_flags < 0
                    || ::fcntl(descriptor, F_SETFD, descriptor_flags | FD_CLOEXEC) < 0)
                {
                    ::close(descriptor);
                    descriptor = -1;
                    return;
                }

                if (::flock(descriptor, LOCK_EX | LOCK_NB) == 0)
                {
                    acquired = true;
                    return;
                }

                already_locked = errno == EWOULDBLOCK || errno == EAGAIN;
                ::close(descriptor);
                descriptor = -1;
            }

            ScopedLinuxSourceUpdateLock(const ScopedLinuxSourceUpdateLock&) = delete;
            ScopedLinuxSourceUpdateLock& operator=(const ScopedLinuxSourceUpdateLock&) = delete;

            ~ScopedLinuxSourceUpdateLock()
            {
                if (descriptor < 0)
                    return;

                if (acquired)
                    ::flock(descriptor, LOCK_UN);
                ::close(descriptor);
            }
        };

        inline void close_inherited_linux_source_update_lock() noexcept
        {
#if defined(__linux__)
            try
            {
                const std::string expected_lock =
                    normalized_absolute_path_string(linux_source_update_lock_path());
                std::error_code ec;
                for (std::filesystem::directory_iterator it{ "/proc/self/fd", ec }, end;
                    it != end;
                    it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    const std::string descriptor_text = it->path().filename().string();
                    int descriptor = -1;
                    const auto [ptr, parse_ec] = std::from_chars(
                        descriptor_text.data(),
                        descriptor_text.data() + descriptor_text.size(),
                        descriptor);
                    if (parse_ec != std::errc{}
                        || ptr != descriptor_text.data() + descriptor_text.size()
                        || descriptor <= 2)
                    {
                        continue;
                    }

                    ec.clear();
                    const auto linked_path = std::filesystem::read_symlink(it->path(), ec);
                    if (ec || normalized_absolute_path_string(linked_path) != expected_lock)
                        continue;

                    ::close(descriptor);
                }
            }
            catch (...)
            {
            }
#endif
        }

        [[nodiscard]] inline bool linux_source_update_lock_held()
        {
            ScopedLinuxSourceUpdateLock probe{ linux_source_update_lock_path() };
            return !probe.acquired && probe.already_locked;
        }

        [[nodiscard]] inline bool write_linux_source_active_marker(
            const std::filesystem::path& marker_path,
            const std::filesystem::path& run_dir)
        {
            const int descriptor = ::open(
                marker_path.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);
            if (descriptor < 0)
                return false;

            const std::string marker =
                run_dir.string() + "\npid="
                + std::to_string(static_cast<unsigned long>(::getpid())) + "\n";
            std::size_t written = 0u;
            while (written < marker.size())
            {
                const auto count = ::write(
                    descriptor,
                    marker.data() + written,
                    marker.size() - written);
                if (count < 0 && errno == EINTR)
                    continue;
                if (count <= 0)
                    break;
                written += static_cast<std::size_t>(count);
            }

            const bool closed = ::close(descriptor) == 0;
            if (written == marker.size() && closed)
                return true;

            std::error_code ec;
            std::filesystem::remove(marker_path, ec);
            return false;
        }
#endif

        struct ScopedSourceActiveMarkerCleanup final
        {
            std::filesystem::path marker_path{};
            bool owns_marker{ false };

            ~ScopedSourceActiveMarkerCleanup()
            {
                if (!owns_marker)
                    return;

                std::error_code ec;
                std::filesystem::remove(marker_path, ec);
            }
        };

        [[nodiscard]] inline bool file_recently_modified(
            const std::filesystem::path& path,
            const std::chrono::seconds max_age) noexcept
        {
            std::error_code ec;
            const auto write_time = std::filesystem::last_write_time(path, ec);
            if (ec)
                return false;

            const auto now = std::filesystem::file_time_type::clock::now();
            return write_time >= now || (now - write_time) <= max_age;
        }

        [[nodiscard]] std::filesystem::path source_update_log_path_for(
            const std::filesystem::path& target_binary)
        {
            return ensure_directory(target_binary.parent_path() / "logs") / "epoch_source_update.log";
        }

        [[nodiscard]] std::filesystem::path update_handoff_log_path_for(
            const std::filesystem::path& target_binary)
        {
            return ensure_directory(target_binary.parent_path() / "logs") / "epoch_update_handoff.log";
        }

        [[nodiscard]] inline bool contains_text(
            const std::string_view text,
            const std::string_view needle) noexcept
        {
            return !needle.empty() && text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] inline bool source_update_terminal_evidence(
            const std::string_view source_log,
            const std::string_view handoff_log) noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return contains_text(source_log, needle) || contains_text(handoff_log, needle);
                };

            return has("[ERROR]")
                || has("Source update cancel complete")
                || has("Source update canceled by operator")
                || has("Built runtime ready at")
                || has("Waiting for runtime handoff")
                || has("Source runtime files copied successfully")
                || has("Restarted updated runtime");
        }

        [[nodiscard]] inline bool source_update_pending_log_evidence(
            const std::filesystem::path& target_binary)
        {
            const std::string source_log = read_text_file_tail(source_update_log_path_for(target_binary));
            const std::string handoff_log = read_text_file_tail(update_handoff_log_path_for(target_binary));
            if (source_log.empty() && handoff_log.empty())
                return false;

            if (source_update_terminal_evidence(source_log, handoff_log))
                return false;

            const auto has = [&](const std::string_view needle) noexcept
                {
                    return contains_text(source_log, needle) || contains_text(handoff_log, needle);
                };

            return has("Source update worker started")
                || has("Source rebuild cancellation requested")
                || has("Source update cancel requested")
                || has("Source snapshot download")
                || has("Downloading latest")
                || has("Downloading managed vcpkg")
                || has("Restoring source dependencies")
                || has("MSBuild attempt")
                || has("Waiting for target runtime unlock");
        }

        [[nodiscard]] inline bool source_update_recent_pending_log_evidence(
            const std::filesystem::path& target_binary,
            const std::chrono::seconds max_age = std::chrono::minutes{ 3 })
        {
            if (!source_update_pending_log_evidence(target_binary))
                return false;

            return file_recently_modified(source_update_log_path_for(target_binary), max_age)
                || file_recently_modified(update_handoff_log_path_for(target_binary), max_age);
        }

        [[nodiscard]] inline std::optional<std::filesystem::file_time_type> file_write_time_or_none(
            const std::filesystem::path& path) noexcept
        {
            std::error_code ec;
            const auto write_time = std::filesystem::last_write_time(path, ec);
            if (ec)
                return std::nullopt;

            return write_time;
        }

        [[nodiscard]] inline int source_update_recent_cancel_seconds_remaining(
            const std::filesystem::path& target_binary,
            const std::chrono::seconds cooldown)
        {
            if (cooldown.count() <= 0)
                return 0;

            const auto source_log_path = source_update_log_path_for(target_binary);
            const auto handoff_log_path = update_handoff_log_path_for(target_binary);
            const std::string source_log = read_text_file_tail(source_log_path);
            const std::string handoff_log = read_text_file_tail(handoff_log_path);

            const auto latest_line_has_cancel_terminal = [](const std::string_view text) noexcept
                {
                    std::size_t end = text.find_last_not_of(" \t\r\n");
                    while (end != std::string_view::npos)
                    {
                        const std::size_t start = text.find_last_of("\r\n", end);
                        const std::string_view line =
                            start == std::string_view::npos
                                ? text.substr(0, end + 1u)
                                : text.substr(start + 1u, end - start);
                        if (!line.empty())
                        {
                            return contains_text(line, "Source update cancel complete")
                                || contains_text(line, "Source update canceled by operator");
                        }

                        if (start == std::string_view::npos || start == 0u)
                            break;
                        end = text.find_last_not_of(" \t\r\n", start - 1u);
                    }

                    return false;
                };

            const bool source_cancel_terminal = latest_line_has_cancel_terminal(source_log);
            const bool handoff_cancel_terminal = latest_line_has_cancel_terminal(handoff_log);
            if (!source_cancel_terminal && !handoff_cancel_terminal)
                return 0;

            std::optional<std::filesystem::file_time_type> newest_cancel_write{};
            const auto note_write_time = [&](const std::filesystem::path& path)
                {
                    if (const auto write_time = file_write_time_or_none(path))
                    {
                        if (!newest_cancel_write || *write_time > *newest_cancel_write)
                            newest_cancel_write = write_time;
                    }
                };

            if (source_cancel_terminal)
                note_write_time(source_log_path);
            if (handoff_cancel_terminal)
                note_write_time(handoff_log_path);
            if (!newest_cancel_write)
                return 0;

            const auto now = std::filesystem::file_time_type::clock::now();
            const auto elapsed = *newest_cancel_write >= now
                ? std::chrono::seconds{ 0 }
                : std::chrono::duration_cast<std::chrono::milliseconds>(now - *newest_cancel_write);
            const auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::seconds>(elapsed);
            const int remaining = static_cast<int>(cooldown.count() - elapsed_seconds.count());

            return (std::max)(0, remaining);
        }

        [[nodiscard]] inline bool source_update_active_marker_live(
            const std::filesystem::path& target_binary,
            std::filesystem::path* active_run_out = nullptr)
        {
            const auto marker = read_source_active_marker(target_binary);
#if !defined(_WIN32)
            const std::string source_log = read_text_file_tail(source_update_log_path_for(target_binary));
            const std::string handoff_log = read_text_file_tail(update_handoff_log_path_for(target_binary));
            const bool completed_handoff = source_update_terminal_evidence(source_log, handoff_log);
            if (completed_handoff
                && !in_process_source_update_active().load(std::memory_order_acquire))
            {
                close_inherited_linux_source_update_lock();
                remove_update_cache_path_best_effort(source_active_run_path(target_binary));
                remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            const bool lock_held = linux_source_update_lock_held();
            if (lock_held)
            {
                const bool marker_owner_live = marker
                    && marker->has_process_id
                    && process_is_running(marker->process_id);
                const bool legacy_marker_live = marker
                    && !marker->has_process_id
                    && source_update_recent_pending_log_evidence(
                        target_binary,
                        std::chrono::seconds{ 20 });
                if (marker_owner_live || legacy_marker_live)
                {
                    if (active_run_out)
                        *active_run_out = marker->run_dir;
                    return true;
                }

                close_inherited_linux_source_update_lock();
            }

            remove_update_cache_path_best_effort(source_active_run_path(target_binary));
            if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                remove_update_cache_path_best_effort(source_cancel_path(target_binary));
            return false;
#else
            if (!marker)
                return false;

            std::error_code ec;
            const bool run_exists = std::filesystem::exists(marker->run_dir, ec) && !ec;

            if (marker->has_process_id)
            {
                if (process_is_running(marker->process_id))
                {
                    if (active_run_out)
                        *active_run_out = marker->run_dir;
                    return true;
                }

                remove_update_cache_path_best_effort(source_active_run_path(target_binary));
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            const auto active_marker_path = source_active_run_path(target_binary);
            if (!run_exists)
            {
                remove_update_cache_path_best_effort(active_marker_path);
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            const bool old_marker_recent =
                file_recently_modified(active_marker_path, std::chrono::seconds{ 15 })
                || source_update_recent_pending_log_evidence(target_binary, std::chrono::minutes{ 3 });
            if (!old_marker_recent)
            {
                remove_update_cache_path_best_effort(active_marker_path);
                if (!source_update_recent_pending_log_evidence(target_binary, std::chrono::seconds{ 20 }))
                    remove_update_cache_path_best_effort(source_cancel_path(target_binary));
                return false;
            }

            if (active_run_out)
                *active_run_out = marker->run_dir;
            return true;
#endif
        }

        [[nodiscard]] bool source_update_session_active(
            const std::filesystem::path& target_binary)
        {
            return source_update_active_marker_live(target_binary);
        }

        [[nodiscard]] inline bool source_update_cancellation_pending(
            const std::filesystem::path& target_binary)
        {
            const bool cancel_marker_present = source_cancel_requested(target_binary);
            const std::string source_log = read_text_file_tail(source_update_log_path_for(target_binary));
            const std::string handoff_log = read_text_file_tail(update_handoff_log_path_for(target_binary));
            if (source_update_terminal_evidence(source_log, handoff_log))
                return false;

            if (cancel_marker_present)
            {
                return source_update_active_marker_live(target_binary);
            }

            return (contains_text(source_log, "Source rebuild cancellation requested")
                    || contains_text(handoff_log, "Source rebuild cancellation requested"))
                && source_update_active_marker_live(target_binary);
        }

        [[nodiscard]] inline std::filesystem::path make_temp_download_path(const std::string_view stem)
        {
            static std::atomic<unsigned long long> s_counter{ 0 };

            auto temp_root = ensure_directory(updater_cache_root() / "tmp");

            std::string safe_stem;
            safe_stem.reserve(stem.size());
            for (const char ch : stem)
            {
                const auto uch = static_cast<unsigned char>(ch);
                safe_stem.push_back(std::isalnum(uch) ? ch : '_');
            }

            if (safe_stem.empty())
                safe_stem = "download";

            const auto serial = s_counter.fetch_add(1, std::memory_order_relaxed);
            const auto tick = static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());

            return temp_root / ("epoch_" + safe_stem + "_" + std::to_string(tick) + "_" + std::to_string(serial) + ".tmp");
        }

        [[nodiscard]] inline std::filesystem::path make_temp_script_path(const std::string_view stem)
        {
#if defined(_WIN32)
            return make_temp_download_path(stem).replace_extension(".bat");
#else
            return make_temp_download_path(stem).replace_extension(".sh");
#endif
        }

        [[nodiscard]] std::filesystem::path staged_update_handoff_script_path()
        {
#if defined(_WIN32)
            return updater_cache_root() / "epoch_staged_update_handoff.bat";
#else
            return updater_cache_root() / "epoch_staged_update_handoff.sh";
#endif
        }

#if defined(_WIN32)
        [[nodiscard]] std::filesystem::path make_temp_powershell_script_path(const std::string_view stem)
        {
            return make_temp_download_path(stem).replace_extension(".ps1");
        }

        [[nodiscard]] std::string powershell_escape_single_quoted(const std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 8);
            for (const char ch : text)
            {
                if (ch == '\'')
                    out += "''";
                else
                    out.push_back(ch);
            }
            return out;
        }

        [[nodiscard]] bool launch_powershell_script(
            const std::filesystem::path& script_path,
            const bool hidden,
            const std::filesystem::path& log_path = {})
        {
            auto powershell = env_path("SystemRoot") / "System32/WindowsPowerShell/v1.0/powershell.exe";
            if (powershell.empty() || !std::filesystem::exists(powershell))
                powershell = std::filesystem::path{ "powershell.exe" };

            return run_process_with_visibility(
                powershell,
                {
                    "-NoLogo",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    script_path.string()
                },
                script_path.parent_path(),
                log_path,
                false,
                hidden,
                nullptr);
        }
#endif

        [[nodiscard]] std::filesystem::path source_solution_path(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_SOLUTION_NAME();
        }

        [[nodiscard]] std::filesystem::path source_runtime_output_dir(const std::filesystem::path& source_root)
        {
#if defined(_WIN32)
            return build_layout::windows_bin(source_root, SOURCE_BUILD_CONFIGURATION(), SOURCE_BUILD_PLATFORM());
#else
            return build_layout::linux_cmake(source_root, "Clang-Release");
#endif
        }

        [[nodiscard]] inline std::filesystem::path resolve_runtime_binary_path(
            const std::filesystem::path& runtime_dir,
            const std::filesystem::path& target_binary)
        {
            if (runtime_dir.empty() || !runtime_dir.is_absolute()) return {};
#if defined(_WIN32)
            const std::array<std::filesystem::path, 3> names{
                target_binary.filename(),
                "EpochEditor.exe",
                "epoch.exe",
            };
#else
            const std::array<std::filesystem::path, 2> names{
                target_binary.filename(),
                "epoch",
            };
#endif

            std::error_code ec;
            for (const auto& name : names)
            {
                const auto candidate = runtime_dir / name;
                if (!candidate.empty() && std::filesystem::exists(candidate, ec))
                    return candidate;
                ec.clear();
            }

            if (std::filesystem::exists(runtime_dir, ec) && std::filesystem::is_directory(runtime_dir, ec))
            {
                for (std::filesystem::recursive_directory_iterator it{ runtime_dir, ec }, end; it != end; it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    if (!it->is_regular_file(ec))
                    {
                        ec.clear();
                        continue;
                    }

                    const auto filename = it->path().filename();
                    if (std::find(names.begin(), names.end(), filename) != names.end())
                        return it->path();
                }
            }

            return runtime_dir / target_binary.filename();
        }

        [[nodiscard]] std::filesystem::path source_runtime_binary_path(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
            return resolve_runtime_binary_path(source_runtime_output_dir(source_root), target_binary);
        }

        [[nodiscard]] std::filesystem::path source_manifest_root(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_MANIFEST_ROOT_NAME();
        }

        [[nodiscard]] inline std::filesystem::path source_version_file_path(const std::filesystem::path& source_root)
        {
            return source_root / SOURCE_VERSION_FILE_NAME();
        }

        [[nodiscard]] inline std::string read_local_source_version(const std::filesystem::path& source_root)
        {
            const auto version_file = source_version_file_path(source_root);
            if (!version_file.empty())
            {
                const auto text = read_text_file(version_file);
                const auto version = extract_version_string(text);
                if (!version.empty())
                    return version;
            }

            return extract_version_string(PROJECT_SOURCE_VERSION);
        }

        [[nodiscard]] std::filesystem::path find_msbuild_path()
        {
#if defined(_WIN32)
            const auto from_env = env_path("MSBUILD_EXE_PATH");
            if (!from_env.empty() && std::filesystem::exists(from_env))
                return from_env;

            const auto program_files_x86 = env_path("ProgramFiles(x86)");
            const auto vswhere =
                program_files_x86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";

            if (!vswhere.empty() && std::filesystem::exists(vswhere))
            {
                int query_exit = -1;
                const auto text = capture_process_output(
                    vswhere,
                    {
                        "-latest",
                        "-requires",
                        "Microsoft.Component.MSBuild",
                        "-find",
                        "MSBuild\\**\\Bin\\MSBuild.exe"
                    },
                    vswhere.parent_path(),
                    &query_exit);

                if (query_exit == 0)
                {
                    if (!text.empty() && std::filesystem::exists(text))
                        return std::filesystem::path{ text };
                }
            }

            const auto program_files = env_path("ProgramFiles");

            const std::array<std::filesystem::path, 20> candidates{
                program_files / "Microsoft Visual Studio/2026/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2026/Preview/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files / "Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",

                program_files_x86 / "Microsoft Visual Studio/2026/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2026/Preview/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Enterprise/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/MSBuild.exe",
                program_files_x86 / "Microsoft Visual Studio/2022/Preview/MSBuild/Current/Bin/MSBuild.exe",
            };

            for (const auto& candidate : candidates)
            {
                if (!candidate.empty() && std::filesystem::exists(candidate))
                    return candidate;
            }
#endif
            return {};
        }

        [[nodiscard]] inline bool build_runtime_from_source(
            const std::filesystem::path& source_root,
            const std::filesystem::path& target_binary)
        {
            if (!source_tree_matches_edition(source_root))
            {
                log_error("Source rebuild refused a missing or different edition marker.");
                return false;
            }
#if defined(_WIN32)
            const auto manifest_root = source_manifest_root(source_root);
            const auto build_log = source_update_log_path_for(target_binary);

            const auto msbuild = find_msbuild_path();
            if (msbuild.empty())
            {
                log_error("Could not locate MSBuild. Install Visual Studio Build Tools or set MSBUILD_EXE_PATH.");
                return false;
            }

            const auto solution = source_solution_path(source_root);
            if (!std::filesystem::exists(solution))
            {
                log_error("Extracted source snapshot does not contain the configured solution file.");
                return false;
            }

            const auto vcpkg_exe = find_or_prepare_vcpkg(manifest_root, build_log);
            if (vcpkg_exe.empty())
            {
                log_error("Could not prepare vcpkg for the source update.");
                return false;
            }

            const auto vcpkg_root = vcpkg_exe.parent_path();
            append_log_line(build_log, "[INFO] Source update build started");
            append_log_line(build_log, "[INFO] Source root: " + source_root.string());
            append_log_line(build_log, "[INFO] Manifest root: " + manifest_root.string());
            append_log_line(build_log, "[INFO] vcpkg root: " + vcpkg_root.string());
            append_log_line(build_log, "[INFO] MSBuild: " + msbuild.string());
            append_log_line(
                build_log,
                "[INFO] Source updater build lane: "
                + SOURCE_BUILD_CONFIGURATION()
                + "|"
                + SOURCE_BUILD_PLATFORM()
                + " (runtime may currently be Debug).");

            const bool using_managed_vcpkg =
                path_is_inside(managed_tools_root(), vcpkg_root);
            if (using_managed_vcpkg
                && !prepare_manifest_for_managed_vcpkg_registry(
                    manifest_root,
                    vcpkg_root,
                    build_log))
            {
                log_error("Could not prepare the source snapshot for managed vcpkg.");
                return false;
            }
            if (!using_managed_vcpkg)
            {
                append_log_line(
                    build_log,
                    "[INFO] Using installed vcpkg registry; source manifest baseline left intact.");
            }

            const auto overlay_ports = prepare_vcpkg_overlay_ports(vcpkg_root, build_log);

            log_info("Building updated runtime from source.");
            log_info("Restoring source dependencies with vcpkg.");
            append_log_line(build_log, "[INFO] Restoring source dependencies with vcpkg.");

            std::vector<std::string> vcpkg_args{
                "install",
                "--triplet",
                SOURCE_BUILD_PLATFORM() + "-windows",
                "--x-manifest-root=" + manifest_root.string(),
                "--x-builtin-ports-root=" + (vcpkg_root / "ports").string(),
                "--x-builtin-registry-versions-dir=" + (vcpkg_root / "versions").string()
            };
            if (!overlay_ports.empty())
                vcpkg_args.push_back("--overlay-ports=" + overlay_ports.string());

            int vcpkg_exit = -1;
            if (!run_process_hidden(
                vcpkg_exe,
                vcpkg_args,
                manifest_root,
                build_log,
                true,
                &vcpkg_exit) || vcpkg_exit != 0)
            {
                append_log_line(
                    build_log,
                    "[ERROR] vcpkg dependency restore failed with exit code " + std::to_string(vcpkg_exit));
                log_error("vcpkg dependency restore failed. See logs/epoch_source_update.log for details.");
                return false;
            }

            log_info("MSBuild: " + msbuild.string());
            append_log_line(build_log, "[INFO] Building updated runtime with MSBuild.");

            const auto vcpkg_install_root = manifest_root / "vcpkg_installed";

            const std::vector<std::string> msbuild_args{
                solution.string(),
                "/t:" + SOURCE_BUILD_TARGET(),
                "/p:Configuration=" + SOURCE_BUILD_CONFIGURATION(),
                "/p:Platform=" + SOURCE_BUILD_PLATFORM(),
                "/p:VcpkgRoot=" + vcpkg_root.string(),
                "/p:VcpkgManifestRoot=" + manifest_root.string(),
                "/p:VcpkgInstalledDir=" + vcpkg_install_root.string(),
                "/p:VcpkgManifestInstall=false",
                "/p:VcpkgTriplet=" + SOURCE_BUILD_PLATFORM() + "-windows",
                "/p:UseMultiToolTask=false",
                "/p:BuildInParallel=false",
                "/m:1",
                "/nr:false",
                "/clp:ErrorsOnly"
            };

            bool build_ok = false;
            for (int attempt = 1; attempt <= 2; ++attempt)
            {
                append_log_line(
                    build_log,
                    "[INFO] MSBuild attempt " + std::to_string(attempt) + " started.");

                int msbuild_exit = -1;
                const bool launched = run_process_hidden(
                    msbuild,
                    msbuild_args,
                    source_root,
                    build_log,
                    true,
                    &msbuild_exit);

                if (launched && msbuild_exit == 0)
                {
                    build_ok = true;
                    append_log_line(
                        build_log,
                        "[INFO] MSBuild attempt " + std::to_string(attempt) + " completed successfully.");
                    break;
                }

                append_log_line(
                    build_log,
                    "[WARN] MSBuild attempt " + std::to_string(attempt)
                    + " failed with exit code " + std::to_string(msbuild_exit) + ".");

                if (attempt == 1)
                {
                    append_log_line(
                        build_log,
                        "[INFO] Retrying the source build once after dependency restore.");
                }
            }

            if (!build_ok)
            {
                append_log_line(build_log, "[ERROR] Source build failed after two attempts.");
                log_error("Source build failed. See logs/epoch_source_update.log for details.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                append_log_line(build_log, "[ERROR] Source build completed without a runtime binary.");
                log_error("Source build completed without producing the runtime binary.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source build completed successfully.");
            return true;
#else
            const auto manifest_root = source_manifest_root(source_root);
            const auto build_log = source_update_log_path_for(target_binary);
            const auto build_script = manifest_root / "build.sh";

            if (!std::filesystem::exists(build_script))
            {
                log_error("Extracted source snapshot does not contain the Linux build script.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source update build started");
            append_log_line(build_log, "[INFO] Source root: " + source_root.string());
            append_log_line(build_log, "[INFO] Manifest root: " + manifest_root.string());
            append_log_line(build_log, "[INFO] Build script: " + build_script.string());

            if (stop_source_update_if_canceled(target_binary, build_log, "Linux dependency preparation"))
                return false;

            const auto vcpkg_exe = find_or_prepare_vcpkg(manifest_root, build_log);
            if (vcpkg_exe.empty())
            {
                append_log_line(build_log, "[ERROR] Could not locate or prepare vcpkg for the Linux source update.");
                log_error("Could not prepare vcpkg for the Linux source update.");
                return false;
            }

            const auto vcpkg_root = vcpkg_exe.parent_path();
            const bool using_managed_vcpkg = path_is_inside(managed_tools_root(), vcpkg_root);
            append_log_line(build_log, "[INFO] Linux source update vcpkg root: " + vcpkg_root.string());

            if (stop_source_update_if_canceled(target_binary, build_log, "managed vcpkg selection"))
                return false;

            if (using_managed_vcpkg
                && !prepare_manifest_for_managed_vcpkg_registry(
                    manifest_root,
                    vcpkg_root,
                    build_log))
            {
                append_log_line(build_log, "[ERROR] Could not prepare the Linux source snapshot for managed vcpkg.");
                log_error("Could not prepare the Linux source snapshot for managed vcpkg.");
                return false;
            }

            if (stop_source_update_if_canceled(target_binary, build_log, "managed vcpkg registry preparation"))
                return false;

            if (!using_managed_vcpkg)
            {
                append_log_line(
                    build_log,
                    "[INFO] Using installed vcpkg registry for the Linux source update.");
            }

            const auto overlay_ports = prepare_vcpkg_overlay_ports(vcpkg_root, build_log);
            if (stop_source_update_if_canceled(target_binary, build_log, "vcpkg overlay preparation"))
                return false;
            const auto run_linux_build =
                [&](const std::string& compiler_choice, const std::filesystem::path& expected_dir) -> bool
                {
                    append_log_line(
                        build_log,
                        "[INFO] Linux source build attempt started with compiler '" + compiler_choice + "'.");

                    std::vector<std::string> build_args{
                        build_script.string(),
                        "--bootstrap-current-toolchain",
                        "--tool-cache-root",
                        managed_tools_root().string(),
                        "--cancel-file",
                        source_cancel_path(target_binary).string(),
                        "--vcpkg-root",
                        vcpkg_root.string()
                    };
                    if (!overlay_ports.empty())
                    {
                        build_args.push_back("--vcpkg-overlay-ports");
                        build_args.push_back(overlay_ports.string());
                    }
                    build_args.push_back(compiler_choice);
                    build_args.push_back(SOURCE_BUILD_CONFIGURATION());

                    int build_exit = -1;
                    const bool launched = run_process_hidden(
                        std::filesystem::path{ "/bin/bash" },
                        build_args,
                        manifest_root,
                        build_log,
                        true,
                        &build_exit);

                    if (!launched)
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Failed to launch Linux source build with compiler '" + compiler_choice + "'.");
                        return false;
                    }

                    if (build_exit != 0)
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Linux source build with compiler '" + compiler_choice
                            + "' exited with code " + std::to_string(build_exit) + ".");
                        return false;
                    }

                    const auto built_binary = resolve_runtime_binary_path(expected_dir, target_binary);
                    if (!std::filesystem::exists(built_binary))
                    {
                        append_log_line(
                            build_log,
                            "[WARN] Linux source build with compiler '" + compiler_choice
                            + "' completed without producing the runtime binary.");
                        return false;
                    }

                    append_log_line(
                        build_log,
                        "[INFO] Linux source build completed successfully with compiler '" + compiler_choice + "'.");
                    return true;
                };

            const bool build_ok = run_linux_build(
                "clang",
                build_layout::linux_cmake(source_root, "Clang-Release"));

            if (!build_ok)
            {
                append_log_line(build_log, "[WARN] Linux source updater does not fall back to GCC after a Clang full-engine build failure.");
            }

            if (!build_ok)
            {
                append_log_line(build_log, "[ERROR] Linux source build failed.");
                log_error("Linux source build failed. See logs/epoch_source_update.log for details.");
                return false;
            }

            const auto built_binary = source_runtime_binary_path(source_root, target_binary);
            if (!std::filesystem::exists(built_binary))
            {
                append_log_line(build_log, "[ERROR] Linux source build completed without a runtime binary.");
                log_error("Linux source build completed without producing the runtime binary.");
                return false;
            }

            append_log_line(build_log, "[INFO] Source build completed successfully.");
            return true;
#endif
        }

        [[nodiscard]] inline std::filesystem::path first_subdirectory(const std::filesystem::path& root)
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(root, ec))
            {
                if (entry.is_directory())
                    return entry.path();
            }

            return {};
        }

        [[nodiscard]] inline bool env_flag_enabled(const char* name)
        {
            const auto value = lower_ascii(trim_ascii(env_path(name).string()));
            return value == "1"
                || value == "true"
                || value == "yes"
                || value == "on";
        }
    }


