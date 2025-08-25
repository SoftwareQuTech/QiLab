cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-Og")

project(QILab)

#get the stim library
include(FetchContent)
FetchContent_Declare(stim
        GIT_REPOSITORY https://github.com/quantumlib/stim.git
        GIT_TAG v1.15.0)
FetchContent_GetProperties(stim)
if(NOT stim_POPULATED)
  FetchContent_Populate(stim)
  add_subdirectory(${stim_SOURCE_DIR})
endif()
include_directories("src")

add_executable(stim_evaluation
  application.cpp
  src/CommandHandler/command_handler.cpp
  src/CommandHandler/handlers/mock_handler.cpp
  src/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  )
target_link_libraries(stim_evaluation PRIVATE libstim)
