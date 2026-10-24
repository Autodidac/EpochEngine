// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::string ready_text(bool ready)
        {
            return ready ? "Ready" : "Missing";
        }

        [[nodiscard]] std::string_view ai_workspace_domain_name(AiWorkspaceDomain domain) noexcept
        {
            switch (domain)
            {
            case AiWorkspaceDomain::Control:
                return "Engine Development Sandbox";
            case AiWorkspaceDomain::Tooling:
                return "Tool Harness";
            case AiWorkspaceDomain::Engine:
                return "Engine Assistant";
            case AiWorkspaceDomain::Software:
                return "Project Launcher";
            case AiWorkspaceDomain::Evidence:
                return "Evidence";
            case AiWorkspaceDomain::Visualizer:
                return "AI Visualizer";
            case AiWorkspaceDomain::Ops:
                return "Ops / How-To";
            default:
                return "AI";
            }
        }

        [[nodiscard]] std::string ascii_lower_model_name(std::string_view value)
        {
            std::string lower{};
            lower.reserve(value.size());
            for (const unsigned char ch : value)
            {
                lower.push_back(static_cast<char>(
                    ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch));
            }
            return lower;
        }

        [[nodiscard]] int ai_model_suggestion_score(std::string_view modelId)
        {
            const std::string lower = ascii_lower_model_name(modelId);
            const auto contains = [&](std::string_view token) noexcept
            {
                return lower.find(token) != std::string::npos;
            };

            int score = 0;
            // Rank exact advertised capabilities ahead of historical model names.
            // This hint is not authority to switch an operator-selected model.
            if (contains("qwen3.8-flash-next") || contains("qwen3_8_flash_next")
                || contains("qwenflash://")) score += 1'000;
            if (contains("coder")) score += 500;
            if (lower.starts_with("qwen/")) score += 220;
            if (contains("qwen")) score += 160;
            if (contains("glm")) score += 130;
            if (contains("bonsai")) score += 90;
            if (contains("a3b")) score += 20;
            if (contains("embedding")) score -= 1'000;
            if (contains("muse") || contains("glimmer")) score -= 700;
            if (contains("derestricted")) score -= 80;
            return score;
        }

        [[nodiscard]] std::string suggest_discovered_ai_model(
            const std::vector<std::string>& models)
        {
            std::string suggestion{};
            int bestScore = -100'000;
            for (const auto& model : models)
            {
                const int score = ai_model_suggestion_score(model);
                if (suggestion.empty() || score > bestScore)
                {
                    suggestion = model;
                    bestScore = score;
                }
            }
            return suggestion;
        }

        [[nodiscard]] bool model_inventory_contains(
            const std::vector<std::string>& models,
            std::string_view modelId)
        {
            return std::find(models.begin(), models.end(), modelId)
                != models.end();
        }

        void seed_pending_ai_model(
            EditorState& editor,
            const std::vector<std::string>&)
        {
            // Inventory describes availability, not the operator's choice. Ejecting
            // a model (or a failed scan) must not silently select another model.
            if (!editor.aiPendingModelSelection.empty())
                return;
            editor.aiPendingModelSelection = epochengine::ai::active_model_name();
        }

        void request_ai_model_inventory_refresh(EditorState& editor)
        {
            if (editor.aiModelInventoryPending)
                return;

            editor.aiModelInventoryScanned = false;
            editor.aiModelConsentStatus =
                "Scanning the approved local endpoint in the background...";
            try
            {
                editor.aiModelInventoryPending.emplace(std::async(
                    std::launch::async,
                    []
                    {
                        return epochengine::ai::refresh_detected_models();
                    }));
            }
            catch (const std::exception& ex)
            {
                editor.aiModelInventoryScanned = true;
                editor.aiModelConsentStatus =
                    std::string{"Could not start local model discovery: "}
                    + ex.what();
            }
            catch (...)
            {
                editor.aiModelInventoryScanned = true;
                editor.aiModelConsentStatus =
                    "Could not start local model discovery.";
            }
        }

        [[nodiscard]] std::vector<std::string> poll_ai_model_inventory(
            EditorState& editor)
        {
            if (editor.aiModelInventoryPending)
            {
                if (editor.aiModelInventoryPending->wait_for(
                        std::chrono::milliseconds{0})
                    != std::future_status::ready)
                {
                    return {};
                }

                std::vector<std::string> models{};
                try
                {
                    models = editor.aiModelInventoryPending->get();
                }
                catch (const std::exception& ex)
                {
                    editor.aiModelConsentStatus =
                        std::string{"Local model discovery failed: "}
                        + ex.what();
                }
                catch (...)
                {
                    editor.aiModelConsentStatus =
                        "Local model discovery failed.";
                }
                editor.aiModelInventoryPending.reset();
                editor.aiModelInventoryScanned = true;
                seed_pending_ai_model(editor, models);
                if (models.empty())
                {
                    if (!editor.aiModelConsentStatus.starts_with(
                            "Local model discovery failed"))
                    {
                        editor.aiModelConsentStatus =
                            "No model inventory was reported. Your selection is retained; "
                            "check the endpoint if Send or Start cannot connect.";
                    }
                }
                else
                {
                    editor.aiModelConsentStatus =
                        "Found " + std::to_string(models.size())
                        + " model(s) reported. Selection retained; scanning sends no prompt "
                          "and does not load or eject a model.";
                    push_editor_log(
                        editor,
                        "[ai] Local API model inventory is ready.");
                }
                return models;
            }

            if (!editor.aiModelInventoryScanned)
                return {};

            auto models = epochengine::ai::detected_model_names();
            seed_pending_ai_model(editor, models);
            return models;
        }
        [[nodiscard]] bool confirm_pending_ai_model(EditorState& editor)
        {
            std::scoped_lock selectionLock(aiModelSelectionMutex);
            if (aiModelSelectionUsers != 0u)
            {
                editor.aiModelConsentStatus =
                    "A request in an editor context is still queued, working, or stopping. "
                    "Wait for it to finish before changing the model.";
                return false;
            }
            if (editor.aiPendingModelSelection.empty())
            {
                editor.aiModelConsentStatus =
                    "Choose a model before using this endpoint.";
                return false;
            }

            if (!epochengine::ai::select_active_model(
                    editor.aiPendingModelSelection))
            {
                editor.aiModelConsentStatus =
                    epochengine::ai::model_detection_status();
                push_editor_log(
                    editor,
                    "[ai] Local model confirmation failed: "
                        + editor.aiPendingModelSelection);
                return false;
            }

            editor.aiLocalModelConfirmedAt = std::chrono::steady_clock::now();
            editor.aiModelConsentStatus =
                "Approved local model: " + editor.aiPendingModelSelection;
            push_editor_log(
                editor,
                "[ai] Operator approved local model: "
                    + editor.aiPendingModelSelection);
            if (editor.aiDeferredRequestKind != AiDeferredRequestKind::None)
            {
                editor.aiDeferredModelLease = std::make_shared<AiModelSelectionLease>();
            }
            return true;
        }

        [[nodiscard]] bool stage_ai_model_consent(
            EditorState& editor,
            AiDeferredRequestKind requestKind,
            std::string prompt,
            std::string display)
        {
            auto models = poll_ai_model_inventory(editor);
            if (!editor.aiModelInventoryScanned
                && !editor.aiModelInventoryPending)
            {
                request_ai_model_inventory_refresh(editor);
            }
            editor.aiDeferredPrompt = std::move(prompt);
            editor.aiDeferredDisplay = std::move(display);
            editor.aiDeferredRequestKind = requestKind;
            editor.aiDeferredDispatchQueued = false;
            // A staged request must surface model confirmation instead of
            // silently parking the deferred dispatch forever.
            editor.showAiModelConsentModal = false;
            editor.aiSetupVisible = true;
            if(editor.aiMethodSelection==1)editor.aiAutomaticModelSelection=true;
            editor.openMenu = TopMenu::None;

            if (editor.aiPendingModelSelection.empty())
            {
                editor.aiModelConsentStatus =
                    "No model is ready to confirm. Start the local endpoint, "
                    "then use Rescan.";
            }
            else
            {
                editor.aiModelConsentStatus =
                    "Confirm " + editor.aiPendingModelSelection
                    + " before Epoch sends this request.";
            }
            return true;
        }

        // A successful native model-set load selects a foundation inside the
        // runtime. The editor also needs the matching *transport*: otherwise
        // loaded weights coexist with aiMethodSelection == -1 and queued
        // messages never dispatch. Keep this separate from selecting a role;
        // preserve queued tickets that did not have a transport yet.
        [[nodiscard]] bool select_native_chat_transport(EditorState& editor)
        {
            editor.aiMethodSelection = 0;
            editor.aiAutomaticModelSelection = false;
            return true;
        }

        // EpochGui owns the presentation. Assistant runs in-process via its
        // native C++ runtime; Epoch owns all presentation and process lifecycle.
        std::string nativeModelId{};
        // Reusable role editor: role assignments do not select/replace the
        // active request transport. This remains available under Local API,
        // Direct GGUF, MCP and Codex without changing their connections.
        void render_native_ai_role_controls(float width)
        {
            namespace native = epochengine::ai::native_runtime;
            const auto state = native::snapshot();
            const bool editable = !state.busy && aiModelSelectionUsers == 0u;
            // snapshot(false) intentionally omits inventory metadata. The role
            // editor needs snapshot(true) regardless of the current transport.
            if (gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Refresh available role models"}}, 28.0f, 6.0f, gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent, .alignment=gui::InlineButtonAlignment::Center, .wrap=true, .available_width=width}))
                (void)native::refresh();
            if (state.roles.empty())
                gui::wrapped_label("No local role catalog has been discovered. Refresh available role models first.", width);

                gui::wrapped_label("Foundation uses your selected provider/model. Controller and specialist roles can select loaded native models independently; non-resident external models retain provider-managed preparation. The resident list in Model Manager shows actual native processes.",width);
                for (const auto& role:state.roles) if (role.role=="assignment" && role.id!="foundation") {
                    const bool implementation = role.id == "implementation";
                    std::vector<std::string> names{implementation
                        ? "Use selected loaded model" : "Automatic"};
                    std::vector<std::string> uris{std::string{}};
                    for (const auto& model:state.models) {
                        // Source implementation choices list actual resident
                        // processes, not downloaded or merely registered weights.
                        if (implementation && !model.active) continue;
                        names.push_back(model.label.empty()?model.id:model.label);
                        uris.push_back(model.uri);
                    }
                    if (!implementation)
                        for (const auto& model:state.roles)
                            if (model.role!="assignment" && model.active && model.trainable) {
                                names.push_back(model.label);
                                uris.push_back(model.uri);
                            }
                    std::vector<std::string_view> views;
                    for (const auto& name:names) views.emplace_back(name);
                    std::string value=names.front();
                    for (std::size_t n=1;n<uris.size();++n)
                        if (uris[n]==role.uri) value=names[n];
                    gui::label(role.id=="controller"?"System 1 controller":role.id);
                    if (implementation)
                        gui::wrapped_label("Choose a loaded model for source coding, including Flash. "
                            "Automatic uses the currently selected loaded model. "
                            "No model is loaded as a side effect of running a task.", width);
                    const auto choice=gui::select_box({.id=role.id,.selected=value,
                        .options=views,.size={width,28.0f},.max_visible_options=6u,.enabled=editable});
                    if(choice.changed && choice.selected_index)
                        (void)native::assign_role(role.id,uris[*choice.selected_index]);
                }
        }

        void render_native_ai_controls(EditorState& editor, float width)
        {
            namespace native = epochengine::ai::native_runtime;
            const auto state=native::snapshot();
            // Allow switching among already-running models while the native
            // connection is selected. Only an active inference holds the gate.
            const bool editable=!state.busy&&aiModelSelectionUsers==0u;
            gui::wrapped_label("Tool calls, host validation and captured raw model streams are "
                "available in expandable Activity entries in Assistant.", width);
            // Do not initialize the native RuntimeHost just by visiting AI Settings.
            // The explicit Prepare/Retry action below owns expensive model setup.
            gui::wrapped_label(state.phase,width);
            if(!state.inventory_error.empty())gui::wrapped_label("Inventory: "+state.inventory_error,width);
            // Request selection never implicitly starts GGUF model weights.
            // Loading an unresident model remains an explicit Model Manager action.
            std::vector<std::string> names{"Keep current resident model"};
            std::vector<std::string> ids{std::string{}};
            for (const auto& model:state.models) if (model.active) {
                names.push_back(model.label.empty()?model.id:model.label);
                ids.push_back(model.id);
            }
            std::vector<std::string_view> views;
            for (const auto& name:names) views.emplace_back(name);
            std::string selectedName=names.front();
            for (std::size_t n=1;n<ids.size();++n)
                if (ids[n] == state.model) selectedName = names[n];
            gui::wrapped_label("Choose among models already loaded. Load other models explicitly "
                "through Model Manager; selection never downloads or loads weights.", width);
            const auto modelChoice=gui::select_box({.id="epoch-native-model",
                .placeholder="Choose loaded model",.selected=selectedName,
                .options=views,.size={width,30.0f},.max_visible_options=6u,
                .enabled=editable && state.loaded});
            if (modelChoice.changed && modelChoice.selected_index
                && *modelChoice.selected_index > 0u
                && *modelChoice.selected_index < ids.size()) {
                nativeModelId = ids[*modelChoice.selected_index];
                if (native::use_loaded(nativeModelId))
                    ++editor.aiChatUserModelSelectionRevision;
            }
            const std::array actions{
                gui::InlineButtonSpec{.label=state.loaded?"Prepare / Retry":"Prepare model",.width=0.0f,.enabled=editable},
                gui::InlineButtonSpec{.label=state.busy?"Cancel setup":"Unload",.width=0.0f,.enabled=state.busy||state.loaded}};
            if(const auto action=gui::inline_button_row(actions,30.0f,6.0f))
            {
                bool queued=false;
                switch(*action)
                {
                    case 0u: queued=native::prepare(nativeModelId);break;
                    case 1u: if(state.busy){native::cancel_setup();queued=true;}else queued=native::unload();break;
                    default:break;
                }
                if(!queued)push_editor_log(editor,"[Local Models] Request rejected: busy or invalid model ID");
            }
#if EPOCH_EDITION_PUBLIC
            constexpr std::string_view tabs[]{"Models","Roles","Guidance"};
            if (editor.aiSetupTab == 2 || editor.aiSetupTab == 3) editor.aiSetupTab = 0;
#endif
            std::array<gui::InlineButtonSpec,std::size(tabs)> tabButtons{};
            for(std::size_t n=0;n<tabButtons.size();++n)tabButtons[n]={.label=tabs[n],.width=0.0f};
            if(const auto tab=gui::inline_button_row(tabButtons,28.0f,4.0f)) {
                editor.aiSetupTab=static_cast<int>(*tab);
#if EPOCH_EDITION_PUBLIC
                if (editor.aiSetupTab == 2) editor.aiSetupTab = 4;
#endif
            }
            if(editor.aiSetupTab==0){
                for(const auto& model:state.models){
                    gui::property_row(model.label.empty()?model.id:model.label,model.active?"Loaded":model.installed?"Cached":"Download available");
                    if(model.expected_bytes)gui::wrapped_label(epochengine::format_text("{} / {} MiB cached",model.cached_bytes/(1024u*1024u),model.expected_bytes/(1024u*1024u)),width);
                }
                gui::wrapped_label("Browse approved downloadable models or register existing weights. The model catalog uses Epoch's existing verified package installer, not a second unmanaged downloader.",width);
                if (gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Open Model Manager"}}, 30.0f, 6.0f, gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent, .alignment=gui::InlineButtonAlignment::Center, .wrap=true, .available_width=width}))
            {
                editor.showAiModelManagerModal = true;
                editor.aiModelManagerTab = 0;
            }
                gui::wrapped_label("Add existing compatible GGUF weights to the model library. Epoch preserves shared weights and GPU tuning.",width);
                gui::label("Name");(void)gui::edit_box(editor.aiRegisteredModelId,{width,28.0f},256u,false);
                gui::label("Existing model path");(void)gui::edit_box(editor.aiRegisteredModelPath,{width,28.0f},2048u,false);
                const std::array registerAction{gui::InlineButtonSpec{.label="Register existing weights",.width=0.0f,
                    .enabled=editable&&!editor.aiRegisteredModelId.empty()&&!editor.aiRegisteredModelPath.empty()}};
                if(gui::inline_button_row(registerAction,28.0f,4.0f))
                    (void)native::register_model(editor.aiRegisteredModelId,editor.aiRegisteredModelPath);
            }else if(editor.aiSetupTab==1){
                render_native_ai_role_controls(width);
            }
            else{
                gui::wrapped_label("Use one plain-language objective. System 1 selects source owners and keeps verified context; implementation, builds and repair use the same retained task. Ask for project setup, scripts, build or run through Project Assistant. Engine edits stay in the candidate until review.",width);
                gui::property_row("Routing",state.routing);
                gui::wrapped_label("Host-verified activity and individually reviewed tool calls"
                    " are displayed without raw token streams.",width);
            }
        }

        // The compact composer owns the voice buttons. They use the existing
        // consent-aware Epoch voice runtime, never an external microphone shim.
        void activate_ai_voice_control(EditorState& editor, AiChat& chat, std::size_t mode)
        {
            namespace voice = epochengine::ai::voice_runtime;
            const auto owner = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(&editor));
            auto state = voice::snapshot();
            if (!state.probed) {
                voice::probe();
                chat.append_status("VOICE: Checking Windows speech devices in the background. Try the mic again shortly.");
                return;
            }
            if (!state.available || (mode == 1u && !state.speech_available))
            {
                chat.append_status("VOICE: " + state.status + " Dictation requires a working Windows recognizer.");
                return;
            }
            if (!editor.aiVoiceSession.snapshot().capabilities.can_transcribe())
                (void)editor.aiVoiceSession.configure({
                    .microphone_capture=true, .speech_to_text=true,
                    .text_to_speech=state.speech_available, .interruption=true});
            const bool active = state.owner == owner;
            if (active && mode == 1u && !editor.aiVoiceConversation)
            {
                editor.aiVoiceConversation = true;
                editor.aiVoiceSpokenLines = chat.lines.size();
            }
            else if (active)
            {
                voice::stop(owner);
                editor.aiVoiceSession.cancel();
                editor.aiVoiceConversation = false;
            }
            else if (!state.owner)
            {
                editor.aiVoiceConversation = mode == 1u;
                editor.aiVoiceSpokenLines = chat.lines.size();
                const auto result = editor.aiVoiceSession.request_start(
                    editor.aiVoiceConversation
                        ? epochengine::ai::voice::Mode::conversation
                        : epochengine::ai::voice::Mode::dictation);
                if (result == epochengine::ai::voice::Result::consent_required)
                    (void)editor.aiVoiceSession.resolve_microphone_consent(true);
                if (!voice::start(owner)) editor.aiVoiceSession.cancel();
            }
            else chat.append_status("Another Epoch voice session owns the microphone.");
        }

        [[nodiscard]] bool append_ai_chat_attachment(AiChat& chat, const std::filesystem::path& path)
        {
            std::error_code ec;
#if EPOCH_EDITION_PUBLIC
            const bool textAllowed = editor_ai_file_scope::allows_text(path,
                core::path::find_epoch_repo_root(core::path::executable_dir()));
            if (!textAllowed)
            {
                // Clipboard images live in executable-local cache folders, which
                // can be inside a source checkout. Read only a format header first;
                // never load a denied text/source file into an AI attachment.
                std::ifstream header{path, std::ios::binary};
                std::array<char, 24u> signature{};
                header.read(signature.data(), static_cast<std::streamsize>(signature.size()));
                const auto count = static_cast<std::size_t>(header.gcount());
                const std::string_view bytes{signature.data(), count};
                const bool image = (bytes.size() >= 24u && bytes.substr(0, 8) == std::string_view{"\x89PNG\r\n\x1a\n", 8})
                    || (bytes.size() >= 3u && static_cast<unsigned char>(bytes[0]) == 0xffu
                        && static_cast<unsigned char>(bytes[1]) == 0xd8u && static_cast<unsigned char>(bytes[2]) == 0xffu)
                    || (bytes.size() >= 12u && bytes.starts_with("RIFF") && bytes.substr(8, 4) == "WEBP");
                if (!image)
                { chat.append_status("Engine source files cannot be attached in this edition. Generic project files and reference images remain available."); return false; }
            }
#endif
            if (!std::filesystem::is_regular_file(path, ec) || ec)
            { chat.append_status("Attachment is not a readable file."); return false; }
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size > 4u * 1024u * 1024u)
            { chat.append_status("Attachment exceeds the 4 MiB image limit."); return false; }
            std::ifstream file{path, std::ios::binary};
            std::string bytes(static_cast<std::size_t>(size), '\0');
            if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
            { chat.append_status("Could not read the complete attachment."); return false; }
            std::string mime;
            if (bytes.size() >= 24u && bytes.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) == 0) mime = "image/png";
            else if (bytes.size() >= 3u && static_cast<unsigned char>(bytes[0]) == 0xffu
                && static_cast<unsigned char>(bytes[1]) == 0xd8u && static_cast<unsigned char>(bytes[2]) == 0xffu) mime = "image/jpeg";
            else if (bytes.size() >= 12u && bytes.starts_with("RIFF") && bytes.compare(8, 4, "WEBP") == 0) mime = "image/webp";
            if (!mime.empty())
            {
                std::size_t total = bytes.size();
                for (const auto& image : chat.draftReferenceImages) total += image.bytes.size();
                if (chat.draftReferenceImages.size() >= 4u || total > 4u * 1024u * 1024u)
                { chat.append_status("Attach up to four images totaling 4 MiB. Remove an image to add another."); return false; }
                const auto identity = core::sha256::hex(core::sha256::hash(bytes));
                if (!chat.referencePreviews.contains(identity) && chat.referencePreviews.size() < 64u)
                {
                    const auto decoded = platform::filesystem::image_thumbnail(bytes);
                    const auto preview = decoded.rgba.empty() ? SpriteHandle{}
                        : gui::register_runtime_surface("chat.reference." + identity,
                            decoded.rgba, decoded.width, decoded.height);
                    chat.referencePreviews.emplace(identity, gui::ChatImageAttachment{
                        .id = identity, .name = path.filename().string(),
                        .status = preview.is_valid() ? "Attached" : "Preview unavailable",
                        .preview = preview,
                        .source_size = {static_cast<float>(decoded.width), static_cast<float>(decoded.height)}});
                }
                chat.draftReferenceImages.push_back({path.filename().string(), std::move(mime), std::move(bytes), identity});
                chat.append_status("Image attached: " + path.filename().string() + ". Send requires an image-capable model.");
                return true;
            }
