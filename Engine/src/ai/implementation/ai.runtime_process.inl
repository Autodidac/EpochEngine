        struct ProcessCapture
        {
            std::string output{};
            int exit_code{-1};
            bool launched{};
            bool timed_out{};
            bool cancelled{};
        };

        constexpr std::size_t kMaximumInferenceOutputBytes = 16u * 1024u * 1024u;

        static void append_process_output(std::string& output, const char* data, std::size_t size)
        {
            if (output.size() >= kMaximumInferenceOutputBytes)
                return;
            output.append(data, (std::min)(size, kMaximumInferenceOutputBytes - output.size()));
        }

#if defined(_WIN32)
        [[nodiscard]] static std::wstring utf8_to_wide(std::string_view value)
        {
            if (value.empty())
                return {};
            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (required <= 0)
                return {};
            std::wstring converted(static_cast<std::size_t>(required), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    converted.data(), required) != required)
                return {};
            return converted;
        }

        [[nodiscard]] static std::wstring quote_windows_argument(std::wstring_view argument)
        {
            if (argument.find_first_of(L" \t\"") == std::wstring_view::npos)
                return std::wstring{argument};

            std::wstring quoted{L"\""};
            std::size_t slashes = 0u;
            for (const wchar_t character : argument)
            {
                if (character == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == L'\"')
                {
                    quoted.append(slashes * 2u + 1u, L'\\');
                    quoted.push_back(L'\"');
                    slashes = 0u;
                    continue;
                }
                quoted.append(slashes, L'\\');
                slashes = 0u;
                quoted.push_back(character);
            }
            quoted.append(slashes * 2u, L'\\');
            quoted.push_back(L'\"');
            return quoted;
        }

        [[nodiscard]] static ProcessCapture capture_process(
            const std::filesystem::path& executable,
            const std::vector<std::string>& arguments,
            std::chrono::seconds timeout,
            std::stop_token cancellation)
        {
            ProcessCapture capture{};
            if (cancellation.stop_requested())
            {
                capture.cancelled = true;
                return capture;
            }
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &security, 0))
                return capture;
            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            std::wstring command = quote_windows_argument(executable.wstring());
            for (const auto& argument : arguments)
            {
                command.push_back(L' ');
                command += quote_windows_argument(utf8_to_wide(argument));
            }
            command.push_back(L'\0');

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_HIDE;
            startup.hStdOutput = writePipe;
            startup.hStdError = writePipe;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION process{};
            if (cancellation.stop_requested())
            {
                CloseHandle(writePipe);
                CloseHandle(readPipe);
                capture.cancelled = true;
                return capture;
            }
            capture.launched = CreateProcessW(
                executable.wstring().c_str(), command.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                executable.parent_path().wstring().c_str(),
                &startup, &process) != FALSE;
            CloseHandle(writePipe);
            if (!capture.launched)
            {
                CloseHandle(readPipe);
                return capture;
            }

            const auto deadline = std::chrono::steady_clock::now() + timeout;
            bool running = true;
            char buffer[8192];
            while (running)
            {
                DWORD available = 0u;
                while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0u)
                {
                    if (cancellation.stop_requested() || std::chrono::steady_clock::now() >= deadline)
                        break;
                    DWORD read = 0u;
                    const DWORD requested = (std::min)(available, static_cast<DWORD>(sizeof(buffer)));
                    if (!ReadFile(readPipe, buffer, requested, &read, nullptr) || read == 0u)
                        break;
                    append_process_output(capture.output, buffer, read);
                }
                running = WaitForSingleObject(process.hProcess, 20u) == WAIT_TIMEOUT;
                if (running && cancellation.stop_requested())
                {
                    capture.cancelled = true;
                    TerminateProcess(process.hProcess, 125u);
                    WaitForSingleObject(process.hProcess, 5000u);
                    running = false;
                }
                else if (running && std::chrono::steady_clock::now() >= deadline)
                {
                    capture.timed_out = true;
                    TerminateProcess(process.hProcess, 124u);
                    WaitForSingleObject(process.hProcess, 5000u);
                    running = false;
                }
            }

            // A descendant may still hold the write end. Never block on EOF
            // after the owned process exits/cancels; drain only ready bytes.
            const auto drainDeadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds{100};
            DWORD available = 0u;
            while (!cancellation.stop_requested()
                && std::chrono::steady_clock::now() < drainDeadline
                && PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr)
                && available > 0u)
            {
                DWORD read = 0u;
                if (!ReadFile(readPipe, buffer,
                        (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &read, nullptr)
                    || read == 0u)
                    break;
                append_process_output(capture.output, buffer, read);
            }
            capture.cancelled = capture.cancelled || cancellation.stop_requested();
            DWORD exitCode = 1u;
            GetExitCodeProcess(process.hProcess, &exitCode);
            capture.exit_code = static_cast<int>(exitCode);
            CloseHandle(readPipe);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return capture;
        }
