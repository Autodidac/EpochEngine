// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.showAiModelManagerModal)
        {
            if (!editor.aiModelManagerOpenLogged)
            {
                editor.aiModelManagerOpenLogged = true;
                editor.aiModelManagerAutoRescanPending = true;
#if EPOCH_EDITION_PUBLIC
                push_editor_log(editor, "[ai-models] Model Manager opened; scheduling asynchronous read-only native rescan.");
#endif
            }
            editor.openMenu = TopMenu::None;
            namespace native = epochengine::ai::native_runtime;
            // The first open requests read-only discovery. Never construct a
            // RuntimeHost or block modal painting; defer while a native worker
            // is busy and grey the manual Rescan until scanning is complete.
            if (editor.aiModelManagerAutoRescanPending
                && !native::snapshot(false).busy
                && native::refresh()) {
                editor.aiModelManagerAutoRescanPending = false;
            }
            const gui::Vec2 modelModalSize = fit_modal_size(
                { (std::min)(1000.0f, w * 0.90f), h * 0.90f }, { 500.0f, 480.0f });
            const gui::Vec2 modelModalPos = centered_modal_position(modelModalSize);
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Model Manager",
                .position = modelModalPos,
                .size = modelModalSize,
                .viewport_size = {w, h},
                .dim_background = true});
            // Information and setting fields share the body's left edge.
            // Only action rows center their fitted buttons within this width.
            const float managerWidth = (std::max)(1.0f, modelModalSize.x - 58.0f);
            const float managerControlWidth = (std::min)(managerWidth, 720.0f);
            const float managerContentX = modelModalPos.x + (std::min)(29.0f, modelModalSize.x * 0.5f);
            gui::set_cursor({managerContentX, gui::cursor_position().y});
            const gui::InlineButtonRowOptions managerActionLayout{
                .sizing = gui::InlineButtonSizing::FitContent,
                .alignment = gui::InlineButtonAlignment::Center,
                .wrap = true,
                .available_width = managerWidth};
            gui::wrapped_label(
                "Inference models: discover, prepare, load or select. "
                "Project Package Manager is for shipping project assets, including model files.",
                managerWidth);
            // Local endpoint discovery belongs to the Local API tab. Never
            // start a scanning worker merely by opening Model Manager's native
            // tab: the initial UI frame must be free of native/filesystem work.
            const bool suggestionsVisible = editor.aiModelManagerSection == 0
                && editor.aiModelManagerTab == 1;
            if (suggestionsVisible && !editor.aiSuggestionScanStarted
                && !editor.aiSuggestionScanPending)
            {
#if EPOCH_EDITION_PUBLIC
                push_editor_log(editor, "[ai-models] Model Manager: starting passive API endpoint scan.");
#endif
                editor.aiSuggestionScanStarted = true;
                editor.aiSuggestionScanStatus = "Scanning LM Studio, Bionic-compatible and saved harness endpoints...";
                try
                {
                    // Capture all settings on the editor thread. The async
                    // worker must not initialize filesystem/path caches.
                    auto targets = epochengine::ai::prepare_local_api_model_scan();
                    editor.aiSuggestionScanStatus = "Checking "
                        + std::to_string(targets.size()) + " local endpoints...";
                    editor.aiSuggestionScanPending.emplace(std::async(std::launch::async,
                        [targets = std::move(targets)]() mutable {
                            return epochengine::ai::scan_local_api_model_suggestions(
                                std::move(targets));
                        }));
                }
                catch (const std::exception& ex)
                {
                    editor.aiSuggestionScanStatus = std::string{"Scan unavailable: "} + ex.what();
                }
            }
            if (editor.aiSuggestionScanPending && editor.aiSuggestionScanPending->wait_for(
                    std::chrono::milliseconds{0}) == std::future_status::ready)
            {
                try
                {
                    editor.aiSuggestedModels = editor.aiSuggestionScanPending->get();
#if EPOCH_EDITION_PUBLIC
                    push_editor_log(editor, "[ai-models] Model Manager: passive scan results received.");
#endif
                    editor.aiSelectedSuggestion = 0u;
                    editor.aiSuggestionScanStatus = std::to_string(editor.aiSuggestedModels.size())
                        + " suggestions found. No models registered or loaded.";
                }
                catch (const std::exception& ex)
                {
#if EPOCH_EDITION_PUBLIC
                    push_editor_log(editor, "[ai-models] Model Manager: scan returned a C++ exception.");
#endif
                    editor.aiSuggestionScanStatus = std::string{"Scan failed: "} + ex.what();
                }
                catch (...)
                {
#if EPOCH_EDITION_PUBLIC
                    push_editor_log(editor, "[ai-models] Model Manager: scan returned an unknown exception.");
#endif
                    editor.aiSuggestionScanStatus = "Scan failed: unknown endpoint error.";
                }
                editor.aiSuggestionScanPending.reset();
            }
#if EPOCH_EDITION_PUBLIC
            constexpr std::string_view managerSections[]{
                "Models & Runtimes", "System 1 & Subagents", "Agent Routing"};
            if (editor.aiModelManagerSection >= 3) editor.aiModelManagerSection = 0;
            const auto managerSectionIndex = static_cast<std::size_t>(
                std::clamp(editor.aiModelManagerSection, 0, 2));
