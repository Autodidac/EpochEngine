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
option(EPOCH_SOFTWARE_BASE_ONLY "Configure the editor-free CLI/native-window software base without engine or renderer targets." OFF)
option(EPOCH_BUILD_STATIC_RUNTIME "Build Epoch as a reusable static runtime for generated child applications." OFF)
option(EPOCH_BUILD_AI_TRANSPORT_CONTRACT "Expose the opt-in console model HTTP transport probe (never runs automatically)." OFF)
option(EPOCH_ENABLE_AUTHORING_PLATFORM "Gate the shared Epoch authoring-document and editor foundation as implementation units land." ON)
option(EPOCH_ENABLE_TEXTURE_EDITOR "Gate texture painting, compositing, and texture-node authoring implementation units." ON)
option(EPOCH_ENABLE_TILEMAP_EDITOR "Gate tile palette, sparse map authoring, and map compilation implementation units." ON)
option(EPOCH_ENABLE_MODEL_EDITOR "Gate mesh editing, procedural modeling, and sculpt implementation units." ON)
option(EPOCH_ENABLE_NODE_EDITOR "Gate the shared typed node-graph editor and evaluator implementation units." ON)
option(EPOCH_ENABLE_MATERIAL_EDITOR "Gate material graph authoring and preview implementation units." ON)
option(EPOCH_ENABLE_ANIMATION_EDITOR "Gate animation graph and timeline authoring implementation units." ON)
option(EPOCH_ENABLE_AUTHORING_COLLABORATION "Gate branch sharing, review, and collaborative authoring implementation units." OFF)
option(EPOCH_ENABLE_AUTHORING_METRICS "Gate document, history, cache, GPU, and evaluation metrics implementation units." ON)
option(EPOCH_ENABLE_PHYSICAL_AUDIO "Compile the renderer-independent physical audio adapter." ON)
option(EPOCH_ENABLE_PHYSICAL_INPUT "Compile the renderer-independent physical controller adapter." ON)
option(EPOCH_BINARY_ONLY_DISTRIBUTION "Compile Epoch without authorized source download or source-rebuild lanes." OFF)
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
    message(STATUS "  Static child runtime: ${EPOCH_BUILD_STATIC_RUNTIME}")
    message(STATUS "  Authoring platform: ${EPOCH_ENABLE_AUTHORING_PLATFORM}")
    message(STATUS "  Authoring editors: texture=${EPOCH_ENABLE_TEXTURE_EDITOR}; tilemap=${EPOCH_ENABLE_TILEMAP_EDITOR}; model=${EPOCH_ENABLE_MODEL_EDITOR}; node=${EPOCH_ENABLE_NODE_EDITOR}; material=${EPOCH_ENABLE_MATERIAL_EDITOR}; animation=${EPOCH_ENABLE_ANIMATION_EDITOR}")
    message(STATUS "  Authoring collaboration: ${EPOCH_ENABLE_AUTHORING_COLLABORATION}")
    message(STATUS "  Authoring metrics: ${EPOCH_ENABLE_AUTHORING_METRICS}")
    message(STATUS "  Physical audio: ${EPOCH_PHYSICAL_AUDIO_ACTIVE}")
    message(STATUS "  Physical input: ${EPOCH_PHYSICAL_INPUT_ACTIVE}")
    message(STATUS "  Native extensions: OFF (project add-ons compile into projects)")
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
    if(EPOCH_CONFIGURES_MODULE_TARGETS)
        add_executable(epoch_platform_child_process_contract
            src/platform/platform.child_process.cpp
            src/platform/platform.child_process_contract.cpp
        )
        target_sources(epoch_platform_child_process_contract PRIVATE
            FILE_SET epoch_platform_child_process_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform.child_process.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.sha256.ixx
        )
        target_compile_definitions(
            epoch_platform_child_process_contract
            PRIVATE EPOCH_PLATFORM_CHILD_PROCESS_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_platform_child_process_contract)
        epoch_apply_warnings(epoch_platform_child_process_contract)
        epoch_apply_sanitizers(epoch_platform_child_process_contract)
        if(WIN32)
            target_link_libraries(epoch_platform_child_process_contract PRIVATE userenv advapi32 bcrypt ws2_32)
        endif()
        add_test(
            NAME epoch_platform_child_process_contract
            COMMAND epoch_platform_child_process_contract
        )

        add_executable(epoch_editor_candidate_artifact_contract
            src/editor/ai/editor.candidate_artifact_contract.cpp)
        target_sources(epoch_editor_candidate_artifact_contract PRIVATE
            FILE_SET candidate_artifact_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform.child_process.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/editor.candidate_artifact.ixx
        )
        epoch_apply_cxx_dialect(epoch_editor_candidate_artifact_contract)
        epoch_apply_warnings(epoch_editor_candidate_artifact_contract)
        epoch_apply_sanitizers(epoch_editor_candidate_artifact_contract)
        add_test(NAME epoch_editor_candidate_artifact_contract
            COMMAND epoch_editor_candidate_artifact_contract)

        # Filesystem transactions/materialization use synthetic fixtures only;
        # keep this regression available without configuring renderer targets.
        add_executable(epoch_ai_development_executor_contract
            src/ai/ai.development_executor.cpp
            src/ai/ai.development_executor_contract.cpp)
        target_sources(epoch_ai_development_executor_contract PRIVATE
            FILE_SET epoch_ai_development_executor_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.sha256.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform.filesystem.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.development_guard.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.development_executor.ixx)
        epoch_apply_cxx_dialect(epoch_ai_development_executor_contract)
        epoch_apply_warnings(epoch_ai_development_executor_contract)
        epoch_apply_sanitizers(epoch_ai_development_executor_contract)
        add_test(NAME epoch_ai_development_executor_contract
            COMMAND epoch_ai_development_executor_contract)
    endif()
endif()

if(EPOCH_SOFTWARE_BASE_ONLY AND NOT EPOCH_CONFIGURES_MODULE_TARGETS)
    message(FATAL_ERROR "The software base uses a small C++23 module closure; use MSVC or Clang with Ninja/Visual Studio.")
endif()

