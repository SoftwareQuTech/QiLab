cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-O0")
project(QILab)

# ---- Project compilation ----
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


option(ENABLE_PROFILING "Enable fast TLS profiler" ON)

if(ENABLE_PROFILING)
  add_compile_definitions(PROF_ENABLED=1)
  add_library(profiler STATIC src/logger/profiler.cpp)
else()
  add_compile_definitions(PROF_ENABLED=0)
  # Do NOT compile profiler.cpp when disabled
endif()

add_library(qilab_lib STATIC
  src/CommandHandler/command_handler.cpp
  src/CommandHandler/handlers/mock_handler.cpp
  src/SimulatorRunner/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  src/Simulator/adapters/stim_adapter.cpp
  src/CommandNode/command_node.cpp
  src/SimulatorRunner/entanglement_manager.cpp
  src/logger/logger.cpp
  src/logger/profiler.cpp
)
target_include_directories(qilab_lib PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(qilab_lib PRIVATE libstim)

include_directories("src")

add_executable(qilab
  application.cpp
  src/CommandHandler/command_handler.cpp
  src/CommandHandler/handlers/mock_handler.cpp
  src/SimulatorRunner/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  src/Simulator/adapters/stim_adapter.cpp
  src/CommandNode/command_node.cpp
  src/SimulatorRunner/entanglement_manager.cpp
  src/logger/logger.cpp
  src/logger/profiler.cpp
  )
target_link_libraries(qilab PRIVATE libstim)

include(CTest)                 # defines BUILD_TESTING (default ON)

# Tests live in a separate CMake; add only when requested
if(BUILD_TESTING)
  add_subdirectory(test)
endif()