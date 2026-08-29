cmake_minimum_required(VERSION 3.24)

project(EpochEngineExtensionsDescriptor VERSION 0.1.0 LANGUAGES NONE)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME EpochEngineExtensions.Catalog
        COMMAND "${CMAKE_COMMAND}"
            -DCATALOG=${CMAKE_CURRENT_SOURCE_DIR}/catalog/extensions.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_catalog.cmake
    )
endif()