if(EPOCH_CONFIGURES_MODULE_TARGETS)
    # This target deliberately does not link epoch, epoch_gui, the legacy entry,
    # renderer adapters, editor/project services, updater, AI, or child processes.
    add_library(epoch_software_base STATIC
        src/epoch/epoch.software_application.cpp
        src/platform/platform.window.cpp
        src/core.log.cpp
        src/core.time.cpp
    )
    add_library(Epoch::SoftwareBase ALIAS epoch_software_base)
    target_sources(epoch_software_base PUBLIC
        FILE_SET software_base_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
            FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.error.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.log.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.time.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/context.type.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/context.admission.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform.window.ixx
    )
    target_include_directories(epoch_software_base PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_include_directories(epoch_software_base PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    if(WIN32)
        target_link_libraries(epoch_software_base PRIVATE user32)
    endif()
    find_package(Threads REQUIRED)
    target_link_libraries(epoch_software_base PRIVATE Threads::Threads)
    epoch_apply_cxx_dialect(epoch_software_base)
    epoch_apply_warnings(epoch_software_base)
    epoch_apply_sanitizers(epoch_software_base)

    add_executable(epoch_software_app examples/SoftwareBase/epoch.software_main.cpp)
    target_link_libraries(epoch_software_app PRIVATE Epoch::SoftwareBase)
    set_target_properties(epoch_software_app PROPERTIES OUTPUT_NAME EpochSoftware)
    epoch_apply_cxx_dialect(epoch_software_app)
    epoch_apply_warnings(epoch_software_app)
    epoch_apply_sanitizers(epoch_software_app)

    if(WIN32)
        # Native windows require an explicit operator run. Do not register this
        # executable with CTest or include it in the default build target.
        add_executable(epoch_platform_window_native_contract EXCLUDE_FROM_ALL
            src/platform/platform.window_native_contract.cpp)
        target_link_libraries(epoch_platform_window_native_contract PRIVATE Epoch::SoftwareBase)
        epoch_apply_cxx_dialect(epoch_platform_window_native_contract)
        epoch_apply_warnings(epoch_platform_window_native_contract)
        epoch_apply_sanitizers(epoch_platform_window_native_contract)
    endif()

    if(BUILD_TESTING)
        add_executable(epoch_software_application_contract
            src/epoch/epoch.software_application_contract.cpp)
        target_link_libraries(epoch_software_application_contract PRIVATE Epoch::SoftwareBase)
        epoch_apply_cxx_dialect(epoch_software_application_contract)
        epoch_apply_warnings(epoch_software_application_contract)
        epoch_apply_sanitizers(epoch_software_application_contract)
        add_test(NAME epoch_software_application_contract COMMAND epoch_software_application_contract)
        add_test(NAME epoch_software_cli_smoke COMMAND epoch_software_app --profile cli --ticks 3)
        set_tests_properties(epoch_software_cli_smoke PROPERTIES
            PASS_REGULAR_EXPRESSION "software_application.result=pass profile=cli ticks=3 window_created=0")
        add_test(NAME epoch_software_invalid_profile COMMAND epoch_software_app --profile editor)
        set_tests_properties(epoch_software_invalid_profile PROPERTIES WILL_FAIL TRUE)

        add_executable(epoch_context_admission_contract
            src/renderers/core/context.admission_contract.cpp)
        target_sources(epoch_context_admission_contract PRIVATE
            FILE_SET context_admission_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/context.type.ixx
                    ${CMAKE_CURRENT_SOURCE_DIR}/modules/context.admission.ixx
        )
        epoch_apply_cxx_dialect(epoch_context_admission_contract)
        epoch_apply_warnings(epoch_context_admission_contract)
        epoch_apply_sanitizers(epoch_context_admission_contract)
        add_test(NAME epoch_context_admission_contract COMMAND epoch_context_admission_contract)
    endif()
endif()

if(EPOCH_BUILD_AI_TRANSPORT_CONTRACT)
    # Compile the same production transport implementation with its explicit
    # renderer-free module closure. This target never acquires a dependency on
    # HeadlessCI, the editor entry point, native windows, or renderer probes.
    # MSVC's companion project instead consumes the existing EpochEngine.lib.
    add_executable(epoch_ai_transport_contract EXCLUDE_FROM_ALL
        examples/AiTransportContract/ai.transport_contract.cpp
        src/ai/ai.engine.cpp
        src/ai/ai.development_proposal_codec.cpp
        src/ai/ai.model_install.cpp
        src/ai/ai.project_profile.cpp
        src/core.log.cpp
        src/core.time.cpp
        src/platform/core.path.cpp
    )
    target_sources(epoch_ai_transport_contract PRIVATE
        FILE_SET epoch_ai_transport_modules TYPE CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
            FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.engine.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.runtime.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.session.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.voice_session.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.mcp.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.eval.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.development_proposal_codec.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.model_install.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/ai.project_profile.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.log.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.time.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.path.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/core.sha256.ixx
                ${CMAKE_CURRENT_SOURCE_DIR}/modules/platform.filesystem.ixx
    )
    target_include_directories(epoch_ai_transport_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_ai_transport_contract PRIVATE ENGINE_STATICLIB=1)
    find_package(Threads REQUIRED)
    target_link_libraries(epoch_ai_transport_contract PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(epoch_ai_transport_contract PRIVATE winhttp)
    else()
        find_package(CURL REQUIRED)
        target_link_libraries(epoch_ai_transport_contract PRIVATE CURL::libcurl)
        target_compile_definitions(epoch_ai_transport_contract PRIVATE EPOCH_HAS_CURL=1)
    endif()
    set_target_properties(epoch_ai_transport_contract PROPERTIES OUTPUT_NAME EpochAiTransportContract)
    epoch_apply_cxx_dialect(epoch_ai_transport_contract)
    epoch_apply_warnings(epoch_ai_transport_contract)
    epoch_apply_sanitizers(epoch_ai_transport_contract)
    # Intentionally no add_test(), post-build execution, installation, or
    # default-build dependency: a model request requires explicit CLI --run.
endif()

if(EPOCH_SOFTWARE_BASE_ONLY)
    message(STATUS "Software base only: editor, renderer, GUI, updater and project targets are excluded; the separate AI transport probe is opt-in only.")
    return()
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
    dep/EpochGui/src/epochgui/dock_layout.cpp
    dep/EpochGui/src/epochgui/hierarchy_tree.cpp
    dep/EpochGui/src/epochgui/input.cpp
    dep/EpochGui/src/epochgui/image.cpp
    dep/EpochGui/src/epochgui/rounded_rect.cpp
    dep/EpochGui/src/epochgui/node_graph_workspace.cpp
    dep/EpochGui/src/epochgui/system_workspace.cpp
    src/epochgui/gui.dockable_window.cpp
    src/epochgui/gui.dock_layout.cpp
    src/epochgui/gui.floating_window.cpp
    src/epochgui/gui.panel_host.cpp
    src/epochgui/gui.popup_layout.cpp
    src/epochgui/gui.text_control.cpp
    dep/EpochGui/src/epochgui/text_editor.cpp
    dep/EpochGui/src/epochgui/tile_workspace.cpp
)

target_sources(epoch_gui PUBLIC
    FILE_SET epoch_gui_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.input.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.image.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.tile_workspace.ixx
            ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/modules/epoch.gui.rounded_rect.ixx
)

target_include_directories(epoch_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/include
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
    src/updater/updater.device_identity.cpp
    src/updater/updater.source_access.cpp
    src/updater/updater.system.cpp
    src/updater/updater.source_worker_win.cpp
    src/epoch/epoch.build_validation.cpp
    src/epoch/epoch.build_validation_contract.cpp
    src/editor/editor.code_workspace.cpp
    src/editor/editor.code_workspace_contract.cpp
    src/ai/ai.development_executor.cpp
    src/ai/ai.iteration_loop.cpp
    src/ai/ai.iteration_loop_contract.cpp
    src/ai/ai.iteration_session.cpp
    src/ai/ai.iteration_session_contract.cpp
    src/ai/ai.project_profile.cpp
    src/ai/ai.project_profile_contract.cpp
    src/ai/ai.model_install.cpp
    src/ai/ai.model_install_contract.cpp
    src/ai/ai.curated_context_bundle.cpp
    src/ai/ai.curated_context_bundle_contract.cpp
    src/ai/ai.iteration_campaign.cpp
    src/ai/ai.iteration_campaign_contract.cpp
    src/ai/ai.iteration_campaign_queue.cpp
    src/ai/ai.iteration_campaign_queue_contract.cpp
    src/ai/ai.iteration_campaign_scheduler.cpp
    src/ai/ai.iteration_campaign_scheduler_contract.cpp
    src/ai/ai.iteration_supervisor_control.cpp
    src/ai/ai.iteration_supervisor_control_contract.cpp
    src/ai/ai.mcp_supervisor_adapter.cpp
    src/ai/ai.mcp_supervisor_adapter_checkpoint.cpp
    src/ai/ai.mcp_supervisor_adapter_contract.cpp
    src/ai/ai.mcp_campaign.cpp
    src/ai/ai.mcp_campaign_contract.cpp
    src/ai/ai.mcp_child_host.cpp
    src/ai/ai.mcp_child_host_contract.cpp
    src/ai/ai.self_iteration_orchestrator.cpp
    src/ai/ai.self_iteration_orchestrator_contract.cpp
    src/ai/ai.mcp_orchestrator_bridge.cpp
    src/ai/ai.mcp_orchestrator_bridge_contract.cpp
    src/ai/ai.source_patch_bundle.cpp
    src/ai/ai.source_patch_bundle_contract.cpp
    src/ai/ai.source_patch_proposal.cpp
    src/ai/ai.source_patch_proposal_contract.cpp
    src/ai/ai.source_patch_stager.cpp
    src/ai/ai.source_patch_stager_contract.cpp
    src/ai/ai.iteration_patch_adapter.cpp
    src/ai/ai.iteration_patch_adapter_contract.cpp
    src/ai/ai.iteration_validation_adapter.cpp
    src/ai/ai.iteration_validation_adapter_contract.cpp
    src/ai/ai.development_proposal_codec.cpp
    src/ai/ai.development_proposal_codec_contract.cpp
    src/ai/ai.mcp_stdio.cpp
    src/ai/ai.mcp_stdio_contract.cpp
    src/extensions/extension.catalog_contract.cpp
    src/packages/package.catalog.cpp
    src/audio/audio.device.cpp
    src/audio/audio.device_contract.cpp
    src/audio/audio.manager.cpp
    src/audio/audio.mixer.cpp
    src/audio/audio.mixer_contract.cpp
    src/audio/audio.playback_runtime.cpp
    src/audio/audio.playback_runtime_contract.cpp
    src/input/input.controller.cpp
    src/input/input.controller_sdl.cpp
    src/input/input.controller_contract.cpp
    src/assets/audio/asset.audio_import.cpp
    src/assets/audio/asset.audio_import_contract.cpp
    src/project/project.audio_profile.cpp
    src/project/project.audio_profile_contract.cpp
    src/physics/physics.manager.cpp
    src/physics/physics.solver2d.cpp
    src/physics/physics.solver2d_contract.cpp
    src/project/project.sprite_animation.cpp
    src/project/project.sprite_animation_contract.cpp
    src/render/render.portal_contract.cpp
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
    src/renderers/opengl/opengl.frame_capture.cpp
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
    src/editor/editor.hierarchy_adapter.cpp
    src/editor/editor.hierarchy_adapter_contract.cpp
    src/editor/editor.project_textures.cpp
    src/editor/editor.project_input_settings.cpp
    src/editor/editor.project_input_settings_contract.cpp
    src/editor/editor.tilemap_workspace.cpp
    src/editor/ai/editor.ai_development_controller.cpp
    src/editor/ai/editor.ai_development_panel.cpp
    src/editor/ai/editor.ai_development_controller_contract.cpp
    src/editor/systems/editor.systems_workspace.cpp
    src/editor/systems/editor.systems_panel.cpp
    src/editor/systems/editor.systems_workspace_contract.cpp
    src/authoring/gui/authoring.gui_document_codec.cpp
    src/authoring/gui/authoring.gui_templates.cpp
    src/authoring/gui/authoring.gui_document_contract.cpp
    src/authoring/gui/authoring.gui_compiler.cpp
    src/authoring/gui/authoring.gui_compiler_contract.cpp
    src/authoring/task_graph/authoring.task_graph_contract.cpp
    src/renderers/canvas2d/render.canvas2d_scene.cpp
    src/renderers/canvas2d/render.canvas2d_evidence.cpp
    src/renderers/canvas2d/render.canvas2d_scene_contract.cpp
    src/renderers/canvas2d/canvas2d.scene_contracts.cpp
    src/renderers/canvas2d/render.canvas2d_tilemap.cpp
    src/renderers/canvas2d/render.canvas2d_tilemap_contract.cpp
    src/assets/tilemap/asset.tilemap_artifact.cpp
    src/assets/gui/asset.gui_artifact.cpp
    src/project/project.gui_runtime.cpp
    src/project/project.gui_runtime_contract.cpp
    src/project/project.gui_library.cpp
    src/project/project.gui_library_contract.cpp
    src/project/project.gui_epochgui.cpp
    src/project/project.tilemap_library.cpp
    src/project/project.tilemap_pipeline.cpp
    src/project/project.tilemap_runtime.cpp
    src/project/project.tilemap_runtime_contract.cpp
    src/project/project.tilemap_source.cpp
    src/project/project.input_profile.cpp
    src/project/project.input_profile_contract.cpp
    src/project/project.ai_self_iteration.cpp
    src/project/project.ai_self_iteration_session.cpp
    src/project/project.input_controller.cpp
    src/project/project.input_controller_contract.cpp
    src/project/project.actor2d_runtime.cpp
    src/project/project.actor2d_runtime_contract.cpp
    src/project/project.gameplay2d_acceptance.cpp
    src/project/project.gameplay2d_runtime.cpp
    src/project/project.gameplay2d_runtime_contract.cpp
    src/utility.filewatch.cpp
    src/font.renderer.cpp
    src/project/project.asset_registry.cpp
    src/project/project.forest_library.cpp
    src/project/project.texture_resources.cpp
    src/project/project.texture_library.cpp
    src/project/project.texture_pipeline.cpp
    src/project/project.texture_source.cpp
    src/project/project.contracts.cpp
    src/project/project.lifecycle_contract.cpp
    src/assets/texture/asset.texture_artifact.cpp
    src/assets/texture/asset.texture_import.cpp
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
    src/platform/platform.child_process.cpp
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
if(EPOCH_ENABLE_AUTHORING_PLATFORM AND EPOCH_ENABLE_TILEMAP_EDITOR)
    list(APPEND EPOCH_SOURCES
        src/authoring/tilemap/authoring.tilemap_document.cpp
    )
endif()


if(EPOCH_BUILD_STATIC_RUNTIME)
    add_library(epoch STATIC ${EPOCH_SOURCES})
    target_compile_definitions(epoch PRIVATE
        ENGINE_STATICLIB=1
        EPOCH_MAIN_IN_MAIN_CPP=1)
    set_target_properties(epoch PROPERTIES
        OUTPUT_NAME EpochRuntime
        POSITION_INDEPENDENT_CODE ON)
else()
    add_executable(epoch ${EPOCH_SOURCES})
endif()

if(EPOCH_BUILD_STATIC_RUNTIME)
    # The child executable owns its runtime output and executable-local assets.
elseif(WIN32)
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
        src/editor/editor.application.cpp
        PROPERTIES
            COMPILE_OPTIONS /bigobj;/Zm2000;/Od;/Ob0;/GL-
    )

    set_source_files_properties(
        src/epoch.engine_legacy.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000"
    )

    # The Windows source worker emits a large generated script. Keep that
    # implementation unit out of whole-program/inlining work so the module
    # interface and ordinary updater orchestration remain lean.
    set_source_files_properties(
        src/updater/updater.source_access.cpp
    src/updater/updater.system.cpp
    src/updater/updater.source_worker_win.cpp
        PROPERTIES
            COMPILE_OPTIONS "/bigobj;/Zm2000;/Od;/Ob0;/GL-"
    )

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
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/include
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
set(EPOCH_LOCAL_MCP_ASSET_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/ai/epoch.local_mcp_iteration.ps1)
set(EPOCH_LOCAL_MCP_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets/ai)

file(MAKE_DIRECTORY ${EPOCH_VULKAN_SHADER_DIR})
file(MAKE_DIRECTORY ${EPOCH_FONT_ASSET_DIR})
file(MAKE_DIRECTORY ${EPOCH_LOCAL_MCP_ASSET_DIR})
configure_file(
    ${EPOCH_LOCAL_MCP_ASSET_SOURCE}
    ${EPOCH_LOCAL_MCP_ASSET_DIR}/epoch.local_mcp_iteration.ps1
    COPYONLY
)
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
set(EPOCH_PHYSICAL_AUDIO_ACTIVE ${EPOCH_ENABLE_PHYSICAL_AUDIO})
set(EPOCH_PHYSICAL_INPUT_ACTIVE ${EPOCH_ENABLE_PHYSICAL_INPUT})
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

set(EPOCH_SDL3_AVAILABLE OFF)
if(EPOCH_SDL_ACTIVE OR EPOCH_PHYSICAL_AUDIO_ACTIVE OR EPOCH_PHYSICAL_INPUT_ACTIVE)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        target_link_libraries(epoch PRIVATE SDL3::SDL3)
        set(EPOCH_SDL3_AVAILABLE ON)
    else()
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 QUIET sdl3)
            if(SDL3_FOUND)
                target_include_directories(epoch PRIVATE ${SDL3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${SDL3_LIBRARIES})
                set(EPOCH_SDL3_AVAILABLE ON)
            else()
                if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or disable the requesting SDL renderer, physical audio, and physical input features.")
                else()
                    message(WARNING "SDL3 was not found; the SDL backend, physical audio, and physical input adapters will be disabled when requested.")
                    set(EPOCH_SDL_ACTIVE OFF)
                    set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                    set(EPOCH_PHYSICAL_AUDIO_ACTIVE OFF)
                    set(EPOCH_ENABLE_PHYSICAL_AUDIO OFF CACHE BOOL "Compile the renderer-independent physical audio adapter." FORCE)
                    set(EPOCH_PHYSICAL_INPUT_ACTIVE OFF)
                    set(EPOCH_ENABLE_PHYSICAL_INPUT OFF CACHE BOOL "Compile the renderer-independent physical controller adapter." FORCE)
                endif()
            endif()
        else()
            if(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or disable the requesting SDL renderer, physical audio, and physical input features.")
            else()
                message(WARNING "SDL3 was not found; the SDL backend, physical audio, and physical input adapters will be disabled when requested.")
                set(EPOCH_SDL_ACTIVE OFF)
                set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                set(EPOCH_PHYSICAL_AUDIO_ACTIVE OFF)
                set(EPOCH_ENABLE_PHYSICAL_AUDIO OFF CACHE BOOL "Compile the renderer-independent physical audio adapter." FORCE)
                set(EPOCH_PHYSICAL_INPUT_ACTIVE OFF)
                set(EPOCH_ENABLE_PHYSICAL_INPUT OFF CACHE BOOL "Compile the renderer-independent physical controller adapter." FORCE)
            endif()
        endif()
    endif()

    if(EPOCH_SDL_ACTIVE AND EPOCH_SDL3_AVAILABLE)
        find_package(SDL3_image CONFIG QUIET)
        if(SDL3_image_FOUND)
            target_link_libraries(epoch PRIVATE SDL3_image::SDL3_image)
        endif()
    endif()

    if(EPOCH_PHYSICAL_AUDIO_ACTIVE AND EPOCH_SDL3_AVAILABLE)
        target_sources(epoch PRIVATE src/audio/audio.device_sdl.cpp)
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

if(NOT EPOCH_PHYSICAL_AUDIO_ACTIVE)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])audio\.device_sdl\.ixx$]])
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

epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])extension\.plugin\.ixx$]])

if(NOT EPOCH_ENABLE_AUTHORING_PLATFORM OR NOT EPOCH_ENABLE_TILEMAP_EDITOR)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])authoring\.tilemap\.ixx$]])
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])project\.tilemap_source\.ixx$]])
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])editor\.tilemap_workspace\.ixx$]])
    set_source_files_properties(src/editor/editor.tilemap_workspace.cpp src/project/project.tilemap_source.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE)
endif()

epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])almondshell\.ixx$]])

if(NOT EPOCH_ENABLE_AUTHORING_PLATFORM OR NOT EPOCH_ENABLE_TEXTURE_EDITOR)
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[(^|[/\\])authoring\.texture\.ixx$]])
    epoch_exclude_modules(EPOCH_ACTIVE_MODULE_FILES [[project[.]texture_source[.]ixx$]])
    set_source_files_properties(src/project/project.texture_source.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE)
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
    add_executable(epoch_input_controller_contract
        src/input/input.controller.cpp
        src/input/input.controller_contract.cpp
    )
    target_sources(epoch_input_controller_contract PRIVATE
        FILE_SET epoch_input_controller_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/input.controller.ixx
    )
    target_include_directories(epoch_input_controller_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_input_controller_contract PRIVATE
        EPOCH_INPUT_CONTROLLER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_input_controller_contract)
    epoch_apply_warnings(epoch_input_controller_contract)
    epoch_apply_sanitizers(epoch_input_controller_contract)
    add_test(
        NAME epoch_input_controller_contract
        COMMAND epoch_input_controller_contract
    )

    add_executable(epoch_audio_mixer_contract
        src/audio/audio.manager.cpp
        src/audio/audio.mixer.cpp
        src/audio/audio.mixer_contract.cpp
    )
    target_sources(epoch_audio_mixer_contract PRIVATE
        FILE_SET epoch_audio_mixer_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/audio.types.ixx
                ${EPOCH_MODULE_DIR}/audio.manager.ixx
                ${EPOCH_MODULE_DIR}/audio.mixer.ixx
    )
    target_include_directories(epoch_audio_mixer_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_audio_mixer_contract PRIVATE
        EPOCH_AUDIO_MIXER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_audio_mixer_contract)
    epoch_apply_warnings(epoch_audio_mixer_contract)
    epoch_apply_sanitizers(epoch_audio_mixer_contract)
    add_test(
        NAME epoch_audio_mixer_contract
        COMMAND epoch_audio_mixer_contract
    )

    add_executable(epoch_audio_playback_runtime_contract
        src/audio/audio.manager.cpp
        src/audio/audio.mixer.cpp
        src/audio/audio.device.cpp
        src/audio/audio.playback_runtime.cpp
        src/audio/audio.playback_runtime_contract.cpp
    )
    target_sources(epoch_audio_playback_runtime_contract PRIVATE
        FILE_SET epoch_audio_playback_runtime_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/audio.types.ixx
                ${EPOCH_MODULE_DIR}/audio.manager.ixx
                ${EPOCH_MODULE_DIR}/audio.mixer.ixx
                ${EPOCH_MODULE_DIR}/audio.device.ixx
                ${EPOCH_MODULE_DIR}/audio.playback_runtime.ixx
    )
    target_include_directories(epoch_audio_playback_runtime_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_audio_playback_runtime_contract PRIVATE
        EPOCH_AUDIO_PLAYBACK_RUNTIME_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_audio_playback_runtime_contract)
    epoch_apply_warnings(epoch_audio_playback_runtime_contract)
    epoch_apply_sanitizers(epoch_audio_playback_runtime_contract)
    add_test(
        NAME epoch_audio_playback_runtime_contract
        COMMAND epoch_audio_playback_runtime_contract
    )

    add_executable(epoch_audio_import_contract
        src/assets/audio/asset.audio_import.cpp
        src/assets/audio/asset.audio_import_contract.cpp
    )
    target_sources(epoch_audio_import_contract PRIVATE
        FILE_SET epoch_audio_import_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/audio.types.ixx
                ${EPOCH_MODULE_DIR}/audio.manager.ixx
                ${EPOCH_MODULE_DIR}/audio.mixer.ixx
                ${EPOCH_MODULE_DIR}/asset.audio_import.ixx
    )
    target_include_directories(epoch_audio_import_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_audio_import_contract PRIVATE
        EPOCH_AUDIO_IMPORT_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_audio_import_contract)
    epoch_apply_warnings(epoch_audio_import_contract)
    epoch_apply_sanitizers(epoch_audio_import_contract)
    add_test(
        NAME epoch_audio_import_contract
        COMMAND epoch_audio_import_contract
    )

    add_executable(epoch_project_audio_profile_contract
        src/audio/audio.manager.cpp
        src/audio/audio.mixer.cpp
        src/audio/audio.device.cpp
        src/audio/audio.playback_runtime.cpp
        src/assets/audio/asset.audio_import.cpp
        src/project/project.audio_profile.cpp
        src/project/project.audio_profile_contract.cpp
    )
    target_sources(epoch_project_audio_profile_contract PRIVATE
        FILE_SET epoch_project_audio_profile_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/audio.types.ixx
                ${EPOCH_MODULE_DIR}/audio.manager.ixx
                ${EPOCH_MODULE_DIR}/audio.mixer.ixx
                ${EPOCH_MODULE_DIR}/audio.device.ixx
                ${EPOCH_MODULE_DIR}/audio.playback_runtime.ixx
                ${EPOCH_MODULE_DIR}/asset.audio_import.ixx
                ${EPOCH_MODULE_DIR}/project.audio_profile.ixx
    )
    target_include_directories(epoch_project_audio_profile_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_audio_profile_contract PRIVATE
        EPOCH_PROJECT_AUDIO_PROFILE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_audio_profile_contract)
    epoch_apply_warnings(epoch_project_audio_profile_contract)
    epoch_apply_sanitizers(epoch_project_audio_profile_contract)
    add_test(
        NAME epoch_project_audio_profile_contract
        COMMAND epoch_project_audio_profile_contract
    )
    add_executable(epoch_audio_device_contract
        src/audio/audio.device.cpp
        src/audio/audio.device_contract.cpp
    )
    target_sources(epoch_audio_device_contract PRIVATE
        FILE_SET epoch_audio_device_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/audio.device.ixx
    )
    target_include_directories(epoch_audio_device_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_audio_device_contract PRIVATE
        EPOCH_AUDIO_DEVICE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_audio_device_contract)
    epoch_apply_warnings(epoch_audio_device_contract)
    epoch_apply_sanitizers(epoch_audio_device_contract)
    add_test(
        NAME epoch_audio_device_contract
        COMMAND epoch_audio_device_contract
    )
    add_executable(epoch_scene_surface_alignment_contract
        src/scene/scene.surface_alignment_contract.cpp
    )
    target_sources(epoch_scene_surface_alignment_contract PRIVATE
        FILE_SET epoch_scene_surface_alignment_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/scene.surface_alignment.ixx
    )
    target_compile_definitions(epoch_scene_surface_alignment_contract PRIVATE
        EPOCH_SCENE_SURFACE_ALIGNMENT_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_scene_surface_alignment_contract)
    epoch_apply_warnings(epoch_scene_surface_alignment_contract)
    epoch_apply_sanitizers(epoch_scene_surface_alignment_contract)
    add_test(
        NAME epoch_scene_surface_alignment_contract
        COMMAND epoch_scene_surface_alignment_contract
    )

   add_executable(epoch_media_timeline_preview_contract
       src/media/media.timeline_preview_contract.cpp
   )
    target_sources(epoch_media_timeline_preview_contract PRIVATE
        FILE_SET epoch_media_timeline_preview_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/media.timeline_preview.ixx
    )
    target_compile_definitions(epoch_media_timeline_preview_contract PRIVATE
        EPOCH_MEDIA_TIMELINE_PREVIEW_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_media_timeline_preview_contract)
    epoch_apply_warnings(epoch_media_timeline_preview_contract)
    epoch_apply_sanitizers(epoch_media_timeline_preview_contract)
   add_test(
       NAME epoch_media_timeline_preview_contract
       COMMAND epoch_media_timeline_preview_contract
   )
    add_executable(epoch_forest_preview_mesh_contract
        src/forest/forest.preview_mesh_contract.cpp
    )
    target_sources(epoch_forest_preview_mesh_contract PRIVATE
        FILE_SET epoch_forest_preview_mesh_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/authoring.document.ixx
                ${EPOCH_MODULE_DIR}/voxel.field.ixx
                ${EPOCH_MODULE_DIR}/authoring.morphology.ixx
                ${EPOCH_MODULE_DIR}/forest.preview_mesh.ixx
    )
    target_compile_definitions(epoch_forest_preview_mesh_contract PRIVATE
        EPOCH_FOREST_PREVIEW_MESH_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_forest_preview_mesh_contract)
    epoch_apply_warnings(epoch_forest_preview_mesh_contract)
    epoch_apply_sanitizers(epoch_forest_preview_mesh_contract)
    add_test(
        NAME epoch_forest_preview_mesh_contract
        COMMAND epoch_forest_preview_mesh_contract
    )

    add_executable(epoch_ai_model_install_contract
        src/ai/ai.model_install.cpp
        src/ai/ai.model_install_contract.cpp
    )
    target_sources(epoch_ai_model_install_contract PRIVATE
        FILE_SET epoch_ai_model_install_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/ai.model_install.ixx
    )
    target_compile_definitions(epoch_ai_model_install_contract PRIVATE
        EPOCH_AI_MODEL_INSTALL_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_model_install_contract)
    epoch_apply_warnings(epoch_ai_model_install_contract)
    epoch_apply_sanitizers(epoch_ai_model_install_contract)
    add_test(
        NAME epoch_ai_model_install_contract
        COMMAND epoch_ai_model_install_contract
    )

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

    add_executable(epoch_physics_solver2d_contract
        src/physics/physics.manager.cpp
        src/physics/physics.solver2d.cpp
        src/physics/physics.solver2d_contract.cpp
    )
    target_sources(epoch_physics_solver2d_contract PRIVATE
        FILE_SET epoch_physics_solver2d_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/physics.manager.ixx
                ${EPOCH_MODULE_DIR}/physics.solver2d.ixx
    )
    target_include_directories(epoch_physics_solver2d_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_physics_solver2d_contract PRIVATE
        EPOCH_PHYSICS_SOLVER2D_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_physics_solver2d_contract)
    epoch_apply_warnings(epoch_physics_solver2d_contract)
    epoch_apply_sanitizers(epoch_physics_solver2d_contract)
    add_test(
        NAME epoch_physics_solver2d_contract
        COMMAND epoch_physics_solver2d_contract
    )

    add_executable(epoch_project_sprite_animation_contract
        src/project/project.asset_registry.cpp
        src/project/project.sprite_animation.cpp
        src/project/project.sprite_animation_contract.cpp
    )
    target_sources(epoch_project_sprite_animation_contract PRIVATE
        FILE_SET epoch_project_sprite_animation_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/asset.texture_artifact.ixx
                ${EPOCH_MODULE_DIR}/authoring.document.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/project.sprite_animation.ixx
    )
    target_include_directories(epoch_project_sprite_animation_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_sprite_animation_contract PRIVATE
        EPOCH_PROJECT_SPRITE_ANIMATION_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_sprite_animation_contract)
    epoch_apply_warnings(epoch_project_sprite_animation_contract)
    epoch_apply_sanitizers(epoch_project_sprite_animation_contract)
    add_test(
        NAME epoch_project_sprite_animation_contract
        COMMAND epoch_project_sprite_animation_contract
    )
    add_executable(epoch_project_input_profile_contract
        src/project/project.asset_registry.cpp
        src/project/project.input_profile.cpp
        src/project/project.input_profile_contract.cpp
    )
    target_sources(epoch_project_input_profile_contract PRIVATE
        FILE_SET epoch_project_input_profile_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/project.input_profile.ixx
    )
    target_include_directories(epoch_project_input_profile_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_input_profile_contract PRIVATE
        EPOCH_PROJECT_INPUT_PROFILE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_input_profile_contract)
    epoch_apply_warnings(epoch_project_input_profile_contract)
    epoch_apply_sanitizers(epoch_project_input_profile_contract)
    add_test(
        NAME epoch_project_input_profile_contract
        COMMAND epoch_project_input_profile_contract
    )

    add_executable(epoch_project_input_controller_contract
        src/input/input.controller.cpp
        src/project/project.asset_registry.cpp
        src/project/project.input_profile.cpp
        src/project/project.input_controller.cpp
        src/project/project.input_controller_contract.cpp
    )
    target_sources(epoch_project_input_controller_contract PRIVATE
        FILE_SET epoch_project_input_controller_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/input.controller.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/project.input_profile.ixx
                ${EPOCH_MODULE_DIR}/project.input_controller.ixx
    )
    target_include_directories(epoch_project_input_controller_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_input_controller_contract PRIVATE
        EPOCH_PROJECT_INPUT_CONTROLLER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_input_controller_contract)
    epoch_apply_warnings(epoch_project_input_controller_contract)
    epoch_apply_sanitizers(epoch_project_input_controller_contract)
    add_test(
        NAME epoch_project_input_controller_contract
        COMMAND epoch_project_input_controller_contract
    )

    add_executable(epoch_editor_project_input_settings_contract
        src/input/input.controller.cpp
        src/project/project.asset_registry.cpp
        src/project/project.input_profile.cpp
        src/project/project.input_controller.cpp
        src/editor/editor.project_input_settings.cpp
        src/editor/editor.project_input_settings_contract.cpp
    )
    target_sources(epoch_editor_project_input_settings_contract PRIVATE
        FILE_SET epoch_editor_project_input_settings_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/input.controller.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/project.input_profile.ixx
                ${EPOCH_MODULE_DIR}/project.input_controller.ixx
                ${EPOCH_MODULE_DIR}/editor.project_input_settings.ixx
    )
    target_include_directories(epoch_editor_project_input_settings_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_editor_project_input_settings_contract PRIVATE
        EPOCH_EDITOR_PROJECT_INPUT_SETTINGS_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_editor_project_input_settings_contract)
    epoch_apply_warnings(epoch_editor_project_input_settings_contract)
    epoch_apply_sanitizers(epoch_editor_project_input_settings_contract)
    add_test(
        NAME epoch_editor_project_input_settings_contract
        COMMAND epoch_editor_project_input_settings_contract
    )

    add_executable(epoch_editor_hierarchy_adapter_contract
        dep/EpochGui/src/epochgui/hierarchy_tree.cpp
        src/editor/editor.hierarchy_adapter.cpp
        src/editor/editor.hierarchy_adapter_contract.cpp
    )
    target_sources(epoch_editor_hierarchy_adapter_contract PRIVATE
        FILE_SET epoch_editor_hierarchy_adapter_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/editor.hierarchy_adapter.ixx
    )
    target_include_directories(epoch_editor_hierarchy_adapter_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/dep/EpochGui/include
    )
    target_compile_definitions(epoch_editor_hierarchy_adapter_contract PRIVATE
        EPOCH_EDITOR_HIERARCHY_ADAPTER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_editor_hierarchy_adapter_contract)
    epoch_apply_warnings(epoch_editor_hierarchy_adapter_contract)
    epoch_apply_sanitizers(epoch_editor_hierarchy_adapter_contract)
    add_test(
        NAME epoch_editor_hierarchy_adapter_contract
        COMMAND epoch_editor_hierarchy_adapter_contract
    )

    add_executable(epoch_project_actor2d_contract
        src/physics/physics.manager.cpp
        src/physics/physics.solver2d.cpp
        src/project/project.actor2d_runtime.cpp
        src/project/project.actor2d_runtime_contract.cpp
    )
    target_sources(epoch_project_actor2d_contract PRIVATE
        FILE_SET epoch_project_actor2d_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/physics.manager.ixx
                ${EPOCH_MODULE_DIR}/physics.solver2d.ixx
                ${EPOCH_MODULE_DIR}/project.actor2d_runtime.ixx
    )
    target_include_directories(epoch_project_actor2d_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_actor2d_contract PRIVATE
        EPOCH_PROJECT_ACTOR2D_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_actor2d_contract)
    epoch_apply_warnings(epoch_project_actor2d_contract)
    epoch_apply_sanitizers(epoch_project_actor2d_contract)
    add_test(
        NAME epoch_project_actor2d_contract
        COMMAND epoch_project_actor2d_contract
    )
    add_executable(epoch_project_gui_library_contract
        src/assets/gui/asset.gui_artifact.cpp
        src/project/project.gui_library.cpp
        src/project/project.gui_library_contract.cpp
    )
    target_sources(epoch_project_gui_library_contract PRIVATE
        FILE_SET epoch_project_gui_library_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/project.asset_registry.ixx
                ${EPOCH_MODULE_DIR}/asset.gui_artifact.ixx
                ${EPOCH_MODULE_DIR}/project.gui_library.ixx
    )
    target_include_directories(epoch_project_gui_library_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_gui_library_contract PRIVATE
        EPOCH_PROJECT_GUI_LIBRARY_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_gui_library_contract)
    epoch_apply_warnings(epoch_project_gui_library_contract)
    epoch_apply_sanitizers(epoch_project_gui_library_contract)
    add_test(
        NAME epoch_project_gui_library_contract
        COMMAND epoch_project_gui_library_contract
    )

    add_executable(epoch_project_gui_runtime_contract
        src/assets/gui/asset.gui_artifact.cpp
        src/project/project.gui_runtime.cpp
        src/project/project.gui_runtime_contract.cpp
    )
    target_sources(epoch_project_gui_runtime_contract PRIVATE
        FILE_SET epoch_project_gui_runtime_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/asset.gui_artifact.ixx
                ${EPOCH_MODULE_DIR}/project.gui_runtime.ixx
    )
    target_include_directories(epoch_project_gui_runtime_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(epoch_project_gui_runtime_contract PRIVATE
        EPOCH_PROJECT_GUI_RUNTIME_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_project_gui_runtime_contract)
    epoch_apply_warnings(epoch_project_gui_runtime_contract)
    epoch_apply_sanitizers(epoch_project_gui_runtime_contract)
    add_test(
        NAME epoch_project_gui_runtime_contract
        COMMAND epoch_project_gui_runtime_contract
    )

    if(EPOCH_ENABLE_AUTHORING_PLATFORM)
        add_executable(epoch_authoring_gui_document_contract
            src/authoring/gui/authoring.gui_document_codec.cpp
            src/authoring/gui/authoring.gui_templates.cpp
            src/authoring/gui/authoring.gui_document_contract.cpp
        )
        target_sources(epoch_authoring_gui_document_contract PRIVATE
            FILE_SET epoch_authoring_gui_document_modules TYPE CXX_MODULES
                BASE_DIRS ${EPOCH_MODULE_DIR}
                FILES
                    ${EPOCH_MODULE_DIR}/core.sha256.ixx
                    ${EPOCH_MODULE_DIR}/authoring.document.ixx
                    ${EPOCH_MODULE_DIR}/authoring.gui_document.ixx
        )
        target_include_directories(epoch_authoring_gui_document_contract PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_compile_definitions(epoch_authoring_gui_document_contract PRIVATE
            EPOCH_AUTHORING_GUI_DOCUMENT_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_authoring_gui_document_contract)
        epoch_apply_warnings(epoch_authoring_gui_document_contract)
        epoch_apply_sanitizers(epoch_authoring_gui_document_contract)
        add_test(
            NAME epoch_authoring_gui_document_contract
            COMMAND epoch_authoring_gui_document_contract
        )

        add_executable(epoch_authoring_gui_compiler_contract
            src/assets/gui/asset.gui_artifact.cpp
            src/authoring/gui/authoring.gui_document_codec.cpp
            src/authoring/gui/authoring.gui_templates.cpp
            src/authoring/gui/authoring.gui_compiler.cpp
            src/authoring/gui/authoring.gui_compiler_contract.cpp
        )
        target_sources(epoch_authoring_gui_compiler_contract PRIVATE
            FILE_SET epoch_authoring_gui_compiler_modules TYPE CXX_MODULES
                BASE_DIRS ${EPOCH_MODULE_DIR}
                FILES
                    ${EPOCH_MODULE_DIR}/core.sha256.ixx
                    ${EPOCH_MODULE_DIR}/authoring.document.ixx
                    ${EPOCH_MODULE_DIR}/asset.gui_artifact.ixx
                    ${EPOCH_MODULE_DIR}/authoring.gui_document.ixx
                    ${EPOCH_MODULE_DIR}/authoring.gui_compiler.ixx
        )
        target_include_directories(epoch_authoring_gui_compiler_contract PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_compile_definitions(epoch_authoring_gui_compiler_contract PRIVATE
            EPOCH_AUTHORING_GUI_COMPILER_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_authoring_gui_compiler_contract)
        epoch_apply_warnings(epoch_authoring_gui_compiler_contract)
        epoch_apply_sanitizers(epoch_authoring_gui_compiler_contract)
        add_test(
            NAME epoch_authoring_gui_compiler_contract
            COMMAND epoch_authoring_gui_compiler_contract
        )
    endif()
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
    if(EPOCH_ENABLE_AUTHORING_PLATFORM AND EPOCH_ENABLE_TILEMAP_EDITOR)
        add_executable(epoch_authoring_tilemap_contract
            src/assets/tilemap/asset.tilemap_artifact.cpp
            src/authoring/tilemap/authoring.tilemap_document.cpp
            src/authoring/tilemap/authoring.tilemap_contract.cpp
        )
        target_sources(epoch_authoring_tilemap_contract PRIVATE
            FILE_SET epoch_authoring_tilemap_modules TYPE CXX_MODULES
                BASE_DIRS ${EPOCH_MODULE_DIR}
                FILES
                    ${EPOCH_MODULE_DIR}/authoring.document.ixx
                    ${EPOCH_MODULE_DIR}/asset.tilemap_artifact.ixx
                    ${EPOCH_MODULE_DIR}/authoring.tilemap.ixx
        )
        target_include_directories(epoch_authoring_tilemap_contract PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_compile_definitions(epoch_authoring_tilemap_contract PRIVATE
            EPOCH_AUTHORING_TILEMAP_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_authoring_tilemap_contract)
        epoch_apply_warnings(epoch_authoring_tilemap_contract)
        epoch_apply_sanitizers(epoch_authoring_tilemap_contract)
        add_test(
            NAME epoch_authoring_tilemap_contract
            COMMAND epoch_authoring_tilemap_contract
        )
    endif()

    add_executable(epoch_render_context_frame_contract
        src/renderers/core/render.context_frame_contract.cpp
    )
    target_sources(epoch_render_context_frame_contract PRIVATE
        FILE_SET epoch_render_context_frame_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/render.context_frame.ixx
    )
    epoch_apply_cxx_dialect(epoch_render_context_frame_contract)
    epoch_apply_warnings(epoch_render_context_frame_contract)
    epoch_apply_sanitizers(epoch_render_context_frame_contract)
    add_test(
        NAME epoch_render_context_frame_contract
        COMMAND epoch_render_context_frame_contract
    )

    add_executable(epoch_ai_iteration_campaign_queue_contract
        src/ai/ai.iteration_campaign_queue.cpp
        src/ai/ai.iteration_campaign_queue_contract.cpp
    )
    target_sources(epoch_ai_iteration_campaign_queue_contract PRIVATE
        FILE_SET epoch_ai_iteration_campaign_queue_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_queue.ixx
    )
    target_compile_definitions(epoch_ai_iteration_campaign_queue_contract PRIVATE
        EPOCH_AI_ITERATION_CAMPAIGN_QUEUE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_iteration_campaign_queue_contract)
    epoch_apply_warnings(epoch_ai_iteration_campaign_queue_contract)
    epoch_apply_sanitizers(epoch_ai_iteration_campaign_queue_contract)
    add_test(
        NAME epoch_ai_iteration_campaign_queue_contract
        COMMAND epoch_ai_iteration_campaign_queue_contract
    )

    add_executable(epoch_ai_mcp_stdio_contract
        src/ai/ai.mcp_stdio.cpp
        src/ai/ai.mcp_stdio_contract.cpp
    )
    target_sources(epoch_ai_mcp_stdio_contract PRIVATE
        FILE_SET epoch_ai_mcp_stdio_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/ai.mcp.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_stdio.ixx
    )
    target_compile_definitions(epoch_ai_mcp_stdio_contract PRIVATE
        EPOCH_AI_MCP_STDIO_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_mcp_stdio_contract)
    epoch_apply_warnings(epoch_ai_mcp_stdio_contract)
    epoch_apply_sanitizers(epoch_ai_mcp_stdio_contract)
    add_test(
        NAME epoch_ai_mcp_stdio_contract
        COMMAND epoch_ai_mcp_stdio_contract
    )

    add_executable(epoch_ai_iteration_campaign_scheduler_contract
        src/ai/ai.iteration_loop.cpp
        src/ai/ai.iteration_session.cpp
        src/ai/ai.project_profile.cpp
        src/ai/ai.iteration_campaign.cpp
        src/ai/ai.iteration_campaign_queue.cpp
        src/ai/ai.mcp_stdio.cpp
        src/ai/ai.mcp_campaign.cpp
        src/ai/ai.self_iteration_orchestrator.cpp
        src/ai/ai.mcp_orchestrator_bridge.cpp
        src/ai/ai.iteration_campaign_scheduler.cpp
        src/ai/ai.iteration_campaign_scheduler_contract.cpp
    )
    target_sources(epoch_ai_iteration_campaign_scheduler_contract PRIVATE
        FILE_SET epoch_ai_iteration_campaign_scheduler_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_loop.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_session.ixx
                ${EPOCH_MODULE_DIR}/ai.project_profile.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_queue.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_stdio.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_child_host.ixx
                ${EPOCH_MODULE_DIR}/ai.self_iteration_orchestrator.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_orchestrator_bridge.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_scheduler.ixx
    )
    target_compile_definitions(epoch_ai_iteration_campaign_scheduler_contract PRIVATE
        EPOCH_AI_ITERATION_CAMPAIGN_SCHEDULER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_iteration_campaign_scheduler_contract)
    epoch_apply_warnings(epoch_ai_iteration_campaign_scheduler_contract)
    epoch_apply_sanitizers(epoch_ai_iteration_campaign_scheduler_contract)
    add_test(
        NAME epoch_ai_iteration_campaign_scheduler_contract
        COMMAND epoch_ai_iteration_campaign_scheduler_contract
    )

    add_executable(epoch_ai_iteration_supervisor_control_contract
        src/ai/ai.iteration_loop.cpp
        src/ai/ai.iteration_session.cpp
        src/ai/ai.project_profile.cpp
        src/ai/ai.iteration_campaign.cpp
        src/ai/ai.iteration_campaign_queue.cpp
        src/ai/ai.mcp_stdio.cpp
        src/ai/ai.mcp_campaign.cpp
        src/ai/ai.self_iteration_orchestrator.cpp
        src/ai/ai.mcp_orchestrator_bridge.cpp
        src/ai/ai.iteration_campaign_scheduler.cpp
        src/ai/ai.iteration_supervisor_control.cpp
        src/ai/ai.iteration_supervisor_control_contract.cpp
    )
    target_sources(epoch_ai_iteration_supervisor_control_contract PRIVATE
        FILE_SET epoch_ai_iteration_supervisor_control_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_loop.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_session.ixx
                ${EPOCH_MODULE_DIR}/ai.project_profile.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_queue.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_stdio.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_child_host.ixx
                ${EPOCH_MODULE_DIR}/ai.self_iteration_orchestrator.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_orchestrator_bridge.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_scheduler.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_supervisor_control.ixx
    )
    target_compile_definitions(epoch_ai_iteration_supervisor_control_contract PRIVATE
        EPOCH_AI_ITERATION_SUPERVISOR_CONTROL_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_iteration_supervisor_control_contract)
    epoch_apply_warnings(epoch_ai_iteration_supervisor_control_contract)
    epoch_apply_sanitizers(epoch_ai_iteration_supervisor_control_contract)
    add_test(
        NAME epoch_ai_iteration_supervisor_control_contract
        COMMAND epoch_ai_iteration_supervisor_control_contract
    )

    add_executable(epoch_ai_curated_context_bundle_contract
        src/ai/ai.curated_context_bundle.cpp
        src/ai/ai.curated_context_bundle_contract.cpp
    )
    target_sources(epoch_ai_curated_context_bundle_contract PRIVATE
        FILE_SET epoch_ai_curated_context_bundle_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/ai.project_profile.ixx
                ${EPOCH_MODULE_DIR}/ai.curated_context_bundle.ixx
    )
    target_compile_definitions(epoch_ai_curated_context_bundle_contract PRIVATE
        EPOCH_AI_CURATED_CONTEXT_BUNDLE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_curated_context_bundle_contract)
    epoch_apply_warnings(epoch_ai_curated_context_bundle_contract)
    epoch_apply_sanitizers(epoch_ai_curated_context_bundle_contract)
    add_test(
        NAME epoch_ai_curated_context_bundle_contract
        COMMAND epoch_ai_curated_context_bundle_contract
    )

    add_executable(epoch_ai_source_patch_proposal_contract
        src/ai/ai.source_patch_proposal.cpp
        src/ai/ai.source_patch_proposal_contract.cpp
    )
    target_sources(epoch_ai_source_patch_proposal_contract PRIVATE
        FILE_SET epoch_ai_source_patch_proposal_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/ai.source_patch_proposal.ixx
    )
    target_compile_definitions(epoch_ai_source_patch_proposal_contract PRIVATE
        EPOCH_AI_SOURCE_PATCH_PROPOSAL_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_source_patch_proposal_contract)
    epoch_apply_warnings(epoch_ai_source_patch_proposal_contract)
    epoch_apply_sanitizers(epoch_ai_source_patch_proposal_contract)
    add_test(
        NAME epoch_ai_source_patch_proposal_contract
        COMMAND epoch_ai_source_patch_proposal_contract
    )

    add_executable(epoch_ai_source_patch_stager_contract
        src/epoch/epoch.build_validation.cpp
        src/ai/ai.curated_context_bundle.cpp
        src/ai/ai.source_patch_proposal.cpp
        src/ai/ai.source_patch_stager.cpp
        src/ai/ai.source_patch_stager_contract.cpp
    )
    target_sources(epoch_ai_source_patch_stager_contract PRIVATE
        FILE_SET epoch_ai_source_patch_stager_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_loop.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_session.ixx
                ${EPOCH_MODULE_DIR}/ai.project_profile.ixx
                ${EPOCH_MODULE_DIR}/ai.curated_context_bundle.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_queue.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_stdio.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_campaign.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_child_host.ixx
                ${EPOCH_MODULE_DIR}/ai.self_iteration_orchestrator.ixx
                ${EPOCH_MODULE_DIR}/ai.mcp_orchestrator_bridge.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_campaign_scheduler.ixx
                ${EPOCH_MODULE_DIR}/ai.iteration_supervisor_control.ixx
                ${EPOCH_MODULE_DIR}/ai.source_patch_bundle.ixx
                ${EPOCH_MODULE_DIR}/ai.source_patch_proposal.ixx
                ${EPOCH_MODULE_DIR}/epoch.build_validation.ixx
                ${EPOCH_MODULE_DIR}/ai.source_patch_stager.ixx
    )
    target_compile_definitions(epoch_ai_source_patch_stager_contract PRIVATE
        EPOCH_AI_SOURCE_PATCH_STAGER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_source_patch_stager_contract)
    epoch_apply_warnings(epoch_ai_source_patch_stager_contract)
    epoch_apply_sanitizers(epoch_ai_source_patch_stager_contract)
    add_test(
        NAME epoch_ai_source_patch_stager_contract
        COMMAND epoch_ai_source_patch_stager_contract
    )

    add_executable(epoch_ai_voice_session_contract
        src/ai/ai.voice_session_contract.cpp
    )
    target_sources(epoch_ai_voice_session_contract PRIVATE
        FILE_SET epoch_ai_voice_session_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/ai.voice_session.ixx
    )
    epoch_apply_cxx_dialect(epoch_ai_voice_session_contract)
    epoch_apply_warnings(epoch_ai_voice_session_contract)
    epoch_apply_sanitizers(epoch_ai_voice_session_contract)
    add_test(
        NAME epoch_ai_voice_session_contract
        COMMAND epoch_ai_voice_session_contract
    )
    add_executable(epoch_ai_development_guard_contract
        src/ai/ai.development_guard_contract.cpp
    )
    target_sources(epoch_ai_development_guard_contract PRIVATE
        FILE_SET epoch_ai_development_guard_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/ai.development_guard.ixx
    )
    epoch_apply_cxx_dialect(epoch_ai_development_guard_contract)
    epoch_apply_warnings(epoch_ai_development_guard_contract)
    epoch_apply_sanitizers(epoch_ai_development_guard_contract)
    add_test(
        NAME epoch_ai_development_guard_contract
        COMMAND epoch_ai_development_guard_contract
    )


    add_executable(epoch_ai_development_proposal_codec_contract
        src/ai/ai.development_proposal_codec.cpp
        src/ai/ai.development_proposal_codec_contract.cpp
    )
    target_sources(epoch_ai_development_proposal_codec_contract PRIVATE
        FILE_SET epoch_ai_development_proposal_codec_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/ai.development_proposal_codec.ixx
    )
    target_compile_definitions(epoch_ai_development_proposal_codec_contract PRIVATE
        EPOCH_AI_DEVELOPMENT_PROPOSAL_CODEC_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_ai_development_proposal_codec_contract)
    epoch_apply_warnings(epoch_ai_development_proposal_codec_contract)
    epoch_apply_sanitizers(epoch_ai_development_proposal_codec_contract)
    add_test(
        NAME epoch_ai_development_proposal_codec_contract
        COMMAND epoch_ai_development_proposal_codec_contract
    )

    add_executable(epoch_editor_ai_development_controller_contract
        src/ai/ai.development_executor.cpp
        src/ai/ai.development_proposal_codec.cpp
        src/editor/ai/editor.ai_development_controller.cpp
        src/editor/ai/editor.ai_development_controller_contract.cpp
    )
    target_sources(epoch_editor_ai_development_controller_contract PRIVATE
        FILE_SET epoch_editor_ai_development_controller_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/ai.development_guard.ixx
                ${EPOCH_MODULE_DIR}/ai.development_executor.ixx
                ${EPOCH_MODULE_DIR}/ai.development_proposal_codec.ixx
                ${EPOCH_MODULE_DIR}/editor.ai_development_controller.ixx
    )
    target_include_directories(epoch_editor_ai_development_controller_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(epoch_editor_ai_development_controller_contract PRIVATE
        EPOCH_EDITOR_AI_DEVELOPMENT_CONTROLLER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_editor_ai_development_controller_contract)
    epoch_apply_warnings(epoch_editor_ai_development_controller_contract)
    epoch_apply_sanitizers(epoch_editor_ai_development_controller_contract)
    add_test(
        NAME epoch_editor_ai_development_controller_contract
        COMMAND epoch_editor_ai_development_controller_contract
    )

    add_executable(epoch_script_compiler_contract
        src/core.log.cpp
        src/core.time.cpp
        src/platform/core.path.cpp
        src/platform/platform.child_process.cpp
        src/scripts/scripting.compiler_contract.cpp
    )
    target_sources(epoch_script_compiler_contract PRIVATE
        FILE_SET epoch_script_compiler_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.log.ixx
                ${EPOCH_MODULE_DIR}/core.time.ixx
                ${EPOCH_MODULE_DIR}/core.logger.ixx
                ${EPOCH_MODULE_DIR}/core.path.ixx
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/platform.child_process.ixx
                ${EPOCH_MODULE_DIR}/core.sha256.ixx
                ${EPOCH_MODULE_DIR}/scripting.compiler.ixx
    )
    target_include_directories(epoch_script_compiler_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(epoch_script_compiler_contract PRIVATE
        EPOCH_SCRIPT_COMPILER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_script_compiler_contract)
    epoch_apply_warnings(epoch_script_compiler_contract)
    epoch_apply_sanitizers(epoch_script_compiler_contract)
    if(WIN32)
        target_link_libraries(epoch_script_compiler_contract PRIVATE userenv advapi32 bcrypt)
    endif()
    add_test(
        NAME epoch_script_compiler_contract
        COMMAND epoch_script_compiler_contract ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_executable(epoch_gui_node_graph_workspace_contract
        dep/EpochGui/tests/node_graph_workspace_tests.cpp
    )
    target_link_libraries(epoch_gui_node_graph_workspace_contract PRIVATE epoch_gui)
    epoch_apply_cxx_dialect(epoch_gui_node_graph_workspace_contract)
    epoch_apply_warnings(epoch_gui_node_graph_workspace_contract)
    epoch_apply_sanitizers(epoch_gui_node_graph_workspace_contract)
    add_test(
        NAME epoch_gui_node_graph_workspace_contract
        COMMAND epoch_gui_node_graph_workspace_contract
    )

    add_executable(epoch_gui_dock_layout_contract
        dep/EpochGui/tests/dock_layout_tests.cpp
    )
    target_link_libraries(epoch_gui_dock_layout_contract PRIVATE epoch_gui)
    target_compile_definitions(epoch_gui_dock_layout_contract PRIVATE
        EPOCHGUI_TEST_MODULE_IMPORT=1
    )
    epoch_apply_cxx_dialect(epoch_gui_dock_layout_contract)
    epoch_apply_warnings(epoch_gui_dock_layout_contract)
    epoch_apply_sanitizers(epoch_gui_dock_layout_contract)
    add_test(
        NAME epoch_gui_dock_layout_contract
        COMMAND epoch_gui_dock_layout_contract
    )

    add_executable(epoch_gui_system_workspace_contract
        dep/EpochGui/tests/system_workspace_tests.cpp
    )
    target_link_libraries(epoch_gui_system_workspace_contract PRIVATE epoch_gui)
    epoch_apply_cxx_dialect(epoch_gui_system_workspace_contract)
    epoch_apply_warnings(epoch_gui_system_workspace_contract)
    epoch_apply_sanitizers(epoch_gui_system_workspace_contract)
    add_test(
        NAME epoch_gui_system_workspace_contract
        COMMAND epoch_gui_system_workspace_contract
    )

    add_executable(epoch_editor_workspace_layout_contract
        src/editor/editor.workspace_layout_contract.cpp
    )
    target_sources(epoch_editor_workspace_layout_contract PRIVATE
        FILE_SET epoch_editor_workspace_layout_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/editor.workspace_layout.ixx
    )
    epoch_apply_cxx_dialect(epoch_editor_workspace_layout_contract)
    epoch_apply_warnings(epoch_editor_workspace_layout_contract)
    epoch_apply_sanitizers(epoch_editor_workspace_layout_contract)
    add_test(
        NAME epoch_editor_workspace_layout_contract
        COMMAND epoch_editor_workspace_layout_contract
    )

    add_executable(epoch_build_validation_contract
        src/epoch/epoch.build_validation.cpp
        src/epoch/epoch.build_validation_contract.cpp
    )
    target_sources(epoch_build_validation_contract PRIVATE
        FILE_SET epoch_build_validation_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/epoch.build_validation.ixx
    )
    target_compile_definitions(epoch_build_validation_contract PRIVATE
        EPOCH_BUILD_VALIDATION_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_build_validation_contract)
    epoch_apply_warnings(epoch_build_validation_contract)
    epoch_apply_sanitizers(epoch_build_validation_contract)
    add_test(
        NAME epoch_build_validation_contract
        COMMAND epoch_build_validation_contract
    )

    add_executable(epoch_editor_code_workspace_contract
        src/editor/editor.code_workspace.cpp
        src/editor/editor.code_workspace_contract.cpp
    )
    target_sources(epoch_editor_code_workspace_contract PRIVATE
        FILE_SET epoch_editor_code_workspace_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/platform.filesystem.ixx
                ${EPOCH_MODULE_DIR}/editor.code_workspace.ixx
    )
    target_compile_definitions(epoch_editor_code_workspace_contract PRIVATE
        EPOCH_EDITOR_CODE_WORKSPACE_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_editor_code_workspace_contract)
    epoch_apply_warnings(epoch_editor_code_workspace_contract)
    epoch_apply_sanitizers(epoch_editor_code_workspace_contract)
    add_test(
        NAME epoch_editor_code_workspace_contract
        COMMAND epoch_editor_code_workspace_contract
    )

    add_executable(epoch_editor_workspace_commands_contract
        src/editor/editor.workspace_commands_contract.cpp
    )
    target_sources(epoch_editor_workspace_commands_contract PRIVATE
        FILE_SET epoch_editor_workspace_commands_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/editor.workspace_commands.ixx
    )
    epoch_apply_cxx_dialect(epoch_editor_workspace_commands_contract)
    epoch_apply_warnings(epoch_editor_workspace_commands_contract)
    epoch_apply_sanitizers(epoch_editor_workspace_commands_contract)
    add_test(
        NAME epoch_editor_workspace_commands_contract
        COMMAND epoch_editor_workspace_commands_contract
    )

    add_executable(epoch_editor_authoring_history_contract
        src/editor/editor.authoring_history_contract.cpp
    )
    target_sources(epoch_editor_authoring_history_contract PRIVATE
        FILE_SET epoch_editor_authoring_history_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES ${EPOCH_MODULE_DIR}/editor.authoring_history.ixx
    )
    epoch_apply_cxx_dialect(epoch_editor_authoring_history_contract)
    epoch_apply_warnings(epoch_editor_authoring_history_contract)
    epoch_apply_sanitizers(epoch_editor_authoring_history_contract)
    add_test(
        NAME epoch_editor_authoring_history_contract
        COMMAND epoch_editor_authoring_history_contract
    )

    add_executable(epoch_editor_task_scheduler_contract
        src/core.log.cpp
        src/core.time.cpp
        src/editor/systems/editor.task_scheduler_contract.cpp
    )
    target_sources(epoch_editor_task_scheduler_contract PRIVATE
        FILE_SET epoch_editor_task_scheduler_modules TYPE CXX_MODULES
            BASE_DIRS ${EPOCH_MODULE_DIR}
            FILES
                ${EPOCH_MODULE_DIR}/core.log.ixx
                ${EPOCH_MODULE_DIR}/core.time.ixx
                ${EPOCH_MODULE_DIR}/core.logger.ixx
                ${EPOCH_MODULE_DIR}/core.mpmc_bounded_queue.ixx
                ${EPOCH_MODULE_DIR}/platform.engine.ixx
                ${EPOCH_MODULE_DIR}/systems.registry.ixx
                ${EPOCH_MODULE_DIR}/systems.task.ixx
                ${EPOCH_MODULE_DIR}/taskgraph.dotsystem.ixx
                ${EPOCH_MODULE_DIR}/editor.task_scheduler.ixx
    )
    target_include_directories(epoch_editor_task_scheduler_contract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(epoch_editor_task_scheduler_contract PRIVATE
        EPOCH_EDITOR_TASK_SCHEDULER_CONTRACT_MAIN=1
    )
    epoch_apply_cxx_dialect(epoch_editor_task_scheduler_contract)
    epoch_apply_warnings(epoch_editor_task_scheduler_contract)
    epoch_apply_sanitizers(epoch_editor_task_scheduler_contract)
    add_test(
        NAME epoch_editor_task_scheduler_contract
        COMMAND epoch_editor_task_scheduler_contract
    )

    if(EPOCH_ENABLE_AUTHORING_PLATFORM AND EPOCH_ENABLE_NODE_EDITOR)
        add_executable(epoch_authoring_task_graph_contract
            src/authoring/task_graph/authoring.task_graph_contract.cpp
        )
        target_sources(epoch_authoring_task_graph_contract PRIVATE
            FILE_SET epoch_authoring_task_graph_modules TYPE CXX_MODULES
                BASE_DIRS ${EPOCH_MODULE_DIR}
                FILES
                    ${EPOCH_MODULE_DIR}/authoring.document.ixx
                    ${EPOCH_MODULE_DIR}/authoring.task_graph.ixx
        )
        target_compile_definitions(epoch_authoring_task_graph_contract PRIVATE
            EPOCH_AUTHORING_TASK_GRAPH_CONTRACT_MAIN=1
        )
        epoch_apply_cxx_dialect(epoch_authoring_task_graph_contract)
        epoch_apply_warnings(epoch_authoring_task_graph_contract)
        epoch_apply_sanitizers(epoch_authoring_task_graph_contract)
        add_test(
            NAME epoch_authoring_task_graph_contract
            COMMAND epoch_authoring_task_graph_contract
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
    EPOCH_BINARY_ONLY_DISTRIBUTION=$<BOOL:${EPOCH_BINARY_ONLY_DISTRIBUTION}>
    EPOCH_ENABLE_AUTHORING_PLATFORM=$<BOOL:${EPOCH_ENABLE_AUTHORING_PLATFORM}>
    EPOCH_ENABLE_TEXTURE_EDITOR=$<BOOL:${EPOCH_ENABLE_TEXTURE_EDITOR}>
    EPOCH_ENABLE_TILEMAP_EDITOR=$<BOOL:${EPOCH_ENABLE_TILEMAP_EDITOR}>
    EPOCH_ENABLE_MODEL_EDITOR=$<BOOL:${EPOCH_ENABLE_MODEL_EDITOR}>
    EPOCH_ENABLE_NODE_EDITOR=$<BOOL:${EPOCH_ENABLE_NODE_EDITOR}>
    EPOCH_ENABLE_MATERIAL_EDITOR=$<BOOL:${EPOCH_ENABLE_MATERIAL_EDITOR}>
    EPOCH_ENABLE_ANIMATION_EDITOR=$<BOOL:${EPOCH_ENABLE_ANIMATION_EDITOR}>
    EPOCH_ENABLE_AUTHORING_COLLABORATION=$<BOOL:${EPOCH_ENABLE_AUTHORING_COLLABORATION}>
    EPOCH_ENABLE_AUTHORING_METRICS=$<BOOL:${EPOCH_ENABLE_AUTHORING_METRICS}>
    EPOCH_ENABLE_PHYSICAL_AUDIO=$<BOOL:${EPOCH_PHYSICAL_AUDIO_ACTIVE}>
    EPOCH_ENABLE_PHYSICAL_INPUT=$<BOOL:${EPOCH_PHYSICAL_INPUT_ACTIVE}>
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

if(WIN32)
    target_link_libraries(epoch PRIVATE comdlg32 ncrypt userenv advapi32 bcrypt)
endif()

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

find_package(OpenSSL REQUIRED COMPONENTS Crypto)
target_link_libraries(epoch PRIVATE OpenSSL::Crypto)

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
    # vcpkg's native Linux triplet contributes a find root for manifest
    # packages. X11 and OpenGL remain host integration libraries, so allow
    # CMake to search both that root and the host while resolving them.
    set(_EPOCH_FIND_ROOT_LIBRARY_MODE_WAS_DEFINED FALSE)
    if(DEFINED CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
        set(_EPOCH_FIND_ROOT_LIBRARY_MODE_WAS_DEFINED TRUE)
        set(_EPOCH_SAVED_FIND_ROOT_LIBRARY_MODE "${CMAKE_FIND_ROOT_PATH_MODE_LIBRARY}")
    endif()
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    if(_EPOCH_FIND_ROOT_LIBRARY_MODE_WAS_DEFINED)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "${_EPOCH_SAVED_FIND_ROOT_LIBRARY_MODE}")
    else()
        unset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY)
    endif()
    unset(_EPOCH_SAVED_FIND_ROOT_LIBRARY_MODE)
    unset(_EPOCH_FIND_ROOT_LIBRARY_MODE_WAS_DEFINED)

    if(X11_INCLUDE_DIR)
        target_include_directories(epoch PRIVATE ${X11_INCLUDE_DIR})
    endif()
    target_link_libraries(epoch PRIVATE dl m X11::X11 OpenGL::GL)
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
