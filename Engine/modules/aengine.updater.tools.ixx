/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 ***********************************************/
module;

export module aengine.updater.tools;

import <array>;
import <cstdlib>;
import <filesystem>;
import <fstream>;
import <iterator>;
import <source_location>;
import <string>;
import <string_view>;
import <system_error>;

import aengine.updater.config;
import aengine.core.logger;

export namespace epochnamespace::updater
{
    namespace detail
    {
        [[nodiscard]] inline std::string logger_safe(std::string text)
        {
            if (text.size() >= 3
                && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB
                && static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }

            for (char& ch : text)
            {
                const auto uch = static_cast<unsigned char>(ch);
                if (uch < 0x20 || uch == 0x7F)
                    ch = ' ';
            }

            while (!text.empty() && text.back() == ' ')
                text.pop_back();

            return text;
        }

        inline void log_info(const std::string& message)
        {
            logger::get("Updater").log(
                logger::LogLevel::INFO,
                logger_safe(message),
                std::source_location::current());
        }

        inline void log_error(const std::string& message)
        {
            logger::get("Updater").log(
                logger::LogLevel::Error,
                logger_safe(message),
                std::source_location::current());
        }

        [[nodiscard]] inline std::string quote_shell_arg(const std::string& value)
        {
#if defined(_WIN32)
            std::string out;
            out.reserve(value.size() + 2);
            out.push_back('"');
            for (const char c : value)
            {
                if (c == '"')
                    out += "\\\"";
                else
                    out.push_back(c);
            }
            out.push_back('"');
            return out;
#else
            std::string out;
            out.reserve(value.size() + 2);
            out.push_back('"');
            for (const char c : value)
            {
                if (c == '"' || c == '\\' || c == '$' || c == '`')
                    out.push_back('\\');
                out.push_back(c);
            }
            out.push_back('"');
            return out;
#endif
        }

        [[nodiscard]] inline std::string strip_utf8_bom(std::string text)
        {
            if (text.size() >= 3
                && static_cast<unsigned char>(text[0]) == 0xEF
                && static_cast<unsigned char>(text[1]) == 0xBB
                && static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }

            return text;
        }

        [[nodiscard]] inline std::string normalize_line_endings(std::string text)
        {
            std::string out;
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (c == '\r')
                {
                    if ((i + 1) < text.size() && text[i + 1] == '\n')
                        continue;

                    out.push_back('\n');
                    continue;
                }

                out.push_back(c);
            }

            return out;
        }

        [[nodiscard]] inline std::string read_text_file_normalized(const std::filesystem::path& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};

            std::string text{
                std::istreambuf_iterator<char>{ in },
                std::istreambuf_iterator<char>{}
            };

