# EPOCH_MANAGED_GENERATED_FILE: cmake_lists_v2
cmake_minimum_required(VERSION 4.4)
project(EpochGeneratedProject LANGUAGES C CXX)

include("${CMAKE_CURRENT_LIST_DIR}/epoch.project.cmake")
add_executable(epoch_project_runtime source/epoch.main.cpp)
epoch_configure_embedded_project(epoch_project_runtime)
if(WIN32 AND MSVC)
    target_link_options(epoch_project_runtime PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${EPOCH_REPO_ROOT}/Engine/resource/epoch.manifest")
endif()
if(WIN32)
    set(epoch_project_output_dir "${CMAKE_CURRENT_LIST_DIR}/Artifacts/Runtime/windows/${CMAKE_BUILD_TYPE}/x64")
else()
    set(epoch_project_output_dir "${CMAKE_CURRENT_LIST_DIR}/Artifacts/Runtime/linux/${CMAKE_BUILD_TYPE}/x64")
endif()
set_target_properties(epoch_project_runtime PROPERTIES
    OUTPUT_NAME "PlatformerExample2D"
    RUNTIME_OUTPUT_DIRECTORY "${epoch_project_output_dir}")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/Assets")
    add_custom_command(TARGET epoch_project_runtime POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_LIST_DIR}/Assets"
            "$<TARGET_FILE_DIR:epoch_project_runtime>/Assets"
        VERBATIM)
endif()
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
    add_custom_command(TARGET epoch_project_runtime POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_LIST_DIR}/assets"
            "$<TARGET_FILE_DIR:epoch_project_runtime>/Assets"
        VERBATIM)
endif()
set(epoch_required_runtime_assets
    "fonts/Roboto-Regular.ttf"
    "fonts/OFL.txt"
    "vulkan/texture.ppm"
    "vulkan/shaders/vert.spv"
    "vulkan/shaders/frag.spv"
    "vulkan/shaders/gui_vert.spv"
    "vulkan/shaders/gui_frag.spv"
    "vulkan/shaders/atmosphere.vert.spv"
    "vulkan/shaders/atmosphere.frag.spv"
    "vulkan/shaders/clouds.vert.spv"
    "vulkan/shaders/clouds.frag.spv"
)
foreach(epoch_asset IN LISTS epoch_required_runtime_assets)
    set(epoch_asset_source "${EPOCH_REPO_ROOT}/Engine/assets/${epoch_asset}")
    if(NOT EXISTS "${epoch_asset_source}" OR IS_DIRECTORY "${epoch_asset_source}" OR IS_SYMLINK "${epoch_asset_source}")
        message(FATAL_ERROR "Required Engine runtime asset is missing or invalid: ${epoch_asset_source}")
    endif()
    get_filename_component(epoch_asset_directory "${epoch_asset}" DIRECTORY)
    add_custom_command(TARGET epoch_project_runtime POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/assets/${epoch_asset_directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_asset_source}" "${epoch_project_output_dir}/assets/${epoch_asset}"
        VERBATIM)
endforeach()
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochGui/LICENSE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochgui.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochGui/NOTICE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochgui-NOTICE.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochPlatformEngine/LICENSE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochplatformengine.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochPlatformEngine/NOTICE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochplatformengine-NOTICE.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochAudioEngine/LICENSE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochaudioengine.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochAudioEngine/NOTICE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochaudioengine-NOTICE.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochMediaEngine/LICENSE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochmediaengine.txt"
    VERBATIM)
set(epoch_notice_source "${EPOCH_REPO_ROOT}/Engine/dep/EpochMediaEngine/NOTICE")
if(NOT EXISTS "${epoch_notice_source}" OR IS_DIRECTORY "${epoch_notice_source}" OR IS_SYMLINK "${epoch_notice_source}")
    message(FATAL_ERROR "Required Engine dependency notice is missing or invalid: ${epoch_notice_source}")
endif()
add_custom_command(TARGET epoch_project_runtime POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${epoch_project_output_dir}/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${epoch_notice_source}" "${epoch_project_output_dir}/licenses/epochmediaengine-NOTICE.txt"
    VERBATIM)
message(STATUS "Epoch generated child target: epoch_project_runtime")
