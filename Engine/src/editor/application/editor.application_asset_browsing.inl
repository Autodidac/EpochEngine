// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::string read_text_tail(const std::filesystem::path& path, std::size_t maxChars = 2400)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return "(missing)";

            std::ifstream in(resolvedPath, std::ios::binary);
            if (!in)
                return "(unreadable)";

            in.seekg(0, std::ios::end);
            const auto end = in.tellg();
            const auto endOffset = static_cast<std::streamoff>(end);
            if (endOffset <= 0)
                return "(empty)";

            const auto size = static_cast<std::size_t>(endOffset);
            const std::size_t readSize = (std::min)(size, maxChars);
            in.seekg(static_cast<std::streamoff>(size - readSize), std::ios::beg);
            std::string text(readSize, '\0');
            in.read(text.data(), static_cast<std::streamsize>(text.size()));
            return text;
        }

        [[nodiscard]] std::string file_ready_summary(const std::filesystem::path& path)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return "missing";

            if (std::filesystem::is_regular_file(resolvedPath, ec) && !ec)
            {
                ec.clear();
                const auto size = std::filesystem::file_size(resolvedPath, ec);
                if (!ec)
                    return epochengine::format_text("ready ({} bytes)", size);
            }

            return "ready";
        }

        struct EditorBrowserEntry
        {
            std::string label{};
            std::string path{};
            std::string kind{};
            bool directory{ false };
            std::uintmax_t size{ 0 };
            std::size_t child_count{};
            bool child_count_truncated{};
        };

        [[nodiscard]] static bool should_skip_browser_directory(std::string_view name) noexcept
        {
            return name == ".git"
                || name == ".vs"
                || name == "build"
                || name == "bin"
                || name == "Debug"
                || name == "Release"
                || name == "x64"
                || name == "vcpkg_installed";
        }

        [[nodiscard]] static std::string sanitize_script_id(std::string_view value)
        {
            std::string result;
            result.reserve(value.size());
            bool previousUnderscore = false;
            for (const unsigned char raw : value)
            {
                if (std::isalnum(raw))
                {
                    result.push_back(static_cast<char>(std::tolower(raw)));
                    previousUnderscore = false;
                }
                else if ((raw == '_' || raw == '-' || raw == ' ' || raw == '.') && !previousUnderscore && !result.empty())
                {
                    result.push_back('_');
                    previousUnderscore = true;
                }
            }

            while (!result.empty() && result.back() == '_')
                result.pop_back();
            if (result.empty())
                result = "sandbox_iteration";
            return result;
        }

        [[nodiscard]] static std::string script_id_from_source_path(const std::filesystem::path& path)
        {
            std::string fileName = path.filename().string();
            constexpr std::string_view suffix = ".ascript.cpp";
            if (fileName.ends_with(suffix))
                fileName.resize(fileName.size() - suffix.size());
            return sanitize_script_id(fileName);
        }

        [[nodiscard]] static std::string relative_browser_path(
            const std::filesystem::path& root,
            const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto relative = std::filesystem::relative(path, root, ec);
            if (!ec && !relative.empty())
                return relative.generic_string();
            return path.filename().generic_string();
        }

        [[nodiscard]] static std::string browser_kind_for_path(const std::filesystem::path& path, bool directory)
        {
            if (directory)
                return "DIR";
            const std::string name = path.filename().string();
            std::string ext = path.extension().string();
            std::transform(
                ext.begin(),
                ext.end(),
                ext.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            if (name.ends_with(".ascript.cpp"))
                return "SCRIPT";
            if (ext == ".cpp" || ext == ".hpp" || ext == ".ixx" || ext == ".h")
                return "CODE";
            if (ext == ".epochmap")
                return "MAP";
            if (ext == ".epochgui")
                return "GUI";
            if (ext == ".epoch" || ext == ".json" || ext == ".txt" || ext == ".md")
                return "TEXT";
            if (ext == ".bmp" || ext == ".tga" || ext == ".ppm"
                || ext == ".epoch_texture")
            {
                return "TEXTURE";
            }
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
                return "IMAGE";
            if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx")
                return "MODEL";
            if (ext == ".wav" || ext == ".ogg" || ext == ".mp3"
                || ext == ".epochaudio")
                return "AUDIO";
            return "FILE";
        }

        [[nodiscard]] static EditorBrowserEntry make_browser_entry(
            const std::filesystem::path& root,
            const std::filesystem::path& path,
            bool directory)
        {
            std::error_code ec;
            const std::string kind = browser_kind_for_path(path, directory);
            const std::string relative = relative_browser_path(root, path);
            std::uintmax_t size = 0;
            std::size_t childCount = 0u;
            bool childCountTruncated = false;
            if (!directory)
            {
                ec.clear();
                size = std::filesystem::file_size(path, ec);
                if (ec)
                    size = 0;
            }
            else
            {
                constexpr std::size_t kMaximumCountedChildren = 256u;
                ec.clear();
                for (std::filesystem::directory_iterator iterator{
                         path,
                         std::filesystem::directory_options::skip_permission_denied,
                         ec},
                     end;
                     !ec && iterator != end;
                     iterator.increment(ec))
                {
                    if (childCount >= kMaximumCountedChildren)
                    {
                        childCountTruncated = true;
                        break;
                    }
                    ++childCount;
                }
            }

            return EditorBrowserEntry{
                .label = std::string("[") + kind + "] " + relative + (directory ? "/" : ""),
                .path = display_project_path(path),
                .kind = kind,
                .directory = directory,
                .size = size,
                .child_count = childCount,
                .child_count_truncated = childCountTruncated
            };
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_project_browser_entries(
            std::string_view projectRoot,
            std::size_t maxEntries = 48)
        {
            std::vector<EditorBrowserEntry> entries;
            const auto root = resolve_editor_path(std::filesystem::path{ projectRoot });
            std::error_code ec;
            if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                return entries;

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

                const auto path = it->path();
                const bool directory = it->is_directory(ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                if (directory)
                {
                    if (should_skip_browser_directory(path.filename().string()))
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    if (it.depth() >= 2)
                        it.disable_recursion_pending();
                }

                if (it.depth() > 2)
                    continue;

                entries.push_back(make_browser_entry(root, path, directory));
            }

            return entries;
        }

        [[nodiscard]] static bool project_texture_source_path(
            std::string_view value)
        {
            if (value.empty())
                return false;
            std::string extension =
                std::filesystem::path{value}.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return extension == ".bmp"
                || extension == ".tga"
                || extension == ".ppm"
                || extension == ".epoch_texture";
        }

        [[nodiscard]] static bool asset_category_visible(
            std::size_t category,
            std::string_view kind) noexcept
        {
            if (kind == "DIR")
                return true;
            switch (category)
            {
            case 0u: return true;
            case 1u: return kind == "TEXTURE" || kind == "IMAGE";
            case 2u: return kind == "MAP" || kind == "SCENE";
            case 3u: return kind == "AUDIO";
            case 4u: return kind == "MODEL";
            case 5u: return kind == "SCRIPT" || kind == "CODE";
            case 6u: return kind == "GUI";
            default: return false;
            }
        }

        static void draw_asset_category_tabs(
            EditorState& editor,
            bool central)
        {
            const std::array primary{
                gui::TabButtonSpec{
                    .id = central ? "central.assets.all" : "outliner.assets.all",
                    .label = "All",
                    .width = 48.0f,
                    .active = editor.assetCategory == 0u},
                gui::TabButtonSpec{
                    .id = central ? "central.assets.images" : "outliner.assets.images",
                    .label = "Images",
                    .width = 66.0f,
                    .active = editor.assetCategory == 1u},
                gui::TabButtonSpec{
                    .id = central ? "central.assets.maps" : "outliner.assets.maps",
                    .label = "Maps",
                    .width = 58.0f,
                    .active = editor.assetCategory == 2u},
                gui::TabButtonSpec{
                    .id = central ? "central.assets.gui" : "outliner.assets.gui",
                    .label = "GUI",
                    .width = 50.0f,
                    .active = editor.assetCategory == 6u}
            };
            if (const auto tabs = gui::tab_bar_buttons(
                    primary, 28.0f, 1.0f);
                tabs.selected_index)
            {
                constexpr std::array<std::size_t, 4> categories{
                    0u, 1u, 2u, 6u};
                editor.assetCategory = categories[*tabs.selected_index];
            }

            const std::array secondary{
                gui::TabButtonSpec{
                    .id = central ? "central.assets.audio" : "outliner.assets.audio",
                    .label = "Audio",
                    .width = 60.0f,
                    .active = editor.assetCategory == 3u},
                gui::TabButtonSpec{
                    .id = central ? "central.assets.models" : "outliner.assets.models",
                    .label = "Models",
                    .width = 66.0f,
                    .active = editor.assetCategory == 4u},
                gui::TabButtonSpec{
                    .id = central ? "central.assets.code" : "outliner.assets.code",
                    .label = "Code",
                    .width = 54.0f,
                    .active = editor.assetCategory == 5u}
            };
            if (const auto tabs = gui::tab_bar_buttons(
                    secondary, 28.0f, 1.0f);
                tabs.selected_index)
            {
                editor.assetCategory = 3u + *tabs.selected_index;
            }
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry>
            collect_asset_manager_entries(
                std::string_view projectRoot,
                std::string_view relativeFolder,
                std::size_t maximumEntries = 512u)
        {
            std::vector<EditorBrowserEntry> entries{};
            const std::filesystem::path root = resolve_editor_path(
                std::filesystem::path{projectRoot});
            if (root.empty())
                return entries;

            std::filesystem::path folder = root;
            if (!relativeFolder.empty())
            {
                const std::filesystem::path relative{relativeFolder};
                for (const auto& component : relative)
                {
                    if (component == "." || component == "..")
                        return entries;
                }
                folder /= relative;
            }

            std::error_code error{};
            const std::filesystem::path canonicalRoot =
                std::filesystem::weakly_canonical(root, error);
            if (error)
                return entries;
            const std::filesystem::path canonicalFolder =
                std::filesystem::weakly_canonical(folder, error);
            if (error || !std::filesystem::is_directory(
                    canonicalFolder, error) || error)
            {
                return entries;
            }
            const std::filesystem::path relativeCheck =
                std::filesystem::relative(
                    canonicalFolder,
                    canonicalRoot,
                    error);
            if (error)
                return entries;
            for (const auto& component : relativeCheck)
            {
                if (component == "..")
                    return entries;
            }

            for (std::filesystem::directory_iterator iterator{
                     canonicalFolder,
                     std::filesystem::directory_options::skip_permission_denied,
                     error},
                 end;
                 !error
                    && iterator != end
                    && entries.size() < maximumEntries;
                 iterator.increment(error))
            {
                std::error_code entryError{};
                const bool directory =
                    iterator->is_directory(entryError);
                if (entryError)
                    continue;
                if (directory
                    && should_skip_browser_directory(
                        iterator->path().filename().string()))
                {
                    continue;
                }
                entries.push_back(make_browser_entry(
                    canonicalRoot,
                    iterator->path(),
                    directory));
            }
            std::sort(
                entries.begin(),
                entries.end(),
                [](const EditorBrowserEntry& left,
                   const EditorBrowserEntry& right)
                {
                    if (left.directory != right.directory)
                        return left.directory;
                    return left.label < right.label;
                });
            return entries;
        }

        static void enter_asset_browser_folder(
            EditorState& editor,
            const EditorBrowserEntry& entry)
        {
            if (!entry.directory)
                return;
            const auto root = resolve_editor_path(
                std::filesystem::path{editor.projectRoot});
            const auto selected = resolve_editor_path(
                std::filesystem::path{entry.path});
            editor.assetBrowserFolder = relative_browser_path(
                root,
                selected);
            editor.assetGridScroll = 0.0f;
            editor.assetGridVisibleIds.clear();
            editor.selectedProjectFile.clear();
            editor.selectedAssetPath.clear();
        }

        static void leave_asset_browser_folder(EditorState& editor)
        {
            if (editor.assetBrowserFolder.empty())
                return;
            editor.assetBrowserFolder =
                std::filesystem::path{editor.assetBrowserFolder}
                    .parent_path()
                    .generic_string();
            editor.assetGridScroll = 0.0f;
            editor.assetGridVisibleIds.clear();
            editor.selectedProjectFile.clear();
            editor.selectedAssetPath.clear();
        }
        [[nodiscard]]
        [[nodiscard]]
        editor_project_textures::ProjectTextureController*
            project_texture_controller(EditorState& editor)
        {
            if (editor.projectTextures)
                return editor.projectTextures.get();
            try
            {
                editor.projectTextures = std::make_unique<
                    editor_project_textures::ProjectTextureController>(
                        editor.projectId,
                        resolve_editor_path(
                            std::filesystem::path{editor.projectRoot}));
            }
            catch (...)
            {
                editor.projectTextureStatus =
                    "Texture controller allocation failed.";
                return nullptr;
            }
            if (!editor.projectTextures->valid())
            {
                editor.projectTextureStatus =
                    "Texture controller could not resolve the active project.";
                editor.projectTextures.reset();
                return nullptr;
            }
            return editor.projectTextures.get();
        }

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        [[nodiscard]]
        editor_tilemaps::TileMapWorkspaceController*
            tilemap_workspace_controller(EditorState& editor)
        {
            if (editor.tileMapWorkspace)
                return editor.tileMapWorkspace.get();
            try
            {
                editor.tileMapWorkspace = std::make_unique<
                    editor_tilemaps::TileMapWorkspaceController>(
                        editor.projectId,
                        resolve_editor_path(
                            std::filesystem::path{editor.projectRoot}),
                        "Assets/Maps/main.epochmap");
            }
            catch (...)
            {
                editor.tileMapStatus = "Tile-map workspace allocation failed.";
                return nullptr;
            }
            if (!editor.tileMapWorkspace->valid())
            {
                editor.tileMapStatus =
                    "Tile-map workspace could not resolve the active project.";
                editor.tileMapWorkspace.reset();
                return nullptr;
            }
            const auto opened = editor.tileMapWorkspace->open_or_create();
            editor.tileMapStatus = std::string(editor.tileMapWorkspace->status());
            if (!opened)
            {
                editor.tileMapWorkspace.reset();
                return nullptr;
            }
            return editor.tileMapWorkspace.get();
        }
#endif

        void import_selected_project_texture(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx)
        {
            auto* controller = project_texture_controller(editor);
            if (!controller)
                return;
            if (editor.selectedProjectFile.empty())
            {
                editor.projectTextureStatus =
                    "Select an image or .epoch_texture source under Project/Assets.";
                return;
            }

            const RendererBackendKind backend = renderer_backend_kind(ctx);
            const capability::SubsystemProfile renderer =
                capability::renderer_subsystem_profile_for(backend);
            const Budgets budgets =
                renderer.capability.recommended_budgets;
            const std::filesystem::path selectedPath =
                resolve_editor_path(
                    std::filesystem::path{editor.selectedProjectFile});
            if (selectedPath.extension() == ".epoch_texture")
            {
                std::error_code relativeError{};
                const std::filesystem::path projectRoot =
                    resolve_editor_path(
                        std::filesystem::path{editor.projectRoot});
                const std::filesystem::path relativePath =
                    std::filesystem::relative(
                        selectedPath,
                        projectRoot,
                        relativeError);
                if (relativeError)
                {
                    editor.projectTextureStatus =
                        "Texture document is outside the active project.";
                    return;
                }

                const auto opened = controller->open_editable(
                    relativePath.generic_string(),
                    renderer,
                    budgets);
                if (!opened)
                {
                    editor.projectTextureStatus = epochengine::format_text(
                        "Texture document open refused: {} | source {} | compile {}.",
                        editor_project_textures::controller_code_name(
                            opened.code),
                        project_texture_sources::source_code_name(
                            opened.source_code),
                        asset::texture::artifact_compilation_status_name(
                            opened.compilation_status));
                    push_editor_log(
                        editor,
                        "[assets] " + editor.projectTextureStatus);
                    return;
                }

                editor.selectedAssetPath = opened.entry->logical_path;
                editor.projectTextureStatus = epochengine::format_text(
                    "{} opened at revision {} with {} compiled bytes.",
                    opened.entry->logical_path,
                    opened.entry->source_sequence,
                    opened.entry->decoded_bytes);
                push_editor_log(
                    editor,
                    "[assets] " + editor.projectTextureStatus);
                return;
            }

            const auto imported = controller->import_source(
                selectedPath,
                renderer,
                budgets);
            if (!imported)
            {
                editor.projectTextureStatus = epochengine::format_text(
                    "Texture import refused: {} | import {} | admission {} | pipeline {}.",
                    editor_project_textures::controller_code_name(
                        imported.code),
                    asset::texture::texture_import_status_name(
                        imported.import_status),
                    project_textures::texture_admission_reason_name(
                        imported.admission_reason),
                    project_textures::pipeline_code_name(
                        imported.pipeline_code));
                push_editor_log(
                    editor,
                    "[assets] " + editor.projectTextureStatus);
                return;
            }

            editor.selectedAssetPath = imported.entry->logical_path;
            editor.projectTextureStatus = epochengine::format_text(
                "{} imported as {} ({}x{}, {} KiB decoded).",
                std::filesystem::path{
                    imported.entry->source_path}.filename().string(),
                imported.entry->logical_path,
                imported.entry->width,
                imported.entry->height,
                imported.entry->decoded_bytes / 1024u);
            push_editor_log(
                editor,
                "[assets] " + editor.projectTextureStatus);
        }

        void refresh_project_texture_preview(EditorState& editor)
        {
            auto* controller = project_texture_controller(editor);
            if (!controller)
            {
                editor.projectTexturePreviewSprite = {};
                editor.projectTexturePreviewPath.clear();
                editor.projectTexturePreviewKey = {};
                editor.projectTexturePreviewWidth = 0u;
                editor.projectTexturePreviewHeight = 0u;
                return;
            }

            std::optional<editor_project_textures::TexturePreview> preview{};
            const auto editable = controller->editable_state();
            if (editable)
            {
                preview = controller->editable_preview();
            }
            else
            {
                if (!controller->selected())
                {
                    editor.projectTexturePreviewSprite = {};
                    editor.projectTexturePreviewPath.clear();
                    editor.projectTexturePreviewKey = {};
                    editor.projectTexturePreviewWidth = 0u;
                    editor.projectTexturePreviewHeight = 0u;
                    return;
                }
                preview = controller->selected_preview();
            }
            if (!preview)
            {
                editor.projectTextureStatus =
                    "The selected texture could not produce a compiled preview.";
                editor.projectTexturePreviewSprite = {};
                return;
            }
            if (editor.projectTexturePreviewSprite.is_valid()
                && editor.projectTexturePreviewPath
                    == preview->logical_path
                && editor.projectTexturePreviewKey
                    == preview->artifact_key.words)
            {
                return;
            }

            const std::string surfaceId = epochengine::format_text(
                "project.texture.preview.{}.{}",
                controller->project_id(),
                preview->logical_path);
            const SpriteHandle sprite = gui::register_runtime_surface(
                surfaceId,
                preview->rgba8,
                preview->width,
                preview->height);
            if (!sprite.is_valid())
            {
                editor.projectTextureStatus =
                    "The selected texture preview could not enter GUI residency.";
                editor.projectTexturePreviewSprite = {};
                return;
            }

            editor.projectTexturePreviewSprite = sprite;
            editor.projectTexturePreviewPath = preview->logical_path;
            editor.projectTexturePreviewKey = preview->artifact_key.words;
            editor.projectTexturePreviewWidth = preview->width;
            editor.projectTexturePreviewHeight = preview->height;
        }
        [[nodiscard]] std::uint64_t project_texture_catalog_signature(
            std::span<const editor_project_textures::TextureCatalogEntry>
                catalog) noexcept
        {
            constexpr std::uint64_t offset = 1469598103934665603ull;
            constexpr std::uint64_t prime = 1099511628211ull;
            std::uint64_t hash = offset;
            const auto mix = [&](std::uint64_t value) noexcept
            {
                hash ^= value;
                hash *= prime;
            };
            for (const auto& entry : catalog)
            {
                for (const unsigned char character : entry.logical_path)
                    mix(character);
                mix(0xffu);
                for (const std::uint64_t word : entry.artifact_key.words)
                    mix(word);
                mix(entry.width);
                mix(entry.height);
            }
            return hash == 0u ? 1u : hash;
        }

        [[nodiscard]] bool refresh_project_texture_thumbnail_atlas(
            EditorState& editor,
            editor_project_textures::ProjectTextureController& controller)
        {
            const auto catalog = controller.catalog();
            const std::uint64_t signature =
                project_texture_catalog_signature(catalog);
            if (editor.projectTextureThumbnailAtlasSignature == signature)
                return !editor.projectTextureThumbnails.empty();

            editor.projectTextureThumbnails.clear();
            editor.projectTextureThumbnailAtlasSignature = 0u;
            if (catalog.empty())
            {
                editor.projectTextureThumbnailAtlasSignature = signature;
                return true;
            }

            std::vector<editor_project_textures::TextureThumbnail>
                thumbnails{};
            thumbnails.reserve(catalog.size());
            for (const auto& entry : catalog)
            {
                const auto preview = controller.preview(entry.logical_path);
                if (!preview)
                    continue;
                auto thumbnail =
                    editor_project_textures::make_texture_thumbnail(*preview);
                if (thumbnail)
                    thumbnails.push_back(std::move(*thumbnail));
            }
            if (thumbnails.empty())
                return false;

            std::vector<std::string> surfaceIds{};
            std::vector<gui::RuntimeSurfaceDescriptor> surfaces{};
            surfaceIds.reserve(thumbnails.size());
            surfaces.reserve(thumbnails.size());
            for (std::size_t index = 0u;
                 index < thumbnails.size();
                 ++index)
            {
                constexpr std::uint64_t logicalOffset =
                    1469598103934665603ull;
                constexpr std::uint64_t logicalPrime =
                    1099511628211ull;
                std::uint64_t logicalKey = logicalOffset;
                for (const unsigned char character :
                     thumbnails[index].logical_path)
                {
                    logicalKey ^= character;
                    logicalKey *= logicalPrime;
                }
                surfaceIds.push_back(epochengine::format_text(
                    "project.texture.thumbnail.{}.{}",
                    controller.project_id(),
                    logicalKey));
            }
            for (std::size_t index = 0u;
                 index < thumbnails.size();
                 ++index)
            {
                const auto& thumbnail = thumbnails[index];
                surfaces.push_back(gui::RuntimeSurfaceDescriptor{
                    .id = surfaceIds[index],
                    .rgba_pixels = thumbnail.rgba8,
                    .width = thumbnail.width,
                    .height = thumbnail.height
                });
            }

            const gui::RuntimeSurfaceAtlasResult atlas =
                gui::register_runtime_surface_atlas(surfaces);
            for (std::size_t index = 0u;
                 index < thumbnails.size()
                    && index < atlas.handles.size();
                 ++index)
            {
                if (!atlas.handles[index].is_valid())
                    continue;
                const auto& thumbnail = thumbnails[index];
                const auto source = std::find_if(
                    catalog.begin(),
                    catalog.end(),
                    [&](const auto& entry)
                    {
                        return entry.logical_path == thumbnail.logical_path;
                    });
                editor.projectTextureThumbnails.insert_or_assign(
                    thumbnail.logical_path,
                    ProjectTextureThumbnailState{
                        .sprite = atlas.handles[index],
                        .artifact_key = thumbnail.artifact_key.words,
                        .width = thumbnail.width,
                        .height = thumbnail.height,
                        .source_width = source == catalog.end()
                            ? thumbnail.width
                            : source->width,
                        .source_height = source == catalog.end()
                            ? thumbnail.height
                            : source->height,
                        .source_bytes = source == catalog.end()
                            ? 0u
                            : source->source_bytes
                    });
            }
            if (!atlas
                || editor.projectTextureThumbnails.size()
                    != thumbnails.size())
            {
                editor.projectTextureStatus =
                    "Texture thumbnail atlas could not admit every compiled preview.";
                return false;
            }

            editor.projectTextureThumbnailAtlasSignature = signature;
            return true;
        }

        [[nodiscard]] SpriteHandle project_texture_thumbnail(
            EditorState& editor,
            editor_project_textures::ProjectTextureController& controller,
            const editor_project_textures::TextureCatalogEntry& entry)
        {
            (void)refresh_project_texture_thumbnail_atlas(
                editor,
                controller);
            const auto cached =
                editor.projectTextureThumbnails.find(entry.logical_path);
            if (cached == editor.projectTextureThumbnails.end()
                || cached->second.artifact_key
                    != entry.artifact_key.words)
            {
                return {};
            }
            return cached->second.sprite;
        }

        [[nodiscard]] std::filesystem::path normalized_project_asset_path(
            std::string_view projectRoot,
            std::string_view rawPath)
        {
            if (rawPath.empty())
                return {};

            std::filesystem::path path{rawPath};
            const std::filesystem::path root = resolve_editor_path(
                std::filesystem::path{projectRoot});
            if (path.is_relative())
            {
                const std::filesystem::path projectCandidate = root / path;
                std::error_code candidateError{};
                if (!root.empty()
                    && std::filesystem::exists(
                        projectCandidate,
                        candidateError)
                    && !candidateError)
                {
                    path = projectCandidate;
                }
                else
                {
                    path = resolve_editor_path(path);
                }
            }

            std::error_code canonicalError{};
            const std::filesystem::path canonical =
                std::filesystem::weakly_canonical(path, canonicalError);
            return canonicalError ? path.lexically_normal() : canonical;
        }

        [[nodiscard]]
        const editor_project_textures::TextureCatalogEntry*
            project_texture_for_asset_path(
                editor_project_textures::ProjectTextureController& controller,
                std::string_view projectRoot,
                std::string_view assetPath)
        {
            if (assetPath.empty())
                return nullptr;

            const auto catalog = controller.catalog();
            const auto logical = std::find_if(
                catalog.begin(),
                catalog.end(),
                [assetPath](const auto& texture)
                {
                    return texture.logical_path == assetPath;
                });
            if (logical != catalog.end())
                return &*logical;

            const std::filesystem::path candidate =
                normalized_project_asset_path(projectRoot, assetPath);
            if (candidate.empty())
                return nullptr;

            const std::filesystem::path root = resolve_editor_path(
                std::filesystem::path{projectRoot});
            std::error_code relativeError{};
            const std::filesystem::path relative =
                root.empty()
                ? std::filesystem::path{}
                : std::filesystem::relative(
                    candidate,
                    root,
                    relativeError);
            const std::string candidateLogical =
                relativeError || relative.empty()
                ? std::string{}
                : relative.generic_string();

            const auto found = std::find_if(
                catalog.begin(),
                catalog.end(),
                [&](const auto& texture)
                {
                    const std::filesystem::path source =
                        normalized_project_asset_path(
                            projectRoot,
                            texture.source_path.generic_string());
                    std::error_code equivalentError{};
                    const bool equivalent =
                        !source.empty()
                        && std::filesystem::equivalent(
                            candidate,
                            source,
                            equivalentError);
                    return (!equivalentError && equivalent)
                        || (!candidateLogical.empty()
                            && texture.logical_path == candidateLogical)
                        || source.lexically_normal()
                            == candidate.lexically_normal();
                });
            return found == catalog.end() ? nullptr : &*found;
        }

        [[nodiscard]] std::uint64_t texture_source_surface_key(
            const std::filesystem::path& path) noexcept
        {
            constexpr std::uint64_t offset = 1469598103934665603ull;
            constexpr std::uint64_t prime = 1099511628211ull;
            std::uint64_t hash = offset;
            for (const unsigned char character : path.generic_string())
            {
                hash ^= character;
                hash *= prime;
            }
            return hash == 0u ? 1u : hash;
        }

        [[nodiscard]] const ProjectTextureThumbnailState*
            project_source_texture_thumbnail(
                EditorState& editor,
                std::string_view assetPath)
        {
            const std::filesystem::path source =
                normalized_project_asset_path(
                    editor.projectRoot,
                    assetPath);
            if (source.empty())
                return nullptr;

            std::string extension = source.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            if (extension != ".bmp"
                && extension != ".tga"
                && extension != ".ppm")
            {
                return nullptr;
            }

            std::error_code metadataError{};
            const std::uintmax_t sourceBytes =
                std::filesystem::file_size(source, metadataError);
            if (metadataError)
                return nullptr;
            const std::filesystem::file_time_type sourceWriteTime =
                std::filesystem::last_write_time(
                    source,
                    metadataError);
            if (metadataError)
                return nullptr;

            const std::string cacheKey = source.generic_string();
            const auto cached =
                editor.projectSourceTextureThumbnails.find(cacheKey);
            if (cached != editor.projectSourceTextureThumbnails.end()
                && cached->second.sprite.is_valid()
                && cached->second.source_bytes == sourceBytes
                && cached->second.source_write_time == sourceWriteTime)
            {
                return &cached->second;
            }
            editor.projectSourceTextureThumbnails.erase(cacheKey);

            asset::texture::TextureCompileProfile profile{};
            profile.format =
                asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = asset::texture::ColorSpace::linear;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            const asset::texture::TextureImportResult imported =
                asset::texture::import_texture_file({
                    .source_path = source,
                    .source_sequence = 1u,
                    .profile = profile
                });
            if (!imported
                || imported.artifact.mips.empty()
                || !imported.artifact.mips.front().valid())
            {
                return nullptr;
            }

            const auto& mip = imported.artifact.mips.front();
            editor_project_textures::TexturePreview preview{
                .logical_path = display_project_path(source),
                .artifact_key = imported.artifact.identity.key,
                .width = mip.width,
                .height = mip.height
            };
            try
            {
                preview.rgba8.reserve(mip.texels.size());
                for (const std::byte texel : mip.texels)
                {
                    preview.rgba8.push_back(
                        std::to_integer<std::uint8_t>(texel));
                }
            }
            catch (...)
            {
                return nullptr;
            }

            const auto thumbnail =
                editor_project_textures::make_texture_thumbnail(preview);
            if (!thumbnail)
                return nullptr;

            const std::string surfaceId = epochengine::format_text(
                "project.texture.source_thumbnail.{}.{}",
                editor.projectId,
                texture_source_surface_key(source));
            const gui::RuntimeSurfaceDescriptor descriptor{
                .id = surfaceId,
                .rgba_pixels = thumbnail->rgba8,
                .width = thumbnail->width,
                .height = thumbnail->height
            };
            const gui::RuntimeSurfaceAtlasResult atlas =
                gui::register_runtime_surface_atlas(
                    std::span<const gui::RuntimeSurfaceDescriptor>{
                        &descriptor,
                        1u});
            if (!atlas
                || atlas.handles.empty()
                || !atlas.handles.front().is_valid())
            {
                return nullptr;
            }

            const auto [inserted, _] =
                editor.projectSourceTextureThumbnails.insert_or_assign(
                    cacheKey,
                    ProjectTextureThumbnailState{
                        .sprite = atlas.handles.front(),
                        .artifact_key =
                            thumbnail->artifact_key.words,
                        .width = thumbnail->width,
                        .height = thumbnail->height,
                        .source_width = imported.width,
                        .source_height = imported.height,
                        .source_bytes = sourceBytes,
                        .source_write_time = sourceWriteTime
                    });
            return &inserted->second;
        }

        [[nodiscard]] const ProjectTextureThumbnailState*
            project_texture_thumbnail_state(
                EditorState& editor,
                editor_project_textures::ProjectTextureController& controller,
                const editor_project_textures::TextureCatalogEntry& entry)
        {
            const SpriteHandle sprite = project_texture_thumbnail(
                editor,
                controller,
                entry);
            if (!sprite.is_valid())
                return nullptr;
            const auto cached =
                editor.projectTextureThumbnails.find(entry.logical_path);
            return cached == editor.projectTextureThumbnails.end()
                ? nullptr
                : &cached->second;
        }

