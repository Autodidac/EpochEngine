cmake_minimum_required(VERSION 3.28)

project(epoch_extension_planetary_terrain LANGUAGES CXX)

# Exact, reviewed upstream replacement owners. This optional target does not
# import EpochPlanet's aggregate Core target (which includes excluded voxel
# derivatives), worker pool, viewer, platform code or renderer dependencies.
set(_epoch_planet_upstream "${CMAKE_CURRENT_SOURCE_DIR}/upstream/EpochPlanet-0.2.3")
set(_epoch_planet_include "${_epoch_planet_upstream}/include")
set(_epoch_package_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(MSVC)
    # MSVC's include scanner still refuses an ordinary header path at MAX_PATH
    # in deeply nested project attachments. Keep the approved source untouched;
    # all package targets consume one short, byte-identical build input tree.
    set(_epoch_planet_include "${CMAKE_CURRENT_BINARY_DIR}/pinc")
    set(_epoch_package_include "${_epoch_planet_include}")
    file(MAKE_DIRECTORY "${_epoch_planet_include}/epoch/planet"
        "${_epoch_package_include}/epoch/extensions/planetary_terrain")
    foreach(_epoch_header IN ITEMS math.hpp world.hpp terrain.hpp simulation.hpp)
        configure_file("${_epoch_planet_upstream}/include/epoch/planet/${_epoch_header}"
            "${_epoch_planet_include}/epoch/planet/${_epoch_header}" COPYONLY)
    endforeach()
    foreach(_epoch_header IN ITEMS planetary_terrain.hpp planet.runtime_bridge.hpp)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/epoch/extensions/planetary_terrain/${_epoch_header}"
            "${_epoch_package_include}/epoch/extensions/planetary_terrain/${_epoch_header}" COPYONLY)
    endforeach()
endif()
add_library(epoch_extension_planetary_core STATIC
    "${_epoch_planet_upstream}/src/core/world.cpp"
    "${_epoch_planet_upstream}/src/core/terrain.cpp"
    "${_epoch_planet_upstream}/src/core/simulation.cpp"
)
add_library(EpochEngineExtensions::PlanetaryCore ALIAS epoch_extension_planetary_core)
target_include_directories(epoch_extension_planetary_core PUBLIC
    "$<BUILD_INTERFACE:${_epoch_planet_include}>")
target_compile_features(epoch_extension_planetary_core PUBLIC cxx_std_23)
set_target_properties(epoch_extension_planetary_core PROPERTIES
    CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES ON POSITION_INDEPENDENT_CODE ON)
target_sources(epoch_extension_planetary_core PUBLIC
    FILE_SET planetary_core_modules TYPE CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules"
    FILES modules/planet.terrain.ixx modules/planet.coverage.ixx modules/planet.streaming.ixx
          modules/planet.exploration.ixx)
if(MSVC)
    target_compile_options(epoch_extension_planetary_core PUBLIC
        /sdl /permissive- /Zc:__cplusplus /utf-8)
    target_compile_options(epoch_extension_planetary_core PRIVATE /W4)
    target_compile_definitions(epoch_extension_planetary_core PRIVATE _LIB)
