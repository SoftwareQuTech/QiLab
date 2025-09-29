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

add_executable(stim_command_handler_test 
                ${CMAKE_CURRENT_SOURCE_DIR}/command_handler/test_command_handler.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_transmission.hpp
                )
target_include_directories(stim_command_handler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(stim_command_handler_test PRIVATE 
GTest::gtest GTest::gtest_main GTest::gmock qilab_lib)
add_test(NAME stim_command_handler_test COMMAND stim_command_handler_test)

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

include(GoogleTest)
#gtest_discover_tests(stim_command_handler_test prof_tests_disabled prof_tests_enabled)

