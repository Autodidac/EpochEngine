cmake_minimum_required(VERSION 3.22.1)

project(EpochRoot LANGUAGES C CXX)

if(CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR
        "Epoch's module-aware CMake build currently requires CMake 3.28 or newer. "
        "If you are on an older baseline such as 3.22.1, use the checked-in "
        "Visual Studio/MSBuild solution or update CMake before configuring the "
        "CMake build path.")
endif()

include(CTest)

add_subdirectory(Engine)
