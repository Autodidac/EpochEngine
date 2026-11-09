cmake_minimum_required(VERSION 3.28)

project(epoch_extension_engine_client_listen_server LANGUAGES CXX)

add_library(epoch_extension_engine_client_listen_server STATIC
    src/engine_client_listen_server.cpp
)

target_include_directories(epoch_extension_engine_client_listen_server
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../engine_network_runtime/include"
)

target_compile_features(epoch_extension_engine_client_listen_server PUBLIC cxx_std_23)
set_target_properties(epoch_extension_engine_client_listen_server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_engine_client_listen_server_self_test
    tests/engine_client_listen_server_self_test.cpp
)

target_link_libraries(epoch_extension_engine_client_listen_server_self_test
    PRIVATE epoch_extension_engine_client_listen_server
)

target_compile_features(epoch_extension_engine_client_listen_server_self_test PRIVATE cxx_std_23)
