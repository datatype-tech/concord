cmake_minimum_required(VERSION 3.24)

project(ConcordScriptGameDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(game_demo
    generated/Main.gen.cpp
    generated/GameTypes.gen.cpp
    generated/SpaceDefense.gen.cpp
    generated/ConcordScriptRegistry.gen.cpp
)
target_include_directories(game_demo PRIVATE generated)
target_link_libraries(game_demo PRIVATE concord::concord)

include("${CMAKE_CURRENT_SOURCE_DIR}/generated/ConcordScriptShaders.cmake" OPTIONAL)
if(COMMAND concordscript_attach_shaders)
    concordscript_attach_shaders(game_demo)
    if(COMMAND concordscript_attach_vulkan)
        concordscript_attach_vulkan(game_demo)
    endif()
endif()

concord_stage_runtime(game_demo)
