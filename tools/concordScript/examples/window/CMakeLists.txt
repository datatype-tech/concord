cmake_minimum_required(VERSION 3.24)

project(ConcordScriptWindowExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(window_example generated/Main.gen.cpp generated/ConcordScriptRegistry.gen.cpp)
target_include_directories(window_example PRIVATE generated)
target_link_libraries(window_example PRIVATE concord::concord)

concord_stage_runtime(window_example)
