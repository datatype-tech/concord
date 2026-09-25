cmake_minimum_required(VERSION 3.24)

# concordc / cc — the ConcordScript compiler CLIs.
#
# Uses GNU flex + bison (msys2 usr/bin on this machine) to build the lexer
# and parser from script/src/lexer.l and script/src/parser.y. Generated
# lexer/parser C sources are build artifacts, never committed (see
# .gitignore); flex/bison regenerate them on every configure.
#
# Two personalities share one compiler core:
#   concordc.exe — historical default: --cpp generates .gen.h/.gen.cpp
#   cc.exe       — Concord Visual Machine default: --cvm [jit|aot] lowers
#                  @cvm blocks straight to LLVM IR and runs (JIT) or emits
#                  an AOT object. --cpp still works from both binaries.

project(ConcordScript
    VERSION 0.1.0
    DESCRIPTION "ConcordScript compiler: C++ transpiler and Concord Visual Machine (LLVM)"
    LANGUAGES C CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# msys2's flex/bison live under usr/bin, which is not on PATH by default.
list(APPEND CMAKE_PROGRAM_PATH "C:/msys64/usr/bin")

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# LLVM 22 backs the Concord Visual Machine. Users may override with
# -DLLVM_DIR=<prefix>/lib/cmake/llvm; the MSYS2 UCRT64 location is the
# documented default for this machine.
if(NOT DEFINED LLVM_DIR)
    foreach(candidate "C:/msys64/ucrt64/lib/cmake/llvm")
        if(EXISTS "${candidate}/LLVMConfig.cmake")
            set(LLVM_DIR "${candidate}" CACHE PATH "LLVM CMake package directory")
            break()
        endif()
    endforeach()
endif()

# LLVM's static imported targets reference ZLIB::ZLIB (and friends), and
# LLVMConfig.cmake re-runs find_package(ZLIB) internally. A configure from
# Git Bash without extra flags does not search the MSYS2 prefix, so add it
# here; an explicit -DCMAKE_PREFIX_PATH from the caller still wins.
set(CONCORD_MSYS2_PREFIX "C:/msys64/ucrt64")
if(EXISTS "${CONCORD_MSYS2_PREFIX}/lib/cmake/llvm/LLVMConfig.cmake" AND
   NOT "${CONCORD_MSYS2_PREFIX}" IN_LIST CMAKE_PREFIX_PATH)
    list(APPEND CMAKE_PREFIX_PATH "${CONCORD_MSYS2_PREFIX}")
endif()

find_package(LLVM CONFIG REQUIRED)

message(STATUS "Concord Visual Machine: LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

BISON_TARGET(ConcordScriptParser
    src/parser.y
    "${GENERATED_DIR}/parser.tab.cpp"
    DEFINES_FILE "${GENERATED_DIR}/parser.tab.h"
)

FLEX_TARGET(ConcordScriptLexer
    src/lexer.l
    "${GENERATED_DIR}/lexer.yy.cpp"
)

ADD_FLEX_BISON_DEPENDENCY(ConcordScriptLexer ConcordScriptParser)

# The CVM standard library, in plain C.
#
# A @cvm block lowers to LLVM IR, which can only call a C ABI, so the library
# every registered host function lives in is C rather than C++. The same static
# library serves both paths: the JIT binds its symbols into the process, and an
# AOT object references them by name and links against the .a.
add_library(concord_cvm_runtime STATIC
    runtime/CvmHandles.c
    runtime/CvmCollections.c
    runtime/CvmCollections32.c
    runtime/CvmCollectionsF64.c
    runtime/CvmCollectionsText.c
    runtime/CvmConcurrency.c
    runtime/CvmMaps.c
    runtime/CvmMath.c
    runtime/CvmString.c
    runtime/CvmSystem.c
    runtime/CvmFile.c
)
target_include_directories(concord_cvm_runtime PUBLIC runtime)
set_target_properties(concord_cvm_runtime PROPERTIES OUTPUT_NAME "ConcordCvmRuntime")

set(CONCORDSCRIPT_COMPILER_SOURCES
    src/main.cpp
    src/AnnotationCodeGen.cpp
    src/AnnotationDirectives.cpp
    src/AnnotationPass.cpp
    src/AnnotationPassArguments.cpp
    src/AnnotationPassSupport.cpp
    src/AnnotationShader.cpp
    src/AnnotationShaderOptions.cpp
    src/AnnotationShaderSupport.cpp
    src/AnnotationShaderStages.cpp
    src/AnnotationText.cpp
    src/AnnotationTextArguments.cpp
    src/AnnotationTextLiterals.cpp
    src/AnnotationTextSplit.cpp
    src/AnnotationTextTemplates.cpp
    src/AnnotationValues.cpp
    src/AnnotationValidation.cpp
    src/AnnotationValidationIncludes.cpp
    src/AnnotationValidationPass.cpp
    src/AnnotationValidationShader.cpp
    src/CvmBackend.cpp
    src/CvmEmitter.cpp
    src/CvmHostModule.cpp
    src/CvmHostRegistry.cpp
    src/CvmProgram.cpp
    src/CvmStrings.cpp
    src/CliReport.cpp
    src/CliOptions.cpp
    src/SourceDiscovery.cpp
    src/ProjectTemplate.cpp
    src/CliStyle.cpp
    src/CodeGen.cpp
    src/CodeGenManifest.cpp
    src/KeywordSubstitution.cpp
    src/ModuleName.cpp
    src/MetadataCodeGen.cpp
    src/MetadataCmake.cpp
    src/Registry.cpp
    src/Render.cpp
    src/StdlibRegistry.cpp
    ${FLEX_ConcordScriptLexer_OUTPUTS}
    ${BISON_ConcordScriptParser_OUTPUTS}
)

# LLVM_DEFINITIONS is a single "-D..." string; split it before use.
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")

# Link the static LLVM component libraries. The shared libLLVM-22.dll on
# MSYS2 crashes when host-allocated Module/LLVMContext objects are destroyed
# across the DLL boundary, so the Concord Visual Machine links LLVM
# statically instead; llvm_expand_dependencies orders the component graph.
llvm_map_components_to_libnames(CONCORDSCRIPT_LLVM_COMPONENTS
    core support bitwriter executionengine orcjit jitlink native nativecodegen)
llvm_expand_dependencies(CONCORDSCRIPT_LLVM_LIBS ${CONCORDSCRIPT_LLVM_COMPONENTS})

add_library(concordscript_compiler STATIC ${CONCORDSCRIPT_COMPILER_SOURCES})
add_executable(concordc src/concordc_main.cpp)
add_executable(cc src/cc_main.cpp)
set(CONCORDSCRIPT_RELEASE_VERSION "0.1.0-preview.1" CACHE STRING "Compiler release identifier")

foreach(tool concordscript_compiler)
    target_include_directories(${tool} PUBLIC src PRIVATE "${GENERATED_DIR}" ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(${tool} PRIVATE
        ${LLVM_DEFINITIONS_LIST}
        CONCORDSCRIPT_VERSION="${CONCORDSCRIPT_RELEASE_VERSION}")
    target_link_libraries(${tool} PRIVATE ${CONCORDSCRIPT_LLVM_LIBS} concord_cvm_runtime)
endforeach()

# Static LLVM still needs its system libraries; they arrive through the
# imported targets' INTERFACE_LINK_LIBRARIES, but MinGW also wants the usual
# Windows socket/OLE imports when LLVM is linked fully statically.
foreach(tool concordc cc)
    target_link_libraries(${tool} PRIVATE concordscript_compiler version ole32 uuid)
endforeach()

# Stage the MinGW runtime DLLs next to the tools, so the statically linked
# executables still run straight from the build tree.
set(MSYS2_BIN_DIR "C:/msys64/ucrt64/bin" CACHE PATH
    "Directory holding the MinGW runtime DLLs")
set(CONCORDSCRIPT_RUNTIME_DLLS
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    zlib1.dll
    libzstd.dll
    libxml2-16.dll
    libffi-8.dll
)
foreach(tool concordc cc)
    foreach(dll IN LISTS CONCORDSCRIPT_RUNTIME_DLLS)
        if(EXISTS "${MSYS2_BIN_DIR}/${dll}")
            add_custom_command(TARGET ${tool} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MSYS2_BIN_DIR}/${dll}" "$<TARGET_FILE_DIR:${tool}>"
                COMMENT "Stage ${dll} beside ${tool}"
                VERBATIM)
        endif()
    endforeach()
endforeach()

# Optional: the engine runtime DLL used by the CVM engine-binding fixture.
#
# The script project does not build the engine, so the fixture is skipped unless
# a module is named here. Point it at a host build, for example
#   -DCONCORD_CVM_HOST_DLL=<build>/ConcordFlashGameEngineRuntime.dll
set(CONCORD_CVM_HOST_DLL "" CACHE FILEPATH
    "Concord runtime DLL that supplies the CVM engine binding (optional)")

# The second engine module. Only a script that opens a window needs it, because
# this is the DLL whose load registers a render backend.
set(CONCORD_CVM_RENDER_DLL "" CACHE FILEPATH
    "Concord render DLL loaded alongside the CVM host module (optional)")

if(BUILD_TESTING)
    add_test(NAME concordscript_cli_tests
        COMMAND ${CMAKE_COMMAND} -DCONCORDC=$<TARGET_FILE:concordc>
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunCliTests.cmake)
    add_test(NAME concordscript_annotation_fixtures
        COMMAND ${CMAKE_COMMAND}
            -DCONCORDC=$<TARGET_FILE:concordc>
            -DCC=$<TARGET_FILE:cc>
            -DFIXTURE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
            -DEXAMPLE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/examples
            -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests
            -DCVM_HOST_DLL=${CONCORD_CVM_HOST_DLL}
            -DCVM_RENDER_DLL=${CONCORD_CVM_RENDER_DLL}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/RunAnnotationTests.cmake)
endif()
