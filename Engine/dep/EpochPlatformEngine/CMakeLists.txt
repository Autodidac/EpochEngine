cmake_minimum_required(VERSION 3.28)

project(EpochPlatformEngine
    VERSION 0.2.0
    DESCRIPTION "Cross-platform C++23 multi-context platform runtime"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

# Header-first public API: do not require compiler module scanners.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(EPOCH_PLATFORM_BUILD_STATIC "Build EpochPlatformEngine as a static library" ON)
option(EPOCH_PLATFORM_BUILD_SHARED "Build EpochPlatformEngine as a shared library" ON)
option(EPOCH_PLATFORM_BUILD_EXAMPLES "Build internal-entrypoint examples" ON)
option(EPOCH_PLATFORM_BUILD_TESTS "Build tests" ON)
option(EPOCH_PLATFORM_ENABLE_X11 "Enable the native X11 driver when available" ON)
option(EPOCH_PLATFORM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

find_package(Threads REQUIRED)

# A C++23 mode flag alone does not establish standard-library compatibility.
# For example, Clang 18 + Ubuntu libstdc++ hides std::expected behind its
# concepts feature test. Diagnose the actual compiler/library pair up front.
function(epoch_platform_check_standard_library)
    if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 23)
        set(CMAKE_CXX_STANDARD 23)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles([=[
        #include <expected>
        #include <thread>
        #include <stop_token>
        int main() {
            std::expected<int, int> result = 7;
            result = std::unexpected(3);
            std::jthread worker([](std::stop_token token) { (void)token; });
            worker.request_stop();
            return result.has_value() ? 1 : result.error() - 3;
        }
    ]=] EPOCH_PLATFORM_HAS_REQUIRED_STANDARD_LIBRARY)
    if(NOT EPOCH_PLATFORM_HAS_REQUIRED_STANDARD_LIBRARY)
        message(FATAL_ERROR
            "EpochPlatformEngine requires C++23 std::expected and std::jthread. "
            "Select a compatible compiler AND standard library; the Linux CI pair "
            "is Clang 19 with libstdc++ 13 or newer. Do not override feature macros.")
    endif()
endfunction()
epoch_platform_check_standard_library()

set(EPOCH_PLATFORM_COMMON_SOURCES
    src/application.cpp
    src/runtime.cpp
    src/runtime_host.cpp
    src/types.cpp
    src/drivers/headless_driver.cpp)

set(EPOCH_PLATFORM_PLATFORM_SOURCES)
set(EPOCH_PLATFORM_PLATFORM_LIBRARIES)
set(EPOCH_PLATFORM_PLATFORM_DEFINITIONS)

if(WIN32)
    list(APPEND EPOCH_PLATFORM_PLATFORM_SOURCES src/drivers/win32_driver.cpp)
    list(APPEND EPOCH_PLATFORM_PLATFORM_LIBRARIES user32 shell32)
    list(APPEND EPOCH_PLATFORM_PLATFORM_DEFINITIONS
        UNICODE _UNICODE
        _WIN32_WINNT=0x0A00 WINVER=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND EPOCH_PLATFORM_PLATFORM_SOURCES src/drivers/cocoa_driver.mm)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    list(APPEND EPOCH_PLATFORM_PLATFORM_LIBRARIES ${COCOA_FRAMEWORK})
elseif(UNIX AND EPOCH_PLATFORM_ENABLE_X11)
    find_package(X11 QUIET)
    if(X11_FOUND)
        list(APPEND EPOCH_PLATFORM_PLATFORM_SOURCES src/drivers/x11_driver.cpp)
        list(APPEND EPOCH_PLATFORM_PLATFORM_LIBRARIES X11::X11)
        list(APPEND EPOCH_PLATFORM_PLATFORM_DEFINITIONS EPOCH_PLATFORM_HAS_X11=1)
    endif()
endif()

function(epoch_platform_configure_library target linkage)
    add_library(${target} ${linkage}
        ${EPOCH_PLATFORM_COMMON_SOURCES}
        ${EPOCH_PLATFORM_PLATFORM_SOURCES})

    target_compile_features(${target} PUBLIC cxx_std_23)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(${target} PUBLIC Threads::Threads ${EPOCH_PLATFORM_PLATFORM_LIBRARIES})
    target_compile_definitions(${target} PUBLIC ${EPOCH_PLATFORM_PLATFORM_DEFINITIONS})

    if(linkage STREQUAL "STATIC")
        target_compile_definitions(${target} PUBLIC EPOCH_PLATFORM_ENGINE_STATIC=1)
    else()
        target_compile_definitions(${target} PRIVATE EPOCH_PLATFORM_ENGINE_EXPORTS=1)
        set_target_properties(${target} PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc)
        if(EPOCH_PLATFORM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(EPOCH_PLATFORM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(APPLE)
        set_source_files_properties(src/drivers/cocoa_driver.mm PROPERTIES COMPILE_FLAGS "-fno-objc-arc")
    endif()

    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
endfunction()

if(EPOCH_PLATFORM_BUILD_STATIC)
    epoch_platform_configure_library(EpochPlatformEngineStatic STATIC)
    set_target_properties(EpochPlatformEngineStatic PROPERTIES OUTPUT_NAME EpochPlatformEngineStatic EXPORT_NAME Static)
    add_library(EpochPlatformEngine::Static ALIAS EpochPlatformEngineStatic)
endif()

if(EPOCH_PLATFORM_BUILD_SHARED)
    epoch_platform_configure_library(EpochPlatformEngineShared SHARED)
    set_target_properties(EpochPlatformEngineShared PROPERTIES OUTPUT_NAME EpochPlatformEngine EXPORT_NAME Shared)
    add_library(EpochPlatformEngine::Shared ALIAS EpochPlatformEngineShared)
endif()

function(epoch_platform_add_entrypoint target engine_target)
    add_library(${target} STATIC src/entrypoint/entrypoint.cpp)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(${target} PUBLIC ${engine_target})
endfunction()

if(TARGET EpochPlatformEngineStatic)
    epoch_platform_add_entrypoint(EpochPlatformEntrypointConsoleStatic EpochPlatformEngineStatic)
    set_target_properties(EpochPlatformEntrypointConsoleStatic PROPERTIES OUTPUT_NAME EpochPlatformEntrypointConsoleStatic EXPORT_NAME EntrypointConsoleStatic)
    add_library(EpochPlatformEngine::EntrypointConsoleStatic ALIAS EpochPlatformEntrypointConsoleStatic)
endif()

if(TARGET EpochPlatformEngineShared)
    epoch_platform_add_entrypoint(EpochPlatformEntrypointConsoleShared EpochPlatformEngineShared)
    set_target_properties(EpochPlatformEntrypointConsoleShared PROPERTIES OUTPUT_NAME EpochPlatformEntrypointConsoleShared EXPORT_NAME EntrypointConsoleShared)
    add_library(EpochPlatformEngine::EntrypointConsoleShared ALIAS EpochPlatformEntrypointConsoleShared)
endif()

if(WIN32)
    if(TARGET EpochPlatformEngineStatic)
        epoch_platform_add_entrypoint(EpochPlatformEntrypointWindowedStatic EpochPlatformEngineStatic)
        target_compile_definitions(EpochPlatformEntrypointWindowedStatic PRIVATE EPOCH_PLATFORM_WINDOWED_ENTRYPOINT=1)
        set_target_properties(EpochPlatformEntrypointWindowedStatic PROPERTIES OUTPUT_NAME EpochPlatformEntrypointWindowedStatic EXPORT_NAME EntrypointWindowedStatic)
        add_library(EpochPlatformEngine::EntrypointWindowedStatic ALIAS EpochPlatformEntrypointWindowedStatic)
    endif()
    if(TARGET EpochPlatformEngineShared)
        epoch_platform_add_entrypoint(EpochPlatformEntrypointWindowedShared EpochPlatformEngineShared)
        target_compile_definitions(EpochPlatformEntrypointWindowedShared PRIVATE EPOCH_PLATFORM_WINDOWED_ENTRYPOINT=1)
        set_target_properties(EpochPlatformEntrypointWindowedShared PROPERTIES OUTPUT_NAME EpochPlatformEntrypointWindowedShared EXPORT_NAME EntrypointWindowedShared)
        add_library(EpochPlatformEngine::EntrypointWindowedShared ALIAS EpochPlatformEntrypointWindowedShared)
    endif()
endif()

# A named-module consumer uses the same canonical header declarations and native
# implementation as existing callers. Each linkage gets its correct ABI macros.
option(EPOCH_PLATFORM_BUILD_MODULES "Build C++23 named module API" ON)
if(EPOCH_PLATFORM_BUILD_MODULES)
    foreach(linkage IN ITEMS Static Shared)
        if(TARGET EpochPlatformEngine${linkage})
            add_library(EpochPlatformModule${linkage} STATIC)
            target_compile_features(EpochPlatformModule${linkage} PUBLIC cxx_std_23)
            set_target_properties(EpochPlatformModule${linkage} PROPERTIES CXX_SCAN_FOR_MODULES ON EXPORT_NAME Module${linkage})
            target_sources(EpochPlatformModule${linkage} PUBLIC
                FILE_SET CXX_MODULES FILES modules/platform.native.ixx)
            target_sources(EpochPlatformModule${linkage} PUBLIC
                FILE_SET adapter_modules TYPE CXX_MODULES FILES
                    modules/context.admission.ixx
                    modules/context.type.ixx
                    modules/core.env.ixx
                    modules/core.error.ixx
                    modules/core.log.ixx
                    modules/core.time.ixx
                    modules/perf.select.ixx
                    modules/perf.tier.ixx
                    modules/platform.application.ixx
                    modules/platform.budgets.ixx
                    modules/platform.capabilities.ixx
                    modules/platform.engine.ixx
                    modules/platform.window.ixx
                    modules/platform.window_attachment.ixx)
            target_sources(EpochPlatformModule${linkage} PRIVATE
                    src/core/core.env.cpp
                    src/core/core.log.cpp
                    src/core/core.time.cpp
                    src/epoch/epoch.software_application.cpp
                    src/platform/platform.window.cpp
                    src/platform/platform.window_attachment.cpp)
            if(WIN32)
                target_link_libraries(EpochPlatformModule${linkage} PUBLIC comctl32)
            endif()
            target_link_libraries(EpochPlatformModule${linkage} PUBLIC EpochPlatformEngine${linkage})
            add_library(EpochPlatformEngine::Module${linkage} ALIAS EpochPlatformModule${linkage})
            if(BUILD_TESTING AND EPOCH_PLATFORM_BUILD_TESTS)
                add_executable(EpochPlatformModuleContract${linkage} tests/platform.module_contract.cpp)
                set_target_properties(EpochPlatformModuleContract${linkage} PROPERTIES CXX_SCAN_FOR_MODULES ON)
                target_link_libraries(EpochPlatformModuleContract${linkage} PRIVATE EpochPlatformModule${linkage})
                add_test(NAME EpochPlatformModuleContract${linkage} COMMAND EpochPlatformModuleContract${linkage})
            endif()
        endif()
    endforeach()
endif()

include(cmake/EpochPlatformApplication.cmake)

if(EPOCH_PLATFORM_BUILD_EXAMPLES)
    if(TARGET EpochPlatformEntrypointConsoleStatic)
        epoch_platform_add_application(EpochPlatformExampleStatic
            LINKAGE STATIC
            SOURCES examples/basic_app.cpp)
        epoch_platform_add_application(EpochPlatformReleaseExampleStatic
            WINDOWED
            LINKAGE STATIC
            SOURCES examples/release_app.cpp)
    endif()
    if(TARGET EpochPlatformEntrypointConsoleShared)
        epoch_platform_add_application(EpochPlatformExampleShared
            LINKAGE SHARED
            SOURCES examples/basic_app.cpp)
        epoch_platform_add_application(EpochPlatformReleaseExampleShared
            WINDOWED
            LINKAGE SHARED
            SOURCES examples/release_app.cpp)

        if(APPLE)
            set_target_properties(EpochPlatformReleaseExampleShared PROPERTIES
                BUILD_RPATH "@loader_path"
                INSTALL_RPATH "@loader_path")
        elseif(UNIX)
            set_target_properties(EpochPlatformReleaseExampleShared PROPERTIES
                BUILD_RPATH "$ORIGIN"
                INSTALL_RPATH "$ORIGIN")
        endif()
    endif()
endif()

if(BUILD_TESTING AND EPOCH_PLATFORM_BUILD_TESTS AND TARGET EpochPlatformEngineStatic)
    add_executable(EpochPlatformEngineTests tests/platform_tests.cpp)
    target_compile_features(EpochPlatformEngineTests PRIVATE cxx_std_23)
    target_link_libraries(EpochPlatformEngineTests PRIVATE EpochPlatformEngineStatic)
    add_test(NAME EpochPlatformEngineTests COMMAND EpochPlatformEngineTests)
endif()

set(EPOCH_PLATFORM_INSTALL_TARGETS)
foreach(target IN ITEMS
    EpochPlatformEngineStatic EpochPlatformEngineShared
    EpochPlatformModuleStatic EpochPlatformModuleShared
    EpochPlatformEntrypointConsoleStatic EpochPlatformEntrypointConsoleShared
    EpochPlatformEntrypointWindowedStatic EpochPlatformEntrypointWindowedShared)
    if(TARGET ${target})
        list(APPEND EPOCH_PLATFORM_INSTALL_TARGETS ${target})
    endif()
endforeach()

install(TARGETS ${EPOCH_PLATFORM_INSTALL_TARGETS}
    EXPORT EpochPlatformEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_DATADIR}/EpochPlatformEngine/modules
    FILE_SET adapter_modules DESTINATION ${CMAKE_INSTALL_DATADIR}/EpochPlatformEngine/modules)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES cmake/EpochPlatformApplication.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochPlatformEngine)
install(EXPORT EpochPlatformEngineTargets
    FILE EpochPlatformEngineTargets.cmake
    NAMESPACE EpochPlatformEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochPlatformEngine)

set(EPOCH_PLATFORM_CONFIG_NEEDS_X11 OFF)
if(X11_FOUND)
    set(EPOCH_PLATFORM_CONFIG_NEEDS_X11 ON)
endif()

configure_package_config_file(
    cmake/EpochPlatformEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/EpochPlatformEngineConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochPlatformEngine)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/EpochPlatformEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/EpochPlatformEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/EpochPlatformEngineConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/EpochPlatformEngine)

