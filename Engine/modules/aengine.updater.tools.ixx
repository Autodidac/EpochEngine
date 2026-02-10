module;

export module aengine.updater.tools;

import <cstdlib>;
import <fstream>;
import <iostream>;
import <string>;
import <string_view>;
import <thread>;
import <chrono>;

import aengine.updater.config;

export namespace almondnamespace::updater
{
    // ─────────────────────────────────────────────
    // Download file
    // ─────────────────────────────────────────────
    export bool download_file(
        const std::string& url,
        const std::string& output_path)
    {
        std::cout << "[INFO] Downloading: "
            << url << " -> " << output_path << '\n';

#if defined(_WIN32)
        const std::string command =
            "curl -L --fail --silent --show-error -o \""
            + output_path + "\" \"" + url + "\"";
#else
        const std::string command =
            "wget --quiet --show-progress --output-document="
            + output_path + " " + url;
#endif

        const int result = std::system(command.c_str());

        std::ifstream check(output_path,
            std::ios::binary | std::ios::ate);

        if (result != 0 || !check || check.tellg() <= 1)
        {
            std::cerr << "[ERROR] Download failed: "
                << output_path << '\n';
            std::remove(output_path.c_str());
            return false;
        }

        return true;
    }

    // ─────────────────────────────────────────────
    // 7-Zip availability
    // ─────────────────────────────────────────────
    export bool is_7z_available()
    {
#if defined(_WIN32)
        return std::ifstream(SEVEN_ZIP_LOCAL_BINARY()).good();
#else
        return std::system("which 7z > /dev/null 2>&1") == 0;
#endif
    }

    // ─────────────────────────────────────────────
    // Ensure 7-Zip
    // ─────────────────────────────────────────────
    export bool setup_7zip()
    {
        if (is_7z_available())
            return true;

#if defined(_WIN32)
        if (!download_file(SEVEN_ZIP_EXE_URL(), "7z_installer.exe"))
            return false;

        if (std::system(
            "cmd.exe /C start /wait 7z_installer.exe /S "
            "/D=\"C:\\Program Files\\7-Zip\"") != 0)
            return false;

        return is_7z_available();
#else
        return std::system(SEVEN_ZIP_INSTALL_CMD().c_str()) == 0;
#endif
    }

    // ─────────────────────────────────────────────
    // Extract archive
    // ─────────────────────────────────────────────
    export bool extract_archive(
        const std::string& archive,
        const std::string& destination = ".")
    {
        if (!setup_7zip())
            return false;

#if defined(_WIN32)
        const std::string cmd =
            "\"\"" + SEVEN_ZIP_LOCAL_BINARY()
            + "\" x \"" + archive
            + "\" -o\"" + destination + "\" -y\"";
#else
        const std::string cmd =
            "7z x \"" + archive
            + "\" -o\"" + destination + "\" -y";
#endif
        return std::system(cmd.c_str()) == 0;
    }

    // ─────────────────────────────────────────────
    // LLVM / Clang
    // ─────────────────────────────────────────────
    export bool setup_llvm_clang()
    {
        std::string clang = LLVM_BIN_PATH() + "/clang++";
#if defined(_WIN32)
        clang += ".exe";
#endif

        if (std::ifstream(clang))
            return true;

        if (!download_file(LLVM_SOURCE_URL(), "llvm.zip"))
            return false;

        if (!extract_archive("llvm.zip", "llvm"))
            return false;

        std::remove("llvm.zip");

#if defined(_WIN32)
        return std::system(
            "cd llvm && cmake -G \"Visual Studio 17 2022\" "
            "-A x64 . && cmake --build . --config Release") == 0;
#else
        return std::system(
            "cd llvm && cmake -G Ninja . && ninja") == 0;
#endif
    }

    // ─────────────────────────────────────────────
    // Ninja
    // ─────────────────────────────────────────────
    export bool setup_ninja()
    {
#if defined(_WIN32)
        if (std::ifstream("ninja.exe"))
            return true;
#else
        if (std::ifstream("ninja"))
            return true;
#endif

        return download_file(NINJA_ZIP_URL(), "ninja.zip")
            && extract_archive("ninja.zip");
    }

    // ─────────────────────────────────────────────
    // Generate build file (compat)
    // ─────────────────────────────────────────────
    export bool generate_ninja_build_file()
    {
        std::ofstream f("build.ninja");
        if (!f)
            return false;

        f << "rule noop\n  command = echo build\n"
            << "build all: noop\n";
        return true;
    }

    // ─────────────────────────────────────────────
    // Cleanup
    // ─────────────────────────────────────────────
    export void clean_up_build_files()
    {
#if defined(_WIN32)
        std::system(
            "del /F /Q build.ninja llvm.zip ninja.zip "
            "7z_installer.exe >nul 2>&1");
#else
        std::system(
            "rm -rf build.ninja llvm.zip ninja.zip");
#endif
    }
}
