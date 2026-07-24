# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

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

add_executable(transmission_processor_test
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/TransmissionProcessor/test_transmission_processor_sync.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/TransmissionProcessor/test_transmission_processor_simple.cpp                
                #${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_transmission.hpp
                )
target_compile_definitions(transmission_processor_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(transmission_processor_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(transmission_processor_test PRIVATE 
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME transmission_processor_test COMMAND transmission_processor_test)

add_executable(entanglement_manager_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Entanglement/test_entanglement_manager.cpp
                )
target_compile_definitions(entanglement_manager_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(entanglement_manager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entanglement_manager_test PRIVATE 
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
target_compile_definitions(entanglement_manager_test PRIVATE LOGGING_ENABLED=0)
add_test(NAME entanglement_manager_test COMMAND entanglement_manager_test)

add_executable(message_broker_test
                ${CMAKE_CURRENT_SOURCE_DIR}/MessageBroker/test_message_broker.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/MessageBroker/test_ring_buffer.cpp
                )
target_compile_definitions(message_broker_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(message_broker_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(message_broker_test PRIVATE 
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
target_compile_definitions(message_broker_test PRIVATE LOGGING_ENABLED=0)
add_test(NAME message_broker_test COMMAND message_broker_test)

add_executable(config_loader_test
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_config_loader.cpp
                )
target_compile_definitions(config_loader_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(config_loader_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(config_loader_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME config_loader_test COMMAND config_loader_test)

add_executable(qubit_models_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_qubit_models.cpp
                )
target_compile_definitions(qubit_models_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(qubit_models_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qubit_models_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME qubit_models_test COMMAND qubit_models_test)

add_executable(gate_noise_models_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_gate_noise_models.cpp
                )
target_compile_definitions(gate_noise_models_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(gate_noise_models_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gate_noise_models_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME gate_noise_models_test COMMAND gate_noise_models_test)

add_executable(entanglement_attempt_noise_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_entanglement_attempt_noise.cpp
                )
target_compile_definitions(entanglement_attempt_noise_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(entanglement_attempt_noise_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entanglement_attempt_noise_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME entanglement_attempt_noise_test COMMAND entanglement_attempt_noise_test)

add_executable(entanglement_channel_noise_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_entanglement_channel_noise.cpp
                )
target_compile_definitions(entanglement_channel_noise_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(entanglement_channel_noise_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entanglement_channel_noise_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME entanglement_channel_noise_test COMMAND entanglement_channel_noise_test)

add_executable(operations_engine_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorManager/test_operations_engine.cpp
                )
target_compile_definitions(operations_engine_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(operations_engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(operations_engine_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME operations_engine_test COMMAND operations_engine_test)

add_executable(simulator_manager_test
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorManager/test_simulator_manager.cpp
                )
target_compile_definitions(simulator_manager_test PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(simulator_manager_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(simulator_manager_test PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME simulator_manager_test COMMAND simulator_manager_test)

# -------- Enabled tests (only build if PROF_ENABLED is set) --------
add_executable(prof_tests_enabled 
${CMAKE_CURRENT_SOURCE_DIR}/logger/test_profiler_enabled.cpp)
target_include_directories(prof_tests_enabled PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(prof_tests_enabled PRIVATE GTest::gtest_main profiler qilab_lib)
# Make sure tests see the enabled flag
target_compile_definitions(prof_tests_enabled PRIVATE PROF_ENABLED=1)
add_test(NAME prof_enabled COMMAND prof_tests_enabled)

# -------- Disabled tests (always build) --------
add_executable(prof_tests_disabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_profiler_disabled.cpp)
target_include_directories(prof_tests_disabled PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(prof_tests_disabled PRIVATE PROF_ENABLED=0)
target_link_libraries(prof_tests_disabled PRIVATE GTest::gtest_main qilab_lib)
add_test(NAME prof_disabled COMMAND prof_tests_disabled)

# -------- Data acquisition: enabled (links the real implementation) --------
add_executable(daq_tests_enabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_data_acquisition.cpp)
target_include_directories(daq_tests_enabled PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# DAQ_ENABLED=1 propagates from the data_acquisition target (PUBLIC).
target_link_libraries(daq_tests_enabled PRIVATE GTest::gtest_main data_acquisition qilab_lib)
add_test(NAME daq_enabled COMMAND daq_tests_enabled)

# -------- Data acquisition: disabled (no-op macros, no linking) --------
add_executable(daq_tests_disabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_data_acquisition.cpp)
target_include_directories(daq_tests_disabled PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(daq_tests_disabled PRIVATE DAQ_ENABLED=0)
target_link_libraries(daq_tests_disabled PRIVATE GTest::gtest_main qilab_lib)
add_test(NAME daq_disabled COMMAND daq_tests_disabled)

include(GoogleTest)
gtest_discover_tests(transmission_processor_test)
gtest_discover_tests(prof_tests_disabled)
gtest_discover_tests(prof_tests_enabled)
gtest_discover_tests(daq_tests_enabled)
gtest_discover_tests(daq_tests_disabled)
gtest_discover_tests(entanglement_manager_test)
gtest_discover_tests(message_broker_test)
gtest_discover_tests(config_loader_test)
gtest_discover_tests(qubit_models_test)
gtest_discover_tests(gate_noise_models_test)
gtest_discover_tests(entanglement_attempt_noise_test)
gtest_discover_tests(entanglement_channel_noise_test)
gtest_discover_tests(operations_engine_test)
gtest_discover_tests(simulator_manager_test)

