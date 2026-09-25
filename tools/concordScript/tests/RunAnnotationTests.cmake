cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CONCORDC OR NOT DEFINED FIXTURE_ROOT OR NOT DEFINED EXAMPLE_ROOT OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "CONCORDC, FIXTURE_ROOT, EXAMPLE_ROOT, and TEST_ROOT are required")
endif()

function(assert_file_contains path needle)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected generated file does not exist: ${path}")
    endif()
    file(READ "${path}" contents)
    string(FIND "${contents}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${path} does not contain expected text: ${needle}")
    endif()
endfunction()

function(assert_file_not_contains path needle)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected generated file does not exist: ${path}")
    endif()
    file(READ "${path}" contents)
    string(FIND "${contents}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${path} unexpectedly contains text: ${needle}")
    endif()
endfunction()

function(assert_file_starts_with path prefix)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected generated file does not exist: ${path}")
    endif()
    file(READ "${path}" contents)
    string(LENGTH "${prefix}" prefix_length)
    string(LENGTH "${contents}" contents_length)
    if(contents_length LESS prefix_length)
        message(FATAL_ERROR "${path} is shorter than expected prefix")
    endif()
    string(SUBSTRING "${contents}" 0 ${prefix_length} actual)
    if(NOT actual STREQUAL prefix)
        message(FATAL_ERROR "${path} does not start with expected text: ${prefix}")
    endif()
endfunction()

set(positive_out "${TEST_ROOT}/positive")
file(REMOVE_RECURSE "${positive_out}")
execute_process(
    COMMAND "${CONCORDC}" --cpp --project "${FIXTURE_ROOT}/annotations" --out "${positive_out}"
    RESULT_VARIABLE positive_result
    OUTPUT_VARIABLE positive_stdout
    ERROR_VARIABLE positive_stderr)
if(NOT positive_result EQUAL 0)
    message(FATAL_ERROR "Positive annotation fixture failed (${positive_result}): ${positive_stdout}\n${positive_stderr}")
endif()

assert_file_contains("${positive_out}/Main.gen.h" "#include <vector>")
assert_file_contains("${positive_out}/Main.gen.h" "#include \"my/local.hpp\"")
assert_file_contains("${positive_out}/Main.gen.h" "#pragma once")
assert_file_contains("${positive_out}/Main.gen.h" "#include <string>")
assert_file_contains("${positive_out}/Main.gen.h" "AnnotationString")
assert_file_contains("${positive_out}/Main.gen.h" "AnnotationHeaderType")
assert_file_contains("${positive_out}/Main.gen.h" "metadata @binding")
assert_file_contains("${positive_out}/Main.gen.h" "metadata @feature")
assert_file_contains("${positive_out}/Main.gen.h" "metadata @pipeline")
assert_file_contains("${positive_out}/Main.gen.cpp" "metadata @entrymeta")
assert_file_contains("${positive_out}/Main.gen.cpp" "UserVulkanState")
assert_file_contains("${positive_out}/Main.gen.cpp" "RegisterVulkanPass")
assert_file_contains("${positive_out}/Main.gen.cpp" "VulkanPassPhase::BeforeScene")
assert_file_contains("${positive_out}/Main.gen.cpp" "VulkanPassPhase::Initialize")
assert_file_contains("${positive_out}/Main.gen.cpp" "VulkanPassPhase::Shutdown")
assert_file_contains("${positive_out}/Main.gen.cpp" "fixture_vulkan_pass")
assert_file_contains("${positive_out}/Main.gen.cpp" "fixture_positional_pass")
assert_file_contains("${positive_out}/Main.gen.cpp" "PositionalFixturePass")
assert_file_contains("${positive_out}/Main.gen.cpp" "raw_return_guard")
assert_file_contains("${positive_out}/Main.gen.cpp" "conditional_return")
assert_file_contains("${positive_out}/Main.gen.cpp" ".callback = (FixturePointer)")
assert_file_contains("${positive_out}/Main.gen.cpp" "metadata @tool::standalone")
assert_file_contains("${positive_out}/Raw.gen.cpp" "@shader(name=ignored)")
assert_file_contains("${positive_out}/Raw.gen.cpp" "metadata @rawmeta")
assert_file_contains("${positive_out}/Raw.annotations.json" "rawmeta")
assert_file_contains("${positive_out}/ConcordScriptRegistry.gen.cpp" "Main::Registered")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"toon\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"frag\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"mesh\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"vert\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_primary\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rgen\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_miss\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rmiss\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_hit\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rchit\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_any\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rahit\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_intersection\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rint\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"ray_callable\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"rcall\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"inline_compute\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"mixed_named\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"alias_mixed\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"stage\": \"comp\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"hlsl_vertex\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"language\": \"hlsl\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"external\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"vertex\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"external\": true")
assert_file_contains("${positive_out}/Main.annotations.json" "\"name\": \"external_standalone\"")
assert_file_contains("${positive_out}/Main.annotations.json" "assets/standalone.frag")
assert_file_contains("${positive_out}/Main.annotations.json" "\"path\": \"custom/toon.frag\"")
assert_file_contains("${positive_out}/Main.annotations.json" "\"defines\": [\"TOON=1\"]")
assert_file_contains("${positive_out}/Main.annotations.json" "\"includeDirs\": [\".\"]")
assert_file_contains("${positive_out}/Main.annotations.json" "\"options\": [\"-g\"]")
assert_file_contains("${positive_out}/Main.annotations.json" "\"path\": \"custom/mesh.vert\"")
assert_file_contains("${positive_out}/ConcordScriptManifest.json" "\"stage\": \"frag\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "CONCORDSCRIPT_SHADER_SOURCES")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "CONCORDSCRIPT_SHADER_ENTRIES")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "concordscript_attach_external_shaders")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "custom/toon.frag")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "concordscript_attach_vulkan")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "concordscript_attach_external_shaders")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "ray_primary")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "\"rgen\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "\"rmiss\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "assets/external.frag")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "assets/standalone.frag")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "DEFINES \"TOON=1\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "INCLUDE_DIRS \".\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "OPTIONS \"-g\"")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "CONCORDSCRIPT_METADATA_HEADER")
assert_file_contains("${positive_out}/ConcordScriptShaders.cmake" "concordscript_attach_metadata")
assert_file_contains("${positive_out}/ConcordScriptManifest.json" "ConcordScriptMetadata.gen.h")
assert_file_contains("${positive_out}/ConcordScriptMetadata.gen.h" "namespace Concord::Script::Metadata")
assert_file_contains("${positive_out}/ConcordScriptMetadata.gen.h" "FindShader")
assert_file_contains("${positive_out}/custom/toon.frag" "layout(location = 0) out vec4 color;")
assert_file_contains("${positive_out}/custom/mesh.vert" "gl_Position")
assert_file_starts_with("${positive_out}/shaders/Main.custom_entry.frag" "#version 460")
assert_file_starts_with("${positive_out}/shaders/Main.ray_miss.rmiss" "#version 460")
assert_file_starts_with("${positive_out}/shaders/Main.ray_hit.rchit" "#version 460")

set(edge_out "${TEST_ROOT}/parser_edges")
file(REMOVE_RECURSE "${edge_out}")
execute_process(
    COMMAND "${CONCORDC}" --cpp --project "${FIXTURE_ROOT}/parser_edges" --out "${edge_out}"
    RESULT_VARIABLE edge_result
    OUTPUT_VARIABLE edge_stdout
    ERROR_VARIABLE edge_stderr)
if(NOT edge_result EQUAL 0)
    message(FATAL_ERROR "Parser edge fixture failed (${edge_result}): ${edge_stdout}\n${edge_stderr}")