            text = strip_utf8_bom(std::move(text));
            text = normalize_line_endings(std::move(text));
            return text;
        }

        [[nodiscard]] inline bool file_exists_and_nontrivial(
            const std::filesystem::path& path,
            const std::uintmax_t minimum_size = 2)
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || ec)
                return false;

            const auto size = std::filesystem::file_size(path, ec);
            if (ec)
                return false;

            return size >= minimum_size;
        }

        [[nodiscard]] inline std::string powershell_escape_single_quoted(std::string value)
        {
            std::string out;
            out.reserve(value.size() + 8);

            for (const char c : value)
            {
                if (c == '\'')
                    out += "''";
                else
                    out.push_back(c);
            }

            return out;
        }
    }

    export bool download_file(
        const std::string& url,
        const std::string& output_path)
    {
        namespace fs = std::filesystem;

        detail::log_info("Downloading: " + url + " -> " + output_path);

        std::error_code ec;
        fs::remove(fs::path{ output_path }, ec);

#if defined(_WIN32)
        const std::string command =
            "curl -L --fail --silent --show-error "
            "-A \"EpochUpdater\" "
            "-H \"Accept: application/octet-stream, application/vnd.github+json\" "
            "-H \"X-GitHub-Api-Version: 2022-11-28\" "
            "-o " + detail::quote_shell_arg(output_path) + " "
            + detail::quote_shell_arg(url);
#else
        const std::string command =
            "wget --quiet --show-progress "
            "--output-document=" + detail::quote_shell_arg(output_path) + " "
            + detail::quote_shell_arg(url);
#endif

        const int result = std::system(command.c_str());

        if (result != 0 || !detail::file_exists_and_nontrivial(output_path))
        {
            detail::log_error("Download failed: " + output_path);
            fs::remove(fs::path{ output_path }, ec);
            return false;
        }

        return true;
    }

    export bool is_7z_available()
    {
#if defined(_WIN32)
        return detail::file_exists_and_nontrivial(
            std::filesystem::path{ SEVEN_ZIP_LOCAL_BINARY() },
            1);
#else
        return std::system("which 7z > /dev/null 2>&1") == 0;
#endif
    }

    export bool setup_7zip()
    {
        if (is_7z_available())
            return true;

#if defined(_WIN32)
        const std::string installer_path = "7z_installer.exe";

        if (!download_file(SEVEN_ZIP_EXE_URL(), installer_path))
            return false;

        const int result = std::system(
            "cmd.exe /C start /wait \"\" 7z_installer.exe /S /D=\"C:\\Program Files\\7-Zip\"");

        if (result != 0)
        {
            detail::log_error("7-Zip silent install failed.");
            return false;
        }

        return is_7z_available();
#else
        return std::system(SEVEN_ZIP_INSTALL_CMD().c_str()) == 0;
#endif
    }

    export bool extract_archive(
        const std::string& archive,
        const std::string& destination = ".")
    {
        namespace fs = std::filesystem;

        std::error_code ec;
        fs::remove_all(fs::path{ destination }, ec);
        fs::create_directories(fs::path{ destination }, ec);

#if defined(_WIN32)
        const std::string archive_ps = detail::powershell_escape_single_quoted(archive);
        const std::string destination_ps = detail::powershell_escape_single_quoted(destination);

        const std::string cmd =
            "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \""
            "$archive='" + archive_ps + "'; "
            "$dest='" + destination_ps + "'; "
            "if(Test-Path -LiteralPath $dest){Remove-Item -LiteralPath $dest -Recurse -Force}; "
            "New-Item -ItemType Directory -Path $dest -Force | Out-Null; "
            "Expand-Archive -LiteralPath $archive -DestinationPath $dest -Force\"";
#else
        if (!setup_7zip())
            return false;

        const std::string cmd =
            "7z x "
            + detail::quote_shell_arg(archive)
            + " -o" + detail::quote_shell_arg(destination)
            + " -y > /dev/null";
#endif

        if (std::system(cmd.c_str()) != 0)
        {
            detail::log_error("Failed to extract archive: " + archive);
            return false;
        }

        return true;
    }

    export bool setup_llvm_clang()
    {
        const std::filesystem::path llvm_bin = std::filesystem::path{ LLVM_BIN_PATH() };

#if defined(_WIN32)
        const auto clang = llvm_bin / "clang++.exe";
        const std::string archive_name = "llvm.zip";
#else
        const auto clang = llvm_bin / "clang++";
        const std::string archive_name =
            LLVM_SOURCE_URL().ends_with(".tar.gz") ? "llvm.tar.gz" : "llvm.zip";
#endif

        if (detail::file_exists_and_nontrivial(clang, 1))
            return true;

        if (!download_file(LLVM_SOURCE_URL(), archive_name))
            return false;

        if (!extract_archive(archive_name, "llvm"))
            return false;

        std::error_code ec;
        std::filesystem::remove(archive_name, ec);

#if defined(_WIN32)
        const int result = std::system(
            "cmd.exe /C \"cd /d llvm && cmake -G \\\"Visual Studio 17 2022\\\" -A x64 . && "
            "cmake --build . --config Release\"");
#else
        const int result = std::system(
            "sh -c 'cd llvm && cmake -G Ninja . && ninja'");
#endif

        if (result != 0)
        {
            detail::log_error("LLVM/Clang setup failed.");
            return false;
        }

        return detail::file_exists_and_nontrivial(clang, 1);
    }

    export bool setup_ninja()
    {
#if defined(_WIN32)
        if (detail::file_exists_and_nontrivial("ninja.exe", 1))
            return true;
#else
        if (detail::file_exists_and_nontrivial("ninja", 1))
            return true;
#endif

        if (!download_file(NINJA_ZIP_URL(), "ninja.zip"))
            return false;

        if (!extract_archive("ninja.zip"))
            return false;

        return true;
    }

    export bool generate_ninja_build_file()
    {
        std::ofstream file("build.ninja", std::ios::binary);
        if (!file)
            return false;

        file << "rule noop\n"
                "  command = echo build\n"
                "build all: noop\n";

        return static_cast<bool>(file);
    }

    export void clean_up_build_files()
    {
        namespace fs = std::filesystem;

        const std::array<fs::path, 6> paths{
            fs::path{ "build.ninja" },
            fs::path{ "llvm.zip" },
            fs::path{ "llvm.tar.gz" },
            fs::path{ "ninja.zip" },
            fs::path{ "7z_installer.exe" },
            fs::path{ "llvm" }
        };

        for (const auto& path : paths)
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
}
