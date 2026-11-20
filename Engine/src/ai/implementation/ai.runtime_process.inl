        struct ProcessCapture
        {
            std::string output{};
            int exit_code{-1};
            bool launched{};
            bool timed_out{};
            bool cancelled{};
            bool pinned_package_rejected{};
        };

        constexpr std::size_t kMaximumInferenceOutputBytes = 16u * 1024u * 1024u;

#if defined(_WIN32)
        struct OfflineRuntimeFile final
        {
            std::string_view name;
            std::uint64_t bytes;
            std::string_view sha256;
        };

        // All files from the pinned flat Vulkan x64 ZIP. Keep this bound to
        // the same archive installed by install_epoch_qwen38.ps1.
        static_assert(kEpochLocalLlamaCppArtifactSha256.compare(
            "6c83027548f989a4d5c9502b31b580dc5e1254998bd49c1d78e85f316900d118") == 0);
        constexpr std::array<OfflineRuntimeFile, 53u> kPinnedOfflineRuntimeFiles{{
            {"LICENSE-LLVM-OpenMP", 19741u, "fdad1758a9e1f9d5a81e18879b3406772115edc92c24bfa36b70c654f325e8e4"},
            {"ggml-base.dll", 832000u, "587dcd7f81f80724ee14ff14953bafea1346968f3f096f951d95e4fa26e572df"},
            {"ggml-cpu-alderlake.dll", 1463808u, "f70a8cc1cfbd69ac3a95adaba975c257bdede7ad1238c5f92ec1cea41438b751"},
            {"ggml-cpu-cannonlake.dll", 1695232u, "af54f08387fbeef670d5de0edeef5ef1ddfc42a4a9b15bd58b81b4d1cb8f706e"},
            {"ggml-cpu-cascadelake.dll", 1668096u, "fb97fab3578130edf5533d4c389567b3aeb972270de315467c4677a83449a6b8"},
            {"ggml-cpu-cooperlake.dll", 1677824u, "1e2d2bdc6977f09b654377b07ae19e0791dd3c234357b466d5f68328a2eb32e8"},
            {"ggml-cpu-haswell.dll", 1468928u, "35677b3ac32a03b6b48217066fd1d620b2640b3719fd21e6a7c161108fd4d8ee"},
            {"ggml-cpu-icelake.dll", 1674752u, "9e147b47d1e68caf8a8273ab6d5b5f52af98c03a2f4ab1e3f793ae4ccc646c46"},
            {"ggml-cpu-ivybridge.dll", 1350144u, "15501e8e765231d407a05adf56cf8d4d3605659d22e62436db3434e4ff490aba"},
            {"ggml-cpu-piledriver.dll", 1343488u, "04ce34713f3f9202861c1e74c00100d1caab36d0a68b4c626a240af097cc4748"},
            {"ggml-cpu-sandybridge.dll", 1319424u, "1a429902e26cab5b9bf43d38dd9d43969ec7164a0c9782ca1c59a5329c9b6860"},
            {"ggml-cpu-sapphirerapids.dll", 1954304u, "65daa85821f3381e47f2cae93019f2d798ee21c2eb8ab1d1cb192ff1920fc6f4"},
            {"ggml-cpu-skylakex.dll", 1688576u, "8bbc4c0495575fdbdf64494adf74bc49e3b652efc7fcd94e61a7c43c036c0fa7"},
            {"ggml-cpu-sse42.dll", 941568u, "85b5bd3dfcb169ee0db8fe3fcdaf58cd61019a4c075b5d0ac44559982b576bdf"},
            {"ggml-cpu-x64.dll", 934400u, "bea0f77b917474f5add3ea55a99d9b11691ee1ca659ceb2877a62441ba694603"},
            {"ggml-cpu-zen4.dll", 1683968u, "ff38b2b0ec2a12f4d07ea33860244279c016dcd674d9e5fb56bceb5919dfe3fd"},
            {"ggml-rpc-server.exe", 117248u, "9a1abe949252bdafc87fcc44b82bc77dd2e460be184901136c411e6ef66a737c"},
            {"ggml-rpc.dll", 234496u, "8515a1e95f176fb45560c551d541d0562f9b0a33077c20c7e18ad4c71361d205"},
            {"ggml-vulkan.dll", 42399232u, "6c91d33589c15759592995da4baa1402c7045cbd87c753e05dd43a7088f6648b"},
            {"ggml.dll", 78848u, "3719f52b642eb88e80ce8ab0bb33ee2e77cd4c83adbeb09459e9c4a480ba3d3d"},
            {"libomp.dll", 768000u, "a12116ba72d1d6820407cf30be23da04ce79d6bb8a71a5ee71759c5a1faa6f1c"},
            {"llama-batched-bench-impl.dll", 62464u, "835834ef9b9f22ec5e96a64c0978a7b6c6f0dee2acb7cc8c712a873b40f75802"},
            {"llama-batched-bench.exe", 9728u, "bf36634bf9f6ba2a0ceea31841cbfff1f7cecde57aeebf7f716da9345a87f72c"},
            {"llama-bench-impl.dll", 406528u, "870c14553970c42b11f76c2c3e295876ffa4fb8386c738f2aef1f2704a7142e0"},
            {"llama-bench.exe", 9728u, "6c54709285fe050636f1a548c416faeb98ebff0e1d52eea3ea927d8126c1fa41"},
            {"llama-cli-impl.dll", 3399168u, "ab69c99e41ab7a7efb783482a8f31310ff8095d02c8c59ccd580a046ffd524ee"},
            {"llama-cli.exe", 9728u, "08254d6b9710464977e798d8b3ed59dee264548f4b975d8e47085d086d8a68cc"},
            {"llama-common.dll", 8616960u, "36e2a476a6e6e373ee61cd34e2dd8862bfdf6845b8cd674a1c4b55f56995697c"},
            {"llama-completion-impl.dll", 139264u, "594fe239032ad9ec4667fa7be3136b784411ed1e684bc4b96d67d0f4217ad2f6"},
            {"llama-completion.exe", 9728u, "56595af52fcf6ceef15d9851241a36480275c458767f49f637e96c1d94d8235d"},
            {"llama-connect.exe", 11473920u, "9f2bdd778f96971995a80add8d6b2478c156f799933337a19dbf80cdaad21b1d"},
            {"llama-fit-params-impl.dll", 48128u, "bdfe40716250231c398f37fe81c59c58e97e489f5fe0b7b1c774fc93b3290e01"},
            {"llama-fit-params.exe", 9728u, "75b92ece03a9b0cf572afe85eadcb77b9938aa1bf46b72499f070319be7d2b11"},
            {"llama-gemma3-cli.exe", 27648u, "6877706eb2daefc92e4bbb98288425a721694837a31c550f6ad95e27b31f1890"},
            {"llama-gguf-split.exe", 61952u, "d1061886969e9a9073f80f8754e3bb1cc25f2223bae8e2858b9b58b19c8e574c"},
            {"llama-imatrix.exe", 255488u, "1dcd0865cdd33959dc5829158a4e23c9ff0a523632c3d23722f1636ff35a0c55"},
            {"llama-llava-cli.exe", 27648u, "6877706eb2daefc92e4bbb98288425a721694837a31c550f6ad95e27b31f1890"},
            {"llama-minicpmv-cli.exe", 27648u, "6877706eb2daefc92e4bbb98288425a721694837a31c550f6ad95e27b31f1890"},
            {"llama-mtmd-cli.exe", 97792u, "7071c151a7e955759995998550ad37d6a61f17b4f599865d3e72020ac8e204e5"},
            {"llama-mtmd-debug.exe", 57344u, "e5fd9fa5fd21ffd181614149f62f70618a940db44d1867a74c6ea32707186681"},
            {"llama-perplexity-impl.dll", 198144u, "5f1863e1e566d9d55ddb26e1c41954fffa90ba2fe743d2c190d75fee56b20a66"},
            {"llama-perplexity.exe", 9728u, "ecd79e49c4037f9c109f08021b77dba58cd173a2948a45aa5920191198e50cf1"},
            {"llama-quantize-impl.dll", 141312u, "3e64b9014165bb7efb874699d5dd99ad6d4cae107a4e407eefc92c3a616b125a"},
            {"llama-quantize.exe", 9728u, "8e8e18ab1d0c14fc835f10514b3fa22d109aaec7c35ada2cde76cf34be6c014a"},
            {"llama-qwen2vl-cli.exe", 27648u, "6877706eb2daefc92e4bbb98288425a721694837a31c550f6ad95e27b31f1890"},
            {"llama-results.exe", 47616u, "7399b067e46c8a5a129a2d4a2d5241d4fc53ac7d5096c7928f1c74b5c445eb36"},
            {"llama-server-impl.dll", 10480128u, "c5fbd51cab1545c92c6b15f4787d8e68277f1a1ad49dba798c50ee6cc5dc1ea7"},
            {"llama-server.exe", 9728u, "30c18be89647e00bb1045fe9bc8a497279f5e01ae1543d7068dcb1279b13a7a7"},
            {"llama-tokenize.exe", 59904u, "dc35b9bab2d27bd3c8ff764003494e74359c8ac7169a7d52bc534b7fcfae1de7"},
            {"llama-tts.exe", 49152u, "c9b03d9455556877b4c78ca9bb188fa7bf390b2916060dc8f9a1b5c855ee8fad"},
            {"llama.dll", 3928064u, "9556435b91316e8f8b7ac6f012d32e7eb5f019c1e8b637f8f5a8156dfe67dbc7"},
            {"llama.exe", 105984u, "a6e96b4736f0da51e83f58037ffe7deb68ec604d4bb45a086081222e5dccdb84"},
            {"mtmd.dll", 2863104u, "3fc0f2d757439ebfd5e04ec43d08b4182167cbd8c1eec13fd25190c8407744e2"},
        }};

        constexpr OfflineRuntimeFile kApplicationCpuRuntime{
            "epoch-llama-cpu.exe", 5'010'944u,
            "c8a52ad44097a68c057676a23d57fd9c2c08ac3e0de97e6a79c0fdac0ca94bae"};
        constexpr std::size_t kApplicationCpuThreadLimit = 4u;

        [[nodiscard]] static bool is_application_cpu_image(const std::filesystem::path& image)
        {
            return platform::child_process::compare_file_names(
                image.filename(), std::filesystem::path{kApplicationCpuRuntime.name}) == 0;
        }

        struct OfflineRuntimeFiles final
        {
            std::vector<std::shared_ptr<void>> leases;
            std::shared_ptr<void> request_files;
        };

        struct OfflineRuntimeAdmission final
        {
            std::optional<platform::child_process::ExecutableIdentity> image;
            bool pinned_package{};
            std::shared_ptr<OfflineRuntimeFiles> files;
        };

        [[nodiscard]] static OfflineRuntimeAdmission
            verify_offline_runtime_directory(const std::filesystem::path& directory,
                std::span<const OfflineRuntimeFile> requiredFiles, std::string_view entrypoint,
                std::stop_token cancellation)
        {
            namespace fs = std::filesystem;
            namespace child = platform::child_process;
            std::error_code error;
            const auto directoryStatus = fs::symlink_status(directory, error);
            if (error || !fs::is_directory(directoryStatus)) return {};

            std::size_t observed{};
            std::vector<bool> seen(requiredFiles.size());
            std::optional<child::ExecutableIdentity> cli;
            auto files = std::make_shared<OfflineRuntimeFiles>();
            files->leases.reserve(requiredFiles.size());
            for (fs::directory_iterator it{directory, error}, end; !error && it != end; it.increment(error))
            {
                if (cancellation.stop_requested() || ++observed > requiredFiles.size()) return {};
                const auto file = it->path();
                const auto status = it->symlink_status(error);
                if (error || !fs::is_regular_file(status)) return {};

                const auto entry = std::ranges::find_if(requiredFiles, [&](const OfflineRuntimeFile& required)
                {
                    return child::compare_file_names(file.filename(), fs::path{required.name}) == 0;
                });
                if (entry == requiredFiles.end() || fs::file_size(file, error) != entry->bytes || error)
                    return {};
                const auto index = static_cast<std::size_t>(entry - requiredFiles.begin());
                if (seen[index]) return {};
                seen[index] = true;
                child::ExecutableIdentity identity{entry->bytes, std::string{entry->sha256}};
                auto lease = child::lock_verified_file_for_read(file, identity, cancellation);
                if (!lease) return {};
                files->leases.push_back(std::move(lease));
                if (entry->name == entrypoint)
                    cli = std::move(identity);
            }
            if (error || observed != requiredFiles.size() || !cli || cancellation.stop_requested())
                return {};
            return {std::move(cli), true, std::move(files)};
        }

        [[nodiscard]] static OfflineRuntimeAdmission
            admit_offline_runtime(const std::filesystem::path& image,
                const std::filesystem::path& configuredPackageImage, std::stop_token cancellation)
        {
            namespace fs = std::filesystem;
            namespace child = platform::child_process;
            if (is_application_cpu_image(image)) return {{}, true};
            std::error_code error;
            const auto packagedImage = fs::absolute(configuredPackageImage, error).lexically_normal();
            if (error || packagedImage.empty()) return {};
            const auto bin = image.parent_path();
            bool inPackage = bin == packagedImage.parent_path();
            if (!inPackage)
                inPackage = fs::equivalent(bin, packagedImage.parent_path(), error) && !error;
            if (inPackage)
            {
                if (child::compare_file_names(image.filename(), packagedImage.filename()) != 0)
                    return {{}, true};
                auto admission = verify_offline_runtime_directory(bin,
                    kPinnedOfflineRuntimeFiles, "llama-cli.exe", cancellation);
                admission.pinned_package = true;
                return admission;
            }

            // An alias retaining the same image bytes must not import DLLs
            // from another directory under the pinned package identity.
            error.clear();
            if (fs::equivalent(image, packagedImage, error) && !error)
                return {{}, true};
            auto identity = child::inspect_executable(image);
            // The application helper cannot take the ordinary route under an alias.
            if (identity && identity->size_bytes == kApplicationCpuRuntime.bytes
                && identity->sha256 == kApplicationCpuRuntime.sha256)
                return {{}, true};
            return {std::move(identity), false};
        }

        struct OfflineApplicationModel final
        {
            std::filesystem::path path;
            platform::child_process::ExecutableIdentity identity;
        };

        [[nodiscard]] static std::shared_ptr<void> lock_application_model(
            const OfflineApplicationModel& model, const std::filesystem::path& runtime,
            const platform::child_process::WorkspaceIsolation& isolation, std::stop_token cancellation)
        {
            namespace fs = std::filesystem;
            namespace child = platform::child_process;
            const auto directory = model.path.parent_path();
            const auto& grants = isolation.read_only;
            if (directory == runtime || grants.size() != 2u
                || std::ranges::find(grants, directory) == grants.end()
                || std::ranges::find(grants, runtime) == grants.end()
                || !model.path.is_absolute()
                || child::compare_file_names(model.path.extension(), ".gguf") != 0)
                return {};
            std::error_code error;
            if (fs::canonical(model.path, error) != model.path || error)
                return {};
            fs::directory_iterator entry{directory, error};
            const fs::directory_iterator end;
            if (error || entry == end || entry->path() != model.path)
                return {};
            if (entry.increment(error) != end || error)
                return {};
            return child::lock_verified_file_for_read(model.path, model.identity, cancellation);
        }

        [[nodiscard]] static ProcessCapture capture_process(
            const std::filesystem::path& executable,
            const std::filesystem::path& requestDirectory,
            std::string_view correlation,
            const std::vector<std::string>& arguments,
            std::chrono::seconds timeout,
            std::stop_token cancellation,
            const std::shared_ptr<void>& transientFiles,
            const std::optional<platform::child_process::WorkspaceIsolation>& isolation = std::nullopt,
            const std::optional<OfflineApplicationModel>& applicationModel = std::nullopt)
        {
            namespace child = platform::child_process;
            ProcessCapture capture{};
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            if (cancellation.stop_requested())
            {
                capture.cancelled = true;
                return capture;
            }

            std::error_code pathError;
            const auto image = std::filesystem::absolute(executable, pathError).lexically_normal();
            if (pathError || image.empty()) return capture;
            OfflineRuntimeAdmission admission;
            if (!isolation)
                admission = admit_offline_runtime(image, epoch_local_llama_cpp_executable_path(), cancellation);
            else if (is_application_cpu_image(image))
            {
                if (isolation->writable.size() == 1u && isolation->writable.front() == requestDirectory)
                    admission = verify_offline_runtime_directory(image.parent_path(),
                        std::span{&kApplicationCpuRuntime, 1u}, kApplicationCpuRuntime.name, cancellation);
            }
            else if (child::compare_file_names(image.filename(), "llama-cli.exe") == 0)
                admission = verify_offline_runtime_directory(image.parent_path(),
                    kPinnedOfflineRuntimeFiles, "llama-cli.exe", cancellation);
            if (isolation && admission.image)
            {
                auto modelLease = applicationModel
                    ? lock_application_model(*applicationModel, image.parent_path(), *isolation, cancellation)
                    : std::shared_ptr<void>{};
                if (modelLease)
                    admission.files->leases.push_back(std::move(modelLease));
                else
                    admission.image.reset();
            }
            if (!admission.image)
            {
                capture.cancelled = cancellation.stop_requested();
                capture.pinned_package_rejected = (admission.pinned_package || isolation.has_value()) && !capture.cancelled;
                return capture;
            }
            auto environment = child::prepare_workspace_environment(requestDirectory);
            if (!environment) return capture;

            std::shared_ptr<void> retirementGuard = transientFiles;
            if (admission.files)
            {
                admission.files->request_files = transientFiles;
                retirementGuard = std::move(admission.files);
            }

            const auto diagnostics = requestDirectory / "diagnostics.txt";
            const child::LaunchRequest request{
                .executable = image,
                .working_directory = image.parent_path(),
                .merged_output_path = diagnostics,
                .arguments = arguments,
                .correlation_key = std::string{correlation},
                .exclusive_group = "epoch-offline-llama-cli",
                .display_name = "Offline model inference",
                .window_mode = child::WindowMode::hidden,
                .expected_executable = std::move(admission.image),
                .environment = std::move(environment),
                .disconnect_standard_input = true,
                .isolation = isolation,
                .retirement_guard = std::move(retirementGuard),
                .suppress_console = true,
                .merged_output_limit_bytes = kMaximumInferenceOutputBytes,
                .below_normal_priority = true};
            if (cancellation.stop_requested())
            {
                capture.cancelled = true;
                return capture;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                capture.timed_out = true;
                return capture;
            }

            const auto launch = child::launch_or_focus(request);
            if (!launch.owns_new_process()) return capture;

            const auto timeoutNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            const auto result = child::wait(launch.handle, cancellation,
                static_cast<std::uint64_t>((std::max)(std::int64_t{1}, static_cast<std::int64_t>(timeoutNs))));
            if (!result.process || result.process->active() || !child::release(launch.handle))
            {
                // The supervised slot retains its request files until its process tree retires.
                throw ModelTransportRetirementFailure(
                    "The offline model process did not confirm retirement.");
            }

            capture.cancelled = result.code == child::WaitCode::cancelled || cancellation.stop_requested();
            capture.timed_out = result.code == child::WaitCode::timed_out;
            capture.launched = launch.code == child::LaunchCode::started;
            if (result.code == child::WaitCode::exited && result.process->exit_code_valid)
                capture.exit_code = result.process->exit_code;
            if (capture.launched && capture.exit_code != 0 && !capture.cancelled && !capture.timed_out)
                capture.output = read_exact_text_file(diagnostics, kMaximumInferenceOutputBytes);
            return capture;
        }
