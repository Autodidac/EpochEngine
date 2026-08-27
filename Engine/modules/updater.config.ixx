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

#include <string>
#include <string_view>
#include <vector>

export module updater.config;

import platform.engine;
import epoch.version;

namespace epochengine::updater
{
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Installation behavior
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline constexpr bool LEAVE_NO_FILES_ALWAYS_REDOWNLOAD = true;

#if defined(EPOCH_BINARY_ONLY_DISTRIBUTION) && EPOCH_BINARY_ONLY_DISTRIBUTION
    export inline constexpr bool AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED = false;
#else
    export inline constexpr bool AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED = true;
#endif

    export inline constexpr bool SOURCE_UPDATE_FALLBACK_ENABLED =
        AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED;
    export inline constexpr bool SOURCE_PROJECT_DOWNLOAD_ENABLED =
        AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED;

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Project identity
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline constexpr std::string_view OWNER = "Autodidac";
    export inline constexpr std::string_view REPO = "EpochEngine";
    export inline constexpr std::string_view BRANCH = "main";

    export inline const std::string PROJECT_VERSION =
        epochengine::GetEngineVersionString();
    export inline const std::string PROJECT_SOURCE_VERSION =
        epochengine::GetEngineVersionString();
    export inline const std::string PROJECT_PACKAGED_VERSION =
        epochengine::GetPackagedVersionString();

    export inline constexpr std::string_view WINDOWS_RUNTIME_BINARY_PREFIX =
        "epoch_win10_x64_v";
    export inline constexpr std::string_view WINDOWS_RUNTIME_BINARY_SUFFIX =
        ".zip";
    export inline constexpr std::string_view LINUX_RUNTIME_BINARY_PREFIX =
        "epoch_linux_x64_v";
    export inline constexpr std::string_view LINUX_RUNTIME_BINARY_SUFFIX =
        ".tar.gz";
    export inline constexpr std::string_view WINDOWS_BOOTSTRAP_BINARY_PREFIX =
        "epoch_updater_shell_only_win10_x64_v";
    export inline constexpr std::string_view WINDOWS_BOOTSTRAP_BINARY_SUFFIX =
        ".zip";
    export inline constexpr std::string_view LINUX_BOOTSTRAP_BINARY_PREFIX =
        "epoch_updater_shell_only_linux_x64_v";
    export inline constexpr std::string_view LINUX_BOOTSTRAP_BINARY_SUFFIX =
        ".tar.gz";

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Runtime / source build metadata
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline std::string RUNTIME_BINARY_NAME()
    {
#if defined(_WIN32)
        return "EpochEditor.exe";
#else
        return "epoch";
#endif
    }

    export inline std::string CACHE_ROOT_SUBDIR()
    {
        return "cache";
    }

    export inline std::string UPDATER_CACHE_SUBDIR()
    {
        return "updates";
    }

    export inline std::string PACKAGE_CACHE_SUBDIR()
    {
        return "packages";
    }

