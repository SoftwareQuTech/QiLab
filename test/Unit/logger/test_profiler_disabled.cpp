// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

// tests/test_prof_disabled.cpp
#include <gtest/gtest.h>
#include "logger/profiler_api.hpp"

TEST(Disabled, MacrosCompileAndDoNothing) {
#if PROF_ENABLED
  GTEST_SKIP() << "This target is for PROF_ENABLED=0";
#else
  // These should all compile and run without linking prof.cpp
  PROF_THREAD_INIT();
  PROF_START("foo");
  PROF_STOP("foo");
  PROF_EVENT("hello");
  PROF_THREAD_FLUSH();
  SUCCEED();
#endif
}
