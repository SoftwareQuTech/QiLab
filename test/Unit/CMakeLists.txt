# Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
# SPDX-License-Identifier: BSD-3-Clause-Clear

add_executable(unit_tests
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_config_loader.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_qubit_parsers.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_scalar_helpers.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_entanglement_parsers.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/TransmissionProcessor/test_transmission_processor_sync.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/TransmissionProcessor/test_transmission_processor_simple.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/handlers/test_command_helper.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/handlers/test_mock_handler.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTypes/test_commands.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/MessageBroker/test_message_broker.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/MessageBroker/test_ring_buffer.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/test_pauli_channel_math.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_qubit_models.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_gate_noise_models.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_entanglement_attempt_noise.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/NoiseModels/test_entanglement_link_noise.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorManager/test_operations_engine.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorAdapters/test_stim_adapter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorAdapters/test_stim_nv_adapter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorAdapters/test_mock_adapter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorRunners/test_simulator_runner.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorRunners/test_entanglement_runner.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/DataTransmission/TransmissionRunner/test_transmission_runner.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/utils/test_periodic_pacer.cpp
                )
target_compile_definitions(unit_tests PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(unit_tests PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib libstim)
add_test(NAME unit_tests COMMAND unit_tests)

# -------- Enabled tests (only build if PROF_ENABLED is set) --------
add_executable(prof_tests_enabled
${CMAKE_CURRENT_SOURCE_DIR}/logger/test_profiler_enabled.cpp)
target_include_directories(prof_tests_enabled PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(prof_tests_enabled PRIVATE GTest::gtest_main profiler qilab_lib)
# Make sure tests see the enabled flag
target_compile_definitions(prof_tests_enabled PRIVATE PROF_ENABLED=1)
add_test(NAME prof_enabled COMMAND prof_tests_enabled)

# -------- Disabled tests (always build) --------
add_executable(prof_tests_disabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_profiler_disabled.cpp)
target_include_directories(prof_tests_disabled PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_compile_definitions(prof_tests_disabled PRIVATE PROF_ENABLED=0)
target_link_libraries(prof_tests_disabled PRIVATE GTest::gtest_main qilab_lib)
add_test(NAME prof_disabled COMMAND prof_tests_disabled)

# -------- Data acquisition: enabled (links the real implementation) --------
add_executable(daq_tests_enabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_data_acquisition.cpp)
target_include_directories(daq_tests_enabled PRIVATE ${CMAKE_SOURCE_DIR}/test)
# DAQ_ENABLED=1 propagates from the data_acquisition target (PUBLIC).
target_link_libraries(daq_tests_enabled PRIVATE GTest::gtest_main data_acquisition qilab_lib)
add_test(NAME daq_enabled COMMAND daq_tests_enabled)

# -------- Data acquisition: disabled (no-op macros, no linking) --------
add_executable(daq_tests_disabled
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_data_acquisition.cpp)
target_include_directories(daq_tests_disabled PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_compile_definitions(daq_tests_disabled PRIVATE DAQ_ENABLED=0)
target_link_libraries(daq_tests_disabled PRIVATE GTest::gtest_main qilab_lib)
add_test(NAME daq_disabled COMMAND daq_tests_disabled)

# -------- StandardLogger: needs LOGGING_ENABLED=1, which conflicts with
# unit_tests' global LOGGING_ENABLED=0, so it gets its own small binary
# (same reasoning as the prof/daq enabled/disabled split above). --------
add_executable(logger_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/logger/test_logger.cpp)
target_include_directories(logger_tests PRIVATE ${CMAKE_SOURCE_DIR}/test)
# LOGGING_ENABLED=1 propagates from the logger target (PUBLIC).
target_link_libraries(logger_tests PRIVATE GTest::gtest_main logger)
add_test(NAME logger_tests COMMAND logger_tests)

gtest_discover_tests(unit_tests          PROPERTIES LABELS "unit")
gtest_discover_tests(prof_tests_enabled  PROPERTIES LABELS "unit")
gtest_discover_tests(prof_tests_disabled PROPERTIES LABELS "unit")
gtest_discover_tests(daq_tests_enabled   PROPERTIES LABELS "unit")
gtest_discover_tests(daq_tests_disabled  PROPERTIES LABELS "unit")
gtest_discover_tests(logger_tests        PROPERTIES LABELS "unit")