endif()
assert_file_contains("${edge_out}/Main.gen.cpp" "template<class T>")
assert_file_contains("${edge_out}/Main.gen.cpp" "class NativeFinal final")
assert_file_contains("${edge_out}/Main.gen.h" "class Registered")
assert_file_contains("${edge_out}/Main.gen.h" "R\"tag(var pub priv { @not_an_annotation })tag\"")
assert_file_contains("${edge_out}/Main.gen.h" "auto value = 9")
assert_file_contains("${edge_out}/Main.gen.cpp" "R\"tag(@fake { var pub })tag\"")
assert_file_contains("${edge_out}/Main.gen.cpp" "u8R\"tag(@fake { var priv })tag\"")
assert_file_contains("${edge_out}/Main.gen.cpp" "LR\"tag(@fake { var prot })tag\"")

set(std_packages_out "${TEST_ROOT}/std_packages")
file(REMOVE_RECURSE "${std_packages_out}")
execute_process(
    COMMAND "${CONCORDC}" --cpp --project "${FIXTURE_ROOT}/std_packages" --out "${std_packages_out}"
    RESULT_VARIABLE std_packages_result
    OUTPUT_VARIABLE std_packages_stdout
    ERROR_VARIABLE std_packages_stderr)
if(NOT std_packages_result EQUAL 0)
    message(FATAL_ERROR "std.xxx package fixture failed (${std_packages_result}): ${std_packages_stdout}\n${std_packages_stderr}")
endif()
assert_file_contains("${std_packages_out}/Main.gen.h" "#include <vector>")
assert_file_contains("${std_packages_out}/Main.gen.h" "#include <string>")
assert_file_contains("${std_packages_out}/Main.gen.h" "#include <optional>")
assert_file_contains("${std_packages_out}/Main.gen.h" "#include <Concord/CApplication.h>")
assert_file_not_contains("${std_packages_out}/Main.gen.h" "vector.gen.h")
assert_file_contains("${edge_out}/Main.annotations.json" "\"key\": \"type\", \"value\": \"std::array<int, 4>\"")
assert_file_contains("${edge_out}/Main.gen.cpp" "R\"tag({ @inside, var })tag\"")
assert_file_contains("${edge_out}/Main.annotations.json" "\"name\": \"comparison\"")
assert_file_contains("${edge_out}/Main.annotations.json" "\"key\": \"right\", \"value\": \"2\"")
assert_file_contains("${edge_out}/Main.annotations.json" "std::map<std::string, std::vector<int>>")
assert_file_contains("${edge_out}/Main.annotations.json" "\"name\": \"pipeline\"")
assert_file_contains("${edge_out}/String.annotations.json" "\"key\": \"value\"")
assert_file_contains("${edge_out}/String.annotations.json" "\"key\": \"next\", \"value\": \"2\"")
assert_file_contains("${edge_out}/String.annotations.json" "\"name\": \"raw_delimiter\"")
assert_file_contains("${edge_out}/String.annotations.json" "\"key\": \"next\", \"value\": \"3\"")
assert_file_contains("${edge_out}/Forward.gen.cpp" "class Forward;")
assert_file_contains("${edge_out}/ConcordScriptRegistry.gen.cpp" "Main::Registered")

include("${positive_out}/ConcordScriptShaders.cmake")
if(NOT CONCORDSCRIPT_USES_VULKAN)
    message(FATAL_ERROR "Generated CMake fragment did not detect @vulkan")
endif()
list(LENGTH CONCORDSCRIPT_SHADER_SOURCES source_count)
list(LENGTH CONCORDSCRIPT_SHADER_STAGES stage_count)
list(LENGTH CONCORDSCRIPT_SHADER_ENTRIES entry_count)
list(LENGTH CONCORDSCRIPT_SHADER_LANGUAGES language_count)
list(LENGTH CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES external_source_count)
list(LENGTH CONCORDSCRIPT_EXTERNAL_SHADER_STAGES external_stage_count)
list(LENGTH CONCORDSCRIPT_EXTERNAL_SHADER_ENTRIES external_entry_count)
list(LENGTH CONCORDSCRIPT_EXTERNAL_SHADER_LANGUAGES external_language_count)
if(NOT source_count EQUAL 14 OR NOT stage_count EQUAL 14 OR
   NOT entry_count EQUAL 14 OR NOT language_count EQUAL 14 OR
   NOT external_source_count EQUAL 2 OR NOT external_stage_count EQUAL 2 OR
   NOT external_entry_count EQUAL 2 OR NOT external_language_count EQUAL 2)
    message(FATAL_ERROR "Generated shader metadata lists have inconsistent lengths")
endif()
list(FIND CONCORDSCRIPT_SHADER_SOURCES "${positive_out}/assets/external.frag" external_index)
if(NOT external_index EQUAL -1)
    message(FATAL_ERROR "External shader was incorrectly added to inline shader sources")
endif()
list(FIND CONCORDSCRIPT_EXTERNAL_SHADER_SOURCES "${positive_out}/assets/external.frag" external_index)
if(external_index EQUAL -1)
    message(FATAL_ERROR "External shader was not added to external shader sources")
endif()

set(metadata_build "${TEST_ROOT}/metadata_consumer_build")
file(REMOVE_RECURSE "${metadata_build}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/metadata_consumer"
            -B "${metadata_build}" "-DGENERATED_DIR=${positive_out}"
    RESULT_VARIABLE metadata_configure_result
    OUTPUT_VARIABLE metadata_configure_stdout
    ERROR_VARIABLE metadata_configure_stderr)
if(NOT metadata_configure_result EQUAL 0)
    message(FATAL_ERROR "Metadata consumer configure failed: ${metadata_configure_stdout}\n${metadata_configure_stderr}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${metadata_build}"
    RESULT_VARIABLE metadata_build_result
    OUTPUT_VARIABLE metadata_build_stdout
    ERROR_VARIABLE metadata_build_stderr)
if(NOT metadata_build_result EQUAL 0)
    message(FATAL_ERROR "Metadata consumer build failed: ${metadata_build_stdout}\n${metadata_build_stderr}")
endif()
file(GLOB_RECURSE metadata_executables LIST_DIRECTORIES false
     "${metadata_build}/metadata_consumer"
     "${metadata_build}/metadata_consumer.exe")
if(NOT metadata_executables)
    file(GLOB_RECURSE metadata_executables LIST_DIRECTORIES false
         "${metadata_build}/*/metadata_consumer"
         "${metadata_build}/*/metadata_consumer.exe")
endif()
list(LENGTH metadata_executables metadata_executable_count)
if(metadata_executable_count EQUAL 0)
    message(FATAL_ERROR "Metadata consumer executable was not produced")
endif()
list(GET metadata_executables 0 metadata_executable)
execute_process(
    COMMAND "${metadata_executable}"
    RESULT_VARIABLE metadata_run_result
    OUTPUT_VARIABLE metadata_run_stdout
    ERROR_VARIABLE metadata_run_stderr)
if(NOT metadata_run_result EQUAL 0)
    message(FATAL_ERROR "Metadata consumer failed (${metadata_run_result}): ${metadata_run_stdout}\n${metadata_run_stderr}")
endif()

