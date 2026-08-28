/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <filesystem>

export module core.path;

export namespace epochengine::core::path
{
    using path = std::filesystem::path;

    // Absolute path to the current executable (best-effort).
    path executable_path();

    // Directory containing the current executable (best-effort).
    path executable_dir();

    // True when the candidate looks like the Epoch repo root.
    bool is_epoch_repo_root(const path& candidate);

    // Walk upward from the given starting point to find the Epoch repo root.
    // Returns an empty path when no repo root can be confirmed.
    path find_epoch_repo_root(const path& start);

    // Canonical editor runtime workspace used by the repo-local EpochEditor build.
    // Falls back to a sibling executable workspace for packaged/runtime installs.
    path example_console_workspace_dir();

    // Canonical repo/install runtime root derived from executable location.
    // For repo-local runs this resolves to the repo root. For packaged/runtime
    // installs it resolves to the nearest asset-bearing executable root.
    path runtime_root_dir();

    // Shared engine asset root.
    path engine_asset_dir();

    // Editor/runtime asset root used by the EpochEditor shell.
    path example_asset_dir();

    // Preferred log root derived from the executable/runtime location.
    path log_output_dir();

    // Preferred capture root. Honors explicit override first.
    path capture_output_dir();

    // Best-effort engine include root for embedded/project compilation helpers.
    path engine_include_dir();

    // Normalizes a path (lexically). Does not hit the filesystem.
    path normalize(const path& p);

    // Joins two paths (p / child) and normalizes.
    path join(const path& p, const path& child);
}
