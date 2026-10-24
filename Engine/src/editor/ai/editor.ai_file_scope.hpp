#pragma once

#include <filesystem>
#include <system_error>

namespace epochengine::editor_ai_file_scope
{
    [[nodiscard]] inline bool regular_file(const std::filesystem::path& path,
        std::error_code& error)
    {
        const auto status = std::filesystem::status(path, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        return !error && std::filesystem::is_regular_file(status);
    }
    // Filesystem admission policy, not OS confinement. The caller retains image
    // bytes only after validating their actual image format separately.
    [[nodiscard]] inline bool engine_tree(const std::filesystem::path& root,
        std::error_code& error)
    {
        if (regular_file(root / "Engine.sln", error)) return true;
        if (error) return false;
        const bool module = regular_file(
            root / "Engine/modules/epoch.version.ixx", error);
        if (error || !module) return false;
        return regular_file(root / "Engine/include/epoch.edition.hpp", error);
    }

    [[nodiscard]] inline bool contains(const std::filesystem::path& parent,
        const std::filesystem::path& child)
    {
        const auto relative = child.lexically_relative(parent);
        return !relative.empty() && *relative.begin() != "..";
    }

    [[nodiscard]] inline bool allows_text(const std::filesystem::path& path,
        const std::filesystem::path& knownEngineRoot = {})
    {
        std::error_code error;
        const auto resolved = std::filesystem::weakly_canonical(path, error);
        if (error || resolved.empty() || !resolved.is_absolute()) return false;
        const auto known = knownEngineRoot.empty() ? std::filesystem::path{}
            : std::filesystem::weakly_canonical(knownEngineRoot, error);
        if (error) return false;
        const bool directory = std::filesystem::is_directory(resolved, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return false;
        if (!directory && regular_file(resolved, error))
        {
            // Canonical paths cannot reveal a hard link's other names. Public
            // text inputs must have one filesystem identity, including generic
            // references outside a recognized engine tree. A copied project
            // file remains usable; this is admission, not OS confinement.
            const auto links = std::filesystem::hard_link_count(resolved, error);
            if (error || links != 1u) return false;
        }
        if (error) return false;
        const auto start = directory ? resolved : resolved.parent_path();
        std::filesystem::path manifestRoot;
        for (auto cursor = start; !cursor.empty(); cursor = cursor.parent_path())
        {
            const bool engine = (!known.empty() && cursor == known) || engine_tree(cursor, error);
            if (error) return false;
            if (engine)
            {
                // A normal game project stored inside the owner checkout is
                // permitted. A nested engine copy is encountered first and denied.
                return !manifestRoot.empty() && contains(cursor / "Projects", manifestRoot);
            }
            if (manifestRoot.empty() && regular_file(
                    cursor / "project.epoch.json", error)) manifestRoot = cursor;
            if (error) return false;
            if (cursor == cursor.parent_path()) break;
        }
        return true; // Explicit generic files outside recognized engine trees.
    }
}
