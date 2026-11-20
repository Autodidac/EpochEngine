# Android generated applications use SDL's Activity and the engine runtime.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(EPOCH_BINARY_ONLY_DISTRIBUTION ON)
set(EPOCH_ENABLE_NATIVE_AI OFF)
set(EPOCH_ENABLE_PHYSICAL_AUDIO OFF)
set(EPOCH_ENABLE_PHYSICAL_INPUT OFF)
set(EPOCH_ENABLE_SDL ON)
set(EPOCH_ENABLE_RAYLIB OFF)
set(EPOCH_ENABLE_SFML OFF)
set(EPOCH_ENABLE_VULKAN OFF)
set(EPOCH_ENABLE_OPENGL OFF)
set(EPOCH_ENABLE_DIRECTX OFF)
set(EPOCH_ENABLE_SOFTWARE_RENDERER OFF)
set(EPOCH_REQUIRE_OPTIONAL_DEPENDENCIES ON)

find_package(SDL3 CONFIG REQUIRED)

function(epoch_add_android_project_runtime)
    add_library(epoch_project_runtime SHARED source/epoch.main.cpp)
    epoch_configure_embedded_project(epoch_project_runtime)
    target_link_libraries(epoch_project_runtime PRIVATE SDL3::SDL3)
    set(planetary_pose_pins
        EPOCH_ANDROID_PLANETARY_POSE_SHA256
        EPOCH_ANDROID_PLANETARY_RECIPE_SHA256
        EPOCH_ANDROID_PLANETARY_MANIFEST_SHA256
        EPOCH_ANDROID_PLANETARY_SCENE_SHA256)
    set(has_planetary_pose OFF)
    foreach(pin IN LISTS planetary_pose_pins)
        if(DEFINED ${pin} AND NOT "${${pin}}" STREQUAL "")
            set(has_planetary_pose ON)
        endif()
    endforeach()
    if(EPOCH_ANDROID_PLANETARY_FLIGHT)
        if(NOT has_planetary_pose OR EPOCH_ANDROID_FIXED_SHORELINE_SHA256 OR EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256)
            message(FATAL_ERROR "Android planetary flight requires a packaged pose without a fixed shoreline or terrain cut.")
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/source/epoch.main.cpp" flight_entry_marker
            LIMIT_COUNT 1 REGEX "EPOCH_ANDROID_PLANETARY_FLIGHT")
        if(NOT flight_entry_marker)
            message(FATAL_ERROR "Refresh the managed Android Game entry before opting into planetary flight.")
        endif()
    endif()
    if(EPOCH_ANDROID_SHORELINE_FLIGHT)
        if(NOT EPOCH_ANDROID_FIXED_SHORELINE_SHA256 OR has_planetary_pose)
            message(FATAL_ERROR "Android shoreline flight requires a packaged shoreline descriptor.")
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/source/epoch.main.cpp" shoreline_entry_marker
            LIMIT_COUNT 1 REGEX "EPOCH_ANDROID_SHORELINE_FLIGHT")
        if(NOT shoreline_entry_marker)
            message(FATAL_ERROR "Refresh the managed Android Game entry before opting into shoreline flight.")
        endif()
    endif()
    if(NOT "${EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256}" STREQUAL "")
        if(NOT has_planetary_pose OR EPOCH_ANDROID_FIXED_SHORELINE_SHA256)
            message(FATAL_ERROR "A packaged planetary terrain cut requires a packaged planetary pose.")
        endif()
        string(LENGTH "${EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256}" terrain_pin_length)
        if(NOT terrain_pin_length EQUAL 64 OR NOT "${EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256}" MATCHES "^[a-f0-9]+$")
            message(FATAL_ERROR "A packaged planetary terrain cut requires a valid EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256.")
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/source/epoch.main.cpp" terrain_entry_marker
            LIMIT_COUNT 1 REGEX "EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256")
        if(NOT terrain_entry_marker)
            message(FATAL_ERROR "Refresh the managed Android Game entry before packaging a planetary terrain cut.")
        endif()
    endif()
    if(has_planetary_pose)
        if(EPOCH_ANDROID_FIXED_SHORELINE_SHA256)
            message(FATAL_ERROR "Choose either a packaged planetary pose or a fixed shoreline.")
        endif()
        foreach(pin IN LISTS planetary_pose_pins)
            string(LENGTH "${${pin}}" pin_length)
            if(NOT pin_length EQUAL 64 OR NOT "${${pin}}" MATCHES "^[a-f0-9]+$")
                message(FATAL_ERROR "A packaged planetary pose requires a valid ${pin}.")
            endif()
        endforeach()
        get_target_property(planet_hash epoch_project_runtime EPOCH_PLANETARY_SOURCE_HASH)
        get_target_property(fft_hash epoch_project_runtime EPOCH_FFT_SOURCE_HASH)
        if(fft_hash STREQUAL "fft_hash-NOTFOUND" AND EPOCH_ANDROID_PLANETARY_FLIGHT)
            file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Worlds/Planet.epoch_planet" dry_recipe
                REGEX "^(mode|stream_lod|ocean_surface)=")
            if(NOT "mode=planet" IN_LIST dry_recipe OR NOT "stream_lod=1" IN_LIST dry_recipe
                OR "ocean_surface=1" IN_LIST dry_recipe)
                message(FATAL_ERROR "Flight without FFT requires a dry single-body Planet recipe with streaming enabled.")
            endif()
            set(fft_hash "")
        endif()
        string(LENGTH "${planet_hash}" planet_hash_length)
        string(LENGTH "${fft_hash}" fft_hash_length)
        if(NOT planet_hash_length EQUAL 71 OR NOT "${planet_hash}" MATCHES "^sha256:[a-f0-9]+$"
            OR (NOT "${fft_hash}" STREQUAL "" AND (NOT fft_hash_length EQUAL 71 OR NOT "${fft_hash}" MATCHES "^sha256:[a-f0-9]+$"))
            OR ("${fft_hash}" STREQUAL "" AND NOT EPOCH_ANDROID_PLANETARY_FLIGHT))
            message(FATAL_ERROR "A packaged pose requires attached Planetary source. FFT is required for a wet pose.")
        endif()
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/source/epoch.main.cpp" pose_entry_marker
            LIMIT_COUNT 1 REGEX "epochengine::project::AndroidPackagedPlanetaryPose[{]EPOCH_ANDROID_PLANETARY_POSE_SHA256")
        if(NOT pose_entry_marker)
            message(FATAL_ERROR "Refresh the managed Android Game entry before opting into a packaged planetary pose.")
        endif()
        foreach(pin IN LISTS planetary_pose_pins)
            target_compile_definitions(epoch_project_runtime PRIVATE ${pin}="${${pin}}")
        endforeach()
        target_compile_definitions(epoch_project_runtime PRIVATE
            EPOCH_ANDROID_PLANETARY_SOURCE_HASH="${planet_hash}"
            EPOCH_ANDROID_FFT_SOURCE_HASH="${fft_hash}"
            EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256="${EPOCH_ANDROID_PLANETARY_TERRAIN_SHA256}"
            EPOCH_ANDROID_PLANETARY_FLIGHT=$<BOOL:${EPOCH_ANDROID_PLANETARY_FLIGHT}>)
    endif()
    if(EPOCH_ANDROID_FIXED_SHORELINE_SHA256)
        string(LENGTH "${EPOCH_ANDROID_FIXED_SHORELINE_SHA256}" pin_length)
        get_target_property(planet_hash epoch_project_runtime EPOCH_PLANETARY_SOURCE_HASH)
        get_target_property(fft_hash epoch_project_runtime EPOCH_FFT_SOURCE_HASH)
        if(NOT pin_length EQUAL 64 OR NOT EPOCH_ANDROID_FIXED_SHORELINE_SHA256 MATCHES "^[a-f0-9]+$"
            OR NOT planet_hash MATCHES "^sha256:[a-f0-9]+$" OR NOT fft_hash MATCHES "^sha256:[a-f0-9]+$")
            message(FATAL_ERROR "A fixed shoreline requires a descriptor hash and attached Planetary and FFT sources.")
        endif()
        target_compile_definitions(epoch_project_runtime PRIVATE
            EPOCH_ANDROID_FIXED_SHORELINE_SHA256="${EPOCH_ANDROID_FIXED_SHORELINE_SHA256}"
            EPOCH_ANDROID_SHORELINE_FLIGHT=$<BOOL:${EPOCH_ANDROID_SHORELINE_FLIGHT}>
            EPOCH_ANDROID_PLANETARY_SOURCE_HASH="${planet_hash}"
            EPOCH_ANDROID_FFT_SOURCE_HASH="${fft_hash}")
    endif()
    set_target_properties(epoch_project_runtime PROPERTIES
        OUTPUT_NAME main
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apk/jniLibs/${ANDROID_ABI}")
endfunction()
