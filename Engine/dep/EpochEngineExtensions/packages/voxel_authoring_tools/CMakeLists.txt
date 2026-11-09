cmake_minimum_required(VERSION 3.28)

project(epoch_extension_voxel_authoring_tools LANGUAGES CXX)

add_library(epoch_extension_voxel_authoring_tools STATIC
    src/voxel_authoring_tools.cpp
)

target_include_directories(epoch_extension_voxel_authoring_tools
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(epoch_extension_voxel_authoring_tools PUBLIC cxx_std_23)
set_target_properties(epoch_extension_voxel_authoring_tools PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_voxel_authoring_tools_self_test
    tests/voxel_authoring_tools_self_test.cpp
)

target_link_libraries(epoch_extension_voxel_authoring_tools_self_test
    PRIVATE epoch_extension_voxel_authoring_tools
)

target_compile_features(epoch_extension_voxel_authoring_tools_self_test PRIVATE cxx_std_23)
