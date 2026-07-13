cmake_minimum_required(VERSION 4.4)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(EpochGui VERSION 0.87.66 LANGUAGES CXX)

set(EPOCHGUI_SOURCE_ROOT "" CACHE PATH "Directory containing include/gui and src/epochgui.")

set(_epochgui_source_root_candidates)
if(EPOCHGUI_SOURCE_ROOT)
    list(APPEND _epochgui_source_root_candidates "${EPOCHGUI_SOURCE_ROOT}")
endif()

list(APPEND _epochgui_source_root_candidates
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

set(_epochgui_resolved_source_root "")
foreach(_epochgui_candidate IN LISTS _epochgui_source_root_candidates)
    get_filename_component(_epochgui_candidate_abs "${_epochgui_candidate}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(EXISTS "${_epochgui_candidate_abs}/include/gui/floating_window.hpp"
        AND EXISTS "${_epochgui_candidate_abs}/src/epochgui/floating_window.cpp")
        set(_epochgui_resolved_source_root "${_epochgui_candidate_abs}")
        break()
    endif()
endforeach()

if(NOT _epochgui_resolved_source_root)
    message(FATAL_ERROR
        "EpochGui could not find its source root. Expected include/gui and "
        "src/epochgui either beside this CMakeLists.txt or two directories above it. "
        "Pass -DEPOCHGUI_SOURCE_ROOT=<path> to select another source root.")
endif()

set(EPOCHGUI_SOURCE_ROOT "${_epochgui_resolved_source_root}" CACHE PATH "Directory containing include/gui and src/epochgui." FORCE)

set(EPOCHGUI_MODULE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Directory containing modules/epoch.gui.ixx." FORCE)

set(EPOCHGUI_MODULES
    "${EPOCHGUI_MODULE_ROOT}/modules/epoch.gui.ixx"
)

set(EPOCHGUI_PUBLIC_HEADERS
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/dockable_window.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/dock_layout.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/floating_window.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/layout_primitives.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/panel_host.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/popup_layout.hpp"
    "${EPOCHGUI_SOURCE_ROOT}/include/gui/version.hpp"
)

set(EPOCHGUI_SOURCES
    "${EPOCHGUI_SOURCE_ROOT}/src/epochgui/dockable_window.cpp"
    "${EPOCHGUI_SOURCE_ROOT}/src/epochgui/dock_layout.cpp"
    "${EPOCHGUI_SOURCE_ROOT}/src/epochgui/floating_window.cpp"
    "${EPOCHGUI_SOURCE_ROOT}/src/epochgui/panel_host.cpp"
    "${EPOCHGUI_SOURCE_ROOT}/src/epochgui/popup_layout.cpp"
)

add_library(EpochGui STATIC
    ${EPOCHGUI_PUBLIC_HEADERS}
    ${EPOCHGUI_SOURCES}
)

target_sources(EpochGui PUBLIC
    FILE_SET epochgui_modules TYPE CXX_MODULES
        BASE_DIRS "${EPOCHGUI_MODULE_ROOT}/modules"
        FILES ${EPOCHGUI_MODULES}
)

add_library(epoch_gui ALIAS EpochGui)
add_library(Autodidac::EpochGui ALIAS EpochGui)

target_include_directories(EpochGui PUBLIC
    "$<BUILD_INTERFACE:${EPOCHGUI_SOURCE_ROOT}/include>"
)

target_compile_features(EpochGui PUBLIC cxx_std_23)

set_target_properties(EpochGui PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    EXPORT_NAME EpochGui
    OUTPUT_NAME EpochGui
)

if(MSVC)
    target_compile_options(EpochGui PRIVATE /permissive-)
endif()

source_group(TREE "${EPOCHGUI_SOURCE_ROOT}/include" PREFIX "Header Files" FILES ${EPOCHGUI_PUBLIC_HEADERS})
source_group(TREE "${EPOCHGUI_SOURCE_ROOT}/src" PREFIX "Source Files" FILES ${EPOCHGUI_SOURCES})
source_group(TREE "${EPOCHGUI_MODULE_ROOT}/modules" PREFIX "Module Files" FILES ${EPOCHGUI_MODULES})