foreach(case_name IN ITEMS invalid_duplicate invalid_path invalid_shader_collision
                              invalid_reserved_path invalid_stage invalid_section
                              invalid_shader_name invalid_shader_ambiguous invalid_empty_argument invalid_unterminated
                              invalid_raw_string
                              invalid_pass_callback invalid_pass_phase invalid_pass_order
                              invalid_pass_userdata invalid_pass_name_positional
                              invalid_vulkan_positional_pass
                               invalid_shader_unknown invalid_shader_extra
                               invalid_shader_alias_conflict invalid_shader_alias_positional
                               invalid_shader_alias_fields
                               invalid_pass_unknown
                               invalid_pass_extra invalid_pass_alias_fields invalid_include_path
                               invalid_include_alias_fields
                              invalid_multi_entry invalid_same_file_entry invalid_no_entry invalid_pass_name
                              invalid_module_case invalid_keyword_module invalid_std_package)
    set(negative_out "${TEST_ROOT}/${case_name}")
    file(REMOVE_RECURSE "${negative_out}")
    execute_process(
        COMMAND "${CONCORDC}" --project "${FIXTURE_ROOT}/${case_name}" --out "${negative_out}"
        RESULT_VARIABLE negative_result
        OUTPUT_VARIABLE negative_stdout
        ERROR_VARIABLE negative_stderr)
    if(negative_result EQUAL 0)
        message(FATAL_ERROR "Negative annotation fixture unexpectedly succeeded: ${case_name}")
    endif()
    if(case_name STREQUAL "invalid_duplicate")
        set(expected_error "duplicate argument")
    elseif(case_name STREQUAL "invalid_path")
        set(expected_error "shader path")
    elseif(case_name STREQUAL "invalid_shader_collision")
        set(expected_error "shader path collision")
    elseif(case_name STREQUAL "invalid_reserved_path")
        set(expected_error "reserved")
    elseif(case_name STREQUAL "invalid_stage")
        set(expected_error "unsupported shader stage")
    elseif(case_name STREQUAL "invalid_section")
        set(expected_error "section must")
    elseif(case_name STREQUAL "invalid_shader_name")
        set(expected_error "duplicate shader")
    elseif(case_name STREQUAL "invalid_shader_ambiguous")
        set(expected_error "ambiguous shader positional")
    elseif(case_name STREQUAL "invalid_empty_argument")
        set(expected_error "empty argument")
    elseif(case_name STREQUAL "invalid_unterminated")
        set(expected_error "unterminated")
    elseif(case_name STREQUAL "invalid_raw_string")
        set(expected_error "unterminated raw string")
    elseif(case_name STREQUAL "invalid_pass_callback")
        set(expected_error "pass requires callback")
    elseif(case_name STREQUAL "invalid_pass_phase")
        set(expected_error "pass phase")
    elseif(case_name STREQUAL "invalid_pass_order")
        set(expected_error "pass order")
    elseif(case_name STREQUAL "invalid_pass_userdata")
        set(expected_error "pass userData")
    elseif(case_name STREQUAL "invalid_pass_name_positional")
        set(expected_error "duplicate Vulkan pass")
    elseif(case_name STREQUAL "invalid_vulkan_positional_pass")
        set(expected_error "duplicate Vulkan pass")
    elseif(case_name STREQUAL "invalid_shader_unknown")
        set(expected_error "unknown shader argument")
    elseif(case_name STREQUAL "invalid_shader_extra")
        set(expected_error "too many positional arguments")
    elseif(case_name STREQUAL "invalid_shader_alias_conflict")
        set(expected_error "shader stage alias")
    elseif(case_name STREQUAL "invalid_shader_alias_positional")
        set(expected_error "shader stage alias")
    elseif(case_name STREQUAL "invalid_shader_alias_fields")
        set(expected_error "mutually exclusive")
    elseif(case_name STREQUAL "invalid_pass_unknown")
        set(expected_error "unknown pass argument")
    elseif(case_name STREQUAL "invalid_pass_extra")
        set(expected_error "too many positional arguments")
    elseif(case_name STREQUAL "invalid_pass_alias_fields")
        set(expected_error "mutually exclusive")
    elseif(case_name STREQUAL "invalid_include_path")
        set(expected_error "include path is invalid")
    elseif(case_name STREQUAL "invalid_include_alias_fields")
        set(expected_error "mutually exclusive")
    elseif(case_name STREQUAL "invalid_multi_entry")
        set(expected_error "multiple @entry")
    elseif(case_name STREQUAL "invalid_same_file_entry")
        set(expected_error "multiple @entry")
    elseif(case_name STREQUAL "invalid_no_entry")
        set(expected_error "no @entry")
    elseif(case_name STREQUAL "invalid_pass_name")
        set(expected_error "duplicate Vulkan pass")
    elseif(case_name STREQUAL "invalid_module_case")
        set(expected_error "namespace collision")
    elseif(case_name STREQUAL "invalid_keyword_module")
        set(expected_error "invalid module name")
    elseif(case_name STREQUAL "invalid_std_package")
        set(expected_error "did you mean 'std.vector'")
    endif()
    string(FIND "${negative_stderr}" "${expected_error}" error_position)
    if(error_position EQUAL -1)
        message(FATAL_ERROR "${case_name} did not report '${expected_error}': ${negative_stderr}")
    endif()
    if(EXISTS "${negative_out}/Main.gen.cpp")
        message(FATAL_ERROR "Failed translation wrote a generated source file: ${case_name}")
    endif()
    if(EXISTS "${negative_out}")
        file(GLOB_RECURSE negative_files LIST_DIRECTORIES false "${negative_out}/*")
        if(negative_files)
            message(FATAL_ERROR "Failed translation left output files: ${case_name}")
        endif()
    endif()
endforeach()

set(cvm_out "${TEST_ROOT}/cvm_demo")
file(REMOVE_RECURSE "${cvm_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_demo" --out "${cvm_out}" --cvm aot
    RESULT_VARIABLE cvm_result
    OUTPUT_VARIABLE cvm_stdout
    ERROR_VARIABLE cvm_stderr)
if(NOT cvm_result EQUAL 0)
    message(FATAL_ERROR "CVM AOT fixture failed (${cvm_result}): ${cvm_stdout}\n${cvm_stderr}")
endif()
foreach(artifact ConcordVisualMachine.ll ConcordVisualMachine.bc ConcordVisualMachine.obj)
    if(NOT EXISTS "${cvm_out}/${artifact}")
        message(FATAL_ERROR "CVM AOT did not produce ${artifact}")
    endif()
endforeach()
assert_file_contains("${cvm_out}/ConcordVisualMachine.ll" "target triple =")
assert_file_contains("${cvm_out}/ConcordVisualMachine.ll" "define i64 @main()")
assert_file_contains("${cvm_out}/ConcordVisualMachine.ll" "declare i64 @print(i64)")

set(cvm_jit_out "${TEST_ROOT}/cvm_demo_jit")
file(REMOVE_RECURSE "${cvm_jit_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_demo" --out "${cvm_jit_out}" --cvm jit
    RESULT_VARIABLE cvm_jit_result
    OUTPUT_VARIABLE cvm_jit_stdout
    ERROR_VARIABLE cvm_jit_stderr)
if(NOT cvm_jit_result EQUAL 0)
    message(FATAL_ERROR "CVM JIT fixture failed (${cvm_jit_result}): ${cvm_jit_stdout}\n${cvm_jit_stderr}")
endif()
string(FIND "${cvm_jit_stdout}" "cvm: 42" print_position)
if(print_position EQUAL -1)
    message(FATAL_ERROR "CVM JIT did not print 42: ${cvm_jit_stdout}")
endif()
string(FIND "${cvm_jit_stdout}" "cvm jit result: 30" result_position)
if(result_position EQUAL -1)
    message(FATAL_ERROR "CVM JIT result was not 30: ${cvm_jit_stdout}")
endif()

set(cvm_no_run_out "${TEST_ROOT}/cvm_demo_norun")
file(REMOVE_RECURSE "${cvm_no_run_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_demo" --out "${cvm_no_run_out}" --no-run
    RESULT_VARIABLE cvm_no_run_result
    OUTPUT_VARIABLE cvm_no_run_stdout
    ERROR_VARIABLE cvm_no_run_stderr)
