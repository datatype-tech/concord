cmake_minimum_required(VERSION 3.24)

# Verifies concordc's output actually compiles and links against the
# Concord engine — the "hello" example's generated C++, built exactly like
# any other consumer of concord::concord would build it.

project(ConcordScriptHelloExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../concord" concord_build)

add_executable(hello_example
    generated/Main.gen.cpp
    generated/Player.gen.cpp
    generated/ConcordScriptRegistry.gen.cpp
)
target_include_directories(hello_example PRIVATE generated)
target_link_libraries(hello_example PRIVATE concord::concord)

concord_stage_runtime(hello_example)
