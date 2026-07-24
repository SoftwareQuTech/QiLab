add_executable(functional_tests
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/SimulatorManager/test_simulator_manager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Entanglement/test_entanglement_manager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Pipeline/test_full_pipeline_single_node_ops.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Pipeline/test_full_pipeline_entanglement_ops.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/ConfigurationEngine/test_entanglement_config_integration.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/Simulator/test_noise_statistical_validation.cpp
                )
target_compile_definitions(functional_tests PRIVATE PROF_ENABLED=0 LOGGING_ENABLED=0)
target_include_directories(functional_tests PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(functional_tests PRIVATE
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib libstim)
add_test(NAME functional_tests COMMAND functional_tests)

gtest_discover_tests(functional_tests PROPERTIES LABELS "functional")
