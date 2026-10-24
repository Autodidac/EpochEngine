    struct OwnedProjectProfile
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string id{};
        std::string display_name{};
        std::string root_path{};
        std::string scene_path{};
        std::string tilemap_path{};
        std::string input_profile_path{};
        std::string sprite_animation_path{};
        std::string audio_profile_path{};
        std::string gui_path{};
        std::string world_name{};
        std::string runtime_scene_id{};
        std::string manifest_path{};
        std::string template_family{};
        std::string default_script{};
        std::string description{};
        std::string engine_integration_mode{};
        std::string public_include_root{};
        std::string demo_model_asset{};
        epochengine::EditorProjectCapabilityPolicy renderer_capability{};

        [[nodiscard]] EditorProjectProfile view() const noexcept
        {
            return {
                .kind = kind,
                .id = id,
                .display_name = display_name,
                .root_path = root_path,
                .scene_path = scene_path,
                .tilemap_path = tilemap_path,
                .input_profile_path = input_profile_path,
                .sprite_animation_path = sprite_animation_path,
                .audio_profile_path = audio_profile_path,
                .gui_path = gui_path,
                .world_name = world_name,
                .runtime_scene_id = runtime_scene_id,
                .manifest_path = manifest_path,
                .template_family = template_family,
                .default_script = default_script,
                .description = description,
                .engine_integration_mode = engine_integration_mode,
                .public_include_root = public_include_root,
                .demo_model_asset = demo_model_asset,
                .renderer_capability = renderer_capability
            };
        }
    };

    [[nodiscard]] static std::vector<OwnedProjectProfile>& discovered_project_profiles()
    {
        static std::vector<OwnedProjectProfile> storage{};
        return storage;
    }

    [[nodiscard]] static std::vector<OwnedProjectProfile>& admitted_project_profiles()
    {
        static std::vector<OwnedProjectProfile> storage{};
        return storage;
    }

    [[nodiscard]] static std::vector<EditorProjectProfile>& cached_project_profiles()
    {
        static std::vector<EditorProjectProfile> profiles{};
        return profiles;
    }

    [[nodiscard]] static bool& project_profiles_cache_dirty() noexcept
    {
        static bool dirty = true;
        return dirty;
    }

    static void invalidate_project_profile_cache() noexcept
    {
        project_profiles_cache_dirty() = true;
    }

    [[nodiscard]] static std::string read_text_file(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};

        return std::string(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>());
    }

    [[nodiscard]] static std::string read_env_var(std::string_view name)
    {
#if defined(_WIN32)
        char* raw = nullptr;
        std::size_t rawSize = 0;
        const std::string key{ name };
        if (_dupenv_s(&raw, &rawSize, key.c_str()) != 0 || raw == nullptr)
            return {};

        std::string value{ raw };
        std::free(raw);
        return value;
#else
        if (const char* const value = std::getenv(std::string(name).c_str()))
            return std::string{ value };
        return {};
#endif
    }

    [[nodiscard]] static bool is_epoch_repo_root(const fs::path& candidate) noexcept
    {
        std::error_code ec;
        return fs::exists(candidate / "Engine" / "include" / "epoch.engine.hpp", ec)
            && !ec
            && fs::exists(candidate / "Engine" / "examples" / "EpochEngine" / "EpochEngine.vcxproj", ec)
            && !ec;
    }

    [[nodiscard]] static std::optional<fs::path> ascend_to_repo_root(fs::path start)
    {
        std::error_code ec;
        if (start.empty())
            return std::nullopt;

        start = fs::absolute(start, ec).lexically_normal();
        if (ec)
            return std::nullopt;

        if (fs::is_regular_file(start, ec))
            start = start.parent_path();

        while (!start.empty())
        {
            if (is_epoch_repo_root(start))
                return start;

            const fs::path parent = start.parent_path();
            if (parent == start)
                break;
            start = parent;
        }

        return std::nullopt;
    }

    [[nodiscard]] static fs::path resolve_epoch_repo_root(const fs::path& project_root)
    {
        // Private runtime data is a sibling of the candidate source, so
        // ascending from a generated project can reach the original checkout.
        // A candidate must build its own code, never that unrelated ancestor.
        if (!epochengine::core::path::candidate_data_root().empty())
        {
            if (const auto found = ascend_to_repo_root(
                    epochengine::core::path::executable_path()); found)
                return *found;
            return {};
        }
        if (const auto found = epochengine::core::path::find_epoch_repo_root(project_root); !found.empty())
            return found;

        if (const auto found = ascend_to_repo_root(project_root); found)
            return *found;

        if (const auto found = ascend_to_repo_root(
                epochengine::core::path::executable_path()); found)
            return *found;

        std::error_code currentPathError{};
        const fs::path currentPath = fs::current_path(currentPathError);
        if (!currentPathError)
            if (const auto found = ascend_to_repo_root(currentPath); found)
                return *found;

        if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
            return runtimeRoot;

        std::error_code ec;
        return fs::absolute(project_root.empty() ? fs::path{} : project_root, ec).lexically_normal();
    }

    [[nodiscard]] static bool project_path_below(
        const fs::path& path, const fs::path& root, bool allow_root = false)
    {
        if (path.empty() || root.empty() || !path.is_absolute() || !root.is_absolute())
            return false;
        const fs::path relative = path.lexically_relative(root);
        if (relative.empty() || relative.is_absolute())
            return false;
        if (relative == ".")
            return allow_root;
        for (const auto& component : relative)
        {
            const auto text = component.generic_string();
            if (text.empty() || text == "." || text == ".."
                || text.find(':') != std::string::npos
                || std::ranges::any_of(text, [](unsigned char value) { return value < 32u; }))
                return false;
        }
        return true;
    }

    // Candidate project paths are data, never aliases for the source checkout's
    // Projects directory. Keep lexical routing independently testable, and check
    // existing filesystem aliases separately before opening or writing a file.
    [[nodiscard]] static fs::path candidate_project_path_lexical(
        const fs::path& data_root, const fs::path& requested, bool allow_root = false)
    {
        if (data_root.empty() || !data_root.is_absolute() || requested.empty())
            return {};
        for (const auto& component : requested.relative_path())
            if (component == "." || component == "..")
                return {};
        const fs::path projects = (data_root / "Projects").lexically_normal();
        fs::path resolved{};
        if (requested.is_absolute())
            resolved = requested.lexically_normal();
        else
        {
            if (requested.has_root_path() || *requested.begin() != "Projects")
                return {};
            resolved = (data_root / requested).lexically_normal();
        }
        return project_path_below(resolved, projects, allow_root) ? resolved : fs::path{};
    }

    [[nodiscard]] static fs::path candidate_project_path(
        const fs::path& data_root, const fs::path& requested, bool allow_root = false)
    {
        const fs::path routed = candidate_project_path_lexical(data_root, requested, allow_root);
        if (routed.empty())
            return {};
        std::error_code error{};
        const fs::path canonical_data = fs::canonical(data_root, error);
        if (error)
            return {};
        const fs::path canonical_path = fs::weakly_canonical(routed, error);
        if (error || canonical_path != (canonical_data / routed.lexically_relative(data_root)).lexically_normal())
            return {};
        fs::path observed = data_root;
        for (const auto& component : routed.lexically_relative(data_root))
        {
            observed /= component;
            const auto status = fs::symlink_status(observed, error);
            if (error == std::errc::no_such_file_or_directory)
            {
                error.clear();
                continue;
            }
            if (error || fs::is_symlink(status))
                return {};
            if (fs::is_regular_file(status)
                && (fs::hard_link_count(observed, error) != 1u || error))
                return {};
        }
        return routed;
    }

    [[nodiscard]] static fs::path resolve_projects_root(const fs::path& hint = {}) noexcept
    {
        if (const auto data = epochengine::core::path::candidate_data_root(); !data.empty())
            return candidate_project_path(data, "Projects", true);
        return (resolve_epoch_repo_root(hint) / "Projects").lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_project_root_path(const fs::path& project_root) noexcept
    {
        if (project_root.empty())
            return {};

        if (const auto data = epochengine::core::path::candidate_data_root(); !data.empty())
            return candidate_project_path(data, project_root);

        std::error_code ec;
        if (project_root.is_absolute())
            return project_root.lexically_normal();

        return fs::absolute(resolve_epoch_repo_root(project_root) / project_root, ec).lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_repo_relative_path(const fs::path& candidate, const fs::path& hint = {}) noexcept
    {
        if (candidate.empty())
            return {};
        if (const auto data = epochengine::core::path::candidate_data_root();
            !data.empty() && !candidate.is_absolute()
            && *candidate.begin() == "Projects")
            return candidate_project_path(data, candidate);
        if (candidate.is_absolute())
            return candidate.lexically_normal();
        return (resolve_epoch_repo_root(hint) / candidate).lexically_normal();
    }

    [[nodiscard]] static fs::path resolve_project_demo_model_path(const EditorProjectProfile& profile) noexcept
    {
        if (profile.demo_model_asset.empty())
            return {};

        const fs::path declared{ profile.demo_model_asset };
        if (declared.is_absolute())
            return declared.lexically_normal();

        const std::string declaredText = declared.generic_string();
        if (declaredText.starts_with("Engine/") || declaredText.starts_with("Projects/"))
            return resolve_repo_relative_path(declared, profile.root_path);

        const fs::path projectRoot = resolve_project_root_path(fs::path{ profile.root_path });
        if (projectRoot.empty())
            return {};
        const fs::path projectLocal = (projectRoot / declared).lexically_normal();
        std::error_code ec;
        if (fs::exists(projectLocal, ec) && !ec)
            return projectLocal;

        if (const fs::path exampleAssets = epochengine::core::path::example_asset_dir(); !exampleAssets.empty())
        {
            const fs::path exampleRelative = (exampleAssets / declared).lexically_normal();
            if (fs::exists(exampleRelative, ec) && !ec)
                return exampleRelative;
        }

        if (const fs::path engineAssets = epochengine::core::path::engine_asset_dir(); !engineAssets.empty())
        {
            const fs::path engineRelative = (engineAssets / declared).lexically_normal();
            if (fs::exists(engineRelative, ec) && !ec)
                return engineRelative;
        }

        return resolve_repo_relative_path(declared, profile.root_path);
    }

    [[nodiscard]] static std::string to_windows_path(std::string value)
    {
        std::replace(value.begin(), value.end(), '/', '\\');
        return value;
    }

    [[nodiscard]] static std::string xml_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string json_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string cxx_escape(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 16);
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(ch); break;
            }
        }
        return out;
    }

    [[nodiscard]] static std::string powershell_escape_single_quoted(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 8);
        for (const char ch : value)
        {
            if (ch == '\'')
                out += "''";
            else
                out.push_back(ch);
        }
        return out;
    }

    [[nodiscard]] static std::string bash_escape_single_quoted(std::string_view value)
    {
        std::string out{};
        out.reserve(value.size() + 8);
        for (const char ch : value)
        {
            if (ch == '\'')
                out += "'\"'\"'";
            else
                out.push_back(ch);
        }
        return out;
    }

    [[nodiscard]] static std::uint64_t fnv1a64(std::string_view value, std::uint64_t seed = 14695981039346656037ull) noexcept
    {
        std::uint64_t hash = seed;
        for (const unsigned char ch : value)
        {
            hash ^= ch;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    [[nodiscard]] static std::string hex64(std::uint64_t value)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string out(16, '0');
        for (int i = 15; i >= 0; --i)
        {
            out[static_cast<std::size_t>(i)] = digits[value & 0xF];
            value >>= 4;
        }
        return out;
    }

    [[nodiscard]] static std::string deterministic_guid(std::string_view seed)
    {
        const std::string joined = hex64(fnv1a64(seed)) + hex64(fnv1a64(seed, 1099511628211ull));
        return joined.substr(0, 8)
            + "-" + joined.substr(8, 4)
            + "-" + joined.substr(12, 4)
            + "-" + joined.substr(16, 4)
            + "-" + joined.substr(20, 12);
    }

    enum class JsonStringFieldState : std::uint8_t
    {
        missing,
        present,
        malformed
    };

    struct JsonStringFieldResult
    {
        JsonStringFieldState state{ JsonStringFieldState::missing };
        std::string value{};
    };

    [[nodiscard]] static JsonStringFieldResult inspect_json_string_field(
        std::string_view text,
        std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        const std::size_t keyPos = text.find(needle);
        if (keyPos == std::string_view::npos)
            return {};
        if (text.find(needle, keyPos + needle.size()) != std::string_view::npos)
            return { JsonStringFieldState::malformed, {} };

        const std::size_t colonPos = text.find(':', keyPos + needle.size());
        if (colonPos == std::string_view::npos)
            return { JsonStringFieldState::malformed, {} };

        std::size_t firstQuote = colonPos + 1u;
        while (firstQuote < text.size())
        {
            const char c = text[firstQuote];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                break;
            ++firstQuote;
        }
        if (firstQuote >= text.size() || text[firstQuote] != '"')
            return { JsonStringFieldState::malformed, {} };

        std::string value{};
        bool escaped = false;
        for (std::size_t i = firstQuote + 1; i < text.size(); ++i)
        {
            const char c = text[i];
            if (escaped)
            {
                switch (c)
                {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(c); break;
                }
                escaped = false;
                continue;
            }

            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == '"')
                return { JsonStringFieldState::present, std::move(value) };

            value.push_back(c);
        }

        return { JsonStringFieldState::malformed, {} };
    }

    [[nodiscard]] static std::optional<std::string> extract_json_string_field(
        std::string_view text,
        std::string_view key)
    {
        JsonStringFieldResult result = inspect_json_string_field(text, key);
        if (result.state != JsonStringFieldState::present)
            return std::nullopt;
        return std::move(result.value);
    }

    [[nodiscard]] static std::string default_world_name(EditorProjectKind kind)
    {
        if (kind == EditorProjectKind::Editor)
            return "EditorWorkspace";
        return kind == EditorProjectKind::Tool ? "ToolWorkspace" : "PersistentLevel";
    }

    [[nodiscard]] static std::string_view project_manifest_kind(EditorProjectKind kind) noexcept
    {
        switch (kind)
        {
        case EditorProjectKind::Editor: return "editor";
        case EditorProjectKind::Tool: return "tool";
        case EditorProjectKind::EngineDevelopment: return "engine-development-sandbox";
        default: return "game";
        }
    }

    [[nodiscard]] static std::optional<EditorProjectKind> parse_project_manifest_kind(
        std::string_view kind) noexcept
    {
        if (kind == "editor") return EditorProjectKind::Editor;
        if (kind == "tool") return EditorProjectKind::Tool;
        if (kind == "game") return EditorProjectKind::Game;
        if (kind == "engine-development-sandbox" || kind == "engine-self-iteration-sandbox")
            return EditorProjectKind::EngineDevelopment;
        return std::nullopt;
    }

    [[nodiscard]] static std::string parse_world_name(const fs::path& scene_path, EditorProjectKind kind)
    {
        const std::string sceneText = read_text_file(scene_path);
        constexpr std::string_view token = "scene \"";
        const std::size_t tokenPos = sceneText.find(token);
        if (tokenPos != std::string::npos)
        {
            const std::size_t start = tokenPos + token.size();
            const std::size_t end = sceneText.find('"', start);
            if (end != std::string::npos && end > start)
                return sceneText.substr(start, end - start);
        }

        return default_world_name(kind);
    }

    [[nodiscard]] static bool is_builtin_project_id(std::string_view project_id) noexcept
    {
        if (project_id == kEngineDevelopmentCompatibilityProfile.id)
            return true;
        for (const auto& profile : kProjectProfiles)
            if (profile.id == project_id)
                return true;
        return false;
    }

    [[nodiscard]] static bool is_launcher_application_id(
        std::string_view project_id) noexcept
    {
        // Old generated Plant Lab shells may remain on disk. They are ignored
        // because Plant Lab is a launcher-owned authoring application.
        return project_id == "plantlab";
    }

    [[nodiscard]] static std::optional<OwnedProjectProfile>
        parse_manifest_project_profile(
            const fs::path& requested_manifest,
            std::string* diagnostic = nullptr,
            bool* requires_migration = nullptr)
    {
        if (diagnostic)
            diagnostic->clear();
        if (requires_migration)
            *requires_migration = false;
        const auto fail = [&](std::string message)
            -> std::optional<OwnedProjectProfile>
        {
            if (diagnostic)
                *diagnostic = std::move(message);
            return std::nullopt;
        };

        const fs::path candidate_data = epochengine::core::path::candidate_data_root();
        const fs::path manifest_path = candidate_data.empty() ? requested_manifest
            : candidate_project_path(candidate_data, requested_manifest);
        if (manifest_path.empty())
            return fail("Candidate previews can open projects only inside their private Projects directory.");

        const std::string manifestText = read_text_file(manifest_path);
        if (manifestText.empty())
            return fail("The selected project manifest is empty or unreadable.");

        const JsonStringFieldResult formatField =
            inspect_json_string_field(manifestText, "project_format");
        if (formatField.state == JsonStringFieldState::malformed)
            return fail("Project manifest project_format must be one unique JSON string.");
        if (formatField.state == JsonStringFieldState::present
            && formatField.value != kProjectManifestFormat)
        {
            return fail(
                "Unsupported project manifest format '" + formatField.value
                + "'. This Epoch source accepts "
                + std::string{kProjectManifestFormat} + ".");
        }

        const JsonStringFieldResult buildProfileField =
            inspect_json_string_field(manifestText, "build_profile");
        if (buildProfileField.state == JsonStringFieldState::malformed)
            return fail("Project manifest build_profile must be one unique JSON string.");
        if (buildProfileField.state == JsonStringFieldState::present
            && buildProfileField.value != kStaticRuntimeBuildProfile)
        {
            return fail(
                "Unsupported project build profile '" + buildProfileField.value
                + "'. This source accepts "
                + std::string{kStaticRuntimeBuildProfile} + ".");
        }
        if (requires_migration
            && (formatField.state == JsonStringFieldState::missing
                || buildProfileField.state == JsonStringFieldState::missing))
        {
            *requires_migration = true;
        }

        const auto id = extract_json_string_field(manifestText, "id");
        const auto displayName = extract_json_string_field(manifestText, "display_name");
        const auto kindText = extract_json_string_field(manifestText, "kind");
        const auto sceneText = extract_json_string_field(manifestText, "scene");
        if (!id || !displayName || !kindText || !sceneText)
        {
            return fail(
                "Project manifest identity, display_name, kind, and scene must each be one JSON string.");
        }

        OwnedProjectProfile profile{};
        const auto parsedKind = parse_project_manifest_kind(*kindText);
        if (!parsedKind)
        {
            return fail(
                "Project manifest kind must be game, tool, editor, or engine-development-sandbox.");
        }
        profile.kind = *parsedKind;
        if (profile.kind == EditorProjectKind::Editor)
        {
            const auto edition = inspect_json_string_field(manifestText, "engine_edition");
            if (edition.state != JsonStringFieldState::present
                || edition.value != epochengine::updater::distribution::edition_id)
                return fail("Editor project must declare this engine's source edition; cross-edition Editor projects are not admitted.");
        }
        profile.id = *id;
        profile.display_name = *displayName;
        profile.root_path = manifest_path.parent_path().lexically_normal().generic_string();

        fs::path scenePath{ *sceneText };
        if (!candidate_data.empty())
        {
            if (scenePath.is_relative() && !scenePath.empty()
                && *scenePath.begin() != "Projects")
                scenePath = manifest_path.parent_path() / scenePath;
            scenePath = candidate_project_path(candidate_data, scenePath);
            if (scenePath.empty()
                || !project_path_below(scenePath, manifest_path.parent_path()))
                return fail("Candidate project scenes must stay inside the selected private project.");
        }
        if (scenePath.is_relative()
            && !scenePath.empty()
            && !scenePath.generic_string().starts_with("Projects/"))
        {
            scenePath = manifest_path.parent_path() / scenePath;
        }
        scenePath = scenePath.lexically_normal();
        profile.scene_path = scenePath.generic_string();
        profile.tilemap_path = extract_json_string_field(
            manifestText, "tilemap").value_or("");
        if (!candidate_data.empty() && !profile.tilemap_path.empty())
        {
            const fs::path tilemap = candidate_project_path(candidate_data,
                manifest_path.parent_path() / fs::path{profile.tilemap_path});
            if (tilemap.empty() || !project_path_below(tilemap, manifest_path.parent_path()))
                return fail("Candidate tilemap source must stay inside the selected private project.");
        }
        profile.input_profile_path = extract_json_string_field(
            manifestText, "input_profile").value_or("");
        if (!profile.input_profile_path.empty()
            && profile.input_profile_path
                != epochengine::project_input::canonical_source_path)
        {
            return fail(
                "Project manifest input_profile does not use the canonical Assets/Config path.");
        }
        profile.sprite_animation_path = extract_json_string_field(
            manifestText, "sprite_animation").value_or("");
        if (!profile.sprite_animation_path.empty()
            && profile.sprite_animation_path
                != epochengine::project_sprite_animation::canonical_source_path)
        {
            return fail(
                "Project manifest sprite_animation does not use the canonical Assets/Animations path.");
        }
        profile.audio_profile_path = extract_json_string_field(
            manifestText, "audio_profile").value_or("");
        if (!profile.audio_profile_path.empty()
            && profile.audio_profile_path
                != epochengine::project_audio::canonical_source_path)
        {
            return fail(
                "Project manifest audio_profile does not use the canonical Assets/Audio path.");
        }
        profile.gui_path = extract_json_string_field(
            manifestText, "gui").value_or("");
        if (!profile.gui_path.empty()
            && profile.gui_path
                != epochengine::project_gui::canonical_source_path)
        {
            return fail(
                "Project manifest gui does not use the canonical Assets/Gui path.");
        }
        profile.world_name = parse_world_name(scenePath, profile.kind);
        profile.runtime_scene_id = "project:" + profile.id;
        profile.manifest_path = manifest_path.lexically_normal().generic_string();
        profile.template_family = extract_json_string_field(manifestText, "template_family")
            .value_or(profile.kind == EditorProjectKind::EngineDevelopment
                ? "engine-development-sandbox"
                : (profile.kind == EditorProjectKind::Editor ? "editor-project"
                    : (profile.kind == EditorProjectKind::Tool ? "tool-project" : "game-project")));
        profile.default_script = extract_json_string_field(manifestText, "default_script")
            .value_or(profile.kind == EditorProjectKind::EngineDevelopment
                ? "engine_development_harness"
                : (profile.kind == EditorProjectKind::Game ? "project_demo_bootstrap" : "tool_bootstrap"));
        if (!candidate_data.empty())
        {
            const fs::path script = candidate_project_path(candidate_data,
                manifest_path.parent_path() / "scripts" / (profile.default_script + ".ascript.cpp"));
            if (script.empty() || !project_path_below(script, manifest_path.parent_path() / "scripts"))
                return fail("Candidate default script must stay inside the selected private project's scripts directory.");
        }
        profile.engine_integration_mode = extract_json_string_field(manifestText, "engine_integration")
            .value_or(std::string{kStaticRuntimeBuildProfile});
        profile.public_include_root = extract_json_string_field(manifestText, "public_include_root")
            .value_or("Engine/include");
        profile.demo_model_asset = extract_json_string_field(manifestText, "demo_model_asset")
            .value_or("");
        if (!candidate_data.empty() && !profile.demo_model_asset.empty())
        {
            const fs::path declared{profile.demo_model_asset};
            if (declared.is_relative() && *declared.begin() == "Engine")
            {
                // Engine assets stay immutable; they are not redirected into
                // project data. A manifest cannot turn this into another root.
                const fs::path asset = resolve_repo_relative_path(declared, manifest_path.parent_path());
                if (!project_path_below(asset, epochengine::core::path::engine_asset_dir()))
                    return fail("Candidate engine model assets must stay inside the immutable engine asset directory.");
                for (const auto& component : declared)
                    if (component == "." || component == "..")
                        return fail("Candidate model asset paths cannot contain traversal.");
            }
            else
            {
                const fs::path asset = candidate_project_path(candidate_data,
                    declared.is_relative() && *declared.begin() != "Projects"
                        ? manifest_path.parent_path() / declared : declared);
                if (asset.empty() || !project_path_below(asset, manifest_path.parent_path()))
                    return fail("Candidate model assets must belong to the selected private project or immutable engine assets.");
            }
        }
        const JsonStringFieldResult capabilityField =
            inspect_json_string_field(manifestText, "capability_profile");
        if (capabilityField.state == JsonStringFieldState::malformed)
            return fail(
                "Project manifest capability_profile must be one unique JSON string.");
        if (capabilityField.state == JsonStringFieldState::present)
        {
            const auto capabilityPolicy =
                epochengine::editor_project_capability_policy(capabilityField.value);
            if (!capabilityPolicy)
            {
                return fail(
                    "Project manifest capability_profile is not supported by this build.");
            }
            profile.renderer_capability = *capabilityPolicy;
        }
        profile.description =
            "Generated "
            + std::string(profile.kind == EditorProjectKind::EngineDevelopment
                ? "guarded engine development"
                : (profile.kind == EditorProjectKind::Editor ? "editor"
                    : (profile.kind == EditorProjectKind::Tool ? "software/tool" : "game")))
            + " project shell rooted at "
            + profile.root_path
            + " with runtime scene "
            + profile.world_name
            + ".";

        return profile;
    }
    static void rebuild_project_profile_cache()
    {
        if (!project_profiles_cache_dirty())
            return;

        auto& owned = discovered_project_profiles();
        auto& cached = cached_project_profiles();
        owned.clear();
        cached.clear();

        for (const auto& profile : kProjectProfiles)
            cached.push_back(profile);

        for (const auto& profile : admitted_project_profiles())
        {
            if (!is_builtin_project_id(profile.id)
                && !is_launcher_application_id(profile.id)
                && !resolve_project_root_path(fs::path{profile.root_path}).empty())
            {
                owned.push_back(profile);
            }
        }

        if (const std::string requestedManifest = read_env_var("EPOCH_EDITOR_PROJECT_MANIFEST");
            !requestedManifest.empty())
        {
            const auto parsed = parse_manifest_project_profile(fs::path{ requestedManifest });
            if (parsed
                && !is_builtin_project_id(parsed->id)
                && !is_launcher_application_id(parsed->id))
                owned.push_back(*parsed);
        }

        const fs::path projectsRoot = resolve_projects_root();
        std::error_code ec;
        if (!projectsRoot.empty() && fs::exists(projectsRoot, ec) && !ec)
        {
            constexpr auto options = fs::directory_options::skip_permission_denied;
            for (fs::recursive_directory_iterator it(projectsRoot, options, ec), end; !ec && it != end; it.increment(ec))
            {
                if (!it->is_regular_file(ec) || ec)
                    continue;

                const fs::path path = it->path().lexically_normal();
                if (path.filename() != "project.epoch.json")
                    continue;
                if (path.generic_string().find("/Templates/") != std::string::npos)
                    continue;

                const auto parsed = parse_manifest_project_profile(path);
                if (!parsed
                    || is_builtin_project_id(parsed->id)
                    || is_launcher_application_id(parsed->id))
                    continue;
                if (std::any_of(owned.begin(), owned.end(), [&](const OwnedProjectProfile& existing)
                    {
                        return existing.id == parsed->id || existing.manifest_path == parsed->manifest_path;
                    }))
                {
                    continue;
                }

                owned.push_back(*parsed);
            }
        }

        cached.reserve(cached.size() + owned.size());
        for (const auto& profile : owned)
            cached.push_back(profile.view());

        project_profiles_cache_dirty() = false;
    }

    [[nodiscard]] static const std::vector<EditorProjectProfile>& live_project_profiles()
    {
        rebuild_project_profile_cache();
        return cached_project_profiles();
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> sandbox_seed_entities()
    {
        return {
            { "PersistentLevel", "Level", "World", { 0.0f, -0.32f, 0.0f }, {}, { 14.0f, 0.20f, 14.0f } },
            { "GroundPlatform", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 12.0f, 0.5f, 12.0f } },
            { "PlayerStart", "Spawn", "Gameplay", { -2.5f, 0.0f, 2.5f } },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 4.8f, 8.5f }, { -28.0f, 0.0f, 0.0f } },
            { "DirectionalLight", "Light", "Lighting", { 2.0f, 5.5f, 2.0f }, { -48.0f, 35.0f, 0.0f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> platformer_seed_entities()
    {
        return {
            { "PlatformerLevel", "Level", "World" },
            { "GameplayCamera", "Camera", "Gameplay", { 0.0f, 3.0f, 8.0f }, { -18.0f, 0.0f, 0.0f } },
            { "SkyLight", "Light", "Lighting", { 3.0f, 6.0f, 2.0f }, { -25.0f, 35.0f, 0.0f } },
            { "GroundPlane", "StaticMesh", "Gameplay", { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 16.0f, 1.0f, 4.0f } },
            { "PlayerStart", "Spawn", "Gameplay", { -4.0f, 0.0f, 0.0f } },
            { "MovingPlatform_A", "Mover", "Gameplay", { 1.5f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.5f, 0.4f, 1.0f } },
            { "CoinArc", "CollectibleSet", "Gameplay", { 4.0f, 2.5f, 0.0f } }
        };
    }

    [[nodiscard]] EditorSceneSeedEntity arcade_scene_seed(
        const epochengine::render_arcade::ArcadeSceneNodeContract& node)
    {
        return EditorSceneSeedEntity{
            .name = std::string(node.name),
            .type = std::string(node.type),
            .category = "EngineArcade",
            .position = node.position,
            .rotation = { 0.0f, 0.0f, 0.0f },
            .scale = node.scale,
            .visible = true,
            .editor_only = false
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> engine_arcade_seed_entities()
    {
        return {
            { "PersistentLevel", "Level", "World", { 0.0f, -0.32f, 0.0f }, {}, { 12.0f, 0.20f, 12.0f } },
            { "GroundPlatform", "Ground", "World", { 0.0f, -0.25f, 0.0f }, {}, { 10.0f, 0.5f, 10.0f } },
            { "OverviewCamera", "Camera", "Editor", { 0.0f, 4.2f, 7.2f }, { -28.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 0.0f, 5.2f, -1.2f }, { -42.0f, 0.0f, 0.0f } },
            arcade_scene_seed(epochengine::render_arcade::kCabinetBodySceneNode),
            arcade_scene_seed(epochengine::render_arcade::kScreenSceneNode),
            { "PlayerStart", "Spawn", "Gameplay", { 0.0f, 0.0f, -2.4f } }
        };
    }

    [[nodiscard]] std::vector<EditorSceneSeedEntity> software_seed_entities()
    {
        return {
            { "ToolWorkspace", "Level", "World" },
            { "EditorCamera", "Camera", "Editor", { 0.0f, 4.5f, 9.0f }, { -28.0f, 0.0f, 0.0f } },
            { "KeyLight", "Light", "Lighting", { 2.0f, 6.5f, 2.0f }, { -34.0f, 35.0f, 0.0f } },
            { "ToolStart", "Spawn", "Tool", { 0.0f, 0.0f, 0.0f } }
        };
    }

    [[nodiscard]] const EditorScriptProfile* find_script_profile(std::string_view script_id) noexcept
    {
        for (const auto& script : kScriptProfiles)
            if (script.id == script_id)
                return &script;
        return nullptr;
    }

    [[nodiscard]] static std::string project_script_source_path(std::string_view script_name, std::string_view project_root)
    {
        if (project_root.empty())
            return {};

        const fs::path root = resolve_project_root_path(fs::path{project_root});
        if (root.empty())
            return {};
        const fs::path script = root / "scripts" / (std::string{script_name} + ".ascript.cpp");
        if (const fs::path data = epochengine::core::path::candidate_data_root(); !data.empty()
            && (candidate_project_path(data, script).empty()
                || !project_path_below(script, root / "scripts")))
            return {};
        return script.lexically_normal().generic_string();
    }

    [[nodiscard]] static std::string make_project_pcm16_wave(
        float frequency,
        std::uint32_t durationMilliseconds,
        float amplitude,
        bool fadeOut)
    {
        constexpr std::uint32_t sampleRate{48'000u};
        constexpr double tau{6.283185307179586476925286766559};
        const std::uint32_t frames = (sampleRate * durationMilliseconds) / 1'000u;
        std::string bytes{};
        bytes.reserve(44u + static_cast<std::size_t>(frames) * 2u);
        const auto u16 = [&bytes](std::uint16_t value)
        {
            bytes.push_back(static_cast<char>(value));
            bytes.push_back(static_cast<char>(value >> 8u));
        };
        const auto u32 = [&bytes](std::uint32_t value)
        {
            bytes.push_back(static_cast<char>(value));
            bytes.push_back(static_cast<char>(value >> 8u));
            bytes.push_back(static_cast<char>(value >> 16u));
            bytes.push_back(static_cast<char>(value >> 24u));
        };
        bytes.append("RIFF", 4u);
        u32(36u + frames * 2u);
        bytes.append("WAVEfmt ", 8u);
        u32(16u);
        u16(1u);
        u16(1u);
        u32(sampleRate);
        u32(sampleRate * 2u);
        u16(2u);
        u16(16u);
        bytes.append("data", 4u);
        u32(frames * 2u);
        for (std::uint32_t frame{}; frame < frames; ++frame)
        {
            const double progress = frames <= 1u
                ? 1.0
                : static_cast<double>(frame)
                    / static_cast<double>(frames - 1u);
            const double envelope = fadeOut
                ? (1.0 - progress) * (1.0 - progress)
                : 1.0;
            const double sample = std::sin(
                tau * static_cast<double>(frequency)
                * static_cast<double>(frame)
                / static_cast<double>(sampleRate));
            const auto encoded = static_cast<std::int16_t>(std::llround(
                sample * envelope
                * static_cast<double>((std::clamp)(amplitude, 0.0f, 1.0f))
                * 32'767.0));
            u16(static_cast<std::uint16_t>(encoded));
        }
        return bytes;
    }

    [[nodiscard]] static bool write_text_file(const fs::path& path, std::string_view text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(out);
    }

    [[nodiscard]] static fs::path generated_project_entry_source_path(const fs::path& root)
    {
        return root / "source" / "epoch.main.cpp";
    }

    [[nodiscard]] static fs::path generated_project_cmake_lists_path(const fs::path& root)
    {
        return root / "CMakeLists.txt";
    }

    [[nodiscard]] static fs::path generated_project_windows_build_script_path(const fs::path& root)
    {
        return root / "build_project.ps1";
    }

    [[nodiscard]] static fs::path generated_project_linux_build_script_path(const fs::path& root)
    {
        return root / "build_project.sh";
    }

    [[nodiscard]] static std::string generated_project_artifact_stem(const fs::path& root)
    {
        const std::string stem = root.filename().string();
        return stem == "Sandbox" ? "EpochEngine" : stem;
    }

    [[nodiscard]] static fs::path generated_project_windows_vcxproj_path(const fs::path& root)
    {
        return root / (generated_project_artifact_stem(root) + ".vcxproj");
    }

    [[nodiscard]] static fs::path generated_project_build_log_path(const fs::path& root)
    {
#if defined(_WIN32)
        return root / "build" / "logs" / "build-debug-x64.log";
#else
        return root / "build" / "logs" / "build-linux.log";
#endif
    }

    [[nodiscard]] static fs::path generated_project_output_path(const fs::path& root)
    {
#if defined(_WIN32)
        return root / "build" / "bin" / "windows" / "Debug" / "x64" / (generated_project_artifact_stem(root) + ".exe");
#else
        return root / "build" / "bin" / "linux" / "Debug" / "x64" / generated_project_artifact_stem(root);
#endif
    }

    [[nodiscard]] static std::vector<fs::path> generated_project_output_candidates(const fs::path& root)
    {
#if defined(_WIN32)
        const fs::path outputDir = root / "build" / "bin" / "windows" / "Debug" / "x64";
#else
        const fs::path outputDir = root / "build" / "bin" / "linux" / "Debug" / "x64";
#endif
        std::vector<fs::path> candidates;
        const auto add_candidate = [&](std::string name) {
            if (name.empty())
                return;
#if defined(_WIN32)
            if (!name.ends_with(".exe"))
                name += ".exe";
#endif
            const fs::path path = outputDir / name;
            for (const auto& candidate : candidates)
            {
                if (candidate == path)
                    return;
            }
            candidates.push_back(path);
        };

        add_candidate(generated_project_artifact_stem(root));
        add_candidate(root.filename().string());
        add_candidate("EpochEngine");
#if defined(_WIN32)
        add_candidate("EpochEditor");
#else
        add_candidate("epoch");
#endif
        return candidates;
    }

    [[nodiscard]] static fs::path generated_project_existing_output_path(const fs::path& root)
    {
        std::error_code ec;
        for (const auto& candidate : generated_project_output_candidates(root))
        {
            if (fs::exists(candidate, ec) && !ec)
                return candidate;
            ec.clear();
        }
        return generated_project_output_path(root);
    }

    [[nodiscard]] static std::string next_generated_project_name(EditorProjectKind kind)
    {
        const std::string prefix = kind == EditorProjectKind::Editor ? "EditorProject"
            : (kind == EditorProjectKind::Tool ? "ToolProject" : "GameProject");
        const fs::path projectsRoot = resolve_projects_root();
        for (int ordinal = 1; ordinal < 1000; ++ordinal)
        {
            const std::string candidate = prefix + (ordinal < 10 ? "0" : "") + std::to_string(ordinal);
            if (!fs::exists(projectsRoot / candidate))
                return candidate;
        }
        return prefix + "_overflow";
    }

