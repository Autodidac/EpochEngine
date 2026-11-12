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
                || name == "Artifacts"
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
            {
                const auto utf8 = relative.generic_u8string();
                return {utf8.begin(), utf8.end()};
            }
            const auto utf8 = path.filename().generic_u8string();
            return {utf8.begin(), utf8.end()};
        }

        [[nodiscard]] static std::string browser_kind_for_path(const std::filesystem::path& path, bool directory)
        {
            if (directory)
                return "DIR";
            const auto nameUtf8 = path.filename().generic_u8string();
            const std::string name{nameUtf8.begin(), nameUtf8.end()};
            const auto extensionUtf8 = path.extension().generic_u8string();
            std::string ext{extensionUtf8.begin(), extensionUtf8.end()};
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
            if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx" || ext == ".epoch_mesh")
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
            const auto extensionUtf8 = std::filesystem::path{
                std::u8string(value.begin(), value.end())}.extension().generic_u8string();
            std::string extension{extensionUtf8.begin(), extensionUtf8.end()};
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return extension == ".png"
                || extension == ".bmp"
                || extension == ".tga"
                || extension == ".ppm"
                || extension == ".epoch_texture";
        }

        [[nodiscard]] static bool project_image_preview_path(std::string_view value)
        {
            const auto extensionUtf8 = std::filesystem::path{
                std::u8string(value.begin(), value.end())}.extension().generic_u8string();
            std::string extension{extensionUtf8.begin(), extensionUtf8.end()};
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension == ".jpg" || extension == ".jpeg"
                || extension == ".png" || extension == ".bmp"
                || extension == ".tga" || extension == ".ppm";
        }

        struct ProjectImageSourceResult
        {
            std::filesystem::path path{};
            std::string diagnostic{};
            bool texture_source{};
            asset::texture::ContentHash generated_texture_key{};
            [[nodiscard]] explicit operator bool() const noexcept { return !path.empty(); }
        };

        [[nodiscard]] ProjectImageSourceResult add_project_image_source(
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& sourcePath,
            std::optional<std::string_view> generatedSha256 = std::nullopt)
        {
            namespace fs = std::filesystem;
            const auto fail = [](std::string reason) {
                return ProjectImageSourceResult{{}, std::move(reason), false}; };
            try
            {
                std::error_code error{};
                const auto root = fs::canonical(projectRoot, error);
                if (error || !fs::is_directory(root))
                    return fail("Open a project before adding an image.");
                const auto sourceStatus = fs::symlink_status(sourcePath, error);
                if (error || !fs::is_regular_file(sourceStatus) || fs::is_symlink(sourceStatus))
                    return fail("Choose a regular image file, not a link or directory.");
                const auto source = fs::canonical(sourcePath, error);
                const auto sourceUtf8 = source.generic_u8string();
                const std::string sourceName(sourceUtf8.begin(), sourceUtf8.end());
                if (error || !project_image_preview_path(sourceName))
                    return fail("Choose a PNG, JPEG, BMP, TGA or PPM image.");
                const bool textureSource = project_texture_source_path(sourceName);
                const auto sourceBytes = fs::file_size(source, error);
                const std::uintmax_t maximumBytes = textureSource
                    ? 64u * 1024u * 1024u : 4u * 1024u * 1024u;
                if (error || sourceBytes == 0u || sourceBytes > maximumBytes)
                    return fail(textureSource ? "Image file exceeds the 64 MiB source limit."
                        : "JPEG reference exceeds the 4 MiB preview limit.");
                std::ifstream input{source, std::ios::binary};
                std::string bytes(static_cast<std::size_t>(sourceBytes), '\0');
                if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))
                    || input.peek() != std::char_traits<char>::eof())
                    return fail("The image file could not be read completely.");
                asset::texture::ContentHash generatedKey{};
                if (generatedSha256)
                {
                    std::string extension = source.extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                    if (extension != ".png" || generatedSha256->size() != 64u
                        || !std::all_of(generatedSha256->begin(), generatedSha256->end(),
                            [](char value) { return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'); })
                        || core::sha256::hex(core::sha256::hash(std::string_view{bytes})) != *generatedSha256)
                        return fail("The generated PNG no longer matches its reviewed receipt. Review it again before importing.");
                    const auto decoded = asset::texture::import_texture_memory({
                        .source = std::as_bytes(std::span{bytes}), .extension = ".png",
                        .profile = {.format = asset::texture::ArtifactFormat::rgba8_unorm,
                            .color_space = asset::texture::ColorSpace::linear,
                            .mipmaps = asset::texture::MipmapPolicy::preserve_authored}});
                    if (!decoded) return fail("The generated PNG could not be decoded. No project image was added.");
                    generatedKey = decoded.artifact.identity.key;
                }
                const auto images = root / "Assets" / "Images";
                for (const auto& directory : {root / "Assets", images})
                {
                    const auto status = fs::symlink_status(directory, error);
                    if (error && error != std::errc::no_such_file_or_directory)
                        return fail("The project image directory could not be inspected.");
                    error.clear();
                    if (fs::exists(status) && (!fs::is_directory(status) || fs::is_symlink(status)))
                        return fail("Assets/Images must be an ordinary project directory, not a link.");
                }
                fs::create_directories(images, error);
                if (error || fs::canonical(images, error) != images || error)
                    return fail("The image destination is outside the project or unavailable.");
                const auto destination = images / (generatedSha256
                    ? fs::path{"generated-" + std::string{*generatedSha256} + ".png"} : source.filename());
                const auto destinationStatus = fs::symlink_status(destination, error);
                if ((!error && fs::exists(destinationStatus))
                    || (error && error != std::errc::no_such_file_or_directory))
                {
                    if (!generatedSha256 || error || !fs::is_regular_file(destinationStatus)
                        || fs::is_symlink(destinationStatus) || fs::canonical(destination, error) != destination || error
                        || fs::file_size(destination, error) != sourceBytes || error)
                        return fail("An image with that name already exists. Existing project files were not changed.");
                    std::ifstream existing{destination, std::ios::binary};
                    std::string retained(bytes.size(), '\0');
                    if (!existing.read(retained.data(), static_cast<std::streamsize>(retained.size()))
                        || existing.peek() != std::char_traits<char>::eof() || retained != bytes)
                        return fail("The generated image destination contains different data. Existing project files were not changed.");
                    return {destination, "This exact generated image is already in Assets/Images.", true, generatedKey};
                }
                static std::atomic<std::uint64_t> nextImport{1u};
                fs::path pending;
                for (unsigned attempt = 0; attempt < 16u; ++attempt)
                {
                    pending = images / (".epoch-image-import-"
                        + std::to_string(nextImport.fetch_add(1u, std::memory_order_relaxed))
                        + source.extension().string());
                    if (platform::filesystem::exclusive_create_and_write(
                            pending, std::as_bytes(std::span{bytes}), error))
                        break;
                    pending.clear();
                }
                if (pending.empty()) return fail("Could not stage the image inside this project.");
                struct PendingFile final
                {
                    fs::path path;
                    ~PendingFile() { std::error_code ignored; fs::remove(path, ignored); }
                } cleanup{pending};
                if (textureSource && !generatedSha256)
                {
                    if (!asset::texture::import_texture_file({.source_path = pending}))
                        return fail("The image could not be decoded. No project image was added.");
                }
                else if (!textureSource && platform::filesystem::image_thumbnail(bytes).rgba.empty())
                    return fail("JPEG preview is unavailable for this file or platform. No project image was added.");
                if (fs::canonical(images, error) != images || error
                    || !platform::filesystem::atomic_publish_new_same_filesystem(pending, destination, error))
                    return fail("Image publication was refused. Existing project files were not changed.");
                return {destination,
                    textureSource ? "Image copied into Assets/Images."
                        : "JPEG copied as a project reference; full texture compilation is not available for JPEG.",
                    textureSource, generatedKey};
            }
            catch (...) { return fail("Image import failed. Existing project files were not replaced."); }
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
                std::filesystem::path{std::u8string(projectRoot.begin(), projectRoot.end())});
            if (root.empty())
                return entries;

            std::filesystem::path folder = root;
            if (!relativeFolder.empty())
            {
                const std::filesystem::path relative{
                    std::u8string(relativeFolder.begin(), relativeFolder.end())};
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
                const auto nameUtf8 = iterator->path().filename().generic_u8string();
                if (directory
                    && should_skip_browser_directory(
                        std::string{nameUtf8.begin(), nameUtf8.end()}))
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
                std::filesystem::path{std::u8string(editor.projectRoot.begin(), editor.projectRoot.end())});
            const auto selected = resolve_editor_path(
                std::filesystem::path{std::u8string(entry.path.begin(), entry.path.end())});
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
            const auto parent = std::filesystem::path{
                std::u8string(editor.assetBrowserFolder.begin(), editor.assetBrowserFolder.end())}
                .parent_path().generic_u8string();
            editor.assetBrowserFolder.assign(parent.begin(), parent.end());
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
                            std::filesystem::path{std::u8string(editor.projectRoot.begin(), editor.projectRoot.end())}));
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

        [[nodiscard]] project_planetary::SceneMeshBudget editor_planetary_mesh_budget(const EditorState& editor)
        {
            const auto& policy=editor.planetaryMeshPolicy;
            if(!policy || policy->selected_root!=editor.projectRoot
                || !editor.planetaryAttachment.source_attached()
                || policy->source.content_hash!=editor.planetaryAttachment.content_hash)
                return {};
            return project_planetary::scene_mesh_budget(policy->settings);
        }

        [[nodiscard]] bool editor_planetary_mesh_policy_current(EditorState& editor)
        {
            if(!editor.planetaryMeshPolicy)return false;
            const auto root=resolve_editor_path(std::filesystem::path{editor.projectRoot});
            const auto saved=project_planetary::load_settings(root);
            const auto& policy=*editor.planetaryMeshPolicy;
            if(policy.selected_root==editor.projectRoot &&editor.planetaryAttachment.source_attached()
                &&policy.source.content_hash==editor.planetaryAttachment.content_hash
                &&project_planetary::source_generation_is_current(root,policy.source))
                return saved&&*saved==policy.settings;
            editor.planetaryMeshPolicy.reset();
            return false;
        }

        [[nodiscard]] std::optional<project_planetary::MeshGenerationIdentity>
            editor_planetary_mesh_identity(const EditorEntity& entity)
        {
            if(entity.category!="Planetary Terrain"||!entity.meshAsset)return {};
            project_planetary::MeshGenerationIdentity identity{entity.meshAsset->artifact_key,
                entity.meshAsset->vertex_count,entity.meshAsset->index_count,{}};
            if(entity.meshAsset->coordinates)
                identity.coordinates={entity.meshAsset->coordinates->meters_per_unit,
                    entity.meshAsset->coordinates->source_origin_meters};
            return identity;
        }

        [[nodiscard]] bool editor_planetary_stream_identity_matches(
            const EditorState& editor,const EditorEntity& entity)
        {
            const auto identity=editor_planetary_mesh_identity(entity);
            return identity&&editor.planetaryMeshPolicy
                &&editor.planetaryMeshPolicy->selected_root==editor.projectRoot
                &&editor.planetaryAttachment.source_attached()
                &&editor.planetaryMeshPolicy->source.content_hash==editor.planetaryAttachment.content_hash
                &&editor.planetaryMeshPolicy->generated_mesh==identity;
        }

        [[nodiscard]] bool editor_planetary_outliner_only(
            const EditorState& editor, const EditorEntity& entity)
        {
            // A generated system is an Outliner-selected container, not a solid
            // pick volume enclosing every independently placed scene object.
            return editor_planetary_stream_identity_matches(editor, entity)
                && editor.planetaryMeshPolicy->settings.mode == "universe";
        }

        void refresh_editor_planetary_body_inventory(EditorState& editor,const EditorEntity& entity)
        {
            auto& policy=editor.planetaryMeshPolicy;
            if(!policy||policy->body_inventory_checked
                ||!editor_planetary_stream_identity_matches(editor,entity))return;
            // One verified-base attempt per policy; a missing/corrupt saved
            // artifact is not retried on each render/view refresh.
            policy->body_inventory_checked=true;
            auto generated=entity.meshSnapshot;
            const auto& identity=*policy->generated_mesh;
            if(!generated||generated->key().words!=identity.artifact_key
                ||generated->vertices().size()!=identity.vertex_count
                ||generated->indices().size()!=identity.index_count)
            {
                // The displayed LOD cut is disposable and has a different
                // content key. Body receipts belong to the exact saved asset;
                // validating them must not replace the current render snapshot.
                const auto loaded=asset::mesh::read_project_artifact(
                    resolve_editor_path(std::filesystem::path{editor.projectRoot}),
                    entity.meshAsset->logical_path,{identity.artifact_key},
                    editor_planetary_mesh_budget(editor).artifact);
                if(!loaded||loaded.artifact->vertices().size()!=identity.vertex_count
                    ||loaded.artifact->indices().size()!=identity.index_count)return;
                generated=loaded.artifact;
            }
            policy->generated_bodies=project_planetary::mesh_generation_bodies(
                resolve_editor_path(std::filesystem::path{editor.projectRoot}),policy->source,
                identity,*generated);
        }

        // Source verification and the ordinary validated mesh load are separate
        // reopen stages. Pending work is not evidence that regeneration is needed.
        [[nodiscard]] std::string_view editor_planetary_restore_progress(const EditorState& editor) noexcept
        {
            if(editor.projectRoot.empty()) return {};
            if(editor.planetaryCheckedRoot!=editor.projectRoot || editor.planetaryAttachmentPending)
                return "Validating this project's saved Planetary source and generation evidence...";
            if(!editor.planetaryAttachment.source_attached() || !editor.planetaryMeshPolicy) return {};
            for(const auto& entity:editor.entities)
            {
                if(!entity.visible || !editor_planetary_stream_identity_matches(editor,entity)) continue;
                if(!entity.meshSnapshot && !entity.meshLoadAttempted)
                    return "Saved terrain is awaiting preview validation. Open World to load its mesh and body inventory.";
                if(entity.meshSnapshot && !editor.planetaryMeshPolicy->body_inventory_checked)
                    return "Validating the saved terrain's body inventory...";
            }
            return {};
        }

        [[nodiscard]] bool select_editor_planetary_body(EditorState& editor,std::size_t index,std::uint32_t body)
        {
            if(index>=editor.entities.size())return false;
            const auto& entity=editor.entities[index];
            if(!editor_planetary_stream_identity_matches(editor,entity)
                ||!project_planetary::source_generation_is_current(
                    resolve_editor_path(std::filesystem::path{editor.projectRoot}),editor.planetaryMeshPolicy->source)
                ||std::none_of(editor.planetaryMeshPolicy->generated_bodies.begin(),editor.planetaryMeshPolicy->generated_bodies.end(),
                    [&](const auto& value){return value.id==body;}))return false;
            select_editor_entity(editor,index);
            editor.planetaryBodySelection=EditorState::PlanetaryBodySelection{entity.sceneObjectId,entity.meshAsset->artifact_key,body};
            return true;
        }

        [[nodiscard]] std::optional<scene_interaction::FocusTarget> editor_planetary_body_focus(
            const EditorState& editor,const EditorEntity& entity)
        {
            if(!editor_planetary_stream_identity_matches(editor,entity))return {};
            const auto& policy=*editor.planetaryMeshPolicy;
            auto selected=policy.settings.body_index;
            if(editor.planetaryBodySelection&&editor.planetaryBodySelection->entity==entity.sceneObjectId
                &&editor.planetaryBodySelection->mesh_key==entity.meshAsset->artifact_key)
                selected=editor.planetaryBodySelection->body;
            const auto body=std::find_if(policy.generated_bodies.begin(),policy.generated_bodies.end(),
                [&](const auto& value){return value.id==selected;});
            if(body==policy.generated_bodies.end())return {};
            const auto units=scene::mesh_render_scale({1.0f,1.0f,1.0f},entity.meshAsset);
            if(!units)return {};
            const ray::Aabb local{{body->minimum[0]*(*units)[0],body->minimum[1]*(*units)[1],body->minimum[2]*(*units)[2]},
                {body->maximum[0]*(*units)[0],body->maximum[1]*(*units)[1],body->maximum[2]*(*units)[2]}};
            scene::SceneObjectSnapshot object;object.position=entity.position;object.rotation=entity.rotation;object.scale=entity.scale;
            const auto bounds=scene_interaction::detail::world_bounds(object,local,0.0f);
            if(!bounds)return {};
            const auto target=scene_interaction::detail::scale(scene_interaction::detail::add(bounds->minimum,bounds->maximum),0.5f);
            const auto radius=scene_interaction::detail::length(scene_interaction::detail::scale(
                scene_interaction::detail::subtract(bounds->maximum,bounds->minimum),0.5f));
            if(!scene_interaction::detail::finite(target)||!std::isfinite(radius)||radius<=0.0f)return {};
            return scene_interaction::FocusTarget{*bounds,target,radius,0.0f,1u};
        }

        void refresh_editor_planetary_mesh_policy(EditorState& editor, bool prefer_saved_settings=false)
        {
            editor.sceneInteractionDirty = true;
            editor.planetaryMeshPolicy.reset();
            if(editor.projectRoot.empty()||!editor.planetaryAttachment.source_attached())return;
            const auto root=resolve_editor_path(std::filesystem::path{editor.projectRoot});
            const auto saved=project_planetary::load_settings(root);
            if(!saved)return;
            const auto source=project_planetary::capture_source_generation(root,editor.planetaryAttachment.content_hash);
            if(!source||!project_planetary::source_generation_is_current(root,*source))return;
            editor.planetaryMeshPolicy=EditorState::PlanetaryMeshPolicy{editor.projectRoot,*source,*saved};
            if(!prefer_saved_settings)for(const auto& entity:editor.entities)
                if(const auto identity=editor_planetary_mesh_identity(entity))
                    if(const auto generated=project_planetary::mesh_generation_settings(root,*source,*identity))
                    {editor.planetaryMeshPolicy->settings=*generated;editor.planetaryMeshPolicy->generated_mesh=*identity;
                        refresh_editor_planetary_body_inventory(editor,entity);break;}
            // Verified source admission changes the shared mesh envelope. An
            // ordinary mesh may have been deferred while terrain still counted
            // against the fallback budget; retry missing loads once here too.
            for(auto& entity:editor.entities)
                if(!entity.meshSnapshot)entity.meshLoadAttempted=false;
        }

        [[nodiscard]] bool model_import_review_current(const EditorState& editor) noexcept
        {
            if (!editor.modelImportReview) return false;
            const auto& review = *editor.modelImportReview;
            const auto& evidence = editor.projectLifecycle.evidence();
            return evidence.project_selected && review.projectGeneration != 0u
                && review.projectGeneration == evidence.generations.selected_project.generation
                && review.projectId == editor.projectId && review.projectRoot == editor.projectRoot
                && review.requestedPath == editor.meshImportPathDraft;
        }

        [[nodiscard]] bool review_project_model(EditorState& editor, const std::filesystem::path& source)
        {
            editor.modelImportReview.reset();
            try
            {
                const auto& evidence = editor.projectLifecycle.evidence();
                if (editor.projectRoot.empty() || editor.projectId.empty() || !evidence.project_selected)
                { editor.projectStatus = "Open a project before reviewing a model."; return false; }
                const auto status = std::filesystem::symlink_status(source);
                if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status))
                { editor.projectStatus = "Choose a regular GLB model file."; return false; }
                std::ifstream input(source, std::ios::binary | std::ios::ate);
                const auto length = input.tellg();
                if (!input || length <= 0 || std::uint64_t(length) > 64ull * 1024ull * 1024ull)
                { editor.projectStatus = "The model is unreadable or exceeds the 64 MiB import limit."; return false; }
                EditorState::ModelImportReview review{};
                review.requestedPath = editor.meshImportPathDraft;
                review.projectId = editor.projectId; review.projectRoot = editor.projectRoot;
                review.projectGeneration = evidence.generations.selected_project.generation;
                review.source = source;
                review.sourceBytes.resize(static_cast<std::size_t>(length)); input.seekg(0);
                if (!input.read(reinterpret_cast<char*>(review.sourceBytes.data()), static_cast<std::streamsize>(review.sourceBytes.size()))
                    || input.peek() != std::char_traits<char>::eof())
                { editor.projectStatus = "The model changed while being captured. Review it again."; return false; }
                review.sourceSha256 = core::sha256::hex(core::sha256::hash(std::span{
                    reinterpret_cast<const std::uint8_t*>(review.sourceBytes.data()), review.sourceBytes.size()}));
                review.compiled = asset::model_import::compile_base_color_glb(review.sourceBytes);
                if (!review.compiled)
                { editor.projectStatus = "Model review refused: " + review.compiled.diagnostic; return false; }
                auto minimum = review.compiled.model.primitives.front().geometry->minimum();
                auto maximum = review.compiled.model.primitives.front().geometry->maximum();
                for (const auto& primitive : review.compiled.model.primitives)
                {
                    const auto& sampler = review.compiled.model.materials.at(*primitive.material).base_color_texture->sampler;
                    if ((sampler.min_filter && *sampler.min_filter >= 9984u)
                        || (sampler.min_filter && sampler.mag_filter && sampler.min_filter != sampler.mag_filter))
                    { editor.projectStatus = "This model needs a mipmap or mixed min/mag sampler not yet represented by scene materials."; return false; }
                    const auto low = primitive.geometry->minimum(), high = primitive.geometry->maximum();
                    for (std::size_t axis = 0; axis < 3; ++axis)
                    {
                        minimum[axis] = (std::min)(minimum[axis], low[axis]);
                        maximum[axis] = (std::max)(maximum[axis], high[axis]);
                    }
                }
                std::array<double, 3> size{};
                for (std::size_t axis = 0; axis < size.size(); ++axis)
                    size[axis] = (double(maximum[axis]) - minimum[axis]) * review.compiled.model.meters_per_unit;
                editor.modelImportReview = std::move(review);
                editor.projectStatus = epochengine::format_text(
                    "Model bounds {:.3f} × {:.3f} × {:.3f} m (X × Y × Z). Walking collision is off at import.",
                    size[0], size[1], size[2]);
                return true;
            }
            catch (...) { editor.projectStatus = "Model review failed; the scene and project assets are unchanged."; return false; }
        }

        [[nodiscard]] bool prepare_model_import_directory(const std::filesystem::path& root,
            const std::filesystem::path& relative)
        {
            if (relative.empty() || relative.is_absolute()) return false;
            auto current = root;
            for (const auto& component : relative)
            {
                if (component.empty() || component == "." || component == "..") return false;
                current /= component;
                std::error_code error{};
                auto status = std::filesystem::symlink_status(current, error);
                if (error && error != std::errc::no_such_file_or_directory) return false;
                if (!std::filesystem::exists(status))
                {
                    error.clear();
                    if (!std::filesystem::create_directory(current, error) || error) return false;
                }
                if (std::filesystem::is_symlink(std::filesystem::symlink_status(current))
                    || !std::filesystem::is_directory(current) || std::filesystem::canonical(current) != current)
                    return false;
            }
            return true;
        }

        [[nodiscard]] bool retain_model_import_file(const std::filesystem::path& path,
            std::span<const std::byte> bytes)
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (error && error != std::errc::no_such_file_or_directory) return false;
            if (std::filesystem::exists(status))
            {
                if (!std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)
                    || std::filesystem::canonical(path) != path || std::filesystem::file_size(path) != bytes.size()) return false;
                std::ifstream input(path, std::ios::binary);
                std::vector<std::byte> existing(bytes.size());
                return input.read(reinterpret_cast<char*>(existing.data()), static_cast<std::streamsize>(existing.size()))
                    && input.peek() == std::char_traits<char>::eof()
                    && std::equal(existing.begin(), existing.end(), bytes.begin(), bytes.end());
            }
            error.clear();
            return platform::filesystem::exclusive_create_and_write(path, bytes, error);
        }

        struct ModelSurfacePose final
        {
            std::array<double, 3> pivot{};
            std::array<float, 3> rotation{};
            scene::SceneTerrainAttachment attachment{};
            struct PlayerStart final { std::array<double, 3> pivot{}; std::array<float, 3> rotation{}; };
            std::optional<PlayerStart> playerStart{};
        };

        [[nodiscard]] std::optional<ModelSurfacePose> model_surface_pose(
            const asset::model_import::BaseColorCompilation& compiled,
            const previewgrid::PreciseMeshTransform& terrain,
            const project_planetary::SurfaceQuery& query,
            const project_planetary::SurfaceResult& sample,
            scene::SceneObjectId terrainId, const project_planetary::SurfaceResult* start = nullptr)
        {
            const auto supported = [&](const project_planetary::SurfaceResult& point) {
                return point.code == project_planetary::SurfaceCode::ready && point.request_id == query.request_id
                    && std::isfinite(point.height_meters) && (query.body.id != 0u || point.height_meters > 0.0);
            };
            if (!compiled || !supported(sample) || (start && !supported(*start))
                || !previewgrid::valid_precise_mesh_transform(terrain))
                return {};
            namespace alignment = scene::surface_alignment;
            std::optional<alignment::VerticalBounds> bounds;
            for (const auto& part : compiled.model.primitives)
            {
                if (!part.geometry) return {};
                const alignment::VerticalBounds local{part.geometry->minimum()[1], part.geometry->maximum()[1]};
                if (!local.valid()) return {};
                bounds = bounds ? alignment::merge_vertical_bounds(*bounds, local) : std::optional{local};
            }
            if (!bounds || compiled.model.meters_per_unit <= 0.0) return {};
            const auto support = alignment::align_bottom_to_surface(*bounds, 0.0f,
                static_cast<float>(compiled.model.meters_per_unit), 0.0f);
            if (!support) return {};
            scene::SceneTerrainAttachment attachment{
                .terrain_object_id = terrainId, .body_index = query.body.id,
                .terrain_artifact_key = query.mesh.artifact_key,
                .terrain_coordinates = {query.mesh.coordinates.meters_per_unit, query.mesh.coordinates.source_origin_meters}};
            std::array<double, 3> local{}, up{};
            for (std::size_t axis = 0; axis < local.size(); ++axis)
            {
                attachment.reference_center_meters[axis] =
                    query.body.center_meters[axis] - query.mesh.coordinates.source_origin_meters[axis];
                local[axis] = attachment.reference_center_meters[axis] + sample.point_meters[axis];
            }
            if (!attachment.valid()) return {};
            for (std::size_t row = 0; row < up.size(); ++row)
                for (std::size_t axis = 0; axis < up.size(); ++axis)
                    up[row] += terrain.rotation[row * 3 + axis] * terrain.scale[axis] * sample.radial_up[axis];
            const auto length = std::hypot(up[0], up[1], up[2]);
            if (!std::isfinite(length) || length < 1e-10) return {};
            for (auto& axis : up) axis /= length;
            const auto rotation = forest::preview_mesh::surface_rotation(up, {0.0, 0.0, 1.0});
            if (!rotation) return {};
            auto pivot = terrain.apply(local);
            for (std::size_t axis = 0; axis < pivot.size(); ++axis)
                pivot[axis] += up[axis] * support->origin_y;
            if (!scene::world_position({}, pivot)) return {};
            ModelSurfacePose pose{pivot, *rotation, attachment};
            if (start)
            {
                std::array<double, 3> localStart{}, startUp{}, heading{};
                for (std::size_t axis = 0; axis < localStart.size(); ++axis)
                    localStart[axis] = attachment.reference_center_meters[axis] + start->point_meters[axis];
                const auto startPivot = terrain.apply(localStart);
                for (std::size_t row = 0; row < startUp.size(); ++row)
                {
                    for (std::size_t axis = 0; axis < startUp.size(); ++axis)
                        startUp[row] += terrain.rotation[row * 3 + axis] * start->radial_up[axis];
                    // The surface rotation aligns +Z, while Play faces -Z.
                    heading[row] = startPivot[row] - pivot[row];
                }
                const auto startRotation = forest::preview_mesh::surface_rotation(startUp, heading);
                if (!scene::world_position({}, startPivot) || !startRotation) return {};
                pose.playerStart = ModelSurfacePose::PlayerStart{startPivot, *startRotation};
            }
            return pose;
        }

        [[nodiscard]] bool approve_project_model_import(EditorState& editor, const ModelSurfacePose* placement = nullptr)
        {
            const auto fail = [&](std::string message)
            { editor.projectStatus = std::move(message); push_editor_log(editor, "[asset] " + editor.projectStatus); return false; };
            if (!model_import_review_current(editor))
            { editor.modelImportReview.reset(); return fail("Model review expired after a project or path change. Review the model again."); }
            try
            {
                const auto& review = *editor.modelImportReview;
                const auto& compiled = review.compiled;
                const bool landing = placement && placement->playerStart;
                if (!compiled || core::sha256::hex(core::sha256::hash(std::span{
                        reinterpret_cast<const std::uint8_t*>(review.sourceBytes.data()), review.sourceBytes.size()})) != review.sourceSha256)
                    return fail("The reviewed model snapshot is invalid; no scene change was made.");
                const auto root = std::filesystem::canonical(resolve_editor_path(
                    std::filesystem::path{std::u8string{editor.projectRoot.begin(), editor.projectRoot.end()}}));
                if (!editor.sceneDocument.initialized() && !rebuild_editor_scene_document(editor, "Initialize model import")) return false;
                const bool policyCurrent = editor_planetary_mesh_policy_current(editor);
                const auto budget = policyCurrent ? editor_planetary_mesh_budget(editor) : project_planetary::SceneMeshBudget{};
                std::uint64_t indices{}, cacheBytes{}, ordinaryIndices{}, ordinaryBytes{};
                for (const auto& entity : editor.entities) if (entity.meshAsset)
                {
                    const auto count = entity.meshSnapshot ? entity.meshSnapshot->indices().size() : entity.meshAsset->index_count;
                    const auto vertices = entity.meshSnapshot ? entity.meshSnapshot->vertices().size() : entity.meshAsset->vertex_count;
                    const auto bytes = 64u + std::uint64_t(vertices)*44u + std::uint64_t(count)*4u;
                    cacheBytes += bytes; if (entity.visible) indices += count;
                    if (entity.category != "Planetary Terrain" || !policyCurrent)
                    { ordinaryBytes += bytes; if (entity.visible) ordinaryIndices += count; }
                }
                for (const auto& primitive : compiled.model.primitives)
                {
                    const auto count = primitive.geometry->indices().size();
                    const auto bytes = 64u + primitive.geometry->vertices().size()*44u + count*4u;
                    indices += count; ordinaryIndices += count; cacheBytes += bytes; ordinaryBytes += bytes;
                }
                if (indices > budget.frame_indices || cacheBytes > budget.cache_bytes
                    || ordinaryIndices > 196'608u || ordinaryBytes > 16ull*1024ull*1024ull)
                    return fail("This model would exceed the existing scene geometry budget. The previous scene is retained.");
                project_textures::TextureArtifactLibrary library{editor.projectId, root.generic_string()};
                if (!library.valid()) return fail("The project's existing texture library is unavailable.");
                std::vector<std::string> texturePaths;
                std::vector<project_textures::TextureArtifactLocator> locators;
                for (const auto& texture : compiled.textures)
                {
                    const auto logical = "Assets/Textures/" + asset::texture::content_hash_hex(texture.artifact.identity.key) + ".png";
                    const auto locator = library.identify(logical, texture.artifact);
                    if (!locator) return fail("The compiled model texture cannot enter this project's texture library.");
                    texturePaths.push_back(logical); locators.push_back(locator);
                }
                std::vector<authoring::scene::SceneOperation> operations;
                std::vector<EditorEntity> entities;
                if (landing)
                {
                    EditorEntity start{};
                    start.positionAnchor = placement->playerStart->pivot;
                    start.rotation = placement->playerStart->rotation;
                    start.terrainAttachment = placement->attachment;
                    auto spawn = editor_surface_spawn_operations(editor, start);
                    if (!spawn) return fail("The project needs a visible primary PlayerStart on the reviewed terrain before setting up landing.");
                    for (auto& operation : *spawn) operations.push_back(std::move(operation));
                    const auto used = std::ranges::count_if(editor.entities, [](const EditorEntity& entity) {
                        return entity.visible && entity.terrainAttachment && entity.terrainAttachment->solid_walk_box;
                    });
                    if (static_cast<std::size_t>(used) + compiled.model.primitives.size() > project_planetary_runtime::maximum_walk_boxes)
                        return fail("The reviewed model would exceed the planetary walking collider budget.");
                }
                const auto sourceNameUtf8 = review.source.stem().generic_u8string();
                const std::string baseName{sourceNameUtf8.begin(), sourceNameUtf8.end()};
                const auto transactionLabel = landing ? std::string{"Set Up Landing Area"} : "Import model " + baseName;
                const auto address = [](std::uint32_t wrap) { return wrap == 33071u ? scene::SceneTextureAddress::clamp_to_edge
                    : wrap == 33648u ? scene::SceneTextureAddress::mirrored_repeat : scene::SceneTextureAddress::repeat; };
                for (std::size_t index{}; index < compiled.model.primitives.size(); ++index)
                {
                    const auto& primitive = compiled.model.primitives[index];
                    const auto textureIndex = compiled.primitive_textures.at(index);
                    const auto& material = compiled.model.materials.at(*primitive.material);
                    const auto& sampler = material.base_color_texture->sampler;
                    EditorEntity entity{};
                    entity.name = make_entity_name(editor, baseName + (compiled.model.primitives.size() == 1u ? "" : " " + std::to_string(index+1u)));
                    entity.type = "IndexedMesh"; entity.category = "World";
                    entity.sceneObjectId = allocate_editor_scene_object_id(editor, entity.name);
                    if (placement)
                    {
                        entity.positionAnchor = placement->pivot;
                        entity.rotation = placement->rotation;
                        entity.terrainAttachment = placement->attachment;
                        entity.terrainAttachment->solid_walk_box = landing;
                    }
                    entity.meshAsset = scene::SceneMeshSnapshot{
                        "Assets/Mesh/" + asset::texture::content_hash_hex(primitive.geometry->key()) + ".epoch_mesh",
                        primitive.geometry->key().words, static_cast<std::uint32_t>(primitive.geometry->vertices().size()),
                        static_cast<std::uint32_t>(primitive.geometry->indices().size()),
                        scene::SceneMeshCoordinates{compiled.model.meters_per_unit,{0,0,0}}};
                    const auto& logical = texturePaths.at(textureIndex);
                    entity.textureMaterial = scene::SceneTextureMaterialSnapshot{
                        .logical_path=logical, .artifact_key=compiled.textures.at(textureIndex).artifact.identity.key.words,
                        .stable_key=project_assets::stable_asset_identity(library.project_key(),project_assets::AssetKind::material,
                            "Assets/Models/"+review.sourceSha256+"/material-"+std::to_string(*primitive.material)),
                        .filter=(sampler.mag_filter.value_or(sampler.min_filter.value_or(9729u))==9728u)
                            ? scene::SceneTextureFilter::nearest : scene::SceneTextureFilter::linear,
                        .address_u=address(sampler.wrap_s),.address_v=address(sampler.wrap_t),
                        .alpha=scene::SceneTextureAlphaMode::opaque,.color_space=scene::SceneTextureColorSpace::srgb};
                    const auto descriptor = authoring::scene::descriptor_from_snapshot_object(editor_entity_snapshot(entity),
                        static_cast<std::uint32_t>(editor.entities.size()+index),editor.sceneDocument.limits());
                    if (!descriptor) return fail("The scene cannot represent this reviewed model; no files were published.");
                    operations.emplace_back(authoring::scene::ObjectCreatedOperation{.object=*descriptor});
                    entity.meshSnapshot=primitive.geometry;entity.meshLoadAttempted=true;entities.push_back(std::move(entity));
                }
                if (landing)
                {
                    const auto scale = compiled.model.meters_per_unit;
                    const auto basis = previewgrid::make_precise_mesh_transform(placement->pivot, {scale, scale, scale},
                        {placement->rotation[0], placement->rotation[1], placement->rotation[2]});
                    if (!basis) return fail("The beacon's reviewed bounds could not be measured.");
                    auto settings = editor.sceneDocument.project_settings();
                    double nearest = std::numeric_limits<double>::infinity();
                    for (const auto& part : entities)
                    {
                        const auto low = part.meshSnapshot->minimum(), high = part.meshSnapshot->maximum();
                        std::array<double, 3> closest{};
                        for (std::size_t axis = 0; axis < closest.size(); ++axis)
                        {
                            double coordinate{};
                            for (std::size_t row = 0; row < closest.size(); ++row)
                                coordinate += basis->rotation[row * 3 + axis] * (placement->playerStart->pivot[row] - basis->position[row]);
                            closest[axis] = std::clamp(coordinate / scale, double(low[axis]), double(high[axis]));
                        }
                        const auto point = basis->apply(closest);
                        const auto distance = std::hypot(point[0] - placement->playerStart->pivot[0],
                            point[1] - placement->playerStart->pivot[1], point[2] - placement->playerStart->pivot[2]);
                        if (distance < nearest)
                        {
                            nearest = distance;
                            settings.reach_objective = scene::SceneReachObjective{part.sceneObjectId, 5.0};
                        }
                    }
                    if (!std::isfinite(nearest) || nearest <= 5.0)
                        return fail("PlayerStart needs to be outside the beacon objective's 5 m range.");
                    operations.emplace_back(authoring::scene::ProjectSettingsChangedOperation{std::move(settings)});
                }
                auto trial = editor.sceneDocument;
                if (!trial.apply_transaction(operations,transactionLabel))
                    return fail("The complete model transaction was refused; the previous scene and files are unchanged.");
                if (!prepare_model_import_directory(root,"Assets/Models")
                    || !prepare_model_import_directory(root,"Assets/Textures")
                    || !asset::mesh::prepare_project_mesh_directory(root))
                    return fail("Model asset directories are redirected or unavailable; the previous scene is retained.");
                for (std::size_t index{}; index < compiled.textures.size(); ++index)
                {
                    const auto& texture = compiled.textures[index];
                    if (!retain_model_import_file(root/texturePaths[index],compiled.model.images.at(texture.source_image).encoded))
                        return fail("An existing model image differs or cannot be written. It was not overwritten.");
                    const auto relative = std::filesystem::path{locators[index].storage_path}.lexically_relative(root);
                    if (!prepare_model_import_directory(root,relative.parent_path()))
                        return fail("Compiled texture directories are redirected or unavailable; the previous scene is retained.");
                    const auto published = library.persist(texturePaths[index],texture.artifact);
                    if (!published)
                        return fail("Compiled texture publication failed (library code " + std::to_string(static_cast<unsigned>(published.code))
                            + "); the previous scene is retained. New immutable assets remain available.");
                }
                if (!retain_model_import_file(root/"Assets/Models"/(review.sourceSha256+".glb"),review.sourceBytes))
                    return fail("The model source could not be retained without replacing existing bytes.");
                for (const auto& entity : entities)
                {
                    const auto bytes = asset::mesh::encode_artifact(*entity.meshSnapshot);
                    if (bytes.empty() || !retain_model_import_file(root/entity.meshAsset->logical_path,bytes))
                        return fail("Mesh publication failed; the previous scene is retained and no existing asset was replaced.");
                }
                if (!model_import_review_current(editor)) return fail("The project changed before import commit. Review again.");
                const auto selected = entities.front().sceneObjectId;
                if (!apply_editor_scene_transaction(editor,operations,transactionLabel,selected)) return false;
                for (const auto& imported : entities)
                    if (const auto index = editor_entity_index(editor,imported.sceneObjectId))
                    { editor.entities[*index].meshSnapshot=imported.meshSnapshot;editor.entities[*index].meshLoadAttempted=true; }
                // Import selects the new parts, not the previously selected
                // terrain or other objects that a subsequent move would alter.
                if (const auto first = editor_entity_index(editor,selected))
                    select_editor_entity(editor,*first);
                for (std::size_t index=1u;index<entities.size();++index)
                    if (const auto part = editor_entity_index(editor,entities[index].sceneObjectId))
                        add_editor_entity_selection(editor,*part);
                editor.modelImportReview.reset();
                editor.projectStatus = landing
                    ? "Landing area set up. Save Project retains the solid beacon, PlayerStart and 5 m reach objective."
                    : placement
                    ? "Reviewed model attached to the verified dry ocean shoreline. Save Project retains the attachment. Collision requires a separate check."
                    : "Reviewed model imported in metres with exact mesh and base-color texture references. Undo restores the scene; Save Project retains placement.";
                push_editor_log(editor,"[asset] "+editor.projectStatus);
                return true;
            }
            catch (...) { return fail("Model import failed. Existing source/assets were not replaced; new immutable assets may remain unreferenced."); }
        }

        [[nodiscard]] bool import_project_mesh(EditorState& editor, const std::filesystem::path& source,
            std::string_view managed_category = {}, asset::mesh::Limits read_limits = asset::mesh::Limits(),
            std::optional<project_planetary::GeneratedMeshFrame> mesh_frame = {})
        {
            const auto fail = [&](std::string message)
            {
                editor.projectStatus = std::move(message);
                push_editor_log(editor, "[asset] " + editor.projectStatus);
                return false;
            };
            if (editor.projectRoot.empty() || editor.projectId.empty()
                || !editor.projectLifecycle.evidence().project_selected)
                return fail("Open a project before importing a mesh.");
            const bool policyCurrent=editor_planetary_mesh_policy_current(editor);
            const bool planetary=managed_category=="Planetary Terrain"&&policyCurrent;
            const auto budget=policyCurrent?editor_planetary_mesh_budget(editor):project_planetary::SceneMeshBudget{};
            const auto allowed=planetary?budget.artifact:asset::mesh::Limits{};
            if(read_limits.maximum_vertices>allowed.maximum_vertices||read_limits.maximum_indices>allowed.maximum_indices
                ||read_limits.maximum_bytes>allowed.maximum_bytes)
                return fail("This mesh needs an active, verified planetary dependency and its saved recipe.");
            const auto imported = asset::mesh::read_artifact_file(source,read_limits);
            if (!imported) return fail("The mesh could not be imported. Use a valid .epoch_mesh artifact within the geometry and file-size limits.");
            std::optional<EditorEntity> replaced{};
            if (!managed_category.empty())
            {
                for (const auto& entity : editor.entities)
                    if (entity.category == managed_category)
                    {
                        if (replaced) return fail("More than one terrain object owns this package preview. Resolve it before regenerating.");
                        replaced = entity;
                    }
            }
            std::uint64_t frame_indices = imported.artifact->indices().size();
            std::uint64_t cache_bytes=64u+imported.artifact->vertices().size()*sizeof(asset::mesh::Vertex)
                +imported.artifact->indices().size()*sizeof(std::uint32_t);
            std::uint64_t ordinary_indices=planetary?0u:frame_indices,ordinary_bytes=planetary?0u:cache_bytes;
            for (const auto& entity : editor.entities)
                if (entity.meshAsset && (!replaced || entity.sceneObjectId != replaced->sceneObjectId))
                {
                    const auto count=entity.meshSnapshot?entity.meshSnapshot->indices().size():entity.meshAsset->index_count;
                    const auto vertices=entity.meshSnapshot?entity.meshSnapshot->vertices().size():entity.meshAsset->vertex_count;
                    const auto bytes=64u+std::uint64_t(vertices)*sizeof(asset::mesh::Vertex)+std::uint64_t(count)*sizeof(std::uint32_t);
                    cache_bytes+=bytes;
                    if(entity.visible)frame_indices+=count;
                    if(entity.category!="Planetary Terrain"||!policyCurrent)
                    {ordinary_bytes+=bytes;if(entity.visible)ordinary_indices+=count;}
                }
            if (frame_indices > budget.frame_indices ||cache_bytes>budget.cache_bytes
                ||ordinary_indices>196'608u||ordinary_bytes>std::uint64_t{16u}*1024u*1024u)
                return fail("This mesh would exceed the preview geometry budget. Reduce its detail or remove another imported mesh.");
            try
            {
                const auto root = std::filesystem::canonical(resolve_editor_path(std::filesystem::path{editor.projectRoot}));
                const std::string relative = "Assets/Mesh/" + asset::texture::content_hash_hex(imported.artifact->key()) + ".epoch_mesh";
                const auto destination = root / relative;
                if (!asset::mesh::prepare_project_mesh_directory(root))
                    return fail("The project mesh directory is unavailable or points outside its project.");
                if (std::filesystem::exists(destination))
                {
                    if (!asset::mesh::read_project_artifact(root, relative, imported.artifact->key(),read_limits))
                        return fail("An existing mesh artifact at that identity is invalid. It was not overwritten.");
                }
                else
                {
                    const auto bytes = asset::mesh::encode_artifact(*imported.artifact);
                    std::error_code error;
                    if (bytes.empty() || !platform::filesystem::exclusive_create_and_write(destination, bytes, error))
                        return fail("Could not save the imported mesh in this project.");
                }
                EditorEntity entity = replaced.value_or(EditorEntity{});
                entity.name = replaced ? replaced->name : make_entity_name(editor,
                    managed_category.empty() ? source.stem().string() : std::string(managed_category));
                entity.type = "IndexedMesh";
                entity.category = managed_category.empty() ? "World" : std::string(managed_category);
                entity.meshAsset = scene::SceneMeshSnapshot{relative, imported.artifact->key().words,
                    static_cast<std::uint32_t>(imported.artifact->vertices().size()),
                    static_cast<std::uint32_t>(imported.artifact->indices().size())};
                if (mesh_frame)
                {
                    scene::SceneMeshCoordinates coordinates{
                        mesh_frame->meters_per_unit, mesh_frame->source_origin_meters};
                    if (!coordinates.valid())
                        return fail("The mesh coordinate frame is invalid. The previous scene is retained.");
                    entity.meshAsset->coordinates = coordinates;
                }
                const auto name = entity.name;
                bool committed{};
                if (replaced)
                {
                    const auto handle = editor.sceneDocument.find(authoring::scene::ObjectId{entity.sceneObjectId});
                    const auto converted = authoring::scene::descriptor_from_snapshot_object(
                        editor_entity_snapshot(entity), 0u, editor.sceneDocument.limits());
                    if (handle && converted)
                    {
                        const std::array<authoring::scene::SceneOperation, 2> operations{
                            authoring::scene::ObjectDestroyedOperation{.object=*handle},
                            authoring::scene::ObjectCreatedOperation{.object=*converted}};
                        committed = apply_editor_scene_transaction(editor, operations,
                            "Regenerate " + name, entity.sceneObjectId);
                    }
                }
                else committed = create_editor_scene_entity(editor, std::move(entity), "Import mesh " + name);
                if (!committed)
                    return fail("The scene rejected the mesh. Its imported source artifact is retained in Assets/Mesh.");
                if (const auto selected = editor_entity_index(editor, editor.selectedEntityId))
                {
                    editor.entities[*selected].meshSnapshot = imported.artifact;
                    editor.entities[*selected].meshLoadAttempted = true;
                }
                editor.sceneInteractionDirty = true;
                if(planetary)refresh_editor_planetary_mesh_policy(editor);
                editor.projectStatus = "Imported " + name + ". Save Project to keep its scene placement.";
                push_editor_log(editor, "[asset] " + editor.projectStatus);
                return true;
            }
            catch (...) { return fail("Could not write the mesh into the active project. Existing source was not replaced."); }
        }

        std::optional<editor_project_textures::TextureCatalogEntry> import_selected_project_texture(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            const asset::texture::ContentHash& generatedKey = asset::texture::ContentHash())
        {
            auto* controller = project_texture_controller(editor);
            if (!controller)
                return {};
            if (editor.selectedProjectFile.empty())
            {
                editor.projectTextureStatus =
                    "Select an image or .epoch_texture source under Project/Assets.";
                return {};
            }

            const RendererBackendKind backend = renderer_backend_kind(ctx);
            const capability::SubsystemProfile renderer =
                capability::renderer_subsystem_profile_for(backend);
            const Budgets budgets =
                renderer.capability.recommended_budgets;
            const std::filesystem::path selectedPath =
                resolve_editor_path(
                    std::filesystem::path{std::u8string(editor.selectedProjectFile.begin(), editor.selectedProjectFile.end())});
            if (selectedPath.extension() == ".epoch_texture")
            {
                std::error_code relativeError{};
                const std::filesystem::path projectRoot =
                    resolve_editor_path(
                        std::filesystem::path{std::u8string(editor.projectRoot.begin(), editor.projectRoot.end())});
                const std::filesystem::path relativePath =
                    std::filesystem::relative(
                        selectedPath,
                        projectRoot,
                        relativeError);
                if (relativeError)
                {
                    editor.projectTextureStatus =
                        "Texture document is outside the active project.";
                    return {};
                }

                const auto relativeUtf8 = relativePath.generic_u8string();
                const auto opened = controller->open_editable(
                    std::string{relativeUtf8.begin(), relativeUtf8.end()},
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
                    return {};
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
                return opened.entry;
            }

            if (!generatedKey.empty())
            {
                const auto catalog = controller->catalog();
                const auto retained = std::find_if(catalog.begin(), catalog.end(), [&](const auto& entry) {
                    return bool(entry) && entry.source_path == selectedPath && entry.artifact_key == generatedKey;
                });
                if (retained != catalog.end())
                {
                    const auto& currentLibrary = controller->pipeline().library();
                    project_textures::TextureArtifactLibrary exactLibrary{std::string{currentLibrary.project_id()},
                        std::string{currentLibrary.project_root()}, currentLibrary.limits()};
                    const auto loaded = exactLibrary.load_exact(retained->logical_path, generatedKey);
                    if (!loaded || !project_textures::assess_texture_admission(loaded.artifact, renderer, {}, budgets))
                    {
                        editor.projectTextureStatus = "The existing generated texture could not be restored or admitted. Nothing was replaced.";
                        push_editor_log(editor, "[assets] " + editor.projectTextureStatus);
                        return {};
                    }
                    const auto selected = controller->select(retained->logical_path);
                    if (!selected)
                    {
                        editor.projectTextureStatus = "The existing generated texture is no longer selectable. Review the project texture again.";
                        return {};
                    }
                    editor.selectedAssetPath = selected.entry->logical_path;
                    editor.projectTextureStatus = "Reused the verified project texture " + selected.entry->logical_path + ".";
                    push_editor_log(editor, "[assets] " + editor.projectTextureStatus);
                    return selected.entry;
                }
            }

            const auto imported = controller->import_source(
                selectedPath,
                renderer,
                budgets,
                generatedKey);
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
                return {};
            }

            editor.selectedAssetPath = imported.entry->logical_path;
            editor.projectTextureStatus = epochengine::format_text(
                "{} imported as {} ({}x{}, {} KiB decoded).",
                relative_browser_path(imported.entry->source_path.parent_path(), imported.entry->source_path),
                imported.entry->logical_path,
                imported.entry->width,
                imported.entry->height,
                imported.entry->decoded_bytes / 1024u);
            push_editor_log(
                editor,
                "[assets] " + editor.projectTextureStatus);
            return imported.entry;
        }

        void browse_project_image_source(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx)
        {
            if (editor.projectRoot.empty() || editor.projectId.empty()
                || !editor.projectLifecycle.evidence().project_selected)
            {
                editor.projectTextureStatus = "Open or create a project before adding an image.";
                return;
            }
            const auto selected = platform::filesystem::open_file_dialog(
                L"Add an image to this project", L"Images (JPEG is preview-only)",
                L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm",
                resolve_editor_path(std::filesystem::path{
                    std::u8string(editor.projectRoot.begin(), editor.projectRoot.end())}) / "Assets");
            if (!selected.accepted)
            {
                if (!selected.cancelled) editor.projectTextureStatus = selected.diagnostic;
                return;
            }
            const auto added = add_project_image_source(
                resolve_editor_path(std::filesystem::path{
                    std::u8string(editor.projectRoot.begin(), editor.projectRoot.end())}), selected.path);
            editor.projectTextureStatus = added.diagnostic;
            if (!added)
            {
                push_editor_log(editor, "[assets] " + editor.projectTextureStatus);
                return;
            }
            editor.assetBrowserFolder = "Assets/Images";
            editor.assetWorkspaceSection = 0u;
            editor.assetCategory = 1u;
            editor.assetSearch.clear();
            editor.assetGridScroll = 0.0f;
            editor.selectedProjectFile = display_project_path(added.path);
            editor.selectedAssetPath = editor.selectedProjectFile;
            if (added.texture_source)
                import_selected_project_texture(editor, ctx);
            else
                push_editor_log(editor, "[assets] " + editor.projectTextureStatus);
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

            std::filesystem::path path{std::u8string(rawPath.begin(), rawPath.end())};
            const std::filesystem::path root = resolve_editor_path(
                std::filesystem::path{std::u8string(projectRoot.begin(), projectRoot.end())});
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
                std::filesystem::path{std::u8string(projectRoot.begin(), projectRoot.end())});
            std::error_code relativeError{};
            const std::filesystem::path relative =
                root.empty()
                ? std::filesystem::path{}
                : std::filesystem::relative(
                    candidate,
                    root,
                    relativeError);
            const auto relativeUtf8 = relative.generic_u8string();
            const std::string candidateLogical =
                relativeError || relative.empty()
                ? std::string{}
                : std::string{relativeUtf8.begin(), relativeUtf8.end()};

            const auto found = std::find_if(
                catalog.begin(),
                catalog.end(),
                [&](const auto& texture)
                {
                    const std::filesystem::path source =
                        normalized_project_asset_path(
                            projectRoot,
                            display_project_path(texture.source_path));
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
            for (const char8_t character : path.generic_u8string())
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
            const bool jpeg = extension == ".jpg" || extension == ".jpeg";
            if (!jpeg && extension != ".png" && extension != ".bmp"
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

            const auto sourceUtf8 = source.generic_u8string();
            const std::string cacheKey{sourceUtf8.begin(), sourceUtf8.end()};
            const auto cached =
                editor.projectSourceTextureThumbnails.find(cacheKey);
            if (cached != editor.projectSourceTextureThumbnails.end()
                && cached->second.source_bytes == sourceBytes
                && cached->second.source_write_time == sourceWriteTime)
            {
                return cached->second.sprite.is_valid() ? &cached->second : nullptr;
            }
            editor.projectSourceTextureThumbnails.erase(cacheKey);

            // Retain failed metadata too: malformed/unsupported references must
            // not decode and allocate again on every idle editor frame.
            if (editor.projectSourceTextureThumbnails.size() >= 512u)
                return nullptr;
            auto [cachedSource, insertedSource] =
                editor.projectSourceTextureThumbnails.try_emplace(cacheKey,
                    ProjectTextureThumbnailState{
                        .source_bytes = sourceBytes,
                        .source_write_time = sourceWriteTime});
            (void)insertedSource;
            editor_project_textures::TexturePreview preview{
                .logical_path = display_project_path(source)};
            std::uint32_t sourceWidth{}, sourceHeight{};

            if (jpeg)
            {
                if (sourceBytes == 0u || sourceBytes > 4u * 1024u * 1024u)
                    return nullptr;
                std::ifstream input{source, std::ios::binary};
                std::string encoded(static_cast<std::size_t>(sourceBytes), '\0');
                if (!input.read(encoded.data(), static_cast<std::streamsize>(encoded.size())))
                    return nullptr;
                auto decoded = platform::filesystem::image_thumbnail(encoded);
                if (decoded.rgba.empty())
                    return nullptr;
                preview.artifact_key = asset::texture::deterministic_content_hash(
                    "editor.image_reference_preview", std::as_bytes(std::span{encoded}));
                preview.width = decoded.width;
                preview.height = decoded.height;
                preview.rgba8 = std::move(decoded.rgba);
                sourceWidth = decoded.source_width;
                sourceHeight = decoded.source_height;
            }
            else
            {

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
            preview.artifact_key = imported.artifact.identity.key;
            preview.width = mip.width;
            preview.height = mip.height;
            sourceWidth = imported.width;
            sourceHeight = imported.height;
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

            cachedSource->second = ProjectTextureThumbnailState{
                        .sprite = atlas.handles.front(),
                        .artifact_key =
                            thumbnail->artifact_key.words,
                        .width = thumbnail->width,
                        .height = thumbnail->height,
                        .source_width = sourceWidth,
                        .source_height = sourceHeight,
                        .source_bytes = sourceBytes,
                        .source_write_time = sourceWriteTime
                    };
            return &cachedSource->second;
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


        [[nodiscard]] std::filesystem::path asset_generation_cache_root()
        {
            auto root=core::path::candidate_data_root();if(root.empty())root=core::path::executable_dir();
            if(root.empty()||!std::filesystem::is_directory(root))return {};
            for(const auto part:{"cache","asset-generation"})
            {
                root/=part;std::error_code error;
                if(!std::filesystem::exists(root,error))std::filesystem::create_directory(root,error);
                if(error||std::filesystem::is_symlink(std::filesystem::symlink_status(root))||!std::filesystem::is_directory(root))
                    throw std::runtime_error{"Generation cache folder is unavailable or redirected."};
            }
            return std::filesystem::absolute(root).lexically_normal();
        }
        void restore_asset_generation_backend_selection(EditorState& editor)
        {
            auto& state=editor.assetGeneration;
            if(editor.assetGenerationPending||state.backend_selection_attempted
                ||state.phase->load()!=editor_asset_generation::Stage::idle)return;
            try
            {
                const auto root=asset_generation_cache_root();if(root.empty())return;
                std::string diagnostic;
                (void)editor_asset_generation::restore_backend_selection(state,root/"selected-backends.epoch-generation",diagnostic);
                if(!diagnostic.empty()){state.status=diagnostic;push_editor_log(editor,"[asset-generation] "+diagnostic);}
            }
            catch(...)
            {
                state.backend_selection_attempted=true;
                state.status="Remembered generation backend settings could not be read. Choose the local backend settings again; nothing was started.";
                push_editor_log(editor,"[asset-generation] "+state.status);
            }
        }
        void remember_asset_generation_backend_selection(EditorState& editor)
        {
            auto& state=editor.assetGeneration;state.backend_selection_attempted=true;
            try
            {
                const auto root=asset_generation_cache_root();std::string diagnostic;
                if(root.empty()||!editor_asset_generation::remember_backend_selection(state,root/"selected-backends.epoch-generation",diagnostic))
                    state.status+=" Backend selection could not be remembered for next launch. "+diagnostic;
            }
            catch(...){state.status+=" Backend selection could not be remembered for next launch; the current selection is retained.";}
        }
        [[nodiscard]] editor_asset_generation::ProjectIdentity asset_generation_project(const EditorState& editor)
        {
            return {editor.projectRoot,editor.projectId,
                editor.projectLifecycle.evidence().generations.selected_project.generation};
        }
        void poll_asset_generation_task(EditorState& editor)
        {
            restore_asset_generation_backend_selection(editor);
            if(!editor.assetGenerationPending&&editor.mainSurface==EditorMainSurface::AssetGeneration)
            {
                try
                {
                    const auto root=asset_generation_cache_root();
                    if(!root.empty())
                    {
                        const auto project=asset_generation_project(editor);
                        const auto path=std::filesystem::absolute(root/"last-completed.epoch-generation-result").lexically_normal();
                        if(editor_asset_generation::begin_recall(editor.assetGeneration,path,project))
                        {
                            editor.assetGeneration.phase->store(editor_asset_generation::Stage::restoring_outputs);
                            auto scheduled=schedule_editor_cancellable_task(editor,"Restore last completed asset generation",
                                [path,project](std::stop_token stop){return editor_asset_generation::restore_last_completed(path,project,stop);});
                            editor.assetGenerationCancellation=scheduled.cancellation;editor.assetGenerationPending=std::move(scheduled.completion);
                        }
                    }
                }
                catch(const std::exception& error){editor.assetGeneration.phase->store(editor_asset_generation::Stage::idle);editor.assetGeneration.status=error.what();}
            }
            if(!editor.assetGenerationPending
                ||editor.assetGenerationPending->wait_for(std::chrono::milliseconds{0})!=std::future_status::ready)return;
            try
            {
                auto completion=editor.assetGenerationPending->get();
                editor.assetGenerationPending.reset();editor.assetGenerationCancellation={};
                const bool current=editor_asset_generation::current_project(completion,asset_generation_project(editor));
                if(completion.recalled_workspace&&!current)
                {editor.assetGeneration.phase->store(editor_asset_generation::Stage::idle);
                 editor.assetGeneration.status="Project changed while retained outputs were read. Current previews were preserved.";return;}
                editor_asset_generation::accept_completion(editor.assetGeneration,std::move(completion));
                editor.assetGenerationFocusPending=true;
                if(!current)editor.assetGeneration.status+=" Project changed. Output is retained; review it for the current project before importing.";
                push_editor_log(editor,"[asset-generation] "+editor.assetGeneration.status);
            }
            catch(...)
            {
                editor.assetGenerationPending.reset();editor.assetGenerationCancellation={};
                editor.assetGeneration.phase->store(editor_asset_generation::Stage::idle);
                editor.assetGeneration.status="Asset task failed. No automatic approval or import was performed; retained job files remain available.";
            }
        }
        [[nodiscard]] std::string asset_output_review_binding(
            const EditorState& editor, bool model, std::string& diagnostic)
        {
            const auto& state = editor.assetGeneration;
            const auto& completion = model ? state.generated_model : state.reference;
            if (!completion || !completion->result || (model ? !completion->model : !completion->image))
            {
                diagnostic = model ? "Open or generate a supported GLB in Asset Generation before reviewing its import."
                    : "Open or generate a PNG in Asset Generation before reviewing it.";
                return {};
            }
            if (!has_open_project(editor) || !editor_asset_generation::current_project(*completion, asset_generation_project(editor)))
            {
                diagnostic = "Open the output for review in the current project.";
                return {};
            }
            const auto& result = completion->result;
            const auto output = display_project_path(result.output);
            const auto work = display_project_path(result.work_directory);
            return result.output_sha256 + ":" + std::to_string(output.size()) + ":" + output + work;
        }

        [[nodiscard]] bool forest_asset_step_current(const EditorState& editor, std::string_view operation)
        {
            const auto& sequence = editor.aiGoalTools;
            if (editor.sceneDocumentRevision != sequence.forest_asset_scene_revision) return false;
            if (operation == "project.save") return true;
            const auto* tree = selected_forest_species_target(editor);
            if (!tree || tree->sceneObjectId != sequence.forest_asset_target) return false;
            if (sequence.forest_asset_kind == epochengine::editor::ai_chat_goal::ForestAssetKind::tree
                && (editor.plantLabDocument.revision != sequence.forest_asset_recipe_revision
                    || editor.plantLabDocument.contentHash != sequence.forest_asset_recipe_hash)) return false;
            if (operation == "asset.image.generate" || operation == "forest.species.add") return true;
            std::string diagnostic;
            const bool model = operation == "asset.model.setup_landing";
            const auto binding = asset_output_review_binding(editor, model, diagnostic);
            return !binding.empty() && binding == sequence.forest_asset_output;
        }

        [[nodiscard]] bool generated_model_review_current(const EditorState& editor)
        {
            const auto& model = editor.assetGeneration.generated_model;
            return model && model->result && model->model
                && editor_asset_generation::current_project(*model, asset_generation_project(editor))
                && model_import_review_current(editor)
                && editor.modelImportReview->source == model->result.output
                && editor.modelImportReview->sourceSha256 == model->result.output_sha256;
        }

        [[nodiscard]] bool review_generated_project_model(EditorState& editor)
        {
            if (asset_output_review_binding(editor, true, editor.projectStatus).empty()) return false;
            const auto& result = editor.assetGeneration.generated_model->result;
            editor.meshImportPathDraft = display_project_path(result.output);
            if (!review_project_model(editor, result.output)) return false;
            if (generated_model_review_current(editor)) return true;
            editor.modelImportReview.reset();
            editor.projectStatus = "Generated model changed after validation. Review the exact retained model again.";
            return false;
        }

        [[nodiscard]] bool model_import_scene_busy(const EditorState& editor)
        {
            return editor.projectBuildPending || editor.scriptBuildPending || editor.planetaryGeneratePending
                || editor.forestSurfacePending || editor.projectRuntimeProcess.valid()
                || (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active());
        }

        [[nodiscard]] bool import_reviewed_generated_model(EditorState& editor)
        {
            if (model_import_scene_busy(editor) || editor.modelShorelinePending)
            {
                editor.projectStatus = "Stop the project and finish current build or terrain work before importing the model.";
                return false;
            }
            if (generated_model_review_current(editor)) return approve_project_model_import(editor);
            editor.projectStatus = "Review the exact retained model again before approving project import.";
            return false;
        }

        [[nodiscard]] std::optional<previewgrid::PreciseMeshTransform> model_shoreline_terrain_transform(
            const EditorEntity& terrain)
        {
            if (!terrain.visible || terrain.editorOnly
                || std::ranges::any_of(terrain.scale, [](float scale) { return scale <= 0.0f; })) return {};
            const auto pivot = scene::world_position(terrain.position, terrain.positionAnchor);
            return pivot ? previewgrid::make_precise_mesh_transform(*pivot,
                {terrain.scale[0], terrain.scale[1], terrain.scale[2]},
                {terrain.rotation[0], terrain.rotation[1], terrain.rotation[2]}) : std::nullopt;
        }

        [[nodiscard]] bool prepare_model_shoreline_review(EditorState& editor, bool landing = false)
        {
            if (!generated_model_review_current(editor) || model_import_scene_busy(editor))
            {
                editor.projectStatus = "Stop the project and review the generated GLB before requesting shoreline placement.";
                return false;
            }
            const auto treeIndex = landing && editor.selectedEntityIds.size() <= 1u
                ? editor_entity_index(editor, editor.selectedEntityId) : std::nullopt;
            const auto* tree = treeIndex ? &editor.entities[*treeIndex] : nullptr;
            const auto* forest = tree && tree->visible && tree->forestRegion && tree->forestRegion->valid()
                ? &*tree->forestRegion : nullptr;
            if (landing && !forest)
            {
                editor.projectStatus = "Select one painted tree to identify the saved forest for this landing area.";
                return false;
            }
            const auto admitted = [&](const auto& entity) {
                return (!forest || entity.sceneObjectId == forest->terrain_object_id)
                    && entity.visible && !entity.editorOnly && editor_planetary_stream_identity_matches(editor, entity);
            };
            const auto terrain = std::find_if(editor.entities.begin(), editor.entities.end(), admitted);
            if (terrain == editor.entities.end() || (!landing && !editor.planetaryMeshPolicy->settings.ocean_surface)
                || editor.sceneDocumentRevision == 0u)
            {
                editor.projectStatus = landing ? "Save the admitted planetary terrain and its painted forest before reviewing a landing area."
                    : "Generate and save planetary terrain with Ocean On before reviewing shoreline placement.";
                return false;
            }
            if (std::any_of(std::next(terrain), editor.entities.end(), admitted))
            {
                editor.projectStatus = "Keep one admitted ocean terrain visible before reviewing shoreline placement.";
                return false;
            }
            refresh_editor_planetary_body_inventory(editor, *terrain);
            const auto& policy = *editor.planetaryMeshPolicy;
            const auto body = std::find_if(policy.generated_bodies.begin(), policy.generated_bodies.end(),
                [&](const auto& value) { return value.id == (forest ? forest->body_index : 0u); });
            const auto transform = model_shoreline_terrain_transform(*terrain);
            if (body == policy.generated_bodies.end() || !transform)
            {
                editor.projectStatus = "The selected body or terrain transform could not be verified.";
                return false;
            }
            double beaconRadius{};
            if (forest)
            {
                if (!policy.generated_mesh || forest->terrain_artifact_key != policy.generated_mesh->artifact_key
                    || forest->mesh_source_origin_meters != policy.generated_mesh->coordinates.source_origin_meters
                    || forest->recipe != project_planetary::encode_settings(policy.settings)
                    || forest->body_radius_meters != body->radius_meters || forest->admitted_body_center_meters != body->center_meters
                    || policy.source.content_hash != "sha256:" + forest->provider_content_hash
                    || std::ranges::any_of(terrain->scale, [](float scale) { return std::abs(scale - 1.0f) > 1e-6f; }))
                {
                    editor.projectStatus = "Refresh forest placement against the current terrain and use terrain Scale 1, 1, 1 before setting up landing.";
                    return false;
                }
                for (const auto& part : editor.modelImportReview->compiled.model.primitives)
                {
                    const auto low = part.geometry->minimum(), high = part.geometry->maximum();
                    beaconRadius = (std::max)(beaconRadius, std::hypot(double((std::max)(std::abs(low[0]),std::abs(high[0]))),
                        double((std::max)(std::abs(low[2]),std::abs(high[2])))));
                }
                beaconRadius *= editor.modelImportReview->compiled.model.meters_per_unit;
                if (!std::isfinite(beaconRadius) || beaconRadius <= 0.0)
                { editor.projectStatus = "The reviewed model has no usable footprint for a landing beacon."; return false; }
            }
            auto artifact = terrain->meshSnapshot;
            if (artifact && artifact->key().words != policy.generated_mesh->artifact_key) artifact.reset();
            editor.modelImportReview->shoreline = EditorState::ModelShorelineTarget{
                .terrain = terrain->sceneObjectId, .sceneRevision = editor.sceneDocumentRevision,
                .workDirectory = editor.assetGeneration.generated_model->result.work_directory, .transform = *transform,
                .query = {policy.source, policy.settings, *policy.generated_mesh, *body,
                    std::move(artifact), forest ? forest->center_direction : std::array{0.0, 1.0, 0.0}, editor.sceneDocumentRevision},
                .forestTree = tree ? tree->sceneObjectId : scene::kInvalidSceneObjectId,
                .forest = forest ? std::optional{*forest} : std::nullopt, .beaconRadius = beaconRadius};
            return true;
        }

        [[nodiscard]] bool model_shoreline_review_current(const EditorState& editor)
        {
            if (!generated_model_review_current(editor) || !editor.modelImportReview->shoreline
                || model_import_scene_busy(editor)) return false;
            const auto& target = *editor.modelImportReview->shoreline;
            if (editor.sceneDocumentRevision != target.sceneRevision
                || editor.assetGeneration.generated_model->result.work_directory != target.workDirectory) return false;
            if (target.forest)
            {
                const auto tree = editor_entity_index(editor, target.forestTree);
                if (!tree || editor.selectedEntityId != target.forestTree || editor.selectedEntityIds.size() > 1u
                    || !editor.entities[*tree].visible || editor.entities[*tree].forestRegion != target.forest) return false;
            }
            const auto index = editor_entity_index(editor, target.terrain);
            if (!index || !editor_planetary_stream_identity_matches(editor, editor.entities[*index])) return false;
            const auto& policy = *editor.planetaryMeshPolicy;
            return model_shoreline_terrain_transform(editor.entities[*index]) == target.transform
                && policy.generated_mesh == target.query.mesh && policy.settings == target.query.settings
                && policy.source.content_hash == target.query.source.content_hash
                && policy.source.project_root == target.query.source.project_root
                && policy.source.project_manifest == target.query.source.project_manifest
                && std::ranges::find(policy.generated_bodies, target.query.body) != policy.generated_bodies.end();
        }

        [[nodiscard]] bool begin_model_shoreline_import(EditorState& editor)
        {
            if (editor.modelShorelinePending || !model_shoreline_review_current(editor))
            {
                editor.projectStatus = "The shoreline review expired or is already running. Request a fresh review.";
                return false;
            }
            const auto& target = *editor.modelImportReview->shoreline;
            const auto index = editor_entity_index(editor, target.terrain);
            const auto path = editor.entities[*index].meshAsset->logical_path;
            const auto limits = editor_planetary_mesh_budget(editor).artifact;
            try
            {
                const auto forestRadius = target.forest ? target.forest->radius_meters : 0.0;
                auto submitted = schedule_editor_cancellable_task(editor,
                    target.forest ? "Find a walking route outside the reviewed forest" : "Find dry ocean shoreline for model import",
                    [query = target.query, path, limits, forestRadius, beaconRadius = target.beaconRadius](std::stop_token stop) mutable
                    {
                        if (stop.stop_requested()) return EditorState::ModelSurfaceResult{
                            project_planetary::SurfaceCode::cancelled, {.message = "Shoreline search cancelled."}};
                        if (!query.artifact)
                        {
                            const auto loaded = asset::mesh::read_project_artifact(
                                query.source.project_root, path, {query.mesh.artifact_key}, limits);
                            if (!loaded) return EditorState::ModelSurfaceResult{
                                project_planetary::SurfaceCode::invalid, {.message = "The saved terrain mesh could not be verified."}};
                            query.artifact = loaded.artifact;
                        }
                        if (forestRadius > 0.0)
                        {
                            auto site = project_planetary::query_forest_landing_site(query.source.project_root,
                                query, forestRadius, beaconRadius, stop);
                            return EditorState::ModelSurfaceResult{site.code, std::move(site.operation),
                                std::move(site.beacon), std::move(site.player_start)};
                        }
                        auto shore = project_planetary::query_ocean_shoreline(query.source.project_root, query, 100.0, stop);
                        return EditorState::ModelSurfaceResult{shore.code, std::move(shore.operation), std::move(shore.land)};
                    });
                editor.modelShorelineCancellation = submitted.cancellation;
                editor.modelShorelinePending.emplace(std::move(submitted.completion));
                editor.projectStatus = target.forest ? "Sampling routes from PlayerStart to the beacon outside the reviewed forest."
                    : "Finding dry ground within 100 m of the reviewed ocean shoreline.";
                return true;
            }
            catch (...)
            {
                editor.projectStatus = "The shoreline search could not be scheduled. The scene is unchanged.";
                return false;
            }
        }

        [[nodiscard]] bool poll_model_shoreline_import(EditorState& editor, AiChat& chat)
        {
            if (!editor.modelShorelinePending
                || editor.modelShorelinePending->wait_for(0ms) != std::future_status::ready) return false;
            auto completion = std::move(*editor.modelShorelinePending);
            editor.modelShorelinePending.reset();
            editor.modelShorelineCancellation = {};
            if (!editor.modelImportReview || !editor.modelImportReview->shoreline) return false;
            bool placed = false;
            try
            {
                const auto result = completion.get();
                if (!model_shoreline_review_current(editor))
                    editor.projectStatus = "The scene, model or terrain changed during the shoreline search. Request a fresh review.";
                else if (result.code != project_planetary::SurfaceCode::ready)
                    editor.projectStatus = result.operation.message.empty() ? "No verified dry shoreline was returned." : result.operation.message;
                else
                {
                    const auto& review = *editor.modelImportReview;
                    const auto& target = *review.shoreline;
                    const auto pose = bool(target.forest) == bool(result.playerStart)
                        ? model_surface_pose(review.compiled, target.transform, target.query, result.land, target.terrain,
                            result.playerStart ? &*result.playerStart : nullptr) : std::nullopt;
                    if (pose)
                    {
                        const auto body = target.query.body.id;
                        placed = approve_project_model_import(editor, &*pose);
                        if (placed)
                            editor.projectStatus += epochengine::format_text(" Body {} at ({}, {}, {}) m.",
                                body, pose->pivot[0], pose->pivot[1], pose->pivot[2]);
                    }
                    else editor.projectStatus = "The returned land sample could not support this model. The scene is unchanged.";
                }
            }
            catch (...)
            {
                editor.projectStatus = "The shoreline search failed. Check Output and request a fresh review.";
            }
            if (editor.modelImportReview) editor.modelImportReview->shoreline.reset();
            editor.aiToolStatus = editor.assetGeneration.status = editor.projectStatus;
            chat.append_status(editor.projectStatus);
            push_editor_log(editor, "[asset] " + editor.projectStatus);
            return placed;
        }

        std::optional<editor_project_textures::TextureCatalogEntry> import_asset_generation_texture(
            EditorState& editor, const std::shared_ptr<core::Context>& ctx)
        {
            auto& state = editor.assetGeneration;
            if (asset_output_review_binding(editor, false, state.status).empty()) return {};
            const auto& reference = state.reference->result;
            const auto added = add_project_image_source(resolve_editor_path(std::filesystem::path{
                std::u8string{editor.projectRoot.begin(),editor.projectRoot.end()}}), reference.output,
                std::string_view{reference.output_sha256});
            state.status = added.diagnostic;
            if (!added) return {};
            editor.selectedProjectFile = display_project_path(added.path);
            editor.selectedAssetPath = editor.selectedProjectFile;
            auto imported = import_selected_project_texture(editor, ctx, added.generated_texture_key);
            state.status = editor.projectTextureStatus;
            return imported;
        }

        void dispatch_asset_generation(EditorState& editor,const std::shared_ptr<core::Context>& ctx,
            editor_asset_generation::Action action,bool foliageCutout = false)
        {
            namespace generation=editor_asset_generation;
            auto& state=editor.assetGeneration;
            if(action==generation::Action::none)return;
            if(action==generation::Action::cancel)
            {
                if(editor.taskScheduler&&editor.assetGenerationCancellation.valid())
                    (void)editor.taskScheduler->cancel(editor.assetGenerationCancellation);
                state.status="Cancellation requested. Waiting for worker retirement and coding-model restoration.";return;
            }
            if(editor.assetGenerationPending){state.status="Wait for the current owned generation task to settle.";return;}
            if(action==generation::Action::copy_image_path||action==generation::Action::copy_model_path)
            {
                const auto& output=action==generation::Action::copy_image_path?state.reference:state.generated_model;
                if(output&&gui::set_clipboard_text(display_project_path(output->result.output)))
                    state.status="Output location copied. The original remains in its working folder.";
                else state.status="No output location could be copied.";
                return;
            }
            if(action==generation::Action::save_image||action==generation::Action::save_model)
            {
                const bool image=action==generation::Action::save_image;
                const auto& output=image?state.reference:state.generated_model;
                if(!output){state.status="Generate or open the output before saving a copy.";return;}
                const auto selected=platform::filesystem::save_file_dialog(image?L"Save Generated Image":L"Save Generated Model",
                    image?L"PNG image":L"GLB model",image?L"*.png":L"*.glb",image?L"generated.png":L"generated.glb",
                    image?L"png":L"glb",core::path::executable_dir());
                if(selected.accepted)
                {
                    try
                    {
                    generation::Completion source;source.result=output->result;
                    const auto project=asset_generation_project(editor);
                    state.phase->store(generation::Stage::exporting_output);
                    auto scheduled=schedule_editor_cancellable_task(editor,"Save generated output copy",
                        [source=std::move(source),path=selected.path,project](std::stop_token stop)
                        {
                            generation::Completion completed;completed.project=project;completed.output_copy=true;
                            (void)generation::save_output_copy(source,path,completed.result.diagnostic,stop);return completed;
                        });
                    editor.assetGenerationCancellation=scheduled.cancellation;
                    editor.assetGenerationPending=std::move(scheduled.completion);
                    state.status="Saving a verified copy. The working output and approvals remain unchanged.";
                    }
                    catch(...)
                    {state.phase->store(generation::Stage::idle);state.status="The output-save task could not start. The working image and existing files were preserved.";}
                }
                else if(!selected.cancelled)state.status=selected.diagnostic;
                if(selected.accepted||!selected.cancelled)push_editor_log(editor,"[asset-generation] "+state.status);
                return;
            }
            if(action==generation::Action::save_draft)
            {
                const auto selected=platform::filesystem::save_file_dialog(L"Save Generation Draft",
                    L"Epoch generation draft",L"*.epoch-generation",L"generation.epoch-generation",L"epoch-generation",core::path::executable_dir());
                if(selected.accepted)(void)generation::save_draft(state,selected.path,state.status);
                else if(!selected.cancelled)state.status=selected.diagnostic;
                return;
            }
            if(action==generation::Action::load_draft)
            {
                const auto selected=platform::filesystem::open_file_dialog(L"Load Generation Draft",
                    L"Epoch generation draft",L"*.epoch-generation",core::path::executable_dir());
                if(selected.accepted)
                {
                    std::string diagnostic;
                    if(generation::load_draft(state,selected.path,diagnostic))editor.assetGenerationFocusPending=true;
                    state.status=std::move(diagnostic);
                }
                else if(!selected.cancelled)state.status=selected.diagnostic;
                return;
            }
            if(action==generation::Action::load_image_settings||action==generation::Action::load_model_settings)
            {
                const auto selected=platform::filesystem::open_file_dialog(L"Load pinned local generation backend settings",
                    L"Epoch asset job settings",L"*.json",core::path::executable_dir());
                if(selected.accepted)
                {
                    if(generation::load_settings(state,selected.path,
                        action==generation::Action::load_image_settings?ai::asset_jobs::Action::image:ai::asset_jobs::Action::model))
                        remember_asset_generation_backend_selection(editor);
                }
                else if(!selected.cancelled)state.status=selected.diagnostic;
                if(selected.accepted||!selected.cancelled)push_editor_log(editor,"[asset-generation] "+state.status);
                return;
            }
            if(action==generation::Action::approve_reference)
            {(void)generation::approve_reference(state,state.status);return;}
            if(action==generation::Action::import_texture)
            {
                (void)import_asset_generation_texture(editor, ctx);
                return;
            }
            if(action==generation::Action::review_model||action==generation::Action::import_model)
            {
                if(action==generation::Action::review_model)
                    (void)review_generated_project_model(editor);
                else (void)import_reviewed_generated_model(editor);
                state.status=editor.projectStatus;return;
            }
            try
            {
                const auto project=asset_generation_project(editor);
                if(action==generation::Action::open_completed_generation)
                {
                    if(state.phase->load()!=generation::Stage::idle)
                    {state.status="Wait for generation, restoration or output inspection to settle first.";return;}
                    const auto selected=platform::filesystem::open_file_dialog(L"Open Completed Generation",
                        L"Completed generation result",L"result.json",core::path::executable_dir());
                    if(!selected.accepted){if(!selected.cancelled)state.status=selected.diagnostic;return;}
                    state.phase->store(generation::Stage::restoring_outputs);
                    auto scheduled=schedule_editor_cancellable_task(editor,"Review completed asset generation",
                        [path=selected.path,project](std::stop_token stop){return generation::inspect_completed_generation(path,project,stop);});
                    editor.assetGenerationCancellation=scheduled.cancellation;
                    editor.assetGenerationPending=std::move(scheduled.completion);return;
                }
                if(action==generation::Action::choose_reference)
                {
                    const auto selected=platform::filesystem::open_file_dialog(L"Choose the 3D reference to review",L"PNG reference",L"*.png",core::path::executable_dir());
                    if(!selected.accepted){if(!selected.cancelled)state.status=selected.diagnostic;return;}
                    state.phase->store(generation::Stage::inspecting_reference);
                    auto scheduled=schedule_editor_cancellable_task(editor,"Review asset reference PNG",
                        [path=selected.path,project](std::stop_token stop){return generation::inspect_reference(path,project,stop);});
                    editor.assetGenerationCancellation=scheduled.cancellation;
                    editor.assetGenerationPending=std::move(scheduled.completion);return;
                }
                auto root=core::path::candidate_data_root();if(root.empty())root=core::path::executable_dir();
                if(root.empty()||!std::filesystem::is_directory(root))throw std::runtime_error{"Local generation output root is unavailable."};
                for(const auto part:{"cache","asset-generation"})
                {
                    root/=part;std::error_code error;
                    if(!std::filesystem::exists(root,error))std::filesystem::create_directory(root,error);
                    if(error||std::filesystem::is_symlink(std::filesystem::symlink_status(root))||!std::filesystem::is_directory(root))
                        throw std::runtime_error{"Generation output folder is unavailable or redirected."};
                }
                const auto name=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                const auto kind=action==generation::Action::generate_image?ai::asset_jobs::Action::image:ai::asset_jobs::Action::model;
                auto job=generation::prepare_job(state,kind,std::filesystem::absolute(root/name).lexically_normal(),state.status);
                if(!job){push_editor_log(editor,"[asset-generation] "+state.status);return;}
                const auto phase=state.phase;phase->store(generation::Stage::waiting_for_models);
                const auto workDirectory=job->work_directory;
                auto scheduled=schedule_editor_cancellable_task(editor,kind==ai::asset_jobs::Action::image?"Generate 2D asset":"Generate 3D asset",
                    [job=std::move(*job),project,phase,foliageCutout](std::stop_token stop)mutable
                    {return generation::execute(std::move(job),project,phase,stop,foliageCutout);});
                editor.assetGenerationCancellation=scheduled.cancellation;
                editor.assetGenerationPending=std::move(scheduled.completion);
                state.active_work_directory=workDirectory;
                state.status="Queued. Existing coding jobs are protected; generation waits for an owned model handoff.";
                push_editor_log(editor,"[asset-generation] "+state.status);
            }
            catch(const std::exception& error){state.phase->store(generation::Stage::idle);state.status=error.what();push_editor_log(editor,"[asset-generation] "+state.status);}
            catch(...){state.phase->store(generation::Stage::idle);state.status="Could not schedule the local asset stage.";push_editor_log(editor,"[asset-generation] "+state.status);}
        }
