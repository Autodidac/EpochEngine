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


#include <cstdint>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <wtypes.h>
#endif

//import platform.engine;
//import engine.config;
#include "engine.config.hpp" // for engine/backend configuration macros

//export import platform.engine;
//export import ecs.legacy_components;
//export import aengine.renderers;
//export import aengine.menu;
//export import input.engine;
//export import aengine.aallocator;
//export import aengine.aatlasmanager;
//export import aengine.aatlastexture;
//export import aengine.aimageloader;
//export import aengine.aimageatlaswriter;
//export import aengine.aimagewriter;
//export import aengine.atexture;
//export import aengine.autilities;

//#include "epoch.engine.hpp"          // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT

// Window dimensions
//inline constexpr int DEFAULT_WINDOW_WIDTH = 1024;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 768;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 1920;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1080;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 2048;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1080;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 2732;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1536;

// Logical desktop baseline. The native host resolves this through the active
// monitor and Epoch performance-tier policy before creating the window.
inline constexpr int DEFAULT_WINDOW_WIDTH = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 800;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 4096;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 2160;

namespace epochengine::core
{
    void ParseCommandLine(int argc, char** argv);
    void RunEngine();
}

namespace epochengine::project
{
    enum class ArtifactKind : std::uint32_t
    {
        scene = 1u << 0u,
        tilemap = 1u << 1u,
        input_profile = 1u << 2u,
        sprite_animation = 1u << 3u,
        audio_profile = 1u << 4u,
        gui = 1u << 5u
    };

    struct ArtifactAcceptanceRequest final
    {
        std::string_view project_id{};
        std::string_view project_root{};
        std::string_view scene_path{};
        std::string_view tilemap_path{};
        std::string_view input_profile_path{};
        std::string_view sprite_animation_path{};
        std::string_view audio_profile_path{};
        std::string_view gui_path{};
        void (*progress_sink)(const char*) noexcept{};
    };

    struct ArtifactAcceptanceReport final
    {
        bool succeeded{};
        std::uint32_t required_mask{};
        std::uint32_t verified_mask{};
        std::string stage{"not_started"};
        std::string diagnostic{};
    };

    [[nodiscard]] ArtifactAcceptanceReport VerifyArtifacts(
        const ArtifactAcceptanceRequest& request) noexcept;

    struct ArtifactRegenerationReport final
    {
        bool succeeded{};
        std::uint32_t required_mask{};
        std::uint32_t regenerated_mask{};
        std::uint32_t source_files_verified{};
        std::uint32_t artifact_files_quarantined{};
        std::string stage{"not_started"};
        std::string diagnostic{};
    };

    [[nodiscard]] ArtifactRegenerationReport VerifyLibraryRegeneration(
        const ArtifactAcceptanceRequest& request) noexcept;

    struct GameplayAcceptanceReport final
    {
        bool succeeded{};
        std::uint64_t input_frames{};
        std::uint64_t fixed_steps{};
        std::uint64_t animation_samples{};
        std::uint64_t audio_triggers{};
        std::uint64_t canvas_hash{};
        std::uint64_t logical_texture_bytes{};
        std::uint64_t emitted_sprites{};
        std::uint64_t emitted_batches{};
        std::uint32_t collision_surfaces{};
        std::uint32_t peak_contacts_per_step{};
        std::uint64_t audio_resident_bytes{};
        std::uint64_t canvas_rejections{};
        std::uint32_t portable_budget_violations{};
        std::string stage{"not_started"};
        std::string diagnostic{};
    };

    [[nodiscard]] GameplayAcceptanceReport VerifyPlayable2D(
        const ArtifactAcceptanceRequest& request) noexcept;
}

#if defined(_WIN32) && defined(EPOCH_USING_WINMAIN)

// Max string length for title and class name
#define MAX_LOADSTRING 100

namespace epochengine::core
{
    // Forward declarations of Win32 functions
    ATOM RegisterWindowClass(HINSTANCE hInstance, LPCWSTR window_name, LPCWSTR child_name);
    void PrintLastWin32Error(const wchar_t* lpszFunction);
    void ShowConsole();
}

#endif
#if defined(_WIN32)
HWND InitWindowInstance(HINSTANCE hInstance, int nCmdShow, LPCWSTR szWindowClass, LPCWSTR szTitle, int32_t windowWidth, int32_t windowHeight);
#endif
