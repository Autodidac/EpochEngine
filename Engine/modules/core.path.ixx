module;

#include <filesystem>

export module core.path;

export namespace epoch::core::path
{
    using path = std::filesystem::path;

    // Absolute path to the current executable (best-effort).
    path executable_path();

    // Directory containing the current executable (best-effort).
    path executable_dir();

    // Normalizes a path (lexically). Does not hit the filesystem.
    path normalize(const path& p);

    // Joins two paths (p / child) and normalizes.
    path join(const path& p, const path& child);
}
