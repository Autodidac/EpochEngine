    bool direct_llama_cpp_prompt_transport_contract()
    {
        return direct_llama_cpp_prompt_file_arguments_contract();
    }

    bool local_model_preference_contract()
    {
        const auto engCoder = local_api_routes("http://127.0.0.1:14321/v1");
        const LocalEndpointPreferences savedEndpoints{
            {"http://localhost:1234/v1", engCoder.base_url}, engCoder.base_url, {engCoder.base_url},
            {{engCoder.base_url, "engcoder-foundation", 81'920u}}};
        const auto encodedEndpoints = encode_endpoint_preferences(savedEndpoints);
        const auto decodedEndpoints = decode_endpoint_preferences(encodedEndpoints);
        if (!decodedEndpoints || decodedEndpoints->endpoints != savedEndpoints.endpoints
            || decodedEndpoints->selected != savedEndpoints.selected
            || decodedEndpoints->non_streaming != savedEndpoints.non_streaming
            || decodedEndpoints->context_overrides != savedEndpoints.context_overrides
            || engCoder.chat_completions_url != "http://127.0.0.1:14321/v1/chat/completions"
            || engCoder.responses_url != "http://127.0.0.1:14321/v1/responses"
            || engCoder.agent_tasks_url != "http://127.0.0.1:14321/api/tasks"
            || !engCoder.stream_replies) return false;
        const LocalEndpointPreferences streamingEndpoints{
            savedEndpoints.endpoints, engCoder.base_url, {}};
        const auto decodedStreaming = decode_endpoint_preferences(
            encode_endpoint_preferences(streamingEndpoints));
        if (!decodedStreaming || !decodedStreaming->non_streaming.empty()
            || decodedStreaming->selected != engCoder.base_url) return false;
        for (const auto& route : {engCoder.base_url, engCoder.chat_completions_url,
                engCoder.responses_url, engCoder.agent_tasks_url})
        {
            if (local_api_routes(route + '/').base_url != engCoder.base_url
                || normalize_openai_chat_endpoint(route) != engCoder.chat_completions_url
                || normalize_model_list_endpoint(route) != "http://127.0.0.1:14321/v1/models")
                return false;
        }
        for (const auto invalid : {"http://127.0.0.1:14321/v1?api_key=secret",
                "http://secret@127.0.0.1:14321/v1", "http://127.0.0.1.evil.test:14321/v1",
                "http://127.0.0.1:14321/unsupported", "http://127.0.0.1:14321/v1\nendpoint=x",
                "file:///etc/passwd", "http://[::1]:65536/v1"})
            if (!local_api_routes(invalid).base_url.empty()) return false;
        for (const auto& invalid : {encodedEndpoints + "token=secret\n",
                encodedEndpoints + "selected=" + engCoder.base_url + '\n',
                encodedEndpoints + "endpoint=" + engCoder.base_url + '\n',
                encodedEndpoints + "nostream=" + engCoder.base_url + '\n',
                encodedEndpoints + "nostream=http://localhost:14322/v1\n",
                encodedEndpoints + "context=81920\t" + engCoder.base_url + "\tengcoder-foundation\n",
                encodedEndpoints + "context=0\t" + engCoder.base_url + "\tother\n",
                encodedEndpoints + "context=81920x\t" + engCoder.base_url + "\tother\n",
                encodedEndpoints + "context=2097152\t" + engCoder.base_url + "\tother\n",
                encodedEndpoints + "context=81920\thttp://localhost:14322/v1\tother\n",
                std::string{kLocalEndpointsHeader} + "selected=" + engCoder.base_url + '\n',
                encodedEndpoints.substr(0u, encodedEndpoints.size() - 1u),
                std::string(16'385u, 'x')})
            if (decode_endpoint_preferences(invalid)) return false;
        if (source_prompt_byte_budget(81'920u, 32'768u) != 122'880u
            || source_prompt_byte_budget(8192u, 8192u) != 0u
            || source_prompt_byte_budget(262'144u, 32'768u) != 663'552u
            || source_prompt_byte_budget(1024u * 1024u, 8192u) != 2u * 1024u * 1024u)
            return false;
        const auto completeBody = openai_chat_request_body("contract-model", {}, "hello", 512u, false, false, false);
        const auto streamedBody = openai_chat_request_body("contract-model", {}, "hello", 512u, false, false, true);
        if (completeBody.find("\"stream\":false") == std::string::npos
            || streamedBody.find("\"stream\":true") == std::string::npos) return false;
        auto tooMany = savedEndpoints;
        for (unsigned port = 14000u; port < 14015u; ++port)
            tooMany.endpoints.push_back("http://127.0.0.1:" + std::to_string(port) + "/v1");
        if (!encode_endpoint_preferences(tooMany).empty()) return false;
        const auto syntheticHeaders = std::vector<std::pair<std::string, std::string>>{
            {"Authorization", "Bearer synthetic-endpoint-token"}};
        if (local_model_headers(engCoder.chat_completions_url, "synthetic-endpoint-token",
                engCoder.chat_completions_url) != syntheticHeaders
            || !local_model_headers(engCoder.chat_completions_url, "synthetic-endpoint-token").empty()
            || !local_model_headers("http://127.0.0.1:14322/v1", "synthetic-endpoint-token",
                engCoder.chat_completions_url).empty()
            || !local_model_headers("http://localhost:14321/v1", "synthetic-endpoint-token",
                engCoder.chat_completions_url).empty()
            || !local_model_headers("http://127.0.0.1:1234/v1", "synthetic-endpoint-token",
                kOriginalLocalEndpoint).empty()) return false;
        // The same codec, endpoint identity and precedence used by production,
        // with no global selection mutation, filesystem I/O or model transport.
        const std::string endpoint{"http://localhost:1234"};
        const LocalModelPreference saved{"qwen/qwen3.8-27b",
            std::string{kOriginalLocalEndpoint}, false};
        const std::string encoded = encode_model_preference(saved);
        const auto decoded = decode_model_preference(encoded);
        const auto restored = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            endpoint, decoded, {}, false);
        const auto configured = resolve_model_selection("operator/current-model",
            LocalModelSelectionOrigin::configured, endpoint, decoded, {}, false);
        const auto explicitSelection = resolve_model_selection("operator/chosen-model",
            LocalModelSelectionOrigin::explicit_selection, endpoint, decoded,
            {"qwen/qwen3.8-27b"}, true);
        const auto ejected = resolve_model_selection(restored.model_id, restored.origin,
            endpoint, decoded, {"another/loaded-model"}, false);
        const auto changedEndpoint = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            "http://localhost:4321", decoded, {}, false);
        const auto remote = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            "https://models.example.test", decoded, {}, false);
        const auto remoteConfigured = resolve_model_selection("operator/remote-model",
            LocalModelSelectionOrigin::configured, "https://models.example.test", decoded, {}, false);
        const auto legacy = decode_model_preference("qwen/qwen3.8-27b\r\n");
        const auto legacyLocal = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            endpoint, legacy, {}, false);
        const auto legacyChanged = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            "http://localhost:4321", legacy, {}, false);
        const auto initialDefault = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            endpoint, {}, {}, false);
        const LocalModelPreference oldQuickDefault{"nvidia/nemotron-3-nano-4b",
            std::string{kOriginalLocalEndpoint}, false};
        const auto migratedQuickDefault = resolve_model_selection({},
            LocalModelSelectionOrigin::none, endpoint, oldQuickDefault, {}, false);
        const auto reported = resolve_model_selection({}, LocalModelSelectionOrigin::none,
            endpoint, decoded, {saved.model_id}, false);
        const std::filesystem::path candidate{"candidate-data"};
        const std::filesystem::path executable{"editor-bin"};
        const std::filesystem::path runtime{"immutable-source"};
#if defined(_WIN32)
        const std::filesystem::path profileWorkingDirectory{"C:/epoch/profile-contract"};
#else
        const std::filesystem::path profileWorkingDirectory{"/epoch/profile-contract"};
#endif
        const auto relativeProfile = project_ai_profile_identity(
            "Assets/AI/project_ai.epochai", profileWorkingDirectory);
        const std::string cachedProfile = relativeProfile.generic_string();
        return !encoded.empty() && decoded.model_id == saved.model_id
            && decoded.endpoint == saved.endpoint && !decoded.legacy
            && restored.model_id == saved.model_id
            && restored.origin == LocalModelSelectionOrigin::remembered
            && restored.reusable_on_request && !restored.confirmed
            && !restored.available_in_inventory
            && configured.model_id == "operator/current-model"
            && configured.origin == LocalModelSelectionOrigin::configured
            && explicitSelection.model_id == "operator/chosen-model"
            && explicitSelection.confirmed && explicitSelection.reusable_on_request
            && model_request_selection_permitted(explicitSelection, true)
            && !model_request_selection_permitted(explicitSelection, false)
            && model_request_selection_permitted(restored, true)
            && !model_request_selection_permitted(restored, false)
            && !model_request_selection_permitted(changedEndpoint, true)
            && !model_request_selection_permitted(remoteConfigured, true)
            && !model_request_selection_permitted({}, true)
            && ejected.model_id == saved.model_id && ejected.reusable_on_request
            && !ejected.available_in_inventory
            && changedEndpoint.model_id == kDefaultLocalModel
            && !changedEndpoint.confirmed && !changedEndpoint.reusable_on_request
            && remote.model_id.empty() && !remote.reusable_on_request
            && remoteConfigured.model_id == "operator/remote-model"
            && !remoteConfigured.reusable_on_request && !remoteConfigured.confirmed
            && legacy.legacy && legacyLocal.model_id == saved.model_id
            && legacyLocal.origin == LocalModelSelectionOrigin::legacy_preference
            && legacyLocal.reusable_on_request && !legacyLocal.confirmed
            && legacyChanged.model_id == kDefaultLocalModel
            && !legacyChanged.reusable_on_request
            && initialDefault.model_id == "qwen/qwen3.8-27b"
            && initialDefault.origin == LocalModelSelectionOrigin::default_local
            && initialDefault.reusable_on_request && !initialDefault.confirmed
            && migratedQuickDefault.model_id == kDefaultLocalModel
            && migratedQuickDefault.origin == LocalModelSelectionOrigin::default_local
            && migratedQuickDefault.reusable_on_request
            && reported.available_in_inventory && !reported.confirmed
            && decode_model_preference("").model_id.empty()
            && decode_model_preference("EPOCH_LOCAL_MODEL_PREFERENCE_V9").model_id.empty()
            && decode_model_preference("first\nsecond").model_id.empty()
            && decode_model_preference(std::string{kLocalModelPreferenceHeader}
                + "endpoint=http://localhost:1234\nmodel=\n").model_id.empty()
            && decode_model_preference(encoded + "model=duplicate\n").model_id.empty()
            && decode_model_preference(std::string{kLocalModelPreferenceHeader}
                + "endpoint=https://remote.example.test\nmodel=remote/model\n").model_id.empty()
            && decode_model_preference(std::string(513u, 'x')).model_id.empty()
            && encode_model_preference({"line\nbreak", saved.endpoint, false}).empty()
            && local_endpoint_identity("HTTP://LOCALHOST:01234/v1/") == kOriginalLocalEndpoint
            && local_endpoint_identity("http://127.0.0.1:1234/v1/models")
                == "http://127.0.0.1:1234/v1/chat/completions"
            && local_endpoint_identity("http://[::1]:1234")
                == "http://[::1]:1234/v1/chat/completions"
            && local_endpoint_identity("http://localhost.evil.test:1234").empty()
            && local_endpoint_identity("http://localhost:1234@evil.test").empty()
            && local_endpoint_identity("http://localhost:0").empty()
            && local_endpoint_identity("http://localhost:65536").empty()
            && local_endpoint_identity("http://localhost:1234/?target=remote").empty()
            && cache_bucket_path(candidate, executable, runtime, "models")
                == candidate / "cache" / "models"
            && cache_bucket_path({}, executable, runtime, "models")
                == executable / "cache" / "models"
            && cache_bucket_path({}, {}, runtime, "models")
                == runtime / "cache" / "models"
            && relativeProfile == profileWorkingDirectory / "Assets/AI/project_ai.epochai"
            && project_ai_profile_identity("Assets/AI/project_ai.epochai",
                profileWorkingDirectory).generic_string() == cachedProfile
            && project_ai_profile_identity(relativeProfile, {}).generic_string() == cachedProfile
            && project_ai_profile_identity("Assets/AI/project_ai.epochai", {}).empty()
            && project_ai_profile_identity({}, profileWorkingDirectory).empty();
    }

    bool model_request_cancellation_contract()
    {
#if EPOCH_EDITION_PUBLIC
        // Untrusted callers can cast a retired numeric workload even though its
        // API and transport implementation are absent from the public source.
        // Reject it before queue admission, model setup, or network dispatch.
        std::vector<ModelRequestStage> publicNotifications{};
        const auto privateWorkload = static_cast<InferenceWorkload>(2u);
        ModelTerminalFailure publicFailure{ModelTerminalFailure::total_timeout};
        if (inference_budget(privateWorkload).valid()
            || send_to_engine_ai("unauthorized workload", privateWorkload, {},
                [&](ModelRequestStage stage) { publicNotifications.push_back(stage); },
                &publicFailure) != "The selected local-model workload has an invalid host budget."
            || publicFailure != ModelTerminalFailure::none
            || publicNotifications != std::vector{ModelRequestStage::failed})
            return false;
#endif
        // Exercise the production admission/cancellation primitives without
        // selecting a model, opening a transport or starting a child process.
        const auto cancelled_waiter = [](bool cancelAll)
        {
            GlobalRequestCancellation global{};
            ModelRequestGate gate{};
            std::stop_source activeStop{};
            std::stop_source queuedStop{};
            RequestCancellation active{activeStop.get_token(), global.capture()};
            RequestCancellation queued{queuedStop.get_token(), global.capture()};
            std::promise<void> entered{};
            auto enteredFuture = entered.get_future();
            std::promise<bool> finished{};
            auto finishedFuture = finished.get_future();
            std::jthread waiter{};
            bool passed{};
            {
                const ScopedModelRequest current{gate, active.token()};
                if (!current.acquired())
                    return false;
                waiter = std::jthread([&]
                {
                    entered.set_value();
                    const ScopedModelRequest pending{gate, queued.token()};
                    finished.set_value(pending.acquired());
                });
                const bool started = enteredFuture.wait_for(std::chrono::seconds{1})
                    == std::future_status::ready;
                const bool blocked = finishedFuture.wait_for(std::chrono::milliseconds{20})
                    == std::future_status::timeout;
                if (cancelAll)
                    global.cancel();
                else
                    (void)queuedStop.request_stop();
                const bool completedWhileCurrentStillOwned =
                    finishedFuture.wait_for(std::chrono::seconds{1}) == std::future_status::ready;
                passed = started && blocked && completedWhileCurrentStillOwned
                    && !finishedFuture.get()
                    && queued.token().stop_requested()
                    && active.token().stop_requested() == cancelAll;
            }
            // Release A even when a test fails before joining B, so a failed
            // cancellation assertion cannot deadlock the contract runner.
            waiter.join();
            const auto fresh = global.capture();
            const ScopedModelRequest next{gate, fresh};
            return passed && !fresh.stop_requested() && next.acquired();
        };
        if (!cancelled_waiter(false) || !cancelled_waiter(true))
            return false;

        std::stop_source stopped{};
        (void)stopped.request_stop();
        ModelRequestGate gate{};
        const ScopedModelRequest denied{gate, stopped.get_token()};
        if (denied.acquired()
            || send_to_engine_ai("cancelled contract request",
                InferenceWorkload::authoring, stopped.get_token())
                != kModelRequestCancelled)
            return false;

        std::stop_source attemptStop{};
        std::uint32_t dispatched{};
        const std::string lateReply = run_model_transport_attempt(attemptStop.get_token(), [&]
        {
            ++dispatched;
            (void)attemptStop.request_stop();
            return std::string{"late successful assistant content"};
        });
        const std::string retry = run_model_transport_attempt(attemptStop.get_token(), [&]
        {
            ++dispatched;
            return std::string{"must not dispatch"};
        });
        if (dispatched != 1u || lateReply != kModelRequestCancelled
            || retry != kModelRequestCancelled)
            return false;

        std::vector<ModelRequestStage> stages{};
        bool wrongThread{};
        const auto callerThread = std::this_thread::get_id();
        const ModelRequestObserver observer = [&](ModelRequestStage stage)
        {
            wrongThread = wrongThread || std::this_thread::get_id() != callerThread;
            stages.push_back(stage);
        };
        ModelTerminalFailure serviceFailure{ModelTerminalFailure::total_timeout};
        if (send_to_engine_ai("cancelled contract request", InferenceWorkload::authoring,
                stopped.get_token(), observer, &serviceFailure) != kModelRequestCancelled
            || serviceFailure != ModelTerminalFailure::cancelled
            || stages != std::vector{ModelRequestStage::cancelled} || wrongThread)
            return false;
        stages.clear();
        EngineAiModel direct{EngineAiModel::Config{.backend = "llama_cpp_cli"}};
        const auto directCancelled = direct.submit("cancelled direct request",
            InferenceWorkload::authoring, stopped.get_token(), observer);
        if (directCancelled.text != kModelRequestCancelled
            || directCancelled.terminal_failure != ModelTerminalFailure::cancelled
            || stages != std::vector{ModelRequestStage::cancelled} || wrongThread)
            return false;
        stages.clear();
        const auto directRejected = direct.submit({}, InferenceWorkload::authoring, {}, observer);
        if (!directRejected.text.empty()
            || directRejected.terminal_failure != ModelTerminalFailure::none
            || stages != std::vector{ModelRequestStage::failed} || wrongThread)
            return false;
        serviceFailure = ModelTerminalFailure::total_timeout;
        if (send_to_engine_ai({}, InferenceWorkload::authoring, {}, {},
                &serviceFailure).empty() || serviceFailure != ModelTerminalFailure::none)
            return false;
        stages.clear();
        {
            ScopedModelObservation completed{observer, {}};
            completed.succeeded = true;
        }
        if (stages != std::vector{ModelRequestStage::completed})
            return false;
        stages.clear();
        {
            ScopedModelObservation uncertain{observer, stopped.get_token()};
            uncertain.succeeded = true;
            uncertain.retirement_failed = true;
        }
        if (stages != std::vector{ModelRequestStage::retirement_failed})
            return false;
        // Observer exceptions never escape into transport/ownership cleanup.
        notify_model_stage([](ModelRequestStage) { throw std::runtime_error("contract observer"); },
            ModelRequestStage::failed);

#if defined(_WIN32)
        // Pure callback decoding: no handles, socket, server or model. Header
        // progress must not pass the send gate, and asynchronous write bytes
        // come from the pointed-to DWORD rather than its four-byte ABI size.
        auto httpState = std::make_shared<AsyncModelHttpState>();
        const auto callbackContext = reinterpret_cast<DWORD_PTR>(httpState.get());
        DWORD headerBytes = 189u;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_REQUEST_SENT, &headerBytes, sizeof(headerBytes));
        if (httpState->completion != 0u || httpState->sentBytes != headerBytes)
            return false;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, nullptr, 0u);
        if (httpState->completion != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)
            return false;
        httpState->prepare_operation();
        DWORD bodyBytes = 1819u;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE, &bodyBytes, sizeof(bodyBytes));
        if (httpState->completion != WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE
            || httpState->bytesWritten != bodyBytes || httpState->error != 0u)
            return false;
        httpState->prepare_operation();
        if (httpState->bytesWritten != 0u || httpState->completion != 0u)
            return false;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE, nullptr, sizeof(DWORD));
        if (httpState->error != ERROR_WINHTTP_INTERNAL_ERROR)
            return false;
        httpState->prepare_operation();
        DWORD availableBytes = 173u;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, &availableBytes, sizeof(availableBytes));
        if (httpState->completion != WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
            || httpState->bytesAvailable != availableBytes) return false;
        httpState->prepare_operation();
        if (httpState->bytesAvailable != 0u) return false;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_READ_COMPLETE, httpState->readBuffer.data(), 37u);
        if (httpState->bytesRead != 37u || httpState->bytesWritten != 0u)
            return false;
        httpState->callbackLifetime = httpState;
        AsyncModelHttpState::callback(nullptr, callbackContext,
            WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING, nullptr, 0u);
        if (!httpState->closed || httpState->callbackLifetime)
            return false;
