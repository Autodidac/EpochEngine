# Admit the one Engine-owned sampling contract to an explicit optional consumer.
function(epoch_admit_water_wave target engine_root)
    if(TARGET epoch)
        target_link_libraries(${target} PUBLIC epoch)
        return()
    endif()
    # Older reviewed Planetary packages share the SDK's snapshot modules
    # through this entry point. Complete their module set when the current SDK
    # adds cloud recipes, without changing the attached package source.
    if(target STREQUAL "epoch_planet_mesh_format")
        get_target_property(mesh_modules ${target} CXX_MODULE_SET_engine_mesh_modules)
        list(FILTER mesh_modules INCLUDE REGEX "(^|/)asset[.]cloud_volume[.]ixx$")
        set(cloud_owner "${engine_root}/Engine/modules/asset.cloud_volume.ixx")
        if(NOT mesh_modules AND EXISTS "${cloud_owner}")
            target_sources(${target} PUBLIC FILE_SET engine_mesh_modules TYPE CXX_MODULES
                BASE_DIRS "${engine_root}/Engine/modules" FILES "${cloud_owner}")
            if(MSVC)
                set_source_files_properties("${cloud_owner}" PROPERTIES VS_SETTINGS
                    "ObjectFileName=$(IntDir)asset.cloud_volume.ixx.obj;ModuleDependenciesFile=$(IntDir)asset.cloud_volume.ixx.module.json;ModuleOutputFile=$(IntDir)asset.cloud_volume.ixx.ifc")
            endif()
        endif()
    endif()
    if(NOT TARGET epoch_water_wave)
        set(owner "${engine_root}/Engine/modules/water.wave.ixx")
        if(NOT EXISTS "${owner}")
            message(FATAL_ERROR "Missing explicit Engine Water wave contract")
        endif()
        add_library(epoch_water_wave STATIC)
        target_sources(epoch_water_wave PUBLIC FILE_SET water_wave_modules TYPE CXX_MODULES
            BASE_DIRS "${engine_root}/Engine/modules" FILES "${owner}")
        target_compile_features(epoch_water_wave PUBLIC cxx_std_23)
        if(MSVC)
            target_compile_options(epoch_water_wave PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8)
            set_source_files_properties("${owner}" PROPERTIES VS_SETTINGS
                "ObjectFileName=$(IntDir)water.wave.obj;ModuleDependenciesFile=$(IntDir)water.wave.module.json;ModuleOutputFile=$(IntDir)water.wave.ifc")
        endif()
    endif()
    target_link_libraries(${target} PUBLIC epoch_water_wave)
endfunction()
