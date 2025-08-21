cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")

project(quantum_simulator)

include(FetchContent)
FetchContent_Declare(stim
        GIT_REPOSITORY https://github.com/quantumlib/stim.git
        GIT_TAG v1.15.0)
FetchContent_GetProperties(stim)
if(NOT stim_POPULATED)
  FetchContent_Populate(stim)
  add_subdirectory(${stim_SOURCE_DIR})
endif()

add_executable(test_stim some.cpp)
target_link_libraries(test_stim PRIVATE libstim)
