cmake_minimum_required(VERSION 3.28)

if(CMAKE_VERSION VERSION_LESS 3.29)
    # CMake 3.27–3.28 require the experimental dyndep scanner to emit BMI rules.
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
endif()

# Keep module scanning enabled across all supported generators/toolchains.
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(AlmondShell LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ALMONDSHELL_SOURCES
        src/aengine.cpp
    src/aengine.context.cpp
    src/aengine.gui.cpp
    src/aengine.scripting.compiler.cpp
    src/aeditor.cpp
    src/aeditor.scene.cpp
    src/afilewatch.cpp
    src/afont.renderer.cpp
    src/acontext.raylib.bridge.cpp
    src/acontext.vulkan.context-texture.cpp
    src/acontext.vulkan.context_buffers.cpp
    src/acontext.vulkan.dispatch_storage.cpp
    src/acontext.vulkan.platform.context.cpp
    src/acontext.vulkan.platform.descriptors.cpp
    src/aengine.context.multiplexer.linux.cpp
    src/aengine.context.multiplexer.win.cpp
    src/core.assert.cpp
    src/core.env.cpp
    src/core.error.cpp
    src/core.id.cpp
    src/core.log.cpp
    src/core.math.cpp
    src/core.path.cpp
    src/core.string.cpp
    src/core.time.cpp
    src/epoch.ai.cpp
    src/epoch.engine.cpp
    src/epoch.headers.cpp
    src/epoch.platform.context.cpp
    src/epoch.platform.window.cpp
    src/epoch.render.graph.cpp
    src/epoch.systems.cpp
    src/main.cpp
    src/renderer_smoke_harness.cpp
    src/runtime.cpp
)


if(WIN32)
    list(APPEND ALMONDSHELL_SOURCES
        src/aengine.context.multiplexer.win.cpp
        src/WinResource.rc
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND ALMONDSHELL_SOURCES
        src/aengine.context.multiplexer.linux.cpp
    )
endif()

add_executable(almondshell ${ALMONDSHELL_SOURCES})

add_executable(almondshell_renderer_smoke
    "src modules/renderer_smoke_harness.cppm"
)