#else
        [[nodiscard]] static ProcessCapture capture_process(
            const std::filesystem::path& executable,
            const std::vector<std::string>& arguments,
            std::chrono::seconds timeout,
            std::stop_token cancellation)
        {
            ProcessCapture capture{};
            if (cancellation.stop_requested())
            {
                capture.cancelled = true;
                return capture;
            }
            int outputPipe[2]{};
            if (pipe(outputPipe) != 0)
                return capture;

            if (cancellation.stop_requested())
            {
                close(outputPipe[0]);
                close(outputPipe[1]);
                capture.cancelled = true;
                return capture;
            }

            const pid_t child = fork();
            if (child < 0)
            {
                close(outputPipe[0]);
                close(outputPipe[1]);
                return capture;
            }
            if (child == 0)
            {
                dup2(outputPipe[1], STDOUT_FILENO);
                dup2(outputPipe[1], STDERR_FILENO);
                close(outputPipe[0]);
                close(outputPipe[1]);
                (void)setpriority(PRIO_PROCESS, 0, 5);

                std::vector<std::string> owned;
                owned.reserve(arguments.size() + 1u);
                owned.push_back(executable.string());
                owned.insert(owned.end(), arguments.begin(), arguments.end());
                std::vector<char*> argv;
                argv.reserve(owned.size() + 1u);
                for (auto& argument : owned)
                    argv.push_back(argument.data());
                argv.push_back(nullptr);
                execvp(argv.front(), argv.data());
                _exit(127);
            }

            capture.launched = true;
            close(outputPipe[1]);
            const int flags = fcntl(outputPipe[0], F_GETFL, 0);
            if (flags >= 0)
                fcntl(outputPipe[0], F_SETFL, flags | O_NONBLOCK);

            const auto deadline = std::chrono::steady_clock::now() + timeout;
            int status = 0;
            bool running = true;
            char buffer[8192];
            while (running)
            {
                for (;;)
                {
                    if (cancellation.stop_requested() || std::chrono::steady_clock::now() >= deadline)
                        break;
                    const ssize_t read = ::read(outputPipe[0], buffer, sizeof(buffer));
                    if (read <= 0)
                        break;
                    append_process_output(capture.output, buffer, static_cast<std::size_t>(read));
                }

                const pid_t waited = waitpid(child, &status, WNOHANG);
                running = waited == 0;
                if (running && cancellation.stop_requested())
                {
                    capture.cancelled = true;
                    kill(child, SIGTERM);
                    std::this_thread::sleep_for(std::chrono::milliseconds{250});
                    if (waitpid(child, &status, WNOHANG) == 0)
                        kill(child, SIGKILL);
                    waitpid(child, &status, 0);
                    running = false;
                }
                else if (running && std::chrono::steady_clock::now() >= deadline)
                {
                    capture.timed_out = true;
                    kill(child, SIGTERM);
                    std::this_thread::sleep_for(std::chrono::milliseconds{250});
                    if (waitpid(child, &status, WNOHANG) == 0)
                        kill(child, SIGKILL);
                    waitpid(child, &status, 0);
                    running = false;
                }
                if (running)
                    std::this_thread::sleep_for(std::chrono::milliseconds{20});
            }

            const auto drainDeadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds{100};
            while (!cancellation.stop_requested()
                && std::chrono::steady_clock::now() < drainDeadline)
            {
                const ssize_t read = ::read(outputPipe[0], buffer, sizeof(buffer));
                if (read <= 0)
                    break;
                append_process_output(capture.output, buffer, static_cast<std::size_t>(read));
            }
            close(outputPipe[0]);
            capture.cancelled = capture.cancelled || cancellation.stop_requested();
            capture.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
            return capture;
        }
