# Reuse the existing Engine scheduler in small supervised CPU consumers.
# This admits canonical module owners; it never implements another worker pool.
include_guard(GLOBAL)
function(epoch_admit_task_scheduler consumer engine_source_root)
    if(TARGET epoch)
        target_link_libraries(${consumer} PRIVATE epoch)
        return()
    endif()
    if(NOT TARGET epoch_task_scheduler)
        set(_engine "${engine_source_root}/Engine")
        if(NOT EXISTS "${_engine}/modules/editor/editor.task_scheduler.ixx")
            message(FATAL_ERROR "Missing explicit Engine scheduler source")
        endif()
        if(NOT TARGET Epoch::PlatformEngine)
            # Options remain local to this admission function. Native platform
            # code is linked, but this consumer starts no platform application.
            set(EPOCH_PLATFORM_BUILD_STATIC ON)
            set(EPOCH_PLATFORM_BUILD_SHARED OFF)
            set(EPOCH_PLATFORM_BUILD_EXAMPLES OFF)
            set(EPOCH_PLATFORM_BUILD_TESTS OFF)
            set(EPOCH_PLATFORM_BUILD_MODULES ON)
            add_subdirectory("${_engine}/dep/EpochPlatformEngine"
                "${CMAKE_CURRENT_BINARY_DIR}/epoch_scheduler_platform" EXCLUDE_FROM_ALL)
            if(MSVC)
                # Standard-header environment must agree across all IFC owners.
                foreach(_platform_owner IN ITEMS EpochPlatformEngineStatic EpochPlatformModuleStatic)
                    target_compile_options(${_platform_owner} PUBLIC /sdl /utf-8 /permissive- /Zc:__cplusplus /EHsc)
                endforeach()
            endif()
        endif()
        add_library(epoch_task_scheduler STATIC)
        add_library(Epoch::TaskScheduler ALIAS epoch_task_scheduler)
        target_link_libraries(epoch_task_scheduler PUBLIC Epoch::PlatformEngine)
        target_sources(epoch_task_scheduler PUBLIC
            FILE_SET scheduler_modules TYPE CXX_MODULES
            BASE_DIRS "${_engine}/modules"
            FILES "${_engine}/modules/core/core.logger.ixx"
                  "${_engine}/modules/core/core.mpmc_bounded_queue.ixx"
                  "${_engine}/modules/systems/systems.registry.ixx"
                  "${_engine}/modules/systems/systems.task.ixx"
                  "${_engine}/modules/taskgraph/taskgraph.dotsystem.ixx"
                  "${_engine}/modules/editor/editor.task_scheduler.ixx")
        target_include_directories(epoch_task_scheduler PUBLIC "${_engine}/include")
        target_compile_features(epoch_task_scheduler PUBLIC cxx_std_23)
        set_target_properties(epoch_task_scheduler PROPERTIES CXX_SCAN_FOR_MODULES ON)
        if(MSVC)
            target_compile_options(epoch_task_scheduler PUBLIC /sdl /utf-8 /permissive- /Zc:__cplusplus /EHsc)
            # Canonical external owners have unique names. Keep scanner outputs
            # below MAX_PATH even for nested project source attachments.
            foreach(_owner IN ITEMS core.logger core.mpmc_bounded_queue systems.registry
                    systems.task taskgraph.dotsystem editor.task_scheduler)
                string(REGEX MATCH "^[^.]+" _category "${_owner}")
                set_source_files_properties("${_engine}/modules/${_category}/${_owner}.ixx" PROPERTIES VS_SETTINGS
                    "ObjectFileName=$(IntDir)${_owner}.obj;ModuleDependenciesFile=$(IntDir)${_owner}.module.json;ModuleOutputFile=$(IntDir)${_owner}.ifc")
            endforeach()
        endif()
    endif()
    target_link_libraries(${consumer} PRIVATE Epoch::TaskScheduler)
endfunction()
