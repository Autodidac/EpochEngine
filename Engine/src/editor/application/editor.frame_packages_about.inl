// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.showPackageManagerModal)
        {
            editor.openMenu = TopMenu::None;
            request_site_package_catalog(editor, false);
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
                if (package.id == epochengine::package_registry::kEngineForestFactoryPackageId)
                {
                    (void)stage_forest_factory_package_opt_in(editor, package);
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
                    editor.packageInstallStatus = "Source archive installation is not connected in this build. No download or build started.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[package] Source installation unavailable for ") + std::string(package.id) + ".");
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
                .title = "Project Package Manager",
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
            const std::array packageTabs{
                gui::TabButtonSpec{.id="package-manager.addons", .label="Project Packages",
                    .width=152.0f, .active=!editor.packageCatalogModelsOnly},
                gui::TabButtonSpec{.id="package-manager.ai-resources", .label="Project AI Resources",
                    .width=180.0f, .active=editor.packageCatalogModelsOnly}
            };
            if (const auto tabs = gui::tab_bar_buttons(packageTabs, 29.0f, 5.0f);
                tabs.selected_index)
                editor.packageCatalogModelsOnly = *tabs.selected_index == 1u;
            const bool projectAiResources = editor.packageCatalogModelsOnly;
            gui::wrapped_label(projectAiResources
                ? "Optional image-generation resources for project assets. These do not select or load the Assistant's inference model."
                : "Software add-ons and dependencies for the active project.",
                contentWidth);
            const std::array managerActions{
                gui::InlineButtonSpec{.label="Model Manager", .width=0.0f},
                gui::InlineButtonSpec{.label=editor.packageCatalogPending ? "Refreshing..." : "Refresh Catalog",
                    .width=0.0f, .enabled=!editor.packageCatalogPending}
            };
            if (const auto action = gui::inline_button_row(managerActions, 28.0f, 6.0f, packageActionLayout))
            {
                if (*action == 0u)
                {
                    editor.showPackageManagerModal = false;
                    editor.showAiModelManagerModal = true;
                }
                else
                    request_site_package_catalog(editor, true);
            }
            gui::wrapped_label(projectAiResources
                ? "Chat/coding weights, inference runtimes and provider connections belong in Model Manager. Project resources below never start a model."
                : editor.packageCatalogPending ? "Refreshing the package list..."
                    : catalogEntries.empty() ? "The package list is unavailable. Choose Refresh Catalog to try again."
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
                    && matches_search(entry.id, entry.display_name, entry.summary);
            };
            const auto visible_resource = [&](const package_registry::PackageDescriptor& resource)
            {
                return projectAiResources
                    && package_registry::catalog_section(resource.id) == package_registry::CatalogSection::ProjectAiResources
                    && matches_search(resource.id, resource.displayName, resource.summary);
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
                selectedCatalog = firstCatalog == catalogEntries.end() ? nullptr : &*firstCatalog;
                selectedPackage = selectedCatalog ? package_registry::find(selectedCatalog->id) : nullptr;
                editor.selectedPackageId = selectedCatalog ? selectedCatalog->id : std::string{};
            }
            const auto visibleCount = projectAiResources
                ? std::count_if(resources.begin(), resources.end(), visible_resource)
                : std::count_if(catalogEntries.begin(), catalogEntries.end(), visible_in_catalog);
            const float packageListTextWidth = (std::max)(1.0f, contentWidth - 24.0f);
            const auto packageListRowHeight = [&](std::string_view summary)
            {
                return 28.0f + gui::measure_wrapped_label_height(summary, packageListTextWidth) + 12.0f;
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
            const bool selectedResource = projectAiResources && selectedPackage;
            const auto selectedPlan = selectedResource ? ai::model_install::plan_for(selectedPackage->id)
                : std::optional<ai::model_install::InstallPlan>{};
            const bool selectedInstalled = selectedPackage && package_installed(*selectedPackage);
            const bool sourceInstallUnavailable = selectedPackage
                && (selectedPackage->kind == package_registry::PackageKind::DownloadableSource
                    || selectedPackage->kind == package_registry::PackageKind::ResearchPrototype);
            const std::string selectedName = selectedCatalog ? selectedCatalog->display_name
                : (selectedPackage ? std::string(selectedPackage->displayName) : std::string("None"));
            const std::string purpose = selectedResource
                ? std::string(package_registry::project_resource_purpose(selectedPackage->id))
                : (selectedCatalog ? selectedCatalog->summary : std::string("Select a project package."));
            const std::string destination = selectedResource
                ? "Shared cache/models; only a reference belongs in project assets/packages. Weights are not bundled."
                : (editor.projectRoot.empty() ? "Open a project before adding project files."
                    : display_project_path(projectRoot / "assets" / "packages"));
            const std::string requirement = selectedResource
                ? (selectedPlan ? (editor.projectRoot.empty()
                        ? "Open a project before downloading this project resource."
                        : "Download available. The download starts only when you choose it.")
                    : "Download and image generation are not available for this resource yet.")
                : (sourceInstallUnavailable ? "Installation is not available in this build yet."
                    : (!selectedCatalog ? "Choose a package."
                        : (!selectedPackage ? "This package is listed but cannot be added yet."
                            : (selectedCatalog->availability == package_catalog::Availability::descriptor_only
                                ? "This package is listed for reference. Installation is not available yet."
                                : (selectedPackage->requiresExplicitNetworkApproval
                                    ? "This package needs permission before it can run a network service."
                                    : (selectedInstalled ? "Added to this project."
                                        : editor.projectRoot.empty() ? "Open a project before adding this package."
                                            : "Ready to add to the open project."))))));
            const bool canInstall = !editor.projectRoot.empty() && selectedPackage
                && (selectedResource ? selectedPlan.has_value()
                    : (selectedCatalog && selectedCatalog->availability == package_catalog::Availability::available
                        && !sourceInstallUnavailable && !selectedPackage->requiresExplicitNetworkApproval));
            const std::array packageActions{
                gui::InlineButtonSpec{.label=selectedInstalled ? "Remove Project Reference"
                    : (selectedResource ? "Download Project Resource" : "Add to Project"),
                    .width=0.0f, .enabled=canInstall},
                gui::InlineButtonSpec{.label="Close", .width=0.0f}
            };
            const float packageFooterHeight = gui::measure_wrapped_label_height(requirement, contentWidth)
                + gui::inline_button_row_height(packageActions, 30.0f, 6.0f, packageActionLayout)
                + 12.0f + (editor.packageModelInstall ? 68.0f : 0.0f);
            gui::label(projectAiResources ? "Project image resources" : "Project packages");
            const auto packageBodyLayout = editor_update_modal::measure_package_manager_body(
                gui::cursor_position().y, packageContentBottom, gui::line_height(), packageFooterHeight);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = projectAiResources ? "package-manager-ai-resource-list" : "package-manager-package-list",
                .size = { contentWidth, packageBodyLayout.listHeight },
                .content_height = (std::max)(packageBodyLayout.listHeight, packageListContentHeight),
                .draw_background = true, .show_scrollbar = true
            });
            if (visibleCount == 0)
                gui::wrapped_label(projectAiResources ? "No project image resources match this search."
                    : "No project packages match this search; refresh the catalog if it is unavailable.", contentWidth - 24.0f);
            if (projectAiResources)
            {
                for (const auto& resource : resources)
                {
                    if (!visible_resource(resource)) continue;
                    const auto row = gui::cursor_position();
                    if (gui::text_link(resource.displayName, {contentWidth - 24.0f, 28.0f},
                            selectedPackage && selectedPackage->id == resource.id))
                        select_package(resource);
                    gui::wrapped_label(package_registry::project_resource_purpose(resource.id), contentWidth - 24.0f);
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
                    if (gui::text_link(entry.display_name, {contentWidth - 24.0f, 28.0f},
                            selectedCatalog && selectedCatalog->id == entry.id))
                        select_catalog_entry(entry);
                    gui::wrapped_label(entry.summary, contentWidth - 24.0f);
                    gui::set_cursor(row);
                    gui::advance_cursor({0.0f, packageListRowHeight(entry.summary)});
                }
            }
            gui::end_scroll_area();

            if (packageBodyLayout.showDetails)
            {
                gui::label(selectedResource ? "Selected image resource" : "Selected project package");
                (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                    .id = "package-manager-detail-scroll",
                    .size = {contentWidth, packageBodyLayout.detailHeight},
                    .content_height = 0.0f, .draw_background = true, .show_scrollbar = true
                });
                gui::wrapped_label(selectedName, contentWidth - 24.0f);
                gui::wrapped_label(purpose, contentWidth - 24.0f);
                gui::wrapped_label(requirement, contentWidth - 24.0f);
                if (gui::inline_button_row(std::array{
                        gui::InlineButtonSpec{.label=editor.packageTechnicalDetailsOpen
                            ? "Hide Technical Details" : "Technical Details", .width=0.0f}},
                        28.0f, 6.0f, packageActionLayout))
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
                        .id="package-manager-technical-details", .size={contentWidth - 24.0f,150.0f},
                        .lines=details, .max_line_chars=4096u, .selectable=true,
                        .stick_to_bottom=false, .wrap_lines=true});
                }
                gui::end_scroll_area();
            }
            gui::set_cursor({modalPos.x + 28.0f, packageBodyLayout.footerTop});
            // Requirements remain visible before any action, even when the
            // compressed modal cannot fit the optional details pane.
            gui::wrapped_label(requirement, contentWidth);
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
