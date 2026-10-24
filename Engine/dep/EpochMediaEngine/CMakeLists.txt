cmake_minimum_required(VERSION 3.28)
project(EpochMediaEngine VERSION 0.1.0 LANGUAGES CXX)
add_library(epoch_media_engine STATIC
)
add_library(Epoch::MediaEngine ALIAS epoch_media_engine)
target_compile_features(epoch_media_engine PUBLIC cxx_std_23)
set_target_properties(epoch_media_engine PROPERTIES CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES ON POSITION_INDEPENDENT_CODE ON)
target_sources(epoch_media_engine PUBLIC FILE_SET epoch_media_engine_modules TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
    FILES
        modules/media.timeline_preview.ixx
)
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()
if(BUILD_TESTING)
    add_executable(epoch_media_timeline_preview_contract tests/media.timeline_preview_contract.cpp)
    target_link_libraries(epoch_media_timeline_preview_contract PRIVATE Epoch::MediaEngine)
    target_compile_definitions(epoch_media_timeline_preview_contract PRIVATE EPOCH_MEDIA_TIMELINE_PREVIEW_CONTRACT_MAIN=1)
    add_test(NAME epoch_media_timeline_preview_contract COMMAND epoch_media_timeline_preview_contract)
endif()

option(EPOCHMEDIA_BUILD_GUI_DEMO "Build the shared interactive library demonstration" OFF)
set(EPOCHGUI_DEMO_ROOT "" CACHE PATH "Path to reviewed standalone EpochGui_Demo source")
if(EPOCHMEDIA_BUILD_GUI_DEMO)
    if(NOT EPOCHGUI_DEMO_ROOT OR NOT EXISTS "${EPOCHGUI_DEMO_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "EPOCHMEDIA_BUILD_GUI_DEMO requires explicit EPOCHGUI_DEMO_ROOT; no Engine fallback")
    endif()
    set(EPOCHGUI_DEMO_COMPOSITION media)
    set(EPOCHGUI_DEMO_LIBRARY_PAGE ON)
    set(EPOCHGUI_DEMO_APP_NAME EpochMediaDemo)
    add_subdirectory("${EPOCHGUI_DEMO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/demonstration")
endif()
