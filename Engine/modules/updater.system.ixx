/*
 * This file is part of the Epoch Project.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

export module updater.system;

export namespace epochengine::updater
{
    struct UpdateCommandResult
    {
        bool operation_failed{ false };
        bool update_available{ false };
        bool packaged_update_available{ false };
        bool force_required{ false };
        bool update_performed{ false };
        bool packaged_update_performed{ false };
        bool packaged_handoff_staged{ false };
        bool source_update_performed{ false };
        bool packaged_release_checked{ false };
        bool packaged_release_found{ false };
        bool packaged_release_missing{ false };
        bool source_fallback_attempted{ false };
        std::string local_version;
        std::string remote_version;
        std::string source_local_version;
        std::string source_remote_version;
        bool source_update_available{ false };
        bool platform_build_checked{ false };
        bool platform_build_ok{ false };
        bool platform_build_pending{ false };
        std::string platform_build_job;
        std::string platform_build_status;
        std::string platform_build_conclusion;
        std::string platform_build_url;
        std::string platform_build_reason;
        std::string packaged_release_reason;
        std::string status_message;
    };

    struct ProjectSourceDownloadResult
    {
        bool ok{ false };
        std::filesystem::path project_root{};
        std::string status_message{};
    };

    enum class SourceAuthorityKind : std::uint8_t
    {
        unavailable,
        explicit_checkout,
        verified_cache
    };

    struct VerifiedSourceAuthority final
    {
        SourceAuthorityKind kind{SourceAuthorityKind::unavailable};
        std::filesystem::path root{};
        std::string source_version{};
        std::string commit{};
        std::string receipt_digest{};
        std::string status_message{};
        bool verified{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return verified;
        }
    };

    enum class UpdateHandoffMode
    {
        LaunchImmediately,
        StageForRestart
    };

    struct UpdateChannel
    {
        std::string version_url;
        std::string binary_url;
        std::string source_url;
        std::string source_version_url;
        std::string platform_build_status_url;
        std::string platform_build_job_name;
    };

    void cleanup_previous_update_artifacts();
    bool update_discovery_contract_self_test();
    ProjectSourceDownloadResult download_project_source_code(const UpdateChannel& channel);
    [[nodiscard]] VerifiedSourceAuthority resolve_verified_source_authority(
        const std::filesystem::path& explicit_checkout = {});
    [[nodiscard]] bool verified_source_authority_contract_self_test();
    std::filesystem::path source_update_log_path();
    std::filesystem::path update_handoff_log_path();
    bool source_update_worker_active();
    int source_update_recent_cancel_seconds_remaining(int cooldown_seconds);
    bool source_update_cancel_requested();
    bool launch_staged_update_handoff();
    bool request_source_update_cancel();
    bool run_source_update_command(
        const UpdateChannel& channel,
        bool recheck_source_version = true,
        bool silent_worker = false,
        bool honor_env_silent = true);
    UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        bool force,
        bool honor_env_silent = true,
        UpdateHandoffMode packaged_handoff_mode = UpdateHandoffMode::LaunchImmediately);
}

namespace epochengine::updater::system_detail
{
    void append_log_line(const std::filesystem::path& log_path, const std::string& line);
    void cleanup_stale_source_tool_runs(const std::filesystem::path& target_binary, const std::filesystem::path& keep_tools_dir);
    void cleanup_stale_source_update_runs(const std::filesystem::path& target_binary, const std::filesystem::path& keep_run_dir);
    std::filesystem::path find_msbuild_path();
    bool launch_powershell_script(const std::filesystem::path& script_path, bool hidden, const std::filesystem::path& log_path);
    void log_error(const std::string& message);
    void log_info(const std::string& message);
    std::string make_source_update_run_token();
    std::filesystem::path make_temp_powershell_script_path(std::string_view stem);
    std::string powershell_escape_single_quoted(std::string_view text);
    void remove_update_cache_path_best_effort(const std::filesystem::path& path);
    std::filesystem::path source_active_run_path(const std::filesystem::path& target_binary);
    std::filesystem::path source_archive_path(const std::filesystem::path& target_binary, std::string_view source_url, std::string_view run_token);
    std::filesystem::path source_cancel_path(const std::filesystem::path& target_binary);
    std::filesystem::path source_final_dir(const std::filesystem::path& target_binary, std::string_view run_token);
    std::filesystem::path source_manifest_root(const std::filesystem::path& source_root);
    std::filesystem::path source_run_dir(const std::filesystem::path& target_binary, std::string_view run_token);
    std::filesystem::path source_run_tools_root(const std::filesystem::path& target_binary, std::string_view run_token);
    std::filesystem::path source_runtime_binary_path(const std::filesystem::path& source_root, const std::filesystem::path& target_binary);
    std::filesystem::path source_runtime_output_dir(const std::filesystem::path& source_root);
    std::filesystem::path source_solution_path(const std::filesystem::path& source_root);
    std::filesystem::path source_staging_dir(const std::filesystem::path& target_binary, std::string_view run_token);
    bool source_update_active_run_exists(const std::filesystem::path& target_binary, std::filesystem::path* active_run_out);
    std::filesystem::path source_update_log_path_for(const std::filesystem::path& target_binary);
    bool source_update_session_active(const std::filesystem::path& target_binary);
    std::filesystem::path source_work_root(const std::filesystem::path& target_binary);
    std::filesystem::path staged_update_handoff_script_path();
    std::filesystem::path update_handoff_log_path_for(const std::filesystem::path& target_binary);
}
