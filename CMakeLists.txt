cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-O0")
project(QILab)



# ---- Tests ----
include(FetchContent)
  FetchContent_Declare(
    googletest
    # Specify the commit you depend on and update it regularly.
    URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(stim_eval_test 
                test/sample_test.cpp)
target_link_libraries(stim_eval_test PRIVATE gtest_main)
include(GoogleTest)
gtest_discover_tests(stim_eval_test)


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
  src/SimulatorRunner/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  src/Simulator/adapters/stim_adapter.cpp
  src/CommandNode/command_node.cpp
  src/logger/logger.cpp
  )
target_link_libraries(stim_evaluation PRIVATE libstim)