if(NOT cvm_no_run_result EQUAL 0)
    message(FATAL_ERROR "CVM --no-run fixture failed (${cvm_no_run_result}): ${cvm_no_run_stdout}\n${cvm_no_run_stderr}")
endif()
if(NOT EXISTS "${cvm_no_run_out}/ConcordVisualMachine.ll")
    message(FATAL_ERROR "CVM --no-run did not write LLVM IR")
endif()

# The CVM language core: locals, assignment, branches, loops and one @cvm
# function calling another. The printed values are a hand-computed oracle for
# the alternating-sign loop, so a regression in any one construct shows up here.
set(cvm_language_out "${TEST_ROOT}/cvm_language")
file(REMOVE_RECURSE "${cvm_language_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_language" --out "${cvm_language_out}"
    RESULT_VARIABLE cvm_language_result
    OUTPUT_VARIABLE cvm_language_stdout
    ERROR_VARIABLE cvm_language_stderr)
if(NOT cvm_language_result EQUAL 0)
    message(FATAL_ERROR "CVM language fixture failed (${cvm_language_result}): ${cvm_language_stdout}\n${cvm_language_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_language_values "${cvm_language_stdout}")
if(NOT cvm_language_values STREQUAL "cvm: 5;cvm: 100")
    message(FATAL_ERROR "CVM language printed unexpected values: ${cvm_language_values}\n${cvm_language_stdout}")
endif()
string(FIND "${cvm_language_stdout}" "cvm jit result: 2" cvm_language_result_position)
if(cvm_language_result_position EQUAL -1)
    message(FATAL_ERROR "CVM language did not return 2: ${cvm_language_stdout}")
endif()

# Short-circuit && / || (mark() must not run), unary !, and break / continue.
set(cvm_logic_out "${TEST_ROOT}/cvm_logic")
file(REMOVE_RECURSE "${cvm_logic_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_logic" --out "${cvm_logic_out}"
    RESULT_VARIABLE cvm_logic_result
    OUTPUT_VARIABLE cvm_logic_stdout
    ERROR_VARIABLE cvm_logic_stderr)
if(NOT cvm_logic_result EQUAL 0)
    message(FATAL_ERROR "CVM logic fixture failed (${cvm_logic_result}): ${cvm_logic_stdout}\n${cvm_logic_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_logic_values "${cvm_logic_stdout}")
list(JOIN cvm_logic_values "|" cvm_logic_actual)
if(NOT cvm_logic_actual STREQUAL "cvm: 2|cvm: 3|cvm: 25")
    message(FATAL_ERROR "CVM logic printed unexpected values: ${cvm_logic_actual}\n${cvm_logic_stdout}")
endif()

# for, += / str +=, true/false, ternary, continue-to-step, and for(;;).
set(cvm_syntax_out "${TEST_ROOT}/cvm_syntax")
file(REMOVE_RECURSE "${cvm_syntax_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_syntax" --out "${cvm_syntax_out}"
    RESULT_VARIABLE cvm_syntax_result
    OUTPUT_VARIABLE cvm_syntax_stdout
    ERROR_VARIABLE cvm_syntax_stderr)
if(NOT cvm_syntax_result EQUAL 0)
    message(FATAL_ERROR "CVM syntax fixture failed (${cvm_syntax_result}): ${cvm_syntax_stdout}\n${cvm_syntax_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_syntax_values "${cvm_syntax_stdout}")
list(JOIN cvm_syntax_values "|" cvm_syntax_actual)
if(NOT cvm_syntax_actual STREQUAL "cvm: 21|cvm: 8|cvm: 3|cvm: 3")
    message(FATAL_ERROR "CVM syntax printed unexpected values: ${cvm_syntax_actual}\n${cvm_syntax_stdout}")
endif()
assert_file_contains("${cvm_syntax_out}/ConcordVisualMachine.ll" "for.cond")
assert_file_contains("${cvm_syntax_out}/ConcordVisualMachine.ll" "tern.merge")
assert_file_contains("${cvm_syntax_out}/ConcordVisualMachine.ll" "cvm_str_concat")

# Text-file save round-trip. The working directory is a throwaway folder so the
# fixture does not write into the source tree.
set(cvm_file_work "${TEST_ROOT}/cvm_file_work")
file(REMOVE_RECURSE "${cvm_file_work}")
file(MAKE_DIRECTORY "${cvm_file_work}")
set(cvm_file_out "${TEST_ROOT}/cvm_file")
file(REMOVE_RECURSE "${cvm_file_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_file" --out "${cvm_file_out}"
    WORKING_DIRECTORY "${cvm_file_work}"
    RESULT_VARIABLE cvm_file_result
    OUTPUT_VARIABLE cvm_file_stdout
    ERROR_VARIABLE cvm_file_stderr)
if(NOT cvm_file_result EQUAL 0)
    message(FATAL_ERROR "CVM file fixture failed (${cvm_file_result}): ${cvm_file_stdout}\n${cvm_file_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_file_values "${cvm_file_stdout}")
list(JOIN cvm_file_values "|" cvm_file_actual)
if(NOT cvm_file_actual STREQUAL "cvm: 0|cvm: 1|cvm: 1|cvm: 42")
    message(FATAL_ERROR "CVM file printed unexpected values: ${cvm_file_actual}\n${cvm_file_stdout}")
endif()

# The built-in standard library: containers, four real threads, an atomic
# counter, a mutex and the clock. Every value is deterministic; 180 is the
# sum of four threads each returning 45, so it only appears if all four ran
# and thread_join returned what the entry function returned.
set(cvm_stdlib_out "${TEST_ROOT}/cvm_stdlib")
file(REMOVE_RECURSE "${cvm_stdlib_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_stdlib" --out "${cvm_stdlib_out}"
    RESULT_VARIABLE cvm_stdlib_result
    OUTPUT_VARIABLE cvm_stdlib_stdout
    ERROR_VARIABLE cvm_stdlib_stderr)
if(NOT cvm_stdlib_result EQUAL 0)
    message(FATAL_ERROR "CVM stdlib fixture failed (${cvm_stdlib_result}): ${cvm_stdlib_stdout}\n${cvm_stdlib_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_stdlib_values "${cvm_stdlib_stdout}")
if(NOT cvm_stdlib_values STREQUAL "cvm: 3;cvm: 20;cvm: 2;cvm: 30;cvm: 0;cvm: 26;cvm: 8;cvm: 180;cvm: 42;cvm: 1;cvm: 1;cvm: 1")
    message(FATAL_ERROR "CVM stdlib printed unexpected values: ${cvm_stdlib_values}\n${cvm_stdlib_stdout}")
endif()
string(FIND "${cvm_stdlib_stdout}" "cvm jit result: 0" cvm_stdlib_result_position)
if(cvm_stdlib_result_position EQUAL -1)
    message(FATAL_ERROR "CVM stdlib did not return 0: ${cvm_stdlib_stdout}")
endif()

set(cvm_math_out "${TEST_ROOT}/cvm_math")
file(REMOVE_RECURSE "${cvm_math_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_math" --out "${cvm_math_out}"
    RESULT_VARIABLE cvm_math_result
    OUTPUT_VARIABLE cvm_math_stdout
    ERROR_VARIABLE cvm_math_stderr)
if(NOT cvm_math_result EQUAL 0)
    message(FATAL_ERROR "CVM math fixture failed (${cvm_math_result}): ${cvm_math_stdout}\n${cvm_math_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_math_values "${cvm_math_stdout}")
list(JOIN cvm_math_values "|" cvm_math_actual)
if(NOT cvm_math_actual STREQUAL "cvm: 0|cvm: 1|cvm: 3|cvm: 2.5|cvm: 1|cvm: 3|cvm: 2|cvm: 2.5|cvm: 180|cvm: 1|cvm: -1|cvm: 2|cvm: 3|cvm: 1|cvm: 7|cvm: 1|cvm: 3|cvm: 1|cvm: 1|cvm: 1")
    message(FATAL_ERROR "CVM math printed unexpected values: ${cvm_math_actual}\n${cvm_math_stdout}")
endif()

# AOT must reference the runtime library by name: the emitted object is linked
# against ConcordCvmRuntime, so these declarations are the link contract.
set(cvm_stdlib_aot_out "${TEST_ROOT}/cvm_stdlib_aot")
file(REMOVE_RECURSE "${cvm_stdlib_aot_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_stdlib" --out "${cvm_stdlib_aot_out}" --cvm aot
    RESULT_VARIABLE cvm_stdlib_aot_result
    OUTPUT_VARIABLE cvm_stdlib_aot_stdout
    ERROR_VARIABLE cvm_stdlib_aot_stderr)
if(NOT cvm_stdlib_aot_result EQUAL 0)
    message(FATAL_ERROR "CVM stdlib AOT failed (${cvm_stdlib_aot_result}): ${cvm_stdlib_aot_stdout}\n${cvm_stdlib_aot_stderr}")
endif()
if(NOT EXISTS "${cvm_stdlib_aot_out}/ConcordVisualMachine.obj")
    message(FATAL_ERROR "CVM stdlib AOT did not produce an object file")
endif()
assert_file_contains("${cvm_stdlib_aot_out}/ConcordVisualMachine.ll" "define i64 @worker()")
assert_file_contains("${cvm_stdlib_aot_out}/ConcordVisualMachine.ll" "declare i64 @cvm_list_new()")
assert_file_contains("${cvm_stdlib_aot_out}/ConcordVisualMachine.ll" "declare i64 @cvm_list_push(i64, i64)")
assert_file_contains("${cvm_stdlib_aot_out}/ConcordVisualMachine.ll" "declare i64 @cvm_thread_spawn(i64)")
assert_file_contains("${cvm_stdlib_aot_out}/ConcordVisualMachine.ll" "declare i64 @cvm_counter_add(i64, i64)")

# The engine binding. These symbols are deliberately absent from the compiler:
# they are resolved by name from the engine runtime DLL at run time, which is
# what lets cc.exe stay free of any dependency on the engine. The fixture only
# runs when a module is named, because this project does not build the engine.
set(cvm_engine_out "${TEST_ROOT}/cvm_engine")
file(REMOVE_RECURSE "${cvm_engine_out}")
if(CVM_HOST_DLL AND EXISTS "${CVM_HOST_DLL}")
    # Both engine modules are given when both are known: Runtime exports the C
    # ABI and Render self-registers a backend, and neither is linked in.
    set(cvm_host_args --cvm-host "${CVM_HOST_DLL}")
    if(CVM_RENDER_DLL AND EXISTS "${CVM_RENDER_DLL}")
        list(APPEND cvm_host_args --cvm-host "${CVM_RENDER_DLL}")
    endif()
    execute_process(
        COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_engine" --out "${cvm_engine_out}"
                ${cvm_host_args}
        RESULT_VARIABLE cvm_engine_result
        OUTPUT_VARIABLE cvm_engine_stdout
        ERROR_VARIABLE cvm_engine_stderr)
    if(NOT cvm_engine_result EQUAL 0)
        message(FATAL_ERROR "CVM engine binding failed (${cvm_engine_result}): ${cvm_engine_stdout}\n${cvm_engine_stderr}")
    endif()
    # Version; an empty scene; four spawns; the camera probe; two distinct entity
    # handles; an absolute position round-tripping through millimetres; a
    # relative move; an entity removed; and then three reads that must all fail
    # after the entity and the scene it came from are gone.
    string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_engine_values "${cvm_engine_stdout}")
    # Version; five queries that must all answer 0 with no window open; an
    # empty scene; four spawns; two distinct handles; a position that now
    # round-trips as a real number; a relative move; and then three reads that
    # must all fail once the entity and its scene are gone.
    set(cvm_engine_expected "cvm: 12;cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 0")
    list(APPEND cvm_engine_expected "cvm: 4;cvm: 1;cvm: 0;cvm: 0")
    list(APPEND cvm_engine_expected "cvm: 1.5;cvm: 2;cvm: -3;cvm: 2;cvm: -2.5")
    list(APPEND cvm_engine_expected "cvm: 0.8;cvm: 1;cvm: 1.2;cvm: 1;cvm: 1;cvm: 0;cvm: 0;cvm: 1")
    list(APPEND cvm_engine_expected "cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 1")
    list(APPEND cvm_engine_expected "cvm: 4;cvm: 1;cvm: 1;cvm: 4;cvm: 2;cvm: 1;cvm: 1")
    list(APPEND cvm_engine_expected "cvm: 224;cvm: 60;cvm: 4;cvm: 1;cvm: 1;cvm: 7.5")
    list(APPEND cvm_engine_expected "cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 0;cvm: 0")
    list(APPEND cvm_engine_expected "cvm: 1;cvm: 3;cvm: 0;cvm: 1;cvm: 0;cvm: 0")
    # Joined before comparing: STREQUAL against a bare variable holding a
    # semicolon list is not a string comparison, because if() sees the
    # separators as argument boundaries.
    list(JOIN cvm_engine_values "|" cvm_engine_actual)
    list(JOIN cvm_engine_expected "|" cvm_engine_wanted)
    if(NOT cvm_engine_actual STREQUAL cvm_engine_wanted)
        message(FATAL_ERROR "CVM engine binding printed unexpected values: ${cvm_engine_actual}\nexpected: ${cvm_engine_wanted}")
    endif()

    # Models, particles, water, animation, textures, lights, ripples and mouse
    # queries. cube.obj lives beside Main.cx, so the load path is relative.
    set(cvm_assets_out "${TEST_ROOT}/cvm_assets")
    file(REMOVE_RECURSE "${cvm_assets_out}")
    execute_process(
        COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_assets" --out "${cvm_assets_out}"
                ${cvm_host_args}
        WORKING_DIRECTORY "${FIXTURE_ROOT}/cvm_assets"
        RESULT_VARIABLE cvm_assets_result
        OUTPUT_VARIABLE cvm_assets_stdout
        ERROR_VARIABLE cvm_assets_stderr)
    if(NOT cvm_assets_result EQUAL 0)
        message(FATAL_ERROR "CVM assets fixture failed (${cvm_assets_result}): ${cvm_assets_stdout}\n${cvm_assets_stderr}")
    endif()
    string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_assets_values "${cvm_assets_stdout}")
    list(JOIN cvm_assets_values "|" cvm_assets_actual)
    set(cvm_assets_wanted "cvm: 0|cvm: 1|cvm: 1|cvm: 1|cvm: 2")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 0|cvm: 0|cvm: 1")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 1|cvm: 1|cvm: 1|cvm: 1")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 1|cvm: 0|cvm: 0|cvm: -1|cvm: 0|cvm: 0|cvm: 1|cvm: 0")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 0|cvm: 0|cvm: 0|cvm: 1|cvm: 3|cvm: 1|cvm: 1|cvm: 0")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 0|cvm: 0|cvm: 0|cvm: 0")
    set(cvm_assets_wanted "${cvm_assets_wanted}|cvm: 6|cvm: 1|cvm: 1")
    if(NOT cvm_assets_actual STREQUAL cvm_assets_wanted)
        message(FATAL_ERROR "CVM assets printed unexpected values: ${cvm_assets_actual}\nexpected: ${cvm_assets_wanted}\n${cvm_assets_stdout}")
    endif()

    # Jolt through the C ABI, no window: a crate rests, a static volume stays,
    # a ray and an overlap see a floor, a throw leaves the ground.
    set(cvm_physics_out "${TEST_ROOT}/cvm_physics")
    file(REMOVE_RECURSE "${cvm_physics_out}")
    execute_process(
        COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_physics" --out "${cvm_physics_out}"
                ${cvm_host_args}
        RESULT_VARIABLE cvm_physics_result
        OUTPUT_VARIABLE cvm_physics_stdout
        ERROR_VARIABLE cvm_physics_stderr)
    if(NOT cvm_physics_result EQUAL 0)
        message(FATAL_ERROR "CVM physics fixture failed (${cvm_physics_result}): ${cvm_physics_stdout}\n${cvm_physics_stderr}")
    endif()
    string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_physics_values "${cvm_physics_stdout}")
    list(JOIN cvm_physics_values "|" cvm_physics_actual)
    if(NOT cvm_physics_actual STREQUAL "cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1")
        message(FATAL_ERROR "CVM physics printed unexpected values: ${cvm_physics_actual}\n${cvm_physics_stdout}")
    endif()
else()
    message(STATUS "CVM engine binding fixture skipped: set CONCORD_CVM_HOST_DLL to the engine runtime DLL")
endif()

# Calling the engine with no module named must explain the fix rather than fail
# somewhere inside the JIT linker.
file(REMOVE_RECURSE "${cvm_engine_out}_nohost")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_engine" --out "${cvm_engine_out}_nohost"
    RESULT_VARIABLE cvm_engine_nohost_result
    OUTPUT_VARIABLE cvm_engine_nohost_stdout
    ERROR_VARIABLE cvm_engine_nohost_stderr)
