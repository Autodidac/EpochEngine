cmake_minimum_required(VERSION 3.30)
project(EpochAndroidModuleProbe LANGUAGES CXX)

set(epoch_android_probe_abis arm64-v8a x86_64)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Android" OR NOT ANDROID_ABI IN_LIST epoch_android_probe_abis)
    message(FATAL_ERROR "Configure with an Android NDK toolchain and ANDROID_ABI=arm64-v8a or x86_64.")
endif()

get_filename_component(engine "${CMAKE_CURRENT_SOURCE_DIR}/../../../Engine" ABSOLUTE)
add_library(epoch_android_probe STATIC)
target_sources(epoch_android_probe PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${engine}/modules/render"
    FILES "${engine}/modules/render/render.math.ixx")
target_compile_features(epoch_android_probe PUBLIC cxx_std_23)
set_target_properties(epoch_android_probe PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

add_executable(epoch_android_module_consumer platform.android_module_contract.cpp)
target_link_libraries(epoch_android_module_consumer PRIVATE epoch_android_probe)
set_target_properties(epoch_android_module_consumer PROPERTIES CXX_EXTENSIONS OFF)

add_library(epoch_android_capability SHARED platform.android_gles_contract.cpp)
target_link_libraries(epoch_android_capability PRIVATE epoch_android_probe android EGL GLESv3 log)
set_target_properties(epoch_android_capability PROPERTIES CXX_EXTENSIONS OFF)
