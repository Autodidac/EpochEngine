module;

export module aengine.updater.config;

// ─────────────────────────────────────────────
// Standard library imports
// ─────────────────────────────────────────────
import <string>;
import <string_view>;

// ─────────────────────────────────────────────
// Engine version module (replacement for aversion.hpp)
// ─────────────────────────────────────────────
import aengine.version;

export namespace almondnamespace::updater
{
    // ─────────────────────────────────────────
    // Installation behavior
    // ─────────────────────────────────────────

    export inline constexpr bool LEAVE_NO_FILES_ALWAYS_REDOWNLOAD = true;

    // ─────────────────────────────────────────
    // Project identity
    // ─────────────────────────────────────────

    export inline std::string OWNER = "Autodidac";
    export inline std::string REPO = "Cpp20_Ultimate_Project_Updater";
    export inline std::string BRANCH = "main";

    export inline std::string PROJECT_VERSION =
        almondnamespace::GetEngineVersionString();

    // ─────────────────────────────────────────
    // Build / output
    // ─────────────────────────────────────────

    export inline std::string OUTPUT_BINARY()
    {
#if defined(_WIN32)
        return "updater_new.exe";
#else
        return "updater_new";
#endif
    }

    export inline std::string SOURCE_MAIN_FILE()
    {
        return "src/update.cpp";
    }

    export inline std::string REPLACE_RUNNING_EXE_SCRIPT_NAME()
    {
#if defined(_WIN32)
        return "replace_updater.bat";
#else
        return "replace_updater.sh";
#endif
    }

    // ─────────────────────────────────────────
    // GitHub base URLs
    // ─────────────────────────────────────────

    export inline std::string GITHUB_BASE()
    {
        return "https://github.com/";
    }

    export inline std::string GITHUB_RAW_BASE()
    {
        return "https://raw.githubusercontent.com/";
    }

    // ─────────────────────────────────────────
    // Project URLs
    // ─────────────────────────────────────────

    export inline std::string PROJECT_VERSION_URL()
    {
        return GITHUB_RAW_BASE()
            + OWNER + "/" + REPO + "/" + BRANCH
            + "/modules/aengine.version.ixx";
    }

    export inline std::string PROJECT_VERSION_HEADER_URL()
    {
        // fallback to legacy aversion module path
        auto url = PROJECT_VERSION_URL();
        constexpr std::string_view needle = "/modules/aengine.version.ixx";

        if (const auto pos = url.rfind(needle); pos != std::string::npos)
            url.replace(pos, needle.size(), "/Modules/aversion.ixx");

        return url;
    }

    export inline std::string PROJECT_SOURCE_URL()
    {
        return GITHUB_BASE()
            + OWNER + "/" + REPO
            + "/archive/refs/heads/" + BRANCH + ".zip";
    }

    export inline std::string PROJECT_BINARY_URL()
    {
        return GITHUB_BASE()
            + OWNER + "/" + REPO
            + "/releases/latest/download/update.exe";
    }

    // ─────────────────────────────────────────
    // LLVM configuration
    // ─────────────────────────────────────────

    export inline std::string LLVM_VERSION = "20.1.0";

#if defined(_WIN32)

    export inline std::string LLVM_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "llvm/llvm-project/archive/refs/tags/"
            + "llvmorg-" + LLVM_VERSION + ".zip";
    }

    export inline std::string LLVM_EXE_URL()
    {
        return GITHUB_BASE()
            + "llvm/llvm-project/releases/download/"
            + "llvmorg-" + LLVM_VERSION
            + "/LLVM-" + LLVM_VERSION + "-win64.exe";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "C:/Program Files/LLVM/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return GITHUB_BASE()
            + "ninja-build/ninja/releases/latest/download/ninja-win.zip";
    }

    export inline std::string NINJA_EXE_URL()
    {
        return GITHUB_BASE()
            + "ninja-build/ninja/releases/latest/download/ninja.exe";
    }

#elif defined(__linux__)

    export inline std::string LLVM_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "llvm/llvm-project/archive/refs/tags/"
            + "llvmorg-" + LLVM_VERSION + "/LLVM-"
            + LLVM_VERSION + "-linux.tar.xz";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "/usr/local/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return GITHUB_BASE()
            + "ninja-build/ninja/releases/latest/download/ninja-linux.zip";
    }

#elif defined(__APPLE__)

    export inline std::string LLVM_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "llvm/llvm-project/archive/refs/tags/"
            + "llvmorg-" + LLVM_VERSION + "/LLVM-"
            + LLVM_VERSION + "-macos.tar.xz";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "/usr/local/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return GITHUB_BASE()
            + "ninja-build/ninja/releases/latest/download/ninja-mac.zip";
    }

#endif

    // ─────────────────────────────────────────
    // 7-Zip configuration
    // ─────────────────────────────────────────

    export inline std::string SEVEN_ZIP_VERSION = "24.09";
    export inline std::string SEVEN_ZIP_VERSION_NAMETAG = "2409";

#if defined(_WIN32)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "ip7z/7zip/archive/refs/tags/"
            + SEVEN_ZIP_VERSION + ".zip";
    }

    export inline std::string SEVEN_ZIP_EXE_URL()
    {
        return GITHUB_BASE()
            + "ip7z/7zip/releases/latest/download/"
            + "7z" + SEVEN_ZIP_VERSION_NAMETAG + "-x64.exe";
    }

    export inline std::string SEVEN_ZIP_LOCAL_BINARY()
    {
        return "C:/Program Files/7-Zip/7z.exe";
    }

    export inline std::string SEVEN_ZIP_BINARY = "7z.exe";

#elif defined(__linux__)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "ip7z/7zip/archive/refs/tags/"
            + SEVEN_ZIP_VERSION
            + "/7z" + SEVEN_ZIP_VERSION_NAMETAG
            + "-linux-x64.tar.xz";
    }

    export inline std::string SEVEN_ZIP_LOCAL_BINARY()
    {
        return "/usr/bin/7z";
    }

    export inline std::string SEVEN_ZIP_BINARY = "7z";

    export inline std::string SEVEN_ZIP_INSTALL_CMD()
    {
        return "sudo apt-get install -y p7zip-full";
    }

#elif defined(__APPLE__)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return GITHUB_BASE()
            + "ip7z/7zip/releases/latest/download/"
            + SEVEN_ZIP_VERSION + "/p7zip-mac.tar.gz";
    }

    export inline std::string SEVEN_ZIP_BINARY = "7z";

    export inline std::string SEVEN_ZIP_INSTALL_CMD()
    {
        return "brew install p7zip";
    }

#endif
}
