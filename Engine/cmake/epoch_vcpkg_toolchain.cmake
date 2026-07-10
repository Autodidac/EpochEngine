# Epoch uses this wrapper from tracked presets so IDEs do not expand an empty
# VCPKG_ROOT into /scripts/buildsystems/vcpkg.cmake.

set(_epoch_vcpkg_candidates)

if(DEFINED EPOCH_VCPKG_ROOT AND NOT "${EPOCH_VCPKG_ROOT}" STREQUAL "")
    list(APPEND _epoch_vcpkg_candidates "${EPOCH_VCPKG_ROOT}")
endif()
if(DEFINED ENV{EPOCH_VCPKG_ROOT} AND NOT "$ENV{EPOCH_VCPKG_ROOT}" STREQUAL "")
    list(APPEND _epoch_vcpkg_candidates "$ENV{EPOCH_VCPKG_ROOT}")
endif()
if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    list(APPEND _epoch_vcpkg_candidates "$ENV{VCPKG_ROOT}")
endif()

get_filename_component(_epoch_engine_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(_epoch_repo_root "${_epoch_engine_root}/.." ABSOLUTE)
get_filename_component(_epoch_repo_parent "${_epoch_repo_root}/.." ABSOLUTE)
list(APPEND _epoch_vcpkg_candidates
    "${_epoch_repo_root}/vcpkg"
    "${_epoch_repo_parent}/vcpkg")

if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND _epoch_vcpkg_candidates
        "$ENV{HOME}/vcpkg"
        "$ENV{HOME}/Documents/repos/vcpkg"
        "$ENV{HOME}/source/repos/vcpkg")
endif()

set(_epoch_vcpkg_root "")
foreach(_epoch_candidate IN LISTS _epoch_vcpkg_candidates)
    if(EXISTS "${_epoch_candidate}/scripts/buildsystems/vcpkg.cmake")
        file(REAL_PATH "${_epoch_candidate}" _epoch_vcpkg_root)
        break()
    endif()
endforeach()

if("${_epoch_vcpkg_root}" STREQUAL "")
    message(FATAL_ERROR
        "Epoch could not locate vcpkg. Set VCPKG_ROOT or EPOCH_VCPKG_ROOT to a checkout containing "
        "scripts/buildsystems/vcpkg.cmake. Checked the Engine-adjacent checkout, the repository sibling, "
        "and standard HOME locations.")
endif()

set(VCPKG_ROOT "${_epoch_vcpkg_root}" CACHE PATH "Epoch vcpkg checkout" FORCE)
set(ENV{VCPKG_ROOT} "${_epoch_vcpkg_root}")
set(VCPKG_FEATURE_FLAGS "manifests" CACHE STRING "vcpkg feature flags" FORCE)
set(ENV{VCPKG_FEATURE_FLAGS} "manifests")

if(UNIX AND NOT APPLE)
    if(NOT DEFINED CMAKE_C_COMPILER OR "${CMAKE_C_COMPILER}" STREQUAL "")
        find_program(_epoch_clang_c NAMES clang-20 clang-19 clang-18 clang)
        if(_epoch_clang_c)
            set(CMAKE_C_COMPILER "${_epoch_clang_c}" CACHE FILEPATH "Epoch Linux C compiler" FORCE)
        endif()
    endif()

    if(NOT DEFINED CMAKE_CXX_COMPILER OR "${CMAKE_CXX_COMPILER}" STREQUAL "")
        find_program(_epoch_clang_cxx NAMES clang++-20 clang++-19 clang++-18 clang++)
        if(_epoch_clang_cxx)
            set(CMAKE_CXX_COMPILER "${_epoch_clang_cxx}" CACHE FILEPATH "Epoch Linux C++ compiler" FORCE)
        endif()
    endif()

    if(NOT DEFINED CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS OR "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}" STREQUAL "")
        find_program(_epoch_clang_scan_deps NAMES clang-scan-deps-20 clang-scan-deps-19 clang-scan-deps-18 clang-scan-deps)
        if(_epoch_clang_scan_deps)
            set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${_epoch_clang_scan_deps}" CACHE FILEPATH "Epoch Clang module scanner" FORCE)
        endif()
    endif()
endif()

include("${_epoch_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