#endif
            const auto sectionChoice = gui::select_box(gui::SelectBoxOptions{
                .id = "epoch-model-manager-section",
                .placeholder = "Manage AI system",
                .selected = managerSections[managerSectionIndex],
                .options = managerSections,
                .size = {managerControlWidth, 30.0f},
                .max_visible_options = 5u});
            if (sectionChoice.changed && sectionChoice.selected_index) {
                editor.aiModelManagerSection = static_cast<int>(*sectionChoice.selected_index);
            }
            constexpr std::string_view managerTabs[]{
                "Local Models", "Local API / Harnesses", "Direct GGUF"};
            if (editor.aiModelManagerSection == 0)
            {
            const auto managerTabChoice = gui::select_box(gui::SelectBoxOptions{
                .id = "epoch-model-manager-transport",
                .placeholder = "Choose model runtime",
                .selected = managerTabs[static_cast<std::size_t>(
                    std::clamp(editor.aiModelManagerTab, 0, 2))],
                .options = managerTabs,
                .size = {managerControlWidth, 30.0f},
                .max_visible_options = 3u});
            if (managerTabChoice.changed && managerTabChoice.selected_index)
                editor.aiModelManagerTab = static_cast<int>(*managerTabChoice.selected_index);
            }
            const auto nativeState = native::snapshot();
            const bool modelRequestActive = nativeState.busy
                || aiModelSelectionUsers != 0u;
            // An unsent composer ticket is waiting FOR model setup/consent.
            // It owns no inference lease and must not disable that setup.
            // Actual provider/model changes invalidate its selection revision
            // and restore the draft through the existing queue guard.
            const bool composerWaiting = editor.aiChatQueuedMethodSelection >= 0;
            if (nativeState.busy)
            {
                gui::semantic_block("MODEL BUSY / LOADING - UI responsiveness may briefly drop",
                    gui::SemanticTone::warning, managerWidth);
                gui::wrapped_label(nativeState.phase, managerWidth);
                gui::wrapped_label("Loading and VRAM placement are asynchronous. A saved GPU slider change "
                    "takes effect only after unload/reload; keep enough GPU memory for the desktop.", managerWidth);
            }
            if (modelRequestActive)
                gui::wrapped_label(
                    "A model setup or agent request is still active. "
                    "Switching runtimes is disabled until it finishes or is cancelled.", managerWidth);
            else if (composerWaiting)
                gui::wrapped_label("Your message has not been sent. Confirm the selected model here, "
                    "or change the connection to return it to the draft.", managerWidth);
            // The header above is variable-height (font scaling, busy state,
            // and localization). Reserve a real footer, then measure the body
            // from the current cursor instead of guessing a 450px viewport and
            // a 920px content area. No controls may paint over the footer.
            const std::array managerFooterActions{
                gui::InlineButtonSpec{.label="Cancel Loading", .width=0.0f,
                    .enabled=nativeState.busy},
                gui::InlineButtonSpec{.label="Close Model Manager", .width=0.0f}};
            const auto visibleManagerFooterActions = std::span{managerFooterActions}.subspan(
                nativeState.busy ? 0u : 1u);
            const gui::InlineButtonRowOptions managerFooterLayout{
                .sizing = gui::InlineButtonSizing::FitContent,
                .alignment = gui::InlineButtonAlignment::End,
                .wrap = true,
                .available_width = managerWidth};
            const float managerFooterHeight = gui::inline_button_row_height(
                visibleManagerFooterActions, 30.0f, 6.0f, managerFooterLayout);
            const float managerFooterY = modelModalPos.y + modelModalSize.y
                - 24.0f - managerFooterHeight;
            const float managerBodyHeight = (std::max)(1.0f,
                managerFooterY - gui::cursor_position().y - 13.0f);
            const gui::Vec2 managerBodyOrigin = gui::cursor_position();
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = "epoch-model-manager-body",
                .size = {managerWidth, managerBodyHeight},
                .content_height = 0.0f, // previous frame's measured content height
                .draw_background = true,
                .show_scrollbar = true});
            if (editor.aiModelManagerSection == 1)
            {
                gui::section_heading("System 1 / Subagent Role Assignments");
                render_native_ai_role_controls(managerControlWidth);
            }
            else if (editor.aiModelManagerSection == 2)
            {
                gui::section_heading("Routing and Provider Policy");
                gui::wrapped_label("Role assignments are independent of connection types. Native/local models are preferred for implementation; optional Codex review requires permission.", managerWidth);
                render_ai_model_picker(editor, managerControlWidth, "manager-routing-method", false);
                if (editor.aiMethodSelection == 1)
                    (void)gui::toggle_switch("Automatically select discovered API model",
                        editor.aiAutomaticModelSelection, {managerControlWidth, 28.0f});
            }
            else if (editor.aiModelManagerTab == 0)
            {
                gui::property_row("Model status", nativeState.phase, 150.0f);
                const auto residentCount=std::ranges::count_if(nativeState.models,
                    [](const auto& entry) { return entry.active; });
                gui::property_row("Resident models",
                    epochengine::format_text("{} / 3", residentCount), 150.0f);
                gui::property_row("Foundation model", nativeState.loaded
                    ? nativeState.model : std::string("None"), 150.0f);
                gui::property_row("Epoch selected", nativeState.selected
                    ? "Local Models" : "Not active for new requests", 150.0f);
                if (!nativeState.inventory_error.empty())
                    gui::wrapped_label(nativeState.inventory_error, managerWidth);
                // Model Manager is read-only until the operator loads a model.
                // Rescan reads saved manifests without constructing RuntimeHost,
                // launching model workers or attempting Qwen backend adoption.
                if (nativeState.models.empty())
                    gui::wrapped_label(
                        nativeState.busy || editor.aiModelManagerAutoRescanPending
                            ? "Scanning registered model profiles; no model workers are started."
                            : "No registered model profiles found. Select Rescan to retry.",
                        managerWidth);
                if (nativeState.inventory_preview)
                    gui::wrapped_label(
                        "Read-only profile inventory: cache/installation status is unverified until a native runtime is explicitly loaded.",
                        managerWidth);
                // Alphabetic names before numeric-leading names in the GUI.
                // Loading priority is independent: smallest resident weight first.
                std::vector<std::size_t> modelDisplayOrder(nativeState.models.size());
                for (std::size_t i = 0; i < modelDisplayOrder.size(); ++i)
                    modelDisplayOrder[i] = i;
                std::stable_sort(modelDisplayOrder.begin(), modelDisplayOrder.end(),
                    [&](std::size_t left, std::size_t right) {
                        auto label = [&](std::size_t index) -> std::string_view {
                            const auto& item = nativeState.models[index];
                            return item.label.empty() ? std::string_view{item.id}
                                : std::string_view{item.label};
                        };
                        const auto lhs = label(left), rhs = label(right);
                        const auto alpha = [](std::string_view value) {
                            return !value.empty() && std::isalpha(
                                static_cast<unsigned char>(value.front())) != 0;
                        };
                        if (alpha(lhs) != alpha(rhs)) return alpha(lhs);
                        return std::lexicographical_compare(lhs.begin(), lhs.end(),
                            rhs.begin(), rhs.end(), [](unsigned char a, unsigned char b) {
                                return std::tolower(a) < std::tolower(b);
                            });
                    });
                std::vector<std::string> labels{};
                labels.reserve(modelDisplayOrder.size());
                for (const auto index : modelDisplayOrder) {
                    const auto& model = nativeState.models[index];
                    labels.push_back(model.label.empty() || model.label == model.id
                        ? model.id : model.label + " [" + model.id + "]");
                }
                std::vector<std::string_view> labelViews{};
                for (const auto& label : labels) labelViews.push_back(label);
                std::string selectedLabel{};
                for (std::size_t i = 0; i < nativeState.models.size(); ++i)
                    if (nativeState.models[i].id == (nativeModelId.empty()
                            ? nativeState.model : nativeModelId)) {
                        const auto it = std::find(modelDisplayOrder.begin(),
                            modelDisplayOrder.end(), i);
                        if (it != modelDisplayOrder.end())
                            selectedLabel = labels[static_cast<std::size_t>(
                                std::distance(modelDisplayOrder.begin(), it))];
                    }
                gui::wrapped_label("GPU placement is saved per model. Automatic fit adapts to available VRAM; Exact layers keeps your requested count. "
                    "Hardware and resident-model budgets may reduce the loaded count. Reload to apply. External runtimes manage their own offload.",managerWidth);
                gui::label("Optional model test prompt (96 output tokens maximum)");
                (void)gui::edit_box(editor.aiNativeTestPrompt,{managerWidth,58.0f},512u,true);
                gui::label("Choose installed or available GGUF model");
                const auto nativeChoice = gui::select_box(gui::SelectBoxOptions{
                    .id = "epoch-model-manager-native-select",
                    .placeholder = "Choose native model",
                    .selected = selectedLabel,
                    .options = labelViews,
                    .size = {managerControlWidth, 30.0f},
                    .max_visible_options = 7u,
                    .enabled = !nativeState.busy && !modelRequestActive});
                if (nativeChoice.changed && nativeChoice.selected_index
                    && *nativeChoice.selected_index < nativeState.models.size())
                {
                    nativeModelId = nativeState.models[modelDisplayOrder[*nativeChoice.selected_index]].id;
                    ++editor.aiChatUserModelSelectionRevision;
                    editor.aiModelPendingUnregister.clear();
                }
                for (const auto& model : nativeState.models)
                {
                    const std::string label = model.label.empty() ? model.id : model.label;
                    const std::string status = model.active ? "Loaded"
                        : nativeState.inventory_preview ? "Registered (cache unchecked)"
                        : model.installed ? "Cached" : "Available to install";
                    gui::property_row(label, status, 230.0f);
                    // Keep inventory readable. Detailed tuning and timing belong
                    // to the selected model, not repeated for every download.
                    if (model.id != (nativeModelId.empty() ? nativeState.model : nativeModelId))
                        continue;
                    if (model.active)
                        gui::property_row("  Live context",
                            model.loaded_context_tokens != 0u
                                ? std::to_string(model.loaded_context_tokens)
                                    + " tokens (runtime-reported)"
                                : "Unknown (runtime has not reported it)", 150.0f);
                    auto [draftIt, inserted] = editor.aiNativeOffloadDraft.try_emplace(
                        model.id, model.gpu_offload_percent);
                    (void)inserted;
                    const bool automaticPlacement = model.tuning.gpu_layers == "auto";
                    if (automaticPlacement)
                    {
                    const std::string gpuSliderLabel = "Automatic GPU target: "
                        + std::to_string(draftIt->second) + "%  -  " + label;
                    const auto gpuChoice = gui::slider(gui::SliderOptions{
                        .id="epoch-native-gpu-"+model.id,
                        .label=gpuSliderLabel,
                        .minimum=0.0f,.maximum=100.0f,
                        .value=static_cast<float>(draftIt->second),
                        .step=1.0f,
                        .size={managerControlWidth,64.0f},
                        .enabled=!modelRequestActive});
                    if (gpuChoice.changed)
                        draftIt->second=static_cast<unsigned>(std::clamp(
                            std::lround(gpuChoice.value),0L,100L));
                    gui::property_row("  Saved GPU target",
                        std::to_string(model.gpu_offload_percent) +
                        "% target (reload to apply; hardware/resident budgets may cap)",150.0f);
                    }
                    else
                        gui::property_row("  Saved GPU target", model.tuning.gpu_layers
                            + " requested layers (Exact layers; reload to apply)", 150.0f);
                    if (!model.effective_gpu_layers.empty())
                        gui::property_row("  Profile GPU layers",model.effective_gpu_layers +
                            " requested layers before multi-model VRAM cap",150.0f);
                    // InlineButtonSpec stores string_view: keep these backing
                    // labels alive through inline_button_row (no temporaries).
                    const std::string saveLabel="Save GPU - "+model.id;
                    const std::string testLabel="Test - "+model.id;
                    const std::array perModelActions{
                        gui::InlineButtonSpec{.label=saveLabel,.width=0.0f,
                            .enabled=!modelRequestActive &&
                                draftIt->second!=model.gpu_offload_percent},
                        gui::InlineButtonSpec{.label=testLabel,.width=0.0f,
                            .enabled=!modelRequestActive && model.active &&
                                !editor.aiNativeTestPrompt.empty()}};
                    const auto visibleModelActions = std::span{perModelActions}.subspan(
                        automaticPlacement ? 0u : 1u);
                    if (const auto action=gui::inline_button_row(visibleModelActions, 28.0f, 5.0f, managerActionLayout)) {
                        const bool savingTarget = automaticPlacement && *action == 0u;
                        const bool accepted=savingTarget
                            ? native::set_gpu_offload(model.id,draftIt->second)
                            : native::benchmark_model(model.id,editor.aiNativeTestPrompt);
                        editor.aiModelManagerFeedback=accepted
                            ? savingTarget ? "Saving offload profile. Reload model to apply."
                                : "Testing selected resident model asynchronously; no tools or source edits."
                            : "Model operation rejected: wait for the current operation and retry.";
                    }
                    if (model.requests > 0) {
                        gui::property_row("  Inference", epochengine::format_text(
                            "{} / {} successful; avg {:.0f} ms",
                            model.inference_ok, model.requests, model.avg_request_ms), 145.0f);
                        gui::property_row("  Output rate", model.tps_known
                            ? epochengine::format_text("{:.1f} t/s (reported tokens)", model.reported_tps)
                            : std::string{"t/s unavailable (no token usage)"}, 145.0f);
                        if (model.first_token_known)
                            gui::property_row("  First token", epochengine::format_text(
                                "{:.0f} ms average", model.avg_first_token_ms), 145.0f);
                        if (model.simple_baseline_available)
                            gui::property_row("  Simple latency delta", epochengine::format_text(
                                "{:+.0f} ms per call vs foundation (observed; not A/B)",
                                model.simple_latency_difference_ms), 145.0f);
                        if (model.agent_completed || model.agent_failed)
                            gui::property_row("  Agent outcomes", epochengine::format_text(
                                "{} completed / {} failed (not build-verified)",
                                model.agent_completed, model.agent_failed), 145.0f);
                    }
                    if (model.expected_bytes)
                        gui::property_row("  Downloaded",
                            epochengine::format_text("{} / {} MiB",
                                model.cached_bytes / (1024u * 1024u),
                                model.expected_bytes / (1024u * 1024u)), 145.0f);
                }
                // Detailed controls apply to one selected native model only.
                // The underlying QFR runtime already accepts these arguments;
                // here they are wired to an atomic, validated model-profile save.
                const std::string selectedNativeId = nativeModelId.empty()
                    ? nativeState.model : nativeModelId;
                const auto selectedNativeIt = std::ranges::find_if(nativeState.models,
                    [&](const auto& model) { return model.id == selectedNativeId; });
                if (selectedNativeIt != nativeState.models.end())
                {
                    gui::property_row("Image input", !selectedNativeIt->active ? "Not loaded"
                        : !selectedNativeIt->image_input_known ? "Unconfirmed"
                        : selectedNativeIt->image_input_supported ? "Ready" : "Unavailable in loaded runtime", 150.0f);
                    if (selectedNativeIt->active && !selectedNativeIt->image_input_supported)
                        gui::wrapped_label("Image attachments need a loaded vision projector or a vision-capable Local API connection. "
                            "A Qwen model name alone does not enable images; text coding can continue.", managerWidth);
                    gui::section_heading("Performance settings — selected model");
                    gui::wrapped_label("Total task time includes prompt processing, generation, builds and checks. "
                        "Compare the same prompt and output length; model size alone does not predict t/s.", managerWidth);
                    gui::property_row("Saved policy", selectedNativeIt->tuning.runtime_policy, 150.0f);
                    if (selectedNativeIt->loaded_tuning_known)
                    {
                        const auto& active = selectedNativeIt->loaded_tuning;
                        gui::property_row("Active GPU / CPU", epochengine::format_text(
                            "{} layers / {} decode threads / {} batch threads",
                            selectedNativeIt->loaded_gpu_layers, active.threads, active.batch_threads), 150.0f);
                        gui::property_row("Active KV / batching", epochengine::format_text(
                            "K {} / V {} / {} KV GPU layers / batch {} / micro {}",
                            active.kv_type_k, active.kv_type_v, active.kv_gpu_layers,
                            active.batch_size, active.ubatch_size), 150.0f);
                        if (selectedNativeIt->loaded_gpu_layers != selectedNativeIt->effective_gpu_layers)
                            gui::wrapped_label("Hardware/startup GPU limit: requested "
                                + selectedNativeIt->effective_gpu_layers + " layers, loaded "
                                + selectedNativeIt->loaded_gpu_layers
                                + ". Saved settings are unchanged; inspect VRAM, resident models and context before raising limits.", managerWidth);
                    }
                    else gui::property_row("Active tuning", "Not reported until model is loaded", 150.0f);
                    gui::property_row("Saved context / slots", epochengine::format_text("{} / {} (reload to apply)",
                        selectedNativeIt->tuning.context_tokens, selectedNativeIt->tuning.parallel_slots), 150.0f);
                    const std::array presetActions{
                        gui::InlineButtonSpec{.label="Save hardware defaults", .width=0.0f,
                            .enabled=!modelRequestActive && !nativeState.busy}};
                    if (gui::inline_button_row(presetActions, 30.0f, 6.0f, managerActionLayout))
                    {
                        auto defaults = selectedNativeIt->tuning;
                        defaults.runtime_policy = "automatic";
                        defaults.parallel_slots = 1u;
                        // Preserve the admitted source context and explicit GPU
                        // budget. Hardware policy resolves batching and caches.
                        defaults.speculative_enabled = false;
                        if (native::save_model_tuning(selectedNativeId, defaults))
                        {
                            editor.aiNativeTuningDraft[selectedNativeId] = defaults;
                            editor.aiModelManagerFeedback = "Hardware defaults saved for this model: one request slot, automatic CPU/batching/cache policy, existing context and GPU target retained. Reload when idle to apply.";
                        }
                        else editor.aiModelManagerFeedback = "Could not queue settings save; current model settings remain active.";
                    }
                    const std::string optionsLabel = editor.aiNativeAdvancedTuningOpen
                        ? "Hide runtime options" : "Show runtime options";
                    if (gui::inline_button_row(std::array{
                            gui::InlineButtonSpec{.label=optionsLabel}},
                            30.0f, 6.0f, managerActionLayout))
                        editor.aiNativeAdvancedTuningOpen = !editor.aiNativeAdvancedTuningOpen;
                    if (editor.aiNativeAdvancedTuningOpen) {
                        auto [draftIt, isNew] = editor.aiNativeTuningDraft.try_emplace(
                            selectedNativeId, selectedNativeIt->tuning);
                        (void)isNew;
                        auto& tune = draftIt->second;
                        const bool canEdit = !modelRequestActive && !nativeState.busy;
                        gui::wrapped_label(
                            "Changes save to this model's profile only; unload/reload to apply. "
                            "Recommended keeps the Qwen baseline; Automatic adapts to hardware. Choose Manual to apply "
                            "individual scheduler, cache and KV overrides.", managerWidth);
                        const std::array<std::string_view,3> policies{
                            "recommended", "automatic", "manual"};
                        const auto policy = gui::select_box(gui::SelectBoxOptions{
                            .id="epoch-native-policy-"+selectedNativeId,
                            .selected=tune.runtime_policy, .options=policies,
                            .size={managerControlWidth,30.0f}, .enabled=canEdit});
                        if (policy.changed && policy.selected_index)
                            tune.runtime_policy = std::string{policies[*policy.selected_index]};
                        // uint32 slider helper: controlled draft values, unique
                        // IDs per model/setting, and no integer narrowing before clamp.
                        const auto number = [&](std::string_view id, std::string_view caption,
                            std::uint32_t& value, float lower, float upper, float step) {
                            const std::string label = std::string{caption} + ": "
                                + std::to_string(value);
                            const auto choice = gui::slider(gui::SliderOptions{
                                .id="epoch-tuning-"+selectedNativeId+"-"+std::string{id},
                                .label=label,.minimum=lower,.maximum=upper,
                                .value=static_cast<float>(value),.step=step,
                                .size={managerControlWidth,48.0f},.enabled=canEdit &&
                                    (tune.runtime_policy == "manual" || id == "layers" || id == "gpu-count" || id == "context" || id == "slots" || id == "reserve")});
                            if (choice.changed) value=static_cast<std::uint32_t>(std::clamp(
                                std::lround(choice.value), static_cast<long>(lower),
                                static_cast<long>(upper)));
                        };
                        gui::label("GPU layers / context");
                        const std::array<std::string_view,2> gpuModes{"Automatic fit", "Exact layers"};
                        const auto gpuMode = gui::select_box(gui::SelectBoxOptions{
                            .id="epoch-native-gpu-placement-"+selectedNativeId,
                            .selected=tune.gpu_layers == "auto" ? gpuModes[0] : gpuModes[1],
                            .options=gpuModes,.size={managerControlWidth,30.0f},.enabled=canEdit});
                        if (gpuMode.changed && gpuMode.selected_index)
                            tune.gpu_layers = *gpuMode.selected_index == 0u ? "auto"
                                : std::to_string((std::min)(30u, tune.gpu_layer_count_hint));
                        if (tune.gpu_layers != "auto")
                        {
                            std::uint32_t count = tune.gpu_layer_count_hint;
                            (void)std::from_chars(tune.gpu_layers.data(),
                                tune.gpu_layers.data()+tune.gpu_layers.size(),count);
                            number("gpu-count", "Requested GPU layers",count,0.0f,
                                static_cast<float>(tune.gpu_layer_count_hint),1.0f);
                            tune.gpu_layers = std::to_string(count);
                        }
                        gui::wrapped_label("Automatic fit keeps dedicated GPU memory headroom. "
                            "Exact layers preserves your standalone request; Windows controls any shared-memory spill. "
                            "Multiple-model loads still share a bounded GPU budget.",managerWidth);
                        number("layers", "Model layer count hint",
                            tune.gpu_layer_count_hint, 1.0f, 256.0f, 1.0f);
                        gui::wrapped_label("100% requests the model layer count; automatic mode keeps VRAM headroom. "
                            "The runtime uses the smaller model layer count when needed; "
                            "multi-model VRAM safeguards may reduce it further.",managerWidth);
                        number("context", "Total context tokens",tune.context_tokens,
                            4096.0f,262144.0f,1024.0f);
                        number("slots", "Parallel slots",tune.parallel_slots,1.0f,4.0f,1.0f);
                        gui::property_row("Context per slot",std::to_string(
                            tune.context_tokens/std::max(1u,tune.parallel_slots))+" tokens",170.0f);
                        number("reserve", "Context reserve tokens", tune.context_reserve_tokens,
                            1024.0f,32768.0f,1024.0f);
                        (void)gui::toggle_switch("Automatic context compaction##epoch-native-compact",
                            tune.auto_compact,{managerWidth,28.0f});
                        if (tune.auto_compact)
                        {
                            const auto ratio = [&](std::string_view id, std::string_view caption,
                                double& value, float minimum, float maximum)
                            {
                                const auto choice = gui::slider(gui::SliderOptions{
                                    .id="epoch-tuning-"+selectedNativeId+"-"+std::string{id},
                                    .label=std::string{caption}+epochengine::format_text(": {:.0f}%",value*100.0),
                                    .minimum=minimum,.maximum=maximum,.value=static_cast<float>(value),
                                    .step=0.01f,.size={managerControlWidth,48.0f},.enabled=canEdit});
                                if (choice.changed) value=std::clamp(static_cast<double>(choice.value),
                                    static_cast<double>(minimum),static_cast<double>(maximum));
                            };
                            ratio("compact-trigger","Compact when context reaches",tune.compact_trigger_ratio,
                                0.60f,0.98f);
                            tune.compact_target_ratio=std::min(tune.compact_target_ratio,
                                tune.compact_trigger_ratio-0.05);
                            ratio("compact-target","Retain context after compaction",tune.compact_target_ratio,
                                0.25f,static_cast<float>(tune.compact_trigger_ratio-0.05));
                        }
                        gui::wrapped_label("Automatic compaction preserves the source task and verified edits. "
                            "Larger context increases prompt/KV cost; keep enough for the task. "
                            "Saved settings take effect after an explicit reload; active values above show what is running.", managerWidth);
                        if (tune.runtime_policy == "manual")
                        {
                        gui::label("CPU / batching");
                        number("threads", "Inference CPU threads",tune.threads,1.0f,32.0f,1.0f);
                        number("batch-threads", "Batch CPU threads",tune.batch_threads,1.0f,32.0f,1.0f);
                        number("batch", "Batch tokens",tune.batch_size,32.0f,8192.0f,32.0f);
                        number("ubatch", "Microbatch tokens",tune.ubatch_size,16.0f,2048.0f,16.0f);
                        (void)gui::toggle_switch("Flash attention##epoch-native-fa",
                            tune.flash_attention,{managerWidth,28.0f});
                        gui::label("KV cache / GPU-host placement");
                        const std::array<std::string_view,3> kvOptions{"q4_0", "q8_0", "f16"};
                        const auto kvK=gui::select_box(gui::SelectBoxOptions{
                            .id="epoch-native-kv-k-"+selectedNativeId,
                            .selected=tune.kv_type_k,.options=kvOptions,
                            .size={managerControlWidth,28.0f},.enabled=canEdit});
                        if (kvK.changed && kvK.selected_index)
                            tune.kv_type_k=std::string{kvOptions[*kvK.selected_index]};
                        const auto kvV=gui::select_box(gui::SelectBoxOptions{
                            .id="epoch-native-kv-v-"+selectedNativeId,
                            .selected=tune.kv_type_v,.options=kvOptions,
                            .size={managerControlWidth,28.0f},.enabled=canEdit});
                        if (kvV.changed && kvV.selected_index)
                            tune.kv_type_v=std::string{kvOptions[*kvV.selected_index]};
                        (void)gui::toggle_switch("KV GPU offload##epoch-native-kv",
                            tune.kv_offload,{managerWidth,28.0f});
                        (void)gui::toggle_switch("Pin CPU KV memory##epoch-native-kvpin",
                            tune.kv_cpu_pinned,{managerWidth,28.0f});
                        number("kv-gpu-layers", "KV GPU layers",tune.kv_gpu_layers,
                            0.0f,128.0f,1.0f);
                        gui::label("System RAM / expert and prompt caches");
                        gui::wrapped_label("Windows shared GPU memory is driver-managed "
                            "system RAM. It cannot be increased like dedicated VRAM using "
                            "a llama GPU-layer slider. Host-pinned cache uses system RAM "
                            "and may accelerate supported MoE runtimes.",managerWidth);
                        const std::array<std::string_view,3> expertModes{"auto","off","on"};
                        const auto expertMode=gui::select_box(gui::SelectBoxOptions{
                            .id="epoch-native-expert-mode-"+selectedNativeId,
                            .selected=tune.expert_cache_mode,.options=expertModes,
                            .size={managerControlWidth,28.0f},.enabled=canEdit});
                        if (expertMode.changed && expertMode.selected_index)
                            tune.expert_cache_mode=std::string{expertModes[*expertMode.selected_index]};
                        number("expert-cache", "GPU expert cache MiB",tune.expert_cache_mib,
                            0.0f,16384.0f,256.0f);
                        number("host-cache", "Pinned system RAM MiB",tune.pinned_host_mib,
                            0.0f,32768.0f,256.0f);
                        number("prompt-cache", "Prompt cache RAM MiB",tune.prompt_cache_mib,
                            0.0f,8192.0f,128.0f);
                        gui::label("Optional MTP / speculative decoding");
                        gui::wrapped_label("Requires a dedicated compatible local GGUF "
                            "draft sidecar. No main-model self-draft is allowed.",managerWidth);
                        (void)gui::toggle_switch("Enable MTP##epoch-native-mtp",
                            tune.speculative_enabled,{managerWidth,28.0f});
                        gui::label("Local draft GGUF path");
                        (void)gui::edit_box(tune.speculative_model,
                            {managerWidth,30.0f},1024u,false);
                        number("spec-tokens", "Speculative tokens",tune.speculative_tokens,
                            1.0f,16.0f,1.0f);
                        }
                        const std::array actionButtons{
                            gui::InlineButtonSpec{.label="Save Options",.width=0.0f,
                                .enabled=canEdit},
                            gui::InlineButtonSpec{.label="Discard Draft",.width=0.0f,
                                .enabled=canEdit}};
                        if (const auto action=gui::inline_button_row(actionButtons, 30.0f, 6.0f, managerActionLayout)) {
                            if (*action == 0u) {
                                const bool accepted=native::save_model_tuning(selectedNativeId,tune);
                                editor.aiModelManagerFeedback=accepted
                                    ? "Saving selected model options; reload after save."
                                    : "Model options save not queued: check active operation.";
                            } else {
                                tune=selectedNativeIt->tuning;
                                editor.aiModelManagerFeedback="Unsaved model option edits discarded.";
                            }
                        }
                    }
                }
                if (!nativeState.benchmark.model_id.empty()) {
                    const auto& test=nativeState.benchmark;
                    gui::property_row("Latest model test",test.model_id + ": " + test.status,150.0f);
                    if (test.completed) {
                        gui::property_row("  Total latency",
                            epochengine::format_text("{:.0f} ms",test.elapsed_ms),150.0f);
                        gui::property_row("  First token",test.first_token_known
                            ? epochengine::format_text("{:.0f} ms",test.first_token_ms)
                            : std::string{"Not reported"},150.0f);
                        gui::property_row("  Output speed",test.tps_known
                            ? epochengine::format_text("{:.1f} t/s ({} reported tokens)",
                                test.output_tps,test.output_tokens)
                            : std::string{"t/s unavailable: no token usage"},150.0f);
                        gui::wrapped_label("Reply: " + test.reply,managerWidth);
                    }
                }
                gui::section_heading("Native resident set (up to three)");
                gui::wrapped_label("Save your native models once. Epoch will reuse "
                    "residents, load remaining models smallest-to-largest, and pause "
                    "4 seconds between loads. Previously loaded models stay resident "
                    "if a later load fails.", managerWidth);
                constexpr std::array<std::string_view, 3> loadSlotIds{
                    "native-load-set-small", "native-load-set-middle", "native-load-set-large"};
                constexpr std::array<std::string_view, 3> loadSlotLabels{
                    "Small model", "Middle model", "Large model"};
                for (std::size_t slot = 0; slot < editor.aiNativeLoadSetIds.size(); ++slot)
                {
                    std::vector<std::string_view> options{};
                    options.reserve(labels.size() + 1u);
                    options.emplace_back("(None)");
                    for (const auto& label : labels) options.emplace_back(label);
                    std::string selected = "(None)";
                    for (std::size_t i = 0; i < modelDisplayOrder.size(); ++i)
                        if (nativeState.models[modelDisplayOrder[i]].id == editor.aiNativeLoadSetIds[slot])
                            selected = labels[i];
                    const auto choice = gui::select_box(gui::SelectBoxOptions{
                        .id = loadSlotIds[slot],
                        .placeholder = loadSlotLabels[slot],
                        .selected = selected,
                        .options = options,
                        .size = {managerControlWidth, 28.0f},
                        .max_visible_options = 6u,
                        .enabled = !modelRequestActive});
                    if (choice.changed && choice.selected_index
                        && *choice.selected_index <= modelDisplayOrder.size()) {
                        const auto index = *choice.selected_index;
                        editor.aiNativeLoadSetIds[slot] = index == 0u
                            ? std::string{} : nativeState.models[modelDisplayOrder[index - 1u]].id;
                    }
                }
                const std::array setActions{
                    gui::InlineButtonSpec{.label="Save & Load Set", .width=0.0f,
                        .enabled=!modelRequestActive && !editor.aiModelManagerAutoRescanPending
                            && !nativeState.models.empty()},
                    gui::InlineButtonSpec{.label="Load Saved Set", .width=0.0f,
                        .enabled=!modelRequestActive && !editor.aiModelManagerAutoRescanPending
                            && !nativeState.models.empty()}};
                if (const auto action = gui::inline_button_row(setActions, 30.0f, 6.0f, managerActionLayout))
                {
                    // A deliberate selection always wins over startup defaults.
                    bool accepted = false;
                    if (*action == 0u) {
                        std::vector<std::string> ids{};
                        for (const auto& id : editor.aiNativeLoadSetIds)
                            if (!id.empty()) ids.push_back(id);
                        if (!ids.empty() && std::unordered_set<std::string>(
                                ids.begin(), ids.end()).size() == ids.size()
                            && native::save_load_set(ids))
                            accepted = native::load_set(std::move(ids));
                    }
                    else accepted = native::load_saved_set();
                    if (accepted)
                    {
                        // The batch chooses a native foundation asynchronously;
                        // select the editor transport now, before queued chat
                        // dispatch. This operation is explicitly user initiated.
                        const bool chatSelected = select_native_chat_transport(editor);
                        editor.aiModelManagerFeedback = chatSelected
                            ? "Native chat selected. Loading smallest-to-largest; queued messages dispatch when ready."
                            : "Set loading, but active development work prevented native chat selection. Finish that work, then choose Local Models in Agent Routing.";
                    }
                    else editor.aiModelManagerFeedback = *action == 1u
                        ? "Saved set was not started. " + native::snapshot(false).phase
                        : "Set rejected: select distinct registered models or finish existing work.";
                }
                // Show the actual persisted role assignments, not just the
                // foundation selected in the chat-model picker. The picker
                // identifies the transport; the Architect selects a responder
                // for each question when automatic routing is enabled.
                auto active_route = [&](std::string_view role) {
                    const auto it = std::find_if(nativeState.roles.begin(),
                        nativeState.roles.end(), [&](const auto& item) {
                            return item.role == "assignment" && item.id == role;
                        });
                    return it == nativeState.roles.end() || it->uri.empty()
                        ? std::string{"foundation fallback"} : it->uri;
                };
                gui::wrapped_label("Source generation: implementation -> " +
                    active_route("implementation") + " (otherwise the selected resident model). "
                    "Automatic chat: difficult work uses foundation; bounded coding needs verified task-family wins. Reviewer: " + active_route("reviewer") +
                    " (second/third attempt only, maximum 64 KiB prompt). "
                    "Simple chat: " + active_route("simple"), managerWidth);
                gui::wrapped_label(
                    "Saved resident models restore automatically when Epoch Editor opens. "
                    "New models load in size order; existing residents are reused. "
                    "Memory admission may pause additional loads.", managerWidth);
                gui::wrapped_label("Model-set status: " + nativeState.phase, managerWidth);
                gui::wrapped_label("Last actual response: " +
                    (nativeState.last_response_model.empty()
                        ? std::string{"none yet"}
                        : nativeState.last_response_role + " -> " +
                            nativeState.last_response_model), managerWidth);
                const std::string target = nativeModelId.empty()
                    ? nativeState.model : nativeModelId;
                const bool allowChange = !nativeState.busy && !modelRequestActive;
                const std::array nativeActions{
                    gui::InlineButtonSpec{.label = "Rescan", .width = 0.0f,
                        .enabled = !modelRequestActive
                            && !editor.aiModelManagerAutoRescanPending},
                    gui::InlineButtonSpec{.label = "Install Only", .width = 0.0f,
                        .enabled = allowChange && !target.empty()},
                    gui::InlineButtonSpec{.label = "Load Additional", .width = 0.0f,
                        .enabled = allowChange && !target.empty()},
                    gui::InlineButtonSpec{.label = "Load & Use", .width = 0.0f,
                        .enabled = allowChange && !target.empty()},
                    gui::InlineButtonSpec{.label = nativeState.busy
                        ? "Cancel Setup" : "Unload Selected", .width = 0.0f,
                        .enabled = nativeState.busy || (allowChange && !target.empty()
                            && std::ranges::any_of(nativeState.models,
                                [&](const auto& item){return item.id==target && item.active;}))},
                    gui::InlineButtonSpec{.label = "Import Model Profile...", .width = 0.0f,
                        .enabled = allowChange}};
                if (const auto action = gui::inline_button_row(nativeActions, 30.0f, 5.0f, managerActionLayout))
                {
                    bool accepted = true;
                    bool showRequestFeedback = true;
                    switch (*action)
                    {
                        case 0u: accepted = native::refresh(); break;
                        case 1u: accepted = native::install(target); break;
                        case 2u:
                            accepted = native::load(target, false);
                            // A first resident foundation must have a usable
                            // editor chat transport as well as loaded weights.
                            // Do not steal an explicitly configured provider.
                            if (accepted && (editor.aiMethodSelection < 0
                                || (editor.aiMethodSelection == 1
                                    && !epochengine::ai::is_model_use_confirmed())))
                                accepted = select_native_chat_transport(editor);
                            break;
                        case 3u:
                            // Explicit Load & Use changes the operator's model,
                            // unlike automatic small/large model routing.
                            ++editor.aiChatUserModelSelectionRevision;
                            if (accepted)
                            {
                                editor.aiMethodSelection = 0;
                                const bool alreadyResident = std::ranges::any_of(
                                    nativeState.models, [&](const auto& entry) {
                                        return entry.id == target && entry.active;
                                    });
                                accepted = alreadyResident
                                    ? native::use_loaded(target)
                                    : native::prepare(target);
                            }
                            break;
                        case 4u:
                            if (nativeState.busy) native::cancel_setup();
                            else accepted = native::unload(target);
                            break;
                        case 5u:
                        {
                            const auto selected = platform::filesystem::open_file_dialog(
                                L"Import an existing model profile", L"Model profile",
                                L"manifest.json", std::filesystem::path{});
                            showRequestFeedback = false;
                            if (selected.accepted)
                            {
                                accepted = native::import_model_profiles(selected.path.generic_string());
                                editor.aiModelManagerFeedback = accepted
                                    ? "Import queued. Existing model files and saved tuning are preserved; no model will be loaded."
                                    : "Import could not start. Finish the active request and try again.";
                            }
                            else editor.aiModelManagerFeedback = selected.cancelled
                                ? "Profile import cancelled."
                                : (selected.diagnostic.empty()
                                    ? "Select an existing model manifest.json."
                                    : selected.diagnostic);
                            break;
                        }
                        default: break;
                    }
                    if (showRequestFeedback)
                        editor.aiModelManagerFeedback = accepted
                            ? "Model request accepted; watch status for completion."
                            : "Model request refused; finish the active request or check the model ID.";
                }
                gui::wrapped_label("Import Model Profile adds an existing local model from its manifest.json, including saved tuning. Development and Public keep separate registrations; importing does not load weights or copy chat history.", managerWidth);
                gui::wrapped_label("Load Additional preserves the current foundation. Load & Use assigns the chosen model as foundation. Unload Selected preserves other resident runtimes. RAM/VRAM and context KV caches are shared system resources; tune each profile to avoid OOM. Performance counts track inference and agent outcomes separately; a completed response is not a verified code build. Same-class baseline comparisons require sufficient measurements and are observational, not guaranteed time savings.", managerWidth);
                gui::wrapped_label("Optional: add an Epoch-managed native GGUF profile (not required for external API models).", managerWidth);
                gui::label("Model ID");
                (void)gui::edit_box(editor.aiRegisteredModelId,
                    {managerWidth, 28.0f}, 256u, false);
                gui::label("Existing GGUF file path");
                (void)gui::edit_box(editor.aiRegisteredModelPath,
                    {managerWidth, 28.0f}, 2048u, false);
                if (gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label="Register Existing GGUF",
                            .enabled=allowChange && !editor.aiRegisteredModelId.empty()
                                && !editor.aiRegisteredModelPath.empty()}},
                        30.0f, 6.0f, managerActionLayout))
                    editor.aiModelManagerFeedback = native::register_model(
                        editor.aiRegisteredModelId, editor.aiRegisteredModelPath)
                        ? "Model registration queued; rescan after it completes."
                        : "Registration rejected. Check model ID/path and runtime state.";

                // Unregister is deliberately separate from deletion of weights.
                // The protected built-in default must remain registered. Never
                // remove a profile while its inference process is loaded.
                constexpr std::string_view builtInId = "qwen3.8-flash-next-ud-q2-k-xl";
                const bool removable = !target.empty() && target != builtInId
                    && !std::ranges::any_of(nativeState.models,
                        [&](const auto& entry){return entry.id == target && entry.active;});
                if (std::ranges::any_of(nativeState.models,
                        [&](const auto& entry){return entry.id == target && entry.active;}))
                    gui::wrapped_label("Unload this model before unregistering it.", managerWidth);
                else if (target == builtInId)
                    gui::wrapped_label("The built-in model profile is protected and cannot be unregistered.", managerWidth);
                if (editor.aiModelPendingUnregister != target)
                {
                    if (gui::inline_button_row(std::array{
                            gui::InlineButtonSpec{.label="Unregister Selected Model",
                                .enabled=removable && allowChange}},
                            30.0f, 6.0f, managerActionLayout))
                        editor.aiModelPendingUnregister = target;
                }
                else if (removable)
                {
                    gui::wrapped_label("Unregister '" + target + "' from Epoch? This removes its model profile, not the GGUF weights on disk.", managerWidth);
                    const std::array confirmationActions{
                        gui::InlineButtonSpec{.label="Confirm Unregister",.width=0.0f,.enabled=allowChange},
                        gui::InlineButtonSpec{.label="Cancel",.width=0.0f}};
                    if (const auto choice = gui::inline_button_row(confirmationActions, 30.0f, 6.0f, managerActionLayout))
                    {
                        if (*choice == 0u)
                        {
                            const bool accepted = native::unregister_model(target);
                            editor.aiModelManagerFeedback = accepted
                                ? "Model unregister requested. GGUF weights remain untouched."
                                : "Unregister refused; check for an active model request.";
                            if (accepted) nativeModelId.clear();
                        }
                        editor.aiModelPendingUnregister.clear();
                    }
                }
            }
            else if (editor.aiModelManagerTab == 1)
            {
                const auto endpoint = epochengine::ai::active_local_api_endpoint();
                gui::property_row("Endpoint", endpoint.base_url, 125.0f);
                gui::wrapped_label(
                    "LM Studio, Bionic and compatible harnesses manage their own weights. "
                    "Epoch scans their APIs without requiring a GGUF path, loading weights "
                    "or permanently registering inventory results.", managerWidth);
                gui::section_heading("Discovered models (suggestions only)");
                gui::wrapped_label("Scans loaded or advertised models at known/saved local APIs. "
                    "Only Epoch-owned model profiles and the last model actually used are remembered. "
                    "Suggestions disappear after restart unless rediscovered; no weights path is needed.", managerWidth);
                if (!editor.aiSuggestionScanStatus.empty())
                    gui::wrapped_label(editor.aiSuggestionScanStatus, managerWidth);
                if (gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label="Rescan All Local Endpoints",
                            .enabled=!editor.aiSuggestionScanPending}},
                        30.0f, 6.0f, managerActionLayout))
                {
                    editor.aiSuggestionScanStarted = false;
                    editor.aiSuggestedModels.clear();
                }
                if (!editor.aiSuggestedModels.empty())
                {
                    std::vector<std::string> suggestedLabels{};
                    std::vector<std::string_view> suggestedViews{};
                    suggestedLabels.reserve(editor.aiSuggestedModels.size());
                    for (const auto& item : editor.aiSuggestedModels)
                        suggestedLabels.push_back(item.model_id + "  —  " + item.endpoint);
                    suggestedViews.reserve(suggestedLabels.size());
                    for (const auto& label : suggestedLabels)
                        suggestedViews.emplace_back(label);
                    editor.aiSelectedSuggestion = (std::min)(editor.aiSelectedSuggestion,
                        editor.aiSuggestedModels.size() - 1u);
                    const auto choice = gui::select_box(gui::SelectBoxOptions{
                        .id = "epoch-model-manager-suggestions",
                        .placeholder = "Choose discovered model",
                        .selected = suggestedViews[editor.aiSelectedSuggestion],
                        .options = suggestedViews,
                        .size = {managerControlWidth, 30.0f},
                        .max_visible_options = 7u,
                        .enabled = !modelRequestActive && !editor.aiModelInventoryPending});
                    if (choice.changed && choice.selected_index
                        && *choice.selected_index < editor.aiSuggestedModels.size())
                        editor.aiSelectedSuggestion = *choice.selected_index;
                    if (gui::inline_button_row(std::array{
                            gui::InlineButtonSpec{.label="Use Suggested Endpoint",
                                .enabled=!modelRequestActive && !editor.aiModelInventoryPending}},
                            30.0f, 6.0f, managerActionLayout))
                    {
                        const auto& suggested = editor.aiSuggestedModels[editor.aiSelectedSuggestion];
                        if (epochengine::ai::select_local_api_endpoint(suggested.endpoint))
                        {
                            editor.aiEndpointDraft = suggested.endpoint;
                            editor.aiPendingModelSelection = suggested.model_id;
                            editor.aiModelInventoryScanned = false;
                            editor.aiAutomaticModelSelection = false;
                            request_ai_model_inventory_refresh(editor);
                            editor.aiModelManagerFeedback = "Endpoint staged and refreshed. Confirm the model below to use it.";
                        }
                        else editor.aiModelManagerFeedback = epochengine::ai::model_detection_status();
                    }
                }
                auto endpointList = epochengine::ai::saved_local_api_endpoints();
                std::vector<std::string_view> endpointViews{};
                for (const auto& entry : endpointList) endpointViews.push_back(entry);
                const auto endpointChoice = gui::select_box(gui::SelectBoxOptions{
                    .id = "epoch-model-manager-endpoint",
                    .placeholder = "Saved endpoints",
                    .selected = endpoint.base_url,
                    .options = endpointViews,
                    .size = {managerControlWidth, 30.0f},
                    .max_visible_options = 5u,
                    .enabled = !modelRequestActive && !editor.aiModelInventoryPending});
                if (endpointChoice.changed && endpointChoice.selected_index
                    && *endpointChoice.selected_index < endpointList.size())
                    editor.aiEndpointDraft = endpointList[*endpointChoice.selected_index];
                gui::label("Endpoint address (LM Studio default: http://127.0.0.1:1234/v1)");
                (void)gui::edit_box(editor.aiEndpointDraft,
                    {managerWidth, 30.0f}, 512u, false);
                const bool canEditEndpoint = !modelRequestActive
                    && !editor.aiModelInventoryPending;
                const std::array endpointActions{
                    gui::InlineButtonSpec{.label = "Save / Connect", .width = 0.0f,
                        .enabled = canEditEndpoint && !editor.aiEndpointDraft.empty()},
                    gui::InlineButtonSpec{.label = "Rescan Models", .width = 0.0f,
                        .enabled = canEditEndpoint}};
                if (const auto action = gui::inline_button_row(endpointActions, 30.0f, 6.0f, managerActionLayout))
                {
                    if (*action == 0u)
                    {
                        if (epochengine::ai::select_local_api_endpoint(editor.aiEndpointDraft))
                        {
                            editor.aiModelInventoryScanned = false;
                            editor.aiPendingModelSelection.clear();
                            editor.aiAutomaticModelSelection = false;
                            request_ai_model_inventory_refresh(editor);
                            editor.aiModelManagerFeedback = "Endpoint saved; model scan started.";
                        }
                        else editor.aiModelManagerFeedback = epochengine::ai::model_detection_status();
                    }
                    else request_ai_model_inventory_refresh(editor);
                }
                if (!editor.aiModelInventoryScanned && !editor.aiModelInventoryPending)
                    request_ai_model_inventory_refresh(editor);
                const auto models = poll_ai_model_inventory(editor);
                std::vector<std::string_view> modelViews{};
                for (const auto& model : models) modelViews.push_back(model);
                const auto selection = epochengine::ai::local_model_selection();
                gui::label("Select model reported by this endpoint");
                const auto modelChoice = gui::select_box(gui::SelectBoxOptions{
                    .id = "epoch-model-manager-local-model",
                    .placeholder = "Choose available model",
                    .selected = editor.aiPendingModelSelection.empty()
                        ? selection.model_id : editor.aiPendingModelSelection,
                    .options = modelViews,
                    .size = {managerControlWidth, 30.0f},
                    .max_visible_options = 7u,
                    .enabled = canEditEndpoint});
                if (modelChoice.changed && modelChoice.selected_index
                    && *modelChoice.selected_index < models.size())
                {
                    editor.aiPendingModelSelection = models[*modelChoice.selected_index];
                    ++editor.aiChatUserModelSelectionRevision;
                }
                gui::property_row("Inventory", editor.aiModelInventoryPending
                    ? "Scanning..." : std::to_string(models.size()) + " models reported", 135.0f);
                gui::property_row("Selected", selection.model_id.empty()
                    ? "None" : selection.model_id, 135.0f);
                gui::property_row("Ready for next Send", selection.confirmed
                    ? "Selected" : "Not confirmed", 135.0f);
                if (gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label="Use Selected Model",
                            .enabled=canEditEndpoint && !editor.aiPendingModelSelection.empty()}},
                        30.0f, 6.0f, managerActionLayout))
                {
                    ++editor.aiChatUserModelSelectionRevision;
#if EPOCH_EDITION_PUBLIC
                        const bool allowed = true;
#endif
                    if (allowed)
                    {
                        (void)native::select_for_epoch_requests(false);
                        epochengine::ai::select_openai_compatible_runtime();
                        editor.aiMethodSelection = 1;
                        editor.aiAutomaticModelSelection = false;
                        editor.aiModelManagerFeedback = confirm_pending_ai_model(editor)
                            ? "Local API model selected for future requests."
                            : editor.aiModelConsentStatus;
                    }
                    else editor.aiModelManagerFeedback = "Current agent transport is busy.";
                }
                gui::wrapped_label(editor.aiModelConsentStatus, managerWidth);
            }
            else
            {
                const auto status = epochengine::ai::direct_runtime_status();
                gui::property_row("Direct runtime", status.message, 135.0f);
                gui::property_row("Executable", status.executable, 135.0f);
                gui::property_row("GGUF", status.model, 135.0f);
                gui::wrapped_label("Direct llama.cpp executes an explicitly selected local "
                    "binary and GGUF file; it does not start an API server.", managerWidth);
                gui::label("llama-cli executable path");
                (void)gui::edit_box(editor.aiDirectExecutableDraft,
                    {managerWidth, 30.0f}, 2048u, false);
                gui::label("Existing GGUF file path");
                (void)gui::edit_box(editor.aiDirectModelDraft,
                    {managerWidth, 30.0f}, 2048u, false);
                const std::array directActions{
                    gui::InlineButtonSpec{.label = "Find Installed", .width = 0.0f,
                        .enabled = !modelRequestActive},
                    gui::InlineButtonSpec{.label = "Use These Paths", .width = 0.0f,
                        .enabled = !modelRequestActive
                            && !editor.aiDirectExecutableDraft.empty()
                            && !editor.aiDirectModelDraft.empty()}};
                if (const auto action = gui::inline_button_row(directActions, 30.0f, 6.0f, managerActionLayout))
                {
                    if (*action == 0u)
                    {
                        const auto found = epochengine::ai::discover_direct_runtime();
                        editor.aiDirectExecutableDraft = found.executable;
                        editor.aiDirectModelDraft = found.model;
                        editor.aiModelManagerFeedback = found.message;
                    }
                    else
                    {
#if EPOCH_EDITION_PUBLIC
                        const bool allowed = true;
#endif
                        const bool accepted = allowed && epochengine::ai::select_direct_runtime(
                            editor.aiDirectExecutableDraft, editor.aiDirectModelDraft);
                        if (accepted)
                        {
                            (void)native::select_for_epoch_requests(false);
                            editor.aiMethodSelection = 2;
                        }
                        editor.aiModelManagerFeedback = accepted
                            ? "Direct GGUF runtime selected; request-time startup is used."
                            : "Direct runtime rejected. Check both paths and active requests.";
                    }
                }
            }
            if (!editor.aiModelManagerFeedback.empty())
            {
                gui::wrapped_label(editor.aiModelManagerFeedback, managerWidth - 18.0f);
            }
            gui::end_scroll_area();
            gui::set_cursor({managerBodyOrigin.x, managerFooterY});
            // Persistent action buttons: scrolling long model inventories must
            // never hide the live cancellation control or the close action.
            if (const auto action = gui::inline_button_row(
                    visibleManagerFooterActions, 30.0f, 6.0f, managerFooterLayout))
            {
                if (nativeState.busy && *action == 0u) {
                    native::cancel_setup();
                    editor.aiModelManagerFeedback =
                        "Stopping model setup; existing resident weights stay loaded.";
                } else {
                    editor.showAiModelManagerModal = false;
                }
            }
            gui::end_modal_window();
        }
        else if (editor.aiModelManagerOpenLogged)
        {
            editor.aiModelManagerOpenLogged = false;
#if EPOCH_EDITION_PUBLIC
            push_editor_log(editor, "[ai-models] Model Manager closed; passive scan may still finish.");
#endif
        }
