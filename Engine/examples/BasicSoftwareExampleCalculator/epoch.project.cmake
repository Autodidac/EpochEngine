# EPOCH_MANAGED_GENERATED_FILE: cmake_fragment_v2
cmake_minimum_required(VERSION 4.4)

get_filename_component(epoch_example_repo_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(EPOCH_REPO_ROOT "${epoch_example_repo_root}" CACHE PATH "Path to the repo-local Epoch checkout")
set(BUILD_TESTING OFF CACHE BOOL "Generated projects build runtime targets only" FORCE)
set(EPOCH_BUILD_STATIC_RUNTIME ON CACHE BOOL "Build the reusable Epoch runtime" FORCE)
set(EPOCH_UPDATER_SHELL_BUILD OFF CACHE BOOL "Generated projects are not updater shells" FORCE)

if(NOT EXISTS "${EPOCH_REPO_ROOT}/Engine/CMakeLists.txt")
    message(FATAL_ERROR "EPOCH_REPO_ROOT does not contain Engine/CMakeLists.txt: ${EPOCH_REPO_ROOT}")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${EPOCH_REPO_ROOT}/Tools/build/verify_epoch_project_source.py"
    --root "${EPOCH_REPO_ROOT}" --edition "epoch.public.github.v1"
    RESULT_VARIABLE epoch_source_admission)
if(NOT epoch_source_admission EQUAL 0)
    message(FATAL_ERROR "Generated project source edition/content verification failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EPOCH_REPO_ROOT}/Engine/include/epoch.edition.hpp" "${EPOCH_REPO_ROOT}/source-projection.json")
add_custom_target(epoch_generated_source_guard
    COMMAND "${Python3_EXECUTABLE}" "${EPOCH_REPO_ROOT}/Tools/build/verify_epoch_project_source.py"
        --root "${EPOCH_REPO_ROOT}" --edition "epoch.public.github.v1" VERBATIM)

if(ANDROID)
    include("${EPOCH_REPO_ROOT}/Engine/cmake/epoch_project_android.cmake")
endif()
add_subdirectory("${EPOCH_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/epoch-engine" EXCLUDE_FROM_ALL)

add_dependencies(epoch epoch_generated_source_guard)

function(epoch_configure_embedded_project target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "epoch_configure_embedded_project target missing: ${target}")
    endif()

    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE ENGINE_STATICLIB=1 EPOCH_MAIN_IN_MAIN_CPP=1)
    target_include_directories(${target} PRIVATE
        "${EPOCH_REPO_ROOT}/Engine/include"
        "${EPOCH_REPO_ROOT}/Engine")
    target_link_libraries(${target} PRIVATE epoch)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "\$ORIGIN/lib")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DEPOCH_RUNTIME_SOURCE=$<TARGET_FILE_DIR:epoch>/lib"
                "-DEPOCH_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:${target}>/lib"
                -P "${EPOCH_REPO_ROOT}/Engine/cmake/epoch_stage_project_linux_libraries.cmake"
            VERBATIM)
    endif()
    set(epoch_dependency_fragment "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Packages/epoch.dependencies.cmake")
    if(EXISTS "${epoch_dependency_fragment}")
        include("${epoch_dependency_fragment}")
    endif()
endfunction()
