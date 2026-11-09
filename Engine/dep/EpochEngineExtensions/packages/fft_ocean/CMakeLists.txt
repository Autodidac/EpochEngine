cmake_minimum_required(VERSION 3.28)

project(epoch_extension_fft_ocean LANGUAGES CXX)

add_library(epoch_extension_fft_ocean STATIC
    src/fft_ocean.cpp
)

target_include_directories(epoch_extension_fft_ocean
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(epoch_extension_fft_ocean PUBLIC cxx_std_23)
set_target_properties(epoch_extension_fft_ocean PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

add_executable(epoch_extension_fft_ocean_self_test
    tests/fft_ocean_self_test.cpp
)

target_link_libraries(epoch_extension_fft_ocean_self_test
    PRIVATE epoch_extension_fft_ocean
)

target_compile_features(epoch_extension_fft_ocean_self_test PRIVATE cxx_std_23)

# Optional Engine Water consumer. Standalone FFT builds retain no Engine
# dependency. The admitted project supplies the exact Engine/source roots.
option(EPOCH_FFT_BUILD_WATER_ADAPTER "Build the shared Engine Water implementation adapter" OFF)
if(EPOCH_FFT_BUILD_WATER_ADAPTER)
    if(NOT EXISTS "${EPOCH_ENGINE_SOURCE_ROOT}/Engine/cmake/epoch_water_wave.cmake")
        message(FATAL_ERROR "Water adapter requires explicit Engine source")
    endif()
    if(MSVC)
        set(_epoch_fft_include "${CMAKE_CURRENT_BINARY_DIR}/finc")
        file(MAKE_DIRECTORY "${_epoch_fft_include}/epoch/extensions/fft_ocean")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/epoch/extensions/fft_ocean/fft_ocean.hpp"
            "${_epoch_fft_include}/epoch/extensions/fft_ocean/fft_ocean.hpp" COPYONLY)
        target_include_directories(epoch_extension_fft_ocean BEFORE PUBLIC "${_epoch_fft_include}")
        target_compile_options(epoch_extension_fft_ocean PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8)
    endif()
    add_library(epoch_extension_water_fft STATIC)
    target_sources(epoch_extension_water_fft PUBLIC FILE_SET water_fft_modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules" FILES modules/water.fft.ixx)
    target_compile_features(epoch_extension_water_fft PUBLIC cxx_std_23)
    target_link_libraries(epoch_extension_water_fft PUBLIC epoch_extension_fft_ocean)
    include("${EPOCH_ENGINE_SOURCE_ROOT}/Engine/cmake/epoch_water_wave.cmake")
    epoch_admit_water_wave(epoch_extension_water_fft "${EPOCH_ENGINE_SOURCE_ROOT}")
endif()