#if EPOCH_EDITION_PUBLIC
            if (!textAllowed)
            { chat.append_status("Engine source attachment was refused; no text was admitted to the model."); return false; }
#endif
            if (size > 32768u || bytes.find('\0') != std::string::npos)
            { chat.append_status("Attach PNG/JPEG/WebP images or UTF-8 text files up to 32 KiB."); return false; }
            std::size_t total = chat.input.size() + bytes.size() + path.filename().string().size() + 96u;
            for (const auto& file : chat.draftReferenceFiles) total += file.content.size() + file.name.size() + 96u;
            if (chat.draftReferenceFiles.size() >= 4u || total > 65536u)
            { chat.append_status("Attach up to four text files within the 64 KiB request limit."); return false; }
            chat.draftReferenceFiles.push_back({path.filename().string(), std::move(bytes)});
            return true;
        }

        [[nodiscard]] bool paste_ai_chat_clipboard(AiChat& chat) noexcept
        {
            try
            {
                const auto output = std::filesystem::path{core::path::example_console_workspace_dir()}
                    / "cache" / "ai" / "attachments" / ("clipboard-" + std::to_string(
                        std::chrono::system_clock::now().time_since_epoch().count()) + ".png");
                const auto clipboard = platform::filesystem::read_clipboard_attachment(output);
                if (!clipboard.recognized) return false;
                if (!clipboard.image.empty()) (void)append_ai_chat_attachment(chat, clipboard.image);
                else if (!clipboard.files.empty())
                    for (const auto& file : clipboard.files) (void)append_ai_chat_attachment(chat, file);
                else chat.append_status(clipboard.diagnostic);
                return true;
            }
            catch (...) { chat.append_status("Clipboard attachment could not be imported."); return true; }
        }

        void add_ai_chat_file_to_input(EditorState& editor, AiChat& chat, bool folder, bool imagesOnly = false)
        {
            const std::filesystem::path initial = editor.projectRoot.empty()
                ? std::filesystem::path{} : resolve_editor_path(editor.projectRoot);
            const auto result = platform::filesystem::open_file_dialog(
                folder ? L"Select a file in the folder to attach" : imagesOnly ? L"Attach reference image" : L"Attach files to Assistant",
                imagesOnly ? L"Images" : L"Images, text and source",
                imagesOnly ? L"*.png;*.jpg;*.jpeg;*.webp" : L"*.png;*.jpg;*.jpeg;*.webp;*.txt;*.md;*.cpp;*.hpp;*.h;*.ixx;*.json;*.log;*.cmake;*.yaml;*.yml;*.toml;*.ini;*.py;*.bat;*.ps1",
                initial);
            if (!result.accepted)
            { if (!result.cancelled) chat.append_status(result.diagnostic); return; }
            if (!folder) { (void)append_ai_chat_attachment(chat, result.path); return; }
            std::error_code ec;
            std::filesystem::directory_iterator iter{result.path.parent_path(), ec}, end;
            std::size_t visited{}, attached{};
            for (; !ec && iter != end && visited < 64u && attached < 12u; iter.increment(ec), ++visited)
            {
                const auto ext = iter->path().extension().string();
                if (ext != ".cpp" && ext != ".hpp" && ext != ".h" && ext != ".ixx"
                    && ext != ".txt" && ext != ".md" && ext != ".json" && ext != ".cmake") continue;
                if (append_ai_chat_attachment(chat, iter->path())) ++attached;
            }
            chat.append_status("Attached " + std::to_string(attached) + " small text files from one folder, without recursion.");
        }

        void request_ai_direct_runtime_setup(EditorState& editor)
        {
            if (editor.aiDirectRuntimePending) return;
            editor.aiModelConsentStatus =
                "Locating the direct GGUF runtime on a background worker...";
            try
            {
                auto submission = schedule_editor_cancellable_task(
                    editor, "Direct GGUF discovery and setup",
                    [](std::stop_token stop) -> std::string
                    {
                        if (stop.stop_requested()) return "Direct GGUF setup cancelled.";
                        const auto found = epochengine::ai::discover_direct_runtime();
                        if (stop.stop_requested()) return "Direct GGUF setup cancelled.";
                        if (!found.ready()) return found.message;
                        const bool selected = epochengine::ai::select_direct_runtime(
                            found.executable, found.model);
                        return selected
                            ? "Direct GGUF model confirmed and ready for Epoch requests."
                            : "Direct GGUF model selection was rejected; check the model and executable paths.";
                    });
                editor.aiDirectRuntimePending.emplace(std::move(submission.completion));
                editor.aiDirectRuntimeCancellation = submission.cancellation;
            }
            catch (const std::exception& exception)
            {
                editor.aiModelConsentStatus =
                    std::string{"Direct GGUF setup could not start: "} + exception.what();
            }
            catch (...)
            {
                editor.aiModelConsentStatus = "Direct GGUF setup could not start.";
            }
        }

        void poll_ai_direct_runtime_setup(EditorState& editor)
        {
            if (!editor.aiDirectRuntimePending
                || editor.aiDirectRuntimePending->wait_for(std::chrono::milliseconds{0})
                    != std::future_status::ready)
                return;
            try
            {
                const auto status = editor.aiDirectRuntimePending->get();
                if (editor.aiMethodSelection == 2)
                    editor.aiModelConsentStatus = status;
            }
            catch (const std::exception& exception)
            {
                editor.aiModelConsentStatus =
                    std::string{"Direct GGUF setup failed: "} + exception.what();
            }
            catch (...)
            {
                editor.aiModelConsentStatus = "Direct GGUF setup failed.";
            }
            editor.aiDirectRuntimePending.reset();
            editor.aiDirectRuntimeCancellation = {};
        }

        void render_ai_model_picker(
            EditorState& editor,
            float width,
            std::string_view selectBoxId,
            bool expanded = true)
        {
            // Never wait for model probing or preparation on the GUI thread.
            poll_ai_direct_runtime_setup(editor);
            const float contentWidth = (std::max)(180.0f, width);
            // Selection is global, requests are context-owned. Do not retarget a
            // queued worker between accepting its prompt and capturing its client.
            std::scoped_lock selectionLock(aiModelSelectionMutex);
            const auto nativeState=epochengine::ai::native_runtime::snapshot(false);
            if(editor.aiMethodSelection<0){
                if (nativeState.selected && nativeState.loaded)
                    (void)select_native_chat_transport(editor);
                else if(epochengine::ai::is_model_use_confirmed())editor.aiMethodSelection=
                    epochengine::ai::current_local_inference_transport()==epochengine::ai::LocalInferenceTransport::LlamaCppCli?2:1;
            }
            // Connection modes are not System 1 or subagent roles. Keep stored
            // selection indices and transports stable across upgrades.
#if EPOCH_EDITION_PUBLIC
            constexpr std::string_view methods[]{"Local Models", "Local API", "Direct GGUF runtime"};
            if (editor.aiMethodSelection > 2) editor.aiMethodSelection = -1;
#endif
            const auto method=gui::select_box({.id="epoch-ai-method",.placeholder="Connection / execution backend",
                .selected=editor.aiMethodSelection<0?std::string_view{}:methods[editor.aiMethodSelection],
                .options=methods,.size={contentWidth,30.0f},.max_visible_options=5u,
                .enabled=aiModelSelectionUsers==0u&&!nativeState.busy
                    && !editor.aiDirectRuntimePending
                    && editor.aiChatQueuedMethodSelection<0});
            if(method.changed&&method.selected_index){
#if EPOCH_EDITION_PUBLIC
                {
#endif
                    editor.aiMethodSelection=static_cast<int>(*method.selected_index);
                    editor.aiAutomaticModelSelection=editor.aiMethodSelection==1;
                    if(editor.aiMethodSelection==0){
                        // Switching back to a resident native foundation must
                        // not reload weights or park new messages indefinitely.
                        const auto resident = epochengine::ai::native_runtime::snapshot(false);
                        if (resident.loaded && !resident.busy)
                            (void)epochengine::ai::native_runtime::select_for_epoch_requests(true);
                        // Selecting a connection must not start or download
                        // another model. Explicit loading lives in Model Manager.
                    }
                    else{
                        (void)epochengine::ai::native_runtime::select_for_epoch_requests(false);
                        if(editor.aiMethodSelection==1){epochengine::ai::select_openai_compatible_runtime();editor.aiModelInventoryScanned=false;editor.aiPendingModelSelection.clear();request_ai_model_inventory_refresh(editor);}
                        if (editor.aiMethodSelection == 2)
                            request_ai_direct_runtime_setup(editor);
                    }
                }
            }
            gui::label("Model connection - NOT subagent selection");
            gui::wrapped_label("This picker selects how Epoch sends a request. It does not assign System 1, Scout, Reviewer, or Foundation roles. They are assigned in the independent System 1 / Subagents role editor.", contentWidth);
            if(editor.aiMethodSelection<0){gui::wrapped_label("Select the transport your installed model actually uses. No subagent is selected by this menu.",contentWidth);return;}
            constexpr std::string_view transportHelp[]{
                "Local Models: use the built-in Epoch model manager and the local native serving runtime. Choose this for installed Qwen Next Flash weights.",
                "Local API: connect to an OpenAI-compatible server, including LM Studio. The server manages model loading and memory.",
                "Direct GGUF runtime: run compatible GGUF weights using the discovered direct local runtime instead of an API server.",
#if EPOCH_EDITION_PUBLIC
            };
#endif
            if (editor.aiMethodSelection>=0 && editor.aiMethodSelection<5)
                gui::wrapped_label(transportHelp[static_cast<std::size_t>(editor.aiMethodSelection)], contentWidth);
            if(editor.aiMethodSelection==1){
                const auto discovered=poll_ai_model_inventory(editor);
                if(editor.aiAutomaticModelSelection&&!editor.aiModelInventoryPending&&!discovered.empty()){
                    if(editor.aiPendingModelSelection.empty())editor.aiPendingModelSelection=suggest_discovered_ai_model(discovered);
                    if(confirm_pending_ai_model(editor))editor.aiAutomaticModelSelection=false;
                }
            }
            if(!expanded){
                gui::wrapped_label(editor.aiMethodSelection==0?nativeState.phase:
                    editor.aiMethodSelection==3?"External MCP · engine task mode":epochengine::ai::model_detection_status(),contentWidth);
                return;
            }
            if(editor.aiMethodSelection==0){render_native_ai_controls(editor,contentWidth);return;}
            if (aiModelSelectionUsers != 0u
                || (editor.aiChatQueuedMethodSelection >= 0
                    && !editor.aiChatQueuedModel.empty()))
            {
                gui::property_row("Model", epochengine::ai::active_model_name());
                gui::wrapped_label("A model request is queued, working, or stopping. "
                    "Model switching is available when it finishes; Cancel remains in the session controls.",
                    contentWidth);
                return;
            }
            auto transport =
                epochengine::ai::current_local_inference_transport();

            gui::property_row(
                "[ai] Runtime",
                std::string(
                    epochengine::ai::local_inference_transport_name(transport)));

            if (editor.aiMethodSelection==2)
            {
                auto directStatus = epochengine::ai::direct_runtime_status();
                const auto epochLocalAi =
                    epochengine::ai::epoch_local_ai_install_status();
                gui::property_row("[ai] Status", directStatus.message);
                gui::property_row(
                    "[ai] Executable",
                    directStatus.executable.empty()
                        ? std::string("not found")
                        : directStatus.executable);
                gui::property_row(
                    "[ai] GGUF model",
                    directStatus.model.empty()
                        ? std::string("not found")
                        : directStatus.model);

                if(gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Retry discovery"}}, 30.0f, 6.0f, gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent, .alignment=gui::InlineButtonAlignment::Center, .wrap=true, .available_width=contentWidth})) {
                    directStatus=epochengine::ai::discover_direct_runtime();
                    if(directStatus.ready())(void)epochengine::ai::select_direct_runtime(directStatus.executable,directStatus.model);
                    editor.aiModelConsentStatus=directStatus.message;
                }
                if (gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Configure Direct GGUF in Model Manager"}}, 30.0f, 6.0f, gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent, .alignment=gui::InlineButtonAlignment::Center, .wrap=true, .available_width=contentWidth}))
                {
                    editor.showAiModelManagerModal = true;
                    editor.aiModelManagerTab = 2;
                    editor.aiEndpointDraft = epochengine::ai::active_local_api_endpoint().base_url;
                }

                gui::wrapped_label(
                    "Direct mode invokes an operator-selected llama-cli child "
                    "process. Epoch never starts a hidden server or listener.",
                    contentWidth);
                return;
            }

            auto models = poll_ai_model_inventory(editor);
            if(editor.aiAutomaticModelSelection&&!editor.aiModelInventoryPending&&!models.empty()){
                if(editor.aiPendingModelSelection.empty())editor.aiPendingModelSelection=suggest_discovered_ai_model(models);
                if(confirm_pending_ai_model(editor))editor.aiAutomaticModelSelection=false;
            }
            if(!editor.aiAdvancedSetup){
                gui::wrapped_label(editor.aiModelInventoryPending?"Discovering models at the selected local endpoint...":epochengine::ai::model_detection_status(),contentWidth);
                std::vector<std::string_view> names;for(const auto& modelName:models)names.emplace_back(modelName);
                const auto choice=gui::select_box({.id=selectBoxId,.placeholder="Model",.selected=epochengine::ai::active_model_name(),.options=names,.size={contentWidth,30.0f},.enabled=!editor.aiModelInventoryPending});
                if(choice.changed&&choice.selected_index){
                    editor.aiPendingModelSelection=models[*choice.selected_index];
                    ++editor.aiChatUserModelSelectionRevision;
                    (void)confirm_pending_ai_model(editor);
                }
                const std::array setup{gui::InlineButtonSpec{.label="Connection settings",.width=0.0f},gui::InlineButtonSpec{.label="Refresh models",.width=0.0f,.enabled=!editor.aiModelInventoryPending}};
                if(const auto action=gui::inline_button_row(setup,28.0f,4.0f)){if(*action==0u)editor.aiAdvancedSetup=true;else request_ai_model_inventory_refresh(editor);}
                return;
            }
            if(gui::inline_button_row(std::array{gui::InlineButtonSpec{.label="Hide connection settings"}}, 28.0f, 6.0f, gui::InlineButtonRowOptions{.sizing=gui::InlineButtonSizing::FitContent, .alignment=gui::InlineButtonAlignment::Center, .wrap=true, .available_width=contentWidth}))editor.aiAdvancedSetup=false;
            const auto endpoints = epochengine::ai::saved_local_api_endpoints();
            const auto routes = epochengine::ai::active_local_api_endpoint();
            const bool endpointEditable = !editor.aiModelInventoryPending;
            const auto useEndpoint = [&](std::string_view endpoint)
            {
                if (!epochengine::ai::select_local_api_endpoint(endpoint))
                {
                    editor.aiEndpointStatus = epochengine::ai::model_detection_status();
                    return false;
                }
                editor.aiEndpointDraft = epochengine::ai::active_local_api_endpoint().base_url;
                editor.aiEndpointStatus = "Endpoint selected. Epoch is discovering its models.";
                editor.aiAutomaticModelSelection=true;
                editor.aiPendingModelSelection.clear();
                editor.aiModelInventoryScanned = false;
                request_ai_model_inventory_refresh(editor);
                return true;
            };
            gui::label("Local API endpoints");
            gui::wrapped_label("Native Qwen Flash Next runs inside Assistant. "
                "Local API connects to your existing host, such as LM Studio at localhost:1234. "
                "Choose Local Models in the method picker to use the embedded agent backend.", contentWidth);
            if (endpointEditable)
            {
                std::vector<std::string_view> endpointViews;
                for (const auto& endpoint : endpoints) endpointViews.emplace_back(endpoint);
                const std::string endpointPickerId = std::string{selectBoxId} + "_endpoint";
                const auto chosen = gui::select_box(gui::SelectBoxOptions{
                    .id = endpointPickerId,
                    .placeholder = "Choose endpoint",
                    .selected = routes.base_url,
                    .options = endpointViews,
                    .size = {contentWidth, 30.0f},
                    .row_height = 30.0f,
                    .max_visible_options = 5});
                if (chosen.changed && chosen.selected_index
                    && *chosen.selected_index < endpoints.size()
                    && useEndpoint(endpoints[*chosen.selected_index])) return;
            }
            else gui::wrapped_label("Finish the model scan before changing endpoints.", contentWidth);
            gui::label("New endpoint URL");
            (void)gui::edit_box(editor.aiEndpointDraft, {contentWidth, 30.0f}, 512u, false);
            const std::array endpointActions{
                gui::InlineButtonSpec{.label = "Paste Endpoint", .width = 152.0f,
                    .enabled = endpointEditable},
                gui::InlineButtonSpec{.label = "Save and Use Endpoint", .width = 184.0f,
                    .enabled = endpointEditable && !editor.aiEndpointDraft.empty()}};
            if (const auto action = gui::inline_button_row(endpointActions, 30.0f, 8.0f))
            {
                if (*action == 0u) editor.aiEndpointDraft = gui::clipboard_text().substr(0u, 512u);
                else if (useEndpoint(editor.aiEndpointDraft)) return;
            }
            if (!editor.aiEndpointStatus.empty()) gui::wrapped_label(editor.aiEndpointStatus, contentWidth);
            gui::property_row("Reply delivery", routes.stream_replies ? "Live activity (streaming)" : "Wait for complete reply");
            gui::wrapped_label("Streaming shows arriving activity; complete-reply mode waits silently. "
                "Both wait for a completed, validated plan before offering Apply. "
                "This setting does not change the model's reasoning or make it faster.", contentWidth);
            gui::property_row("Request deadlines", "Chat 3 min / Project plans 15 min / Engine coding 3 h");
            const std::array endpointOptions{
                gui::InlineButtonSpec{.label = routes.stream_replies ? "Wait for Complete Reply" : "Show Live Reply Activity",
                    .width = 222.0f, .enabled = endpointEditable},
                gui::InlineButtonSpec{.label = editor.showAiEndpointRoutes ? "Hide API Routes" : "Show API Routes",
                    .width = 144.0f}};
            if (const auto action = gui::inline_button_row(endpointOptions, 30.0f, 8.0f))
            {
                if (*action == 1u) editor.showAiEndpointRoutes = !editor.showAiEndpointRoutes;
                else if (!epochengine::ai::set_local_api_streaming(!routes.stream_replies))
                    editor.aiEndpointStatus = epochengine::ai::model_detection_status();
                else return;
            }
            if (editor.showAiEndpointRoutes && !routes.base_url.empty())
            {
                gui::property_row("Chat (used)", routes.chat_completions_url);
                gui::property_row("Responses", routes.responses_url);
                gui::property_row("Agent tasks", routes.agent_tasks_url);
                const std::array routeActions{
                    gui::InlineButtonSpec{.label = "Copy Chat URL", .width = 132.0f},
                    gui::InlineButtonSpec{.label = "Copy Responses URL", .width = 170.0f},
                    gui::InlineButtonSpec{.label = "Copy Agent URL", .width = 140.0f}};
                if (const auto action = gui::inline_button_row(routeActions, 30.0f, 8.0f))
                    (void)gui::set_clipboard_text(*action == 0u ? routes.chat_completions_url
                        : *action == 1u ? routes.responses_url : routes.agent_tasks_url);
                gui::wrapped_label("Epoch requests use Chat Completions. Responses and the full "
                    "Assistant agent task API are separate protocols; these URLs are references, "
                    "not agent execution controls. Endpoint settings are kept in this instance's "
                    "private model cache; API keys are never saved.", contentWidth);
            }
            if (!editor.aiModelInventoryScanned
                && !editor.aiModelInventoryPending)
            {
                request_ai_model_inventory_refresh(editor);
            }
            const bool inventoryPending =
                editor.aiModelInventoryPending.has_value();
            const auto selection = epochengine::ai::local_model_selection();
            seed_pending_ai_model(editor, models);
            const auto manifest = epochengine::ai::active_model_manifest();
            const std::string activeModel =
                epochengine::ai::active_model_name();
            gui::property_row("[model] Endpoint", manifest.endpoint);
            gui::property_row("API key", epochengine::ai::has_local_model_api_token()
                ? "Configured (hidden)" : "Not configured");
            gui::wrapped_label("Copy the API key for the selected local endpoint, then paste it here. "
                "It is kept only for this Epoch session and cleared when the endpoint changes.", contentWidth);
            const std::array authActions{
                gui::InlineButtonSpec{.label = "Paste API Key", .width = 152.0f,
                    .enabled = !inventoryPending},
                gui::InlineButtonSpec{.label = "Clear API Key", .width = 152.0f,
                    .enabled = !inventoryPending}};
            if (const auto action = gui::inline_button_row(authActions, 30.0f, 8.0f))
            {
                std::string token = *action == 0u ? gui::clipboard_text() : std::string{};
                const bool accepted = (*action != 0u || !token.empty())
                    && epochengine::ai::set_local_model_api_token(token);
                std::fill(token.begin(), token.end(), '\0');
                token.clear();
                if (accepted) request_ai_model_inventory_refresh(editor);
                else editor.aiModelConsentStatus = "API key not accepted. Copy a valid key "
                    "for the selected local endpoint and try again.";
            }
            gui::property_row(
                "[model] Session",
                activeModel.empty()
                    ? std::string("No model configured")
                    : selection.confirmed || selection.reusable_on_request
                        ? std::string("Ready on Send / Start: ") + activeModel
                        : std::string("Confirm endpoint use: ")
                            + activeModel);
            using SelectionOrigin = epochengine::ai::LocalModelSelectionOrigin;
            gui::property_row("[model] Selection",
                selection.origin == SelectionOrigin::default_local ? "Default Qwen3.8 27B"
                : selection.origin == SelectionOrigin::remembered ? "Last used at this endpoint"
                : selection.origin == SelectionOrigin::explicit_selection ? "Selected by you"
                : selection.origin == SelectionOrigin::legacy_preference ? "Previous local preference"
                : selection.origin == SelectionOrigin::configured ? "Configured model" : "Not selected");
            gui::wrapped_label(
                "Qwen Flash Next now runs through the Local Models controls above. "
                "The API endpoint below applies only to other, external-compatible "
                "providers such as LM Studio. It is not used for native Qwen Flash.",
                contentWidth);
            gui::property_row(
                "[model] Client",
                epochengine::ai::model_connection_status());
            gui::property_row(
                "[model] Source budget",
                epochengine::format_text(
                    "{}K context / {}K output",
                    manifest.host_context_budget_tokens / 1024u,
                    manifest.host_output_budget_tokens / 1024u));
            gui::property_row(
                "[model] Iteration",
                manifest.source_iteration_budget_available
                    ? "Host budget ready; model evidence still required"
                    : "Unavailable");
            const auto contextOverride = epochengine::ai::local_model_context_override();
            const auto contextIdentity = routes.base_url + '\n' + activeModel;
            if (editor.aiContextCapacityIdentity != contextIdentity)
            {
                editor.aiContextCapacityIdentity = contextIdentity;
                editor.aiContextCapacityDraft = contextOverride != 0u ? std::to_string(contextOverride) : std::string{};
            }
            gui::property_row("Context capacity source", contextOverride != 0u ? "Operator setting (bounded by reported capacity)"
                : manifest.loaded_context_capacity_tokens != 0u ? "Loaded model reported by provider"
                : manifest.maximum_context_capacity_tokens != 0u ? "Provider maximum (check loaded size)" : "Unknown: conservative fallback");
            gui::wrapped_label("Loaded context tokens (optional). Use the actual server setting, "
                "not the model's theoretical maximum. Epoch reserves output/tool space, keeps "
                "complete files when they fit, and uses focused regions for larger files.", contentWidth);
            (void)gui::edit_box(editor.aiContextCapacityDraft, {contentWidth, 30.0f}, 7u, false);
            std::size_t declaredContext{};
            const auto capacityText = std::string_view{editor.aiContextCapacityDraft};
            const auto parsedContext = std::from_chars(capacityText.data(), capacityText.data() + capacityText.size(), declaredContext);
            const bool validContext = !capacityText.empty() && parsedContext.ec == std::errc{}
                && parsedContext.ptr == capacityText.data() + capacityText.size()
                && declaredContext >= 8192u && declaredContext <= 1024u * 1024u;
            const bool contextEditable = !inventoryPending && !activeModel.empty()
                ;
            const std::array contextActions{
                gui::InlineButtonSpec{.label = "Save Context Size", .width = 166.0f, .enabled = contextEditable && validContext},
                gui::InlineButtonSpec{.label = "Use Automatic Size", .width = 172.0f, .enabled = contextEditable && contextOverride != 0u}};
            if (const auto action = gui::inline_button_row(contextActions, 30.0f, 8.0f))
            {
                if (epochengine::ai::set_local_model_context_override(*action == 0u ? declaredContext : 0u))
                {
                    if (*action == 1u) editor.aiContextCapacityDraft.clear();
                    editor.aiEndpointStatus = "Context size saved for this endpoint/model; no request or model reload was started.";
                    return;
                }
                editor.aiEndpointStatus = "Context size could not be saved; existing settings are unchanged.";
            }
            if (!contextEditable) gui::wrapped_label("Finish or stop engine work before changing its context budget.", contentWidth);

            const auto epochLocalAi =
                epochengine::ai::epoch_local_ai_install_status();
            const std::array inventoryActions{
                gui::InlineButtonSpec{
                    .label = inventoryPending
                        ? "Scanning..."
                        : "Rescan Models",
                    .width = 152.0f,
                    .enabled = !inventoryPending},
                gui::InlineButtonSpec{
                    .label = epochLocalAi.ready()
                        ? "Use Local GGUF"
                        : "Configure Direct GGUF",
                    .width = 206.0f}
            };
            if (const auto action =
                    gui::inline_button_row(
                        inventoryActions, 30.0f, 8.0f))
            {
                if (*action == 0u)
                {
                    request_ai_model_inventory_refresh(editor);
                    models.clear();
                    push_editor_log(
                        editor,
                        "[ai] Local API model scan started.");
                }
                else
                {
                    if (epochLocalAi.ready()
                        && epochengine::ai::select_direct_runtime(
                            epochLocalAi.runtime_executable,
                            epochLocalAi.model_file))
                    {
                        editor.aiModelInventoryScanned = true;
                        editor.aiPendingModelSelection.clear();
                        editor.aiModelConsentStatus =
                            "Operator selected installed Epoch-local Qwen3.8.";
                        push_editor_log(
                            editor,
                            "[ai] Epoch-local Qwen3.8 selected for internal inference.");
                    }
                    else
                    {
                        editor.aiModelManagerTab = 2;
                        editor.showAiModelManagerModal = true;
                        editor.aiModelConsentStatus = epochLocalAi.message
                            + " Configure a local inference executable and GGUF weights in Direct GGUF. Project packages do not install the Editor model runtime.";
                        push_editor_log(
                            editor,
                            "[ai] Opened Direct GGUF configuration in Model Manager.");
                    }
                }
            }

            if (models.empty())
            {
                gui::wrapped_label(
                    inventoryPending
                        ? "Scanning the approved local endpoint in the "
                          "background. The editor remains interactive."
                        : activeModel.empty()
                            ? "No local API models are visible. Discovery is "
                              "read-only and sends no prompts."
                            : "The approved model remains selected, but the "
                              "endpoint did not report a current inventory.",
                    contentWidth);
                gui::wrapped_label(
                    inventoryPending
                        ? editor.aiModelConsentStatus
                        : epochengine::ai::model_detection_status(),
                    contentWidth);
            }
            // Saved/ejected models remain selectable even when discovery is empty.
            for (const auto& retained : {activeModel, editor.aiPendingModelSelection})
                if (!retained.empty() && !model_inventory_contains(models, retained))
                    models.push_back(retained);
            std::vector<std::string_view> modelViews{};
            modelViews.reserve(models.size());
            for (const auto& modelId : models)
                modelViews.emplace_back(modelId);

            const std::string selectedCandidate =
                editor.aiPendingModelSelection.empty()
                    ? activeModel
                    : editor.aiPendingModelSelection;
            const auto selectResult =
                gui::select_box(gui::SelectBoxOptions{
                    .id = selectBoxId,
                    .placeholder = "Choose model",
                    .selected = selectedCandidate.empty()
                        ? std::string_view{}
                        : std::string_view{selectedCandidate},
                    .options = modelViews,
                    .size = {contentWidth, 30.0f},
                    .row_height = 30.0f,
                    .max_visible_options = 7});
            if (selectResult.changed
                && selectResult.selected_index
                && *selectResult.selected_index < models.size())
            {
                editor.aiPendingModelSelection =
                    models[*selectResult.selected_index];
                (void)confirm_pending_ai_model(editor);
            }

            const bool switchPending =
                !editor.aiPendingModelSelection.empty()
                && editor.aiPendingModelSelection != activeModel;
            if (activeModel.empty() || switchPending
                || (editor.aiDeferredRequestKind != AiDeferredRequestKind::None
                    && !editor.aiDeferredDispatchQueued)
                || (!selection.confirmed && !selection.reusable_on_request))
            {
                gui::wrapped_label(
                    editor.aiModelConsentStatus,
                    contentWidth);
                const char* actionLabel =
                    !epochengine::ai::is_model_use_confirmed()
                        ? "Confirm Model"
                        : activeModel.empty() ? "Use Model" : "Switch Model";
                if (gui::button(actionLabel, {144.0f, 30.0f}))
                {
                    if (confirm_pending_ai_model(editor)
                        && editor.aiDeferredRequestKind
                            != AiDeferredRequestKind::None)
                    {
                        editor.aiDeferredDispatchQueued = true;
                    }
                }
            }
            else
            {
                gui::wrapped_label(
                    "Send or Start uses this model. Inventory does not prove it is "
                    "loaded; the endpoint handles loading when a request arrives.",
                    contentWidth);
            }
        }

