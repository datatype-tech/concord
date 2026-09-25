cmake_minimum_required(VERSION 3.24)

project(ConcordScriptMetadataConsumer LANGUAGES CXX)

if(NOT DEFINED GENERATED_DIR)
    message(FATAL_ERROR "GENERATED_DIR is required")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(metadata_consumer main.cpp)

include("${GENERATED_DIR}/ConcordScriptShaders.cmake")
concordscript_attach_metadata(metadata_consumer)
