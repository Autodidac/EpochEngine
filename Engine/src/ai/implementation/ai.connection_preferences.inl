        [[nodiscard]] static bool valid_preferred_model(std::string_view model)
        {
            return !model.empty() && model.size() <= 512u
                && std::none_of(model.begin(), model.end(), [](unsigned char value)
                    { return value < 32u || value == 127u; });
        }

        [[nodiscard]] static std::string local_endpoint_identity(std::string endpoint)
        {
            if (endpoint.empty() || endpoint.size() > 512u
                || std::any_of(endpoint.begin(), endpoint.end(), [](unsigned char value)
                    { return value <= 32u || value >= 127u || value == '\\'; })
                || endpoint.find_first_of("?#@") != std::string::npos)
                return {};
            const auto schemeEnd = endpoint.find("://");
            if (schemeEnd == std::string::npos)
                return {};
            std::string scheme = endpoint.substr(0u, schemeEnd);
            std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if (scheme != "http" && scheme != "https")
                return {};
            const auto authorityEnd = endpoint.find('/', schemeEnd + 3u);
            std::string authority = endpoint.substr(schemeEnd + 3u,
                authorityEnd == std::string::npos ? std::string::npos
                    : authorityEnd - schemeEnd - 3u);
            std::transform(authority.begin(), authority.end(), authority.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            const auto portStart = authority.starts_with("[")
                ? authority.find(':', authority.find(']')) : authority.find(':');
            const std::string host = authority.substr(0u, portStart);
            if (host != "localhost" && host != "127.0.0.1" && host != "[::1]")
                return {};
            std::string port{};
            if (portStart != std::string::npos)
            {
                const auto rawPort = std::string_view{authority}.substr(portStart + 1u);
                if (rawPort.empty() || rawPort.size() > 5u)
                    return {};
                unsigned value{};
                for (const char digit : rawPort)
                {
                    if (digit < '0' || digit > '9') return {};
                    value = value * 10u + static_cast<unsigned>(digit - '0');
                }
                if (value == 0u || value > 65535u) return {};
                if (!((scheme == "http" && value == 80u)
                    || (scheme == "https" && value == 443u)))
                    port = ":" + std::to_string(value);
            }
            std::string suffix = authorityEnd == std::string::npos
                ? std::string{} : endpoint.substr(authorityEnd);
            rstrip_slashes(suffix);
            if (!suffix.empty() && suffix != "/v1"
                && suffix != "/v1/chat/completions" && suffix != "/v1/models"
                && suffix != "/v1/responses" && suffix != "/api/tasks"
                && suffix != "/api/v1/chat" && suffix != "/api/v1"
                && suffix != "/api/v1/models")
                return {};
            return scheme + "://" + host + port + "/v1/chat/completions";
        }

        [[nodiscard]] static LocalApiEndpoint local_api_routes(std::string_view endpoint)
        {
            const auto chat = local_endpoint_identity(trim(endpoint));
            if (chat.empty()) return {};
            const auto root = chat.substr(0u,
                chat.size() - std::string_view{"/v1/chat/completions"}.size());
            return {root + "/v1", chat, root + "/v1/responses", root + "/api/tasks"};
        }

        struct LocalEndpointPreferences
        {
            std::vector<std::string> endpoints{};
            std::string selected{};
            std::vector<std::string> non_streaming{};
            std::vector<LocalContextOverride> context_overrides{};
        };
        constexpr std::string_view kLocalEndpointsHeader = "EPOCH_LOCAL_API_ENDPOINTS_V1\n";

        [[nodiscard]] static std::optional<LocalEndpointPreferences>
            decode_endpoint_preferences(std::string_view bytes)
        {
            if (bytes.size() > 16'384u || !bytes.starts_with(kLocalEndpointsHeader))
                return std::nullopt;
            bytes.remove_prefix(kLocalEndpointsHeader.size());
            LocalEndpointPreferences preferences{};
            while (!bytes.empty())
            {
                const auto newline = bytes.find('\n');
                if (newline == std::string_view::npos) return std::nullopt;
                const auto line = bytes.substr(0u, newline);
                bytes.remove_prefix(newline + 1u);
                if (line.starts_with("context="))
                {
                    const auto first = line.find('\t');
                    const auto second = first == std::string_view::npos ? first : line.find('\t', first + 1u);
                    if (first == std::string_view::npos || second == std::string_view::npos
                        || preferences.context_overrides.size() >= 16u) return std::nullopt;
                    std::size_t tokens{};
                    const auto parsed = std::from_chars(line.data() + 8u, line.data() + first, tokens);
                    const auto endpoint = line.substr(first + 1u, second - first - 1u);
                    const auto model = line.substr(second + 1u);
                    if (parsed.ec != std::errc{} || parsed.ptr != line.data() + first
                        || tokens < 8192u || tokens > 1u * 1024u * 1024u
                        || local_api_routes(endpoint).base_url != endpoint || !valid_preferred_model(model)
                        || std::ranges::any_of(preferences.context_overrides, [&](const auto& value)
                            { return value.endpoint == endpoint && value.model == model; })) return std::nullopt;
                    preferences.context_overrides.push_back({std::string{endpoint}, std::string{model}, tokens});
                    continue;
                }
                const bool selected = line.starts_with("selected=");
                const bool nonStreaming = line.starts_with("nostream=");
                if (!selected && !nonStreaming && !line.starts_with("endpoint=")) return std::nullopt;
                const auto routes = local_api_routes(line.substr(9u));
                if (routes.base_url.empty() || routes.base_url != line.substr(9u))
                    return std::nullopt;
                if (nonStreaming)
                {
                    if (preferences.non_streaming.size() >= 16u
                        || std::find(preferences.non_streaming.begin(), preferences.non_streaming.end(),
                            routes.base_url) != preferences.non_streaming.end()) return std::nullopt;
                    preferences.non_streaming.push_back(routes.base_url);
                }
                else if (selected)
                {
                    if (!preferences.selected.empty()) return std::nullopt;
                    preferences.selected = routes.base_url;
                }
                else
                {
                    if (preferences.endpoints.size() >= 16u
                        || std::find(preferences.endpoints.begin(), preferences.endpoints.end(),
                            routes.base_url) != preferences.endpoints.end()) return std::nullopt;
                    preferences.endpoints.push_back(routes.base_url);
                }
            }
            if (preferences.selected.empty()
                || std::find(preferences.endpoints.begin(), preferences.endpoints.end(),
                    preferences.selected) == preferences.endpoints.end()) return std::nullopt;
            for (const auto& endpoint : preferences.non_streaming)
                if (std::find(preferences.endpoints.begin(), preferences.endpoints.end(), endpoint)
                    == preferences.endpoints.end()) return std::nullopt;
            for (const auto& value : preferences.context_overrides)
                if (std::ranges::find(preferences.endpoints, value.endpoint) == preferences.endpoints.end())
                    return std::nullopt;
            return preferences;
        }

        [[nodiscard]] static std::string encode_endpoint_preferences(
            const LocalEndpointPreferences& preferences)
        {
            std::string bytes{kLocalEndpointsHeader};
            bytes += "selected=" + preferences.selected + '\n';
            for (const auto& endpoint : preferences.endpoints)
                bytes += "endpoint=" + endpoint + '\n';
            for (const auto& endpoint : preferences.non_streaming)
                bytes += "nostream=" + endpoint + '\n';
            for (const auto& value : preferences.context_overrides)
                bytes += "context=" + std::to_string(value.tokens) + '\t' + value.endpoint + '\t' + value.model + '\n';
            return decode_endpoint_preferences(bytes) ? bytes : std::string{};
        }

        static void restore_endpoint_preferences_if_needed()
        {
            if (g_endpointPreferencesLoaded) return;
            g_endpointPreferencesLoaded = true;
            const auto preferences = decode_endpoint_preferences(
                read_small_text_file(local_endpoint_preference_file(), 16'384u));
            if (!preferences) return;
            g_savedLocalEndpoints = preferences->endpoints;
            g_nonStreamingLocalEndpoints = preferences->non_streaming;
            g_localContextOverrides = preferences->context_overrides;
            // An explicit process configuration always outranks persisted UI state.
            if (configured_endpoint_override().empty()) g_selectedEndpoint = preferences->selected;
        }

        [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
            local_model_headers(const std::string& endpoint, std::string token,
                std::string_view authorizedEndpoint = kOriginalLocalEndpoint,
                bool allowOriginalAliases = false)
        {
            const auto identity = local_endpoint_identity(endpoint);
            const auto authorized = local_endpoint_identity(std::string{authorizedEndpoint});
            const bool originalEnvironmentHost = allowOriginalAliases
                && authorizedEndpoint == kOriginalLocalEndpoint
                && (identity == kOriginalLocalEndpoint
                    || identity == "http://127.0.0.1:1234/v1/chat/completions"
                    || identity == "http://[::1]:1234/v1/chat/completions");
            if (identity.empty() || (!originalEnvironmentHost && identity != authorized))
                return {};
            if (token.empty()) return {};
            if (token.size() > 4096u || std::any_of(token.begin(), token.end(),
                [](unsigned char value) { return value <= 32u || value >= 127u; }))
                throw std::runtime_error("LM_API_TOKEN contains invalid header characters.");
            return {{"Authorization", "Bearer " + token}};
        }

        [[nodiscard]] static std::string model_http_status_message(unsigned status)
        {
            if (status == 401u || status == 403u)
                return "Local model authentication failed at the selected endpoint; "
                    "open Model Settings and use Paste API Key, then Rescan Models.";
            return "Model endpoint returned HTTP status " + std::to_string(status);
        }

        static std::string extract_json_error_message(const std::string& response);

        class ModelHttpFailure final : public std::runtime_error
        {
        public:
            ModelHttpFailure(unsigned status, const std::string& response)
                : std::runtime_error(model_http_status_message(status)), status(status)
            {
                // Classify only the API error message, never a successful model
                // result. Do not copy provider bodies (which can echo prompts,
                // credentials or paths) into ordinary logs or the chat surface.
                const auto detail = lowercase_ascii(extract_json_error_message(response));
                provider_timeout = status == 408u || status == 504u
                    || (status >= 500u && (detail.find("timed out") != std::string::npos
                        || detail.find("timeout") != std::string::npos
                        || detail.find("time-out") != std::string::npos
                        || detail.find("deadline") != std::string::npos));
                provider_model_unloaded = detail.find("model unloaded") != std::string::npos
                    || detail.find("explicitmodelunloaderror") != std::string::npos;
                rejected = status >= 400u && status < 500u
                    && status != 408u && status != 429u;
            }

            [[nodiscard]] std::string diagnostic() const
            {
                if (provider_model_unloaded)
                    return std::string{what()} +
                        ". The upstream model was unloaded during this request. No partial action was admitted "
                        "and Epoch did not reload it or resend the request. Select an available model before resuming; "
                        "the saved plan and accepted sandbox remain retained.";
                if (provider_timeout)
                    return std::string{what()} +
                        ". The provider or its upstream inference timed out before Epoch's request budget expired. "
                        "The upstream generation may still be running; the same request was not resent. "
                        "Check the provider's inference timeout and active generation before resuming.";
                return std::string{what()} + (rejected
                    ? ". The request was rejected; identical automatic retries cannot repair authentication, model selection or unsupported request parameters."
                    : ". The provider returned a temporary HTTP failure.");
            }

            unsigned status{};
            bool provider_timeout{};
            bool provider_model_unloaded{};
            bool rejected{};
        };

        [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
            model_request_headers(const std::string& endpoint)
        {
            const std::lock_guard<std::recursive_mutex> lock{g_aiStateMutex};
            return g_localModelApiToken
                ? local_model_headers(endpoint, *g_localModelApiToken, g_localModelApiTokenEndpoint)
                : local_model_headers(endpoint, read_env_var("LM_API_TOKEN"), kOriginalLocalEndpoint, true);
        }

        [[nodiscard]] static LocalModelPreference decode_model_preference(std::string_view bytes)
        {
            if (bytes.empty() || bytes.size() > 2048u)
                return {};
            if (!bytes.starts_with(kLocalModelPreferenceHeader))
            {
                if (bytes.starts_with("EPOCH_LOCAL_MODEL_PREFERENCE_")) return {};
                const std::string model = trim(bytes);
                return valid_preferred_model(model)
                    ? LocalModelPreference{model, {}, true} : LocalModelPreference{};
            }
            bytes.remove_prefix(kLocalModelPreferenceHeader.size());
            constexpr std::string_view endpointKey = "endpoint=";
            constexpr std::string_view modelKey = "model=";
            if (!bytes.starts_with(endpointKey)) return {};
            const auto newline = bytes.find('\n');
            if (newline == std::string_view::npos) return {};
            const std::string endpoint = local_endpoint_identity(
                std::string{bytes.substr(endpointKey.size(), newline - endpointKey.size())});
            bytes.remove_prefix(newline + 1u);
            if (!bytes.starts_with(modelKey) || endpoint.empty()) return {};
            bytes.remove_prefix(modelKey.size());
            if (bytes.ends_with('\n')) bytes.remove_suffix(1u);
            if (!valid_preferred_model(bytes)) return {};
            return {std::string{bytes}, endpoint, false};
        }

        [[nodiscard]] static std::string encode_model_preference(const LocalModelPreference& preference)
        {
            const std::string endpoint = local_endpoint_identity(preference.endpoint);
            if (endpoint.empty() || !valid_preferred_model(preference.model_id)) return {};
            return std::string{kLocalModelPreferenceHeader} + "endpoint=" + endpoint
                + "\nmodel=" + preference.model_id + "\n";
        }

        [[nodiscard]] static LocalModelSelection resolve_model_selection(
            std::string current, LocalModelSelectionOrigin currentOrigin,
            const std::string& endpoint, const LocalModelPreference& remembered,
            const std::vector<std::string>& inventory, bool confirmed)
        {
            LocalModelSelection selection{};
            selection.endpoint = endpoint;
            const std::string localEndpoint = local_endpoint_identity(endpoint);
            if (!current.empty())
            {
                if (!valid_preferred_model(current)) return selection;
                selection.model_id = std::move(current);
                selection.origin = currentOrigin;
            }
            else if (!localEndpoint.empty() && !remembered.legacy
                && remembered.endpoint == localEndpoint
                && valid_preferred_model(remembered.model_id)
                && remembered.model_id != "nvidia/nemotron-3-nano-4b")
            {
                selection.model_id = remembered.model_id;
                selection.origin = LocalModelSelectionOrigin::remembered;
            }
            else if (localEndpoint == kOriginalLocalEndpoint && remembered.legacy
                && valid_preferred_model(remembered.model_id)
                && remembered.model_id != "nvidia/nemotron-3-nano-4b")
            {
                // Old model-only preferences are reused only on the original
                // local endpoint, and upgraded only by an actual Send/Start.
                selection.model_id = remembered.model_id;
                selection.origin = LocalModelSelectionOrigin::legacy_preference;
            }
            else if (!localEndpoint.empty())
            {
                selection.model_id = kDefaultLocalModel;
                selection.origin = LocalModelSelectionOrigin::default_local;
            }
            selection.available_in_inventory = !selection.model_id.empty()
                && std::find(inventory.begin(), inventory.end(), selection.model_id) != inventory.end();
            selection.confirmed = confirmed && !selection.model_id.empty();
            if (localEndpoint.empty() || selection.model_id.empty()) return selection;
            switch (selection.origin)
            {
            case LocalModelSelectionOrigin::configured:
            case LocalModelSelectionOrigin::explicit_selection:
                selection.reusable_on_request = true;
                break;
            case LocalModelSelectionOrigin::remembered:
                selection.reusable_on_request = remembered.endpoint == localEndpoint
                    && remembered.model_id == selection.model_id && !remembered.legacy;
                break;
            case LocalModelSelectionOrigin::legacy_preference:
                selection.reusable_on_request = localEndpoint == kOriginalLocalEndpoint
                    && remembered.legacy && remembered.model_id == selection.model_id;
                break;
            case LocalModelSelectionOrigin::default_local:
                // A newly configured endpoint must not inherit remembered
                // consent. The automatic Qwen3.8 coding default is for the original host.
                selection.reusable_on_request = localEndpoint == kOriginalLocalEndpoint;
                break;
            default: break;
            }
            return selection;
        }

        [[nodiscard]] static bool model_request_selection_permitted(
            const LocalModelSelection& selection, bool projectAllowsLocalModel) noexcept
        {
            return projectAllowsLocalModel && !selection.model_id.empty()
                && (selection.confirmed || selection.reusable_on_request);
        }

        // File format is one portable, bounded model-id/choice pair per line.
        // No endpoint URL, API key, model weights or transcript is persisted.
        static void restore_thinking_preferences_if_needed()
        {
            if (g_thinkingPreferencesLoaded) return;
            g_thinkingPreferencesLoaded = true;
            const std::string saved = read_small_text_file(thinking_preference_file());
            std::istringstream lines{saved};
            std::string line;
            while (std::getline(lines, line) && g_thinkingOverrides.size() < 64u)
            {
                const auto sep = line.find('\t');
                if (sep == std::string::npos || sep == 0u || sep > 256u) continue;
                const auto key = line.substr(0u, sep);
                const auto choice = std::string_view{line}.substr(sep + 1u);
                if (choice.compare("off") == 0) g_thinkingOverrides[key] = thinking::Mode::off;
                else if (choice.compare("low") == 0) g_thinkingOverrides[key] = thinking::Mode::low;
                else if (choice.compare("high") == 0) g_thinkingOverrides[key] = thinking::Mode::high;
            }
        }
        [[nodiscard]] static thinking::Mode resolved_thinking_mode(std::string_view model)
        {
            const std::lock_guard<std::recursive_mutex> lock{g_aiStateMutex};
            restore_thinking_preferences_if_needed();
            const auto found = g_thinkingOverrides.find(std::string{model});
            return thinking::effective(found == g_thinkingOverrides.end()
                ? thinking::Mode::automatic : found->second, model);
        }
        static void persist_thinking_preferences()
        {
            std::string serialized;
            for (const auto& [key, choice] : g_thinkingOverrides)
            {
                if (key.empty() || key.size() > 256u) continue;
                serialized += key;
                serialized += '\t';
                if (choice == thinking::Mode::off) serialized += "off";
                else if (choice == thinking::Mode::low) serialized += "low";
                else if (choice == thinking::Mode::high) serialized += "high";
                else continue;
                serialized += '\n';
            }
            (void)write_text_file(thinking_preference_file(), serialized);
        }

        static void restore_selected_model_preference_if_needed()
        {
            if (g_selectedModelPreferenceLoaded)
                return;
            g_selectedModelPreferenceLoaded = true;
            g_rememberedModel = decode_model_preference(
                read_small_text_file(selected_model_preference_file()));
            const auto selection = resolve_model_selection(g_selectedModel, g_selectedModelOrigin,
                g_selectedEndpoint, g_rememberedModel, g_detectedModels, g_modelUseConfirmedForSession);
            g_selectedModel = selection.model_id;
            g_selectedModelOrigin = selection.origin;
            if (!g_selectedModel.empty())
                g_modelDetectionStatus = "Selected local model: " + g_selectedModel
                    + "; availability has not been checked and no model was loaded.";
        }

        static void persist_selected_model_preference(std::string_view model_id)
        {
            const std::string selected = trim(model_id);
            const LocalModelPreference preference{selected, local_endpoint_identity(g_selectedEndpoint), false};
            const std::string encoded = encode_model_preference(preference);
            if (encoded.empty()) return; // Remote selections never grant local reuse.
            if (!write_text_file(selected_model_preference_file(), encoded))
                core::log::error("ai", "Failed to persist selected OS model preference.");
            else
            {
                g_rememberedModel = preference;
                if (g_selectedModelOrigin == LocalModelSelectionOrigin::legacy_preference)
                    g_selectedModelOrigin = LocalModelSelectionOrigin::remembered;
            }
        }

        static void restore_runtime_preference_if_needed()
        {
            restore_endpoint_preferences_if_needed();
            if (g_runtimePreferenceLoaded)
                return;
            g_runtimePreferenceLoaded = true;

            std::istringstream input(read_small_text_file(local_runtime_preference_file(), 16384));
            std::string line;
            while (std::getline(input, line))
            {
                const auto separator = line.find('=');
                if (separator == std::string::npos)
                    continue;
                const std::string key = trim(std::string_view{line}.substr(0u, separator));
                const std::string value = trim(std::string_view{line}.substr(separator + 1u));
                if (key == "transport" && value == "llama_cpp_cli")
                    g_localTransport = LocalInferenceTransport::LlamaCppCli;
                else if (key == "transport" && value == "openai_compatible")
                    g_localTransport = LocalInferenceTransport::OpenAiCompatible;
                else if (key == "executable" && g_directExecutable.empty())
                    g_directExecutable = value;
                else if (key == "model" && g_directModel.empty())
                    g_directModel = value;
            }
        }

        static void persist_runtime_preference()
        {
            std::ostringstream output;
            output << "transport="
                   << (g_localTransport == LocalInferenceTransport::LlamaCppCli
                       ? "llama_cpp_cli" : "openai_compatible") << '\n';
            output << "executable=" << g_directExecutable << '\n';
            output << "model=" << g_directModel << '\n';
            if (!write_text_file(local_runtime_preference_file(), output.str()))
                core::log::error("ai", "Failed to persist local inference runtime preference.");
        }

        [[nodiscard]] static bool regular_file(const std::filesystem::path& path)
        {
            std::error_code ec;
            return !path.empty() && std::filesystem::is_regular_file(path, ec);
        }
        [[nodiscard]] static bool regular_file_with_size(
            const std::filesystem::path& path,
            std::uint64_t expectedBytes)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) || ec)
                return false;
            const auto bytes = std::filesystem::file_size(path, ec);
            return !ec && bytes == expectedBytes;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_llama_cpp_root_path()
        {
            return std::filesystem::path{executable_cache_bucket("packages")}
                / std::string{kEpochLocalLlamaCppRuntimePackageId};
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_llama_cpp_executable_path()
        {
            std::filesystem::path path = epoch_local_llama_cpp_root_path()
                / "versions" / std::string{kEpochLocalLlamaCppRelease} / "bin";
#if defined(_WIN32)
            path /= "llama-cli.exe";
#else
            path /= "llama-cli";
#endif
            return path;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_legacy_root_path()
        {
            return std::filesystem::path{executable_cache_bucket("models")}
                / std::string{kEpochLocalQwen38ModelPackageId};
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_root_path()
        {
            const auto plan = model_install::plan_for(
                kEpochLocalQwen38ModelPackageId);
            if (!plan || !model_install::valid_plan(*plan))
                return {};
            const std::filesystem::path modelsRoot{
                executable_cache_bucket("models")};
            const std::filesystem::path versioned =
                model_install::version_root(modelsRoot, *plan);
            return versioned.empty()
                ? modelsRoot / plan->package_id / "versions" / plan->revision
                : versioned;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_model_path()
        {
            return epoch_local_qwen38_root_path()
                / std::string{kEpochLocalQwen38ModelFile};
        }

        [[nodiscard]] static bool receipt_contains(
            const std::filesystem::path& path,
            std::initializer_list<std::string_view> fields)
        {
            const std::string receipt = read_small_text_file(path, 64u * 1024u);
            return !receipt.empty()
                && std::all_of(fields.begin(), fields.end(),
                    [&](std::string_view field)
                    {
                        return receipt.find(field) != std::string::npos;
                    });
        }

        [[nodiscard]] static std::filesystem::path project_ai_profile_identity(
            const std::filesystem::path& configured,
            const std::filesystem::path& workingDirectory)
        {
            if (configured.empty()) return {};
            if (configured.is_absolute()) return configured.lexically_normal();
            if (!workingDirectory.is_absolute()) return {};
            return (workingDirectory / configured).lexically_normal();
        }

        static void apply_project_ai_profile_if_present()
        {
            const std::string configured =
                trim_env_value(read_env_var("EPOCH_PROJECT_AI_PROFILE"));
            if (configured.empty()) return;
            std::error_code pathError{};
            const std::filesystem::path requested{configured};
            const auto profilePath = project_ai_profile_identity(requested,
                requested.is_absolute() ? std::filesystem::path{}
                    : std::filesystem::current_path(pathError));
            if (pathError || profilePath.empty())
            {
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_projectAiAllowsLocalFallback = false;
                g_modelDetectionStatus = "Project AI profile rejected: its configured path could not be resolved.";
                return;
            }
            const std::string identity = profilePath.generic_string();
            if (identity == g_loadedProjectAiProfile) return;

            g_projectAiAllowsLocalFallback = false;

            const std::string profile =
                read_small_text_file(profilePath, 64u * 1024u);
            if (profile.empty())
            {
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI profile rejected: the configured profile could not be read within its bounded size.";
                return;
            }

            g_loadedProjectAiProfile = identity;
            const project_profile::CodecResult decoded =
                project_profile::parse_profile(profile);
            if (!decoded)
            {
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI profile rejected: " + decoded.status;
                return;
            }

            switch (decoded.profile.provider)
            {
            case project_profile::Provider::disabled:
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI is disabled by its explicit project profile.";
                return;
            case project_profile::Provider::epoch_local_qwen38:
            {
                const EpochLocalAiInstallStatus installed =
                    epoch_local_ai_install_status();
                if (!installed.ready())
                {
                    g_engineAi.reset();
                    g_modelUseConfirmedForSession = false;
                    g_modelDetectionStatus = installed.message;
                    return;
                }
                g_localTransport =
                    LocalInferenceTransport::LlamaCppCli;
                g_directExecutable = installed.runtime_executable;
                g_directModel = installed.model_file;
                g_selectedModel =
                    std::string{kEpochLocalQwen38ModelFile};
                g_modelUseConfirmedForSession = true;
                g_projectAiAllowsLocalFallback = true;
                g_modelDetectionStatus =
                    "Project selected the installed Epoch-local Qwen3.8 provider.";
                return;
            }
            case project_profile::Provider::external_mcp:
                g_projectAiAllowsLocalFallback = true;
                g_localTransport =
                    LocalInferenceTransport::OpenAiCompatible;
                g_modelUseConfirmedForSession =
                    !g_selectedModel.empty();
                g_modelDetectionStatus = g_selectedModel.empty()
                    ? "Project selected external MCP/OpenAI-compatible inference; choose the operator-managed model."
                    : "Project selected the existing external MCP/OpenAI-compatible provider.";
                return;
            case project_profile::Provider::engine_selected:
                g_projectAiAllowsLocalFallback = true;
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project delegates AI to the engine-selected provider; confirm the current local or external model for this session.";
                return;
            }
        }
        [[nodiscard]] static std::filesystem::path find_path_executable(std::string_view name)
        {
            const std::filesystem::path requested{std::string{name}};
            if (regular_file(requested))
                return std::filesystem::absolute(requested);

            const std::string pathValue = read_env_var("PATH");
#if defined(_WIN32)
            constexpr char separator = ';';
            constexpr std::string_view suffix = ".exe";
#else
            constexpr char separator = ':';
            constexpr std::string_view suffix{};
#endif
            std::size_t first = 0u;
            while (first <= pathValue.size())
            {
                const std::size_t pastLast = pathValue.find(separator, first);
                const std::string_view item = std::string_view{pathValue}.substr(
                    first,
                    pastLast == std::string::npos ? std::string::npos : pastLast - first);
                if (!item.empty())
                {
                    std::filesystem::path candidate = std::filesystem::path{std::string{item}} / requested;
                    if (candidate.extension().empty())
                        candidate += suffix;
                    if (regular_file(candidate))
                        return std::filesystem::absolute(candidate);
                }
                if (pastLast == std::string::npos)
                    break;
                first = pastLast + 1u;
            }
            return {};
        }

        [[nodiscard]] static std::filesystem::path discover_llama_executable(
            std::optional<std::string_view> preferred = std::nullopt)
        {
            const std::filesystem::path configured = preferred
                ? std::filesystem::path{std::string{*preferred}}
                : std::filesystem::path{g_directExecutable};
            if (regular_file(configured))
                return std::filesystem::absolute(configured);
            if (const auto installed = epoch_local_llama_cpp_executable_path();
                regular_file(installed))
                return std::filesystem::absolute(installed);

            const std::filesystem::path cacheRoot{executable_cache_bucket("packages")};
            constexpr std::string_view names[] = {
#if defined(_WIN32)
                "llama-cli.exe", "main.exe"
#else
                "llama-cli", "main"
#endif
            };
            const std::filesystem::path roots[] = {
                cacheRoot / "local_ai_llama_cpp_runtime" / "bin",
                cacheRoot / "llama.cpp" / "bin",
                cacheRoot / "llama.cpp" / "build" / "bin"
            };
            for (const auto& root : roots)
                for (const auto name : names)
                    if (const auto candidate = root / name; regular_file(candidate))
                        return std::filesystem::absolute(candidate);
            for (const auto name : names)
                if (const auto candidate = find_path_executable(name); !candidate.empty())
                    return candidate;
            return {};
        }

        [[nodiscard]] static std::filesystem::path discover_gguf_model(
            std::optional<std::string_view> preferred = std::nullopt)
        {
            const std::filesystem::path configured = preferred
                ? std::filesystem::path{std::string{*preferred}}
                : std::filesystem::path{g_directModel};
            if (regular_file(configured))
                return std::filesystem::absolute(configured);
            if (const auto installed = epoch_local_qwen38_model_path();
                regular_file_with_size(installed, kEpochLocalQwen38ModelBytes))
                return std::filesystem::absolute(installed);

            const std::filesystem::path root{executable_cache_bucket("models")};
            std::error_code ec;
            if (!std::filesystem::exists(root, ec))
                return {};

            std::filesystem::path best;
            std::size_t visited = 0u;
            std::filesystem::recursive_directory_iterator iterator{
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec};
            const std::filesystem::recursive_directory_iterator end{};
            while (!ec && iterator != end && visited++ < 8192u)
            {
                const auto path = iterator->path();
                if (iterator->is_regular_file(ec) && path.extension() == ".gguf"
                    && (best.empty() || path.generic_string() < best.generic_string()))
                    best = path;
                iterator.increment(ec);
            }
            return best.empty() ? best : std::filesystem::absolute(best);
        }

        static std::string utc_timestamp_slug()
        {
            const std::time_t now = std::time(nullptr);
            std::tm utc{};
#if defined(_WIN32)
            gmtime_s(&utc, &now);
#else
            gmtime_r(&now, &utc);
#endif

            char buffer[32]{};
            if (std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%SZ", &utc) == 0)
                return "timestamp";
            return buffer;
        }