#endif

        [[nodiscard]] static unsigned int
            responsive_direct_thread_count(unsigned int logicalThreads) noexcept
        {
            if (logicalThreads == 0u)
                logicalThreads = 1u;
            return (std::max)(
                1u, (logicalThreads + 1u) / 2u);
        }

        [[nodiscard]] static std::vector<std::string>
            direct_llama_cpp_file_arguments(
                const EngineAiModel::Config& config,
                const std::filesystem::path& outputPath,
                const std::filesystem::path& systemPromptPath,
                const std::filesystem::path& userPromptPath)
        {
            std::vector<std::string> arguments{
                "--model", config.model,
                "--offline",
                "--log-disable",
                "--no-display-prompt",
                "--no-show-timings",
                "--single-turn",
                "--simple-io",
                "--color", "off",
                "--reasoning", "off",
                "--reasoning-budget", "0",
                "--output-file", outputPath.generic_string(),
                "--system-prompt-file", systemPromptPath.generic_string(),
                "--file", userPromptPath.generic_string(),
                "--ctx-size", std::to_string(config.context_tokens),
                "--n-predict", std::to_string(config.output_tokens),
                "--gpu-layers", std::to_string(config.gpu_layers)
            };
            if (config.threads > 0u)
            {
                arguments.push_back("--threads");
                arguments.push_back(std::to_string(config.threads));
                arguments.push_back("--threads-batch");
                arguments.push_back(std::to_string(config.threads));
            }
            return arguments;
        }

        [[nodiscard]] static bool
            direct_llama_cpp_prompt_file_arguments_contract()
        {
            EngineAiModel::Config config{};
            config.model = "model.gguf";
            config.context_tokens = 65'536u;
            config.output_tokens = 32'768u;
            config.threads = 12u;
            const std::filesystem::path outputPath{"response.txt"};
            const std::filesystem::path systemPath{"system.txt"};
            const std::filesystem::path userPath{"user.txt"};
            const auto arguments = direct_llama_cpp_file_arguments(
                config, outputPath, systemPath, userPath);
            const auto hasPair = [&](std::string_view flag,
                                     std::string_view value)
            {
                for (std::size_t index = 0u; index + 1u < arguments.size(); ++index)
                {
                    if (arguments[index] == flag && arguments[index + 1u] == value)
                        return true;
                }
                return false;
            };
            return hasPair("--output-file", "response.txt")
                && hasPair("--system-prompt-file", "system.txt")
                && hasPair("--file", "user.txt")
                && hasPair("--threads", "12")
                && hasPair("--threads-batch", "12")
                && responsive_direct_thread_count(24u) == 12u
                && responsive_direct_thread_count(0u) == 1u
                && std::ranges::find(arguments, "--prompt") == arguments.end()
                && std::ranges::find(arguments, "--system-prompt")
                    == arguments.end();
        }

        [[nodiscard]] static std::string llama_cpp_complete(
            const EngineAiModel::Config& config,
            std::string_view systemPrompt,
            std::string_view userText,
            bool allowStrictSourcePacket,
            std::stop_token cancellation,
            ModelTerminalFailure& terminalFailure,
            const ModelRequestObserver& observer)
        {
            terminalFailure = ModelTerminalFailure::none;
            if (cancellation.stop_requested())
            {
                terminalFailure = ModelTerminalFailure::cancelled;
                return std::string{kModelRequestCancelled};
            }
            const std::filesystem::path outputRoot =
                std::filesystem::path{executable_cache_bucket("ai")}
                    / "transient";
            std::error_code outputError;
            std::filesystem::create_directories(outputRoot, outputError);
            if (outputError)
                return "Direct llama.cpp inference could not prepare its transient response path.";

            const std::uint64_t sequence =
                g_directInferenceOutputSequence.fetch_add(
                    1u, std::memory_order_relaxed);
            const std::uint64_t clock =
                static_cast<std::uint64_t>(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count());
            const std::string transientStem = "llama-cli-"
                + std::to_string(clock) + "-" + std::to_string(sequence);
            const std::filesystem::path outputPath =
                outputRoot / (transientStem + "-response.txt");
            const std::filesystem::path systemPromptPath =
                outputRoot / (transientStem + "-system.txt");
            const std::filesystem::path userPromptPath =
                outputRoot / (transientStem + "-user.txt");
            for (const auto& path :
                std::array{outputPath, systemPromptPath, userPromptPath})
            {
                std::filesystem::remove(path, outputError);
                outputError.clear();
            }

            struct ScopedTransientFiles final
            {
                ~ScopedTransientFiles()
                {
                    std::error_code ignored;
                    for (const auto& path : paths)
                    {
                        std::filesystem::remove(path, ignored);
                        ignored.clear();
                    }
                }
                std::array<std::filesystem::path, 3u> paths{};
            };
            const ScopedTransientFiles transientCleanup{{
                outputPath, systemPromptPath, userPromptPath}};

            const auto writePrompt = [](const std::filesystem::path& path,
                                        std::string_view text)
            {
                std::ofstream stream{
                    path, std::ios::binary | std::ios::trunc};
                if (!stream)
                    return false;
                stream.write(
                    text.data(), static_cast<std::streamsize>(text.size()));
                return stream.good();
            };
            if (!writePrompt(systemPromptPath, systemPrompt)
                || !writePrompt(userPromptPath, userText))
            {
                return "Direct llama.cpp inference could not prepare its transient prompt files.";
            }
            const std::vector<std::string> arguments =
                direct_llama_cpp_file_arguments(
                    config, outputPath, systemPromptPath, userPromptPath);

            notify_model_stage(observer, ModelRequestStage::sending);
            const auto attemptStarted = std::chrono::steady_clock::now();
            ProcessCapture capture = capture_process(
                std::filesystem::path{config.executable}, arguments,
                std::chrono::seconds{config.timeout_seconds}, cancellation);
            if (capture.cancelled || cancellation.stop_requested())
            {
                terminalFailure = ModelTerminalFailure::cancelled;
                return std::string{kModelRequestCancelled};
            }
            if (!capture.launched)
                return "Direct llama.cpp inference could not start. Check the configured llama-cli executable.";
            if (capture.timed_out)
            {
                terminalFailure = ModelTerminalFailure::total_timeout;
                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - attemptStarted).count();
                return "Direct llama.cpp inference exceeded its "
                    + std::to_string(config.timeout_seconds) + "s whole request budget after "
                    + std::to_string(elapsed)
                    + "s on attempt 1/1. Stop was requested; this generation was not automatically restarted.";
            }
            if (capture.exit_code != 0)
            {
                const std::string captured = trim(capture.output);
                const std::string detail = captured.substr(
                    0u, (std::min)(captured.size(), std::size_t{2048u}));
                return "Direct llama.cpp inference failed with exit code "
                    + std::to_string(capture.exit_code)
                    + (detail.empty() ? std::string{"."} : std::string{": "} + detail);
            }
            notify_model_stage(observer, ModelRequestStage::receiving);
            if (cancellation.stop_requested())
            {
                terminalFailure = ModelTerminalFailure::cancelled;
                return std::string{kModelRequestCancelled};
            }
            const std::string transcript = read_small_text_file(
                outputPath, kMaximumInferenceOutputBytes);
            std::string reply = normalize_direct_llama_cpp_transcript(transcript, userText);
            if (reply.empty())
            {
                return transcript.empty()
                    ? "Direct llama.cpp inference returned no parseable assistant content: the response file was empty."
                    : "Direct llama.cpp inference returned no parseable assistant content: no framed assistant turn or strict source packet was found.";
            }
            return reply;
        }