set(ALMONDSHELL_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)
file(GLOB ALMONDSHELL_MODULE_FILES ${ALMONDSHELL_MODULE_DIR}/*.ixx)
list(SORT ALMONDSHELL_MODULE_FILES)

set(ALMONDSHELL_MODULE_HEADERS
    ${ALMONDSHELL_MODULE_DIR}/almond.core.time.hpp
)

target_sources(almondshell PRIVATE
    FILE_SET almondshell_modules TYPE CXX_MODULES
        BASE_DIRS ${ALMONDSHELL_MODULE_DIR}
        FILES ${ALMONDSHELL_MODULE_FILES}
    FILE_SET almondshell_module_headers TYPE HEADERS
        BASE_DIRS ${ALMONDSHELL_MODULE_DIR}
        FILES ${ALMONDSHELL_MODULE_HEADERS}
)

target_include_directories(almondshell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

target_include_directories(almondshell_renderer_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${CMAKE_CURRENT_SOURCE_DIR}/src modules"
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

find_program(GLSLANG_VALIDATOR glslangValidator)
set(ALMOND_VULKAN_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets/vulkan)
set(ALMOND_VULKAN_SHADER_DIR ${ALMOND_VULKAN_ASSET_DIR}/shaders)

file(MAKE_DIRECTORY ${ALMOND_VULKAN_SHADER_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/texture.ppm
    ${ALMOND_VULKAN_ASSET_DIR}/texture.ppm
    COPYONLY
)

if(GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT
            ${ALMOND_VULKAN_SHADER_DIR}/vert.spv
            ${ALMOND_VULKAN_SHADER_DIR}/frag.spv
        COMMAND ${GLSLANG_VALIDATOR} -V -S vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/vert.glsl
            -o ${ALMOND_VULKAN_SHADER_DIR}/vert.spv
        COMMAND ${GLSLANG_VALIDATOR} -V -S frag
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/frag.glsl
            -o ${ALMOND_VULKAN_SHADER_DIR}/frag.spv
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/vert.glsl
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/shaders/frag.glsl
        COMMENT "Compiling Vulkan GLSL shaders to SPIR-V"
    )

    add_custom_target(almond_vulkan_shaders
        DEPENDS
            ${ALMOND_VULKAN_SHADER_DIR}/vert.spv
            ${ALMOND_VULKAN_SHADER_DIR}/frag.spv
    )

    add_dependencies(almondshell almond_vulkan_shaders)
    add_dependencies(almondshell_renderer_smoke almond_vulkan_shaders)
else()
    message(WARNING "glslangValidator not found; Vulkan SPIR-V shaders will not be generated.")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Modern MSVC releases provide standard module support without the
    # deprecated /experimental:module switch; keep the latest standard
    # mode enabled so BMI emission stays aligned with scan-for-modules.
    target_compile_options(almondshell PRIVATE /std:c++latest)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 14+ requires the Modules TS flag so the dyn-dep scanner emits
    # BMI build rules correctly when paired with Ninja or Makefiles.
    target_compile_options(almondshell PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang builds rely on the Modules TS flag to keep BMI generation
    # consistent across CMake presets and helper scripts.
    target_compile_options(almondshell PRIVATE -fmodules-ts)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(almondshell_renderer_smoke PRIVATE /std:c++latest)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(almondshell_renderer_smoke PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(almondshell_renderer_smoke PRIVATE -fmodules-ts)
endif()

option(ALMOND_ENABLE_RAYLIB "Enable the Raylib backend" ON)
option(ALMOND_ENABLE_SDL "Enable the SDL backend" ON)
option(ALMOND_ENABLE_SFML "Enable the SFML backend" ON)
option(ALMOND_ENABLE_VULKAN "Enable the Vulkan backend" ON)
option(ALMOND_ENABLE_OPENGL "Enable the OpenGL backend" ON)
option(ALMOND_ENABLE_SOFTWARE_RENDERER "Enable the software renderer backend" ON)
option(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES "Treat missing optional backend dependencies as a configuration error" OFF)

set(ALMOND_RAYLIB_ACTIVE ${ALMOND_ENABLE_RAYLIB})
set(ALMOND_SDL_ACTIVE ${ALMOND_ENABLE_SDL})
set(ALMOND_SFML_ACTIVE ${ALMOND_ENABLE_SFML})
set(ALMOND_VULKAN_ACTIVE ${ALMOND_ENABLE_VULKAN})
set(ALMOND_OPENGL_ACTIVE ${ALMOND_ENABLE_OPENGL})
set(ALMOND_SOFTWARE_RENDERER_ACTIVE ${ALMOND_ENABLE_SOFTWARE_RENDERER})

find_package(PkgConfig QUIET)

if(ALMOND_RAYLIB_ACTIVE)
    find_package(raylib CONFIG QUIET)
    if(raylib_FOUND)
        target_link_libraries(almondshell PRIVATE raylib)
    else()
        find_library(RAYLIB_LIBRARY NAMES raylib)
        find_path(RAYLIB_INCLUDE_DIR raylib.h PATH_SUFFIXES raylib)
        if(RAYLIB_LIBRARY AND RAYLIB_INCLUDE_DIR)
            target_include_directories(almondshell PRIVATE ${RAYLIB_INCLUDE_DIR})
            target_link_libraries(almondshell PRIVATE ${RAYLIB_LIBRARY})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "raylib was not found. Install it via vcpkg, your system package manager, or configure with -DALMOND_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "raylib was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_RAYLIB_ACTIVE AND UNIX AND NOT APPLE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(almondshell PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(almondshell PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(almondshell PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DALMOND_ENABLE_RAYLIB=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_RAYLIB_ACTIVE OFF)
                    set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DALMOND_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DALMOND_ENABLE_RAYLIB=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
            endif()
        endif()
    endif()
endif()

if(ALMOND_SDL_ACTIVE)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        target_link_libraries(almondshell PRIVATE SDL3::SDL3)
    else()
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 QUIET sdl3)
            if(SDL3_FOUND)
                target_include_directories(almondshell PRIVATE ${SDL3_INCLUDE_DIRS})
                target_link_libraries(almondshell PRIVATE ${SDL3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SDL=OFF.")
                else()
                    message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DALMOND_ENABLE_SDL=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_SDL_ACTIVE OFF)
                    set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SDL=OFF.")
            else()
                message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DALMOND_ENABLE_SDL=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_SDL_ACTIVE OFF)
                set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_SDL_ACTIVE)
        find_package(SDL3_image CONFIG QUIET)
        if(SDL3_image_FOUND)
            target_link_libraries(almondshell PRIVATE SDL3_image::SDL3_image)
        endif()
    endif()
endif()

if(ALMOND_SFML_ACTIVE)
    find_package(SFML 3.0 COMPONENTS graphics window system QUIET)
    if(SFML_FOUND)
        target_link_libraries(almondshell PRIVATE sfml-graphics sfml-window sfml-system)
    elseif(PkgConfig_FOUND)
        pkg_check_modules(SFML QUIET sfml-graphics sfml-window sfml-system)
        if(SFML_FOUND)
            target_include_directories(almondshell PRIVATE ${SFML_INCLUDE_DIRS})
            target_link_libraries(almondshell PRIVATE ${SFML_LIBRARIES})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SFML=OFF.")
            else()
                message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DALMOND_ENABLE_SFML=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_SFML_ACTIVE OFF)
                set(ALMOND_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
            endif()
        endif()
    else()
        if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
            message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DALMOND_ENABLE_SFML=OFF.")
        else()
            message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DALMOND_ENABLE_SFML=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
            set(ALMOND_SFML_ACTIVE OFF)
            set(ALMOND_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
        endif()
    endif()
endif()

if(ALMOND_VULKAN_ACTIVE)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_link_libraries(almondshell PRIVATE Vulkan::Vulkan)
    else()
        find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1)
        find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
        if(VULKAN_LIBRARY AND VULKAN_INCLUDE_DIR)
            target_include_directories(almondshell PRIVATE ${VULKAN_INCLUDE_DIR})
            target_link_libraries(almondshell PRIVATE ${VULKAN_LIBRARY})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "Vulkan was not found. Install it or rerun with -DALMOND_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "Vulkan was not found; the Vulkan backend will be disabled. Configure with -DALMOND_ENABLE_VULKAN=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_VULKAN_ACTIVE OFF)
                set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_VULKAN_ACTIVE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(almondshell PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(almondshell PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(almondshell PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DALMOND_ENABLE_VULKAN=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DALMOND_ENABLE_VULKAN=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_VULKAN_ACTIVE OFF)
                    set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DALMOND_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DALMOND_ENABLE_VULKAN=OFF to silence this warning or enable ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_VULKAN_ACTIVE OFF)
                set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_VULKAN_ACTIVE)
        list(APPEND ALMONDSHELL_SOURCES
            src/acontext.vulkan.platform.context.cpp
            src/acontext.vulkan.platform.descriptors.cpp
        )
        target_sources(almondshell PRIVATE
            src/acontext.vulkan.platform.context.cpp
            src/acontext.vulkan.platform.descriptors.cpp
        )
    endif()
endif()

find_package(OpenGL COMPONENTS OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(almondshell PRIVATE OpenGL::GL)
elseif(WIN32)
    target_link_libraries(almondshell PRIVATE opengl32)
endif()

if(ALMOND_OPENGL_ACTIVE OR ALMOND_RAYLIB_ACTIVE OR ALMOND_SDL_ACTIVE)
    set(ALMOND_GLAD_NEED_HEADERS ON)
else()
    set(ALMOND_GLAD_NEED_HEADERS OFF)
endif()

set(ALMOND_GLAD_SHOULD_LINK OFF)
if((ALMOND_OPENGL_ACTIVE OR ALMOND_SDL_ACTIVE) AND NOT ALMOND_RAYLIB_ACTIVE)
    set(ALMOND_GLAD_SHOULD_LINK ON)
endif()

if(ALMOND_GLAD_NEED_HEADERS)
    find_package(glad CONFIG QUIET)
    set(ALMOND_GLAD_TARGET "")
    if(TARGET glad::glad)
        set(ALMOND_GLAD_TARGET glad::glad)
    endif()

    set(_almond_builtin_glad_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)

    if(ALMOND_GLAD_TARGET)
        if(ALMOND_GLAD_SHOULD_LINK)
            target_link_libraries(almondshell PRIVATE ${ALMOND_GLAD_TARGET})
        else()
            target_include_directories(almondshell PRIVATE $<TARGET_PROPERTY:${ALMOND_GLAD_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
    elseif(ALMOND_GLAD_SHOULD_LINK)
        if(EXISTS ${_almond_builtin_glad_dir}/src/glad.c AND EXISTS ${_almond_builtin_glad_dir}/include/glad/glad.h)
            add_library(almond_builtin_glad STATIC ${_almond_builtin_glad_dir}/src/glad.c)
            target_include_directories(almond_builtin_glad PUBLIC ${_almond_builtin_glad_dir}/include)
            if(NOT TARGET glad::glad)
                add_library(glad::glad ALIAS almond_builtin_glad)
            endif()
            target_link_libraries(almondshell PRIVATE glad::glad)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            find_library(GLAD_LIBRARY NAMES glad)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(almondshell PRIVATE ${GLAD_INCLUDE_DIR})
            endif()
            if(GLAD_LIBRARY)
                target_link_libraries(almondshell PRIVATE ${GLAD_LIBRARY})
            else()
                message(FATAL_ERROR "glad was not found. Install it via vcpkg, your system package manager, or provide a glad::glad target before configuring AlmondShell.")
            endif()
            if(NOT GLAD_INCLUDE_DIR)
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring AlmondShell.")
            endif()
        endif()
    else()
        if(EXISTS ${_almond_builtin_glad_dir}/include/glad/glad.h)
            target_include_directories(almondshell PRIVATE ${_almond_builtin_glad_dir}/include)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(almondshell PRIVATE ${GLAD_INCLUDE_DIR})
            else()
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring AlmondShell.")
            endif()
        endif()
    endif()
endif()

target_compile_definitions(almondshell PRIVATE
    $<$<BOOL:${ALMOND_RAYLIB_ACTIVE}>:ALMOND_FORCE_ENABLE_RAYLIB>
    $<$<NOT:$<BOOL:${ALMOND_RAYLIB_ACTIVE}>>:ALMOND_FORCE_DISABLE_RAYLIB>
    $<$<BOOL:${ALMOND_SDL_ACTIVE}>:ALMOND_FORCE_ENABLE_SDL>
    $<$<NOT:$<BOOL:${ALMOND_SDL_ACTIVE}>>:ALMOND_FORCE_DISABLE_SDL>
    $<$<BOOL:${ALMOND_SFML_ACTIVE}>:ALMOND_FORCE_ENABLE_SFML>
    $<$<NOT:$<BOOL:${ALMOND_SFML_ACTIVE}>>:ALMOND_FORCE_DISABLE_SFML>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:ALMOND_USING_VULKAN>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:VULKAN_HPP_NO_EXCEPTIONS>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:GLM_FORCE_DEPTH_ZERO_TO_ONE>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:GLM_FORCE_RADIANS>
    $<$<BOOL:${ALMOND_OPENGL_ACTIVE}>:ALMOND_FORCE_ENABLE_OPENGL>
    $<$<NOT:$<BOOL:${ALMOND_OPENGL_ACTIVE}>>:ALMOND_FORCE_DISABLE_OPENGL>
    $<$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>:ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER>
    $<$<NOT:$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>>:ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER>
)

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    target_link_libraries(almondshell PRIVATE glm::glm)
endif()

find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    target_link_libraries(almondshell PRIVATE fmt::fmt)
endif()

find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(almondshell PRIVATE asio::asio)
endif()

find_package(Threads REQUIRED)
target_link_libraries(almondshell PRIVATE Threads::Threads)

if(NOT WIN32)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(almondshell PRIVATE CURL::libcurl)
        target_compile_definitions(almondshell PRIVATE ALMOND_HAS_CURL=1)
    else()
        message(WARNING "libcurl not found; non-Windows AI HTTP transport will be disabled.")
    endif()
endif()

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    if(X11_INCLUDE_DIR)
        target_include_directories(almondshell PRIVATE ${X11_INCLUDE_DIR})
    endif()
    target_link_libraries(almondshell PRIVATE dl m X11::X11 GL)
    if(TARGET X11::Xrandr)
        target_link_libraries(almondshell PRIVATE X11::Xrandr)
    elseif(X11_Xrandr_LIB)
        target_link_libraries(almondshell PRIVATE ${X11_Xrandr_LIB})
    endif()
endif()

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/docs/api
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating AlmondShell API documentation"
        VERBATIM)
else()
    message(STATUS "Doxygen not found; the 'docs' target will be skipped.")
endif()
