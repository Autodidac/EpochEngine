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

export module aengine.updater.config;

import <string>;
import <string_view>;

import aengine.version;

export namespace epochnamespace::updater
{
    // ─────────────────────────────────────────
    // Installation behavior
    // ─────────────────────────────────────────

    export inline constexpr bool LEAVE_NO_FILES_ALWAYS_REDOWNLOAD = true;

    // ─────────────────────────────────────────
    // Project identity
    // ─────────────────────────────────────────

    export inline constexpr std::string_view OWNER = "Autodidac";
    export inline constexpr std::string_view REPO = "EpochEngine";
    export inline constexpr std::string_view BRANCH = "main";

    export inline const std::string PROJECT_VERSION =
        epochnamespace::GetEngineVersionString();

    // ─────────────────────────────────────────
    // Runtime / source build metadata
    // ─────────────────────────────────────────

    export inline std::string RUNTIME_BINARY_NAME()
    {
#if defined(_WIN32)
        return "ConsoleApplication1.exe";
#else
        return "ConsoleApplication1";
#endif
    }

    export inline std::string SOURCE_SOLUTION_NAME()
    {
        return "Engine.sln";
    }

    export inline std::string SOURCE_BUILD_TARGET()
    {
        return "ConsoleApplication1";
    }

    export inline std::string SOURCE_BUILD_CONFIGURATION()
    {
        return "Release";
    }

    export inline std::string SOURCE_BUILD_PLATFORM()
    {
        return "x64";
    }

    export inline std::string SOURCE_MANIFEST_ROOT_NAME()
    {
        return "Engine";
    }

    export inline std::string SOURCE_VERSION_FILE_NAME()
    {
        return "Engine/modules/aengine.version.ixx";
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

    export inline constexpr std::string_view GITHUB_BASE = "https://github.com/";
    export inline constexpr std::string_view GITHUB_RAW_BASE = "https://raw.githubusercontent.com/";
    export inline constexpr std::string_view GITHUB_API_BASE = "https://api.github.com/repos/";

    // ─────────────────────────────────────────
    // Version / package URLs
    // ─────────────────────────────────────────

    export inline std::string PROJECT_SOURCE_VERSION_URL()
    {
        return std::string{ GITHUB_RAW_BASE }
            + std::string{ OWNER } + "/"
            + std::string{ REPO } + "/"
            + std::string{ BRANCH } + "/Engine/modules/aengine.version.ixx";
    }

    export inline std::string PROJECT_PACKAGED_VERSION_URL()
    {
        return std::string{ GITHUB_BASE }
            + std::string{ OWNER } + "/"
            + std::string{ REPO } + "/releases/latest/download/version.txt";
    }

    export inline std::string PROJECT_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + std::string{ OWNER } + "/"
            + std::string{ REPO }
            + "/archive/refs/heads/"
            + std::string{ BRANCH } + ".zip";
    }

    export inline std::string PROJECT_BINARY_URL()
    {
        return std::string{ GITHUB_BASE }
            + std::string{ OWNER } + "/"
            + std::string{ REPO }
        + "/releases/latest/download/main.zip";
    }

    export inline std::string PROJECT_RELEASE_API_URL()
    {
        return std::string{ GITHUB_API_BASE }
            + std::string{ OWNER } + "/"
            + std::string{ REPO } + "/releases/latest";
    }

    // ─────────────────────────────────────────
    // Managed updater tools
    // ─────────────────────────────────────────

    export inline std::string UPDATER_TOOLS_SUBDIR()
    {
        return "Epoch/t";
    }

    export inline std::string UPDATER_WORK_SUBDIR()
    {
        return "Epoch/u";
    }

    export inline constexpr std::string_view VCPKG_OWNER = "microsoft";
    export inline constexpr std::string_view VCPKG_REPO = "vcpkg";
    export inline constexpr std::string_view VCPKG_DEFAULT_REF = "master";
    export inline constexpr std::string_view GIT_WINDOWS_OWNER = "git-for-windows";
    export inline constexpr std::string_view GIT_WINDOWS_REPO = "git";

    export inline std::string VCPKG_ARCHIVE_BASE_URL()
    {
        return std::string{ GITHUB_BASE }
            + std::string{ VCPKG_OWNER } + "/"
            + std::string{ VCPKG_REPO } + "/archive/";
    }

    export inline std::string VCPKG_ARCHIVE_URL(const std::string_view ref)
    {
        const std::string resolved_ref =
            ref.empty() ? std::string{ VCPKG_DEFAULT_REF } : std::string{ ref };

        if (resolved_ref == VCPKG_DEFAULT_REF)
        {
            return VCPKG_ARCHIVE_BASE_URL()
                + "refs/heads/"
                + resolved_ref + ".zip";
        }

        return VCPKG_ARCHIVE_BASE_URL()
            + resolved_ref + ".zip";
    }

