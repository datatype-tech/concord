cmake_minimum_required(VERSION 3.24)

project(ConcordScriptRayTracingExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(raytracing_example
    generated/Main.gen.cpp
    generated/ConcordScriptRegistry.gen.cpp
)
target_include_directories(raytracing_example PRIVATE generated)
target_link_libraries(raytracing_example PRIVATE concord::concord)

include("${CMAKE_CURRENT_SOURCE_DIR}/generated/ConcordScriptShaders.cmake" OPTIONAL)
if(COMMAND concordscript_attach_shaders)
    concordscript_attach_shaders(raytracing_example)
endif()
if(COMMAND concordscript_attach_metadata)
    concordscript_attach_metadata(raytracing_example)
endif()

concord_stage_runtime(raytracing_example)
