cmake_minimum_required(VERSION 3.28)

if(CMAKE_VERSION VERSION_LESS 3.29)
    # CMake 3.27ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“3.28 require the experimental dyndep scanner to emit BMI rules.
    set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
endif()

# Keep module scanning enabled across all supported generators/toolchains.
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(Epoch LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EPOCH_SOURCES
        src/aengine.cpp
    src/aengine.context.cpp
    src/aengine.gui.cpp
    src/aengine.scripting.compiler.cpp
    src/aeditor.cpp
    src/aeditor.scene.cpp
    src/afilewatch.cpp
    src/afont.renderer.cpp
    src/acontext.raylib.bridge.cpp
    src/acontext.vulkan.platform.context.cpp
    src/acontext.vulkan.platform.descriptors.cpp
    src/acontext.vulkan.context-texture.cpp
    src/acontext.vulkan.context_buffers.cpp
    src/acontext.vulkan.dispatch_storage.cpp
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
    src/runtime.cpp
)


if(WIN32)
    list(APPEND EPOCH_SOURCES
        src/aengine.context.multiplexer.win.cpp
        src/WinResource.rc
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND EPOCH_SOURCES
        src/aengine.context.multiplexer.linux.cpp
    )
endif()

add_executable(epoch ${EPOCH_SOURCES})
set_target_properties(epoch PROPERTIES OUTPUT_NAME epoch)

add_executable(epoch_renderer_smoke
    src/renderer_smoke_harness.cpp
)


