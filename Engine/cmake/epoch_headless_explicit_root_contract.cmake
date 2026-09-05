# Explicit console-only regression driver. This is never run by configure,
# ordinary builds, or CTest. Fixtures/transcripts remain available for review.
cmake_minimum_required(VERSION 4.4)

if(NOT EPOCH_RUN_HEADLESS_ROOT_CONTRACT)
    message(FATAL_ERROR "Set EPOCH_RUN_HEADLESS_ROOT_CONTRACT=ON to request these actual HeadlessCI console runs.")
endif()
if(NOT DEFINED EPOCH_HEADLESS_EXECUTABLE
    OR NOT IS_ABSOLUTE "${EPOCH_HEADLESS_EXECUTABLE}"
    OR NOT EXISTS "${EPOCH_HEADLESS_EXECUTABLE}"
    OR IS_DIRECTORY "${EPOCH_HEADLESS_EXECUTABLE}")
    message(FATAL_ERROR "EPOCH_HEADLESS_EXECUTABLE must name an existing absolute HeadlessCI executable.")
endif()

get_filename_component(executable_directory "${EPOCH_HEADLESS_EXECUTABLE}" DIRECTORY)
string(RANDOM LENGTH 20 ALPHABET 0123456789abcdef fixture_id)
set(fixture "${executable_directory}/headless-root-contract-${fixture_id}")
if(EXISTS "${fixture}")
    message(FATAL_ERROR "Fresh HeadlessCI regression fixture already exists; nothing was overwritten.")
endif()
set(parent "${fixture}/parent")
set(missing "${parent}/candidates/missing")
set(candidate "${parent}/candidates/valid_without_docs")
set(invalid "${parent}/candidates/invalid")
set(contract_relative "Engine/ai/control/continuous_build_loop.json")
file(MAKE_DIRECTORY "${parent}/Engine/ai/control" "${parent}/Changes"
    "${missing}" "${candidate}/Engine/ai/control" "${invalid}/Engine/ai/control")
set(valid_contract [=[{
  "fixture": "@CASE@",
  "stages": [{"stage": "planner"}, {"stage": "builder"},
             {"stage": "verifier"}, {"stage": "gate"}],
  "policy": "Never allow blind repo write-through"
}
]=])
string(REPLACE "@CASE@" "parent" parent_contract "${valid_contract}")
string(REPLACE "@CASE@" "candidate" candidate_contract "${valid_contract}")
file(WRITE "${parent}/${contract_relative}" "${parent_contract}")
file(WRITE "${parent}/Changes/roadmap.md" "Synthetic valid ancestor fixture, not Engine source.\n")
file(WRITE "${candidate}/${contract_relative}" "${candidate_contract}")
file(WRITE "${invalid}/${contract_relative}" "{\"stages\": [{\"stage\": \"planner\"}]}\n")
file(SHA256 "${parent}/${contract_relative}" parent_digest)
file(SHA256 "${candidate}/${contract_relative}" candidate_digest)
file(SHA256 "${invalid}/${contract_relative}" invalid_digest)
message(STATUS "HeadlessCI explicit-root evidence: ${fixture}")

function(run_case name expected_exit cwd requested_root expected_root)
    if(requested_root STREQUAL "DISCOVER")
        execute_process(COMMAND "${EPOCH_HEADLESS_EXECUTABLE}"
            WORKING_DIRECTORY "${cwd}" TIMEOUT 15
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    else()
        execute_process(COMMAND "${EPOCH_HEADLESS_EXECUTABLE}" "${requested_root}"
            WORKING_DIRECTORY "${cwd}" TIMEOUT 15
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    endif()
    file(WRITE "${fixture}/${name}.stdout.log" "${output}")
    file(WRITE "${fixture}/${name}.stderr.log" "${errors}")
    file(WRITE "${fixture}/${name}.result.txt" "exit=${result}\nexpected=${expected_exit}\n")
    if(NOT "${result}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR "${name}: expected exit ${expected_exit}, received ${result}; evidence retained in ${fixture}")
    endif()
    # Use the production logger's observed root as well as exit status. A valid
    # ancestor must not make a missing/invalid nested candidate look successful.
    string(REPLACE "\\" "/" observed "${output}\n${errors}")
    string(REPLACE "\r" "" observed "${observed}")
    string(FIND "${observed}" "repo probe: ${expected_root}\n" root_position)
    if(root_position EQUAL -1)
        message(FATAL_ERROR "${name}: the production log did not identify the exact expected root ${expected_root}")
    endif()
    if(NOT expected_root STREQUAL parent)
        string(FIND "${observed}" "AI control contract: ${parent}/${contract_relative}" ancestor_position)
        if(NOT ancestor_position EQUAL -1)
            message(FATAL_ERROR "${name}: production read the ancestor's control contract")
        endif()
    endif()
    message(STATUS "${name}: PASS (exit ${result}, exact root observed)")
endfunction()

run_case(parent_control 0 "${executable_directory}" "${parent}" "${parent}")
run_case(missing_nested_contract 1 "${executable_directory}" "${missing}" "${missing}")
run_case(valid_nested_without_docs 0 "${executable_directory}" "${candidate}" "${candidate}")
run_case(invalid_nested_contract 1 "${executable_directory}" "${invalid}" "${invalid}")
run_case(relative_explicit_root 0 "${parent}" "candidates/valid_without_docs" "${candidate}")
run_case(no_argument_developer_discovery 0 "${missing}" DISCOVER "${parent}")

file(SHA256 "${parent}/${contract_relative}" parent_after)
file(SHA256 "${candidate}/${contract_relative}" candidate_after)
file(SHA256 "${invalid}/${contract_relative}" invalid_after)
if(NOT parent_after STREQUAL parent_digest
    OR NOT candidate_after STREQUAL candidate_digest
    OR NOT invalid_after STREQUAL invalid_digest
    OR EXISTS "${candidate}/Changes" OR EXISTS "${missing}/${contract_relative}")
    message(FATAL_ERROR "HeadlessCI changed a synthetic input or created replacement candidate evidence.")
endif()
file(WRITE "${fixture}/summary.txt" "PASS: six production HeadlessCI console cases; exact roots, exits and unchanged synthetic inputs verified.\n")
message(STATUS "PASS: six HeadlessCI root cases. Evidence retained: ${fixture}")
