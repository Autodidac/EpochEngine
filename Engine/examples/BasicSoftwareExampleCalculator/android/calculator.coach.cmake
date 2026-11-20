function(epoch_add_calculator_android_coach runtime)
    set(EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE "" CACHE FILEPATH "Local, pinned b12503 llama.cpp source archive")
    if(NOT IS_ABSOLUTE "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}"
        OR NOT EXISTS "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}"
        OR IS_DIRECTORY "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}"
        OR IS_SYMLINK "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}")
        message(FATAL_ERROR "Supply the local b12503 archive with EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE.")
    endif()
    file(SHA256 "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}" archive_digest)
    if(NOT archive_digest STREQUAL "cb0058aa0d883a1f8b3704b85158776dd0c67fbd1e6922a361ec1f74d82f9381")
        message(FATAL_ERROR "Calculator Coach llama.cpp archive did not match the admitted source.")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}")
    set(source_parent "${CMAKE_CURRENT_BINARY_DIR}/coach-source")
    set(llama_source "${source_parent}/llama.cpp-4577932fab595eaa8b27ad71734d5de1f3c07d8c")
    if(NOT EXISTS "${llama_source}/CMakeLists.txt")
        file(MAKE_DIRECTORY "${source_parent}")
        file(ARCHIVE_EXTRACT INPUT "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}" DESTINATION "${source_parent}")
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/verify_coach_source.py"
            "${EPOCH_CALCULATOR_COACH_LLAMA_ARCHIVE}" "${llama_source}"
        RESULT_VARIABLE source_verified)
    if(NOT source_verified EQUAL 0)
        message(FATAL_ERROR "The extracted Calculator Coach source failed verification. Use a fresh build directory.")
    endif()

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(BUILD_SHARED_LIBS OFF)
    set(GIT_EXE OFF)
    set(LLAMA_BUILD_COMMIT "4577932")
    set(LLAMA_BUILD_NUMBER 12503)
    foreach(flag IN ITEMS GGML_NATIVE GGML_OPENMP GGML_LLAMAFILE GGML_BLAS GGML_CUDA GGML_VULKAN
        GGML_METAL GGML_RPC GGML_BACKEND_DL LLAMA_OPENSSL LLAMA_BUILD_COMMON LLAMA_BUILD_TOOLS
        LLAMA_BUILD_EXAMPLES LLAMA_BUILD_SERVER LLAMA_BUILD_TESTS LLAMA_BUILD_APP)
        set(${flag} OFF)
    endforeach()
    add_subdirectory("${llama_source}" "${CMAKE_CURRENT_BINARY_DIR}/coach-llama" EXCLUDE_FROM_ALL)

    target_compile_definitions(${runtime} PRIVATE EPOCH_CALCULATOR_ANDROID_COACH=1)
    add_library(epoch_calculator_coach_worker SHARED
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../source/calculator.coach_android_worker.cpp")
    target_compile_features(epoch_calculator_coach_worker PRIVATE cxx_std_23)
    target_link_libraries(epoch_calculator_coach_worker PRIVATE llama)
    target_link_options(epoch_calculator_coach_worker PRIVATE "LINKER:--exclude-libs,ALL" "LINKER:--no-undefined")
    set_target_properties(epoch_calculator_coach_worker PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        OUTPUT_NAME epoch_coach
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apk/jniLibs/${ANDROID_ABI}")
    add_dependencies(${runtime} epoch_calculator_coach_worker)
endfunction()
