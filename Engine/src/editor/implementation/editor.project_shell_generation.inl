    [[nodiscard]] static bool write_text_file_if_allowed(
        const fs::path& path,
        std::string_view text,
        bool overwrite_existing)
    {
        std::error_code ec;
        if (!overwrite_existing && fs::exists(path, ec) && !ec)
            return true;
        return write_text_file(path, text);
    }

    inline constexpr std::string_view managed_generated_file_marker{
        "EPOCH_MANAGED_GENERATED_FILE:"};

    [[nodiscard]] static bool write_managed_generated_text_file(
        const fs::path& path,
        std::string_view text,
        bool overwrite_existing,
        std::initializer_list<std::string_view> legacy_signatures)
    {
        constexpr std::uintmax_t maximum_managed_bytes = 16ull * 1024ull * 1024ull;
        std::error_code error{};
        const fs::file_status status = fs::symlink_status(path, error);
        const bool exists = !error && fs::exists(status);
        if (error && error != std::errc::no_such_file_or_directory)
            return false;
        error.clear();

        if (exists)
        {
            if (fs::is_symlink(status) || !fs::is_regular_file(status))
                return false;
            const std::uintmax_t bytes = fs::file_size(path, error);
            if (error || bytes > maximum_managed_bytes)
                return false;
            std::string current(static_cast<std::size_t>(bytes), '\0');
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return false;
            if (!current.empty())
            {
                input.read(
                    current.data(),
                    static_cast<std::streamsize>(current.size()));
                if (!input
                    || input.gcount()
                        != static_cast<std::streamsize>(current.size()))
                {
                    return false;
                }
            }
            if (current == text)
                return true;

            const std::size_t marker = current.find(
                managed_generated_file_marker);
            bool legacy_owned = legacy_signatures.size() != 0u;
            for (const std::string_view signature : legacy_signatures)
            {
                legacy_owned = legacy_owned
                    && !signature.empty()
                    && current.find(signature) != std::string::npos;
            }
            const bool managed = marker != std::string::npos && marker < 256u;
            if (!overwrite_existing && !managed && !legacy_owned)
                return false;
        }

        fs::create_directories(path.parent_path(), error);
        if (error)
            return false;
        fs::path temporary = path;
        temporary += ".epoch-managed.tmp";
        const fs::file_status temporary_status =
            fs::symlink_status(temporary, error);
        if (!error && fs::exists(temporary_status))
        {
            if (fs::is_symlink(temporary_status)
                || !fs::is_regular_file(temporary_status)
                || !fs::remove(temporary, error) || error)
            {
                return false;
            }
        }
        error.clear();
        const auto bytes = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(text.data()), text.size()};
        if (!epochengine::platform::filesystem::exclusive_create_and_write(
                temporary, bytes, error))
        {
            return false;
        }
        if (!epochengine::platform::filesystem::atomic_replace_same_filesystem(
                temporary, path, error))
        {
            std::error_code cleanup_error{};
            (void)fs::remove(temporary, cleanup_error);
            return false;
        }
        return true;
    }
    [[nodiscard]] static std::optional<std::string>
        migrated_project_manifest_path_text(
            std::string_view original,
            std::string_view fieldName,
            std::string_view expectedPath)
    {
        const std::string_view canonicalPath = fieldName == "input_profile"
            ? epochengine::project_input::canonical_source_path
            : fieldName == "gui"
                ? epochengine::project_gui::canonical_source_path
                : fieldName == "project_format"
                    ? kProjectManifestFormat
                    : fieldName == "build_profile"
                        ? kStaticRuntimeBuildProfile
                        : std::string_view{};
        if (expectedPath.empty() || canonicalPath.empty()
            || expectedPath != canonicalPath)
        {
            return std::nullopt;
        }
        std::string text{original};
        const JsonStringFieldResult field =
            inspect_json_string_field(text, fieldName);
        if (field.state == JsonStringFieldState::present)
        {
            return field.value == expectedPath
                ? std::optional<std::string>{std::move(text)}
                : std::nullopt;
        }
        if (field.state != JsonStringFieldState::missing)
            return std::nullopt;

        const std::size_t open = text.find_first_not_of(" \t\r\n");
        std::size_t close = text.find_last_not_of(" \t\r\n");
        if (open == std::string::npos || close == std::string::npos
            || open >= close || text[open] != '{' || text[close] != '}')
        {
            return std::nullopt;
        }
        const std::size_t previous = text.find_last_not_of(
            " \t\r\n", close - 1u);
        if (previous == std::string::npos || text[previous] == ',')
            return std::nullopt;

        if (text[previous] != '{')
        {
            text.insert(previous + 1u, 1u, ',');
            ++close;
        }
        const std::string line =
            std::string{"  \""} + std::string{fieldName} + "\": \""
            + json_escape(expectedPath)
            + "\"\n";
        text.insert(close, line);

        const JsonStringFieldResult migrated =
            inspect_json_string_field(text, fieldName);
        if (migrated.state != JsonStringFieldState::present
            || migrated.value != expectedPath)
        {
            return std::nullopt;
        }
        return text;
    }

    [[nodiscard]] static bool migrate_project_manifest_path(
        const fs::path& manifest,
        std::string_view fieldName,
        std::string_view expectedPath,
        std::string_view temporarySuffix)
    {
        const auto migrated = migrated_project_manifest_path_text(
            read_text_file(manifest), fieldName, expectedPath);
        if (!migrated || temporarySuffix.empty())
            return false;

        fs::path temporary = manifest;
        temporary += temporarySuffix;
        std::error_code ec;
        const fs::file_status temporaryStatus =
            fs::symlink_status(temporary, ec);
        if (!ec && fs::exists(temporaryStatus)
            && (fs::is_symlink(temporaryStatus)
                || !fs::is_regular_file(temporaryStatus)))
        {
            return false;
        }
        ec.clear();
        fs::remove(temporary, ec);
        ec.clear();
        if (!write_text_file(temporary, *migrated)
            || !epochengine::platform::filesystem::
                atomic_replace_same_filesystem(temporary, manifest, ec))
        {
            std::error_code cleanup;
            fs::remove(temporary, cleanup);
            return false;
        }
        return true;
    }    [[nodiscard]] static bool ensure_project_input_source(
        std::string_view projectId,
        const fs::path& root)
    {
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return false;
        const auto loaded = store.load_source();
        if (loaded)
            return true;
        if (loaded.code != epochengine::project_input::StoreCode::not_found)
            return false;

        epochengine::project_input::ProfileSource source{};
        const auto artifact = store.load_artifact();
        if (artifact)
        {
            source = {
                .id = artifact.artifact.profile_id,
                .display_name = artifact.artifact.display_name,
                .revision = artifact.artifact.source_revision,
                .actions = artifact.artifact.actions,
                .bindings = artifact.artifact.bindings
            };
            if (epochengine::project_input::seal_profile_source(source)
                != epochengine::project_input::ValidationCode::ready)
            {
                return false;
            }
        }
        else if (artifact.code == epochengine::project_input::StoreCode::not_found)
        {
            source = epochengine::project_input::make_legacy_default_profile();
        }
        else
        {
            return false;
        }
        return static_cast<bool>(store.save_source(source));
    }
    [[nodiscard]] static bool ensure_project_audio_source(
        std::string_view projectId,
        const fs::path& root)
    {
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return false;

        const auto defaultSource =
            epochengine::project_audio::make_default_2d_profile();
        auto loaded = store.load();
        epochengine::project_audio::ProfileSource source{};
        if (loaded)
        {
            source = std::move(loaded.source);
        }
        else if (loaded.code
            == epochengine::project_audio::StoreCode::not_found)
        {
            source = defaultSource;
            if (!store.save(source))
                return false;
        }
        else
        {
            return false;
        }

        if (source == defaultSource)
        {
            if (!write_text_file_if_allowed(
                    root / "Assets/Audio/default_jump.wav",
                    make_project_pcm16_wave(660.0f, 95u, 0.24f, true),
                    false)
                || !write_text_file_if_allowed(
                    root / "Assets/Audio/default_land.wav",
                    make_project_pcm16_wave(165.0f, 75u, 0.20f, true),
                    false)
                || !write_text_file_if_allowed(
                    root / "Assets/Audio/default_ambient.wav",
                    make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false),
                    false))
            {
                return false;
            }
        }

        const auto compiled = store.compile(source, false);
        return compiled
            && static_cast<bool>(store.publish_artifact(compiled));
    }

    [[nodiscard]] static std::optional<
        epochengine::project_sprite_animation::DefaultActorSheet>
        actor_sheet_from_tilemap(
            const epochengine::authoring::tilemap::DocumentSnapshot& snapshot)
    {
        namespace sprite_animation =
            epochengine::project_sprite_animation;
        for (const auto& tileSet : snapshot.tile_sets)
        {
            const auto& descriptor = tileSet.descriptor;
            const auto& texture = descriptor.texture;
            sprite_animation::DefaultActorSheet sheet{};
            sheet.material.logical_texture_path = texture.logical_path;
            sheet.material.texture_artifact_key =
                epochengine::asset::texture::ContentHash{
                    texture.artifact_key};
            sheet.material.texture_artifact_revision =
                texture.artifact_revision;
            sheet.material.stable_material_key =
                texture.stable_material_key;
            sheet.material.texture_extent = {
                texture.texture_extent.x,
                texture.texture_extent.y};
            sheet.tile_extent = {
                descriptor.tile_extent.x,
                descriptor.tile_extent.y};
            sheet.grid = {descriptor.grid.x, descriptor.grid.y};
            sheet.margin = {descriptor.margin.x, descriptor.margin.y};
            sheet.spacing = {descriptor.spacing.x, descriptor.spacing.y};
            sheet.tile_count = descriptor.tile_count;
            if (sheet.valid())
                return sheet;
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool ensure_project_2d_assets(
        std::string_view projectId,
        const fs::path& root,
        std::string_view tileMapPath,
        std::string_view spriteAnimationPath,
        std::string& diagnostic)
    {
        diagnostic.clear();
        if (tileMapPath.empty() && spriteAnimationPath.empty())
            return true;
        if (tileMapPath.empty())
        {
            diagnostic =
                "Sprite animation requires a declared tile map texture source.";
            return false;
        }

        epochengine::editor_tilemaps::TileMapWorkspaceController tileMap{
            std::string{projectId}, root, std::string{tileMapPath}};
        const fs::path tileMapSourcePath =
            (root / fs::path{tileMapPath}).lexically_normal();
        std::error_code tileMapStatusError{};
        const bool tileMapSourceExisted =
            fs::exists(tileMapSourcePath, tileMapStatusError);
        if (tileMapStatusError)
        {
            diagnostic = "Tile-map source status could not be inspected.";
            return false;
        }

        epochengine::authoring::tilemap::MapDescriptor mapDescriptor{};
        mapDescriptor.name = "StarterMap";
        mapDescriptor.extent_tiles = {64u, 36u};
        mapDescriptor.default_chunk_extent = {16u, 16u};
        const auto opened = tileMap.open_or_create(mapDescriptor);
        if (!opened)
        {
            diagnostic = "Tile-map source preparation failed: "
                + std::string{
                    epochengine::editor_tilemaps::controller_code_name(
                        opened.code)};
            return false;
        }

        auto mapSnapshot = tileMap.snapshot();
        if (mapSnapshot.tile_sets.empty())
        {
            constexpr std::string_view starterTexturePath{
                "Assets/Textures/starter_actor.epoch_texture"};
            epochengine::editor_project_textures::ProjectTextureController
                textures{std::string{projectId}, root};
            if (!textures.valid())
            {
                diagnostic = "Starter texture controller is invalid.";
                return false;
            }

            const auto renderer =
                epochengine::capability::renderer_subsystem_profile_for(
                    epochengine::RendererBackendKind::opengl);
            const auto budgets = renderer.capability.recommended_budgets;
            const fs::path sourcePath =
                (root / fs::path{starterTexturePath}).lexically_normal();
            std::error_code sourceError{};
            const bool sourceExists = fs::exists(sourcePath, sourceError)
                && !sourceError;

            epochengine::editor_project_textures::ControllerResult texture{};
            if (sourceExists)
            {
                texture = textures.open_editable(
                    starterTexturePath, renderer, budgets);
            }
            else
            {
                epochengine::authoring::texture::CanvasDescriptor canvas{};
                canvas.width = 16u;
                canvas.height = 16u;
                canvas.tile_extent = 16u;
                canvas.mip_count = 1u;
                canvas.format =
                    epochengine::authoring::texture::PixelFormat::rgba8_unorm;
                canvas.color_space =
                    epochengine::asset::texture::ColorSpace::linear;
                texture = textures.create_editable(
                    starterTexturePath, canvas, renderer, budgets);
                if (texture)
                {
                    epochengine::authoring::texture::StrokeDescriptor stroke{};
                    stroke.color = {42u, 202u, 142u, 255u};
                    stroke.radius_subpixels = 196u;
                    stroke.samples.reserve(16u * 16u);
                    for (std::uint32_t y = 0u; y < 16u; ++y)
                    {
                        for (std::uint32_t x = 0u; x < 16u; ++x)
                        {
                            stroke.samples.push_back({
                                .x_subpixels =
                                    static_cast<std::int64_t>(x * 256u + 128u),
                                .y_subpixels =
                                    static_cast<std::int64_t>(y * 256u + 128u)});
                        }
                    }
                    if (!textures.paint_editable(std::move(stroke)))
                    {
                        diagnostic =
                            "Starter texture paint operation was rejected.";
                        return false;
                    }
                    texture = textures.save_editable(renderer, budgets);
                }
            }
            if (!texture || !texture.entry)
            {
                diagnostic = "Starter texture preparation failed: "
                    + std::string{
                        epochengine::editor_project_textures::
                            controller_code_name(texture.code)}
                    + " | admission "
                    + std::string{
                        epochengine::project_textures::
                            texture_admission_reason_name(
                                texture.admission_reason)};
                return false;
            }

            const auto attached = tileMap.attach_texture(
                *texture.entry,
                textures.pipeline().project_key(),
                {16u, 16u});
            if (!attached)
            {
                diagnostic = "Tile-map texture attachment failed: "
                    + std::string{
                        epochengine::editor_tilemaps::controller_code_name(
                            attached.code)};
                return false;
            }
            mapSnapshot = tileMap.snapshot();
            if (!tileMapSourceExisted)
            {
                using Tool =
                    epochengine::gui_lib::tile_workspace::Tool;
                if (!tileMap.select_layer(0u)
                    || !tileMap.select_palette(0u)
                    || !tileMap.select_tool(Tool::collision)
                    || !tileMap.apply_cell({0u, 0u})
                    || !tileMap.select_tool(Tool::pencil))
                {
                    diagnostic =
                        "Starter map collision setup was rejected.";
                    return false;
                }

                constexpr std::uint32_t starterFloorY{8u};
                for (std::uint32_t x = 0u;
                     x < mapDescriptor.extent_tiles.x;
                     ++x)
                {
                    if (!tileMap.apply_cell({x, starterFloorY}))
                    {
                        diagnostic =
                            "Starter map ground authoring was rejected.";
                        return false;
                    }
                }

                if (!tileMap.select_tool(Tool::object)
                    || !tileMap.apply_cell({4u, 4u})
                    || !tileMap.selected_object_draft())
                {
                    diagnostic =
                        "Starter map spawn creation was rejected.";
                    return false;
                }
                auto spawn = *tileMap.selected_object_draft();
                spawn.name = "PlayerSpawn";
                spawn.type = "spawn";
                spawn.position = {4.5f, 4.5f};
                spawn.size = {0.75f, 0.9f};
                if (!tileMap.stage_selected_object(std::move(spawn))
                    || !tileMap.apply_selected_object())
                {
                    diagnostic =
                        "Starter map spawn authoring was rejected.";
                    return false;
                }
                mapSnapshot = tileMap.snapshot();
            }
        }

        const auto published = tileMap.save_and_publish();
        if (!published)
        {
            diagnostic = "Tile-map publication failed: "
                + std::string{
                    epochengine::editor_tilemaps::controller_code_name(
                        published.result.code)};
            return false;
        }

        if (spriteAnimationPath.empty())
            return true;
        const auto sheet = actor_sheet_from_tilemap(mapSnapshot);
        if (!sheet)
        {
            diagnostic =
                "Tile map has no valid texture sheet for sprite animation.";
            return false;
        }
        const auto animations =
            epochengine::project_sprite_animation::
                prepare_project_sprite_animations(
                    projectId,
                    root,
                    spriteAnimationPath,
                    std::addressof(*sheet));
        if (!animations)
        {
            diagnostic = "Sprite-animation preparation failed: "
                + std::string{
                    epochengine::project_sprite_animation::
                        preparation_code_name(animations.code)}
                + " | " + animations.diagnostic;
            return false;
        }
        return true;
    }
    [[nodiscard]] static bool ensure_project_gui_assets(
        std::string_view projectId,
        const fs::path& root,
        std::string_view guiPath,
        std::string& diagnostic)
    {
        namespace authoring_gui = epochengine::authoring::gui;
        namespace project_gui = epochengine::project_gui;
        namespace gui_runtime = epochengine::project_gui_runtime;

        diagnostic.clear();
        if (guiPath.empty())
            return true;
        if (guiPath != project_gui::canonical_source_path)
        {
            diagnostic =
                "Project GUI must use the canonical Assets/Gui source path.";
            return false;
        }

        constexpr std::uint64_t maximumSourceBytes =
            64ull * 1024ull * 1024ull;
        const fs::path sourcePath =
            (root / fs::path{guiPath}).lexically_normal();
        std::optional<authoring_gui::GuiDocumentSnapshot> snapshot{};

        std::error_code statusError{};
        const bool sourceExists = fs::exists(sourcePath, statusError);
        if (statusError)
        {
            diagnostic = "Project GUI source status could not be inspected.";
            return false;
        }
        if (sourceExists)
        {
            const fs::file_status sourceStatus =
                fs::symlink_status(sourcePath, statusError);
            if (statusError || fs::is_symlink(sourceStatus)
                || !fs::is_regular_file(sourceStatus))
            {
                diagnostic =
                    "Project GUI source must be a regular non-symlink file.";
                return false;
            }

            std::error_code sizeError{};
            const std::uintmax_t sourceBytes =
                fs::file_size(sourcePath, sizeError);
            if (sizeError || sourceBytes == 0u
                || sourceBytes > maximumSourceBytes)
            {
                diagnostic = "Project GUI source exceeds its bounded size.";
                return false;
            }
            std::vector<std::byte> bytes(
                static_cast<std::size_t>(sourceBytes));
            std::ifstream input{sourcePath, std::ios::binary};
            if (!input)
            {
                diagnostic = "Project GUI source could not be opened.";
                return false;
            }
            input.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
            if (!input
                || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
            {
                diagnostic = "Project GUI source could not be read exactly.";
                return false;
            }
            const auto decoded =
                authoring_gui::deserialize_gui_document(bytes);
            if (!decoded)
            {
                diagnostic = "Project GUI source validation failed: "
                    + std::string{
                        authoring_gui::snapshot_codec_code_name(decoded.code)};
                return false;
            }
            snapshot = *decoded.snapshot;
        }
        else
        {
            auto created = authoring_gui::make_template_document(
                authoring_gui::TemplatePreset::game_hud);
            if (!created)
            {
                diagnostic = "Default project GUI construction failed.";
                return false;
            }
            snapshot = created->snapshot();
            const auto encoded = authoring_gui::serialize_gui_document(
                *snapshot, maximumSourceBytes);
            if (!encoded)
            {
                diagnostic = "Default project GUI serialization failed: "
                    + std::string{
                        authoring_gui::snapshot_codec_code_name(encoded.code)};
                return false;
            }

            std::error_code directoryError{};
            fs::create_directories(sourcePath.parent_path(), directoryError);
            if (directoryError)
            {
                diagnostic = "Default project GUI directory creation failed.";
                return false;
            }
            fs::path temporary = sourcePath;
            temporary += ".materialize.tmp";
            std::error_code temporaryError{};
            const fs::file_status temporaryStatus =
                fs::symlink_status(temporary, temporaryError);
            if (!temporaryError && fs::exists(temporaryStatus))
            {
                if (fs::is_symlink(temporaryStatus)
                    || !fs::is_regular_file(temporaryStatus)
                    || !fs::remove(temporary, temporaryError)
                    || temporaryError)
                {
                    diagnostic =
                        "Default project GUI temporary source is unsafe.";
                    return false;
                }
            }
            temporaryError.clear();
            if (!epochengine::platform::filesystem::exclusive_create_and_write(
                    temporary, encoded.bytes, temporaryError))
            {
                diagnostic =
                    "Default project GUI temporary source write failed.";
                return false;
            }

            std::error_code concurrentError{};
            if (fs::exists(sourcePath, concurrentError) && !concurrentError)
            {
                std::error_code cleanupError{};
                (void)fs::remove(temporary, cleanupError);
                diagnostic =
                    "Project GUI source appeared during materialization; retry without overwriting it.";
                return false;
            }
            if (!epochengine::platform::filesystem::
                    atomic_replace_same_filesystem(
                        temporary, sourcePath, temporaryError))
            {
                std::error_code cleanupError{};
                (void)fs::remove(temporary, cleanupError);
                diagnostic = "Default project GUI publication failed.";
                return false;
            }
        }

        const auto compiled =
            authoring_gui::compile_gui_document(*snapshot);
        if (!compiled)
        {
            diagnostic = "Project GUI compilation failed: "
                + std::string{authoring_gui::compile_code_name(compiled.code)};
            return false;
        }
        project_gui::ArtifactLibrary library{
            std::string{projectId}, root.generic_string()};
        if (!library.valid())
        {
            diagnostic = "Project GUI Library is invalid.";
            return false;
        }
        const auto published = library.persist(guiPath, compiled.artifact);
        if (!published)
        {
            diagnostic = "Project GUI artifact publication failed: "
                + std::string{
                    project_gui::library_code_name(published.code)};
            return false;
        }
        auto loaded = library.load_exact(
            guiPath, compiled.artifact.identity.key);
        if (!loaded)
        {
            diagnostic = "Project GUI artifact restoration failed: "
                + std::string{project_gui::library_code_name(loaded.code)};
            return false;
        }
        gui_runtime::RuntimeSession runtime{std::move(loaded.artifact)};
        if (!runtime.valid())
        {
            diagnostic = "Project GUI runtime rejected the restored artifact.";
            return false;
        }
        const auto frame = runtime.build_frame({1'280.0f, 720.0f});
        if (!frame)
        {
            diagnostic = "Project GUI runtime frame failed: "
                + std::string{gui_runtime::runtime_code_name(frame.code)};
            return false;
        }
        return true;
    }
    static void replace_all(std::string& text, std::string_view from, std::string_view to)
    {
        if (from.empty())
            return;

        std::size_t pos = 0;
        while ((pos = text.find(from.data(), pos, from.size())) != std::string::npos)
        {
            text.replace(pos, from.size(), to.data(), to.size());
            pos += to.size();
        }
    }

    [[nodiscard]] static bool replace_delimited_value(
        std::string& text,
        std::string_view prefix,
        std::string_view suffix,
        std::string_view value)
    {
        const std::size_t begin = text.find(prefix);
        if (begin == std::string::npos)
            return false;
        const std::size_t valueBegin = begin + prefix.size();
        const std::size_t end = text.find(suffix, valueBegin);
        if (end == std::string::npos)
            return false;
        text.replace(valueBegin, end - valueBegin, value);
        return true;
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_debug_defines() noexcept
    {
        return "ENGINE_STATICLIB;$(EpochRaylibDllDefine)_DEBUG;_CONSOLE;%(PreprocessorDefinitions)";
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_release_defines() noexcept
    {
        return "ENGINE_STATICLIB;$(EpochRaylibDllDefine)NDEBUG;_CONSOLE;%(PreprocessorDefinitions)";
    }

    [[nodiscard]] static constexpr std::string_view generated_child_project_link_dependencies(bool debug) noexcept
    {
        return debug ? "sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;libcurl-d.lib;opengl32.lib;glad.lib;SDL3.lib;raylib.lib;sqlite3.lib;libcrypto.lib;ws2_32.lib;crypt32.lib;wldap32.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;sapi.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;userenv.lib;bcrypt.lib;ncrypt.lib;user32.lib;gdi32.lib;shell32.lib;comdlg32.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)" : "sfml-graphics.lib;sfml-window.lib;sfml-system.lib;libcurl.lib;opengl32.lib;glad.lib;SDL3.lib;raylib.lib;sqlite3.lib;libcrypto.lib;ws2_32.lib;crypt32.lib;wldap32.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;sapi.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;userenv.lib;bcrypt.lib;ncrypt.lib;user32.lib;gdi32.lib;shell32.lib;comdlg32.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)";
    }

    static void repair_generated_project_link_dependencies(std::string& project)
    {
        // Migrate known generated lists only; retain custom linker input.
        constexpr std::array<std::string_view, 5> legacy{"raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;userenv.lib;bcrypt.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)",
            "raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)",
            "sfml-graphics-d.lib;sfml-window-d.lib;sfml-system-d.lib;winmm.lib;EpochEngine.lib;%(AdditionalDependencies)",
            "sfml-graphics.lib;sfml-window.lib;sfml-system.lib;winmm.lib;EpochEngine.lib;%(AdditionalDependencies)",
            "EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)"};
        std::size_t position{};
        while ((position = project.find("<ItemDefinitionGroup", position)) != std::string::npos)
        {
            const auto end = project.find("</ItemDefinitionGroup>", position);
            const auto conditionEnd = project.find('>', position);
            if (end == std::string::npos || conditionEnd == std::string::npos || conditionEnd > end) return;
            const std::string_view condition{project.data() + position, conditionEnd - position};
            const bool debug = condition.find("Debug|x64") != std::string_view::npos;
            if (debug || condition.find("Release|x64") != std::string_view::npos)
            {
                constexpr std::string_view prefix{"<AdditionalDependencies>"};
                const auto begin = project.find(prefix, conditionEnd);
                if (begin != std::string::npos && begin < end)
                {
                    const auto valueBegin = begin + prefix.size();
                    const auto valueEnd = project.find("</AdditionalDependencies>", valueBegin);
                    if (valueEnd != std::string::npos && valueEnd < end)
                    {
                        const std::string_view value{project.data() + valueBegin, valueEnd - valueBegin};
                        if (std::ranges::find(legacy, value) != legacy.end())
                            project.replace(valueBegin, value.size(), generated_child_project_link_dependencies(debug));
                    }
                }
            }
            position = project.find("</ItemDefinitionGroup>", position) + std::string_view{"</ItemDefinitionGroup>"}.size();
        }
    }

    [[nodiscard]] static bool repair_generated_project_gui_acceptance(
        const fs::path& root)
    {
        const fs::path manifest = root / "project.epoch.json";
        const JsonStringFieldResult guiField =
            inspect_json_string_field(read_text_file(manifest), "gui");
        if (guiField.state == JsonStringFieldState::missing)
            return true;
        if (guiField.state != JsonStringFieldState::present
            || guiField.value
                != epochengine::project_gui::canonical_source_path)
        {
            return false;
        }

        const fs::path entrySource =
            generated_project_entry_source_path(root);
        std::string source = read_text_file(entrySource);
        if (source.empty())
            return false;
        constexpr std::string_view assignmentPrefix{ ".gui_path = \"" };
        const std::size_t assignment = source.find(assignmentPrefix);
        if (assignment == std::string::npos
            || source.find(
                assignmentPrefix,
                assignment + assignmentPrefix.size()) != std::string::npos)
        {
            return false;
        }

        const std::size_t valueBegin = assignment + assignmentPrefix.size();
        const std::size_t valueEnd = source.find('\"', valueBegin);
        if (valueEnd == std::string::npos)
            return false;

        const std::string_view currentValue{
            source.data() + valueBegin,
            valueEnd - valueBegin};
        if (currentValue == guiField.value)
            return true;
        if (!currentValue.empty())
            return false;

        source.replace(valueBegin, 0u, guiField.value);
        return write_text_file(entrySource, source);
    }

    [[nodiscard]] static bool repair_generated_windows_child_project_build_files(const fs::path& root)
    {
        const fs::path projectFile = generated_project_windows_vcxproj_path(root);
        const fs::path buildScript = generated_project_windows_build_script_path(root);
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        if (!is_epoch_repo_root(repoRoot))
            return false;
        if (!repair_generated_project_gui_acceptance(root))
            return false;
        epochengine::logger::info("Editor.Scene",
            "Generated child-project repair root: "
            + repoRoot.generic_string());
        const fs::path engineProject =
            repoRoot / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj";
        const fs::path epochGuiProject =
            repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj";
        const std::string repoRootWindows = to_windows_path(repoRoot.string());
        const std::string repoRootXml = xml_escape(repoRootWindows);
        const std::string engineLibraryRelative = xml_escape(to_windows_path(
            epochengine::build_layout::windows_relative_bin(repoRoot, "$(Configuration)").generic_string())) + "\\";
        const std::string engineProjectXml =
            xml_escape(to_windows_path(engineProject.string()));
        const std::string epochGuiProjectXml =
            xml_escape(to_windows_path(epochGuiProject.string()));
        const std::string repoRootPowerShell =
            powershell_escape_single_quoted(repoRootWindows);
        const std::string vcpkgManifestRootPowerShell =
            powershell_escape_single_quoted(to_windows_path((repoRoot / "Engine").string()));
        const std::string projectReferenceProperties =
            "SolutionDir=" + repoRootXml
            + "\\;VcpkgManifestRoot=" + repoRootXml
            + "\\Engine\\;EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1;PlatformToolset=v143";
        const std::string epochGuiProjectReference =
            "    <ProjectReference Include=\"" + epochGuiProjectXml + "\">\n"
            "      <Project>{7B41A9B2-4B7E-4B5D-9B39-68D2408BCA90}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootXml
                + "\\;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n";

        std::error_code ec;
        if (fs::exists(projectFile, ec) && !ec)
        {
            std::string projectText = read_text_file(projectFile);
            const std::string original = projectText;
            const bool rootsRepaired =
                replace_delimited_value(
                    projectText,
                    "<ProjectReference Include=\"",
                    "\">",
                    engineProjectXml)
                && replace_delimited_value(
                    projectText,
                    "<AdditionalProperties>",
                    "</AdditionalProperties>",
                    projectReferenceProperties)
                && replace_delimited_value(
                    projectText,
                    "<EpochRepoRoot>",
                    "</EpochRepoRoot>",
                    repoRootXml + "\\");
            if (!rootsRepaired)
                return false;
            if (projectText.find(epochGuiProjectXml) == std::string::npos)
            {
                constexpr std::string_view groupEnd =
                    "  </ItemGroup>\n  <PropertyGroup Label=\"Globals\">";
                const std::size_t groupEndPosition = projectText.find(groupEnd);
                if (groupEndPosition == std::string::npos)
                    return false;
                projectText.insert(groupEndPosition, epochGuiProjectReference);
            }
            replace_all(projectText, "$(MSBuildThisFileDirectory)bin\\windows\\", "$(MSBuildThisFileDirectory)build\\bin\\windows\\");
            replace_all(projectText, "$(MSBuildThisFileDirectory)build\\windows\\obj\\", "$(MSBuildThisFileDirectory)build\\obj\\msvc\\");
            replace_all(projectText, "$(EpochRepoRoot)x64\\$(Configuration)\\;",
                "$(EpochRepoRoot)" + engineLibraryRelative + ";");
            replace_all(projectText, "StaticLib1.lib", "EpochEngine.lib");
            replace_all(
                projectText,
                "EpochEngine.lib;%(AdditionalDependencies)",
                "EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)");
            replace_all(projectText, "<PlatformToolset>v142</PlatformToolset>", "<PlatformToolset>v143</PlatformToolset>");
            replace_all(projectText, "<PlatformToolset>v145</PlatformToolset>", "<PlatformToolset>v143</PlatformToolset>");
            replace_all(projectText, "<LanguageStandard>stdcpplatest</LanguageStandard>", "<LanguageStandard>stdcpp23</LanguageStandard>");
            replace_all(
                projectText,
                "<VcpkgUseStatic>false</VcpkgUseStatic>",
                "<VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'=='' and ('$(VcpkgTriplet)'=='x64-windows-static' or '$(VcpkgTriplet)'=='x64-windows-static-md' or '$(VcpkgTriplet)'=='x86-windows-static' or '$(VcpkgTriplet)'=='x86-windows-static-md')\">true</VcpkgUseStatic>\n"
                "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'==''\">false</VcpkgUseStatic>\n"
                "    <EpochRaylibDllDefine Condition=\"'$(VcpkgUseStatic)'!='true'\">RAYLIB_DLL;</EpochRaylibDllDefine>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_debug_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;RAYLIB_DLL;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_debug_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_release_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<PreprocessorDefinitions>ENGINE_STATICLIB;RAYLIB_DLL;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>",
                std::string("<PreprocessorDefinitions>") + std::string(generated_child_project_release_defines()) + "</PreprocessorDefinitions>");
            replace_all(
                projectText,
                "<Command>if exist \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>",
                "<Command>if \"$(VcpkgUseStatic)\" NEQ \"true\" if exist \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>");
            replace_all(
                projectText,
                "<Command>if exist \"$(EpochVcpkgInstallRoot)bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>",
                "<Command>if \"$(VcpkgUseStatic)\" NEQ \"true\" if exist \"$(EpochVcpkgInstallRoot)bin\\*.dll\" xcopy /Y /D \"$(EpochVcpkgInstallRoot)bin\\*.dll\" \"$(OutDir)\" &gt;nul</Command>");
            replace_all(
                projectText,
                "<Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">",
                "<Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64' And '$(VcpkgUseStatic)'!='true'\">");
            replace_all(
                projectText,
                "<Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">",
                "<Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64' And '$(VcpkgUseStatic)'!='true'\">");
            repair_generated_project_link_dependencies(projectText);
            // Repair already materialized child .vcxproj files as well as newly
            // generated files. Without longPathAware the deep disposable test
            // roots can fail at ~260-character Windows artifact paths.
            if (projectText.find("Engine\\resource\\epoch.manifest")
                    == std::string::npos)
            {
                constexpr std::string_view linkEnd = "    </Link>\n";
                const std::size_t firstLink = projectText.find(linkEnd);
                const std::size_t secondLink = firstLink == std::string::npos
                    ? std::string::npos
                    : projectText.find(linkEnd, firstLink + linkEnd.size());
                if (secondLink == std::string::npos)
                    return false;
                constexpr std::string_view manifestBlock =
                    "    <Manifest>\n"
                    "      <EmbedManifest>true</EmbedManifest>\n"
                    "      <AdditionalManifestFiles>$(EpochRepoRoot)Engine\\resource\\epoch.manifest;%(AdditionalManifestFiles)</AdditionalManifestFiles>\n"
                    "    </Manifest>\n";
                projectText.insert(secondLink + linkEnd.size(), manifestBlock);
                projectText.insert(firstLink + linkEnd.size(), manifestBlock);
            }
            if (projectText != original && !write_text_file(projectFile, projectText))
                return false;
        }

        if (fs::exists(buildScript, ec) && !ec)
        {
            std::string scriptText = read_text_file(buildScript);
            const std::string original = scriptText;
            replace_all(scriptText, "('bin\\windows\\' + $Configuration", "('build\\bin\\windows\\' + $Configuration");
            const bool scriptRootsRepaired =
                replace_delimited_value(
                    scriptText,
                    "$repoRoot = '",
                    "'",
                    repoRootPowerShell)
                && replace_delimited_value(
                    scriptText,
                    "$vcpkgManifestRoot = '",
                    "'",
                    vcpkgManifestRootPowerShell);
            if (!scriptRootsRepaired)
                return false;
            if (scriptText.find("'/p:PlatformToolset=v143'") == std::string::npos)
            {
                replace_all(
                    scriptText,
                    "    ('/p:Platform=' + $Platform),\n",
                    "    ('/p:Platform=' + $Platform),\n"
                    "    '/p:PlatformToolset=v143',\n");
            }
            if (scriptText != original && !write_text_file(buildScript, scriptText))
                return false;
        }

        return true;
    }

    struct ProjectShellSpec
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string project_name{};
        std::string project_id{};
        fs::path root{};
        fs::path world_file{};
        std::string world_name{};
        std::string tilemap_path{};
        std::string input_profile_path{};
        std::string sprite_animation_path{};
        std::string audio_profile_path{};
        std::string gui_path{};
        std::string template_family{};
        std::string script_id{};
        std::string description{};
        std::string demo_model_asset{};
        std::string capability_profile{ "portable" };
        bool include_engine_arcade_package{ false };
        bool overwrite_existing{ true };
    };

    [[nodiscard]] static std::string make_project_world_scene_text(
        const ProjectShellSpec& spec,
        std::string_view kind_text,
        bool include_engine_arcade_package)
    {
        std::vector<EditorSceneSeedEntity> entities{};
        if (include_engine_arcade_package)
            entities = engine_arcade_seed_entities();
        else if (const auto* application = epochengine::editor_application_for_project(spec.project_id))
        {
            entities = epochengine::make_editor_application_scene(application->kind).entities;
        }
        else if (spec.kind == EditorProjectKind::Editor)
            entities = epochengine::make_editor_application_scene(
                epochengine::EditorApplicationKind::Standard).entities;
        else if (spec.kind == EditorProjectKind::Tool)
            entities = software_seed_entities();
        else
            entities = sandbox_seed_entities();

        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "project:" + spec.project_id;
        snapshot.project_id = spec.project_id;
        snapshot.world_name = spec.world_name;
        snapshot.document_kind = std::string{kind_text};
        snapshot.support_tier = "baseline";
        snapshot.revision = 1u;
        if (include_engine_arcade_package)
            snapshot.packages.emplace_back("engine_arcade");
        snapshot.objects.reserve(entities.size());

        for (const EditorSceneSeedEntity& entity : entities)
        {
            epochengine::scene::SceneObjectSnapshot object{};
            object.id = epochengine::scene::stable_scene_object_id(snapshot.scene_id, entity.name);
            object.name = entity.name;
            object.type = entity.type;
            object.category = entity.category;
            object.position = entity.position;
            object.rotation = entity.rotation;
            object.scale = entity.scale;
            object.visible = entity.visible;
            object.editor_only = entity.editor_only;
            if (snapshot.primary_camera == epochengine::scene::kInvalidSceneObjectId && object.type == "Camera")
                snapshot.primary_camera = object.id;
            if (snapshot.primary_spawn == epochengine::scene::kInvalidSceneObjectId && object.type == "Spawn")
                snapshot.primary_spawn = object.id;
            snapshot.objects.emplace_back(std::move(object));
        }

        epochengine::scene::normalize_scene_document(snapshot);
        return epochengine::scene::serialize_snapshot_text(snapshot);
    }

    [[nodiscard]] static constexpr std::string_view engine_arcade_scene_ids() noexcept
    {
        return epochengine::package_registry::engine_arcade_scene_ids();
    }

    [[nodiscard]] static std::string json_array_from_csv(std::string_view csv)
    {
        std::string result{ "[" };
        std::size_t start = 0;
        bool first = true;
        while (start < csv.size())
        {
            const std::size_t comma = csv.find(',', start);
            const std::size_t end = comma == std::string_view::npos ? csv.size() : comma;
            const std::string_view token = csv.substr(start, end - start);
            if (!token.empty())
            {
                if (!first)
                    result += ", ";
                result += "\"" + json_escape(token) + "\"";
                first = false;
            }
            if (comma == std::string_view::npos)
                break;
            start = comma + 1;
        }
        result += "]";
        return result;
    }

    [[nodiscard]] static std::string make_engine_arcade_script_text(std::string_view script_api_include)
    {
        const std::string sceneIds{ epochengine::package_registry::engine_arcade_scene_ids() };
        const std::string defaultScene{ epochengine::package_registry::engine_arcade_default_scene_id() };
        return std::string(script_api_include)
            + "namespace\n"
            + "{\n"
            + "    void host_log(EpochScriptHost* host, const char* message)\n"
            + "    {\n"
            + "        if (host && host->log)\n"
            + "            host->log(host->user_data, message);\n"
            + "    }\n"
            + "}\n\n"
            + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
            + "{\n"
            + "    if (!host)\n"
            + "        return;\n\n"
            + "    host_log(host, \"engine_arcade_scene: package exposes kernel-owned mini-runtime scenes.\");\n"
            + "    host_log(host, \"engine_arcade_scene: available scenes: " + sceneIds + ".\");\n\n"
            + "    if (!host->request_engine_scene)\n"
            + "    {\n"
            + "        host_log(host, \"engine_arcade_scene: engine scene callback unavailable.\");\n"
            + "        return;\n"
            + "    }\n\n"
            + "    const int result = host->request_engine_scene(host->user_data, \"" + defaultScene + "\");\n"
            + "    host_log(host, result >= 0\n"
            + "        ? \"engine_arcade_scene: selected '" + defaultScene + "'; use Run to launch the engine-owned scene.\"\n"
            + "        : \"engine_arcade_scene: engine rejected built-in scene request.\");\n"
            + "}\n";
    }

    [[nodiscard]] static std::string make_engine_arcade_package_manifest_text()
    {
        const std::string sceneArray = json_array_from_csv(epochengine::package_registry::engine_arcade_scene_ids());
        const std::string defaultScene{ epochengine::package_registry::engine_arcade_default_scene_id() };
        const std::string renderAssetRole{ epochengine::package_registry::engine_arcade_render_asset_role() };
        const std::string rendererRequirements{ epochengine::package_registry::engine_arcade_renderer_requirements() };
        const std::string renderTextureName{ epochengine::package_registry::engine_arcade_render_texture_name() };
        const std::string renderTextureWidth =
            std::to_string(epochengine::package_registry::engine_arcade_render_texture_width());
        const std::string renderTextureHeight =
            std::to_string(epochengine::package_registry::engine_arcade_render_texture_height());
        return std::string{
            "{\n"
            "  \"package_id\": \"engine_arcade\",\n"
            "  \"display_name\": \"Engine Arcade Runtime Package\",\n"
            "  \"kind\": \"kernel-engine-asset-script-package\",\n"
            "  \"ownership\": \"engine-owned; project-selectable\",\n"
            "  \"default_script\": \"engine_arcade_scene\",\n"
            "  \"default_scene\": \"" + json_escape(defaultScene) + "\",\n"
            "  \"runtime_role\": \"built-in scenes for render-to-texture arcade cabinets and in-game terminals\",\n"
            "  \"render_asset_role\": \"" + json_escape(renderAssetRole) + "\",\n"
            "  \"renderer_requirements\": \"" + json_escape(rendererRequirements) + "\",\n"
            "  \"render_texture_name\": \"" + json_escape(renderTextureName) + "\",\n"
            "  \"render_texture_width\": " + renderTextureWidth + ",\n"
            "  \"render_texture_height\": " + renderTextureHeight + ",\n"
            "  \"scenes\": " + sceneArray + ",\n"
            "  \"source_policy\": \"do not copy game implementations into generated projects; invoke engine kernel modules through script host callbacks\"\n"
            "}\n"
        };
    }

    [[nodiscard]] static std::string make_project_bootstrap_script_text(
        const ProjectShellSpec& spec,
        std::string_view script_api_include)
    {
        if (spec.project_id == "sandbox")
        {
            return std::string(script_api_include)
                + "namespace\n"
                + "{\n"
                + "    void host_log(EpochScriptHost* host, const char* message)\n"
                + "    {\n"
                + "        if (host && host->log)\n"
                + "            host->log(host->user_data, message);\n"
                + "    }\n"
                + "}\n\n"
                + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                + "{\n"
                + "    if (!host)\n"
                + "        return;\n\n"
                + "    host_log(host, \"engine_development_harness: sandbox validates proposed Epoch changes without training or modifying an AI model.\");\n"
                + "    host_log(host, \"engine_development_harness: review staged changes, build evidence, and editor behavior before promotion.\");\n"
                + "}\n";
        }

        if (spec.kind == EditorProjectKind::Tool || spec.kind == EditorProjectKind::Editor)
        {
            return std::string(script_api_include)
                + "namespace\n"
                + "{\n"
                + "    void host_log(EpochScriptHost* host, const char* message)\n"
                + "    {\n"
                + "        if (host && host->log)\n"
                + "            host->log(host->user_data, message);\n"
                + "    }\n"
                + "}\n\n"
                + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                + "{\n"
                + "    if (!host)\n"
                + "        return;\n\n"
                + "    host_log(host, \"tool_bootstrap: project shell ready.\");\n"
                + "}\n";
        }

        return std::string(script_api_include)
            + "#include <string>\n\n"
            + "namespace\n"
            + "{\n"
            + "    void host_log(EpochScriptHost* host, const char* message)\n"
            + "    {\n"
            + "        if (host && host->log)\n"
            + "            host->log(host->user_data, message);\n"
            + "    }\n"
            + "}\n\n"
            + "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
            + "{\n"
            + "    if (!host)\n"
            + "        return;\n\n"
            + "    host_log(host, \"project_demo_bootstrap: project shell ready.\");\n"
            + "    if (host->project_model_asset && host->project_model_asset[0] != '\\0')\n"
            + "    {\n"
            + "        std::string message = std::string(\"project_demo_bootstrap: demo model asset -> \") + host->project_model_asset;\n"
            + "        host_log(host, message.c_str());\n"
            + "        if (host->queue_model_load)\n"
            + "        {\n"
            + "            const int result = host->queue_model_load(host->user_data, \"mini_sponza_v2\", host->project_model_asset);\n"
            + "            host_log(host, result >= 0\n"
            + "                ? \"project_demo_bootstrap: queued demo model load.\"\n"
            + "                : \"project_demo_bootstrap: backend rejected demo model load.\");\n"
            + "        }\n"
            + "        else\n"
            + "        {\n"
            + "            host_log(host, \"project_demo_bootstrap: model-load callback unavailable.\");\n"
            + "        }\n"
            + "    }\n"
            + "    else\n"
            + "    {\n"
            + "        host_log(host, \"project_demo_bootstrap: no demo model asset declared for this project.\");\n"
            + "    }\n\n"
            + "    if (host->rotate_all_entities_yaw)\n"
            + "    {\n"
            + "        host->rotate_all_entities_yaw(host->user_data, 6.0f);\n"
            + "        host_log(host, \"project_demo_bootstrap: applied a light +6 yaw demo step.\");\n"
            + "    }\n"
            + "    else\n"
            + "    {\n"
            + "        host_log(host, \"project_demo_bootstrap: rotate callback unavailable.\");\n"
            + "    }\n"
            + "}\n";
    }

    [[nodiscard]] static std::string make_project_entry_run_text(EditorProjectKind kind)
    {
        if (kind == EditorProjectKind::Editor)
        {
            return
                "    std::vector<char*> editor_arguments(argv, argv + argc);\n"
                "    char editor_mode[] = \"--editor\";\n"
                "    editor_arguments.push_back(editor_mode);\n"
                "    epochengine::core::ParseCommandLine(static_cast<int>(editor_arguments.size()), editor_arguments.data());\n"
                "    epochengine::core::RunEditorInterface();\n";
        }
        return
            "    epochengine::core::ParseCommandLine(argc, argv);\n"
            "    epochengine::core::RunEngine();\n";
    }

    [[nodiscard]] static bool generated_project_source_edition_allowed(const fs::path& root)
    {
        if (root.empty() || !root.is_absolute())
            return false;
        std::error_code error;
        const auto marker = root / "Engine/include/epoch.edition.hpp";
        if (!fs::is_regular_file(marker, error) || error
            || fs::is_symlink(marker, error) || error
            || fs::file_size(marker, error) > 4096u || error
            || !epochengine::updater::distribution::marker_matches(read_text_file(marker)))
            return false;
        if constexpr (epochengine::updater::distribution::public_edition)
        {
            // The generated configure/build guard runs the complete source
            // inventory/hash gate before any engine dependency can be linked.
            const auto manifest = root / "source-projection.json";
            return fs::is_regular_file(manifest, error) && !error
                && !fs::is_symlink(manifest, error) && !error
                && fs::is_regular_file(root / "Tools/build/verify_public_source.py", error) && !error;
        }
        return true;
    }

    [[nodiscard]] static EditorProjectCreationResult write_project_shell(const ProjectShellSpec& spec)
    {
        const fs::path root = resolve_project_root_path(spec.root);
        if (root.empty())
            return {.project_id = spec.project_id,
                .summary = "Project creation refused an unavailable or non-private candidate project root."};
        const fs::path repoRoot = resolve_epoch_repo_root(root);
        if (!generated_project_source_edition_allowed(repoRoot))
            return {.project_id = spec.project_id, .root_path = root.generic_string(),
                .summary = "Project creation refused missing or foreign-edition engine source. Select this edition's verified source checkout."};
        const std::string sourceEdition{epochengine::updater::distribution::edition_id};
        const fs::path worlds = root / "worlds";
        const fs::path scripts = root / "scripts";
        const fs::path source = root / "source";
        const fs::path include = root / "include";
        const fs::path modules = root / "modules";
        const fs::path assets = root / "assets";
        const fs::path assetPackages = assets / "packages";
        const fs::path resource = root / "resource";
        const fs::path buildRoot = root / "build";
        const fs::path buildLogs = buildRoot / "logs";
        const fs::path outputDebugDir = root / "build" / "bin" / "windows" / "Debug" / "x64";
        const fs::path outputReleaseDir = root / "build" / "bin" / "windows" / "Release" / "x64";
        const fs::path outputLinuxDebugDir = root / "build" / "bin" / "linux" / "Debug" / "x64";
        const fs::path outputLinuxReleaseDir = root / "build" / "bin" / "linux" / "Release" / "x64";
        const fs::path manifest = root / "project.epoch.json";
        const fs::path readme = root / "README.md";
        const fs::path pathsFile = root / "project.paths.txt";
        const fs::path cmakeFragment = root / "epoch.project.cmake";
        const fs::path cmakeLists = generated_project_cmake_lists_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsBuildScript = generated_project_windows_build_script_path(root);
        const fs::path linuxBuildScript = generated_project_linux_build_script_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        const fs::path aiProfile = root / "Assets" / "AI" / "project_ai.epochai";
        if (!spec.input_profile_path.empty()
            && spec.input_profile_path
                != epochengine::project_input::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Project input profile path must use the canonical Assets/Config location."
            };
        }
        if (!spec.sprite_animation_path.empty()
            && spec.sprite_animation_path
                != epochengine::project_sprite_animation::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Sprite animation path must use the canonical Assets/Animations location."
            };
        }
        if (!spec.audio_profile_path.empty()
            && spec.audio_profile_path
                != epochengine::project_audio::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Audio profile path must use the canonical Assets/Audio location."
            };
        }
        if (!spec.gui_path.empty()
            && spec.gui_path != epochengine::project_gui::canonical_source_path)
        {
            return EditorProjectCreationResult{
                .succeeded = false,
                .project_id = spec.project_id,
                .root_path = root.generic_string(),
                .manifest_path = manifest.generic_string(),
                .summary = "Project GUI path must use the canonical Assets/Gui location."
            };
        }        const fs::path worldFile = spec.world_file.is_absolute()
            ? spec.world_file.lexically_normal()
            : resolve_repo_relative_path(spec.world_file, root);
        const fs::path scriptFile = scripts / (spec.script_id + ".ascript.cpp");
        if (const fs::path data = epochengine::core::path::candidate_data_root(); !data.empty())
        {
            if (candidate_project_path(data, worldFile).empty()
                || !project_path_below(worldFile, root)
                || candidate_project_path(data, scriptFile).empty()
                || !project_path_below(scriptFile, scripts))
                return {.project_id = spec.project_id, .root_path = root.generic_string(),
                    .summary = "Candidate project creation refused a scene or script outside its private project."};
        }
        const fs::path engineArcadePackageFile = assetPackages / "engine_arcade.package.json";
        const fs::path engineArcadeScriptFile = scripts / "script.engine_arcade_scene.cpp";
        const fs::path inputProfileFile = spec.input_profile_path.empty()
            ? fs::path{}
            : (root / fs::path{spec.input_profile_path}).lexically_normal();
        const fs::path audioProfileFile = spec.audio_profile_path.empty()
            ? fs::path{}
            : (root / fs::path{spec.audio_profile_path}).lexically_normal();
        const fs::path defaultJumpAudio =
            root / "Assets" / "Audio" / "default_jump.wav";
        const fs::path defaultLandAudio =
            root / "Assets" / "Audio" / "default_land.wav";
        const fs::path defaultAmbientAudio =
            root / "Assets" / "Audio" / "default_ambient.wav";
        const fs::path rootAbsolute = fs::absolute(root).lexically_normal();
        const std::string artifactStem = generated_project_artifact_stem(root);
        const fs::path manifestAbsolute = fs::absolute(manifest).lexically_normal();
        const fs::path repoEngineInclude = (repoRoot / "Engine" / "include").lexically_normal();
        const fs::path repoEngineProject = (repoRoot / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj").lexically_normal();
        const fs::path repoEpochGuiProject = (repoRoot / "Engine" / "dep" / "EpochGui" / "EpochGui.vcxproj").lexically_normal();
        const bool isEngineDevelopmentSandbox = spec.project_id == "sandbox";
        const bool includeEngineArcadePackage = spec.include_engine_arcade_package && !isEngineDevelopmentSandbox;
        const std::string kindText = isEngineDevelopmentSandbox
            ? "engine-development-sandbox"
            : std::string(project_manifest_kind(spec.kind));
        const std::string kindDisplay = isEngineDevelopmentSandbox
            ? "Engine Development Sandbox"
            : std::string(epochengine::editor_project_kind_name(spec.kind));
        const std::string integrationMode = isEngineDevelopmentSandbox
            ? "repo-local guarded engine-development child build for validating Epoch source and editor behavior only"
            : "repo-local static Epoch runtime child build across headers/modules/source/scripting/resources with project-selected runtime boot";
        const std::string shellLabel = isEngineDevelopmentSandbox ? "engine development sandbox" : "project shell";
        const std::string selfTestTitle = isEngineDevelopmentSandbox
            ? "Epoch engine development sandbox self-test"
            : "Epoch generated project shell self-test";
        const std::string versionTitle = isEngineDevelopmentSandbox
            ? "Epoch engine development sandbox host: "
            : "Epoch generated project shell: ";
        const std::string scriptLabel = isEngineDevelopmentSandbox
            ? "Manipulation/test harness script"
            : "Script";
        const std::string publicIncludeRoot = repoEngineInclude.generic_string();
        const std::string projectGuid = deterministic_guid(spec.project_id + ":windows-child");
        const std::string repoRootWin = xml_escape(to_windows_path(repoRoot.string()));
        const std::string engineLibraryRelative = xml_escape(to_windows_path(
            epochengine::build_layout::windows_relative_bin(repoRoot, "$(Configuration)").generic_string())) + "\\";
        const std::string repoRootPowerShell = powershell_escape_single_quoted(to_windows_path(repoRoot.string()));
        const std::string vcpkgManifestRootPowerShell = powershell_escape_single_quoted(
            to_windows_path((repoRoot / "Engine").string()));
        const std::string repoEngineProjectWin = xml_escape(to_windows_path(repoEngineProject.string()));
        const std::string repoEpochGuiProjectWin = xml_escape(to_windows_path(repoEpochGuiProject.string()));
        const std::string manifestAbsoluteText = manifestAbsolute.generic_string();
        const std::string rootAbsoluteText = rootAbsolute.generic_string();
        const std::string aiProfileAbsoluteText =
            (rootAbsolute / "Assets" / "AI" / "project_ai.epochai")
                .generic_string();
        const std::string worldFileAbsoluteText =
            fs::absolute(worldFile).lexically_normal().generic_string();

        std::error_code ec;
        fs::create_directories(worlds, ec);
        fs::create_directories(scripts, ec);
        fs::create_directories(source, ec);
        fs::create_directories(include, ec);
        fs::create_directories(modules, ec);
        fs::create_directories(assets, ec);
        if (spec.include_engine_arcade_package)
            fs::create_directories(assetPackages, ec);
        fs::create_directories(resource, ec);
        fs::create_directories(buildLogs, ec);
        fs::create_directories(outputDebugDir, ec);
        fs::create_directories(outputReleaseDir, ec);
        fs::create_directories(outputLinuxDebugDir, ec);
        fs::create_directories(outputLinuxReleaseDir, ec);
        fs::create_directories(worldFile.parent_path(), ec);
        if (ec)
        {
            return {
                false,
                spec.project_id,
                root.string(),
                manifest.generic_string(),
                entrySource.generic_string(),
                windowsBuildScript.generic_string(),
                scriptFile.generic_string(),
                "Failed to create project shell directories.",
                integrationMode,
                publicIncludeRoot
            };
        }

        const std::string scriptApiInclude =
            "#if __has_include(<scripting.epoch_api.h>)\n"
            "#  include <scripting.epoch_api.h>\n"
            "#elif __has_include(<include/scripting.epoch_api.h>)\n"
            "#  include <include/scripting.epoch_api.h>\n"
            "#else\n"
            "#  error \"Epoch script API header not found. Add Engine/include (preferred) or Engine/ to your include paths.\"\n"
            "#endif\n\n";

        const std::string demoModelLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "  \"demo_model_asset\": \"" + json_escape(spec.demo_model_asset) + "\",\n";
        const std::string tileMapManifestLine = spec.tilemap_path.empty()
            ? std::string{}
            : "  \"tilemap\": \"" + json_escape(spec.tilemap_path) + "\",\n";
        const std::string inputProfileManifestLine = spec.input_profile_path.empty()
            ? std::string{}
            : "  \"input_profile\": \"" + json_escape(spec.input_profile_path) + "\",\n";
        const std::string spriteAnimationManifestLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "  \"sprite_animation\": \""
                + json_escape(spec.sprite_animation_path) + "\",\n";
        const std::string audioProfileManifestLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "  \"audio_profile\": \""
                + json_escape(spec.audio_profile_path) + "\",\n";
        const std::string guiManifestLine = spec.gui_path.empty()
            ? std::string{}
            : "  \"gui\": \"" + json_escape(spec.gui_path) + "\",\n";
        const std::string packageManifestLine = includeEngineArcadePackage
            ? std::string{ "  \"engine_asset_packages\": [\"engine_arcade\"],\n"
                "  \"engine_arcade_default_scene\": \"" }
                + json_escape(epochengine::package_registry::engine_arcade_default_scene_id()) + "\",\n"
                + "  \"engine_arcade_scenes\": " + json_array_from_csv(epochengine::package_registry::engine_arcade_scene_ids()) + ",\n"
                + "  \"engine_arcade_render_asset_role\": \"" + json_escape(epochengine::package_registry::engine_arcade_render_asset_role()) + "\",\n"
                + "  \"engine_arcade_renderer_requirements\": \"" + json_escape(epochengine::package_registry::engine_arcade_renderer_requirements()) + "\",\n"
                + "  \"engine_arcade_render_texture_name\": \"" + json_escape(epochengine::package_registry::engine_arcade_render_texture_name()) + "\",\n"
                + "  \"engine_arcade_render_texture_width\": " + std::to_string(epochengine::package_registry::engine_arcade_render_texture_width()) + ",\n"
                + "  \"engine_arcade_render_texture_height\": " + std::to_string(epochengine::package_registry::engine_arcade_render_texture_height()) + ",\n"
            : std::string{};
        const std::string readmeDemoLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "- Demo model asset: " + spec.demo_model_asset + "\n";
        const std::string readmeTileMapLine = spec.tilemap_path.empty()
            ? std::string{}
            : "- Canonical tile map: " + spec.tilemap_path + "\n";
        const std::string readmeInputProfileLine = spec.input_profile_path.empty()
            ? std::string{}
            : "- Canonical input profile: " + spec.input_profile_path + "\n";
        const std::string readmeSpriteAnimationLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "- Canonical sprite animations: "
                + spec.sprite_animation_path + "\n";
        const std::string readmeAudioProfileLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "- Canonical audio profile: "
                + spec.audio_profile_path + "\n";
        const std::string readmeGuiLine = spec.gui_path.empty()
            ? std::string{}
            : "- Canonical GUI document: " + spec.gui_path + "\n";
        const std::string readmePackageLine = includeEngineArcadePackage
            ? "- Engine asset package: engine_arcade (kernel-owned mini-runtime scenes for 512x512 render-to-texture arcade assets)\n"
            : std::string{};
        const std::string pathsDemoLine = spec.demo_model_asset.empty()
            ? std::string{}
            : "demo_model_asset=" + spec.demo_model_asset + "\n";
        const std::string pathsTileMapLine = spec.tilemap_path.empty()
            ? std::string{}
            : "tilemap=" + spec.tilemap_path + "\n";
        const std::string pathsInputProfileLine = spec.input_profile_path.empty()
            ? std::string{}
            : "input_profile=" + spec.input_profile_path + "\n";
        const std::string pathsSpriteAnimationLine =
            spec.sprite_animation_path.empty()
            ? std::string{}
            : "sprite_animation=" + spec.sprite_animation_path + "\n";
        const std::string pathsAudioProfileLine =
            spec.audio_profile_path.empty()
            ? std::string{}
            : "audio_profile=" + spec.audio_profile_path + "\n";
        const std::string pathsGuiLine = spec.gui_path.empty()
            ? std::string{}
            : "gui=" + spec.gui_path + "\n";
        const std::string pathsPackageLine = includeEngineArcadePackage
            ? "engine_arcade_package=" + engineArcadePackageFile.generic_string() + "\n"
              "engine_arcade_script=" + engineArcadeScriptFile.generic_string() + "\n"
              "engine_arcade_scenes=" + std::string(engine_arcade_scene_ids()) + "\n"
              "engine_arcade_render_asset_role=" + std::string(epochengine::package_registry::engine_arcade_render_asset_role()) + "\n"
              "engine_arcade_renderer_requirements=" + std::string(epochengine::package_registry::engine_arcade_renderer_requirements()) + "\n"
              "engine_arcade_render_texture_name=" + std::string(epochengine::package_registry::engine_arcade_render_texture_name()) + "\n"
              "engine_arcade_render_texture_width=" + std::to_string(epochengine::package_registry::engine_arcade_render_texture_width()) + "\n"
              "engine_arcade_render_texture_height=" + std::to_string(epochengine::package_registry::engine_arcade_render_texture_height()) + "\n"
            : std::string{};

        const std::string manifestText =
            "{\n"
            "  \"engine\": \"epoch\",\n"
            "  \"project_format\": \"epoch-project-v1\",\n"
            "  \"id\": \"" + json_escape(spec.project_id) + "\",\n"
            "  \"display_name\": \"" + json_escape(spec.project_name) + "\",\n"
            "  \"kind\": \"" + kindText + "\",\n"
            "  \"template_family\": \"" + json_escape(spec.template_family) + "\",\n"
            "  \"engine_edition\": \"" + json_escape(sourceEdition) + "\",\n"
            "  \"scene\": \"" + json_escape(worldFile.generic_string()) + "\",\n"
            "  \"default_script\": \"" + json_escape(spec.script_id) + "\",\n"
            + demoModelLine
            + tileMapManifestLine
            + inputProfileManifestLine
            + spriteAnimationManifestLine
            + audioProfileManifestLine
            + guiManifestLine
            + packageManifestLine
            + "  \"ai_profile\": \"Assets/AI/project_ai.epochai\",\n"
            + "  \"engine_integration\": \"" + json_escape(integrationMode) + "\",\n"
            "  \"build_profile\": \"epoch-runtime-static\",\n"
            "  \"public_include_root\": \"" + json_escape(publicIncludeRoot) + "\",\n"
            "  \"capability_profile\": \"" + json_escape(spec.capability_profile) + "\",\n"
            "  \"engine_module_root\": \"Engine/modules\",\n"
            "  \"engine_source_root\": \"Engine/src\",\n"
            "  \"engine_script_root\": \"Engine/src/scripts\",\n"
            "  \"engine_resource_root\": \"Engine/resource\",\n"
            "  \"build_fragment\": \"" + json_escape(cmakeFragment.filename().generic_string()) + "\",\n"
            "  \"entry_source\": \"" + json_escape(entrySource.generic_string()) + "\",\n"
            "  \"windows_project\": \"" + json_escape(windowsProject.filename().generic_string()) + "\",\n"
            "  \"windows_build_script\": \"" + json_escape(windowsBuildScript.filename().generic_string()) + "\",\n"
            "  \"linux_build_script\": \"" + json_escape(linuxBuildScript.filename().generic_string()) + "\",\n"
            "  \"support_tier\": \"baseline\"\n"
            "}\n";

        const std::string readmeText =
            "# " + spec.project_name + "\n\n"
            "Generated or repaired by the Epoch editor " + shellLabel + " flow.\n\n"
            "- Kind: " + kindDisplay + "\n"
            "- Scene: " + worldFile.filename().string() + "\n"
            "- " + scriptLabel + ": " + scriptFile.filename().string() + "\n"
            + readmeDemoLine
            + readmeTileMapLine
            + readmeInputProfileLine
            + readmeSpriteAnimationLine
            + readmeAudioProfileLine
            + readmeGuiLine
            + readmePackageLine
            + "- Engine integration: " + integrationMode + "\n"
            "- Build profile: epoch-runtime-static\n"
            "- Public include root: " + publicIncludeRoot + "\n"
            "- Capability profile: " + spec.capability_profile + "\n"
            "- Engine module root: Engine/modules\n"
            "- Engine source root: Engine/src\n"
            "- Engine script root: Engine/src/scripts\n"
            "- Engine resource root: Engine/resource\n"
            "- Project include root: " + include.generic_string() + "\n"
            "- Project module root: " + modules.generic_string() + "\n"
            "- Project resource root: " + resource.generic_string() + "\n"
            "- Entry source: " + entrySource.generic_string() + "\n"
            "- Windows project: " + windowsProject.filename().string() + "\n"
            "- Windows build script: " + windowsBuildScript.filename().string() + "\n"
            "- Linux build script: " + linuxBuildScript.filename().string() + "\n"
            "- Build fragment: " + cmakeFragment.filename().string() + "\n";

        const std::string pathsText =
            "root=" + root.generic_string() + "\n"
            + "manifest=" + manifest.generic_string() + "\n"
            + "scene=" + worldFile.generic_string() + "\n"
            + "default_script=" + scriptFile.generic_string() + "\n"
            + pathsDemoLine
            + pathsTileMapLine
            + pathsInputProfileLine
            + pathsSpriteAnimationLine
            + pathsAudioProfileLine
            + pathsGuiLine
            + pathsPackageLine
            + "ai_profile=" + aiProfile.generic_string() + "\n"
            + "entry_source=" + entrySource.generic_string() + "\n"
            + "windows_project=" + windowsProject.generic_string() + "\n"
            + "windows_build_script=" + windowsBuildScript.generic_string() + "\n"
            + "linux_build_script=" + linuxBuildScript.generic_string() + "\n"
            + "build_log=" + generated_project_build_log_path(root).generic_string() + "\n"
            + "debug_output=" + generated_project_output_path(root).generic_string() + "\n";

        const std::string worldText = make_project_world_scene_text(spec, kindText, includeEngineArcadePackage);
        auto aiProvider = epochengine::ai::project_profile::Provider::disabled;
        const auto defaultAiProfile =
            epochengine::ai::project_profile::serialize_profile(
                epochengine::ai::project_profile::make_profile(
                    aiProvider));
        const std::string defaultAiProfileText = defaultAiProfile
            ? defaultAiProfile.canonical_bytes : std::string{};


        const auto defaultInputProfile =
            epochengine::project_input::serialize_profile_source(
                epochengine::project_input::make_legacy_default_profile());
        const std::string defaultInputProfileBytes = defaultInputProfile
            ? std::string{
                reinterpret_cast<const char*>(defaultInputProfile.bytes.data()),
                defaultInputProfile.bytes.size()}
            : std::string{};

        const auto defaultAudioProfile =
            epochengine::project_audio::serialize_profile(
                epochengine::project_audio::make_default_2d_profile());
        const std::string defaultAudioProfileBytes = defaultAudioProfile
            ? std::string{
                reinterpret_cast<const char*>(
                    defaultAudioProfile.bytes.data()),
                defaultAudioProfile.bytes.size()}
            : std::string{};
        const std::string defaultJumpAudioBytes =
            make_project_pcm16_wave(660.0f, 95u, 0.24f, true);
        const std::string defaultLandAudioBytes =
            make_project_pcm16_wave(165.0f, 75u, 0.20f, true);
        const std::string defaultAmbientAudioBytes =
            make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false);

        const std::string scriptText = make_project_bootstrap_script_text(spec, scriptApiInclude);
        const std::string engineArcadeScriptText = make_engine_arcade_script_text(scriptApiInclude);
        const std::string engineArcadePackageText = make_engine_arcade_package_manifest_text();

        const std::string entrySourceText =
            "// EPOCH_MANAGED_GENERATED_FILE: project_entry_v1\n"
            "#include <cstdlib>\n"
            "#include <cstring>\n"
            "#include <string>\n"
            "#include <vector>\n"
            "#if defined(_WIN32)\n"
            "#  include <stdlib.h>\n"
            "#else\n"
            "#  include <unistd.h>\n"
            "#endif\n"
            "#include <epoch.edition.hpp>\n"
            "#include <epoch.engine.hpp>\n\n"
            "static_assert(EPOCH_SOURCE_EDITION_PUBLIC == "
            + std::string{epochengine::updater::distribution::public_edition ? "1" : "0"}
            + ", \"Generated project source edition differs\");\n\n"
            +
            "extern \"C\" void core_log_write(unsigned int lvl, const char* tag_utf8, const char* msg_utf8);\n\n"
            "namespace\n"
            "{\n"
            "    bool has_arg(int argc, char** argv, const char* needle) noexcept\n"
            "    {\n"
            "        if (!needle)\n"
            "            return false;\n"
            "        for (int i = 1; i < argc; ++i)\n"
            "        {\n"
            "            if (argv && argv[i] && std::strcmp(argv[i], needle) == 0)\n"
            "                return true;\n"
            "        }\n"
            "        return false;\n"
            "    }\n\n"
            "    void boot_project_shell()\n"
            "    {\n"
            "#if defined(_WIN32)\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(spec.project_id) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        _putenv_s(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\");\n"
            "        _putenv_s(\"EPOCH_PROJECT_AI_PROFILE\", \"" + cxx_escape(aiProfileAbsoluteText) + "\");\n"
            "#else\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ID\", \"" + cxx_escape(spec.project_id) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_MANIFEST\", \"" + cxx_escape(manifestAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_EDITOR_PROJECT_ROOT\", \"" + cxx_escape(rootAbsoluteText) + "\", 1);\n"
            "        setenv(\"EPOCH_PROJECT_AI_PROFILE\", \"" + cxx_escape(aiProfileAbsoluteText) + "\", 1);\n"
            "#endif\n"
            "    }\n"
            "\n"
            "    void log_project_shell_self_test_line(const char* message) noexcept\n"
            "    {\n"
            "        core_log_write(1u, \"Epoch.ChildProject\", message ? message : \"\");\n"
            "    }\n\n"
            "    int log_project_shell_self_test() noexcept\n"
            "    {\n"
            "        log_project_shell_self_test_line(\"" + cxx_escape(selfTestTitle) + "\");\n"
            "        log_project_shell_self_test_line(\"project_id=" + cxx_escape(spec.project_id) + "\");\n"
            "        log_project_shell_self_test_line(\"project_name=" + cxx_escape(spec.project_name) + "\");\n"
            "        log_project_shell_self_test_line(\"project_root=" + cxx_escape(rootAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"manifest=" + cxx_escape(manifestAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"ai_profile=" + cxx_escape(aiProfileAbsoluteText) + "\");\n"
            "        log_project_shell_self_test_line(\"engine_integration=" + cxx_escape(integrationMode) + "\");\n"
            "        log_project_shell_self_test_line(\"project_format=epoch-project-v1\");\n"
            "        log_project_shell_self_test_line(\"build_profile=epoch-runtime-static\");\n"
            "        const epochengine::project::ArtifactAcceptanceRequest request{\n"
            "            .project_id = \"" + cxx_escape(spec.project_id) + "\",\n"
            "            .project_root = \"" + cxx_escape(rootAbsoluteText) + "\",\n"
            "            .scene_path = \"" + cxx_escape(worldFileAbsoluteText) + "\",\n"
            "            .tilemap_path = \"" + cxx_escape(spec.tilemap_path) + "\",\n"
            "            .input_profile_path = \"" + cxx_escape(spec.input_profile_path) + "\",\n"
            "            .sprite_animation_path = \"" + cxx_escape(spec.sprite_animation_path) + "\",\n"
            "            .audio_profile_path = \"" + cxx_escape(spec.audio_profile_path) + "\",\n"
            "            .gui_path = \"" + cxx_escape(spec.gui_path) + "\",\n"
            "            .scene_requires_runtime = " + std::string(spec.kind == EditorProjectKind::Game || spec.kind == EditorProjectKind::EngineDevelopment ? "true" : "false") + ",\n"
            "            .progress_sink = log_project_shell_self_test_line};\n"
            "        const auto acceptance = epochengine::project::VerifyArtifacts(request);\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.required_mask=\"} + std::to_string(acceptance.required_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.verified_mask=\"} + std::to_string(acceptance.verified_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.stage=\"} + acceptance.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"artifact_acceptance.diagnostic=\"} + acceptance.diagnostic).c_str());\n"
            "        const bool has_derived_artifacts = !request.tilemap_path.empty()\n"
            "            || !request.input_profile_path.empty()\n"
            "            || !request.sprite_animation_path.empty()\n"
            "            || !request.audio_profile_path.empty()\n"
            "            || !request.gui_path.empty();\n"
            "        const auto regeneration = !acceptance.succeeded\n"
            "            ? epochengine::project::ArtifactRegenerationReport{}\n"
            "            : has_derived_artifacts\n"
            "                ? epochengine::project::VerifyLibraryRegeneration(request)\n"
            "                : epochengine::project::ArtifactRegenerationReport{\n"
            "                    .succeeded = true, .stage = \"not_required\",\n"
            "                    .diagnostic = \"project declares no derived library artifacts\"};\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.required_mask=\"} + std::to_string(regeneration.required_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.regenerated_mask=\"} + std::to_string(regeneration.regenerated_mask)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.source_files_verified=\"} + std::to_string(regeneration.source_files_verified)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.artifact_files_quarantined=\"} + std::to_string(regeneration.artifact_files_quarantined)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.stage=\"} + regeneration.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"library_regeneration.diagnostic=\"} + regeneration.diagnostic).c_str());\n"
            "        const bool needs_playable_2d = " + std::string(spec.kind == EditorProjectKind::Game ? "true" : "false") + "\n"
            "            && !request.tilemap_path.empty() && !request.input_profile_path.empty();\n"
            "        const auto gameplay = !acceptance.succeeded || !regeneration.succeeded\n"
            "            ? epochengine::project::GameplayAcceptanceReport{}\n"
            "            : needs_playable_2d\n"
            "                ? epochengine::project::VerifyPlayable2D(request)\n"
            "                : epochengine::project::GameplayAcceptanceReport{\n"
            "                    .succeeded = true, .stage = \"not_required\",\n"
            "                    .diagnostic = \"profile does not declare playable-2D validation inputs\"};\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.input_frames=\"} + std::to_string(gameplay.input_frames)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.fixed_steps=\"} + std::to_string(gameplay.fixed_steps)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.animation_samples=\"} + std::to_string(gameplay.animation_samples)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.audio_triggers=\"} + std::to_string(gameplay.audio_triggers)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.canvas_hash=\"} + std::to_string(gameplay.canvas_hash)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.logical_texture_bytes=\"} + std::to_string(gameplay.logical_texture_bytes)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.emitted_sprites=\"} + std::to_string(gameplay.emitted_sprites)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.emitted_batches=\"} + std::to_string(gameplay.emitted_batches)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.collision_surfaces=\"} + std::to_string(gameplay.collision_surfaces)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.peak_contacts_per_step=\"} + std::to_string(gameplay.peak_contacts_per_step)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.audio_resident_bytes=\"} + std::to_string(gameplay.audio_resident_bytes)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.canvas_rejections=\"} + std::to_string(gameplay.canvas_rejections)).c_str());\n"
            "        log_project_shell_self_test_line(\"gameplay_acceptance.portable_budget_profile=T1-GLES/mobile_30\");\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.portable_budget_violations=\"} + std::to_string(gameplay.portable_budget_violations)).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.stage=\"} + gameplay.stage).c_str());\n"
            "        log_project_shell_self_test_line((std::string{\"gameplay_acceptance.diagnostic=\"} + gameplay.diagnostic).c_str());\n"
            "        return acceptance.succeeded && regeneration.succeeded && gameplay.succeeded ? 0 : 2;\n"
            "    }\n"
            "}\n\n"
            "int main(int argc, char** argv)\n"
            "{\n"
            "    if (has_arg(argc, argv, \"--version\") || has_arg(argc, argv, \"-v\"))\n"
            "    {\n"
            "        log_project_shell_self_test_line(\"" + cxx_escape(versionTitle + spec.project_name) + "\");\n"
            "        return 0;\n"
            "    }\n"
            "    if (has_arg(argc, argv, \"--project-self-test\"))\n"
            "    {\n"
            "        boot_project_shell();\n"
            "        return log_project_shell_self_test();\n"
            "    }\n"
            "    boot_project_shell();\n"
            + make_project_entry_run_text(spec.kind) +
            "    return 0;\n"
            "}\n";

        const std::string sourceAdmissionCmake =
            "find_package(Python3 REQUIRED COMPONENTS Interpreter)\n"
            "execute_process(COMMAND \"${Python3_EXECUTABLE}\" \"${EPOCH_REPO_ROOT}/Tools/build/verify_epoch_project_source.py\"\n"
            "    --root \"${EPOCH_REPO_ROOT}\" --edition \"" + cxx_escape(sourceEdition) + "\"\n"
            "    RESULT_VARIABLE epoch_source_admission)\n"
            "if(NOT epoch_source_admission EQUAL 0)\n"
            "    message(FATAL_ERROR \"Generated project source edition/content verification failed\")\n"
            "endif()\n"
            "set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS \"${EPOCH_REPO_ROOT}/Engine/include/epoch.edition.hpp\" \"${EPOCH_REPO_ROOT}/source-projection.json\")\n"
            "add_custom_target(epoch_generated_source_guard\n"
            "    COMMAND \"${Python3_EXECUTABLE}\" \"${EPOCH_REPO_ROOT}/Tools/build/verify_epoch_project_source.py\"\n"
            "        --root \"${EPOCH_REPO_ROOT}\" --edition \"" + cxx_escape(sourceEdition) + "\" VERBATIM)\n"
            "\n";

        const std::string cmakeText =
            "# EPOCH_MANAGED_GENERATED_FILE: cmake_fragment_v2\n"
            "cmake_minimum_required(VERSION 4.4)\n\n"
            "set(EPOCH_REPO_ROOT \"" + json_escape(repoRoot.generic_string()) + "\" CACHE PATH \"Path to the repo-local Epoch checkout\")\n"
            "set(BUILD_TESTING OFF CACHE BOOL \"Generated projects build runtime targets only\" FORCE)\n"
            "set(EPOCH_BUILD_STATIC_RUNTIME ON CACHE BOOL \"Build the reusable Epoch runtime\" FORCE)\n"
            "set(EPOCH_UPDATER_SHELL_BUILD OFF CACHE BOOL \"Generated projects are not updater shells\" FORCE)\n\n"
            "if(NOT EXISTS \"${EPOCH_REPO_ROOT}/Engine/CMakeLists.txt\")\n"
            "    message(FATAL_ERROR \"EPOCH_REPO_ROOT does not contain Engine/CMakeLists.txt: ${EPOCH_REPO_ROOT}\")\n"
            "endif()\n\n"
            + sourceAdmissionCmake
            + "add_subdirectory(\"${EPOCH_REPO_ROOT}/Engine\" \"${CMAKE_BINARY_DIR}/epoch-engine\" EXCLUDE_FROM_ALL)\n\n"
            "add_dependencies(epoch epoch_generated_source_guard)\n"
            "\n"
            "function(epoch_configure_embedded_project target)\n"
            "    if(NOT TARGET ${target})\n"
            "        message(FATAL_ERROR \"epoch_configure_embedded_project target missing: ${target}\")\n"
            "    endif()\n\n"
            "    target_compile_features(${target} PRIVATE cxx_std_23)\n"
            "    target_compile_definitions(${target} PRIVATE ENGINE_STATICLIB=1 EPOCH_MAIN_IN_MAIN_CPP=1)\n"
            "    target_include_directories(${target} PRIVATE\n"
            "        \"${EPOCH_REPO_ROOT}/Engine/include\"\n"
            "        \"${EPOCH_REPO_ROOT}/Engine\")\n"
            "    target_link_libraries(${target} PRIVATE epoch)\n"
            "endfunction()\n";

        const std::string cmakeListsText =
            "# EPOCH_MANAGED_GENERATED_FILE: cmake_lists_v2\n"
            "cmake_minimum_required(VERSION 4.4)\n"
            "project(EpochGeneratedProject LANGUAGES C CXX)\n\n"
            "include(\"${CMAKE_CURRENT_LIST_DIR}/epoch.project.cmake\")\n"
            "add_executable(epoch_project_runtime source/epoch.main.cpp)\n"
            "epoch_configure_embedded_project(epoch_project_runtime)\n"
            // The host editor is longPathAware; generated executables must be
            // too, or their artifact reads fail beyond the legacy MAX_PATH.
            "if(WIN32 AND MSVC)\n"
            "    target_link_options(epoch_project_runtime PRIVATE\n"
            "        \"/MANIFEST:EMBED\"\n"
            "        \"/MANIFESTINPUT:${EPOCH_REPO_ROOT}/Engine/resource/epoch.manifest\")\n"
            "endif()\n"
            "if(WIN32)\n"
            "    set(epoch_project_output_dir \"${CMAKE_CURRENT_LIST_DIR}/build/bin/windows/${CMAKE_BUILD_TYPE}/x64\")\n"
            "else()\n"
            "    set(epoch_project_output_dir \"${CMAKE_CURRENT_LIST_DIR}/build/bin/linux/${CMAKE_BUILD_TYPE}/x64\")\n"
            "endif()\n"
            "set_target_properties(epoch_project_runtime PROPERTIES\n"
            "    OUTPUT_NAME \"" + json_escape(artifactStem) + "\"\n"
            "    RUNTIME_OUTPUT_DIRECTORY \"${epoch_project_output_dir}\")\n"
            "if(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/Assets\")\n"
            "    add_custom_command(TARGET epoch_project_runtime POST_BUILD\n"
            "        COMMAND \"${CMAKE_COMMAND}\" -E copy_directory\n"
            "            \"${CMAKE_CURRENT_LIST_DIR}/Assets\"\n"
            "            \"$<TARGET_FILE_DIR:epoch_project_runtime>/Assets\"\n"
            "        VERBATIM)\n"
            "endif()\n"
            "if(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/assets\")\n"
            "    add_custom_command(TARGET epoch_project_runtime POST_BUILD\n"
            "        COMMAND \"${CMAKE_COMMAND}\" -E copy_directory\n"
            "            \"${CMAKE_CURRENT_LIST_DIR}/assets\"\n"
            "            \"$<TARGET_FILE_DIR:epoch_project_runtime>/Assets\"\n"
            "        VERBATIM)\n"
            "endif()\n"
            "message(STATUS \"Epoch generated child target: epoch_project_runtime\")\n";
        const std::string windowsProjectText =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<!-- EPOCH_MANAGED_GENERATED_FILE: windows_project_v1 -->\n"
            "<Project DefaultTargets=\"Build\" InitialTargets=\"EpochVerifyGeneratedSource\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
            "  <ItemGroup Label=\"ProjectConfigurations\">\n"
            "    <ProjectConfiguration Include=\"Debug|x64\">\n"
            "      <Configuration>Debug</Configuration>\n"
            "      <Platform>x64</Platform>\n"
            "    </ProjectConfiguration>\n"
            "    <ProjectConfiguration Include=\"Release|x64\">\n"
            "      <Configuration>Release</Configuration>\n"
            "      <Platform>x64</Platform>\n"
            "    </ProjectConfiguration>\n"
            "  </ItemGroup>\n"
            "  <ItemGroup>\n"
            "    <ClCompile Include=\"source\\epoch.main.cpp\" />\n"
            "  </ItemGroup>\n"
            "  <ItemGroup>\n"
            "    <ProjectReference Include=\"" + repoEngineProjectWin + "\">\n"
            "      <Project>{BBA639B7-2B54-4E38-90AC-667FC3303475}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootWin + "\\;VcpkgManifestRoot=" + repoRootWin + "\\Engine\\;EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n"
            "    <ProjectReference Include=\"" + repoEpochGuiProjectWin + "\">\n"
            "      <Project>{7B41A9B2-4B7E-4B5D-9B39-68D2408BCA90}</Project>\n"
            "      <ReferenceOutputAssembly>false</ReferenceOutputAssembly>\n"
            "      <LinkLibraryDependencies>false</LinkLibraryDependencies>\n"
            "      <AdditionalProperties>SolutionDir=" + repoRootWin + "\\;PlatformToolset=v143</AdditionalProperties>\n"
            "    </ProjectReference>\n"
            "  </ItemGroup>\n"
            "  <PropertyGroup Label=\"Globals\">\n"
            "    <VCProjectVersion>17.0</VCProjectVersion>\n"
            "    <Keyword>Win32Proj</Keyword>\n"
            "    <ProjectGuid>{" + projectGuid + "}</ProjectGuid>\n"
            "    <RootNamespace>" + xml_escape(spec.project_name) + "</RootNamespace>\n"
            "    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n"
            "  </PropertyGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\" Label=\"Configuration\">\n"
            "    <ConfigurationType>Application</ConfigurationType>\n"
            "    <UseDebugLibraries>true</UseDebugLibraries>\n"
            "    <PlatformToolset>v143</PlatformToolset>\n"
            "    <CharacterSet>Unicode</CharacterSet>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\" Label=\"Configuration\">\n"
            "    <ConfigurationType>Application</ConfigurationType>\n"
            "    <UseDebugLibraries>false</UseDebugLibraries>\n"
            "    <PlatformToolset>v143</PlatformToolset>\n"
            "    <WholeProgramOptimization>false</WholeProgramOptimization>\n"
            "    <CharacterSet>Unicode</CharacterSet>\n"
            "  </PropertyGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\n"
            "  <ImportGroup Label=\"ExtensionSettings\" />\n"
            "  <ImportGroup Label=\"Shared\" />\n"
            "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n"
            "  </ImportGroup>\n"
            "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\n"
            "  </ImportGroup>\n"
            "  <PropertyGroup Label=\"UserMacros\" />\n"
             "  <PropertyGroup>\n"
             "    <EpochRepoRoot>" + repoRootWin + "\\</EpochRepoRoot>\n"
            "    <SolutionDir Condition=\"'$(SolutionDir)'==''\">$(EpochRepoRoot)</SolutionDir>\n"
            "    <VcpkgManifestRoot Condition=\"'$(VcpkgManifestRoot)'==''\">$(EpochRepoRoot)Engine\\</VcpkgManifestRoot>\n"
            "    <EpochExtraDefines Condition=\"'$(EpochExtraDefines)'==''\">EPOCH_MAIN_IN_MAIN_CPP=1</EpochExtraDefines>\n"
             "    <VcpkgTriplet Condition=\"'$(VcpkgTriplet)'==''\">x64-windows</VcpkgTriplet>\n"
             "    <EpochVcpkgInstallRoot Condition=\"'$(EpochVcpkgInstallRoot)'==''\">$(EpochRepoRoot)Engine\\vcpkg_installed\\$(VcpkgTriplet)\\</EpochVcpkgInstallRoot>\n"
             "    <EpochVcpkgNestedInstallRoot>$(EpochVcpkgInstallRoot)$(VcpkgTriplet)\\</EpochVcpkgNestedInstallRoot>\n"
            "    <EpochVcpkgInstallRoot Condition=\"Exists('$(EpochVcpkgNestedInstallRoot)include\\')\">$(EpochVcpkgNestedInstallRoot)</EpochVcpkgInstallRoot>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <OutDir>$(MSBuildThisFileDirectory)build\\bin\\windows\\Debug\\x64\\</OutDir>\n"
            "    <IntDir>$(MSBuildThisFileDirectory)build\\obj\\msvc\\Debug\\x64\\</IntDir>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <OutDir>$(MSBuildThisFileDirectory)build\\bin\\windows\\Release\\x64\\</OutDir>\n"
            "    <IntDir>$(MSBuildThisFileDirectory)build\\obj\\msvc\\Release\\x64\\</IntDir>\n"
            "  </PropertyGroup>\n"
            "  <PropertyGroup Label=\"Vcpkg\">\n"
            "    <VcpkgEnableManifest>true</VcpkgEnableManifest>\n"
            "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'=='' and ('$(VcpkgTriplet)'=='x64-windows-static' or '$(VcpkgTriplet)'=='x64-windows-static-md' or '$(VcpkgTriplet)'=='x86-windows-static' or '$(VcpkgTriplet)'=='x86-windows-static-md')\">true</VcpkgUseStatic>\n"
            "    <VcpkgUseStatic Condition=\"'$(VcpkgUseStatic)'==''\">false</VcpkgUseStatic>\n"
            "    <VcpkgApplocalDeps Condition=\"'$(VcpkgUseStatic)'=='true'\">false</VcpkgApplocalDeps>\n"
            "    <EpochRaylibDllDefine Condition=\"'$(VcpkgUseStatic)'!='true'\">RAYLIB_DLL;</EpochRaylibDllDefine>\n"
            "  </PropertyGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;$(EpochRaylibDllDefine)_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
            "      <ConformanceMode>true</ConformanceMode>\n"
            "      <LanguageStandard>stdcpp23</LanguageStandard>\n"
            "      <LanguageStandard_C>stdc17</LanguageStandard_C>\n"
            "      <AdditionalIncludeDirectories>$(EpochRepoRoot)Engine\\include;$(EpochVcpkgInstallRoot)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n"
            "      <ScanSourceForModuleDependencies>false</ScanSourceForModuleDependencies>\n"
            "      <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>\n"
            "      <CallingConvention>Cdecl</CallingConvention>\n"
            "    </ClCompile>\n"
            "    <Link>\n"
            "      <SubSystem>Console</SubSystem>\n"
            "      <GenerateDebugInformation>true</GenerateDebugInformation>\n"
            "      <AdditionalLibraryDirectories>$(EpochRepoRoot)" + engineLibraryRelative + ";$(EpochVcpkgInstallRoot)debug\\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n"
            "      <AdditionalDependencies>" + std::string(generated_child_project_link_dependencies(true)) + "</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "    <Manifest>\n"
            "      <EmbedManifest>true</EmbedManifest>\n"
            "      <AdditionalManifestFiles>$(EpochRepoRoot)Engine\\resource\\epoch.manifest;%(AdditionalManifestFiles)</AdditionalManifestFiles>\n"
            "    </Manifest>\n"
            "  </ItemDefinitionGroup>\n"
            "  <ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|x64'\">\n"
            "    <ClCompile>\n"
            "      <WarningLevel>Level3</WarningLevel>\n"
            "      <FunctionLevelLinking>false</FunctionLevelLinking>\n"
            "      <IntrinsicFunctions>false</IntrinsicFunctions>\n"
            "      <SDLCheck>true</SDLCheck>\n"
            "      <PreprocessorDefinitions>ENGINE_STATICLIB;$(EpochRaylibDllDefine)NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
            "      <ConformanceMode>true</ConformanceMode>\n"
            "      <LanguageStandard>stdcpp23</LanguageStandard>\n"
            "      <LanguageStandard_C>stdc17</LanguageStandard_C>\n"
            "      <AdditionalIncludeDirectories>$(EpochRepoRoot)Engine\\include;$(EpochVcpkgInstallRoot)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n"
            "      <ScanSourceForModuleDependencies>false</ScanSourceForModuleDependencies>\n"
            "      <AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>\n"
            "      <CallingConvention>Cdecl</CallingConvention>\n"
            "    </ClCompile>\n"
            "    <Link>\n"
            "      <SubSystem>Console</SubSystem>\n"
            "      <GenerateDebugInformation>true</GenerateDebugInformation>\n"
            "      <AdditionalLibraryDirectories>$(EpochRepoRoot)" + engineLibraryRelative + ";$(EpochVcpkgInstallRoot)lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>\n"
            "      <AdditionalDependencies>" + std::string(generated_child_project_link_dependencies(false)) + "</AdditionalDependencies>\n"
            "      <EntryPointSymbol>mainCRTStartup</EntryPointSymbol>\n"
            "    </Link>\n"
            "    <Manifest>\n"
            "      <EmbedManifest>true</EmbedManifest>\n"
            "      <AdditionalManifestFiles>$(EpochRepoRoot)Engine\\resource\\epoch.manifest;%(AdditionalManifestFiles)</AdditionalManifestFiles>\n"
            "    </Manifest>\n"
            "  </ItemDefinitionGroup>\n"
            "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\n"
            "  <Target Name=\"EpochVerifyGeneratedSource\">\n"
            "    <Exec Command=\"python &quot;$(EpochRepoRoot)Tools/build/verify_epoch_project_source.py&quot; --root &quot;$(EpochRepoRoot).&quot; --edition " + xml_escape(sourceEdition) + "\" />\n"
            "  </Target>\n"
            "  <Target Name=\"EpochCopyDebugVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Debug|x64' And '$(VcpkgUseStatic)'!='true'\">\n"
            "    <ItemGroup>\n"
            "      <EpochDebugVcpkgRuntimeDlls Include=\"$(EpochVcpkgInstallRoot)debug\\bin\\*.dll\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochDebugVcpkgRuntimeDlls)\" DestinationFolder=\"$(OutDir)\" SkipUnchangedFiles=\"true\" Condition=\"'@(EpochDebugVcpkgRuntimeDlls)'!=''\" />\n"
            "  </Target>\n"
            "  <Target Name=\"EpochCopyReleaseVcpkgRuntimeDlls\" AfterTargets=\"Build\" Condition=\"'$(Configuration)|$(Platform)'=='Release|x64' And '$(VcpkgUseStatic)'!='true'\">\n"
            "    <ItemGroup>\n"
            "      <EpochReleaseVcpkgRuntimeDlls Include=\"$(EpochVcpkgInstallRoot)bin\\*.dll\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochReleaseVcpkgRuntimeDlls)\" DestinationFolder=\"$(OutDir)\" SkipUnchangedFiles=\"true\" Condition=\"'@(EpochReleaseVcpkgRuntimeDlls)'!=''\" />\n"
            "  </Target>\n"
            "  <Target Name=\"EpochCopyProjectAssets\" AfterTargets=\"Build\" Condition=\"Exists('$(MSBuildThisFileDirectory)Assets')\">\n"
            "    <ItemGroup>\n"
            "      <EpochProjectAssetFiles Include=\"$(MSBuildThisFileDirectory)Assets\\**\\*\" />\n"
            "    </ItemGroup>\n"
            "    <Copy SourceFiles=\"@(EpochProjectAssetFiles)\" DestinationFiles=\"@(EpochProjectAssetFiles->'$(OutDir)Assets\\%(RecursiveDir)%(Filename)%(Extension)')\" SkipUnchangedFiles=\"true\" />\n"
            "  </Target>\n"
            "  <ImportGroup Label=\"ExtensionTargets\" />\n"
            "</Project>\n";

        const std::string windowsBuildScriptText =
            "# EPOCH_MANAGED_GENERATED_FILE: windows_build_v1\n"
            "param(\n"
            "    [string]$Configuration = 'Debug',\n"
            "    [string]$Platform = 'x64'\n"
            ")\n"
            "$ErrorActionPreference = 'Stop'\n"
            "$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path\n"
            "$projectFile = Join-Path $projectRoot '" + powershell_escape_single_quoted(windowsProject.filename().string()) + "'\n"
            "$repoRoot = '" + repoRootPowerShell + "'\n"
            "& python (Join-Path $repoRoot 'Tools/build/verify_epoch_project_source.py') --root $repoRoot --edition '" + powershell_escape_single_quoted(sourceEdition) + "'\n"
            "if ($LASTEXITCODE -ne 0) { throw 'Generated project source verification failed.' }\n"
            "$solutionDir = $repoRoot.TrimEnd('\\\\') + '\\\\'\n"
            "$vcpkgManifestRoot = '" + vcpkgManifestRootPowerShell + "'\n"
            "$logDir = Join-Path $projectRoot 'build\\logs'\n"
            "$binDir = Join-Path $projectRoot ('build\\bin\\windows\\' + $Configuration + '\\' + $Platform)\n"
            "New-Item -ItemType Directory -Force -Path $logDir | Out-Null\n"
            "New-Item -ItemType Directory -Force -Path $binDir | Out-Null\n"
            "$logPath = Join-Path $logDir ('build-' + $Configuration.ToLowerInvariant() + '-' + $Platform.ToLowerInvariant() + '.log')\n"
            "$repoBuildLockPath = Join-Path $repoRoot 'build\\generated-project-build.lock'\n"
            "function Resolve-MSBuild {\n"
            "    if (-not [string]::IsNullOrWhiteSpace($env:MSBUILD_EXE_PATH) -and (Test-Path -LiteralPath $env:MSBUILD_EXE_PATH)) {\n"
            "        return $env:MSBUILD_EXE_PATH\n"
            "    }\n"
            "    $candidates = @(\n"
            "        'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe',\n"
            "        'C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe'\n"
            "    )\n"
            "    foreach ($candidate in $candidates) {\n"
            "        if (Test-Path -LiteralPath $candidate) { return $candidate }\n"
            "    }\n"
            "    throw 'Could not locate MSBuild. Set MSBUILD_EXE_PATH or install Visual Studio Build Tools.'\n"
            "}\n"
            "function Enter-EpochGeneratedBuildLock {\n"
            "    $lockDir = Split-Path -Parent $repoBuildLockPath\n"
            "    New-Item -ItemType Directory -Force -Path $lockDir | Out-Null\n"
            "    $deadline = [DateTime]::UtcNow.AddMinutes(10)\n"
            "    while ($true) {\n"
            "        try {\n"
            "            return [System.IO.File]::Open($repoBuildLockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)\n"
            "        }\n"
            "        catch [System.IO.IOException] {\n"
            "            if ([DateTime]::UtcNow -gt $deadline) { throw 'Timed out waiting for another Epoch generated project build to finish.' }\n"
            "            Start-Sleep -Milliseconds 250\n"
            "        }\n"
            "    }\n"
            "}\n"
            "if (-not (Test-Path -LiteralPath $projectFile)) {\n"
            "    throw ('Missing generated project file: ' + $projectFile)\n"
            "}\n"
            "$msbuild = Resolve-MSBuild\n"
            "$buildExit = 0\n"
            "$buildLock = $null\n"
            "Push-Location -LiteralPath $projectRoot\n"
            "try {\n"
            "    $buildLock = Enter-EpochGeneratedBuildLock\n"
            "    '[INFO] Project root: ' + $projectRoot | Tee-Object -FilePath $logPath\n"
            "    '[INFO] Repo root: ' + $repoRoot | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] SolutionDir: ' + $solutionDir | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] Build lock: ' + $repoBuildLockPath | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] Epoch child build project: ' + $projectFile | Tee-Object -FilePath $logPath -Append\n"
            "    '[INFO] MSBuild: ' + $msbuild | Tee-Object -FilePath $logPath -Append\n"
            "    $msbuildArgs = @(\n"
            "        $projectFile,\n"
            "        '/t:Build',\n"
            "        ('/p:Configuration=' + $Configuration),\n"
            "        ('/p:Platform=' + $Platform),\n"
            "        '/p:PlatformToolset=v143',\n"
            "        ('/p:SolutionDir=' + $solutionDir),\n"
            "        ('/p:VcpkgManifestRoot=' + $vcpkgManifestRoot),\n"
            "        '/p:EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1',\n"
            "        '/m:1',\n"
            "        '/nr:false',\n"
            "        '/clp:ErrorsOnly'\n"
            "    )\n"
            "    if (-not [string]::IsNullOrWhiteSpace($env:EPOCH_CANDIDATE_DEPENDENCY_ROOT)) {\n"
            "        $dependencyRoot = $env:EPOCH_CANDIDATE_DEPENDENCY_ROOT.TrimEnd('\\', '/') + '\\'\n"
            "        $tripletRoot = Join-Path $dependencyRoot 'x64-windows'\n"
            "        if (-not (Test-Path -LiteralPath (Join-Path $tripletRoot 'include'))) { $tripletRoot = Join-Path $tripletRoot 'x64-windows' }\n"
            "        if (-not (Test-Path -LiteralPath (Join-Path $tripletRoot 'include'))) { throw 'The admitted host dependency triplet is unavailable.' }\n"
            "        $msbuildArgs += @('/p:VcpkgManifestInstall=false', '/p:VcpkgAutoBootstrap=false', ('/p:VcpkgInstalledDir=' + $dependencyRoot), ('/p:EpochVcpkgInstallRoot=' + $tripletRoot + '\\'))\n"
            "    }\n"
            "    $nativeErrorPreference = $ErrorActionPreference\n"
            "    try {\n"
            "        # Windows PowerShell treats native stderr as ErrorRecord. Preserve it and the actual exit code.\n"
            "        $ErrorActionPreference = 'Continue'\n"
            "        & $msbuild @msbuildArgs 2>&1 | Tee-Object -FilePath $logPath -Append\n"
            "        $buildExit = $LASTEXITCODE\n"
            "    } finally { $ErrorActionPreference = $nativeErrorPreference }\n"
            "    ('[INFO] MSBuild exit code: ' + $buildExit) | Tee-Object -FilePath $logPath -Append\n"
            "}\n"
            "catch {\n"
            "    ('[ERROR] Project build host: ' + $_.Exception.Message) | Tee-Object -FilePath $logPath -Append\n"
            "    $buildExit = 1\n"
            "}\n"
            "finally {\n"
            "    if ($null -ne $buildLock) { $buildLock.Dispose() }\n"
            "    Pop-Location\n"
            "}\n"
            "if ($buildExit -ne 0) {\n"
            "    exit $buildExit\n"
            "}\n"
            "$exePath = Join-Path $projectRoot ('build\\bin\\windows\\' + $Configuration + '\\' + $Platform + '\\' + '" + powershell_escape_single_quoted(artifactStem) + ".exe')\n"
            "'[INFO] Output: ' + $exePath | Tee-Object -FilePath $logPath -Append\n";

        const std::string linuxBuildScriptText =
            "#!/usr/bin/env bash\n"
            "# EPOCH_MANAGED_GENERATED_FILE: linux_build_v2\n"
            "set -euo pipefail\n"
            "project_dir=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
            "repo_root='" + bash_escape_single_quoted(repoRoot.generic_string()) + "'\n"
            "engine_dir=\"$repo_root/Engine\"\n"
            "python3 \"$repo_root/Tools/build/verify_epoch_project_source.py\" --root \"$repo_root\" --edition '" + bash_escape_single_quoted(sourceEdition) + "'\n"
            "artifact_stem='" + bash_escape_single_quoted(artifactStem) + "'\n"
            "configuration=\"${1:-Debug}\"\n"
            "case \"$configuration\" in Debug|Release) ;; *) echo \"Configuration must be Debug or Release.\" >&2; exit 2 ;; esac\n"
            "log_dir=\"$project_dir/build/logs\"\n"
            "build_dir=\"$project_dir/build/cmake/linux/Clang-$configuration\"\n"
            "output_dir=\"$project_dir/build/bin/linux/$configuration/x64\"\n"
            "log_path=\"$log_dir/build-linux.log\"\n"
            "tool_cache_root=\"${EPOCH_TOOL_CACHE_ROOT:-${XDG_CACHE_HOME:-${HOME}/.cache}/epoch/tools}\"\n"
            "mkdir -p \"$log_dir\" \"$output_dir\"\n"
            "exec > >(tee \"$log_path\") 2>&1\n\n"
            "echo \"[INFO] Epoch generated project build\"\n"
            "echo \"[INFO] Project root: $project_dir\"\n"
            "echo \"[INFO] Repo root: $repo_root\"\n"
            "echo \"[INFO] Configuration: $configuration\"\n"
            "if [[ ! -x \"$engine_dir/build.sh\" || ! -f \"$engine_dir/unix/current_toolchain.env\" ]]; then\n"
            "  echo \"[ERROR] Epoch build policy files are missing under $engine_dir.\" >&2\n"
            "  exit 3\n"
            "fi\n\n"
            "\"$engine_dir/build.sh\" --bootstrap-current-toolchain --tool-cache-root \"$tool_cache_root\" --check-toolchain clang \"$configuration\"\n"
            "source \"$engine_dir/unix/current_toolchain.env\"\n\n"
            "resolve_tool() {\n"
            "  local candidate\n"
            "  for candidate in \"$@\"; do\n"
            "    if [[ -n \"$candidate\" && -x \"$candidate\" ]]; then printf '%s\\n' \"$candidate\"; return 0; fi\n"
            "  done\n"
            "  return 1\n"
            "}\n\n"
            "cmake_bin=\"$(resolve_tool \"${EPOCH_CMAKE:-}\" \"$HOME/.local/bin/cmake\" \"$tool_cache_root/cmake-$EPOCH_CMAKE_VERSION/bin/cmake\" \"$(command -v cmake 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified CMake $EPOCH_CMAKE_VERSION was not found.\" >&2; exit 4; }\n"
            "ninja_bin=\"$(resolve_tool \"${EPOCH_NINJA:-}\" \"$HOME/.local/bin/ninja\" \"$tool_cache_root/ninja-$EPOCH_NINJA_VERSION/ninja\" \"$(command -v ninja 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Ninja $EPOCH_NINJA_VERSION was not found.\" >&2; exit 4; }\n"
            "clang_c=\"$(resolve_tool \"${CC:-}\" \"$HOME/.local/bin/clang-$EPOCH_LLVM_MAJOR\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang\" \"$(command -v clang-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Clang $EPOCH_LLVM_VERSION C compiler was not found.\" >&2; exit 4; }\n"
            "clang_cxx=\"$(resolve_tool \"${CXX:-}\" \"$HOME/.local/bin/clang++-$EPOCH_LLVM_MAJOR\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang++\" \"$(command -v clang++-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang++ 2>/dev/null || true)\")\" || { echo \"[ERROR] Verified Clang $EPOCH_LLVM_VERSION C++ compiler was not found.\" >&2; exit 4; }\n"
            "clang_scan_deps=\"$(resolve_tool \"${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS:-}\" \"$HOME/.local/bin/clang-scan-deps-$EPOCH_LLVM_MAJOR\" \"$HOME/.local/bin/clang-scan-deps\" \"$tool_cache_root/llvm-$EPOCH_LLVM_VERSION/bin/clang-scan-deps\" \"$(command -v clang-scan-deps-$EPOCH_LLVM_MAJOR 2>/dev/null || true)\" \"$(command -v clang-scan-deps 2>/dev/null || true)\")\" || { echo \"[ERROR] Matching clang-scan-deps was not found.\" >&2; exit 4; }\n\n"
            "resolve_vcpkg_root() {\n"
            "  local candidate cache line toolchain\n"
            "  local -a candidates=(\"${EPOCH_VCPKG_ROOT:-}\" \"${VCPKG_ROOT:-}\" \"$repo_root/vcpkg\" \"$(dirname \"$repo_root\")/vcpkg\" \"$HOME/vcpkg\" \"$HOME/Documents/repos/vcpkg\" \"$HOME/source/repos/vcpkg\" /opt/vcpkg)\n"
            "  for cache in \"$repo_root\"/build/cmake/linux/*/Clang-*/CMakeCache.txt \"$engine_dir\"/Bin/Clang-*/CMakeCache.txt; do\n"
            "    [[ -f \"$cache\" ]] || continue\n"
            "    toolchain=\"\"\n"
            "    while IFS= read -r line; do\n"
            "      case \"$line\" in CMAKE_TOOLCHAIN_FILE:FILEPATH=*) toolchain=\"${line#*=}\"; break ;; esac\n"
            "    done < \"$cache\"\n"
            "    if [[ -n \"$toolchain\" ]]; then candidates+=(\"$(cd \"$(dirname \"$toolchain\")/../..\" 2>/dev/null && pwd || true)\"); fi\n"
            "  done\n"
            "  if command -v vcpkg >/dev/null 2>&1; then candidates+=(\"$(cd \"$(dirname \"$(command -v vcpkg)\")\" && pwd)\"); fi\n"
            "  for candidate in \"${candidates[@]}\"; do\n"
            "    if [[ -n \"$candidate\" && -f \"$candidate/scripts/buildsystems/vcpkg.cmake\" ]]; then printf '%s\\n' \"$candidate\"; return 0; fi\n"
            "  done\n"
            "  return 1\n"
            "}\n\n"
            "vcpkg_root=\"$(resolve_vcpkg_root)\" || { echo \"[ERROR] vcpkg was not found. Set EPOCH_VCPKG_ROOT or VCPKG_ROOT.\" >&2; exit 5; }\n"
            "if ! git -C \"$vcpkg_root\" cat-file -e \"${EPOCH_VCPKG_BASELINE}^{commit}\" 2>/dev/null; then\n"
            "  echo \"[ERROR] Selected vcpkg registry does not contain locked baseline $EPOCH_VCPKG_BASELINE: $vcpkg_root\" >&2\n"
            "  exit 5\n"
            "fi\n"
            "echo \"[INFO] CMake: $cmake_bin\"\n"
            "echo \"[INFO] Ninja: $ninja_bin\"\n"
            "echo \"[INFO] Clang: $clang_cxx\"\n"
            "echo \"[INFO] vcpkg: $vcpkg_root\"\n\n"
            "export CC=\"$clang_c\" CXX=\"$clang_cxx\" VCPKG_ROOT=\"$vcpkg_root\" VCPKG_FORCE_SYSTEM_BINARIES=1 VCPKG_FEATURE_FLAGS=manifests\n"
            "\"$cmake_bin\" --fresh -S \"$project_dir\" -B \"$build_dir\" -G Ninja \\\n"
            "  -DCMAKE_BUILD_TYPE=\"$configuration\" \\\n"
            "  -DCMAKE_C_COMPILER=\"$clang_c\" \\\n"
            "  -DCMAKE_CXX_COMPILER=\"$clang_cxx\" \\\n"
            "  -DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS=\"$clang_scan_deps\" \\\n"
            "  -DCMAKE_MAKE_PROGRAM=\"$ninja_bin\" \\\n"
            "  -DCMAKE_TOOLCHAIN_FILE=\"$vcpkg_root/scripts/buildsystems/vcpkg.cmake\" \\\n"
            "  -DVCPKG_MANIFEST_DIR=\"$engine_dir\" \\\n"
            "  -DVCPKG_INSTALLED_DIR=\"$build_dir/vcpkg_installed\" \\\n"
            "  -DVCPKG_TARGET_TRIPLET=x64-linux-epoch \\\n"
            "  -DVCPKG_OVERLAY_TRIPLETS=\"$engine_dir/cmake/triplets\" \\\n"
            "  -DEPOCH_BUILD_STATIC_RUNTIME=ON \\\n"
            "  -DBUILD_TESTING=OFF\n"
            "\"$cmake_bin\" --build \"$build_dir\" --target epoch_project_runtime --verbose\n"
            "runtime_path=\"$output_dir/$artifact_stem\"\n"
            "if [[ ! -x \"$runtime_path\" ]]; then echo \"[ERROR] Build completed without executable: $runtime_path\" >&2; exit 6; fi\n"
            "if [[ -d \"$build_dir/epoch-engine/assets\" ]]; then \"$cmake_bin\" -E copy_directory \"$build_dir/epoch-engine/assets\" \"$output_dir/assets\"; fi\n"
            "echo \"[INFO] Output: $runtime_path\"\n";
        bool ok = defaultAiProfile
            && write_text_file_if_allowed(manifest, manifestText, spec.overwrite_existing)
            && write_text_file_if_allowed(readme, readmeText, spec.overwrite_existing)
            && write_text_file_if_allowed(pathsFile, pathsText, spec.overwrite_existing)
            && write_text_file_if_allowed(worldFile, worldText, spec.overwrite_existing)
            && write_text_file_if_allowed(aiProfile, defaultAiProfileText, false)
            && (spec.input_profile_path.empty()
                || (!defaultInputProfileBytes.empty()
                    && write_text_file_if_allowed(
                        inputProfileFile, defaultInputProfileBytes, false)))
            && (spec.audio_profile_path.empty()
                || (!defaultAudioProfileBytes.empty()
                    && write_text_file_if_allowed(
                        audioProfileFile, defaultAudioProfileBytes, false)
                    && write_text_file_if_allowed(
                        defaultJumpAudio, defaultJumpAudioBytes, false)
                    && write_text_file_if_allowed(
                        defaultLandAudio, defaultLandAudioBytes, false)
                    && write_text_file_if_allowed(
                        defaultAmbientAudio, defaultAmbientAudioBytes, false)))
            && write_text_file_if_allowed(scriptFile, scriptText, spec.overwrite_existing)
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadePackageFile, engineArcadePackageText, spec.overwrite_existing))
            && (!includeEngineArcadePackage || write_text_file_if_allowed(engineArcadeScriptFile, engineArcadeScriptText, spec.overwrite_existing))
            && write_managed_generated_text_file(
                entrySource,
                entrySourceText,
                spec.overwrite_existing,
                {"extern \"C\" void core_log_write",
                 "void boot_project_shell()",
                 "epochengine::core::RunEngine();"})
            && write_managed_generated_text_file(
                cmakeFragment,
                cmakeText,
                spec.overwrite_existing,
                {"function(epoch_configure_embedded_project target)",
                 "EPOCH_REPO_ROOT"})
            && write_managed_generated_text_file(
                cmakeLists,
                cmakeListsText,
                spec.overwrite_existing,
                {"epoch_configure_embedded_project(epoch_project_runtime)",
                 "source/epoch.main.cpp",
                 "copy_directory"})
            && write_managed_generated_text_file(
                windowsProject,
                windowsProjectText,
                spec.overwrite_existing,
                {"<EpochRepoRoot>",
                 "source\\epoch.main.cpp",
                 "<VcpkgTriplet Condition=",
                 "EpochCopyProjectAssets"})
            && write_managed_generated_text_file(
                windowsBuildScript,
                windowsBuildScriptText,
                spec.overwrite_existing,
                {"Enter-EpochGeneratedBuildLock",
                 "EpochExtraDefines=EPOCH_MAIN_IN_MAIN_CPP=1",
                 "Resolve-MSBuild"})
            && write_managed_generated_text_file(
                linuxBuildScript,
                linuxBuildScriptText,
                spec.overwrite_existing,
                {"EPOCH_MANAGED_GENERATED_FILE: linux_build_v2",
                 "--target epoch_project_runtime",
                 "build-linux.log"});

        std::string materializationDiagnostic{};
        if (ok && !spec.input_profile_path.empty())
        {
            ok = ensure_project_input_source(spec.project_id, root);
            if (!ok)
            {
                materializationDiagnostic =
                    "Canonical input-profile materialization failed.";
            }
        }
        if (ok && !spec.audio_profile_path.empty())
        {
            ok = ensure_project_audio_source(spec.project_id, root);
            if (!ok)
            {
                materializationDiagnostic =
                    "Canonical audio-profile materialization failed.";
            }
        }
        if (ok)
        {
            ok = ensure_project_2d_assets(
                spec.project_id,
                root,
                spec.tilemap_path,
                spec.sprite_animation_path,
                materializationDiagnostic);
        }

        if (ok)
        {
            ok = ensure_project_gui_assets(
                spec.project_id,
                root,
                spec.gui_path,
                materializationDiagnostic);
        }
        if (ok)
            invalidate_project_profile_cache();

        const bool shellAlreadyPresent =
            fs::exists(manifest, ec) && !ec
            && fs::exists(windowsBuildScript, ec) && !ec
            && fs::exists(entrySource, ec) && !ec;

        return {
            ok,
            spec.project_id,
            root.string(),
            manifest.generic_string(),
            entrySource.generic_string(),
            windowsBuildScript.generic_string(),
            scriptFile.generic_string(),
            ok
                ? (spec.overwrite_existing
                    ? "Created project shell at " + root.string()
                    : (shellAlreadyPresent
                        ? "Verified project shell at " + root.string()
                        : "Materialized missing project shell files at " + root.string()))
                : (materializationDiagnostic.empty()
                    ? "Failed to write one or more generated project files."
                    : materializationDiagnostic),
            integrationMode,
            publicIncludeRoot
        };
    }

    [[nodiscard]] static const EditorProjectProfile* find_project_profile_by_root(std::string_view project_root) noexcept
    {
        const fs::path resolved = resolve_project_root_path(fs::path{ project_root });
        if (resolved.empty())
            return nullptr;
        for (const auto& profile : live_project_profiles())
        {
            if (resolve_project_root_path(fs::path{ profile.root_path }) == resolved)
                return &profile;
        }

        return nullptr;
    }
}

