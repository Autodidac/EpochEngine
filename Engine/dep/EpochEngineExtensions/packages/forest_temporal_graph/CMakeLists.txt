cmake_minimum_required(VERSION 3.28)

project(epoch_extension_forest_temporal_graph LANGUAGES CXX)

add_library(epoch_extension_forest_temporal_graph STATIC
    src/forest_temporal_graph.cpp
)

target_include_directories(epoch_extension_forest_temporal_graph
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(epoch_extension_forest_temporal_graph PUBLIC cxx_std_23)
set_target_properties(epoch_extension_forest_temporal_graph PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_forest_temporal_graph_self_test
    tests/forest_temporal_graph_self_test.cpp
)

target_link_libraries(epoch_extension_forest_temporal_graph_self_test
    PRIVATE epoch_extension_forest_temporal_graph
)

target_compile_features(epoch_extension_forest_temporal_graph_self_test PRIVATE cxx_std_23)