if(cvm_engine_nohost_result EQUAL 0)
    message(FATAL_ERROR "CVM engine binding without --cvm-host unexpectedly succeeded")
endif()
string(FIND "${cvm_engine_nohost_stderr}" "pass --cvm-host" cvm_engine_nohost_position)
if(cvm_engine_nohost_position EQUAL -1)
    message(FATAL_ERROR "CVM engine binding without --cvm-host did not explain the fix: ${cvm_engine_nohost_stderr}")
endif()


# Scene coverage: the primitives, the surface edits and the environment
# setters. The two trailing zeroes are the point of the last two lines: an
# edit to a component the entity does not carry must report failure, not
# quietly do nothing.
if(CVM_HOST_DLL AND EXISTS "${CVM_HOST_DLL}")
    set(cvm_scene_out "${TEST_ROOT}/cvm_scene")
    file(REMOVE_RECURSE "${cvm_scene_out}")
    execute_process(
        COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_scene" --out "${cvm_scene_out}"
                ${cvm_host_args}
        RESULT_VARIABLE cvm_scene_result
        OUTPUT_VARIABLE cvm_scene_stdout
        ERROR_VARIABLE cvm_scene_stderr)
    if(NOT cvm_scene_result EQUAL 0)
        message(FATAL_ERROR "CVM scene coverage failed (${cvm_scene_result}): ${cvm_scene_stdout}\n${cvm_scene_stderr}")
    endif()
    string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_scene_values "${cvm_scene_stdout}")
    list(JOIN cvm_scene_values "|" cvm_scene_actual)
    if(NOT cvm_scene_actual STREQUAL "cvm: 4|cvm: 1|cvm: 5|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 1|cvm: 0|cvm: 0|cvm: 1|cvm: 5|cvm: 1")
        message(FATAL_ERROR "CVM scene coverage printed unexpected values: ${cvm_scene_actual}\n${cvm_scene_stdout}")
    endif()