#else
        static void append_process_output(std::string& output, const char* data, std::size_t size)
        {
            if (output.size() >= kMaximumInferenceOutputBytes)
                return;
            output.append(data, (std::min)(size, kMaximumInferenceOutputBytes - output.size()));
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
                const std::filesystem::path& userPromptPath,
                std::size_t maximumThreads = (std::numeric_limits<std::size_t>::max)())
        {
            const auto pathArgument = [](const std::filesystem::path& path)
            {
                const auto bytes = path.generic_u8string();
                return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            };
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
                "--output-file", pathArgument(outputPath),
                "--system-prompt-file", pathArgument(systemPromptPath),
                "--file", pathArgument(userPromptPath),
                "--ctx-size", std::to_string(config.context_tokens),
                "--n-predict", std::to_string(config.output_tokens),
                "--gpu-layers", std::to_string(config.gpu_layers)
            };
            const auto threads = (std::min)(config.threads, maximumThreads);
            if (threads > 0u)
            {
                arguments.push_back("--threads");
                arguments.push_back(std::to_string(threads));
                arguments.push_back("--threads-batch");
                arguments.push_back(std::to_string(threads));
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
            const auto hasPair = [](const std::vector<std::string>& arguments, std::string_view flag,
                                     std::string_view value)
            {
                for (std::size_t index = 0u; index + 1u < arguments.size(); ++index)
                {
                    if (arguments[index] == flag && arguments[index + 1u] == value)
                        return true;
                }
                return false;
            };
            const auto bounded = direct_llama_cpp_file_arguments(config, outputPath, systemPath, userPath, 4u);
            return hasPair(arguments, "--output-file", "response.txt")
                && hasPair(arguments, "--system-prompt-file", "system.txt")
                && hasPair(arguments, "--file", "user.txt")
                && hasPair(arguments, "--threads", "12")
                && hasPair(arguments, "--threads-batch", "12")
                && hasPair(bounded, "--threads", "4")
                && hasPair(bounded, "--threads-batch", "4")
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
            namespace fs = std::filesystem;
            std::error_code outputError;
            auto isolation = config.application_isolation;
            const auto outputRoot = isolation ? isolation->owned_root / "requests"
                : fs::absolute(fs::path{executable_cache_bucket("ai")} / "transient",
                    outputError).lexically_normal();
            if (isolation)
            {
                if (!outputRoot.is_absolute() || fs::canonical(outputRoot, outputError) != outputRoot
                    || outputError || !fs::is_directory(outputRoot, outputError) || outputError)
                {
                    terminalFailure = ModelTerminalFailure::request_rejected;
                    return "The isolated local runtime has no admitted request directory.";
                }
            }
            else if (!outputError)
                fs::create_directories(outputRoot, outputError);
            if (outputError)
            {
                terminalFailure = ModelTerminalFailure::request_failed;
                return "Direct llama.cpp inference could not prepare its transient response path.";
            }

            const std::uint64_t sequence =
                g_directInferenceOutputSequence.fetch_add(
                    1u, std::memory_order_relaxed);
            const std::uint64_t clock =
                static_cast<std::uint64_t>(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count());
            const std::string transientStem = "llama-cli-"
                + std::to_string(clock) + "-" + std::to_string(sequence);
            const auto requestDirectory = outputRoot / transientStem;
            if (!std::filesystem::create_directory(requestDirectory, outputError))
            {
                terminalFailure = ModelTerminalFailure::request_failed;
                return "Direct llama.cpp inference could not reserve a new transient directory.";
            }
            if (isolation)
                isolation->writable = {requestDirectory};

            struct ScopedTransientFiles final
            {
                explicit ScopedTransientFiles(const std::filesystem::path& root) : directory{root} {}
                ~ScopedTransientFiles()
                {
                    std::error_code ignored;
                    // Remove only the known, request-owned artifacts, never a
                    // child-supplied directory tree. The supervisor may hold
                    // this guard after its caller observes uncertain retirement.
                    if (std::filesystem::canonical(directory, ignored) != directory || ignored)
                        return;
                    for (const auto* leaf : {"response.txt", "system.txt", "user.txt", "diagnostics.txt",
                             "cache/process/profile/AppData/Local", "cache/process/profile/AppData/Roaming",
                             "cache/process/profile/AppData", "cache/process/profile", "cache/process/temp",
                             "cache/process/packages", "cache/process", "cache"})
                    {
                        const auto path = directory / leaf;
                        if (std::filesystem::canonical(path.parent_path(), ignored) == path.parent_path() && !ignored)
                            std::filesystem::remove(path, ignored);
                    }
                    std::filesystem::remove(directory, ignored);
                }
                const std::filesystem::path directory;
            };
            const auto transientCleanup = std::make_shared<ScopedTransientFiles>(requestDirectory);
            const auto outputPath = requestDirectory / "response.txt";
            const auto systemPromptPath = requestDirectory / "system.txt";
            const auto userPromptPath = requestDirectory / "user.txt";

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
                terminalFailure = ModelTerminalFailure::request_failed;
                return "Direct llama.cpp inference could not prepare its transient prompt files.";
            }
            const fs::path executable = isolation
                ? fs::path{std::u8string{config.executable.begin(), config.executable.end()}}
                : fs::path{config.executable};
            auto maximumThreads = (std::numeric_limits<std::size_t>::max)();
