cmake_minimum_required(VERSION 4.4)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(EpochEngineExtensions VERSION 0.2.3 LANGUAGES CXX)

set(
    EPOCH_ENGINE_MODULE_DIR
    ""
    CACHE PATH
    "Absolute path to the authoritative EpochEngine module directory."
)

if(NOT EPOCH_ENGINE_MODULE_DIR)
    message(FATAL_ERROR
        "EPOCH_ENGINE_MODULE_DIR is required. Pass the EpochEngine modules "
        "directory that owns render.math and terrain.foundation."
    )
endif()

get_filename_component(
    EPOCH_ENGINE_MODULE_DIR
    "${EPOCH_ENGINE_MODULE_DIR}"
    ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(_epoch_required_core_modules
    "${EPOCH_ENGINE_MODULE_DIR}/render.math.ixx"
    "${EPOCH_ENGINE_MODULE_DIR}/terrain.foundation.ixx"
)
foreach(_epoch_core_module IN LISTS _epoch_required_core_modules)
    if(NOT EXISTS "${_epoch_core_module}")
        message(FATAL_ERROR
            "Required authoritative EpochEngine module is missing: "
            "${_epoch_core_module}"
        )
    endif()
endforeach()

set(
    EPOCH_TIERED_TERRAIN_MODULE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/demonstrations/tiered_terrain/modules"
)

add_library(EpochTieredTerrain STATIC)
add_library(EpochEngineExtensions::TieredTerrain ALIAS EpochTieredTerrain)

target_sources(EpochTieredTerrain PUBLIC
    FILE_SET epoch_tiered_terrain_modules TYPE CXX_MODULES
        BASE_DIRS
            "${EPOCH_ENGINE_MODULE_DIR}"
            "${EPOCH_TIERED_TERRAIN_MODULE_DIR}"
        FILES
            ${_epoch_required_core_modules}
            "${EPOCH_TIERED_TERRAIN_MODULE_DIR}/tiered_terrain.generator.ixx"
)

target_compile_features(EpochTieredTerrain PUBLIC cxx_std_23)
set_target_properties(EpochTieredTerrain PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME EpochTieredTerrain
)

if(MSVC)
    target_compile_options(EpochTieredTerrain PRIVATE /permissive- /W4)
else()
    target_compile_options(EpochTieredTerrain PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

option(EPOCH_BUILD_SOURCE_PACKAGES "Build the preserved standalone source packages" ON)
if(EPOCH_BUILD_SOURCE_PACKAGES)
    set(_epoch_source_packages
        capability_technique_gallery
        engine_authoritative_dedicated_server
        engine_client_listen_server
        engine_network_runtime
        fft_ocean
        forest_temporal_graph
        local_ai_llama_cpp_runtime
        planetary_terrain
        voxel_authoring_tools
        voxel_planetoid_lod
    )
    foreach(_epoch_package IN LISTS _epoch_source_packages)
        add_subdirectory("packages/${_epoch_package}")
        if(BUILD_TESTING)
            set(_epoch_package_test "epoch_extension_${_epoch_package}_self_test")
            get_property(_epoch_registered_tests
                DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packages/${_epoch_package}"
                PROPERTY TESTS
            )
            if(NOT _epoch_package_test IN_LIST _epoch_registered_tests)
                add_test(NAME "${_epoch_package_test}" COMMAND "${_epoch_package_test}")
            endif()
        endif()
    endforeach()
endif()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
        NAME EpochEngineExtensions.PackageManifests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_packages.py"
    )
    add_test(
        NAME EpochEngineExtensions.SourcePackaging
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_source.py"
    )
    add_executable(EpochTieredTerrainContract
        demonstrations/tiered_terrain/tests/tiered_terrain.generator_contract.cpp
    )
    target_link_libraries(EpochTieredTerrainContract PRIVATE EpochTieredTerrain)
    target_compile_features(EpochTieredTerrainContract PRIVATE cxx_std_23)
    set_target_properties(EpochTieredTerrainContract PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(EpochTieredTerrainContract PRIVATE /permissive- /W4)
    else()
        target_compile_options(EpochTieredTerrainContract PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(
        NAME EpochEngineExtensions.TieredTerrain
        COMMAND EpochTieredTerrainContract
    )
    add_test(
        NAME EpochEngineExtensions.Catalog
        COMMAND "${CMAKE_COMMAND}"
            -DCATALOG=${CMAKE_CURRENT_SOURCE_DIR}/catalog/extensions.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_catalog.cmake
    )
endif()