    export inline std::string ATLAS_CACHE_SUBDIR()
    {
        return "atlases";
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
        return "Engine/modules/engine.version.ixx";
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

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // GitHub base URLs retained for managed third-party tool downloads
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline constexpr std::string_view GITHUB_BASE = "https://github.com/";
    export inline constexpr std::string_view GITHUB_RAW_BASE = "https://raw.githubusercontent.com/";
    export inline constexpr std::string_view GITHUB_API_BASE = "https://api.github.com/repos/";
    export inline constexpr std::string_view EPOCH_SITE_BASE =
        "https://epoch.adamrushford.chatgpt.site";

    export inline std::string PROJECT_SOURCE_DEVICE_START_URL()
    {
        return AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED
            ? std::string{ EPOCH_SITE_BASE } + "/api/private/epoch-engine/device/start"
            : std::string{};
    }

    export inline std::string PROJECT_SOURCE_DEVICE_TOKEN_URL()
    {
        return AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED
            ? std::string{ EPOCH_SITE_BASE } + "/api/private/epoch-engine/device/token"
            : std::string{};
    }

    export inline std::string PROJECT_SOURCE_DEVICE_CHALLENGE_URL()
    {
        return AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED
            ? std::string{ EPOCH_SITE_BASE } + "/api/private/epoch-engine/device/challenge"
            : std::string{};
    }

    export inline constexpr std::string_view PROJECT_PRIVATE_SOURCE_MANIFEST_SCHEMA =
        "https://epoch.adamrushford.chatgpt.site/schemas/private-source/v1";
    export inline constexpr std::string_view PROJECT_PRIVATE_SOURCE_WRAP_INFO =
        "EpochEngine private source DEK wrap v1";
    export inline constexpr std::string_view PROJECT_PRIVATE_SOURCE_WRAP_AAD_PREFIX =
        "epoch-source-key-wrap-v1";

    export inline std::string PROJECT_REPOSITORY_URL()
    {
        return {};
    }

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Version / package URLs
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline std::string PROJECT_SOURCE_VERSION_URL()
    {
        return std::string{ EPOCH_SITE_BASE } + "/api/epoch/source-version";
    }

    export inline std::string PROJECT_SOURCE_ARCHIVE_EXTENSION()
    {
        return std::string{ epochengine::platform::policy::source_snapshot_archive_extension() };
    }

    export inline std::string PROJECT_SOURCE_ARCHIVE_LABEL()
    {
        return std::string{ epochengine::platform::policy::source_snapshot_archive_label() };
    }

    export inline std::string PACKAGED_BINARY_ASSET_PREFIX()
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return std::string{ WINDOWS_RUNTIME_BINARY_PREFIX };
        case platform::RuntimePlatform::Linux:
            return std::string{ LINUX_RUNTIME_BINARY_PREFIX };
        case platform::RuntimePlatform::MacOS:
            return "epoch_macos_x64_v";
        default:
            return std::string{ WINDOWS_RUNTIME_BINARY_PREFIX };
        }
    }

    export inline std::string PACKAGED_BINARY_ASSET_SUFFIX()
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return std::string{ WINDOWS_RUNTIME_BINARY_SUFFIX };
        case platform::RuntimePlatform::Linux:
            return std::string{ LINUX_RUNTIME_BINARY_SUFFIX };
        case platform::RuntimePlatform::MacOS:
            return ".tar.gz";
        default:
            return std::string{ WINDOWS_RUNTIME_BINARY_SUFFIX };
        }
    }

    export inline std::string BOOTSTRAP_BINARY_ASSET_PREFIX()
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return std::string{ WINDOWS_BOOTSTRAP_BINARY_PREFIX };
        case platform::RuntimePlatform::Linux:
            return std::string{ LINUX_BOOTSTRAP_BINARY_PREFIX };
        case platform::RuntimePlatform::MacOS:
            return "epoch_updater_shell_only_macos_x64_v";
        default:
            return std::string{ WINDOWS_BOOTSTRAP_BINARY_PREFIX };
        }
    }

    export inline std::string BOOTSTRAP_BINARY_ASSET_SUFFIX()
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return std::string{ WINDOWS_BOOTSTRAP_BINARY_SUFFIX };
        case platform::RuntimePlatform::Linux:
            return std::string{ LINUX_BOOTSTRAP_BINARY_SUFFIX };
        case platform::RuntimePlatform::MacOS:
            return ".tar.gz";
        default:
            return std::string{ WINDOWS_BOOTSTRAP_BINARY_SUFFIX };
        }
    }

    export inline std::string CURRENT_PACKAGED_BINARY_ASSET_NAME()
    {
        return PACKAGED_BINARY_ASSET_PREFIX()
            + std::string{ PROJECT_PACKAGED_VERSION }
            + PACKAGED_BINARY_ASSET_SUFFIX();
    }

    export inline std::string PROJECT_PACKAGED_VERSION_URL()
    {
        return {};
    }

    export inline std::string PROJECT_SOURCE_URL()
    {
        return {};
    }

    export inline std::string PROJECT_SOURCE_CHECKSUM_URL()
    {
        const std::string source_url = PROJECT_SOURCE_URL();
        return source_url.empty() ? std::string{} : source_url + ".sha256";
    }

    export inline std::vector<std::string> PROJECT_SOURCE_FALLBACK_URLS()
    {
        return {};
    }

    export inline std::string PROJECT_BINARY_URL()
    {
        return {};
    }

    export inline std::string PROJECT_ACTION_RUNS_API_URL()
    {
        return std::string{ EPOCH_SITE_BASE } + "/api/epoch/actions/runs";
    }

    export inline std::string PROJECT_UPDATE_BUILD_JOB_NAME()
    {
        switch (platform::current_platform())
        {
        case platform::RuntimePlatform::Windows:
            return "windows-msvc";
        case platform::RuntimePlatform::Linux:
            return "linux-clang-engine";
        default:
            return {};
        }
    }

    export inline std::string PROJECT_RELEASE_API_URL()
    {
        return std::string{ EPOCH_SITE_BASE } + "/api/epoch/releases/latest";
    }

    export inline std::string PROJECT_RELEASES_API_URL()
    {
        return std::string{ EPOCH_SITE_BASE } + "/api/epoch/releases";
    }

    export inline std::string PROJECT_RELEASE_INTEGRITY_URL()
    {
        return std::string{ EPOCH_SITE_BASE } + "/api/epoch/release-integrity";
    }

    export inline constexpr std::string_view PROJECT_RELEASE_INTEGRITY_SCHEMA =
        "https://epoch.adamrushford.chatgpt.site/schemas/release-integrity/v1";
    export inline constexpr std::string_view PROJECT_RELEASE_SIGNING_ALGORITHM = "Ed25519";
    export inline constexpr std::string_view PROJECT_RELEASE_SIGNING_KEY_ID =
        "epoch-release-e76c3921327a2cd0";
    export inline constexpr std::string_view PROJECT_RELEASE_SIGNING_PUBLIC_KEY_BASE64URL =
        "7W8_nJDpPERrsA2QsGQecnTCFbs8i7LTTsS9ORLQGKk";

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // Managed updater tools
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline std::string UPDATER_TOOLS_SUBDIR()
    {
        return "tools";
    }

    export inline std::string UPDATER_WORK_SUBDIR()
    {
        return "work";
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

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // LLVM configuration
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    export inline constexpr std::string_view LLVM_VERSION = "22.1.8";

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
        + "ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip";
    }

    export inline std::string NINJA_EXE_URL()
    {
        return std::string{ GITHUB_BASE }
        + "ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip";
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
        + "ninja-build/ninja/releases/download/v1.13.2/ninja-linux.zip";
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
        + "ninja-build/ninja/releases/download/v1.13.2/ninja-mac.zip";
    }

#endif

    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // 7-Zip / archive tooling
    // â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

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