# Compatibility target names identify the maintained static module owner.
if(TARGET EpochPlatformModuleStatic)
    add_library(Epoch::PlatformEngine ALIAS EpochPlatformModuleStatic)
    if(BUILD_TESTING AND EPOCH_PLATFORM_BUILD_TESTS)
        foreach(contract IN ITEMS policy tier native_input)
            if(contract STREQUAL "tier")
                set(_source tests/perf.tier_contract.cpp)
            else()
                set(_source tests/platform.${contract}_contract.cpp)
            endif()
            add_executable(epoch_platform_${contract}_contract ${_source})
            set_target_properties(epoch_platform_${contract}_contract PROPERTIES CXX_SCAN_FOR_MODULES ON)
            target_link_libraries(epoch_platform_${contract}_contract PRIVATE Epoch::PlatformEngine)
            add_test(NAME epoch_platform_${contract}_contract COMMAND epoch_platform_${contract}_contract)
        endforeach()
    endif()
endif()

# The software-session adapter keeps its explicit entry choice; it never starts
# the canonical Runtime alongside an existing Engine manager.
if(TARGET EpochPlatformModuleStatic AND CMAKE_SYSTEM_NAME MATCHES "^(Windows|Linux|Darwin)$" AND NOT ANDROID AND NOT EMSCRIPTEN)
    add_library(epoch_platform_entry OBJECT EXCLUDE_FROM_ALL src/platform/platform.entry.cpp)
    add_library(Epoch::PlatformEntry ALIAS epoch_platform_entry)
    set_target_properties(epoch_platform_entry PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_link_libraries(epoch_platform_entry PUBLIC Epoch::PlatformEngine)
    if(WIN32)
        target_link_libraries(epoch_platform_entry PUBLIC shell32)
    endif()
    if(BUILD_TESTING AND EPOCH_PLATFORM_BUILD_TESTS)
        add_executable(epoch_platform_entry_contract tests/platform.entry_contract.cpp)
        set_target_properties(epoch_platform_entry_contract PROPERTIES CXX_SCAN_FOR_MODULES ON)
        target_link_libraries(epoch_platform_entry_contract PRIVATE Epoch::PlatformEntry)
        add_test(NAME epoch_platform_entry_contract COMMAND epoch_platform_entry_contract --contract)
    endif()
endif()