else()
    target_compile_options(epoch_extension_planetary_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Preserve the existing small biome/tile sampler and its public header ABI.
add_library(epoch_extension_planetary_terrain STATIC
    src/planetary_terrain.cpp
)

target_include_directories(epoch_extension_planetary_terrain
    PUBLIC
        "${_epoch_package_include}"
)

target_compile_features(epoch_extension_planetary_terrain PUBLIC cxx_std_23)
set_target_properties(epoch_extension_planetary_terrain PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_planetary_terrain_self_test
    tests/planetary_terrain_self_test.cpp
)

target_link_libraries(epoch_extension_planetary_terrain_self_test
    PRIVATE epoch_extension_planetary_terrain
)

target_compile_features(epoch_extension_planetary_terrain_self_test PRIVATE cxx_std_23)

include(CTest)

# An explicit Engine consumer, never a dependency of the standalone terrain core.
option(EPOCH_PLANET_BUILD_MESH_EXPORT "Build the Engine mesh-artifact producer" OFF)
option(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER "Build the optional linked project exploration controller" OFF)
option(EPOCH_PLANET_BUILD_WATER "Build Water from an explicitly admitted FFT package" OFF)
set(EPOCH_PLANET_WATER_SOURCE_ROOT "" CACHE PATH "Exact admitted FFT source; no sibling or cache fallback")
set(EPOCH_ENGINE_SOURCE_ROOT "" CACHE PATH "Explicit Engine source for the optional artifact consumer")
if((EPOCH_PLANET_BUILD_MESH_EXPORT OR EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER) AND NOT TARGET epoch)
    foreach(_epoch_mesh_owner IN ITEMS
        modules/asset.texture_artifact.ixx modules/asset.mesh_artifact.ixx modules/asset.cloud_volume.ixx
        src/assets/texture/asset.texture_artifact.cpp modules/project.planetary_snapshot.ixx
        src/project/project.planetary_snapshot.cpp)
        if(NOT EXISTS "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/${_epoch_mesh_owner}")
            message(FATAL_ERROR "Missing explicit Engine artifact owner: ${_epoch_mesh_owner}")
        endif()
    endforeach()
    add_library(epoch_planet_mesh_format STATIC
        "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/src/assets/texture/asset.texture_artifact.cpp"
        "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/src/project/project.planetary_snapshot.cpp")
    target_sources(epoch_planet_mesh_format PUBLIC FILE_SET engine_mesh_modules TYPE CXX_MODULES
        BASE_DIRS "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules"
        FILES "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/asset.texture_artifact.ixx"
              "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/asset.mesh_artifact.ixx"
              "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/asset.cloud_volume.ixx"
              "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/project.planetary_snapshot.ixx")
    target_compile_features(epoch_planet_mesh_format PUBLIC cxx_std_23)
    target_include_directories(epoch_planet_mesh_format PUBLIC "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/include")
    include("${EPOCH_ENGINE_SOURCE_ROOT}/Engine/cmake/epoch_water_wave.cmake")
    epoch_admit_water_wave(epoch_planet_mesh_format "${EPOCH_ENGINE_SOURCE_ROOT}")
    if(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER AND NOT TARGET epoch)
        if(NOT EXISTS "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/project.planetary_runtime.ixx")
            message(FATAL_ERROR "Missing explicit Engine controller contract")
        endif()
        target_sources(epoch_planet_mesh_format PUBLIC FILE_SET engine_mesh_modules TYPE CXX_MODULES
            BASE_DIRS "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules"
            FILES "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/modules/project.planetary_runtime.ixx")
    endif()
    if(MSVC)
        target_compile_options(epoch_planet_mesh_format PUBLIC
            /sdl /permissive- /Zc:__cplusplus /utf-8)
        # These external Engine owners have unique basenames. CMake's hashed
        # external-source subdirectories can exceed MSVC's module JSON path
        # limit in an ordinary deeply nested project. Keep every compiler
        # output in the existing target/configuration-owned IntDir instead.
        foreach(_epoch_mesh_owner IN ITEMS
            modules/asset.texture_artifact.ixx modules/asset.mesh_artifact.ixx modules/asset.cloud_volume.ixx
            src/assets/texture/asset.texture_artifact.cpp modules/project.planetary_snapshot.ixx
            src/project/project.planetary_snapshot.cpp)
            get_filename_component(_epoch_mesh_name "${_epoch_mesh_owner}" NAME)
            set(_epoch_mesh_outputs "ObjectFileName=$(IntDir)${_epoch_mesh_name}.obj;ModuleDependenciesFile=$(IntDir)${_epoch_mesh_name}.module.json")
            if(_epoch_mesh_owner MATCHES "\\.ixx$")
                string(APPEND _epoch_mesh_outputs ";ModuleOutputFile=$(IntDir)${_epoch_mesh_name}.ifc")
            endif()
            set_source_files_properties("${EPOCH_ENGINE_SOURCE_ROOT}/Engine/${_epoch_mesh_owner}"
                PROPERTIES VS_SETTINGS "${_epoch_mesh_outputs}")
        endforeach()
    endif()
endif()
if(EPOCH_PLANET_BUILD_WATER)
    if(NOT EXISTS "${EPOCH_PLANET_WATER_SOURCE_ROOT}/modules/water.fft.ixx"
        OR NOT EXISTS "${EPOCH_PLANET_WATER_SOURCE_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "Planet Water requires compatible explicitly admitted FFT source")
    endif()
    set(EPOCH_FFT_BUILD_WATER_ADAPTER ON CACHE BOOL "" FORCE)
    if(NOT TARGET epoch_extension_water_fft)
        add_subdirectory("${EPOCH_PLANET_WATER_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/water-extension" EXCLUDE_FROM_ALL)
    endif()
    add_library(epoch_extension_planetary_water STATIC)
    target_sources(epoch_extension_planetary_water PUBLIC FILE_SET planet_water_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules" FILES modules/water.planetary.ixx)
    target_compile_features(epoch_extension_planetary_water PUBLIC cxx_std_23)
    target_link_libraries(epoch_extension_planetary_water PUBLIC EpochEngineExtensions::PlanetaryCore epoch_extension_water_fft)
    if(TARGET epoch)
        target_link_libraries(epoch_extension_planetary_water PUBLIC epoch)
    else()
        target_link_libraries(epoch_extension_planetary_water PUBLIC epoch_planet_mesh_format)
    endif()
endif()
if(EPOCH_PLANET_BUILD_MESH_EXPORT OR EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER)
    add_library(epoch_extension_planetary_clouds STATIC)
    target_sources(epoch_extension_planetary_clouds PUBLIC FILE_SET planetary_cloud_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules" FILES modules/planet.clouds.ixx)
    target_compile_features(epoch_extension_planetary_clouds PUBLIC cxx_std_23)
    target_link_libraries(epoch_extension_planetary_clouds PUBLIC EpochEngineExtensions::PlanetaryCore)
    if(TARGET epoch)
        target_link_libraries(epoch_extension_planetary_clouds PUBLIC epoch)
    else()
        target_link_libraries(epoch_extension_planetary_clouds PUBLIC epoch_planet_mesh_format)
    endif()
endif()
if(EPOCH_PLANET_BUILD_MESH_EXPORT)
    add_executable(epoch_planet_mesh_export tools/planet.mesh_export.cpp tools/planet.stream_export.cpp)
    target_link_libraries(epoch_planet_mesh_export PRIVATE EpochEngineExtensions::PlanetaryCore epoch_extension_planetary_clouds)
    if(TARGET epoch)
        target_link_libraries(epoch_planet_mesh_export PRIVATE epoch)
    else()
        target_link_libraries(epoch_planet_mesh_export PRIVATE epoch_planet_mesh_format)
    endif()
    target_compile_features(epoch_planet_mesh_export PRIVATE cxx_std_23)
    if(EPOCH_PLANET_BUILD_WATER)
        target_link_libraries(epoch_planet_mesh_export PRIVATE epoch_extension_planetary_water)
        target_compile_definitions(epoch_planet_mesh_export PRIVATE EPOCH_PLANET_HAS_WATER_EXTENSION=1)
    endif()
    # Patch builds run through the maintained Engine scheduler. The ordinary
    # standalone core above remains scheduler- and Engine-independent.
    include("${EPOCH_ENGINE_SOURCE_ROOT}/Engine/cmake/epoch_task_scheduler.cmake")
    epoch_admit_task_scheduler(epoch_planet_mesh_export "${EPOCH_ENGINE_SOURCE_ROOT}")
    if(BUILD_TESTING)
        add_executable(epoch_planet_parallel_streaming_contract tests/planet.parallel_streaming_consumer.cpp)
        target_link_libraries(epoch_planet_parallel_streaming_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
        epoch_admit_task_scheduler(epoch_planet_parallel_streaming_contract "${EPOCH_ENGINE_SOURCE_ROOT}")
        target_compile_features(epoch_planet_parallel_streaming_contract PRIVATE cxx_std_23)
        add_test(NAME epoch_planet_parallel_streaming_contract COMMAND epoch_planet_parallel_streaming_contract)
        set_tests_properties(epoch_planet_parallel_streaming_contract PROPERTIES TIMEOUT 30
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_planet_parallel_streaming_contract>")
    endif()
    if(WIN32)
        target_sources(epoch_planet_mesh_export PRIVATE
            "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/resource/epoch.manifest")
    endif()
    if(BUILD_TESTING)
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        if(EPOCH_PLANET_BUILD_WATER)
            add_executable(epoch_planet_water_time_contract tests/planet.water_time_consumer.cpp)
            target_link_libraries(epoch_planet_water_time_contract PRIVATE epoch_extension_planetary_water)
            target_compile_features(epoch_planet_water_time_contract PRIVATE cxx_std_23)
            add_executable(epoch_planet_water_shoreline_contract tests/water.planetary_shoreline_consumer.cpp)
            target_link_libraries(epoch_planet_water_shoreline_contract PRIVATE epoch_extension_planetary_water)
            target_compile_features(epoch_planet_water_shoreline_contract PRIVATE cxx_std_23)
            add_test(NAME epoch_planet_water_shoreline_contract
                COMMAND epoch_planet_water_shoreline_contract
                    "${CMAKE_CURRENT_BINARY_DIR}/planet-water-shoreline.txt")
            set_tests_properties(epoch_planet_water_shoreline_contract PROPERTIES TIMEOUT 30)
        endif()
        add_executable(epoch_planet_snapshot_contract tests/planet.snapshot_consumer.cpp)
        target_link_libraries(epoch_planet_snapshot_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
        if(TARGET epoch)
            target_link_libraries(epoch_planet_snapshot_contract PRIVATE epoch)
        else()
            target_link_libraries(epoch_planet_snapshot_contract PRIVATE epoch_planet_mesh_format)
        endif()
        target_compile_features(epoch_planet_snapshot_contract PRIVATE cxx_std_23)
        add_test(NAME epoch_planet_snapshot_contract COMMAND epoch_planet_snapshot_contract)
        add_test(NAME epoch_planet_patch_export_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.patch_export_contract.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --consumer "$<TARGET_FILE:epoch_planet_snapshot_contract>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        add_test(NAME epoch_planet_size_export_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.size_export_contract.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        add_test(NAME epoch_planet_mesh_export_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.mesh_export_contract.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        add_test(NAME epoch_planet_stream_export_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.stream_export_contract.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        add_test(NAME epoch_planet_view_export_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.view_export_contract.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        add_test(NAME epoch_planet_cloud_density_transport_contract
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/planet.cloud_density_transport.py"
                --producer "$<TARGET_FILE:epoch_planet_mesh_export>"
                --consumer "$<TARGET_FILE:epoch_planet_snapshot_contract>"
                --scratch "${CMAKE_CURRENT_BINARY_DIR}/test-scratch")
        set_tests_properties(epoch_planet_cloud_density_transport_contract PROPERTIES TIMEOUT 90)
    endif()
endif()

if(EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER)
    add_library(epoch_extension_planetary_runtime STATIC src/planet.exploration_runtime.cpp)
    target_sources(epoch_extension_planetary_runtime PUBLIC FILE_SET planetary_runtime_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules" FILES modules/planet.exploration_runtime.ixx)
    target_compile_features(epoch_extension_planetary_runtime PUBLIC cxx_std_23)
    target_include_directories(epoch_extension_planetary_runtime PUBLIC "${_epoch_package_include}")
    target_link_libraries(epoch_extension_planetary_runtime PUBLIC EpochEngineExtensions::PlanetaryCore epoch_extension_planetary_clouds)
    if(EPOCH_PLANET_BUILD_WATER)
        target_link_libraries(epoch_extension_planetary_runtime PUBLIC epoch_extension_planetary_water)
        target_compile_definitions(epoch_extension_planetary_runtime PRIVATE EPOCH_PLANET_HAS_WATER_EXTENSION=1)
    endif()
    if(TARGET epoch)
        # The complete Engine consumer owns these module definitions and IFCs.
        target_link_libraries(epoch_extension_planetary_runtime PUBLIC epoch)
    else()
        target_link_libraries(epoch_extension_planetary_runtime PUBLIC epoch_planet_mesh_format)
    endif()
    if(MSVC)
        target_compile_options(epoch_extension_planetary_runtime PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8)
    endif()
    if(BUILD_TESTING)
        add_executable(epoch_planet_clouds_contract tests/planet.clouds_consumer.cpp)
        target_link_libraries(epoch_planet_clouds_contract PRIVATE epoch_extension_planetary_runtime)
        target_compile_features(epoch_planet_clouds_contract PRIVATE cxx_std_23)
        add_test(NAME epoch_planet_clouds_contract COMMAND epoch_planet_clouds_contract)
        set_tests_properties(epoch_planet_clouds_contract PROPERTIES TIMEOUT 30
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_planet_clouds_contract>")
        add_executable(epoch_planet_runtime_controller_contract tests/planet.runtime_controller_consumer.cpp)
        target_link_libraries(epoch_planet_runtime_controller_contract PRIVATE epoch_extension_planetary_runtime)
        target_compile_features(epoch_planet_runtime_controller_contract PRIVATE cxx_std_23)
        if(EPOCH_PLANET_BUILD_WATER)
            target_compile_definitions(epoch_planet_runtime_controller_contract PRIVATE EPOCH_PLANET_WATER_CONTRACT=1)
        endif()
        add_test(NAME epoch_planet_runtime_controller_contract COMMAND epoch_planet_runtime_controller_contract)
        set_tests_properties(epoch_planet_runtime_controller_contract PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_planet_runtime_controller_contract>" TIMEOUT 30)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(epoch_extension_planetary_size_contract tests/planet.size_consumer.cpp)
    add_executable(epoch_planet_view_priority_contract tests/planet.view_priority_consumer.cpp)
    target_link_libraries(epoch_planet_view_priority_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_planet_view_priority_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_planet_view_priority_contract COMMAND epoch_planet_view_priority_contract
        "${CMAKE_CURRENT_BINARY_DIR}/view-priority-$<CONFIG>.json")
    set_tests_properties(epoch_planet_view_priority_contract PROPERTIES TIMEOUT 30)
    target_link_libraries(epoch_extension_planetary_size_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_extension_planetary_size_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_extension_planetary_size_contract COMMAND epoch_extension_planetary_size_contract)
    set_tests_properties(epoch_extension_planetary_size_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_extension_planetary_size_contract>")
    add_executable(epoch_extension_planetary_exploration_contract tests/planet.exploration_consumer.cpp)
    target_link_libraries(epoch_extension_planetary_exploration_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_extension_planetary_exploration_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_extension_planetary_exploration_contract COMMAND epoch_extension_planetary_exploration_contract)
    set_tests_properties(epoch_extension_planetary_exploration_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_extension_planetary_exploration_contract>")
    add_executable(epoch_extension_planetary_streaming_contract tests/planet.streaming_consumer.cpp)
    target_link_libraries(epoch_extension_planetary_streaming_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_extension_planetary_streaming_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_extension_planetary_streaming_contract COMMAND epoch_extension_planetary_streaming_contract)
    set_tests_properties(epoch_extension_planetary_streaming_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_extension_planetary_streaming_contract>")
    add_executable(epoch_extension_planetary_coverage_contract tests/planet.coverage_consumer.cpp)
    target_link_libraries(epoch_extension_planetary_coverage_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_extension_planetary_coverage_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_extension_planetary_coverage_contract COMMAND epoch_extension_planetary_coverage_contract)
    set_tests_properties(epoch_extension_planetary_coverage_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_extension_planetary_coverage_contract>")
    add_executable(epoch_extension_planetary_core_contract tests/planet.terrain_consumer.cpp)
    target_link_libraries(epoch_extension_planetary_core_contract PRIVATE EpochEngineExtensions::PlanetaryCore)
    target_compile_features(epoch_extension_planetary_core_contract PRIVATE cxx_std_23)
    add_test(NAME epoch_extension_planetary_core_contract COMMAND epoch_extension_planetary_core_contract)
    set_tests_properties(epoch_extension_planetary_core_contract PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:epoch_extension_planetary_core_contract>")
    add_test(NAME epoch_extension_planetary_terrain_self_test COMMAND epoch_extension_planetary_terrain_self_test)
endif()
