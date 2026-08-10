cmake_minimum_required(VERSION 4.4)

# Keep module scanning enabled across all supported generators/toolchains.
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(Epoch LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckCXXCompilerFlag)

option(EPOCH_ENABLE_CPP26_LANE "Enable optional C++26/latest validation flags for this build." OFF)
option(EPOCH_STRICT_WARNINGS "Enable stricter compiler warnings for Epoch targets." OFF)
option(EPOCH_WARNINGS_AS_ERRORS "Treat warnings as errors when strict warnings are enabled." OFF)
option(EPOCH_ENABLE_SANITIZERS "Enable supported sanitizer flags for compatible compilers." OFF)
option(EPOCH_CI_HEADLESS_ONLY "Configure only the portable hosted-CI smoke target." OFF)
option(EPOCH_ENABLE_AUTHORING_PLATFORM "Gate the shared Epoch authoring-document and editor foundation as implementation units land." ON)
option(EPOCH_ENABLE_TEXTURE_EDITOR "Gate texture painting, compositing, and texture-node authoring implementation units." ON)
option(EPOCH_ENABLE_MODEL_EDITOR "Gate mesh editing, procedural modeling, and sculpt implementation units." ON)
option(EPOCH_ENABLE_NODE_EDITOR "Gate the shared typed node-graph editor and evaluator implementation units." ON)
option(EPOCH_ENABLE_MATERIAL_EDITOR "Gate material graph authoring and preview implementation units." ON)
option(EPOCH_ENABLE_ANIMATION_EDITOR "Gate animation graph and timeline authoring implementation units." ON)
option(EPOCH_ENABLE_AUTHORING_COLLABORATION "Gate branch sharing, review, and collaborative authoring implementation units." OFF)
option(EPOCH_ENABLE_AUTHORING_METRICS "Gate document, history, cache, GPU, and evaluation metrics implementation units." ON)
option(EPOCH_ALLOW_GCC_MODULE_ENGINE "Opt into the experimental full-engine GCC modules build on Linux." OFF)
set(EPOCH_GLAD_PROVIDER "auto" CACHE STRING "Select the OpenGL loader provider: auto, vcpkg, or bundled.")
set_property(CACHE EPOCH_GLAD_PROVIDER PROPERTY STRINGS auto vcpkg bundled)
string(TOLOWER "${EPOCH_GLAD_PROVIDER}" EPOCH_GLAD_PROVIDER)
if(NOT EPOCH_GLAD_PROVIDER MATCHES "^(auto|vcpkg|bundled)$")
    message(FATAL_ERROR "EPOCH_GLAD_PROVIDER must be one of: auto, vcpkg, bundled.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT WIN32 AND NOT EPOCH_CI_HEADLESS_ONLY AND NOT EPOCH_ALLOW_GCC_MODULE_ENGINE)
    message(STATUS
        "Linux/GCC full-engine C++ module builds remain opt-in while the current GCC 16 "
        "module lane is validated. Configuring the portable "
        "headless validation target instead. Use Clang for full Linux engine builds or "
        "set EPOCH_ALLOW_GCC_MODULE_ENGINE=ON to experiment.")
    set(EPOCH_CI_HEADLESS_ONLY ON CACHE BOOL "Configure only the portable hosted-CI smoke target." FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND NOT WIN32
    AND NOT EPOCH_CI_HEADLESS_ONLY
    AND EPOCH_ALLOW_GCC_MODULE_ENGINE
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.1)
    message(FATAL_ERROR
        "GCC ${CMAKE_CXX_COMPILER_VERSION} cannot build Epoch's full C++23 module engine. "
        "Use current Clang with the Ninja generator, install GCC 16.1+, or leave "
        "EPOCH_ALLOW_GCC_MODULE_ENGINE=OFF for the GCC headless validation lane.")
endif()

set(EPOCH_GENERATOR_SUPPORTS_MODULES OFF)
if(CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
    set(EPOCH_GENERATOR_SUPPORTS_MODULES ON)
endif()

set(EPOCH_CONFIGURES_MODULE_TARGETS ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT WIN32 AND EPOCH_CI_HEADLESS_ONLY AND NOT EPOCH_ALLOW_GCC_MODULE_ENGINE)
    set(EPOCH_CONFIGURES_MODULE_TARGETS OFF)
endif()

if(EPOCH_CONFIGURES_MODULE_TARGETS AND NOT EPOCH_GENERATOR_SUPPORTS_MODULES)
    message(FATAL_ERROR
        "Epoch's C++23 module targets require the Ninja, Ninja Multi-Config, or "
        "Visual Studio generator. Reconfigure with build.sh, a repo preset, or "
        "`cmake -G Ninja ...`; do not use Unix Makefiles for the full engine.")
endif()

function(epoch_is_msvc_frontend out_var)
    if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(epoch_apply_cxx_dialect target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "epoch_apply_cxx_dialect called for missing target: ${target_name}")
    endif()

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(EPOCH_ENABLE_CPP26_LANE)
        epoch_is_msvc_frontend(_epoch_msvc_frontend)
        if(_epoch_msvc_frontend)
            target_compile_options(${target_name} PRIVATE /std:c++latest /Zc:__cplusplus /permissive-)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            check_cxx_compiler_flag("-std=c++2c" EPOCH_COMPILER_SUPPORTS_STDCXX2C)
            if(EPOCH_COMPILER_SUPPORTS_STDCXX2C)
                target_compile_options(${target_name} PRIVATE -std=c++2c)
            else()
                message(FATAL_ERROR
                    "EPOCH_ENABLE_CPP26_LANE=ON but ${CMAKE_CXX_COMPILER_ID} "
                    "${CMAKE_CXX_COMPILER_VERSION} does not accept -std=c++2c.")
            endif()
        else()
            message(FATAL_ERROR
                "EPOCH_ENABLE_CPP26_LANE=ON is not defined for compiler "
                "${CMAKE_CXX_COMPILER_ID}.")
        endif()
    endif()
endfunction()

function(epoch_apply_warnings target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "epoch_apply_warnings called for missing target: ${target_name}")
    endif()

    if(NOT EPOCH_STRICT_WARNINGS)
        return()
    endif()

    epoch_is_msvc_frontend(_epoch_msvc_frontend)
    if(_epoch_msvc_frontend)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
        if(EPOCH_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
        if(EPOCH_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(epoch_apply_sanitizers target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "epoch_apply_sanitizers called for missing target: ${target_name}")
    endif()

    if(NOT EPOCH_ENABLE_SANITIZERS)
        return()
    endif()

    epoch_is_msvc_frontend(_epoch_msvc_frontend)
    if(_epoch_msvc_frontend)
        message(STATUS "EPOCH_ENABLE_SANITIZERS requested, but MSVC sanitizer flags are not enabled by this first-pass foundation.")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    else()
        message(STATUS "EPOCH_ENABLE_SANITIZERS requested, but ${CMAKE_CXX_COMPILER_ID} is not configured for sanitizers.")
    endif()
endfunction()

function(epoch_print_build_summary)
    set(_epoch_active_backends "headless-only")
    if(DEFINED EPOCH_RAYLIB_ACTIVE)
        set(_epoch_active_backends
            "Raylib=${EPOCH_RAYLIB_ACTIVE}; SDL=${EPOCH_SDL_ACTIVE}; SFML=${EPOCH_SFML_ACTIVE}; Vulkan=${EPOCH_VULKAN_ACTIVE}; OpenGL=${EPOCH_OPENGL_ACTIVE}; DirectX=${EPOCH_DIRECTX_ACTIVE}; Software=${EPOCH_SOFTWARE_RENDERER_ACTIVE}")
    endif()

    message(STATUS "Epoch build summary")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  Generator: ${CMAKE_GENERATOR}")
    message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  C++ baseline: ${CMAKE_CXX_STANDARD}")
    message(STATUS "  C++26/latest lane: ${EPOCH_ENABLE_CPP26_LANE}")
    message(STATUS "  Strict warnings: ${EPOCH_STRICT_WARNINGS}")
    message(STATUS "  Warnings as errors: ${EPOCH_WARNINGS_AS_ERRORS}")
    message(STATUS "  Sanitizers: ${EPOCH_ENABLE_SANITIZERS}")
    message(STATUS "  CI/headless-only: ${EPOCH_CI_HEADLESS_ONLY}")
    message(STATUS "  Authoring platform: ${EPOCH_ENABLE_AUTHORING_PLATFORM}")
    message(STATUS "  Authoring editors: texture=${EPOCH_ENABLE_TEXTURE_EDITOR}; model=${EPOCH_ENABLE_MODEL_EDITOR}; node=${EPOCH_ENABLE_NODE_EDITOR}; material=${EPOCH_ENABLE_MATERIAL_EDITOR}; animation=${EPOCH_ENABLE_ANIMATION_EDITOR}")
    message(STATUS "  Authoring collaboration: ${EPOCH_ENABLE_AUTHORING_COLLABORATION}")
    message(STATUS "  Authoring metrics: ${EPOCH_ENABLE_AUTHORING_METRICS}")
    message(STATUS "  GCC full-engine module opt-in: ${EPOCH_ALLOW_GCC_MODULE_ENGINE}")
    message(STATUS "  GLAD provider requested: ${EPOCH_GLAD_PROVIDER}")
    if(DEFINED EPOCH_GLAD_RESOLVED_PROVIDER)
        message(STATUS "  GLAD provider resolved: ${EPOCH_GLAD_RESOLVED_PROVIDER}")
    endif()
    message(STATUS "  Active backends: ${_epoch_active_backends}")
endfunction()

set(EPOCH_CI_HEADLESS_SOURCES
    examples/HeadlessCI/headless_smoke.cpp
)

set(EPOCH_CI_HEADLESS_USES_MODULE_LOG ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT WIN32 AND EPOCH_CI_HEADLESS_ONLY AND NOT EPOCH_ALLOW_GCC_MODULE_ENGINE)
    set(EPOCH_CI_HEADLESS_USES_MODULE_LOG OFF)
endif()

if(EPOCH_CI_HEADLESS_USES_MODULE_LOG)
    list(APPEND EPOCH_CI_HEADLESS_SOURCES
        src/core.log.cpp
        src/core.time.cpp
    )
else()
    list(APPEND EPOCH_CI_HEADLESS_SOURCES
        src/core.log_headless.cpp
    )
endif()

add_executable(epoch_ci_headless ${EPOCH_CI_HEADLESS_SOURCES})

if(EPOCH_CI_HEADLESS_USES_MODULE_LOG)
    set_target_properties(epoch_ci_headless PROPERTIES
        CXX_SCAN_FOR_MODULES ON
    )

    target_sources(epoch_ci_headless PRIVATE
        FILE_SET epoch_ci_headless_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
            FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.log.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.time.ixx
    )
else()
    set_target_properties(epoch_ci_headless PROPERTIES
        CXX_SCAN_FOR_MODULES OFF
    )
endif()

target_include_directories(epoch_ci_headless PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(epoch_ci_headless PRIVATE
    EPOCH_MAIN_HEADLESS=1
    EPOCH_CI_HEADLESS_BUILD=1
)

epoch_apply_cxx_dialect(epoch_ci_headless)
epoch_apply_warnings(epoch_ci_headless)
epoch_apply_sanitizers(epoch_ci_headless)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME epoch_ci_headless
        COMMAND epoch_ci_headless ${CMAKE_SOURCE_DIR}
    )

    add_executable(epoch_texture_artifact_contract
    src/assets/texture/asset.texture_artifact.cpp
        src/assets/texture/asset.texture_artifact_contract.cpp
    )
    target_sources(epoch_texture_artifact_contract PRIVATE
        FILE_SET epoch_texture_artifact_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
            FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/asset.texture_artifact.ixx
    )
    target_include_directories(epoch_texture_artifact_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_texture_artifact_contract PRIVATE
        EPOCH_TEXTURE_ARTIFACT_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_texture_artifact_contract)
    epoch_apply_warnings(epoch_texture_artifact_contract)
    epoch_apply_sanitizers(epoch_texture_artifact_contract)
    add_test(
        NAME epoch_texture_artifact_contract
        COMMAND epoch_texture_artifact_contract
    )
endif()

if(EPOCH_CI_HEADLESS_ONLY)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT WIN32 AND NOT EPOCH_ALLOW_GCC_MODULE_ENGINE AND NOT TARGET epoch)
        add_custom_target(epoch
            DEPENDS epoch_ci_headless
            COMMAND ${CMAKE_COMMAND} -E echo "Linux/GCC full-engine modules are skipped; epoch target built epoch_ci_headless instead."
            COMMENT "Using headless Epoch validation target for Linux/GCC")
    endif()
    epoch_print_build_summary()
    return()
endif()

add_library(epoch_gui STATIC
    dep/EpochGui/src/epochgui/rounded_rect.cpp
    src/epochgui/gui.dockable_window.cpp
    src/epochgui/gui.dock_layout.cpp
    src/epochgui/gui.floating_window.cpp
    src/epochgui/gui.panel_host.cpp
    src/epochgui/gui.popup_layout.cpp
    src/epochgui/gui.text_control.cpp
    dep/EpochGui/src/epochgui/text_editor.cpp
)

target_sources(epoch_gui PUBLIC
    FILE_SET epoch_gui_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.rounded_rect.ixx
)

target_include_directories(epoch_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

epoch_apply_cxx_dialect(epoch_gui)
epoch_apply_warnings(epoch_gui)
epoch_apply_sanitizers(epoch_gui)

set(EPOCH_VERSION_OVERRIDE_MAJOR "" CACHE STRING "Override engine version major for packaging builds")
set(EPOCH_VERSION_OVERRIDE_MINOR "" CACHE STRING "Override engine version minor for packaging builds")
set(EPOCH_VERSION_OVERRIDE_REVISION "" CACHE STRING "Override engine version revision for packaging builds")
set(EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MAJOR "" CACHE STRING "Override packaged Windows version major")
set(EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MINOR "" CACHE STRING "Override packaged Windows version minor")
set(EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_REVISION "" CACHE STRING "Override packaged Windows version revision")
set(EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR "" CACHE STRING "Override packaged Linux version major")
set(EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR "" CACHE STRING "Override packaged Linux version minor")
set(EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION "" CACHE STRING "Override packaged Linux version revision")
set(EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MAJOR "" CACHE STRING "Override packaged macOS version major")
set(EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MINOR "" CACHE STRING "Override packaged macOS version minor")
set(EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_REVISION "" CACHE STRING "Override packaged macOS version revision")

set(EPOCH_SOURCES
    src/epoch.engine_legacy.cpp
    src/audio/audio.manager.cpp
    src/physics/physics.manager.cpp
    src/renderers/core/core.context_command_queue.cpp
    src/renderers/core/engine.context.cpp
    src/renderers/directx/core.context_directx.cpp
    src/renderers/software/core.context_software.cpp
    src/renderers/vulkan/core.context_vulkan.cpp
    src/renderers/directx/directx.context.cpp
    src/renderers/directx/directx.device.cpp
    src/renderers/directx/directx.gui.cpp
    src/renderers/directx/directx.preview.cpp
    src/renderers/directx/directx.state.cpp
    src/renderers/opengl/opengl.backend.cpp
    src/renderers/opengl/opengl.context.cpp
    src/renderers/opengl/opengl.context_init.cpp
    src/renderers/opengl/opengl.context_process.cpp
    src/renderers/opengl/opengl.context_cleanup.cpp
    src/renderers/opengl/opengl.context_detail.cpp
    src/renderers/opengl/opengl.scene_preview.cpp
    src/renderers/opengl/opengl.canvas2d_scene.cpp
    src/renderers/opengl/opengl.context_process_impl.cpp
    src/renderers/software/software.context.cpp
    src/renderers/software/software.context_capture.cpp
    src/renderers/software/software.context_draw.cpp
    src/renderers/software/software.context_present.cpp
    src/renderers/software/software.context_preview.cpp
    src/renderers/software/software.context_render.cpp
    src/renderers/opengl/core.context_opengl.cpp
    src/renderers/sfml/core.context_sfml.cpp
    src/renderers/sdl/core.context_sdl.cpp
    src/renderers/raylib/core.context_raylib.cpp
    src/renderers/raylib/raylib.api.cpp
    src/epochgui/gui.engine.cpp
    src/scripts/scripting.compiler_engine.cpp
    src/editor/editor.application.cpp
    src/editor/editor.scene.cpp
    src/editor/editor.standard.cpp
    src/editor/editor.plant_lab.cpp
    src/editor/editor.gui.cpp
    src/editor/editor.canvas2d_scene.cpp
    src/renderers/canvas2d/render.canvas2d_scene.cpp
    src/renderers/canvas2d/render.canvas2d_scene_contract.cpp
    src/renderers/canvas2d/canvas2d.scene_contracts.cpp
    src/utility.filewatch.cpp
    src/font.renderer.cpp
    src/project/project.asset_registry.cpp
    src/project/project.texture_resources.cpp
    src/project/project.texture_library.cpp
    src/project/project.texture_pipeline.cpp
    src/project/project.contracts.cpp
    src/assets/texture/asset.texture_artifact.cpp
    src/core.assert.cpp
    src/core.env.cpp
    src/core.error.cpp
    src/core.id.cpp
    src/core.log.cpp
    src/core.math.cpp
    src/platform/core.path.cpp
    src/core.string.cpp
    src/core.time.cpp
    src/ai/ai.engine.cpp
    src/epoch.engine.cpp
    src/epoch.headers.cpp
    src/renderers/platform/platform.context_epoch.cpp
    src/platform/platform.window.cpp
    src/render.graph_epoch.cpp
    src/epoch.systems.cpp
    src/epoch.main.cpp
    src/epoch.runtime.cpp
)


if(WIN32)
    list(APPEND EPOCH_SOURCES
        src/renderers/host/host.context_win.cpp
        src/renderers/opengl/opengl.frame_capture.cpp
        resource/WinResource.rc
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND EPOCH_SOURCES
        src/renderers/host/host.context_linux.cpp
    )
endif()

if(EPOCH_ENABLE_AUTHORING_PLATFORM AND EPOCH_ENABLE_TEXTURE_EDITOR)
    list(APPEND EPOCH_SOURCES
        src/authoring/texture/authoring.texture_document.cpp
    )
endif()

add_executable(epoch ${EPOCH_SOURCES})
if(WIN32)
    set_target_properties(epoch PROPERTIES OUTPUT_NAME EpochEditor)
else()
    set_target_properties(epoch PROPERTIES
        OUTPUT_NAME epoch
        BUILD_RPATH_USE_ORIGIN TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "\$ORIGIN/lib")
endif()

if(MSVC)
    set_target_properties(epoch epoch_ci_headless epoch_gui PROPERTIES
        VS_GLOBAL_VcpkgApplocalDeps "false"
        VS_GLOBAL_VcpkgAutoBootstrap "false"
    )

    target_compile_options(epoch PRIVATE /wd4251 /wd4275 /wd4834)

    set_source_files_properties(
        src/renderers/core/engine.context.cpp
        src/renderers/software/core.context_software.cpp
        src/renderers/vulkan/core.context_vulkan.cpp
        src/renderers/software/software.context.cpp
        src/renderers/software/software.context_capture.cpp
        src/renderers/software/software.context_draw.cpp
        src/renderers/software/software.context_present.cpp
        src/renderers/software/software.context_preview.cpp
        src/renderers/software/software.context_render.cpp
        src/renderers/opengl/core.context_opengl.cpp
        src/renderers/sfml/core.context_sfml.cpp
        src/renderers/sdl/core.context_sdl.cpp
        src/renderers/raylib/core.context_raylib.cpp
        PROPERTIES
            COMPILE_OPTIONS "/Zm500"
    )

    set_source_files_properties(
        src/renderers/core/engine.context.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000"
    )

    set_source_files_properties(
        src/renderers/opengl/core.context_opengl.cpp
        src/renderers/opengl/opengl.backend.cpp
        src/renderers/opengl/opengl.context.cpp
        src/renderers/opengl/opengl.context_init.cpp
        src/renderers/opengl/opengl.context_process.cpp
        src/renderers/opengl/opengl.context_cleanup.cpp
        src/renderers/opengl/opengl.frame_capture.cpp
        src/renderers/opengl/opengl.context_detail.cpp
        src/renderers/opengl/opengl.scene_preview.cpp
        src/renderers/opengl/opengl.context_process_impl.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000;/Od;/Ob0;/GL-"
    )

    set_source_files_properties(
        src/renderers/sfml/core.context_sfml.cpp
        src/renderers/sdl/core.context_sdl.cpp
        src/renderers/raylib/core.context_raylib.cpp
        src/renderers/directx/core.context_directx.cpp
        src/renderers/directx/directx.context.cpp
        src/renderers/directx/directx.device.cpp
        src/renderers/directx/directx.gui.cpp
        src/renderers/directx/directx.preview.cpp
        src/renderers/directx/directx.state.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000;/GL-"
    )

    set_source_files_properties(
        src/renderers/software/software.context_render.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000;/GL-"
    )

    set_source_files_properties(
        src/renderers/raylib/raylib.api.cpp
        PROPERTIES
            COMPILE_OPTIONS "/GL-"
    )
endif()

function(epoch_apply_version_overrides target_name)
    if(NOT "${EPOCH_VERSION_OVERRIDE_MAJOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_VERSION_MAJOR=${EPOCH_VERSION_OVERRIDE_MAJOR})
    endif()
    if(NOT "${EPOCH_VERSION_OVERRIDE_MINOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_VERSION_MINOR=${EPOCH_VERSION_OVERRIDE_MINOR})
    endif()
    if(NOT "${EPOCH_VERSION_OVERRIDE_REVISION}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_VERSION_REVISION=${EPOCH_VERSION_OVERRIDE_REVISION})
    endif()

    if(NOT "${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MAJOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MAJOR=${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MAJOR})
    endif()
    if(NOT "${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MINOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_MINOR=${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_MINOR})
    endif()
    if(NOT "${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_REVISION}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_WINDOWS_PACKAGED_VERSION_REVISION=${EPOCH_WINDOWS_PACKAGED_VERSION_OVERRIDE_REVISION})
    endif()

    if(NOT "${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MAJOR=${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MAJOR})
    endif()
    if(NOT "${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_MINOR=${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_MINOR})
    endif()
    if(NOT "${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_LINUX_PACKAGED_VERSION_REVISION=${EPOCH_LINUX_PACKAGED_VERSION_OVERRIDE_REVISION})
    endif()

    if(NOT "${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MAJOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MAJOR=${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MAJOR})
    endif()
    if(NOT "${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MINOR}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_MINOR=${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_MINOR})
    endif()
    if(NOT "${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_REVISION}" STREQUAL "")
        target_compile_definitions(${target_name} PRIVATE EPOCH_OVERRIDE_MACOS_PACKAGED_VERSION_REVISION=${EPOCH_MACOS_PACKAGED_VERSION_OVERRIDE_REVISION})
    endif()
endfunction()

epoch_apply_version_overrides(epoch)

if(EPOCH_UPDATER_SHELL_BUILD)
  add_custom_command(
    TARGET epoch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:epoch>/epoch_renderer_smoke.exe"
            "$<TARGET_FILE_DIR:epoch>/epoch_renderer_smoke"
    COMMENT "Removing stale renderer smoke artifacts from updater-shell output"
  )
endif()

add_executable(epoch_renderer_smoke EXCLUDE_FROM_ALL
    src/renderers/render.smoke_harness.cpp
    src/core.log.cpp
    src/core.time.cpp
)

target_sources(epoch_renderer_smoke PRIVATE
    FILE_SET epoch_renderer_smoke_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.log.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.time.ixx
)

epoch_apply_version_overrides(epoch_renderer_smoke)


set(EPOCH_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)

# LLVM 22.1.8 can crash in optimization passes while compiling these C++23
# modules on Linux. Neither surface is render hot-path code, so keep the
# workaround source-local instead of lowering engine-wide Release optimization.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE)
    set_source_files_properties(
        "${EPOCH_MODULE_DIR}/core.commandline.ixx"
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-O0>"
    )
    set_source_files_properties(
        "${EPOCH_MODULE_DIR}/network.core.ixx"
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Release>:-O0>"
    )
endif()
file(GLOB EPOCH_MODULE_FILES CONFIGURE_DEPENDS ${EPOCH_MODULE_DIR}/*.ixx)
list(SORT EPOCH_MODULE_FILES)

target_include_directories(epoch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

target_link_libraries(epoch PRIVATE epoch_gui)

find_path(CGLTF_INCLUDE_DIR cgltf.h)
if(CGLTF_INCLUDE_DIR)
    target_include_directories(epoch PRIVATE ${CGLTF_INCLUDE_DIR})
    target_compile_definitions(epoch PRIVATE EPOCH_HAS_CGLTF=1)
else()
    target_compile_definitions(epoch PRIVATE EPOCH_HAS_CGLTF=0)
    message(WARNING "cgltf.h was not found; editor glTF metadata parsing will be disabled for this build.")
endif()

target_include_directories(epoch_renderer_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

find_program(GLSLANG_VALIDATOR glslangValidator)
set(EPOCH_VULKAN_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets/vulkan)
set(EPOCH_VULKAN_SHADER_DIR ${EPOCH_VULKAN_ASSET_DIR}/shaders)
set(EPOCH_FONT_ASSET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts)
set(EPOCH_FONT_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets/fonts)

file(MAKE_DIRECTORY ${EPOCH_VULKAN_SHADER_DIR})
file(MAKE_DIRECTORY ${EPOCH_FONT_ASSET_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/texture.ppm
    ${EPOCH_VULKAN_ASSET_DIR}/texture.ppm
    COPYONLY
)
if(EXISTS ${EPOCH_FONT_ASSET_SOURCE_DIR}/Roboto-Regular.ttf)
    configure_file(
        ${EPOCH_FONT_ASSET_SOURCE_DIR}/Roboto-Regular.ttf
        ${EPOCH_FONT_ASSET_DIR}/Roboto-Regular.ttf
        COPYONLY
    )
endif()
if(EXISTS ${EPOCH_FONT_ASSET_SOURCE_DIR}/OFL.txt)
    configure_file(
        ${EPOCH_FONT_ASSET_SOURCE_DIR}/OFL.txt
        ${EPOCH_FONT_ASSET_DIR}/OFL.txt
        COPYONLY
    )
endif()

if(GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT
            ${EPOCH_VULKAN_SHADER_DIR}/vert.spv
            ${EPOCH_VULKAN_SHADER_DIR}/frag.spv
        COMMAND ${GLSLANG_VALIDATOR} -V -S vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/vert.glsl
            -o ${EPOCH_VULKAN_SHADER_DIR}/vert.spv
        COMMAND ${GLSLANG_VALIDATOR} -V -S frag
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/frag.glsl
            -o ${EPOCH_VULKAN_SHADER_DIR}/frag.spv
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/vert.glsl
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/frag.glsl
        COMMENT "Compiling Vulkan GLSL shaders to SPIR-V"
    )

    add_custom_target(epoch_vulkan_shaders
        DEPENDS
            ${EPOCH_VULKAN_SHADER_DIR}/vert.spv
            ${EPOCH_VULKAN_SHADER_DIR}/frag.spv
    )

    add_dependencies(epoch epoch_vulkan_shaders)
    add_dependencies(epoch_renderer_smoke epoch_vulkan_shaders)
else()
    message(WARNING "glslangValidator not found; Vulkan SPIR-V shaders will not be generated.")
endif()

epoch_apply_cxx_dialect(epoch)
epoch_apply_warnings(epoch)
epoch_apply_sanitizers(epoch)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 16 requires the Modules TS flag so the dyn-dep scanner emits
    # BMI build rules correctly when paired with Ninja or Makefiles.
    target_compile_options(epoch PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang 18+ works with CMake's standard scanning flow directly; the
    # GCC-specific Modules TS switch is not accepted by clang-scan-deps.
endif()

epoch_apply_cxx_dialect(epoch_renderer_smoke)
epoch_apply_warnings(epoch_renderer_smoke)
epoch_apply_sanitizers(epoch_renderer_smoke)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(epoch_renderer_smoke PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # No extra module flag is needed for Clang here.
endif()

function(epoch_option suffix default_value help_text)
    option(EPOCH_${suffix} "${help_text}" ${default_value})
endfunction()

set(EPOCH_DEFAULT_ENABLE_VULKAN ON)
set(EPOCH_DEFAULT_ENABLE_DIRECTX OFF)
if(WIN32)
    set(EPOCH_DEFAULT_ENABLE_DIRECTX ON)
endif()

set(EPOCH_DEFAULT_ENABLE_RAYLIB ON)

epoch_option(ENABLE_RAYLIB ${EPOCH_DEFAULT_ENABLE_RAYLIB} "Enable the Raylib backend")
epoch_option(ENABLE_SDL ON "Enable the SDL backend")
epoch_option(ENABLE_SFML ON "Enable the SFML backend")
epoch_option(ENABLE_VULKAN ${EPOCH_DEFAULT_ENABLE_VULKAN} "Enable the Vulkan backend")
epoch_option(ENABLE_OPENGL ON "Enable the OpenGL backend")
epoch_option(ENABLE_DIRECTX ${EPOCH_DEFAULT_ENABLE_DIRECTX} "Enable the Windows DirectX/D3D11 preview backend")
epoch_option(ENABLE_SOFTWARE_RENDERER ON "Enable the software renderer backend")
epoch_option(UPDATER_SHELL_BUILD OFF "Build the lightweight updater shell by default")
epoch_option(REQUIRE_OPTIONAL_DEPENDENCIES OFF "Treat missing optional backend dependencies as a configuration error")

set(EPOCH_RAYLIB_ACTIVE ${EPOCH_ENABLE_RAYLIB})
set(EPOCH_SDL_ACTIVE ${EPOCH_ENABLE_SDL})
set(EPOCH_SFML_ACTIVE ${EPOCH_ENABLE_SFML})
set(EPOCH_VULKAN_ACTIVE ${EPOCH_ENABLE_VULKAN})
set(EPOCH_OPENGL_ACTIVE ${EPOCH_ENABLE_OPENGL})
set(EPOCH_DIRECTX_ACTIVE ${EPOCH_ENABLE_DIRECTX})
set(EPOCH_SOFTWARE_RENDERER_ACTIVE ${EPOCH_ENABLE_SOFTWARE_RENDERER})

if(NOT WIN32)
    set(EPOCH_DIRECTX_ACTIVE OFF)
    set(EPOCH_ENABLE_DIRECTX OFF CACHE BOOL "Enable the Windows DirectX/D3D11 preview backend" FORCE)
endif()

if(NOT EPOCH_RAYLIB_ACTIVE)
    list(FILTER EPOCH_SOURCES EXCLUDE REGEX [[src/acontext\.raylib\..*\.cpp$]])
endif()

if(NOT EPOCH_VULKAN_ACTIVE)
    list(FILTER EPOCH_SOURCES EXCLUDE REGEX [[src/acontext\.vulkan\..*\.cpp$]])
endif()

find_package(PkgConfig QUIET)

if(EPOCH_RAYLIB_ACTIVE)
    find_package(raylib CONFIG QUIET)
    if(raylib_FOUND)
        target_link_libraries(epoch PRIVATE raylib)
    else()
        find_library(RAYLIB_LIBRARY NAMES raylib)
        find_path(RAYLIB_INCLUDE_DIR raylib.h PATH_SUFFIXES raylib)
        if(RAYLIB_LIBRARY AND RAYLIB_INCLUDE_DIR)
            target_include_directories(epoch PRIVATE ${RAYLIB_INCLUDE_DIR})
            target_link_libraries(epoch PRIVATE ${RAYLIB_LIBRARY})
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "raylib was not found. Install it via vcpkg, your system package manager, or configure with -DEPOCH_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "raylib was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_RAYLIB_ACTIVE OFF)
                set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
            endif()
        endif()
    endif()

    if(EPOCH_RAYLIB_ACTIVE AND UNIX AND NOT APPLE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(epoch PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(epoch PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DEPOCH_ENABLE_RAYLIB=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(EPOCH_RAYLIB_ACTIVE OFF)
                    set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
                endif()
            endif()
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DEPOCH_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_RAYLIB_ACTIVE OFF)
                set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
            endif()
        endif()
    endif()
endif()

if(EPOCH_SDL_ACTIVE)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        target_link_libraries(epoch PRIVATE SDL3::SDL3)
    else()
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 QUIET sdl3)
            if(SDL3_FOUND)
                target_include_directories(epoch PRIVATE ${SDL3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${SDL3_LIBRARIES})
            else()
                if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SDL=OFF.")
                else()
                    message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DEPOCH_ENABLE_SDL=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(EPOCH_SDL_ACTIVE OFF)
                    set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                    set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SDL" FORCE)
                endif()
            endif()
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SDL=OFF.")
            else()
                message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DEPOCH_ENABLE_SDL=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_SDL_ACTIVE OFF)
                set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                    set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SDL" FORCE)
            endif()
        endif()
    endif()

    if(EPOCH_SDL_ACTIVE)
        find_package(SDL3_image CONFIG QUIET)
        if(SDL3_image_FOUND)
            target_link_libraries(epoch PRIVATE SDL3_image::SDL3_image)
        endif()
    endif()
endif()

if(EPOCH_SFML_ACTIVE)
    find_package(SFML 3.0 COMPONENTS Graphics Window System QUIET)
    if(SFML_FOUND)
        if(TARGET SFML::Graphics)
            target_link_libraries(epoch PRIVATE SFML::Graphics SFML::Window SFML::System)
            set(_epoch_sfml_targets SFML::Graphics SFML::Window SFML::System)
        else()
            target_link_libraries(epoch PRIVATE sfml-graphics sfml-window sfml-system)
            set(_epoch_sfml_targets sfml-graphics sfml-window sfml-system)
        endif()

        if(UNIX AND NOT APPLE)
            foreach(_epoch_sfml_target IN LISTS _epoch_sfml_targets)
                get_target_property(_epoch_sfml_target_type ${_epoch_sfml_target} TYPE)
                if(_epoch_sfml_target_type STREQUAL "STATIC_LIBRARY")
                    message(FATAL_ERROR
                        "Linux multicontext builds require Epoch's x64-linux-epoch vcpkg triplet so "
                        "SFML remains shared and does not collide with Raylib's embedded STB symbols.")
                endif()
            endforeach()

            add_custom_command(TARGET epoch POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:epoch>/lib")
            foreach(_epoch_sfml_target IN LISTS _epoch_sfml_targets)
                add_custom_command(TARGET epoch POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${_epoch_sfml_target}>"
                        "$<TARGET_FILE_DIR:epoch>/lib/$<TARGET_FILE_NAME:${_epoch_sfml_target}>"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_SONAME_FILE:${_epoch_sfml_target}>"
                        "$<TARGET_FILE_DIR:epoch>/lib/$<TARGET_SONAME_FILE_NAME:${_epoch_sfml_target}>")
            endforeach()
        endif()
    elseif(PkgConfig_FOUND)
        pkg_check_modules(SFML QUIET sfml-graphics sfml-window sfml-system)
        if(SFML_FOUND)
            target_include_directories(epoch PRIVATE ${SFML_INCLUDE_DIRS})
            target_link_libraries(epoch PRIVATE ${SFML_LIBRARIES})
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SFML=OFF.")
            else()
                message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DEPOCH_ENABLE_SFML=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_SFML_ACTIVE OFF)
                set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
                    set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SFML" FORCE)
            endif()
        endif()
    else()
        if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
            message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SFML=OFF.")
        else()
            message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DEPOCH_ENABLE_SFML=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
            set(EPOCH_SFML_ACTIVE OFF)
            set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
                    set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SFML" FORCE)
        endif()
    endif()
endif()

if(EPOCH_VULKAN_ACTIVE)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_link_libraries(epoch PRIVATE Vulkan::Vulkan)

        if(UNIX AND NOT APPLE)
            get_filename_component(_epoch_vulkan_library_dir "${Vulkan_LIBRARY}" DIRECTORY)
            file(GLOB _epoch_vulkan_runtime_libraries
                "${_epoch_vulkan_library_dir}/libvulkan.so*")
            if(_epoch_vulkan_runtime_libraries)
                add_custom_command(TARGET epoch POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:epoch>/lib")
                foreach(_epoch_vulkan_runtime_library IN LISTS _epoch_vulkan_runtime_libraries)
                    get_filename_component(_epoch_vulkan_runtime_name
                        "${_epoch_vulkan_runtime_library}" NAME)
                    add_custom_command(TARGET epoch POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_epoch_vulkan_runtime_library}"
                            "$<TARGET_FILE_DIR:epoch>/lib/${_epoch_vulkan_runtime_name}")
                endforeach()
            endif()
        endif()
    else()
        find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1)
        find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
        if(VULKAN_LIBRARY AND VULKAN_INCLUDE_DIR)
            target_include_directories(epoch PRIVATE ${VULKAN_INCLUDE_DIR})
            target_link_libraries(epoch PRIVATE ${VULKAN_LIBRARY})
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "Vulkan was not found. Install it or rerun with -DEPOCH_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "Vulkan was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_VULKAN_ACTIVE OFF)
                set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
            endif()
        endif()
    endif()

    if(EPOCH_VULKAN_ACTIVE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(epoch PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(epoch PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DEPOCH_ENABLE_VULKAN=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(EPOCH_VULKAN_ACTIVE OFF)
                    set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
                endif()
            endif()
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DEPOCH_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(EPOCH_VULKAN_ACTIVE OFF)
                set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
            endif()
        endif()
    endif()

    if(EPOCH_VULKAN_ACTIVE)
        target_sources(epoch PRIVATE
            src/image.stb.cpp
            src/renderers/vulkan/vulkan.platform_context.cpp
            src/renderers/vulkan/vulkan.dispatch_storage.cpp
        )
    endif()
endif()

find_package(OpenGL COMPONENTS OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(epoch PRIVATE OpenGL::GL)
elseif(WIN32)
    target_link_libraries(epoch PRIVATE opengl32)
endif()

if(EPOCH_OPENGL_ACTIVE OR EPOCH_SDL_ACTIVE)
    set(EPOCH_GLAD_NEED_HEADERS ON)
else()
    set(EPOCH_GLAD_NEED_HEADERS OFF)
endif()

set(EPOCH_GLAD_SHOULD_LINK OFF)
if(EPOCH_OPENGL_ACTIVE OR EPOCH_SDL_ACTIVE)
    set(EPOCH_GLAD_SHOULD_LINK ON)
endif()

# The Windows Raylib DLL exports gladLoadGL. Epoch still owns its GLAD data
# table, so the checked-in implementation namespaces that one colliding entry
# point while retaining the normal gladLoadGLLoader API and function table.
set(EPOCH_GLAD_RAYLIB_INTEROP OFF)
if(WIN32 AND EPOCH_RAYLIB_ACTIVE AND EPOCH_GLAD_SHOULD_LINK)
    set(EPOCH_GLAD_RAYLIB_INTEROP ON)
endif()

if(EPOCH_GLAD_NEED_HEADERS)
    set(EPOCH_GLAD_TARGET "")
    set(EPOCH_GLAD_RESOLVED_PROVIDER "")
    set(_epoch_builtin_glad_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)

    if(NOT EPOCH_GLAD_PROVIDER STREQUAL "bundled" AND NOT EPOCH_GLAD_RAYLIB_INTEROP)
        find_package(glad CONFIG QUIET)
        if(TARGET glad::glad)
            set(EPOCH_GLAD_TARGET glad::glad)
            set(EPOCH_GLAD_RESOLVED_PROVIDER "vcpkg")
        elseif(EPOCH_GLAD_PROVIDER STREQUAL "vcpkg")
            message(FATAL_ERROR "EPOCH_GLAD_PROVIDER=vcpkg requires the vcpkg glad::glad target. Use -DEPOCH_GLAD_PROVIDER=bundled for Epoch's checked-in loader.")
        endif()
    endif()

    if(NOT EPOCH_GLAD_TARGET)
        if(EXISTS ${_epoch_builtin_glad_dir}/src/glad.c AND EXISTS ${_epoch_builtin_glad_dir}/include/glad/glad.h)
            if(EPOCH_GLAD_SHOULD_LINK)
                add_library(epoch_builtin_glad STATIC ${_epoch_builtin_glad_dir}/src/glad.c)
                target_include_directories(epoch_builtin_glad PUBLIC ${_epoch_builtin_glad_dir}/include)
                if(EPOCH_GLAD_RAYLIB_INTEROP)
                    target_compile_definitions(epoch_builtin_glad PRIVATE gladLoadGL=epochGladLoadGL)
                    target_compile_definitions(epoch PRIVATE gladLoadGL=epochGladLoadGL)
                    set(EPOCH_GLAD_RESOLVED_PROVIDER "bundled-raylib-compatible")
                else()
                    set(EPOCH_GLAD_RESOLVED_PROVIDER "bundled")
                endif()
                set(EPOCH_GLAD_TARGET epoch_builtin_glad)
            else()
                target_include_directories(epoch PRIVATE ${_epoch_builtin_glad_dir}/include)
                set(EPOCH_GLAD_RESOLVED_PROVIDER "bundled-headers")
            endif()
        else()
            message(FATAL_ERROR "Epoch's bundled GLAD loader was not found. Install vcpkg glad or restore third_party/glad.")
        endif()
    endif()

    if(EPOCH_GLAD_TARGET)
        if(EPOCH_GLAD_SHOULD_LINK)
            target_link_libraries(epoch PRIVATE ${EPOCH_GLAD_TARGET})
        else()
            target_include_directories(epoch PRIVATE $<TARGET_PROPERTY:${EPOCH_GLAD_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
    endif()
endif()
set(EPOCH_ACTIVE_MODULE_FILES ${EPOCH_MODULE_FILES})

function(epoch_exclude_modules list_var regex_pattern)
    set(_epoch_filtered ${${list_var}})
    list(FILTER _epoch_filtered EXCLUDE REGEX "${regex_pattern}")
    set(${list_var} "${_epoch_filtered}" PARENT_SCOPE)
endfunction()

if(NOT EPOCH_RAYLIB_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])raylib\.]])
endif()

if(NOT EPOCH_SDL_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])sdl\.]])
endif()

if(NOT EPOCH_SFML_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])sfml\.]])
endif()

if(NOT EPOCH_VULKAN_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])vulkan\.]])
endif()

if(NOT EPOCH_DIRECTX_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])directx\.]])
endif()

epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])almondshell\.ixx$]])

if(NOT EPOCH_ENABLE_AUTHORING_PLATFORM OR NOT EPOCH_ENABLE_TEXTURE_EDITOR)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])authoring\.texture\.ixx$]])
endif()

if(NOT EPOCH_OPENGL_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])opengl\.]])
endif()

if(NOT EPOCH_SOFTWARE_RENDERER_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])software\.]])
endif()

if(NOT WIN32)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\]).*\.win\.ixx$]])
endif()

if(NOT (UNIX AND NOT APPLE))
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\]).*\.linux\.ixx$]])
endif()

if(NOT EPOCH_RAYLIB_ACTIVE)
    set_source_files_properties(
        src/renderers/raylib/core.context_raylib.cpp
        src/renderers/raylib/raylib.api.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()

if(NOT EPOCH_SDL_ACTIVE)
    set_source_files_properties(
        src/renderers/sdl/core.context_sdl.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()

if(NOT EPOCH_SFML_ACTIVE)
    set_source_files_properties(
        src/renderers/sfml/core.context_sfml.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()

if(NOT EPOCH_DIRECTX_ACTIVE)
    set_source_files_properties(
        src/renderers/directx/core.context_directx.cpp
        src/renderers/directx/directx.context.cpp
        src/renderers/directx/directx.device.cpp
        src/renderers/directx/directx.gui.cpp
        src/renderers/directx/directx.preview.cpp
        src/renderers/directx/directx.state.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
endif()

target_sources(epoch PRIVATE
    FILE_SET epoch_modules TYPE CXX_MODULES
        BASE_DIRS ${EPOCH_MODULE_DIR}
        FILES ${EPOCH_ACTIVE_MODULE_FILES}
)
if(BUILD_TESTING)
    add_executable(epoch_physics_manager_contract
        src/physics/physics.manager.cpp
        src/physics/physics.manager_contract.cpp
    )
    target_sources(epoch_physics_manager_contract PRIVATE
        FILE_SET epoch_physics_manager_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/physics.manager.ixx
    )
    target_include_directories(epoch_physics_manager_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_physics_manager_contract PRIVATE
        EPOCH_PHYSICS_MANAGER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_physics_manager_contract)
    epoch_apply_warnings(epoch_physics_manager_contract)
    epoch_apply_sanitizers(epoch_physics_manager_contract)
    add_test(
        NAME epoch_physics_manager_contract
        COMMAND epoch_physics_manager_contract
    )
    if(EPOCH_ENABLE_AUTHORING_PLATFORM AND EPOCH_ENABLE_TEXTURE_EDITOR)
        add_executable(epoch_authoring_texture_contract
    src/assets/texture/asset.texture_artifact.cpp
            src/authoring/texture/authoring.texture_document.cpp
            src/authoring/texture/authoring.texture_contract.cpp
        )
        target_sources(epoch_authoring_texture_contract PRIVATE
            FILE_SET epoch_authoring_texture_modules TYPE CXX_MODULES
                BASE_DIRS ${EPOCH_MODULE_DIR}
                FILES
                    ${EPOCH_MODULE_DIR}/asset.texture_artifact.ixx
                    ${EPOCH_MODULE_DIR}/authoring.texture.ixx
        )
        target_include_directories(epoch_authoring_texture_contract PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_compile_definitions(epoch_authoring_texture_contract PRIVATE
            EPOCH_AUTHORING_TEXTURE_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_authoring_texture_contract)
        epoch_apply_warnings(epoch_authoring_texture_contract)
        epoch_apply_sanitizers(epoch_authoring_texture_contract)
        add_test(
            NAME epoch_authoring_texture_contract
            COMMAND epoch_authoring_texture_contract
        )
    endif()
    add_test(
        NAME epoch_engine_contract
        COMMAND $<TARGET_FILE:epoch> --engine-contract-self-test
    )
    set_tests_properties(epoch_engine_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch>"
    )
    if(UNIX)
        set_property(TEST epoch_engine_contract PROPERTY
            ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY="
        )
    endif()
endif()

target_compile_definitions(epoch PRIVATE
    $<$<BOOL:${EPOCH_UPDATER_SHELL_BUILD}>:EPOCH_UPDATER_SHELL_BUILD=1>
    EPOCH_ENABLE_AUTHORING_PLATFORM=$<BOOL:${EPOCH_ENABLE_AUTHORING_PLATFORM}>
    EPOCH_ENABLE_TEXTURE_EDITOR=$<BOOL:${EPOCH_ENABLE_TEXTURE_EDITOR}>
    EPOCH_ENABLE_MODEL_EDITOR=$<BOOL:${EPOCH_ENABLE_MODEL_EDITOR}>
    EPOCH_ENABLE_NODE_EDITOR=$<BOOL:${EPOCH_ENABLE_NODE_EDITOR}>
    EPOCH_ENABLE_MATERIAL_EDITOR=$<BOOL:${EPOCH_ENABLE_MATERIAL_EDITOR}>
    EPOCH_ENABLE_ANIMATION_EDITOR=$<BOOL:${EPOCH_ENABLE_ANIMATION_EDITOR}>
    EPOCH_ENABLE_AUTHORING_COLLABORATION=$<BOOL:${EPOCH_ENABLE_AUTHORING_COLLABORATION}>
    EPOCH_ENABLE_AUTHORING_METRICS=$<BOOL:${EPOCH_ENABLE_AUTHORING_METRICS}>
    $<$<BOOL:${EPOCH_RAYLIB_ACTIVE}>:EPOCH_FORCE_ENABLE_RAYLIB>
    $<$<NOT:$<BOOL:${EPOCH_RAYLIB_ACTIVE}>>:EPOCH_FORCE_DISABLE_RAYLIB>
    $<$<BOOL:${EPOCH_SDL_ACTIVE}>:EPOCH_FORCE_ENABLE_SDL>
    $<$<NOT:$<BOOL:${EPOCH_SDL_ACTIVE}>>:EPOCH_FORCE_DISABLE_SDL>
    $<$<BOOL:${EPOCH_SFML_ACTIVE}>:EPOCH_FORCE_ENABLE_SFML>
    $<$<NOT:$<BOOL:${EPOCH_SFML_ACTIVE}>>:EPOCH_FORCE_DISABLE_SFML>
    $<$<BOOL:${EPOCH_VULKAN_ACTIVE}>:EPOCH_FORCE_ENABLE_VULKAN>
    $<$<NOT:$<BOOL:${EPOCH_VULKAN_ACTIVE}>>:EPOCH_FORCE_DISABLE_VULKAN>
    $<$<BOOL:${EPOCH_VULKAN_ACTIVE}>:VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1>
    $<$<BOOL:${EPOCH_VULKAN_ACTIVE}>:VULKAN_HPP_NO_EXCEPTIONS>
    $<$<BOOL:${EPOCH_VULKAN_ACTIVE}>:GLM_FORCE_DEPTH_ZERO_TO_ONE>
    $<$<BOOL:${EPOCH_VULKAN_ACTIVE}>:GLM_FORCE_RADIANS>
    $<$<BOOL:${EPOCH_OPENGL_ACTIVE}>:EPOCH_FORCE_ENABLE_OPENGL>
    $<$<NOT:$<BOOL:${EPOCH_OPENGL_ACTIVE}>>:EPOCH_FORCE_DISABLE_OPENGL>
    $<$<BOOL:${EPOCH_DIRECTX_ACTIVE}>:EPOCH_FORCE_ENABLE_DIRECTX>
    $<$<NOT:$<BOOL:${EPOCH_DIRECTX_ACTIVE}>>:EPOCH_FORCE_DISABLE_DIRECTX>
    $<$<BOOL:${EPOCH_SOFTWARE_RENDERER_ACTIVE}>:EPOCH_FORCE_ENABLE_SOFTWARE_RENDERER>
    $<$<NOT:$<BOOL:${EPOCH_SOFTWARE_RENDERER_ACTIVE}>>:EPOCH_FORCE_DISABLE_SOFTWARE_RENDERER>
)

if(WIN32 AND EPOCH_DIRECTX_ACTIVE)
    target_link_libraries(epoch PRIVATE d3d11 dxgi d3dcompiler)
endif()

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    target_link_libraries(epoch PRIVATE glm::glm)
else()
    find_path(GLM_INCLUDE_DIR glm/glm.hpp)
    if(GLM_INCLUDE_DIR)
        target_include_directories(epoch PRIVATE ${GLM_INCLUDE_DIR})
    elseif(EPOCH_VULKAN_ACTIVE)
        message(FATAL_ERROR "glm headers were not found. Install glm or rerun with -DEPOCH_ENABLE_VULKAN=OFF.")
    endif()
endif()

find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    target_link_libraries(epoch PRIVATE fmt::fmt)
endif()

find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(epoch PRIVATE asio::asio)
else()
    find_path(ASIO_INCLUDE_DIR asio.hpp)
    if(ASIO_INCLUDE_DIR)
        target_include_directories(epoch PRIVATE ${ASIO_INCLUDE_DIR})
    endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(epoch PRIVATE Threads::Threads)

if(NOT WIN32)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(epoch PRIVATE CURL::libcurl)
        target_compile_definitions(epoch PRIVATE EPOCH_HAS_CURL=1)
    else()
        message(WARNING "libcurl not found; non-Windows AI HTTP transport will be disabled.")
    endif()
endif()

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    if(X11_INCLUDE_DIR)
        target_include_directories(epoch PRIVATE ${X11_INCLUDE_DIR})
    endif()
    target_link_libraries(epoch PRIVATE dl m X11::X11 GL)
    if(TARGET X11::Xrandr)
        target_link_libraries(epoch PRIVATE X11::Xrandr)
    elseif(X11_Xrandr_LIB)
        target_link_libraries(epoch PRIVATE ${X11_Xrandr_LIB})
    endif()
endif()

epoch_print_build_summary()

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs/api
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating Epoch API documentation"
        VERBATIM)
else()
    message(STATUS "Doxygen not found; the 'docs' target will be skipped.")
endif()
