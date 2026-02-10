module;

export module aengine.updater.system;

import <filesystem>;
import <regex>;
import <iostream>;
import <system_error>;
import <vector>;
import <array>;
import <fstream>;
import <string>;

import aengine.updater.tools;
import aengine.updater.config;

export namespace almondnamespace::updater
{
    // ─────────────────────────────────────────────
    // Results / channels
    // ─────────────────────────────────────────────
    export struct UpdateCommandResult
    {
        bool update_available{ false };
        bool force_required{ false };
        bool update_performed{ false };
    };

    export struct UpdateChannel
    {
        std::string version_url;
        std::string binary_url;
    };

    // ─────────────────────────────────────────────
    // Cleanup
    // ─────────────────────────────────────────────
    export void cleanup_previous_update_artifacts()
    {
        namespace fs = std::filesystem;

        if (!almondnamespace::updater::LEAVE_NO_FILES_ALWAYS_REDOWNLOAD)
            return;

        std::vector<fs::path> targets;

#if defined(_WIN32)
        targets.emplace_back("replace_updater.bat");
#else
        targets.emplace_back("replace_and_restart.sh");
#endif
        targets.emplace_back(
            almondnamespace::updater::REPO + "-main");

        for (const auto& t : targets)
        {
            std::error_code ec;
            fs::remove_all(t, ec);
        }
    }

    // ─────────────────────────────────────────────
    // Binary replacement
    // ─────────────────────────────────────────────
    void replace_binary_from_script(const std::string& new_binary)
    {
#if defined(_WIN32)
        std::ofstream bat("replace_updater.bat");
        bat <<
            "@echo off\n"
            "timeout /t 2 >nul\n"
            "taskkill /IM updater.exe /F >nul 2>&1\n"
            "del updater.exe >nul 2>&1\n"
            "rename \"" << new_binary << "\" updater.exe\n"
            "start updater.exe\n";
        bat.close();

        std::system("start /min replace_updater.bat");
        std::exit(0);
#else
        std::ofstream sh("replace_and_restart.sh");
        sh <<
            "#!/bin/sh\n"
            "sleep 2\n"
            "pkill updater\n"
            "mv \"" << new_binary << "\" updater\n"
            "chmod +x updater\n"
            "./updater &\n";
        sh.close();

        std::system("chmod +x replace_and_restart.sh && ./replace_and_restart.sh &");
        std::exit(0);
#endif
    }

    void replace_binary(const std::string& new_binary)
    {
        almondnamespace::updater::clean_up_build_files();
        replace_binary_from_script(new_binary);
    }

    // ─────────────────────────────────────────────
    // Version check (MODULE-SAFE)
    // ─────────────────────────────────────────────
    bool check_for_updates(const std::string& url)
    {
        constexpr char tmp[] = "remote_version.txt";

        if (!almondnamespace::updater::download_file(url, tmp))
            return false;

        std::ifstream in(tmp);
        std::string latest;
        std::getline(in, latest);
        in.close();
        std::filesystem::remove(tmp);

        std::cout << "[INFO] Local  : "
            << almondnamespace::updater::PROJECT_VERSION << '\n';
        std::cout << "[INFO] Remote : " << latest << '\n';

        return latest != almondnamespace::updater::PROJECT_VERSION;
    }

    // ─────────────────────────────────────────────
    // Installation paths
    // ─────────────────────────────────────────────
    void install_from_binary(const std::string& url)
    {
        const auto bin = almondnamespace::updater::OUTPUT_BINARY();

        if (!almondnamespace::updater::download_file(url, bin))
            return;

        replace_binary(bin);
    }

    // ─────────────────────────────────────────────
    // Entry point
    // ─────────────────────────────────────────────
    export UpdateCommandResult run_update_command(
        const UpdateChannel& channel,
        bool force)
    {
        cleanup_previous_update_artifacts();

        UpdateCommandResult r{};

        if (!check_for_updates(channel.version_url))
            return r;

        r.update_available = true;

        if (!force)
        {
            r.force_required = true;
            return r;
        }

        install_from_binary(channel.binary_url);
        r.update_performed = true;
        return r;
    }
}