#endif

        // A real outer retry entry with a pre-cancelled token cannot construct
        // or send even its first HTTP payload. The URL is deliberately invalid.
#if !defined(_WIN32) && defined(EPOCH_HAS_CURL)
        std::string callbackResponse;
        const ModelReplyChunkConsumer rejectedChunk = [](std::string_view)
            { throw std::runtime_error("contract chunk rejection"); };
        bool receivedChunk{};
        const ModelRequestObserver chunkObserver = [&](ModelRequestStage stage)
            { receivedChunk = stage == ModelRequestStage::receiving; };
        CurlModelReply rejectedReply{callbackResponse, rejectedChunk, chunkObserver};
        char chunkByte = 'x';
        if (curl_model_reply_cb(&chunkByte, 1u, 1u, &rejectedReply) != 0u
            || !rejectedReply.failure || !receivedChunk || callbackResponse != "x")
            return false;
        // Exceptions cannot cross CURL's C callback ABI. The owning worker
        // rethrows only after curl_easy_perform returns and RAII retires it.
        bool capturedFailure{};
        try { std::rethrow_exception(rejectedReply.failure); }
        catch (const std::runtime_error&) { capturedFailure = true; }
        if (!capturedFailure || curl_model_reply_cb(nullptr, 0u, 1u, &rejectedReply) != 0u)
            return false;
#endif
        ModelTerminalFailure terminalFailure{ModelTerminalFailure::none};
        return openai_chat_complete("not-a-transport", "contract-model", {},
            "cancelled contract request", {}, 1u, 1u, false, stopped.get_token(),
            terminalFailure) == kModelRequestCancelled
            && terminalFailure == ModelTerminalFailure::cancelled;
    }

