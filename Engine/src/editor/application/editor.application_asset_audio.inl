// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::uint64_t asset_graph_id(
            std::string_view value) noexcept
        {
            std::uint64_t hash = 1'469'598'103'934'665'603ull;
            for (const unsigned char byte : value)
            {
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= 1'099'511'628'211ull;
            }
            return hash == 0u ? 1u : hash;
        }

        void draw_asset_dependency_graph(
            EditorState& editor,
            float width)
        {
            struct GraphSelection final
            {
                std::uint64_t id{};
                std::string source_path{};
                std::string logical_path{};
                bool compiled_texture{};
            };

            auto entries = collect_asset_manager_entries(
                editor.projectRoot,
                editor.assetBrowserFolder,
                64u);
            auto* textures = project_texture_controller(editor);
            const auto catalog = textures
                ? textures->catalog()
                : std::span<
                    const editor_project_textures::TextureCatalogEntry>{};

            constexpr std::size_t maximumSourceNodes = 8u;
            constexpr std::size_t maximumTextureNodes = 8u;
            const std::size_t sourceCount = (std::min)(
                entries.size(), maximumSourceNodes);
            const std::size_t textureCount = (std::min)(
                catalog.size(), maximumTextureNodes);
            const std::size_t nodeCapacity =
                1u + sourceCount + textureCount;

            std::vector<std::string> subtitles{};
            std::vector<gui::NodeGraphCanvasNode> nodes{};
            std::vector<gui::NodeGraphCanvasEdge> edges{};
            std::vector<GraphSelection> selections{};
            subtitles.reserve(nodeCapacity);
            nodes.reserve(nodeCapacity);
            edges.reserve(sourceCount + textureCount);
            selections.reserve(sourceCount + textureCount);

            const float nodeWidth = (std::clamp)(
                (width - 80.0f) / 3.0f,
                104.0f,
                176.0f);
            constexpr float nodeHeight = 50.0f;
            constexpr float rowGap = 12.0f;
            const float sourceX = 20.0f + nodeWidth + 24.0f;
            const float compiledX = sourceX + nodeWidth + 24.0f;
            const std::uint64_t rootId = asset_graph_id(
                std::string{"project:"} + editor.projectId);
            subtitles.push_back(epochengine::format_text(
                "{} source entries | {} compiled textures",
                entries.size(),
                catalog.size()));
            nodes.push_back({
                .id = rootId,
                .title = editor.projectName,
                .subtitle = subtitles.back(),
                .position = {20.0f, 20.0f},
                .size = {nodeWidth, nodeHeight},
                .role = gui::NodeGraphNodeRole::document,
                .selected = false,
                .enabled = true
            });

            const auto addEdge = [&](
                std::uint64_t source,
                std::uint64_t target,
                gui::Vec2 from,
                gui::Vec2 to)
            {
                const std::string edgeKey = epochengine::format_text(
                    "{}->{}", source, target);
                edges.push_back({
                    .id = asset_graph_id(edgeKey),
                    .source_node = source,
                    .target_node = target,
                    .source = from,
                    .target = to,
                    .enabled = true
                });
            };

            for (std::size_t index = 0u;
                index < sourceCount;
                ++index)
            {
                const auto& entry = entries[index];
                const std::uint64_t id = asset_graph_id(
                    std::string{"source:"} + entry.path);
                const float y = 20.0f
                    + static_cast<float>(index)
                        * (nodeHeight + rowGap);
                subtitles.push_back(entry.directory
                    ? std::string{"Folder"}
                    : epochengine::format_text(
                        "{} | {} bytes",
                        entry.kind,
                        entry.size));
                nodes.push_back({
                    .id = id,
                    .title = entry.label,
                    .subtitle = subtitles.back(),
                    .position = {sourceX, y},
                    .size = {nodeWidth, nodeHeight},
                    .role = entry.directory
                        ? gui::NodeGraphNodeRole::container
                        : gui::NodeGraphNodeRole::control,
                    .selected =
                        editor.selectedAssetPath == entry.path,
                    .enabled = true
                });
                selections.push_back({
                    .id = id,
                    .source_path = entry.path
                });
                addEdge(
                    rootId,
                    id,
                    {20.0f + nodeWidth, 20.0f + nodeHeight * 0.5f},
                    {sourceX, y + nodeHeight * 0.5f});
            }

            for (std::size_t index = 0u;
                index < textureCount;
                ++index)
            {
                const auto& entry = catalog[index];
                const std::uint64_t id = asset_graph_id(
                    std::string{"texture:"} + entry.logical_path);
                const float y = 20.0f
                    + static_cast<float>(index)
                        * (nodeHeight + rowGap);
                subtitles.push_back(epochengine::format_text(
                    "{}x{} | rev {}",
                    entry.width,
                    entry.height,
                    entry.source_sequence));
                nodes.push_back({
                    .id = id,
                    .title = entry.logical_path,
                    .subtitle = subtitles.back(),
                    .position = {compiledX, y},
                    .size = {nodeWidth, nodeHeight},
                    .role = gui::NodeGraphNodeRole::action,
                    .selected = textures
                        && textures->selected_logical_path()
                            == entry.logical_path,
                    .enabled = true
                });
                selections.push_back({
                    .id = id,
                    .source_path =
                        display_project_path(entry.source_path),
                    .logical_path = entry.logical_path,
                    .compiled_texture = true
                });

                std::uint64_t sourceId = rootId;
                gui::Vec2 sourcePoint{
                    20.0f + nodeWidth,
                    20.0f + nodeHeight * 0.5f};
                const std::string sourcePath =
                    display_project_path(entry.source_path);
                const auto source = std::find_if(
                    selections.begin(),
                    selections.end(),
                    [&](const GraphSelection& candidate)
                    {
                        return !candidate.compiled_texture
                            && candidate.source_path == sourcePath;
                    });
                if (source != selections.end())
                {
                    sourceId = source->id;
                    const auto sourceNode = std::find_if(
                        nodes.begin(),
                        nodes.end(),
                        [&](const gui::NodeGraphCanvasNode& node)
                        {
                            return node.id == sourceId;
                        });
                    if (sourceNode != nodes.end())
                    {
                        sourcePoint = {
                            sourceNode->position.x
                                + sourceNode->size.x,
                            sourceNode->position.y
                                + sourceNode->size.y * 0.5f};
                    }
                }
                addEdge(
                    sourceId,
                    id,
                    sourcePoint,
                    {compiledX, y + nodeHeight * 0.5f});
            }

            gui::property_row(
                "Graph",
                epochengine::format_text(
                    "{} nodes | {} dependencies",
                    nodes.size(),
                    edges.size()),
                74.0f);
            if (entries.size() > sourceCount
                || catalog.size() > textureCount)
            {
                gui::wrapped_label(
                    epochengine::format_text(
                        "Showing {} of {} source entries and {} of {} "
                        "compiled textures. Narrow the folder to inspect "
                        "the remaining dependencies.",
                        sourceCount,
                        entries.size(),
                        textureCount,
                        catalog.size()),
                    width);
            }

            const float rows = static_cast<float>((std::max)(
                sourceCount, textureCount));
            const float graphHeight = (std::clamp)(
                46.0f + rows * (nodeHeight + rowGap),
                250.0f,
                540.0f);
            const auto graph = gui::node_graph_canvas({
                .id = "assets.dependency_graph",
                .size = {width, graphHeight},
                .nodes = nodes,
                .edges = edges,
                .grid_step = 24.0f,
                .interactive = true,
                .enabled = true
            });
            if (!graph.clicked_node)
                return;

            const auto selection = std::find_if(
                selections.begin(),
                selections.end(),
                [&](const GraphSelection& candidate)
                {
                    return candidate.id == *graph.clicked_node;
                });
            if (selection == selections.end())
                return;

            editor.selectedAssetPath = selection->source_path;
            editor.selectedProjectFile = selection->source_path;
            if (selection->compiled_texture && textures)
            {
                const auto selected =
                    textures->select(selection->logical_path);
                editor.projectTextureStatus = selected
                    ? "Selected compiled texture from the asset graph."
                    : epochengine::format_text(
                        "Texture graph selection failed: {}.",
                        editor_project_textures::controller_code_name(
                            selected.code));
                editor.assetWorkspaceSection = 1u;
            }
        }
        struct ProjectAudioGainChoice final
        {
            std::string_view label{};
            float value{};
        };

        inline constexpr std::array<ProjectAudioGainChoice, 9>
            kProjectAudioGainChoices{{
                {"0%", 0.0f},
                {"25%", 0.25f},
                {"50%", 0.5f},
                {"75%", 0.75f},
                {"85%", 0.85f},
                {"100%", 1.0f},
                {"125%", 1.25f},
                {"150%", 1.5f},
                {"200%", 2.0f}
            }};

        inline constexpr std::array<EditorProjectAudioCueSemantic, 6>
            kProjectAudioCueSemantics{{
                EditorProjectAudioCueSemantic::custom,
                EditorProjectAudioCueSemantic::jump,
                EditorProjectAudioCueSemantic::land,
                EditorProjectAudioCueSemantic::music,
                EditorProjectAudioCueSemantic::ambient,
                EditorProjectAudioCueSemantic::user_interface
            }};

        inline constexpr std::array<std::string_view, 6>
            kProjectAudioCueSemanticLabels{{
                "Custom",
                "Jump Event",
                "Land Event",
                "Music",
                "Ambient",
                "User Interface"
            }};

        [[nodiscard]] std::string project_audio_gain_label(float gain)
        {
            const auto found = std::find_if(
                kProjectAudioGainChoices.begin(),
                kProjectAudioGainChoices.end(),
                [gain](const ProjectAudioGainChoice& choice) noexcept
                {
                    return std::abs(choice.value - gain) <= 0.0001f;
                });
            if (found != kProjectAudioGainChoices.end())
                return std::string{found->label};
            return epochengine::format_text("{:.0f}%", gain * 100.0f);
        }

        [[nodiscard]] std::string_view project_audio_semantic_label(
            EditorProjectAudioCueSemantic semantic) noexcept
        {
            for (std::size_t index{};
                index < kProjectAudioCueSemantics.size();
                ++index)
            {
                if (kProjectAudioCueSemantics[index] == semantic)
                    return kProjectAudioCueSemanticLabels[index];
            }
            return "Custom";
        }

        void report_project_audio_update(
            EditorState& editor,
            const EditorProjectAudioUpdateResult& update)
        {
            editor.projectAudioStatus = update.summary;
            push_editor_log(
                editor,
                std::string{update.succeeded
                    ? "[audio] "
                    : "[audio][ERROR] "}
                    + update.summary);
            if (update.stable_id != 0u)
                editor.selectedProjectAudioCueId = update.stable_id;
        }

        void draw_project_audio_workspace(
            EditorState& editor,
            float width)
        {
            auto summary = editor_project_audio_profile_summary(
                editor.projectId);
            gui::label("Project Audio");
            gui::property_row(
                "[audio] Source",
                summary.source_path.empty()
                    ? std::string{"Not declared"}
                    : summary.source_path,
                126.0f);
            gui::property_row(
                "[audio] Revision",
                std::to_string(summary.sequence),
                126.0f);
            gui::property_row(
                "[audio] Profile",
                summary.source_materialized
                    ? std::string{"Saved source"}
                    : std::string{"Default draft"},
                126.0f);
            const std::string artifactState =
                !summary.artifact_materialized
                    ? std::string{"Missing"}
                    : (summary.artifact_current
                        ? std::string{"Current"}
                        : std::string{"Stale"});
            gui::property_row(
                "[audio] Artifact",
                artifactState,
                126.0f);
            gui::property_row(
                "[audio] Library",
                summary.artifact_path.empty()
                    ? std::string{"Not declared"}
                    : summary.artifact_path,
                126.0f);
            gui::property_row(
                "[audio] Resources",
                epochengine::format_text(
                    "{} cues | {} buses | {} KiB",
                    summary.cue_count,
                    summary.bus_count,
                    (summary.source_bytes + 1023u) / 1024u),
                126.0f);
            gui::property_row(
                "[audio] Missing",
                std::to_string(summary.missing_source_count),
                126.0f);
            gui::wrapped_label(
                summary.diagnostic.empty()
                    ? std::string{"Project audio is unavailable."}
                    : summary.diagnostic,
                width);

            const std::array audioActions{
                gui::InlineButtonSpec{
                    .label = "Import WAV",
                    .width = 116.0f},
                gui::InlineButtonSpec{
                    .label = "Validate Decode",
                    .width = 138.0f},
                gui::InlineButtonSpec{
                    .label = "Restore Defaults",
                    .width = 144.0f}
            };
            if (const auto action = gui::inline_button_row(
                    audioActions, 30.0f, 8.0f))
            {
                if (*action == 0u)
                {
                    const std::filesystem::path initialDirectory =
                        resolve_editor_path(editor.projectRoot)
                        / "Assets" / "Audio";
                    const auto selected =
                        platform::filesystem::open_file_dialog(
                            L"Import Project Audio",
                            L"WAV audio",
                            L"*.wav",
                            initialDirectory);
                    if (selected.accepted)
                    {
                        report_project_audio_update(
                            editor,
                            editor_import_project_audio_cue(
                                editor.projectId,
                                selected.path.string()));
                    }
                    else if (!selected.cancelled)
                    {
                        editor.projectAudioStatus = selected.diagnostic;
                        push_editor_log(
                            editor,
                            "[audio][ERROR] " + selected.diagnostic);
                    }
                }
                else if (*action == 1u)
                {
                    report_project_audio_update(
                        editor,
                        editor_validate_project_audio_profile(
                            editor.projectId));
                }
                else
                {
                    report_project_audio_update(
                        editor,
                        editor_reset_project_audio_profile(
                            editor.projectId));
                }
                summary = editor_project_audio_profile_summary(
                    editor.projectId);
            }

            gui::wrapped_label(editor.projectAudioStatus, width);
            if (!summary.ready)
                return;

            std::array<std::string_view,
                kProjectAudioGainChoices.size()> gainLabels{};
            for (std::size_t index{};
                index < kProjectAudioGainChoices.size();
                ++index)
            {
                gainLabels[index] =
                    kProjectAudioGainChoices[index].label;
            }

            gui::label("Mix Buses");
            for (const auto& bus : summary.buses)
            {
                gui::property_row(
                    "[bus] " + bus.name,
                    bus.parent_name,
                    126.0f);
                float editedGain = bus.gain;
                const std::string selectedGain =
                    project_audio_gain_label(bus.gain);
                const auto gainSelect = gui::select_box(
                    gui::SelectBoxOptions{
                        .id = "project-audio-bus-gain-"
                            + std::to_string(bus.stable_id),
                        .placeholder = "Bus volume",
                        .selected = selectedGain,
                        .options = std::span<const std::string_view>{
                            gainLabels.data(), gainLabels.size()},
                        .size = {
                            (std::min)(width, 280.0f), 30.0f},
                        .row_height = 27.0f,
                        .max_visible_options = 9});
                if (gainSelect.changed
                    && gainSelect.selected_index
                    && *gainSelect.selected_index
                        < kProjectAudioGainChoices.size())
                {
                    editedGain = kProjectAudioGainChoices[
                        *gainSelect.selected_index].value;
                    report_project_audio_update(
                        editor,
                        editor_set_project_audio_bus_mix(
                            editor.projectId,
                            bus.stable_id,
                            editedGain,
                            bus.muted));
                }
                bool muted = bus.muted;
                if (gui::toggle_switch(
                        "Mute " + bus.name,
                        muted,
                        {(std::min)(width, 280.0f), 27.0f}))
                {
                    report_project_audio_update(
                        editor,
                        editor_set_project_audio_bus_mix(
                            editor.projectId,
                            bus.stable_id,
                            editedGain,
                            muted));
                }
            }

            if (summary.cues.empty())
                return;
            auto selectedCue = std::find_if(
                summary.cues.begin(), summary.cues.end(),
                [&editor](const EditorProjectAudioCueSummary& cue)
                {
                    return cue.stable_id
                        == editor.selectedProjectAudioCueId;
                });
            if (selectedCue == summary.cues.end())
            {
                editor.selectedProjectAudioCueId =
                    summary.cues.front().stable_id;
                selectedCue = summary.cues.begin();
            }

            std::vector<std::string_view> cueLabels{};
            cueLabels.reserve(summary.cues.size());
            for (const auto& cue : summary.cues)
                cueLabels.emplace_back(cue.name);
            const auto cueSelect = gui::select_box(
                gui::SelectBoxOptions{
                    .id = "project-audio-cue-select",
                    .placeholder = "Choose audio cue",
                    .selected = selectedCue->name,
                    .options = std::span<const std::string_view>{
                        cueLabels.data(), cueLabels.size()},
                    .size = {(std::min)(width, 420.0f), 30.0f},
                    .row_height = 28.0f,
                    .max_visible_options = 10});
            if (cueSelect.changed
                && cueSelect.selected_index
                && *cueSelect.selected_index < summary.cues.size())
            {
                editor.selectedProjectAudioCueId = summary.cues[
                    *cueSelect.selected_index].stable_id;
                selectedCue = summary.cues.begin()
                    + static_cast<std::ptrdiff_t>(
                        *cueSelect.selected_index);
            }

            const auto& cue = *selectedCue;
            gui::label("Cue Properties");
            gui::property_row("[cue] Name", cue.name, 126.0f);
            gui::property_row("[cue] Source", cue.logical_path, 126.0f);
            gui::property_row(
                "[cue] Decode source",
                cue.source_exists
                    ? epochengine::format_text(
                        "Ready | {} KiB",
                        (cue.source_bytes + 1023u) / 1024u)
                    : std::string{"Missing"},
                126.0f);

            const auto semanticSelect = gui::select_box(
                gui::SelectBoxOptions{
                    .id = "project-audio-cue-semantic-"
                        + std::to_string(cue.stable_id),
                    .placeholder = "Runtime event",
                    .selected = project_audio_semantic_label(
                        cue.semantic),
                    .options = std::span<const std::string_view>{
                        kProjectAudioCueSemanticLabels.data(),
                        kProjectAudioCueSemanticLabels.size()},
                    .size = {(std::min)(width, 340.0f), 30.0f},
                    .row_height = 27.0f,
                    .max_visible_options = 6});
            if (semanticSelect.changed
                && semanticSelect.selected_index
                && *semanticSelect.selected_index
                    < kProjectAudioCueSemantics.size())
            {
                report_project_audio_update(
                    editor,
                    editor_configure_project_audio_cue(
                        editor.projectId,
                        cue.stable_id,
                        kProjectAudioCueSemantics[
                            *semanticSelect.selected_index],
                        cue.bus_id,
                        cue.gain,
                        cue.looping,
                        cue.autoplay));
            }

            std::vector<std::string_view> busLabels{};
            std::vector<std::uint64_t> busIds{};
            busLabels.reserve(summary.buses.size() + 1u);
            busIds.reserve(summary.buses.size() + 1u);
            busLabels.emplace_back("Master");
            busIds.push_back(0u);
            for (const auto& bus : summary.buses)
            {
                busLabels.emplace_back(bus.name);
                busIds.push_back(bus.stable_id);
            }
            const auto busSelect = gui::select_box(
                gui::SelectBoxOptions{
                    .id = "project-audio-cue-bus-"
                        + std::to_string(cue.stable_id),
                    .placeholder = "Mix bus",
                    .selected = cue.bus_name,
                    .options = std::span<const std::string_view>{
                        busLabels.data(), busLabels.size()},
                    .size = {(std::min)(width, 340.0f), 30.0f},
                    .row_height = 27.0f,
                    .max_visible_options = 10});
            if (busSelect.changed
                && busSelect.selected_index
                && *busSelect.selected_index < busIds.size())
            {
                report_project_audio_update(
                    editor,
                    editor_configure_project_audio_cue(
                        editor.projectId,
                        cue.stable_id,
                        cue.semantic,
                        busIds[*busSelect.selected_index],
                        cue.gain,
                        cue.looping,
                        cue.autoplay));
            }

            const std::string selectedCueGain =
                project_audio_gain_label(cue.gain);
            const auto cueGainSelect = gui::select_box(
                gui::SelectBoxOptions{
                    .id = "project-audio-cue-gain-"
                        + std::to_string(cue.stable_id),
                    .placeholder = "Cue volume",
                    .selected = selectedCueGain,
                    .options = std::span<const std::string_view>{
                        gainLabels.data(), gainLabels.size()},
                    .size = {(std::min)(width, 340.0f), 30.0f},
                    .row_height = 27.0f,
                    .max_visible_options = 9});
            if (cueGainSelect.changed
                && cueGainSelect.selected_index
                && *cueGainSelect.selected_index
                    < kProjectAudioGainChoices.size())
            {
                report_project_audio_update(
                    editor,
                    editor_configure_project_audio_cue(
                        editor.projectId,
                        cue.stable_id,
                        cue.semantic,
                        cue.bus_id,
                        kProjectAudioGainChoices[
                            *cueGainSelect.selected_index].value,
                        cue.looping,
                        cue.autoplay));
            }

            bool looping = cue.looping;
            if (gui::toggle_switch(
                    "Loop Cue",
                    looping,
                    {(std::min)(width, 280.0f), 27.0f}))
            {
                report_project_audio_update(
                    editor,
                    editor_configure_project_audio_cue(
                        editor.projectId,
                        cue.stable_id,
                        cue.semantic,
                        cue.bus_id,
                        cue.gain,
                        looping,
                        cue.autoplay));
            }
            bool autoplay = cue.autoplay;
            if (gui::toggle_switch(
                    "Autoplay On Run",
                    autoplay,
                    {(std::min)(width, 280.0f), 27.0f}))
            {
                report_project_audio_update(
                    editor,
                    editor_configure_project_audio_cue(
                        editor.projectId,
                        cue.stable_id,
                        cue.semantic,
                        cue.bus_id,
                        cue.gain,
                        cue.looping,
                        autoplay));
            }
            if (summary.cues.size() > 1u
                && gui::button(
                    "Delete Cue",
                    {(std::min)(width, 180.0f), 30.0f}))
            {
                const auto removed =
                    editor_remove_project_audio_cue(
                        editor.projectId,
                        cue.stable_id);
                report_project_audio_update(editor, removed);
                if (removed.succeeded && removed.changed)
                    editor.selectedProjectAudioCueId = 0u;
            }
        }
        [[nodiscard]] bool run_assets_interaction_proof(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx)
        {
            namespace fs = std::filesystem;

            static std::atomic<std::uint64_t> sequence{1u};
            fs::path proofRoot{};
            const auto trace_stage = [](std::string_view id, bool passed)
            {
                append_editor_automation_trace(
                    std::string{"assets_interaction_proof."}
                    + std::string{id}
                    + "="
                    + (passed ? "pass" : "fail"));
                return passed;
            };
            const auto finish = [&](bool passed) noexcept
            {
                editor.projectTextures.reset();
                if (!proofRoot.empty())
                {
                    std::error_code ignored{};
                    fs::remove_all(proofRoot, ignored);
                }
                append_editor_automation_trace(
                    std::string{"assets_interaction_proof.result="}
                    + (passed ? "pass" : "fail"));
                return passed;
            };

            try
            {
                std::error_code error{};
                proofRoot = fs::temp_directory_path(error);
                if (error || proofRoot.empty())
                    return finish(false);
                proofRoot /= "epoch_assets_interaction_proof_"
                    + std::to_string(sequence.fetch_add(
                        1u,
                        std::memory_order_relaxed));
                fs::create_directories(proofRoot / "Scenes", error);
                if (error)
                    return finish(false);

                editor.projectRoot = proofRoot.generic_string();
                editor.projectScenePath =
                    (proofRoot / "Scenes" / "AssetsInteractionProof.epoch")
                        .generic_string();
                editor.projectTextures.reset();
                editor.selectedProjectFile.clear();
                editor.selectedAssetPath.clear();
                editor.sceneDocument = authoring::scene::SceneDocument{};
                editor.sceneDocumentRevision = 1u;
                editor.entities.clear();
                for (const auto& seed :
                     editor_seed_entities_for_project(editor.projectId))
                {
                    editor.entities.push_back(
                        editor_entity_from_seed(seed));
                }
                if (!trace_stage(
                        "scene.prepare_isolated",
                        !editor.entities.empty()
                        && rebuild_editor_scene_document(
                            editor,
                            "Prepare isolated Assets interaction proof")))
                {
                    return finish(false);
                }

                bool selectedRenderable = false;
                for (std::size_t index = 0u;
                     index < editor.entities.size();
                     ++index)
                {
                    const EditorEntity& entity = editor.entities[index];
                    if (entity.type == "Camera"
                        || entity.type == "Light"
                        || entity.type == "Spawn")
                    {
                        continue;
                    }
                    select_editor_entity(editor, index);
                    selectedRenderable = true;
                    break;
                }
                if (!trace_stage(
                        "scene.select_renderable",
                        selectedRenderable
                        && selected_entity_accepts_canvas2d_material(editor)))
                {
                    return finish(false);
                }

                editor.newTextureName = "contract_texture";
                editor.newTextureExtentIndex = 0u;
                editor.newTexturePatternIndex = 0u;
                create_starter_project_texture(editor, ctx);
                auto* controller = project_texture_controller(editor);
                const auto* imported =
                    controller == nullptr ? nullptr : controller->selected();
                if (!trace_stage(
                        "assets.texture.create_import",
                        imported != nullptr
                        && static_cast<bool>(*imported)
                        && imported->logical_path
                            == "Assets/Textures/contract_texture.epoch_texture"))
                {
                    return finish(false);
                }

                const auto selected =
                    controller->select(imported->logical_path);
                editor.selectedAssetPath = imported->logical_path;
                if (!trace_stage(
                        "assets.texture.select_compiled",
                        static_cast<bool>(selected)
                        && controller->selected() != nullptr
                        && controller->selected()->artifact_key
                            == imported->artifact_key))
                {
                    return finish(false);
                }

                auto material = controller->selected_material();
                if (material)
                {
                    material->filter = editor.textureFilter;
                    material->address_u = editor.textureAddressU;
                    material->address_v = editor.textureAddressV;
                    material->alpha = editor.textureAlpha;
                    material->color_space = editor.textureColorSpace;
                    material->alpha_cutoff = editor.textureAlphaCutoff;
                }
                assign_selected_project_texture(editor);
                if (!trace_stage(
                        "assets.texture.assign_to_selection",
                        material.has_value()
                        && editor.entities[editor.selectedEntity]
                                .textureMaterial
                            == material))
                {
                    return finish(false);
                }

                clear_selected_project_texture(editor);
                if (!trace_stage(
                        "assets.texture.clear_material",
                        !editor.entities[editor.selectedEntity]
                             .textureMaterial.has_value()))
                {
                    return finish(false);
                }

                const auto undone = editor.sceneDocument.undo();
                const bool undoProjected =
                    static_cast<bool>(undone)
                    && project_scene_document_into_editor(editor);
                if (!trace_stage(
                        "scene.undo",
                        undoProjected
                        && editor.entities[editor.selectedEntity]
                                .textureMaterial
                            == material))
                {
                    return finish(false);
                }

                const auto redone = editor.sceneDocument.redo();
                const bool redoProjected =
                    static_cast<bool>(redone)
                    && project_scene_document_into_editor(editor);
                if (!trace_stage(
                        "scene.redo",
                        redoProjected
                        && !editor.entities[editor.selectedEntity]
                                .textureMaterial.has_value()))
                {
                    return finish(false);
                }

                const auto restoredForSave = editor.sceneDocument.undo();
                if (!restoredForSave
                    || !project_scene_document_into_editor(editor)
                    || editor.entities[editor.selectedEntity].textureMaterial
                        != material)
                {
                    trace_stage("scene.restore_assigned_for_save", false);
                    return finish(false);
                }
                trace_stage("scene.restore_assigned_for_save", true);

                const bool saved = save_editor_scene_snapshot(editor);
                if (!saved)
                {
                    append_editor_automation_trace(
                        std::string{"assets_interaction_proof.project.save_detail="}
                        + editor.projectStatus);
                }
                if (!trace_stage(
                        "project.save",
                        saved))
                {
                    return finish(false);
                }

                clear_selected_project_texture(editor);
                const bool clearedBeforeReopen =
                    !editor.entities[editor.selectedEntity]
                         .textureMaterial.has_value();
                const bool reopened =
                    clearedBeforeReopen
                    && load_editor_scene_snapshot(editor);
                if (!trace_stage(
                        "project.reopen",
                        reopened
                        && editor.entities[editor.selectedEntity]
                                .textureMaterial
                            == material))
                {
                    return finish(false);
                }

                return finish(true);
            }
            catch (...)
            {
                append_editor_automation_trace(
                    "assets_interaction_proof.exception=fail");
                return finish(false);
            }
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_script_browser_entries(
            std::string_view projectRoot,
            std::size_t maxEntries = 32)
        {
            std::vector<EditorBrowserEntry> entries;
            const std::array roots{
                resolve_editor_path(std::filesystem::path{ projectRoot }) / "scripts",
                resolve_editor_path(std::filesystem::path{ "Engine/src/scripts" })
            };

            for (const auto& root : roots)
            {
                std::error_code ec;
                if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                    continue;

                for (const auto& entry : std::filesystem::directory_iterator(root, ec))
                {
                    if (ec || entries.size() >= maxEntries)
                    {
                        ec.clear();
                        break;
                    }

                    std::error_code entryEc;
                    if (!entry.is_regular_file(entryEc) || entryEc)
                        continue;

                    const auto path = entry.path();
                    if (!path.filename().string().ends_with(".ascript.cpp"))
                        continue;

                    auto item = make_browser_entry(root, path, false);
                    item.kind = script_id_from_source_path(path);
                    entries.push_back(std::move(item));
                }
            }

            return entries;
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_asset_browser_entries(
            const EditorState& editor,
            std::size_t maxEntries = 48)
        {
            std::vector<EditorBrowserEntry> entries;
            const auto append_if_ready = [&](const std::filesystem::path& root, const std::filesystem::path& path)
            {
                if (entries.size() >= maxEntries || path.empty())
                    return;
                std::error_code ec;
                const auto resolved = resolve_editor_path(path);
                if (!std::filesystem::exists(resolved, ec) || ec)
                    return;
                entries.push_back(make_browser_entry(root.empty() ? resolved.parent_path() : root, resolved, std::filesystem::is_directory(resolved, ec) && !ec));
            };

            const auto projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            append_if_ready(projectRoot, editor.projectScenePath);
            const auto modelSummary = editor_project_model_summary(editor.projectId);
            append_if_ready(projectRoot, modelSummary.resolved_path);

            const std::array roots{
                projectRoot / "Assets",
                resolve_editor_path(std::filesystem::path{ "Engine/assets" }),
                resolve_editor_path(std::filesystem::path{ "Engine/examples/EpochEditor/assets" })
            };

            for (const auto& root : roots)
            {
                std::error_code ec;
                if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                    continue;

                for (std::filesystem::recursive_directory_iterator it(
                         root,
                         std::filesystem::directory_options::skip_permission_denied,
                         ec),
                     end;
                     it != end && entries.size() < maxEntries;
                     it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    const bool directory = it->is_directory(ec);
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    if (directory)
                    {
                        if (should_skip_browser_directory(it->path().filename().string()))
                        {
                            it.disable_recursion_pending();
                            continue;
                        }
                        if (it.depth() >= 1)
                            it.disable_recursion_pending();
                    }
                    if (it.depth() > 1)
                        continue;

                    entries.push_back(make_browser_entry(root, it->path(), directory));
                }
            }

            if (entries.size() < maxEntries)
            {
                for (auto scriptEntry : collect_script_browser_entries(editor.projectRoot, maxEntries - entries.size()))
                {
                    if (entries.size() >= maxEntries)
                        break;
                    entries.push_back(std::move(scriptEntry));
                }
            }

            return entries;
        }

        [[nodiscard]] static std::string make_project_script_starter_text(std::string_view scriptId)
        {
            return epochengine::format_text(
                "#if __has_include(<scripting.epoch_api.h>)\n"
                "#  include <scripting.epoch_api.h>\n"
                "#elif __has_include(<include/scripting.epoch_api.h>)\n"
                "#  include <include/scripting.epoch_api.h>\n"
                "#else\n"
                "#  error \"Epoch script API header not found. Add Engine/include or Engine/ to include paths.\"\n"
                "#endif\n\n"
                "namespace\n"
                "{{\n"
                "    void host_log(EpochScriptHost* host, const char* message)\n"
                "    {{\n"
                "        if (host && host->log)\n"
                "            host->log(host->user_data, message);\n"
                "    }}\n"
                "}}\n\n"
                "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                "{{\n"
                "    if (!host)\n"
                "        return;\n\n"
                "    host_log(host, \"{}: script starter executed.\");\n"
                "    if (host->rotate_all_entities_yaw)\n"
                "    {{\n"
                "        host->rotate_all_entities_yaw(host->user_data, 3.0f);\n"
                "        host_log(host, \"{}: applied +3 yaw proof step.\");\n"
                "    }}\n"
                "}}\n",
                scriptId,
                scriptId);
        }

        static void create_project_script_starter(EditorState& editor)
        {
            const std::string scriptIdBase = sanitize_script_id(editor.newScriptName);
            auto projectRoot = resolve_editor_path(std::filesystem::path{editor.projectRoot});
            if (!core::path::candidate_data_root().empty())
                projectRoot = editor_resolve_candidate_project_path(projectRoot);
            if (projectRoot.empty())
            {
                editor.scriptBuildStatus = "No writable project is admitted for this script starter.";
                return;
            }
            const auto scriptsRoot = projectRoot / "scripts";
            std::error_code ec;
            std::filesystem::create_directories(scriptsRoot, ec);
            if (ec)
            {
                editor.scriptBuildStatus = "Could not create project scripts directory.";
                push_editor_log(editor, "[script] Failed to create scripts directory: " + scriptsRoot.generic_string());
                return;
            }

            std::string scriptId = scriptIdBase;
            std::filesystem::path scriptPath = scriptsRoot / (scriptId + ".ascript.cpp");
            for (int suffix = 2; std::filesystem::exists(scriptPath, ec) && suffix < 100; ++suffix)
            {
                ec.clear();
                scriptId = scriptIdBase + "_" + std::to_string(suffix);
                scriptPath = scriptsRoot / (scriptId + ".ascript.cpp");
            }

            std::ofstream out(scriptPath, std::ios::binary);
            if (!out)
            {
                editor.scriptBuildStatus = "Could not write project script starter.";
                push_editor_log(editor, "[script] Failed to write script starter: " + display_project_path(scriptPath));
                return;
            }

            out << make_project_script_starter_text(scriptId);
            editor.activeScript = scriptId;
            editor.newScriptName = scriptId;
            editor.selectedProjectFile = display_project_path(scriptPath);
            editor.selectedAssetPath = editor.selectedProjectFile;
            (void)load_script_source_editor(editor, scriptPath, true);
            editor.scriptBuildStatus = "Created project script starter: " + editor.selectedProjectFile;
            push_editor_log(editor, "[script] Created project script starter '" + scriptId + "'.");
            append_project_note(
                editor,
                "Create Project Script Starter",
                std::string("Created ") + scriptId + ".ascript.cpp.",
                "Use Build Selected Script to validate the script asset. The centered Run button remains reserved for the active generated project shell.");
        }

        struct SeedObjectSummary
        {
            std::size_t total{ 0 };
            std::size_t worldCount{ 0 };
            std::size_t gameplayCount{ 0 };
            std::size_t editorCount{ 0 };
            std::size_t visibleCount{ 0 };
            std::size_t editorOnlyCount{ 0 };
            std::vector<std::string> types{};
            std::vector<std::string> categories{};
        };

