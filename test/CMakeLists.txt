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

include(GoogleTest)

add_subdirectory(Unit)
add_subdirectory(Functional)

option(QILAB_BUILD_INTEGRATION_TESTS "Build integration tests requiring real hardware" OFF)
if(QILAB_BUILD_INTEGRATION_TESTS)
  add_subdirectory(Integration)
endif()