endif()

# Strings. Compared as whole lines rather than as extracted numbers, because
# half of what this fixture checks is the text it prints.
set(cvm_strings_out "${TEST_ROOT}/cvm_strings")
file(REMOVE_RECURSE "${cvm_strings_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_strings" --out "${cvm_strings_out}"
    RESULT_VARIABLE cvm_strings_result
    OUTPUT_VARIABLE cvm_strings_stdout
    ERROR_VARIABLE cvm_strings_stderr)
if(NOT cvm_strings_result EQUAL 0)
    message(FATAL_ERROR "CVM strings fixture failed (${cvm_strings_result}): ${cvm_strings_stdout}\n${cvm_strings_stderr}")
endif()
set(cvm_strings_expected "cvm: literal\ncvm: hello\ncvm: wow!\ncvm: 5\ncvm: 1\ncvm: 1\ncvm: 1\ncvm: 2\ncvm: ell\ncvm: 42\ncvm: 1.5\ncvm: 123\ncvm: 2.5\ncvm: 104\ncvm: ab\ncvm: 1\ncvm: 0\ncvm: 1\ncvm jit result: 0")
string(REPLACE "\r\n" "\n" cvm_strings_actual "${cvm_strings_stdout}")
string(STRIP "${cvm_strings_actual}" cvm_strings_actual)
if(NOT cvm_strings_actual STREQUAL cvm_strings_expected)
    message(FATAL_ERROR "CVM strings printed unexpected output:\n${cvm_strings_actual}\nexpected:\n${cvm_strings_expected}")
endif()

# Threads that share data. CVM has no closures and cannot share its globals
# across threads, so a handle is the only thing that reaches anything shared;
# 2000 is only reachable if both threads ran against the same counter.
set(cvm_threads_out "${TEST_ROOT}/cvm_threads")
file(REMOVE_RECURSE "${cvm_threads_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_threads" --out "${cvm_threads_out}"
    RESULT_VARIABLE cvm_threads_result
    OUTPUT_VARIABLE cvm_threads_stdout
    ERROR_VARIABLE cvm_threads_stderr)
