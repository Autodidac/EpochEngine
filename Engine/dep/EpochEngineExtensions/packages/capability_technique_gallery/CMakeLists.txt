cmake_minimum_required(VERSION 3.28)

project(epoch_extension_capability_technique_gallery LANGUAGES CXX)

add_library(epoch_extension_capability_technique_gallery STATIC
    src/capability_technique_gallery.cpp
)

target_include_directories(epoch_extension_capability_technique_gallery
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(epoch_extension_capability_technique_gallery PUBLIC cxx_std_23)
set_target_properties(epoch_extension_capability_technique_gallery PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_capability_technique_gallery_self_test
    tests/capability_technique_gallery_self_test.cpp
)

target_link_libraries(epoch_extension_capability_technique_gallery_self_test
    PRIVATE epoch_extension_capability_technique_gallery
)

target_compile_features(epoch_extension_capability_technique_gallery_self_test PRIVATE cxx_std_23)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME epoch_extension_capability_technique_gallery_self_test
        COMMAND epoch_extension_capability_technique_gallery_self_test
    )
endif()
