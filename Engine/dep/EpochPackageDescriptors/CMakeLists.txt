cmake_minimum_required(VERSION 3.24)

project(EpochPackageDescriptors VERSION 0.1.0 LANGUAGES NONE)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME EpochPackageDescriptors.Models
        COMMAND "${CMAKE_COMMAND}"
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_models.cmake
    )
endif()
