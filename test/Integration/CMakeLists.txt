# Scaffold only — no test sources exist yet. Populate once real transmission-
# processor hardware (SPI-originated commands from external devices) is
# available. Guarded by QILAB_BUILD_INTEGRATION_TESTS (default OFF, set in
# test/CMakeLists.txt) so this never affects default builds or CI.
#
# add_executable(integration_tests ...)
# target_link_libraries(integration_tests PRIVATE GTest::gtest GTest::gtest_main qilab_lib)
# gtest_discover_tests(integration_tests PROPERTIES LABELS "integration")