set(EPOCH_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)
file(GLOB EPOCH_MODULE_FILES ${EPOCH_MODULE_DIR}/*.ixx)
list(SORT EPOCH_MODULE_FILES)

target_sources(epoch PRIVATE
    FILE_SET epoch_modules TYPE CXX_MODULES
        BASE_DIRS ${EPOCH_MODULE_DIR}
        FILES ${EPOCH_MODULE_FILES}
)

target_include_directories(epoch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

target_include_directories(epoch_renderer_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb
)

find_program(GLSLANG_VALIDATOR glslangValidator)
set(EPOCH_VULKAN_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets/vulkan)
set(EPOCH_VULKAN_SHADER_DIR ${EPOCH_VULKAN_ASSET_DIR}/shaders)

file(MAKE_DIRECTORY ${EPOCH_VULKAN_SHADER_DIR})
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/vulkan/texture.ppm
    ${EPOCH_VULKAN_ASSET_DIR}/texture.ppm
    COPYONLY
)

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

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Modern MSVC releases provide standard module support without the
    # deprecated /experimental:module switch; keep the latest standard
    # mode enabled so BMI emission stays aligned with scan-for-modules.
    target_compile_options(epoch PRIVATE /std:c++latest)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 14+ requires the Modules TS flag so the dyn-dep scanner emits
    # BMI build rules correctly when paired with Ninja or Makefiles.
    target_compile_options(epoch PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang builds rely on the Modules TS flag to keep BMI generation
    # consistent across CMake presets and helper scripts.
    target_compile_options(epoch PRIVATE -fmodules-ts)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(epoch_renderer_smoke PRIVATE /std:c++latest)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(epoch_renderer_smoke PRIVATE -fmodules-ts)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(epoch_renderer_smoke PRIVATE -fmodules-ts)
endif()

function(epoch_compat_option suffix default_value help_text)
    set(_legacy_name "ALMOND_${suffix}")
    set(_epoch_name "EPOCH_${suffix}")
    set(_epoch_default "${default_value}")
    if(DEFINED ${_legacy_name} AND NOT DEFINED ${_epoch_name})
        set(_epoch_default "${${_legacy_name}}")
    endif()
    option(${_epoch_name} "${help_text}" ${_epoch_default})
    set(${_legacy_name} "${${_epoch_name}}" CACHE BOOL "Compatibility alias for ${_epoch_name}" FORCE)
endfunction()

epoch_compat_option(ENABLE_RAYLIB ON "Enable the Raylib backend")
epoch_compat_option(ENABLE_SDL ON "Enable the SDL backend")
epoch_compat_option(ENABLE_SFML ON "Enable the SFML backend")
epoch_compat_option(ENABLE_VULKAN ON "Enable the Vulkan backend")
epoch_compat_option(ENABLE_OPENGL ON "Enable the OpenGL backend")
epoch_compat_option(ENABLE_SOFTWARE_RENDERER ON "Enable the software renderer backend")
epoch_compat_option(REQUIRE_OPTIONAL_DEPENDENCIES OFF "Treat missing optional backend dependencies as a configuration error")

set(ALMOND_RAYLIB_ACTIVE ${EPOCH_ENABLE_RAYLIB})
set(ALMOND_SDL_ACTIVE ${EPOCH_ENABLE_SDL})
set(ALMOND_SFML_ACTIVE ${EPOCH_ENABLE_SFML})
set(ALMOND_VULKAN_ACTIVE ${EPOCH_ENABLE_VULKAN})
set(ALMOND_OPENGL_ACTIVE ${EPOCH_ENABLE_OPENGL})
set(ALMOND_SOFTWARE_RENDERER_ACTIVE ${EPOCH_ENABLE_SOFTWARE_RENDERER})

find_package(PkgConfig QUIET)

if(ALMOND_RAYLIB_ACTIVE)
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
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "raylib was not found. Install it via vcpkg, your system package manager, or configure with -DEPOCH_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "raylib was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_RAYLIB_ACTIVE AND UNIX AND NOT APPLE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(epoch PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(epoch PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DEPOCH_ENABLE_RAYLIB=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_RAYLIB_ACTIVE OFF)
                    set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Raylib backend with -DEPOCH_ENABLE_RAYLIB=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Raylib backend will be disabled. Configure with -DEPOCH_ENABLE_RAYLIB=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_RAYLIB_ACTIVE OFF)
                set(EPOCH_ENABLE_RAYLIB OFF CACHE BOOL "Enable the Raylib backend" FORCE)
                    set(ALMOND_ENABLE_RAYLIB OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_RAYLIB" FORCE)
            endif()
        endif()
    endif()
endif()

if(ALMOND_SDL_ACTIVE)
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
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SDL=OFF.")
                else()
                    message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DEPOCH_ENABLE_SDL=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_SDL_ACTIVE OFF)
                    set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                    set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SDL" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SDL3 was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SDL=OFF.")
            else()
                message(WARNING "SDL3 was not found; the SDL backend will be disabled. Configure with -DEPOCH_ENABLE_SDL=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_SDL_ACTIVE OFF)
                set(EPOCH_ENABLE_SDL OFF CACHE BOOL "Enable the SDL backend" FORCE)
                    set(ALMOND_ENABLE_SDL OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SDL" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_SDL_ACTIVE)
        find_package(SDL3_image CONFIG QUIET)
        if(SDL3_image_FOUND)
            target_link_libraries(epoch PRIVATE SDL3_image::SDL3_image)
        endif()
    endif()
endif()

if(ALMOND_SFML_ACTIVE)
    find_package(SFML 3.0 COMPONENTS graphics window system QUIET)
    if(SFML_FOUND)
        target_link_libraries(epoch PRIVATE sfml-graphics sfml-window sfml-system)
    elseif(PkgConfig_FOUND)
        pkg_check_modules(SFML QUIET sfml-graphics sfml-window sfml-system)
        if(SFML_FOUND)
            target_include_directories(epoch PRIVATE ${SFML_INCLUDE_DIRS})
            target_link_libraries(epoch PRIVATE ${SFML_LIBRARIES})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SFML=OFF.")
            else()
                message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DEPOCH_ENABLE_SFML=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_SFML_ACTIVE OFF)
                set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
                    set(ALMOND_ENABLE_SFML OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SFML" FORCE)
            endif()
        endif()
    else()
        if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
            message(FATAL_ERROR "SFML was not found. Install it via vcpkg or rerun with -DEPOCH_ENABLE_SFML=OFF.")
        else()
            message(WARNING "SFML was not found; the SFML backend will be disabled. Configure with -DEPOCH_ENABLE_SFML=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
            set(ALMOND_SFML_ACTIVE OFF)
            set(EPOCH_ENABLE_SFML OFF CACHE BOOL "Enable the SFML backend" FORCE)
                    set(ALMOND_ENABLE_SFML OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_SFML" FORCE)
        endif()
    endif()
endif()

if(ALMOND_VULKAN_ACTIVE)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_link_libraries(epoch PRIVATE Vulkan::Vulkan)
    else()
        find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1)
        find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
        if(VULKAN_LIBRARY AND VULKAN_INCLUDE_DIR)
            target_include_directories(epoch PRIVATE ${VULKAN_INCLUDE_DIR})
            target_link_libraries(epoch PRIVATE ${VULKAN_LIBRARY})
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "Vulkan was not found. Install it or rerun with -DEPOCH_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "Vulkan was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_VULKAN_ACTIVE OFF)
                set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_VULKAN_ACTIVE)
        find_package(glfw3 CONFIG QUIET)
        if(glfw3_FOUND)
            target_link_libraries(epoch PRIVATE glfw)
        elseif(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
            if(GLFW3_FOUND)
                target_include_directories(epoch PRIVATE ${GLFW3_INCLUDE_DIRS})
                target_link_libraries(epoch PRIVATE ${GLFW3_LIBRARIES})
            else()
                if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                    message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DEPOCH_ENABLE_VULKAN=OFF.")
                else()
                    message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                    set(ALMOND_VULKAN_ACTIVE OFF)
                    set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
                endif()
            endif()
        else()
            if(ALMOND_REQUIRE_OPTIONAL_DEPENDENCIES)
                message(FATAL_ERROR "glfw3 was not found. Install it or disable the Vulkan backend with -DEPOCH_ENABLE_VULKAN=OFF.")
            else()
                message(WARNING "glfw3 was not found; the Vulkan backend will be disabled. Configure with -DEPOCH_ENABLE_VULKAN=OFF to silence this warning or enable EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES to treat it as an error.")
                set(ALMOND_VULKAN_ACTIVE OFF)
                set(EPOCH_ENABLE_VULKAN OFF CACHE BOOL "Enable the Vulkan backend" FORCE)
                    set(ALMOND_ENABLE_VULKAN OFF CACHE BOOL "Compatibility alias for EPOCH_ENABLE_VULKAN" FORCE)
            endif()
        endif()
    endif()

    if(ALMOND_VULKAN_ACTIVE)
        list(APPEND EPOCH_SOURCES
        )
        target_sources(epoch PRIVATE
        )
    endif()
endif()

find_package(OpenGL COMPONENTS OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(epoch PRIVATE OpenGL::GL)
elseif(WIN32)
    target_link_libraries(epoch PRIVATE opengl32)
endif()

if(ALMOND_OPENGL_ACTIVE OR ALMOND_RAYLIB_ACTIVE OR ALMOND_SDL_ACTIVE)
    set(ALMOND_GLAD_NEED_HEADERS ON)
else()
    set(ALMOND_GLAD_NEED_HEADERS OFF)
endif()

set(ALMOND_GLAD_SHOULD_LINK OFF)
if(ALMOND_OPENGL_ACTIVE OR ALMOND_SDL_ACTIVE OR ALMOND_RAYLIB_ACTIVE)
    set(ALMOND_GLAD_SHOULD_LINK ON)
endif()

if(ALMOND_GLAD_NEED_HEADERS)
    find_package(glad CONFIG QUIET)
    set(ALMOND_GLAD_TARGET "")
    if(TARGET glad::glad)
        set(ALMOND_GLAD_TARGET glad::glad)
    endif()

    set(_epoch_builtin_glad_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)

    if(ALMOND_GLAD_TARGET)
        if(ALMOND_GLAD_SHOULD_LINK)
            target_link_libraries(epoch PRIVATE ${ALMOND_GLAD_TARGET})
        else()
            target_include_directories(epoch PRIVATE $<TARGET_PROPERTY:${ALMOND_GLAD_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
        endif()
    elseif(ALMOND_GLAD_SHOULD_LINK)
        if(EXISTS ${_epoch_builtin_glad_dir}/src/glad.c AND EXISTS ${_epoch_builtin_glad_dir}/include/glad/glad.h)
            add_library(epoch_builtin_glad STATIC ${_epoch_builtin_glad_dir}/src/glad.c)
            target_include_directories(epoch_builtin_glad PUBLIC ${_epoch_builtin_glad_dir}/include)
            if(NOT TARGET glad::glad)
                add_library(glad::glad ALIAS epoch_builtin_glad)
            endif()
            target_link_libraries(epoch PRIVATE glad::glad)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            find_library(GLAD_LIBRARY NAMES glad)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(epoch PRIVATE ${GLAD_INCLUDE_DIR})
            endif()
            if(GLAD_LIBRARY)
                target_link_libraries(epoch PRIVATE ${GLAD_LIBRARY})
            else()
                message(FATAL_ERROR "glad was not found. Install it via vcpkg, your system package manager, or provide a glad::glad target before configuring Epoch.")
            endif()
            if(NOT GLAD_INCLUDE_DIR)
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring Epoch.")
            endif()
        endif()
    else()
        if(EXISTS ${_epoch_builtin_glad_dir}/include/glad/glad.h)
            target_include_directories(epoch PRIVATE ${_epoch_builtin_glad_dir}/include)
        else()
            find_path(GLAD_INCLUDE_DIR glad/glad.h)
            if(GLAD_INCLUDE_DIR)
                target_include_directories(epoch PRIVATE ${GLAD_INCLUDE_DIR})
            else()
                message(FATAL_ERROR "glad headers were not found. Install them alongside the loader or provide a glad::glad target before configuring Epoch.")
            endif()
        endif()
    endif()
endif()

if(MSVC AND ALMOND_RAYLIB_ACTIVE AND ALMOND_GLAD_SHOULD_LINK)
    target_link_options(epoch PRIVATE /FORCE:MULTIPLE)
endif()

target_compile_definitions(epoch PRIVATE
    EPOCH_MAIN_IN_MAIN_CPP=1
    $<$<BOOL:${ALMOND_RAYLIB_ACTIVE}>:EPOCH_FORCE_ENABLE_RAYLIB>
    $<$<NOT:$<BOOL:${ALMOND_RAYLIB_ACTIVE}>>:EPOCH_FORCE_DISABLE_RAYLIB>
    $<$<BOOL:${ALMOND_SDL_ACTIVE}>:EPOCH_FORCE_ENABLE_SDL>
    $<$<NOT:$<BOOL:${ALMOND_SDL_ACTIVE}>>:EPOCH_FORCE_DISABLE_SDL>
    $<$<BOOL:${ALMOND_SFML_ACTIVE}>:EPOCH_FORCE_ENABLE_SFML>
    $<$<NOT:$<BOOL:${ALMOND_SFML_ACTIVE}>>:EPOCH_FORCE_DISABLE_SFML>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:EPOCH_USING_VULKAN>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:VULKAN_HPP_NO_EXCEPTIONS>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:GLM_FORCE_DEPTH_ZERO_TO_ONE>
    $<$<BOOL:${ALMOND_VULKAN_ACTIVE}>:GLM_FORCE_RADIANS>
    $<$<BOOL:${ALMOND_OPENGL_ACTIVE}>:EPOCH_FORCE_ENABLE_OPENGL>
    $<$<NOT:$<BOOL:${ALMOND_OPENGL_ACTIVE}>>:EPOCH_FORCE_DISABLE_OPENGL>
    $<$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>:EPOCH_FORCE_ENABLE_SOFTWARE_RENDERER>
    $<$<NOT:$<BOOL:${ALMOND_SOFTWARE_RENDERER_ACTIVE}>>:EPOCH_FORCE_DISABLE_SOFTWARE_RENDERER>
)

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    target_link_libraries(epoch PRIVATE glm::glm)
endif()

find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    target_link_libraries(epoch PRIVATE fmt::fmt)
endif()

find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(epoch PRIVATE asio::asio)
endif()

find_package(Threads REQUIRED)
target_link_libraries(epoch PRIVATE Threads::Threads)

if(NOT WIN32)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(epoch PRIVATE CURL::libcurl)
        target_compile_definitions(epoch PRIVATE ALMOND_HAS_CURL=1)
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
