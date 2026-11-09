// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.showPackageManagerModal)
        {
            editor.openMenu = TopMenu::None;
            poll_package_source_task(editor);
            if (!editor.packageSourceView) request_site_package_catalog(editor, false);
            poll_site_package_catalog(editor);
            const gui::Vec2 modalSize = packageManagerModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = (std::max)(1.0f, modalSize.x - 56.0f);
            const auto projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            const auto forestFactoryPackage = projectRoot / "assets" / "packages" / "engine_forest_factory.package.json";
            const auto forestFactoryProfile = projectRoot / "assets" / "packages" / "engine_forest_factory" / "default.forest.json";
            const auto& catalogEntries = editor.packageCatalogEntries;
            const epochengine::package_catalog::Entry* selectedCatalog = nullptr;
            const epochengine::package_registry::PackageDescriptor* selectedPackage = nullptr;
            selectedPackage = package_registry::find(editor.selectedPackageId);
            for (const auto& entry : catalogEntries)
                if (entry.id == editor.selectedPackageId)
                    selectedCatalog = &entry;
            const bool forestFactoryStaged = path_exists(forestFactoryPackage) && path_exists(forestFactoryProfile);
            const auto epochLocalAi =
                epochengine::ai::epoch_local_ai_install_status();

            auto model_install_request_path = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                const std::string safeId = safe_package_artifact_id(package.id);
                return resolve_editor_path(std::filesystem::path{ epochengine::ai::local_model_root() }) / safeId / "install.request.json";
            };
            auto local_ai_runtime_request_path = [&]()
            {
                return resolve_editor_path(std::filesystem::path{ epochengine::ai::epoch_local_llama_cpp_root() })
                    / "install.request.json";
            };

            auto package_installed = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                if (package.id == epochengine::package_registry::kEngineForestFactoryPackageId)
                    return forestFactoryStaged;
                if (package.kind
                    == epochengine::package_registry::PackageKind::ModelAsset)
                {
                    const auto plan = ai::model_install::plan_for(package.id);
                    if (plan && model_snapshot_installed(*plan))
                        return true;
                }
                if (package.id == epochengine::package_registry::kQwenCoderPackageId)
                    return epochLocalAi.model_receipt_ready
                        && epochLocalAi.model_file_ready;
                if (package.id == epochengine::package_registry::kLocalAiLlamaCppRuntimePackageId)
                    return epochLocalAi.runtime_receipt_ready
                        && epochLocalAi.runtime_executable_ready;
                return false;
            };
            auto package_requested = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                if (editor.packageModelInstall
                    && editor.packageModelInstall->plan.package_id == package.id)
                    return true;
                if (package.kind == epochengine::package_registry::PackageKind::ModelAsset)
                    return path_exists(model_install_request_path(package));
                if (package.id == epochengine::package_registry::kLocalAiLlamaCppRuntimePackageId)
                    return path_exists(local_ai_runtime_request_path());
                return false;
            };

            auto package_status = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                if (package_installed(package))
                    return std::string("installed");
                if (package_requested(package))
                    return std::string("install request staged");
                if (package.requiresExplicitNetworkApproval)
                    return std::string("blocked by approval gate");
                if (package.kind == epochengine::package_registry::PackageKind::DownloadableSource
                    || package.kind == epochengine::package_registry::PackageKind::ResearchPrototype)
                    return std::string("source gate available");
                if (package.shipsInCore)
                    return std::string("core");
                return std::string("available");
            };

            auto select_package = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                selectedCatalog = nullptr;
                selectedPackage = &package;
                editor.selectedPackageId = std::string(package.id);
                editor.packageTechnicalDetailsOpen = false;
                editor.packageInstallStatus = std::string("Selected ") + std::string(package.displayName) + ".";
                editor.packageInstallProgress = package_installed(package) ? 1.0f : 0.0f;
            };

            auto select_catalog_entry = [&](const package_catalog::Entry& entry)
            {
                selectedCatalog = &entry;
                selectedPackage = package_registry::find(entry.id);
                editor.selectedPackageId = entry.id;
                editor.packageTechnicalDetailsOpen = false;
                editor.packageInstallStatus = "Selected " + entry.display_name + ".";
                editor.packageInstallProgress = selectedPackage
                    && package_installed(*selectedPackage) ? 1.0f : 0.0f;
            };

            auto install_package = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                if (package_registry::catalog_section(package.id)
                    == package_registry::CatalogSection::ModelManager)
                {
                    editor.showPackageManagerModal = false;
                    editor.showAiModelManagerModal = true;
                    editor.packageInstallStatus = "Inference models and runtimes are managed in Model Manager.";
                    return;
                }
                select_package(package);
                if (builtin_project_package_ready(package.id))
                {
                    add_builtin_project_package(editor, ctx, package);
                    return;
                }

                if (package.requiresExplicitNetworkApproval)
                {
                    editor.packageInstallStatus = "Blocked: network/server packages require explicit human approval.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[package] Blocked ") + std::string(package.id) + ": explicit network approval required.");
                    return;
                }

                if (package.kind == epochengine::package_registry::PackageKind::ModelAsset)
                {
                    if (stage_model_package_opt_in(editor, package))
                        (void)start_model_package_install(editor, package);
                    return;
                }

                if (package.id == epochengine::package_registry::kLocalAiLlamaCppRuntimePackageId)
                {
                    (void)stage_local_ai_runtime_opt_in(editor, package);
                    return;
                }

                if (package.kind == epochengine::package_registry::PackageKind::DownloadableSource
                    || package.kind == epochengine::package_registry::PackageKind::ResearchPrototype)
                {
                    open_package_source_review(editor, std::string(package.id));
                    return;
                }

                editor.packageInstallStatus = "Core opt-in package selected; activate it from the matching editor surface.";
                editor.packageInstallProgress = 0.35f;
                push_editor_log(editor, std::string("[package] Selected core opt-in package ") + std::string(package.id) + ".");
            };

            auto remove_package = [&](const epochengine::package_registry::PackageDescriptor& package)
            {
                select_package(package);
                std::error_code ec{};
                if (package.id == epochengine::package_registry::kEngineForestFactoryPackageId)
                {
                    (void)std::filesystem::remove(forestFactoryPackage, ec);
                    ec.clear();
                    (void)std::filesystem::remove(forestFactoryProfile, ec);
                    editor.packageInstallStatus = "Forest Factory project manifest/profile removed; core editor workspace remains available.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, "[package] Removed forest_factory project-local package files.");
                    return;
                }
                if (package.kind == epochengine::package_registry::PackageKind::ModelAsset)
                {
                    (void)std::filesystem::remove(model_install_request_path(package), ec);
                    editor.packageInstallStatus = "Model install request removed. Existing weights, if any, stay in cache/models for manual review.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[package] Removed model install request for ") + std::string(package.id) + ".");
                    return;
                }

                editor.packageInstallStatus = "No project-local payload exists for this package yet.";
                editor.packageInstallProgress = 0.0f;
            };

            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Package Manager",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const float packageContentTop = gui::cursor_position().y;
            const float packageContentBottom = packageContentTop + (std::max)(
                0.0f, modalSize.y - gui::titled_window_total_height(0.0f));
            const gui::InlineButtonRowOptions packageActionLayout{
                .sizing = gui::InlineButtonSizing::FitContent,
                .alignment = gui::InlineButtonAlignment::Center,
                .wrap = true, .available_width = contentWidth};
            if (editor.packageSourceView)
            {
                bool sourceBusy = package_source_task_busy(editor) || editor.planetaryFeatureReviewPending.has_value()
                    || editor.planetaryFeatureAdmissionPending.has_value() || editor.waterAttachmentPending.has_value();
                const bool standardPackage = !editor.packageSourceExpectedId.empty();
                if (!standardPackage)
                {
                gui::label("Package manifest path (.epoch-package.json)");
                if (sourceBusy) gui::wrapped_label(editor.packageSourcePathDraft, contentWidth);
                else if (gui::edit_box(editor.packageSourcePathDraft, {contentWidth, 30.0f}, 32768u, false).changed)
                {
                    editor.packageSourceReviewCurrent = false;
                    editor.packageSourceReview.reset();
                    editor.packageSourceFileLines.clear();
                    editor.packageSourceStaged = {};
                    editor.packageSourceStatus = "Path changed. Choose Review Source before approving these files.";
                }
                const std::array pathActions{
                    gui::InlineButtonSpec{.label="Review Source", .width=0.0f,
                        .enabled=!sourceBusy && !editor.packageSourcePathDraft.empty()}};
                if (gui::inline_button_row(pathActions, 30.0f, 6.0f, packageActionLayout))
                    start_package_source_inspection(editor);
                }
                sourceBusy = package_source_task_busy(editor) || editor.planetaryFeatureReviewPending.has_value()
                    || editor.planetaryFeatureAdmissionPending.has_value() || editor.waterAttachmentPending.has_value();
                const bool sourceReady = editor.packageSourceReview && editor.packageSourceReview->review_ready();
                const bool sourceStaged = editor.packageSourceStaged.source_staged();
                const bool updateProjectSource=sourceReady&&!editor.aiPackageRequest&&editor.planetaryAttachment.source_attached()
                    &&editor.packageSourceReview->registry_package_id==editor.planetaryAttachment.registry_package_id
                    &&editor.packageSourceReview->content_hash!=editor.planetaryAttachment.content_hash;
                const bool canAttach = editor.packageSourceReviewCurrent && sourceStaged && sourceReady && !sourceBusy && !planetary_project_busy(editor)
                    &&!editor.projectBuildPending&&!editor.scriptBuildPending
                    && (editor.packageSourceReview->registry_package_id == "research_planetary_terrain"
                        || (editor.packageSourceReview->registry_package_id == "research_fft_ocean" && !editor.projectRoot.empty()));
                const std::array sourceActions{
                    gui::InlineButtonSpec{.label=standardPackage
                            ? (!editor.packageSourceReviewCurrent ? "Check Source Again" : (sourceReady && editor.packageSourceReview->registry_package_id != "research_planetary_terrain"
                                    && editor.packageSourceReview->registry_package_id != "research_fft_ocean")
                                ? "Project Activation Unavailable"
                                : (editor.planetaryFeatureRequest ? "Include Water & Continue"
                                    : editor.projectRoot.empty() ? (sourceReady && editor.packageSourceReview->registry_package_id == "research_fft_ocean"
                                        ? "Open Project to Add Water" : "Create Terrain Project") :updateProjectSource?"Update Project Source"
                                    : ai_package_generation_awaiting_attachment(editor) ? "Add & Generate" : "Add to Project"))
                            : (sourceStaged ? "Source Staged" : "Approve & Stage Source"),
                        .width=0.0f, .enabled=standardPackage ? (!sourceBusy && (!editor.packageSourceReviewCurrent || canAttach))
                            : (editor.packageSourceReviewCurrent && sourceReady && !sourceBusy && !sourceStaged)},
                    gui::InlineButtonSpec{.label=standardPackage ? "Package List" : "Browse...", .width=0.0f, .enabled=!sourceBusy},
                    gui::InlineButtonSpec{.label=sourceBusy ? "Cancel Operation" : (editor.planetaryFeatureRequest ? "Cancel Water Review" : standardPackage ? "Advanced Local Source..." : "Package List"), .width=0.0f},
                    gui::InlineButtonSpec{.label="Close", .width=0.0f}
                };
                const bool awaitingSourceApproval = standardPackage ? canAttach
                    : editor.packageSourceReviewCurrent && sourceReady && !sourceBusy && !sourceStaged;
                gui::section_heading(sourceBusy ? "Checking extension files"
                    : awaitingSourceApproval ? "Waiting for your approval"
                        : !editor.packageSourceReviewCurrent ? "Source needs checking" : "Extension source",
                    awaitingSourceApproval ? gui::SemanticTone::warning : gui::SemanticTone::info);
                if (sourceReady)
                    gui::wrapped_label(editor.packageSourceReview->display_name + " — "
                        + std::to_string(editor.packageSourceReview->files.size()) + " checked source files", contentWidth);
                const gui::Vec2 sourceActionPosition = gui::cursor_position();
                gui::advance_cursor({0.0f, gui::inline_button_row_height(sourceActions, 30.0f, 6.0f, packageActionLayout) + 10.0f});
                const float sourceBodyHeight = (std::max)(1.0f, packageContentBottom - gui::cursor_position().y - 8.0f);
                (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                    .id="package-source-review", .size={contentWidth, sourceBodyHeight},
                    .content_height=0.0f, .draw_background=true, .show_scrollbar=true});
                const float sourceWidth = (std::max)(1.0f, contentWidth - 24.0f);
                gui::wrapped_label(editor.packageSourceStatus, sourceWidth);
                gui::wrapped_label(standardPackage
                    ? (ai_package_generation_awaiting_attachment(editor)
                        ? "Review these source files. Add & Generate approves this exact copy, then builds the terrain request shown below."
                        : "Review this extension's local source files. Add it to your open project, or create a terrain example. Building starts only when you choose Build & Generate.")
                    : "Review downloaded source before copying it to the local cache. Nothing is built or run here.", sourceWidth);
                if (editor.packageSourceExpectedId == "research_planetary_terrain")
                    gui::wrapped_label("New Planet projects start with Ocean On and include Water (FFT Ocean). Add first approves these Planet source files; a separate exact Water source review follows before any settings save or build. Existing Ocean Off settings stay dry. Existing project source is never overwritten automatically.", sourceWidth);
                if (editor.planetaryFeatureRequest)
                    gui::wrapped_label("Water will be included automatically for this planet's ocean. Approve its checked files above to continue your requested save or build, or Cancel to keep the current project.", sourceWidth);
                if (sourceReady)
                {
                    const auto& review = *editor.packageSourceReview;
                    gui::wrapped_label(review.registry_package_id == "research_fft_ocean"
                        ? "Water (FFT Ocean)" : review.display_name, sourceWidth);
                    gui::wrapped_label(review.summary, sourceWidth);
                    gui::wrapped_label("License: " + review.license, sourceWidth);
                    if(updateProjectSource)gui::wrapped_label("Update replaces only the installed package source after approval. Previous source is retained; scenes/settings stay unchanged. Current: "
                        +editor.planetaryAttachment.content_hash+" | Reviewed: "+review.content_hash,sourceWidth);
                    gui::wrapped_label(std::to_string(review.files.size()) + " source files, "
                        + std::to_string(review.total_bytes) + " bytes. Approval covers only this checked file set.", sourceWidth);
                    if (ai_package_generation_awaiting_attachment(editor))
                        gui::wrapped_label("After you approve these exact files: generate "
                            + editor.aiPackageRequest->settings.mode + " terrain (seed "
                            + std::to_string(editor.aiPackageRequest->settings.seed)
                            + "). The build runs locally; Cancel remains available in Project.", sourceWidth);
                    gui::wrapped_label("Source files to copy", sourceWidth);
                    (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                        .id="package-source-files", .size={sourceWidth,(std::max)(100.0f, sourceBodyHeight * 0.6f)},
                        .lines=editor.packageSourceFileLines, .max_line_chars=4096u,
                        .selectable=true, .stick_to_bottom=false, .wrap_lines=true});
                    if (gui::inline_button_row(std::array{gui::InlineButtonSpec{
                            .label=editor.packageSourceTechnicalDetails ? "Hide Source Details" : "Source Details",
                            .width=0.0f}}, 28.0f, 6.0f, packageActionLayout))
                        editor.packageSourceTechnicalDetails = !editor.packageSourceTechnicalDetails;
                    if (editor.packageSourceTechnicalDetails)
                    {
                        std::vector<std::string> details{
                            "Package: " + std::string(package_registry::find(review.registry_package_id)
                                ? package_registry::find(review.registry_package_id)->displayName
                                : std::string_view{review.registry_package_id}),
                            "Manifest: " + review.manifest_path.generic_string(),
                            "Repository: " + review.declared_repository,
                            "Reviewed content: " + review.content_hash};
                        if (sourceStaged)
                        {
                            details.push_back("Staged source: " + editor.packageSourceStaged.source_root.generic_string());
                            details.push_back("Receipt: " + editor.packageSourceStaged.receipt_path.generic_string());
                        }
                        (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                            .id="package-source-technical", .size={sourceWidth,150.0f}, .lines=details,
                            .max_line_chars=4096u, .selectable=true, .stick_to_bottom=false, .wrap_lines=true});
                    }
                    gui::wrapped_label(sourceStaged && editor.planetaryAttachment.source_attached()
                            && review.registry_package_id == editor.planetaryAttachment.registry_package_id
                            && review.content_hash == editor.planetaryAttachment.content_hash
                        ? "Source has been added to this project. Open Project to save settings and choose Build & Generate."
                        : sourceStaged
                            ? "Only source files are staged. Nothing has been built, run or added to your project."
                            : "Approve only if you want these files copied to the local source cache. You can cancel while the copy runs.", sourceWidth);
                }
                else
                {
                    gui::wrapped_label(sourceBusy ? "Checking the local package source..."
                        : standardPackage ? "No verified source is ready. Check Source Again retries the local source check. Nothing has been added to your project."
                            : "Choose a local package manifest to review it. No project needs to be open.", sourceWidth);
                    if (standardPackage && !sourceBusy)
                        gui::wrapped_label(selectedCatalog
                                && selectedCatalog->availability == package_catalog::Availability::available
                            ? "This catalog lists a source archive, but source-archive downloads are not available yet. A verified local Extensions checkout is required."
                            : "The catalog does not provide a downloadable source package. Restore the local Extensions checkout, then check again.", sourceWidth);
                }
                gui::end_scroll_area();
                gui::set_cursor(sourceActionPosition);
                if (const auto action = gui::inline_button_row(sourceActions, 30.0f, 6.0f, packageActionLayout))
                {
                    if (*action == 0u)
                    {
                        if (standardPackage && !editor.packageSourceReviewCurrent)
                            open_package_source_review(editor, editor.packageSourceExpectedId);
                        else if (!standardPackage) approve_and_stage_package_source(editor);
                        else if (editor.packageSourceExpectedId == "research_fft_ocean") start_reviewed_water_attachment(editor);
                        else if (editor.projectRoot.empty()) create_planetary_example_project(editor, ctx);
                        else start_planetary_project_attachment(editor,updateProjectSource);
                    }
                    else if (*action == 1u)
                    {
                        if (standardPackage)
                        {
                            cancel_package_source_task(editor);
                            editor.packageSourceView = false;
                        }
                        else browse_package_source(editor);
                    }
                    else if (*action == 2u)
                    {
                        if (sourceBusy || editor.planetaryFeatureRequest)
                        {
                            if (editor.planetaryFeatureReviewPending || editor.planetaryFeatureAdmissionPending || editor.waterAttachmentPending)
                                cancel_planetary_project_task(editor);
                            cancel_package_source_task(editor);
                        }
                        else if (standardPackage) editor.packageSourceExpectedId.clear();
                        else editor.packageSourceView = false;
                    }
                    else
                    {
                        if (editor.planetaryFeatureReviewPending || editor.planetaryFeatureAdmissionPending || editor.waterAttachmentPending)
                            cancel_planetary_project_task(editor);
                        cancel_package_source_task(editor);
                        editor.showPackageManagerModal = false;
                    }
                }
            }
            else
            {
            const std::array packageTabs{
                gui::TabButtonSpec{.id="package-manager.addons", .label="Project Packages",
                    .width=152.0f, .active=!editor.packageCatalogModelsOnly},
                gui::TabButtonSpec{.id="package-manager.ai-resources", .label="Project AI Resources",
                    .width=180.0f, .active=editor.packageCatalogModelsOnly}
            };
            if (const auto tabs = gui::tab_bar_buttons(packageTabs, 29.0f, 5.0f);
                tabs.selected_index)
            {
                editor.packageCatalogModelsOnly = *tabs.selected_index == 1u;
                if (editor.packageCatalogModelsOnly)
                    editor.aiModelManagerAutoRescanPending = true;
            }
            const bool projectAiResources = editor.packageCatalogModelsOnly;
            gui::wrapped_label(projectAiResources
                ? "Let this project use the engine's selected AI service. Model weights stay shared; nothing is bundled or loaded here."
                : "Software add-ons and dependencies for the active project.",
                contentWidth);
            const std::array managerActions{
                gui::InlineButtonSpec{.label="Model Manager", .width=0.0f},
                gui::InlineButtonSpec{.label=editor.packageCatalogPending ? "Refreshing..." : "Refresh Catalog",
                    .width=0.0f, .enabled=!editor.packageCatalogPending},
                gui::InlineButtonSpec{.label="Local Source...", .width=0.0f,
                    .enabled=!package_source_task_busy(editor)}
            };
            if (const auto action = gui::inline_button_row(managerActions, 28.0f, 6.0f, packageActionLayout))
            {
                if (*action == 0u)
                {
                    editor.showPackageManagerModal = false;
                    editor.showAiModelManagerModal = true;
                }
                else if (*action == 1u)
                    request_site_package_catalog(editor, true);
                else
                    open_package_source_review(editor);
            }
            if (projectAiResources)
            {
                namespace native = epochengine::ai::native_runtime;
                if (editor.aiModelManagerAutoRescanPending && !native::snapshot(false).busy
                    && native::refresh())
                    editor.aiModelManagerAutoRescanPending = false;
                const auto models = native::snapshot();
                std::string selectedModel = epochengine::ai::active_model_name();
                if (models.selected && !models.model.empty()) selectedModel = models.model;
                for (const auto& model : models.models)
                    if (selectedModel == model.id || selectedModel == model.uri)
                        selectedModel = model.label.empty() ? model.id : model.label;
                gui::wrapped_label("Current Assistant model: "
                    + (selectedModel.empty() ? std::string{"None confirmed"} : selectedModel), contentWidth);
                const bool canChangeProject = !editor.projectId.empty() && !editor.projectRoot.empty()
                    && !editor.projectBuildPending && !editor.projectRuntimeProcess.valid();
                const std::array aiActions{
                    gui::InlineButtonSpec{.label="Enable Project AI", .width=0.0f, .enabled=canChangeProject},
                    gui::InlineButtonSpec{.label="Disable Project AI", .width=0.0f, .enabled=canChangeProject},
                    gui::InlineButtonSpec{.label="Close", .width=0.0f}};
                const std::string policy = editor.projectRoot.empty()
                    ? "Open or create a project first. Choose a model in Model Manager; no download or model load happens here."
                    : editor.projectBuildPending || editor.projectRuntimeProcess.valid()
                        ? "Stop the project build or runtime before changing its AI setting."
                        : "The project follows the engine-selected model, not a pinned model ID. Each session still needs model confirmation. Weights are not copied; image support is not implied.";
                const bool showStatus = editor.packageInstallStatus.starts_with("Project AI");
                const float footerHeight = gui::measure_wrapped_label_height(policy, contentWidth)
                    + (showStatus ? gui::measure_wrapped_label_height(editor.packageInstallStatus, contentWidth) : 0.0f)
                    + gui::inline_button_row_height(aiActions, 30.0f, 6.0f, packageActionLayout) + 20.0f;
                const auto layout = editor_update_modal::measure_package_manager_body(
                    gui::cursor_position().y, packageContentBottom, gui::line_height(), footerHeight);
                float rowsHeight = 70.0f;
                for (const auto& model : models.models)
                    rowsHeight += gui::measure_wrapped_label_height(
                        model.label.empty() ? model.id : model.label, contentWidth - 24.0f) + 62.0f;
                (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                    .id="project-ai-model-profiles", .size={contentWidth,layout.listHeight},
                    .content_height=(std::max)(layout.listHeight,rowsHeight),
                    .draw_background=true, .show_scrollbar=true});
                gui::label("Registered engine models");
                if (models.models.empty())
                    gui::wrapped_label(models.busy ? "Reading model profiles..."
                        : "No profiles discovered yet. Open Model Manager to rescan or register existing weights.", contentWidth - 24.0f);
                for (const auto& model : models.models)
                {
                    gui::wrapped_label(model.label.empty() ? model.id : model.label, contentWidth - 24.0f);
                    gui::wrapped_label(model.active ? "Loaded"
                        : models.inventory_preview ? "Registered; installation and load status not checked"
                            : model.installed ? "Installed, not loaded" : "Registered, not installed",
                        contentWidth - 24.0f);
                    if (gui::text_link("Model settings", {120.0f,24.0f}, false))
                    {
                        nativeModelId = model.id;
                        editor.aiModelManagerTab = 0;
                        editor.showPackageManagerModal = false;
                        editor.showAiModelManagerModal = true;
                    }
                    gui::advance_cursor({0.0f,8.0f});
                }
                if (!models.inventory_error.empty())
                    gui::wrapped_label(models.inventory_error, contentWidth - 24.0f);
                if (editor.projectRoot.empty()
                    && gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label="Open Project Workspace", .width=0.0f}},
                        28.0f, 6.0f, packageActionLayout))
                {
                    editor.showPackageManagerModal = false;
                    open_editor_surface(EditorMainSurface::Project, "Project AI setup");
                }
                gui::end_scroll_area();
                gui::set_cursor({modalPos.x + 28.0f, layout.footerTop});
                gui::wrapped_label(policy, contentWidth);
                if (showStatus) gui::wrapped_label(editor.packageInstallStatus, contentWidth);
                if (const auto action = gui::inline_button_row(aiActions, 30.0f, 6.0f, packageActionLayout))
                {
                    if (*action == 2u) editor.showPackageManagerModal = false;
                    else (void)write_project_ai_profile(editor, *action == 0u
                        ? ProjectAiProviderMode::engine_selected : ProjectAiProviderMode::disabled);
                }
            }
            else
            {
            gui::wrapped_label(projectAiResources
                ? "Chat/coding weights, inference runtimes and provider connections belong in Model Manager. Project resources below never start a model."
                : editor.packageCatalogPending ? "Refreshing the package list..."
                    : catalogEntries.empty() ? "Local extensions are listed below. Refresh Catalog checks for downloadable packages."
                        : "Choose a package below to see what it adds and whether it can be installed.", contentWidth);
            gui::label(projectAiResources ? "Find an image resource" : "Find a project package");
            (void)gui::edit_box(editor.packageCatalogSearch, {contentWidth,28.0f},128u,false);
            const auto matches_search = [&](std::string_view id, std::string_view name, std::string_view summary)
            {
                if (editor.packageCatalogSearch.empty()) return true;
                const auto needle = ascii_lower_model_name(editor.packageCatalogSearch);
                return ascii_lower_model_name(name).find(needle) != std::string::npos
                    || ascii_lower_model_name(id).find(needle) != std::string::npos
                    || ascii_lower_model_name(summary).find(needle) != std::string::npos;
            };
            const auto visible_in_catalog = [&](const package_catalog::Entry& entry)
            {
                return !projectAiResources
                    && package_registry::catalog_section(entry.id) == package_registry::CatalogSection::ProjectPackages
                    && !package_registry::is_catalog_authority(entry.id)
                    && !builtin_project_package_ready(entry.id)
                    && matches_search(entry.id, entry.display_name, entry.summary);
            };
            const auto visible_resource = [&](const package_registry::PackageDescriptor& resource)
            {
                return projectAiResources
                    && package_registry::catalog_section(resource.id) == package_registry::CatalogSection::ProjectAiResources
                    && matches_search(resource.id, resource.displayName, resource.summary);
            };
            const auto visible_local_package = [&](const package_registry::PackageDescriptor& package)
            {
                return !projectAiResources
                    && package_registry::catalog_section(package.id) == package_registry::CatalogSection::ProjectPackages
                    && !package_registry::is_catalog_authority(package.id)
                    && (builtin_project_package_ready(package.id)
                        || std::any_of(package_registry::kSourceManifestBindings.begin(), package_registry::kSourceManifestBindings.end(),
                            [&](const auto& binding){return binding.registry_id == package.id;}))
                    && (builtin_project_package_ready(package.id)
                        || std::none_of(catalogEntries.begin(), catalogEntries.end(),
                            [&](const auto& entry){return entry.id == package.id;}))
                    && matches_search(package.id, package.displayName, package.summary);
            };
            // A tab/search change cannot leave a hidden package selected for an action.
            const auto firstCatalog = std::find_if(catalogEntries.begin(), catalogEntries.end(), visible_in_catalog);
            const auto resources = package_registry::known_packages();
            const auto firstResource = std::find_if(resources.begin(), resources.end(), visible_resource);
            if (projectAiResources)
            {
                selectedCatalog = nullptr;
                if (!selectedPackage || !visible_resource(*selectedPackage))
                {
                    selectedPackage = firstResource == resources.end() ? nullptr : &*firstResource;
                    editor.selectedPackageId = selectedPackage ? std::string(selectedPackage->id) : std::string{};
                }
            }
            else if (!selectedCatalog || !visible_in_catalog(*selectedCatalog))
            {
                if (!selectedPackage || !visible_local_package(*selectedPackage))
                {
                    selectedCatalog = firstCatalog == catalogEntries.end() ? nullptr : &*firstCatalog;
                    const auto firstLocal = std::find_if(resources.begin(), resources.end(), visible_local_package);
                    selectedPackage = selectedCatalog ? package_registry::find(selectedCatalog->id)
                        : (firstLocal == resources.end() ? nullptr : &*firstLocal);
                    editor.selectedPackageId = selectedCatalog ? selectedCatalog->id
                        : (selectedPackage ? std::string(selectedPackage->id) : std::string{});
                }
            }
            const auto visibleCount = projectAiResources
                ? std::count_if(resources.begin(), resources.end(), visible_resource)
                : std::count_if(catalogEntries.begin(), catalogEntries.end(), visible_in_catalog)
                    + std::count_if(resources.begin(), resources.end(), visible_local_package);
            const float packageListPaneWidth = contentWidth >= 900.0f ? (std::min)(600.0f, contentWidth * 0.44f) : contentWidth;
            const float packageListTextWidth = (std::max)(1.0f, packageListPaneWidth - 24.0f);
            const auto packageListRowHeight = [&](std::string_view summary)
            {
                return gui::menu_item_row_height()
                    + gui::measure_wrapped_label_height(summary, packageListTextWidth) + 14.0f;
            };
            float packageListContentHeight = 12.0f;
            if (projectAiResources)
                for (const auto& resource : resources)
                    if (visible_resource(resource))
                        packageListContentHeight += packageListRowHeight(
                            package_registry::project_resource_purpose(resource.id));
            if (!projectAiResources)
                for (const auto& entry : catalogEntries)
                    if (visible_in_catalog(entry))
                        packageListContentHeight += packageListRowHeight(entry.summary);
            if (!projectAiResources)
                for (const auto& package : resources)
                    if (visible_local_package(package)) packageListContentHeight += packageListRowHeight(package.summary);
            const bool selectedResource = projectAiResources && selectedPackage;
            const auto selectedPlan = selectedResource ? ai::model_install::plan_for(selectedPackage->id)
                : std::optional<ai::model_install::InstallPlan>{};
            const bool selectedInstalled = selectedPackage && package_installed(*selectedPackage);
            const bool readyBuiltin = selectedPackage && builtin_project_package_ready(selectedPackage->id);
            const bool sourceInstallUnavailable = selectedPackage
                && (selectedPackage->kind == package_registry::PackageKind::DownloadableSource
                    || selectedPackage->kind == package_registry::PackageKind::ResearchPrototype);
            const std::string selectedName = selectedCatalog ? selectedCatalog->display_name
                : (selectedPackage ? std::string(selectedPackage->displayName) : std::string("None"));
            const std::string purpose = selectedResource
                ? std::string(package_registry::project_resource_purpose(selectedPackage->id))
                : (selectedCatalog ? selectedCatalog->summary
                    : selectedPackage ? std::string(selectedPackage->summary) : std::string("Select a project package."));
            const std::string destination = selectedResource
                ? "Shared cache/models; only a reference belongs in project assets/packages. Weights are not bundled."
                : (editor.projectRoot.empty() ? "Open a project before adding project files."
                    : display_project_path(projectRoot / "assets" / "packages"));
            const std::string requirement = selectedResource
                ? (selectedPlan ? (editor.projectRoot.empty()
                        ? "Open a project before downloading this project resource."
                        : "Download available. The download starts only when you choose it.")
                    : "Download and image generation are not available for this resource yet.")
                : (readyBuiltin ? (selectedInstalled ? "Added to this project. Its authoring assets remain project-local."
                        : editor.projectRoot.empty() ? "Create a new project with this built-in package. Nothing runs automatically."
                            : "Add this built-in package to the open project. Nothing runs automatically.")
                    : sourceInstallUnavailable ? (selectedPackage->id == "research_planetary_terrain"
                        ? "New Planet projects include Water (FFT Ocean). Review Planet source first; its Add action then opens the exact Water source approval before saving settings or building. Existing Ocean Off settings stay dry. Nothing builds automatically."
                        : selectedPackage->id == "research_fft_ocean"
                            ? "Review and add Water (FFT Ocean) source to the open project. Planet includes this same package automatically when Ocean is On."
                            : "Source review is available. Project activation for this extension is not implemented yet.")
                    : (!selectedCatalog ? "Choose a package."
                        : (!selectedPackage ? "This package is listed but cannot be added yet."
                            : (selectedCatalog->availability == package_catalog::Availability::descriptor_only
                                ? "This package is listed for reference. Installation is not available yet."
                                : (selectedPackage->requiresExplicitNetworkApproval
                                    ? "This package needs permission before it can run a network service."
                                    : (selectedInstalled ? "Added to this project."
                                        : editor.projectRoot.empty() ? "Open a project before adding this package."
                                            : "Ready to add to the open project."))))));
            const bool canInstall = selectedPackage && (!editor.projectRoot.empty() || sourceInstallUnavailable || readyBuiltin)
                && !package_source_task_busy(editor) && !planetary_project_busy(editor)
                && (selectedResource ? selectedPlan.has_value()
                    : (readyBuiltin || sourceInstallUnavailable || (selectedCatalog && selectedCatalog->availability == package_catalog::Availability::available
                        && !selectedPackage->requiresExplicitNetworkApproval)));
            const std::array packageActions{
                gui::InlineButtonSpec{.label=selectedInstalled ? "Remove Project Reference"
                    : (readyBuiltin && editor.projectRoot.empty()
                        ? (selectedPackage->id == package_registry::kEngineForestFactoryPackageId ? "Create Forest Project" : "Create Arcade Project")
                        : (selectedResource ? "Download Project Resource" : sourceInstallUnavailable ? "Review Extension" : "Add to Project")),
                    .width=0.0f, .enabled=canInstall},
                gui::InlineButtonSpec{.label="Close", .width=0.0f}
            };
            const float packageFooterHeight = gui::measure_wrapped_label_height(requirement, contentWidth)
                + gui::inline_button_row_height(packageActions, 30.0f, 6.0f, packageActionLayout)
                + 12.0f + (editor.packageModelInstall ? 68.0f : 0.0f)
                + (editor.packageInstallStatus.empty() ? 0.0f : gui::measure_wrapped_label_height(editor.packageInstallStatus, contentWidth));
            gui::section_heading(projectAiResources ? "Project image resources" : "Available extensions");
            const auto packageBodyLayout = editor_update_modal::measure_package_manager_body(
                gui::cursor_position().y, packageContentBottom,
                gui::measure_section_heading_height(selectedResource
                    ? "Selected image resource" : "Selected extension", contentWidth), packageFooterHeight, contentWidth);
            const gui::Vec2 packageBodyOrigin = gui::cursor_position();
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = projectAiResources ? "package-manager-ai-resource-list" : "package-manager-package-list",
                .size = { packageBodyLayout.listWidth, packageBodyLayout.listHeight },
                .content_height = (std::max)(packageBodyLayout.listHeight, packageListContentHeight),
                .draw_background = true, .show_scrollbar = true
            });
            if (visibleCount == 0)
                gui::wrapped_label(projectAiResources ? "No project image resources match this search."
                    : "No project packages match this search; refresh the catalog if it is unavailable.", packageListTextWidth);
            if (projectAiResources)
            {
                for (const auto& resource : resources)
                {
                    if (!visible_resource(resource)) continue;
                    const auto row = gui::cursor_position();
                    if (gui::menu_item({.label=resource.displayName,
                            .size={packageListTextWidth, gui::menu_item_row_height()},
                            .tone=gui::SemanticTone::info,
                            .selected=selectedPackage && selectedPackage->id == resource.id}))
                        select_package(resource);
                    gui::wrapped_label(package_registry::project_resource_purpose(resource.id), packageListTextWidth);
                    gui::set_cursor(row);
                    gui::advance_cursor({0.0f, packageListRowHeight(
                        package_registry::project_resource_purpose(resource.id))});
                }
            }
            else
            {
                for (const auto& entry : catalogEntries)
                {
                    if (!visible_in_catalog(entry)) continue;
                    const auto row = gui::cursor_position();
                    if (gui::menu_item({.label=entry.display_name,
                            .size={packageListTextWidth, gui::menu_item_row_height()},
                            .tone=gui::SemanticTone::info,
                            .selected=selectedCatalog && selectedCatalog->id == entry.id}))
                        select_catalog_entry(entry);
                    gui::wrapped_label(entry.summary, packageListTextWidth);
                    gui::set_cursor(row);
                    gui::advance_cursor({0.0f, packageListRowHeight(entry.summary)});
                }
            }
                for (const auto& package : resources)
                {
                    if (!visible_local_package(package)) continue;
                    const auto row = gui::cursor_position();
                    if (gui::menu_item({.label=package.displayName,
                            .size={packageListTextWidth, gui::menu_item_row_height()},
                            .tone=gui::SemanticTone::info,
                            .selected=selectedPackage && selectedPackage->id == package.id})) select_package(package);
                    gui::wrapped_label(package.summary, packageListTextWidth);
                    gui::set_cursor(row);
                    gui::advance_cursor({0.0f, packageListRowHeight(package.summary)});
                }
            gui::end_scroll_area();

            if (packageBodyLayout.showDetails)
            {
                if (packageBodyLayout.sideBySide)
                    gui::set_cursor({packageBodyOrigin.x + packageBodyLayout.detailOffsetX, packageBodyOrigin.y});
                const float packageDetailWidth = packageBodyLayout.detailWidth;
                auto packageDetailActions = packageActionLayout;
                packageDetailActions.available_width = packageDetailWidth - 24.0f;
                gui::section_heading(selectedResource ? "Selected image resource" : "Selected extension");
                (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                    .id = "package-manager-detail-scroll",
                    .size = {packageDetailWidth, (std::max)(1.0f, packageBodyLayout.detailHeight
                        - (packageBodyLayout.sideBySide ? gui::measure_section_heading_height(selectedResource
                            ? "Selected image resource" : "Selected extension", packageDetailWidth) : 0.0f))},
                    .content_height = 0.0f, .draw_background = true, .show_scrollbar = true
                });
                gui::section_heading(selectedName, gui::SemanticTone::success);
                gui::wrapped_label(purpose, packageDetailWidth - 24.0f);
                gui::wrapped_label(requirement, packageDetailWidth - 24.0f);
                if (gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label=editor.packageTechnicalDetailsOpen
                            ? "Hide Technical Details" : "Technical Details", .width=0.0f}},
                        28.0f, 6.0f, packageDetailActions))
                    editor.packageTechnicalDetailsOpen = !editor.packageTechnicalDetailsOpen;
                if (editor.packageTechnicalDetailsOpen)
                {
                    std::vector<std::string> details{
                        "Catalog: " + editor.packageCatalogStatus,
                        "Destination: " + destination};
                    if (selectedCatalog)
                    {
                        details.push_back("Source: " + selectedCatalog->source_url);
                        details.push_back("Revision: " + selectedCatalog->immutable_revision);
                        details.push_back("License: " + selectedCatalog->license_url);
                    }
                    else if (selectedPackage)
                        details.push_back("Source: " + std::string(selectedPackage->externalSourceRepo));
                    (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                        .id="package-manager-technical-details", .size={packageDetailWidth - 24.0f,150.0f},
                        .lines=details, .max_line_chars=4096u, .selectable=true,
                        .stick_to_bottom=false, .wrap_lines=true});
                }
                gui::end_scroll_area();
            }
            gui::set_cursor({modalPos.x + 28.0f, packageBodyLayout.footerTop});
            // Requirements remain visible before any action, even when the
            // compressed modal cannot fit the optional details pane.
            gui::wrapped_label(requirement, contentWidth);
            if (!editor.packageInstallStatus.empty()) gui::wrapped_label(editor.packageInstallStatus, contentWidth);
            if (const auto action = gui::inline_button_row(packageActions, 30.0f, 6.0f, packageActionLayout))
            {
                if (*action == 1u)
                    editor.showPackageManagerModal = false;
                else if (selectedPackage)
                {
                    if (selectedInstalled) remove_package(*selectedPackage);
                    else install_package(*selectedPackage);
                }
            }
            if (editor.packageModelInstall)
            {
                gui::progress_bar(gui::ProgressBarOptions{
                    .label="Project resource download", .status=editor.packageInstallStatus,
                    .value=editor.packageInstallProgress, .size={contentWidth,20.0f}, .show_percent=true});
                if (!editor.packageModelInstall->publishPending
                    && gui::button("Cancel Download", {148.0f,28.0f})
                    && editor.packageModelInstall->process.valid())
                    (void)platform::child_process::stop(editor.packageModelInstall->process,
                        platform::child_process::StopMode::graceful);
            }
            }
            }
            gui::end_modal_window();
        }

        if (editor.showAboutModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize = aboutModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "About Epoch",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            const float footerY = modalPos.y + modalSize.y - 46.0f;
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = "editor.about-body", .size = {contentWidth, (std::max)(32.0f, footerY - contentPos.y - 12.0f)},
                .draw_background = false, .show_scrollbar = true});
            gui::label("Epoch Editor");
            const float bodyWidth = (std::max)(1.0f, contentWidth - 22.0f);
            gui::wrapped_label(std::string("Version: ") + epochengine::GetEngineDisplayString(), bodyWidth);
            gui::wrapped_label(updater::PUBLIC_DISTRIBUTION
                ? "Distribution: Public | GitHub"
                : "Distribution: Development | Private GPT Site source", bodyWidth);
            gui::wrapped_label(std::string("Built: ") + __DATE__ " " __TIME__
                + " | " + std::string(build_configuration_label()), bodyWidth);
            gui::wrapped_label("Multi-backend engine/editor shell with project-driven scene play, docked scripting, and engine-owned tools.", bodyWidth);
            gui::wrapped_label(std::string("Renderer: ") + renderer_name(ctx), bodyWidth);
            gui::wrapped_label(std::string("Project: ") + editor.projectName, bodyWidth);
            gui::wrapped_label(
                "Update Epoch checks for updates to this edition. Project Source downloads its source without replacing this engine.", bodyWidth);
            gui::end_scroll_area();
            const gui::Vec2 buttonRow{contentPos.x, footerY};
            const float buttonGap = 6.0f;
            const float buttonWidth = (std::max)(1.0f, (contentWidth - 2.0f * buttonGap) / 3.0f);
            gui::set_cursor(buttonRow);
            if (gui::button("Update Epoch", {buttonWidth, 30.0f}))
            {
                editor.showAboutModal = false;
                editor.showUpdateConfirmModal = true;
                editor.showSourceUpdateConfirmModal = false;
                push_editor_log(editor, "[update] Update modal opened from About.");
                if (editor.updateState != EditorUpdateState::Available
                    && editor.updateState != EditorUpdateState::RestartReady
                    && !editor.updateCheckPending.has_value()
                    && !updater::source_update_worker_active())
                {
                    start_editor_update_check(editor);
                }
                else if (updater::source_update_worker_active())
                {
                    adopt_editor_source_update_worker(editor);
                }
            }
            gui::set_cursor({buttonRow.x + buttonWidth + buttonGap, buttonRow.y});
            if (gui::button("Project Source", {buttonWidth, 30.0f}))
            {
                editor.showAboutModal = false;
                editor.showUpdateConfirmModal = false;
                editor.showSourceUpdateConfirmModal = true;
                editor.updateStatus = "Project Source Code Download stores the latest source snapshot in the executable-local project source cache.";
                push_editor_log(editor, "[update] Project source code download modal opened from About.");
            }
            gui::set_cursor({buttonRow.x + 2.0f * (buttonWidth + buttonGap), buttonRow.y});
            if (gui::button("Close", {buttonWidth, 30.0f}))
                editor.showAboutModal = false;
            gui::end_modal_window();
        }
