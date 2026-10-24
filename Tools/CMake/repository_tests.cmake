# Shared by root and Engine-only CMake builds. All fixtures have one source home.
function(epoch_repository_test_assertions target)
    # These CPU fixtures use assert(); keep checks active in Release too.
    target_compile_options("${target}" PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>")
endfunction()
set(_epoch_repository_root "${CMAKE_CURRENT_LIST_DIR}/../..")
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME epoch_python_contracts
        COMMAND "${Python3_EXECUTABLE}" -X utf8 "${_epoch_repository_root}/Tools/run_tests.py")
    set_tests_properties(epoch_python_contracts PROPERTIES
        WORKING_DIRECTORY "${_epoch_repository_root}" TIMEOUT 180)
endif()

foreach(_epoch_fixture IN ITEMS
        
        
        
        
        
        
        
        
        
        
        gui/gui.theme_cycle_test.cpp)
    get_filename_component(_epoch_name "${_epoch_fixture}" NAME_WLE)
    string(REPLACE "." "_" _epoch_name "${_epoch_name}")
    set(_epoch_target "epoch_${_epoch_name}")
    add_executable("${_epoch_target}" "${_epoch_repository_root}/Tests/${_epoch_fixture}")
    set_target_properties("${_epoch_target}" PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    epoch_apply_cxx_dialect("${_epoch_target}")
    epoch_apply_warnings("${_epoch_target}")
    epoch_apply_sanitizers("${_epoch_target}")
    epoch_repository_test_assertions("${_epoch_target}")
    target_include_directories("${_epoch_target}" PRIVATE "${_epoch_repository_root}/Engine/include")
    
        
    
    add_test(NAME "${_epoch_target}" COMMAND "${_epoch_target}")
    set_tests_properties("${_epoch_target}" PROPERTIES
        TIMEOUT 20 WORKING_DIRECTORY "${_epoch_repository_root}")
endforeach()

# The production routing policy must run in both root and Engine-only builds.










if(WIN32)
    add_executable(epoch_file_drop_contract "${_epoch_repository_root}/Tests/platform/platform.file_drop_contract.cpp")
    set_target_properties(epoch_file_drop_contract PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    epoch_apply_cxx_dialect(epoch_file_drop_contract)
    epoch_apply_warnings(epoch_file_drop_contract)
    epoch_repository_test_assertions(epoch_file_drop_contract)
    add_test(NAME epoch_file_drop_contract COMMAND epoch_file_drop_contract)
    set_tests_properties(epoch_file_drop_contract PROPERTIES TIMEOUT 20)
    add_executable(epoch_image_codec_contract "${_epoch_repository_root}/Tests/platform/platform.image_codec_contract.cpp")
    set_target_properties(epoch_image_codec_contract PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    epoch_apply_cxx_dialect(epoch_image_codec_contract)
    epoch_apply_warnings(epoch_image_codec_contract)
    epoch_repository_test_assertions(epoch_image_codec_contract)
    add_test(NAME epoch_image_codec_contract COMMAND epoch_image_codec_contract)
    set_tests_properties(epoch_image_codec_contract PROPERTIES TIMEOUT 20)
endif()

find_package(nlohmann_json CONFIG QUIET)
if(TARGET nlohmann_json::nlohmann_json)
    
    
    
    
    
    
    
    
    

    
    
    
    
    
    
    
    
    

    
    
    
    
    
    
    
    
endif()
