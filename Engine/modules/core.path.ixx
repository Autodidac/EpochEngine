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

export namespace epoch::core::path
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

    // Canonical example runtime workspace used by the repo-local ConsoleApplication1 build.
    // Falls back to a sibling executable workspace for packaged/runtime installs.
    path example_console_workspace_dir();

    // Normalizes a path (lexically). Does not hit the filesystem.
    path normalize(const path& p);

    // Joins two paths (p / child) and normalizes.
    path join(const path& p, const path& child);
}