    export inline std::string VCPKG_BOOTSTRAP_SCRIPT_NAME()
    {
#if defined(_WIN32)
        return "bootstrap-vcpkg.bat";
#else
        return "bootstrap-vcpkg.sh";
#endif
    }

    export inline std::string VCPKG_EXECUTABLE_NAME()
    {
#if defined(_WIN32)
        return "vcpkg.exe";
#else
        return "vcpkg";
#endif
    }

    export inline std::string GIT_WINDOWS_RELEASE_API_URL()
    {
        return std::string{ GITHUB_API_BASE }
            + std::string{ GIT_WINDOWS_OWNER } + "/"
            + std::string{ GIT_WINDOWS_REPO } + "/releases/latest";
    }

    // ─────────────────────────────────────────
    // LLVM configuration
    // ─────────────────────────────────────────

    export inline constexpr std::string_view LLVM_VERSION = "20.1.0";

#if defined(_WIN32)

    export inline std::string LLVM_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "llvm/llvm-project/archive/refs/tags/llvmorg-"
            + std::string{ LLVM_VERSION } + ".zip";
    }

    export inline std::string LLVM_EXE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "llvm/llvm-project/releases/download/llvmorg-"
            + std::string{ LLVM_VERSION }
        + "/LLVM-" + std::string{ LLVM_VERSION } + "-win64.exe";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "C:/Program Files/LLVM/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return std::string{ GITHUB_BASE }
        + "ninja-build/ninja/releases/latest/download/ninja-win.zip";
    }

    export inline std::string NINJA_EXE_URL()
    {
        return std::string{ GITHUB_BASE }
        + "ninja-build/ninja/releases/latest/download/ninja-win.zip";
    }

#elif defined(__linux__)

    export inline std::string LLVM_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "llvm/llvm-project/archive/refs/tags/llvmorg-"
            + std::string{ LLVM_VERSION } + ".tar.gz";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "/usr/local/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return std::string{ GITHUB_BASE }
        + "ninja-build/ninja/releases/latest/download/ninja-linux.zip";
    }

#elif defined(__APPLE__)

    export inline std::string LLVM_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "llvm/llvm-project/archive/refs/tags/llvmorg-"
            + std::string{ LLVM_VERSION } + ".tar.gz";
    }

    export inline std::string LLVM_BIN_PATH()
    {
        return "/usr/local/bin";
    }

    export inline std::string NINJA_ZIP_URL()
    {
        return std::string{ GITHUB_BASE }
        + "ninja-build/ninja/releases/latest/download/ninja-mac.zip";
    }

#endif

    // ─────────────────────────────────────────
    // 7-Zip / archive tooling
    // ─────────────────────────────────────────

    export inline constexpr std::string_view SEVEN_ZIP_VERSION = "24.09";
    export inline constexpr std::string_view SEVEN_ZIP_VERSION_NAMETAG = "2409";

#if defined(_WIN32)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "ip7z/7zip/archive/refs/tags/"
            + std::string{ SEVEN_ZIP_VERSION } + ".zip";
    }

    export inline std::string SEVEN_ZIP_EXE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "ip7z/7zip/releases/latest/download/7z"
            + std::string{ SEVEN_ZIP_VERSION_NAMETAG } + "-x64.exe";
    }

    export inline std::string SEVEN_ZIP_LOCAL_BINARY()
    {
        return "C:/Program Files/7-Zip/7z.exe";
    }

    export inline constexpr std::string_view SEVEN_ZIP_BINARY = "7z.exe";

#elif defined(__linux__)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "ip7z/7zip/archive/refs/tags/"
            + std::string{ SEVEN_ZIP_VERSION } + ".tar.gz";
    }

    export inline std::string SEVEN_ZIP_LOCAL_BINARY()
    {
        return "/usr/bin/7z";
    }

    export inline constexpr std::string_view SEVEN_ZIP_BINARY = "7z";

    export inline std::string SEVEN_ZIP_INSTALL_CMD()
    {
        return "sudo apt-get install -y p7zip-full";
    }

#elif defined(__APPLE__)

    export inline std::string SEVEN_ZIP_SOURCE_URL()
    {
        return std::string{ GITHUB_BASE }
            + "ip7z/7zip/archive/refs/tags/"
            + std::string{ SEVEN_ZIP_VERSION } + ".tar.gz";
    }

    export inline constexpr std::string_view SEVEN_ZIP_BINARY = "7z";

    export inline std::string SEVEN_ZIP_INSTALL_CMD()
    {
        return "brew install p7zip";
    }

#endif
}
