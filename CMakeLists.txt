cmake_minimum_required(VERSION 3.14)

## Only set cross-compilers if a compiler has not been provided by the user
# This allows CI jobs (or local users) to override compilers by passing
# -DCMAKE_C_COMPILER / -DCMAKE_CXX_COMPILER on the cmake command line.
if (NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "/usr/bin/aarch64-linux-gnu-gcc")
endif()

if (NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "/usr/bin/aarch64-linux-gnu-g++")
endif()
set(CMAKE_CXX_STANDARD 20)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Debug build flags
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")

# Release build flags
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# RelWithDebInfo
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
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

# Get yaml-cpp library for configuration
FetchContent_Declare(yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0)
FetchContent_GetProperties(yaml-cpp)
if(NOT yaml-cpp_POPULATED)
  FetchContent_Populate(yaml-cpp)
  set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "disable yaml tests")
  set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "disable yaml tools")
  add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
endif()


# ---- Options ----
option(ENABLE_PROFILING "Enable fast TLS profiler" ON)
option(ENABLE_LOGGING "Enable logging functionality" OFF)  # Default OFF

if(ENABLE_PROFILING)
  add_library(profiler STATIC src/logger/profiler.cpp)
  target_compile_definitions(profiler PUBLIC PROF_ENABLED=1)
  target_include_directories(profiler PUBLIC ${PROJECT_SOURCE_DIR}/src)
else()
  # Empty interface library when disabled
  add_library(profiler INTERFACE)
  target_compile_definitions(profiler INTERFACE PROF_ENABLED=0)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(ENABLE_LOGGING ON CACHE BOOL "Enable logging functionality" FORCE)
endif()

if(ENABLE_LOGGING)
  add_library(logger STATIC src/logger/logger.cpp)
  target_compile_definitions(logger PUBLIC LOGGING_ENABLED=1)
  target_include_directories(logger PUBLIC ${PROJECT_SOURCE_DIR}/src)
else()
  # Empty interface library when disabled
  add_library(logger INTERFACE)
  target_compile_definitions(logger INTERFACE LOGGING_ENABLED=0)
endif()

add_library(qilab_lib STATIC
  #Data Transmission
  src/DataTransmission/handlers/mock_handler.cpp
  src/DataTransmission/TransmissionRunner/transmission_runner.cpp
  src/DataTransmission/TransmissionProcessor/transmission_processor.cpp
  #Simulator
  src/SimulatorRunners/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  src/Simulator/adapters/stim_adapter.cpp
  #Message Broker
  src/MessageBroker/message_broker.cpp
  #Entanglement Manager
  src/Simulator/entanglement/entanglement_manager.cpp
  src/SimulatorRunners/entanglement_runner.cpp
  #Utils
  src/utils/config_loader.cpp
)

target_include_directories(qilab_lib PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(qilab_lib PRIVATE libstim profiler yaml-cpp::yaml-cpp)

include_directories("src")

add_executable(qilab
  application.cpp
  #Data Transmission
  src/DataTransmission/handlers/mock_handler.cpp
  src/DataTransmission/handlers/spi_master_handler.cpp
  src/DataTransmission/TransmissionRunner/transmission_runner.cpp
  src/DataTransmission/TransmissionProcessor/transmission_processor.cpp
  #Simulator
  src/SimulatorRunners/simulator_runner.cpp
  src/Simulator/adapters/mock_adapter.cpp
  src/Simulator/adapters/stim_adapter.cpp
  #Message Broker
  src/MessageBroker/message_broker.cpp
  #Entanglement Manager
  src/Simulator/entanglement/entanglement_manager.cpp
  src/SimulatorRunners/entanglement_runner.cpp
  #Utils
  src/utils/config_loader.cpp
    )
target_link_libraries(qilab PRIVATE libstim profiler logger yaml-cpp::yaml-cpp)

include(CTest)                 # defines BUILD_TESTING (default ON)

# Tests live in a separate CMake; add only when requested
if(BUILD_TESTING)
  add_subdirectory(test)
endif()