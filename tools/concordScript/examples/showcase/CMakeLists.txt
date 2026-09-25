cmake_minimum_required(VERSION 3.24)

project(ConcordScriptShowcaseExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(showcase_example
    generated/Main.gen.cpp
    generated/Showcase.gen.cpp
    generated/ConcordScriptRegistry.gen.cpp
)
target_include_directories(showcase_example PRIVATE generated)
target_link_libraries(showcase_example PRIVATE concord::concord)

include("${CMAKE_CURRENT_SOURCE_DIR}/generated/ConcordScriptShaders.cmake" OPTIONAL)
if(COMMAND concordscript_attach_shaders)
    concordscript_attach_shaders(showcase_example)
    if(COMMAND concordscript_attach_vulkan)
        concordscript_attach_vulkan(showcase_example)
    endif()
endif()

concord_stage_runtime(showcase_example)
