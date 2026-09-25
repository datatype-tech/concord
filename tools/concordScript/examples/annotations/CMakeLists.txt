cmake_minimum_required(VERSION 3.24)

project(ConcordScriptAnnotationExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(annotation_example
    generated/Main.gen.cpp
    generated/ConcordScriptRegistry.gen.cpp
)
target_include_directories(annotation_example PRIVATE generated)
target_link_libraries(annotation_example PRIVATE concord::concord)

include("${CMAKE_CURRENT_SOURCE_DIR}/generated/ConcordScriptShaders.cmake" OPTIONAL)
if(COMMAND concordscript_attach_shaders)
    concordscript_attach_shaders(annotation_example)
endif()
if(COMMAND concordscript_attach_vulkan)
    concordscript_attach_vulkan(annotation_example)
endif()

concord_stage_runtime(annotation_example)