if(NOT cvm_threads_result EQUAL 0)
    message(FATAL_ERROR "CVM threads fixture failed (${cvm_threads_result}): ${cvm_threads_stdout}\n${cvm_threads_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_threads_values "${cvm_threads_stdout}")
list(JOIN cvm_threads_values "|" cvm_threads_actual)
if(NOT cvm_threads_actual STREQUAL "cvm: 2000|cvm: 21")
    message(FATAL_ERROR "CVM threads printed unexpected values: ${cvm_threads_actual}\n${cvm_threads_stdout}")
endif()

# The f64 view of the containers.
set(cvm_f64list_out "${TEST_ROOT}/cvm_f64list")
file(REMOVE_RECURSE "${cvm_f64list_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_f64list" --out "${cvm_f64list_out}"
    RESULT_VARIABLE cvm_f64list_result
    OUTPUT_VARIABLE cvm_f64list_stdout
    ERROR_VARIABLE cvm_f64list_stderr)
if(NOT cvm_f64list_result EQUAL 0)
    message(FATAL_ERROR "CVM f64 container fixture failed (${cvm_f64list_result}): ${cvm_f64list_stdout}\n${cvm_f64list_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_f64list_values "${cvm_f64list_stdout}")
list(JOIN cvm_f64list_values "|" cvm_f64list_actual)
if(NOT cvm_f64list_actual STREQUAL "cvm: 3|cvm: 2.5|cvm: 2|cvm: 0|cvm: 3|cvm: 2|cvm: 4|cvm: 8|cvm: 3.5")
    message(FATAL_ERROR "CVM f64 containers printed unexpected values: ${cvm_f64list_actual}\n${cvm_f64list_stdout}")
endif()

# Module globals and parameters, driven through the exact shape a frame callback
# has. Calling the callback directly keeps this deterministic and GPU-free: the
# clamped running total is 30, 60, 90, 100 and the unclamped sum is 120.
set(cvm_state_out "${TEST_ROOT}/cvm_state")
file(REMOVE_RECURSE "${cvm_state_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_state" --out "${cvm_state_out}"
    RESULT_VARIABLE cvm_state_result
    OUTPUT_VARIABLE cvm_state_stdout
    ERROR_VARIABLE cvm_state_stderr)
if(NOT cvm_state_result EQUAL 0)
    message(FATAL_ERROR "CVM state fixture failed (${cvm_state_result}): ${cvm_state_stdout}\n${cvm_state_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_state_values "${cvm_state_stdout}")
if(NOT cvm_state_values STREQUAL "cvm: 30;cvm: 60;cvm: 90;cvm: 100;cvm: 4;cvm: 120")
    message(FATAL_ERROR "CVM state printed unexpected values: ${cvm_state_values}\n${cvm_state_stdout}")
endif()
string(FIND "${cvm_state_stdout}" "cvm jit result: 4" cvm_state_result_position)
if(cvm_state_result_position EQUAL -1)
    message(FATAL_ERROR "CVM state did not return the tick count: ${cvm_state_stdout}")
endif()

set(cvm_maps_out "${TEST_ROOT}/cvm_maps")
file(REMOVE_RECURSE "${cvm_maps_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_maps" --out "${cvm_maps_out}"
    RESULT_VARIABLE cvm_maps_result
    OUTPUT_VARIABLE cvm_maps_stdout
    ERROR_VARIABLE cvm_maps_stderr)
if(NOT cvm_maps_result EQUAL 0)
    message(FATAL_ERROR "CVM map fixture failed (${cvm_maps_result}): ${cvm_maps_stdout}\n${cvm_maps_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_maps_values "${cvm_maps_stdout}")
list(JOIN cvm_maps_values "|" cvm_maps_actual)
if(NOT cvm_maps_actual STREQUAL "cvm: 0|cvm: 1|cvm: 1|cvm: 42|cvm: 0|cvm: 0|cvm: 1|cvm: 9|cvm: 1|cvm: 1|cvm: 0|cvm: 0|cvm: 1|cvm: -1|cvm: 1|cvm: 3|cvm: 0|cvm: 1|cvm: 2|cvm: 1|cvm: 0|cvm: 1")
    message(FATAL_ERROR "CVM maps printed unexpected values: ${cvm_maps_actual}\n${cvm_maps_stdout}")
endif()

set(cvm_text_out "${TEST_ROOT}/cvm_text")
file(REMOVE_RECURSE "${cvm_text_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_text" --out "${cvm_text_out}"
    RESULT_VARIABLE cvm_text_result
    OUTPUT_VARIABLE cvm_text_stdout
    ERROR_VARIABLE cvm_text_stderr)
if(NOT cvm_text_result EQUAL 0)
    message(FATAL_ERROR "CVM text fixture failed (${cvm_text_result}): ${cvm_text_stdout}\n${cvm_text_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_text_values "${cvm_text_stdout}")
list(JOIN cvm_text_values "|" cvm_text_actual)
if(NOT cvm_text_actual STREQUAL "cvm: 6|cvm: 5|cvm: 233|cvm: 3|cvm: 2|cvm: 0|cvm: 2|cvm: 0.2|cvm: 0.2|cvm: 4|cvm: 4|cvm: 5|cvm: 9")
    message(FATAL_ERROR "CVM text printed unexpected values: ${cvm_text_actual}\n${cvm_text_stdout}")
endif()
string(FIND "${cvm_text_stdout}" "ABC" cvm_text_upper)
if(cvm_text_upper EQUAL -1)
    message(FATAL_ERROR "CVM text did not print the uppercased literal: ${cvm_text_stdout}")
endif()
string(FIND "${cvm_text_stdout}" "alpha, beta, gamma" cvm_text_join)
if(cvm_text_join EQUAL -1)
    message(FATAL_ERROR "CVM text did not join the string list: ${cvm_text_stdout}")
endif()

set(cvm_records_out "${TEST_ROOT}/cvm_records")
file(REMOVE_RECURSE "${cvm_records_out}")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_records" --out "${cvm_records_out}"
    RESULT_VARIABLE cvm_records_result
    OUTPUT_VARIABLE cvm_records_stdout
    ERROR_VARIABLE cvm_records_stderr)
if(NOT cvm_records_result EQUAL 0)
    message(FATAL_ERROR "CVM records fixture failed (${cvm_records_result}): ${cvm_records_stdout}\n${cvm_records_stderr}")
endif()
string(REGEX MATCHALL "cvm: [0-9.eE+-]+" cvm_records_values "${cvm_records_stdout}")
list(JOIN cvm_records_values "|" cvm_records_actual)
if(NOT cvm_records_actual STREQUAL "cvm: 3|cvm: 4|cvm: 5|cvm: 14|cvm: 4|cvm: 29|cvm: 4|cvm: 7|cvm: 3|cvm: 0|cvm: 7")
    message(FATAL_ERROR "CVM records printed unexpected values: ${cvm_records_actual}\n${cvm_records_stdout}")
endif()

# The full game program under examples/ compiles to an AOT object, which is all
# a GPU-free machine can check. Its IR must carry the frame callback with its
# parameter and the globals the callback keeps its state in.
set(cvm_game_out "${TEST_ROOT}/cvm_game")
file(REMOVE_RECURSE "${cvm_game_out}")
execute_process(
    COMMAND "${CC}" --project "${EXAMPLE_ROOT}/cvm_game"
            --out "${cvm_game_out}" --cvm aot
    RESULT_VARIABLE cvm_game_result
    OUTPUT_VARIABLE cvm_game_stdout
    ERROR_VARIABLE cvm_game_stderr)
if(NOT cvm_game_result EQUAL 0)
    message(FATAL_ERROR "CVM game example failed to compile (${cvm_game_result}): ${cvm_game_stdout}\n${cvm_game_stderr}")
endif()
if(NOT EXISTS "${cvm_game_out}/ConcordVisualMachine.obj")
    message(FATAL_ERROR "CVM game example did not produce an object file")
endif()
# The callback is i64(f64) because delta time is a fraction of a second, look-at
# and mouse capture are what a real camera needs, and clamp is the host maths
# function rather than a helper the example defines itself.
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "define i64 @update(double %deltaTime)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare double @cvm_clamp(double, double, double)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "@yaw = internal global double")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmSetUpdate(i64)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmEntitySetPosition(i64, double, double, double)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare double @ConcordCvmEntityPosition(i64, i64)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmEntityLookAt(i64, double, double, double)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmRaycast(i64, double, double, double, double, double, double, double)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmAddPhysicsSystem()")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmSceneSpawnDynamicBox(i64, double, double, double, double, double, double, i64, i64, i64, double)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmKeyDown(i64)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmSetMouseCaptured(i64)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmUiLabel(double, double, ptr)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "and.rhs")
# A box takes an extent per axis now, and the window title crosses as a pointer.
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmSceneSpawnBox(i64, double, double, double, double, double, double, i64, i64, i64)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmRunScene(i64, i64, i64, ptr)")
assert_file_contains("${cvm_game_out}/ConcordVisualMachine.ll" "declare i64 @ConcordCvmSceneSetFog(i64, double, double, double, double)")

# A module path that does not exist must fail before any artifact is written.
file(REMOVE_RECURSE "${cvm_engine_out}_badhost")
execute_process(
    COMMAND "${CC}" --project "${FIXTURE_ROOT}/cvm_engine" --out "${cvm_engine_out}_badhost"
            --cvm-host "${TEST_ROOT}/does-not-exist.dll"
    RESULT_VARIABLE cvm_engine_badhost_result
    OUTPUT_VARIABLE cvm_engine_badhost_stdout
    ERROR_VARIABLE cvm_engine_badhost_stderr)
if(cvm_engine_badhost_result EQUAL 0)
    message(FATAL_ERROR "CVM host module that does not exist unexpectedly loaded")
endif()
string(FIND "${cvm_engine_badhost_stderr}" "cannot load host module" cvm_engine_badhost_position)
if(cvm_engine_badhost_position EQUAL -1)
    message(FATAL_ERROR "Missing CVM host module did not report a load failure: ${cvm_engine_badhost_stderr}")
endif()

# Each entry is "fixture|expected diagnostic substring". Diagnostics are part of
# the contract here: CVM rejects what it cannot lower instead of emitting it.
# The separator is '|' rather than ';' because CMake splits a quoted list
# element on semicolons, which would tear every entry in half.
set(cvm_negative_cases
    "invalid_cvm_class|CVM accepts only @cvm, @cvm_global and struct declarations"
    "invalid_cvm_entry|CVM accepts only @cvm, @cvm_global and struct declarations"
    "invalid_cvm_noreturn|missing return"
    "invalid_cvm_redeclare|is already declared"
    "invalid_cvm_unknown_call|did you mean 'list_push'?"
    "invalid_cvm_arity|expects 2 argument(s), 1 given"
    "invalid_cvm_noeffect|expression statement has no effect"
    "invalid_cvm_assign_undeclared|unknown variable 'total'"
    "invalid_cvm_syntax|expected '('"
    "invalid_cvm_break|'break' is only valid inside a loop"
    "invalid_cvm_continue|'continue' is only valid inside a loop"
    "invalid_cvm_for|expected '('"
    "invalid_cvm_type_narrow|convert it with to_i64"
    "invalid_cvm_type_unknown|CVM has i64, f64, str"
    "invalid_cvm_return_narrow|the return value is i64"
    "invalid_cvm_global_narrow|initialized with a float literal"
    "invalid_cvm_str_arith|strings do not support arithmetic"
    "invalid_cvm_str_order|strings cannot be ordered"
    "invalid_cvm_str_escape|unknown escape"
    "invalid_cvm_str_convert|does not take a string"
    "invalid_cvm_str_arg|but is given a str"
    "invalid_cvm_callback_shape|declare it with that signature")
foreach(case_entry IN LISTS cvm_negative_cases)
    string(FIND "${case_entry}" "|" cvm_separator)
    string(SUBSTRING "${case_entry}" 0 ${cvm_separator} case_name)
    math(EXPR cvm_after "${cvm_separator} + 1")
    string(SUBSTRING "${case_entry}" ${cvm_after} -1 cvm_expected_error)

    set(cvm_negative_out "${TEST_ROOT}/${case_name}")
    file(REMOVE_RECURSE "${cvm_negative_out}")
    execute_process(
        COMMAND "${CC}" --project "${FIXTURE_ROOT}/${case_name}" --out "${cvm_negative_out}"
        RESULT_VARIABLE cvm_negative_result
        OUTPUT_VARIABLE cvm_negative_stdout
        ERROR_VARIABLE cvm_negative_stderr)
    if(cvm_negative_result EQUAL 0)
        message(FATAL_ERROR "Negative CVM fixture unexpectedly succeeded: ${case_name}")
    endif()
    string(FIND "${cvm_negative_stderr}" "${cvm_expected_error}" cvm_error_position)
    if(cvm_error_position EQUAL -1)
        message(FATAL_ERROR "${case_name} did not report '${cvm_expected_error}': ${cvm_negative_stderr}")
    endif()
    # A rejected program must leave nothing behind, not even a partial artifact.
    if(EXISTS "${cvm_negative_out}")
        file(GLOB_RECURSE cvm_negative_files LIST_DIRECTORIES false "${cvm_negative_out}/*")
        if(cvm_negative_files)
            message(FATAL_ERROR "Failed CVM run left output files: ${case_name}")
        endif()
    endif()
endforeach()

set(cli_conflict_out "${TEST_ROOT}/cli_conflict")
file(REMOVE_RECURSE "${cli_conflict_out}")
execute_process(
    COMMAND "${CC}" --cpp --cvm --project "${FIXTURE_ROOT}/cvm_demo" --out "${cli_conflict_out}"
    RESULT_VARIABLE cli_conflict_result
    OUTPUT_VARIABLE cli_conflict_stdout
    ERROR_VARIABLE cli_conflict_stderr)
if(cli_conflict_result EQUAL 0)
    message(FATAL_ERROR "Mutually exclusive modes unexpectedly succeeded")
endif()
string(FIND "${cli_conflict_stderr}" "mutually exclusive" conflict_position)
if(conflict_position EQUAL -1)
    message(FATAL_ERROR "Mode conflict did not report mutual exclusivity: ${cli_conflict_stderr}")
endif()

execute_process(
    COMMAND "${CC}" --help
    RESULT_VARIABLE cli_help_result
    OUTPUT_VARIABLE cli_help_stdout
    ERROR_VARIABLE cli_help_stderr)
if(NOT cli_help_result EQUAL 0)
    message(FATAL_ERROR "cc --help failed (${cli_help_result}): ${cli_help_stderr}")
endif()
string(FIND "${cli_help_stdout}" "USAGE" cli_help_usage)
string(FIND "${cli_help_stdout}" "--color" cli_help_color)
if(cli_help_usage EQUAL -1 OR cli_help_color EQUAL -1)
    message(FATAL_ERROR "cc --help did not describe usage: ${cli_help_stdout}")
endif()

execute_process(
    COMMAND "${CC}" --version
    RESULT_VARIABLE cli_version_result
    OUTPUT_VARIABLE cli_version_stdout
    ERROR_VARIABLE cli_version_stderr)
if(NOT cli_version_result EQUAL 0)
    message(FATAL_ERROR "cc --version failed (${cli_version_result}): ${cli_version_stderr}")
endif()
string(FIND "${cli_version_stdout}" "0.1.0" cli_version_position)
if(cli_version_position EQUAL -1)
    message(FATAL_ERROR "cc --version did not print the package version: ${cli_version_stdout}")
endif()

string(ASCII 27 ESC)
execute_process(
    COMMAND "${CC}" --color always
    RESULT_VARIABLE cli_color_always_result
    OUTPUT_VARIABLE cli_color_always_stdout
    ERROR_VARIABLE cli_color_always_stderr)
if(cli_color_always_result EQUAL 0)
    message(FATAL_ERROR "cc --color always with no project unexpectedly succeeded")
endif()
if(cli_color_always_esc EQUAL -1)
    message(FATAL_ERROR "cc --color always did not paint error: ${cli_color_always_stderr}")
endif()
string(FIND "${cli_color_always_stderr}" "error" cli_color_always_error)
string(FIND "${cli_color_always_stderr}" "missing --project" cli_color_always_missing)
if(cli_color_always_error EQUAL -1 OR cli_color_always_missing EQUAL -1)
    message(FATAL_ERROR "cc --color always did not report the missing-project error: ${cli_color_always_stderr}")
endif()

execute_process(
    COMMAND "${CC}" --color never
    RESULT_VARIABLE cli_color_never_result
    OUTPUT_VARIABLE cli_color_never_stdout
    ERROR_VARIABLE cli_color_never_stderr)
if(cli_color_never_result EQUAL 0)
    message(FATAL_ERROR "cc --color never with no project unexpectedly succeeded")
endif()
string(FIND "${cli_color_never_stderr}" "${ESC}" cli_color_never_esc)
string(FIND "${cli_color_never_stderr}" "error:" cli_color_never_error)
if(NOT cli_color_never_esc EQUAL -1)
    message(FATAL_ERROR "cc --color never still emitted ANSI: ${cli_color_never_stderr}")
endif()
if(cli_color_never_error EQUAL -1)
    message(FATAL_ERROR "cc --color never did not report error: ${cli_color_never_stderr}")
endif()

message(STATUS "ConcordScript annotation fixtures passed")