#if defined(_WIN32)
            if (isolation && is_application_cpu_image(executable))
                maximumThreads = kApplicationCpuThreadLimit;
#endif
            const auto arguments = direct_llama_cpp_file_arguments(
                config, outputPath, systemPromptPath, userPromptPath, maximumThreads);

            notify_model_stage(observer, ModelRequestStage::sending);
            const auto attemptStarted = std::chrono::steady_clock::now();
#if defined(_WIN32)
            std::optional<OfflineApplicationModel> applicationModel;
            if (isolation && config.application_model_identity)
                applicationModel.emplace(fs::path{std::u8string{config.model.begin(), config.model.end()}},
                    *config.application_model_identity);
            const ProcessCapture capture = capture_process(
                executable, requestDirectory, transientStem, arguments,
                std::chrono::seconds{config.timeout_seconds}, cancellation, transientCleanup, isolation, applicationModel);
#else
            const ProcessCapture capture = capture_process(
                executable, arguments,
                std::chrono::seconds{config.timeout_seconds}, cancellation);
#endif
            if (capture.cancelled || cancellation.stop_requested())
            {
                terminalFailure = ModelTerminalFailure::cancelled;
                return std::string{kModelRequestCancelled};
            }
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
            if (!capture.launched)
            {
                terminalFailure = capture.pinned_package_rejected
                    ? ModelTerminalFailure::request_rejected : ModelTerminalFailure::request_failed;
                return capture.pinned_package_rejected
                    ? "Direct llama.cpp inference could not start because the installed runtime files do not match the pinned package."
                    : "Direct llama.cpp inference could not start. Check the configured local executable.";
            }
            if (capture.exit_code != 0)
            {
                terminalFailure = ModelTerminalFailure::request_failed;
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
            const std::string transcript = trim(read_exact_text_file(outputPath, kMaximumInferenceOutputBytes));
            std::string reply = normalize_direct_llama_cpp_transcript(transcript, userText);
            if (reply.empty())
            {
                terminalFailure = ModelTerminalFailure::request_failed;
                return transcript.empty()
                    ? "Direct llama.cpp inference returned no parseable assistant content: the response file was empty."
                    : "Direct llama.cpp inference returned no parseable assistant content: no framed assistant turn or strict source packet was found.";
            }
            return reply;
        }
