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

    // Explicit startup-only placement for a candidate's mutable state. Empty
    // for ordinary launches. This is not an OS access/confinement boundary.
    path candidate_data_root();

    // Binds once to an existing absolute, ordinary directory. Never creates
    // anything; rejects ambiguous/reparse paths and every subsequent bind.
    [[nodiscard]] bool configure_candidate_data_root(const path& root) noexcept;

    struct CandidateDataArguments final
    {
        bool ok{};
        int option_index{};
    };

    // Run before logging or ordinary CLI parsing. Accepts one exact option
    // and value, binds it, and returns its argv index for removing both entries.
    // An absent option succeeds with index zero. Windows uses native wide argv.
    [[nodiscard]] CandidateDataArguments preflight_candidate_data_arguments(
        int argc, char** argv) noexcept;

    // Absolute path to the current executable (best-effort).
    path executable_path();

    // Directory containing the current executable (best-effort).
    path executable_dir();

    // True when the candidate looks like the Epoch repo root.
    bool is_epoch_repo_root(const path& candidate);

    // Walk upward from the given starting point to find the Epoch repo root.
    // Returns an empty path when no repo root can be confirmed.
    path find_epoch_repo_root(const path& start);

    // Candidate-local workspace when bound; otherwise the canonical editor
    // runtime workspace used by the repo-local EpochEditor build.
    // Falls back to a sibling executable workspace for packaged/runtime installs.
    path example_console_workspace_dir();

    // Derives an ordinary host workspace from a known executable identity.
    // Resolves host launch aliases before appending writable workspace paths;
    // never canonicalizes a candidate workspace or grants redirected outputs.
    // Does not inspect or override the separately bound candidate-data root.
    path example_console_workspace_dir(const path& host_executable);

    // Canonical repo/install runtime root derived from executable location.
    // For repo-local runs this resolves to the repo root. For packaged/runtime
    // installs it resolves to the nearest asset-bearing executable root.
    path runtime_root_dir();

    // Shared engine asset root.
    path engine_asset_dir();

    // Editor/runtime asset root used by the EpochEditor shell.
    path example_asset_dir();

    // Candidate-local logs when bound; otherwise executable/runtime-derived.
    path log_output_dir();

    // Candidate-local captures when bound; otherwise honors explicit override.
    path capture_output_dir();

    // Best-effort engine include root for embedded/project compilation helpers.
    path engine_include_dir();

    // Normalizes a path (lexically). Does not hit the filesystem.
    path normalize(const path& p);

    // Joins two paths (p / child) and normalizes.
    path join(const path& p, const path& child);
}
