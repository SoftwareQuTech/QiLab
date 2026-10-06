// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

// tests/logger/test_data_acquisition.cpp
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "logger/data_acquisition_api.hpp"  // API users include only this
#if DAQ_ENABLED
  #include "logger/data_acquisition.hpp"    // direct access for assertions
#endif

namespace fs = std::filesystem;

static fs::path temp_file(const char* tag) {
  return fs::temp_directory_path()
       / (std::string("daq_ut_") + tag + "_" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
          ".jsonl");
}

static std::vector<std::string> read_lines(const fs::path& p) {
  std::ifstream ifs(p);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(ifs, line)) out.push_back(line);
  return out;
}

TEST(DataAcquisition, WritesJsonlRecords) {
#if DAQ_ENABLED
  const fs::path path = temp_file("write");
  DAQ_CONFIGURE(true, path.string());
  ASSERT_TRUE(DAQ_ACTIVE());

  DAQ_RECORD("entanglement").set("attempt", 42).set("success", true).set("fidelity", 0.5);
  DAQ_RECORD("qubit_noise").set("qubit", 3).set("label", "dephase");

  DAQ_CONFIGURE(false, "");  // close the file so the stream is flushed/released

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 2u);

  // Structural checks (kept dependency-free — no JSON lib in the test tree).
  EXPECT_NE(lines[0].find("\"channel\":\"entanglement\""), std::string::npos);
  EXPECT_NE(lines[0].find("\"ts_us\":"), std::string::npos);
  EXPECT_NE(lines[0].find("\"attempt\":42"), std::string::npos);
  EXPECT_NE(lines[0].find("\"success\":true"), std::string::npos);
  EXPECT_EQ(lines[0].front(), '{');
  EXPECT_EQ(lines[0].back(), '}');

  EXPECT_NE(lines[1].find("\"channel\":\"qubit_noise\""), std::string::npos);
  EXPECT_NE(lines[1].find("\"qubit\":3"), std::string::npos);
  EXPECT_NE(lines[1].find("\"label\":\"dephase\""), std::string::npos);

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

TEST(DataAcquisition, DropsRecordsWhenDisabled) {
#if DAQ_ENABLED
  const fs::path path = temp_file("disabled");
  DAQ_CONFIGURE(false, path.string());
  EXPECT_FALSE(DAQ_ACTIVE());

  DAQ_RECORD("should_not_appear").set("x", 1);

  // Nothing was enabled, so no file should have been created/written.
  EXPECT_FALSE(fs::exists(path));
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

TEST(DataAcquisition, FlushMakesRecordsReadableMidRun) {
#if DAQ_ENABLED
  const fs::path path = temp_file("flush");
  DAQ_CONFIGURE(true, path.string());

  for (int i = 0; i < 5; ++i) {
    DAQ_RECORD("loop").set("i", i);
  }
  // Without per-line flush, records sit in the in-memory accumulator until we
  // ask for them. DAQ_FLUSH() (called here as a non-RT thread would) spills them.
  DAQ_FLUSH();

  const auto lines = read_lines(path);
  EXPECT_EQ(lines.size(), 5u);

  DAQ_CONFIGURE(false, "");
  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

TEST(DataAcquisition, EscapesStringValues) {
#if DAQ_ENABLED
  const fs::path path = temp_file("escape");
  DAQ_CONFIGURE(true, path.string());

  DAQ_RECORD("esc").set("msg", "he said \"hi\"\nnewline\ttab\\slash");

  DAQ_CONFIGURE(false, "");

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 1u);
  // Raw control chars must not leak into the JSONL line.
  EXPECT_EQ(lines[0].find('\t'), std::string::npos);
  EXPECT_NE(lines[0].find("\\\""), std::string::npos);  // escaped quote
  EXPECT_NE(lines[0].find("\\n"), std::string::npos);   // escaped newline
  EXPECT_NE(lines[0].find("\\t"), std::string::npos);   // escaped tab
  EXPECT_NE(lines[0].find("\\\\"), std::string::npos);  // escaped backslash

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Concurrency: N threads each writing M records via DAQ_RECORD, hammering
// write_line()'s mutex-guarded accumulator append at once. Exercises that the
// append (pending_.append + push_back('\n')) never interleaves or truncates
// across threads, so every one of N*M lines comes out well-formed and none is
// lost, merged, or corrupted. Modeled after the multi-thread stress tests in
// test/Unit/MessageBroker/test_ring_buffer.cpp.
TEST(DataAcquisition, ConcurrentWritesProduceExactLineCountWithNoCorruption) {
#if DAQ_ENABLED
  const fs::path path = temp_file("concurrent");
  DAQ_CONFIGURE(true, path.string());
  ASSERT_TRUE(DAQ_ACTIVE());

  constexpr int kNumThreads = 8;
  constexpr int kRecordsPerThread = 500;

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([t]() {
      for (int i = 0; i < kRecordsPerThread; ++i) {
        DAQ_RECORD("concurrent").set("thread", t).set("i", i);
      }
    });
  }
  for (auto& th : threads) th.join();

  DAQ_FLUSH();
  DAQ_CONFIGURE(false, "");

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), static_cast<std::size_t>(kNumThreads * kRecordsPerThread));

  for (const auto& line : lines) {
    ASSERT_FALSE(line.empty());
    EXPECT_EQ(line.front(), '{');
    EXPECT_EQ(line.back(), '}');
    EXPECT_NE(line.find("\"channel\":"), std::string::npos);
  }

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Auto-spill: write comfortably past the internal spill threshold (1 MiB,
// private kSpillThresholdBytes) WITHOUT ever calling DAQ_FLUSH(), and confirm
// records are already readable from a freshly-opened handle on the same file.
// This proves write_line()'s automatic spill_locked() call (triggered once
// pending_ crosses the threshold) really pushes data to the OS stream on its
// own, rather than relying on an explicit flush.
TEST(DataAcquisition, AutoSpillsPastThresholdWithoutExplicitFlush) {
#if DAQ_ENABLED
  const fs::path path = temp_file("autospill");
  DAQ_CONFIGURE(true, path.string());

  // ~1 KB payload per record; a couple thousand of these is comfortably past
  // the 1 MiB threshold without needing to know its exact private value.
  const std::string big_value(1024, 'x');
  constexpr int kNumRecords = 2000;
  for (int i = 0; i < kNumRecords; ++i) {
    DAQ_RECORD("autospill").set("i", i).set("payload", big_value);
  }

  // No DAQ_FLUSH() here: read via a brand-new ifstream and expect to already
  // see records on disk, purely from the automatic spill inside write_line().
  const auto lines_before_flush = read_lines(path);
  EXPECT_GT(lines_before_flush.size(), 0u)
      << "expected write_line()'s automatic spill_locked() to push records "
         "to disk once the accumulator crossed the spill threshold";

  // Sanity: an explicit flush at the end still accounts for every record.
  DAQ_FLUSH();
  const auto lines_after_flush = read_lines(path);
  EXPECT_EQ(lines_after_flush.size(), static_cast<std::size_t>(kNumRecords));

  DAQ_CONFIGURE(false, "");
  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Reconfigure-while-pending: buffered-but-unflushed records must be spilled
// to the ORIGINAL file before configure() switches to a new one (per the
// source's own "don't lose records buffered under the previous config"
// comment on the spill_locked() call at the top of configure()).
TEST(DataAcquisition, ReconfigureFlushesPendingRecordsToOriginalFile) {
#if DAQ_ENABLED
  const fs::path path_a = temp_file("reconfig_a");
  const fs::path path_b = temp_file("reconfig_b");

  DAQ_CONFIGURE(true, path_a.string());
  for (int i = 0; i < 5; ++i) {
    DAQ_RECORD("first").set("i", i);
  }
  // Deliberately no flush: these records only exist in the in-memory
  // accumulator under path_a's configuration at this point.

  DAQ_CONFIGURE(true, path_b.string());

  const auto lines_a = read_lines(path_a);
  ASSERT_EQ(lines_a.size(), 5u);
  for (const auto& line : lines_a) {
    EXPECT_NE(line.find("\"channel\":\"first\""), std::string::npos);
  }

  DAQ_RECORD("second").set("i", 1);
  DAQ_FLUSH();
  const auto lines_b = read_lines(path_b);
  ASSERT_EQ(lines_b.size(), 1u);
  EXPECT_NE(lines_b[0].find("\"channel\":\"second\""), std::string::npos);

  DAQ_CONFIGURE(false, "");
  fs::remove(path_a);
  fs::remove(path_b);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// File-open failure: configuring against a path that can't be opened as a
// regular file (here, an existing directory) must leave acquisition inactive
// rather than throw/crash, and subsequent records must silently no-op.
TEST(DataAcquisition, OpenFailureLeavesAcquisitionInactiveAndDoesNotCrash) {
#if DAQ_ENABLED
  const fs::path dir_path = temp_file("open_fail_dir");
  fs::create_directory(dir_path);

  DAQ_CONFIGURE(true, dir_path.string());
  EXPECT_FALSE(DAQ_ACTIVE());

  // Must not crash; the record should just be silently dropped.
  DAQ_RECORD("should_not_appear").set("x", 1);
  DAQ_FLUSH();

  DAQ_CONFIGURE(false, "");
  fs::remove_all(dir_path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Numeric edge cases: NaN/Inf collapse to JSON null (append_number's
// documented behavior), and integer extremes/negatives/zero round-trip via
// std::to_chars exactly.
TEST(DataAcquisition, NumericEdgeCasesSerializeCorrectly) {
#if DAQ_ENABLED
  const fs::path path = temp_file("numeric");
  DAQ_CONFIGURE(true, path.string());

  DAQ_RECORD("nan_inf")
      .set("nan_val", std::numeric_limits<double>::quiet_NaN())
      .set("pos_inf", std::numeric_limits<double>::infinity())
      .set("neg_inf", -std::numeric_limits<double>::infinity());

  DAQ_RECORD("int_extremes")
      .set("int_min", std::numeric_limits<std::int64_t>::min())
      .set("int_max", std::numeric_limits<std::int64_t>::max())
      .set("neg", static_cast<std::int64_t>(-42))
      .set("zero", static_cast<std::int64_t>(0));

  DAQ_CONFIGURE(false, "");

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 2u);

  EXPECT_NE(lines[0].find("\"nan_val\":null"), std::string::npos);
  EXPECT_NE(lines[0].find("\"pos_inf\":null"), std::string::npos);
  EXPECT_NE(lines[0].find("\"neg_inf\":null"), std::string::npos);

  EXPECT_NE(lines[1].find("\"int_min\":-9223372036854775808"), std::string::npos);
  EXPECT_NE(lines[1].find("\"int_max\":9223372036854775807"), std::string::npos);
  EXPECT_NE(lines[1].find("\"neg\":-42"), std::string::npos);
  EXPECT_NE(lines[1].find("\"zero\":0"), std::string::npos);

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// String edge cases: empty channel/key/value must still serialize as valid
// (if degenerate) JSON, and a several-KB value must force the reused
// thread-local record buffer to grow past its warm-up capacity without
// corrupting the record.
TEST(DataAcquisition, StringEdgeCasesHandleEmptyAndOversizedValues) {
#if DAQ_ENABLED
  const fs::path path = temp_file("string_edge");
  DAQ_CONFIGURE(true, path.string());

  DAQ_RECORD("").set("", std::string_view(""));

  const std::string large_value(8192, 'z');  // force tls_record_buffer to grow
  DAQ_RECORD("large").set("blob", large_value);

  DAQ_CONFIGURE(false, "");

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 2u);

  EXPECT_NE(lines[0].find("\"channel\":\"\""), std::string::npos);
  EXPECT_NE(lines[0].find("\"\":\"\""), std::string::npos);
  EXPECT_EQ(lines[0].front(), '{');
  EXPECT_EQ(lines[0].back(), '}');

  EXPECT_NE(lines[1].find("\"channel\":\"large\""), std::string::npos);
  EXPECT_NE(lines[1].find(large_value), std::string::npos);
  EXPECT_EQ(lines[1].front(), '{');
  EXPECT_EQ(lines[1].back(), '}');

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Path resolution: a relative base_path must resolve under
// $HOME/.qilab/logs/<name>, exercising resolve_daq_path()'s non-absolute
// branch indirectly through configure(). HOME is saved/restored so this
// doesn't affect other tests sharing this binary.
TEST(DataAcquisition, RelativePathResolvesUnderHomeQilabLogs) {
#if DAQ_ENABLED
  // RAII guard: restores HOME on scope exit no matter how the test exits
  // (including an early return from a failing ASSERT_* below), so a failing
  // assertion here can't leak a stale HOME into other tests in this binary.
  struct HomeEnvGuard {
    bool had_home;
    std::string original_home;
    HomeEnvGuard() {
      const char* p = std::getenv("HOME");
      had_home = (p != nullptr);
      original_home = had_home ? p : "";
    }
    ~HomeEnvGuard() {
      if (had_home) {
        setenv("HOME", original_home.c_str(), 1);
      } else {
        unsetenv("HOME");
      }
    }
  } home_guard;

  const fs::path fake_home =
      fs::temp_directory_path() /
      (std::string("daq_ut_home_") +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(fake_home);
  setenv("HOME", fake_home.string().c_str(), 1);

  const std::string relative_name = "relative_daq_test.jsonl";
  DAQ_CONFIGURE(true, relative_name);
  ASSERT_TRUE(DAQ_ACTIVE());

  DAQ_RECORD("home_path").set("ok", true);
  DAQ_CONFIGURE(false, "");

  const fs::path expected = fake_home / ".qilab" / "logs" / relative_name;
  ASSERT_TRUE(fs::exists(expected));
  const auto lines = read_lines(expected);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].find("\"channel\":\"home_path\""), std::string::npos);

  fs::remove_all(fake_home);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// Idempotent reconfigure: calling DAQ_CONFIGURE(true, ...) twice in a row (no
// writes between) and DAQ_CONFIGURE(false, "") twice in a row must both be
// safe no-op-ish operations, not crash, and not duplicate/drop records.
TEST(DataAcquisition, IdempotentReconfigureIsSafe) {
#if DAQ_ENABLED
  const fs::path path = temp_file("idempotent");

  DAQ_CONFIGURE(true, path.string());
  ASSERT_TRUE(DAQ_ACTIVE());
  DAQ_CONFIGURE(true, path.string());
  ASSERT_TRUE(DAQ_ACTIVE());

  DAQ_RECORD("idempotent").set("i", 1);
  DAQ_FLUSH();

  DAQ_CONFIGURE(false, "");
  ASSERT_FALSE(DAQ_ACTIVE());
  DAQ_CONFIGURE(false, "");
  ASSERT_FALSE(DAQ_ACTIVE());

  // Disabled now, so this must be silently dropped, not appended.
  DAQ_RECORD("should_not_appear").set("i", 2);

  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].find("\"channel\":\"idempotent\""), std::string::npos);

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// QILAB_DAQ_DIR relocates relative output off $HOME. Without this the deployed
// service cannot write DAQ data at all: its systemd unit sets ProtectHome=true
// and only grants write access to /var/log/qilab.
TEST(DataAcquisition, DaqDirEnvVarRelocatesRelativePath) {
#if DAQ_ENABLED
  struct EnvGuard {
    const char* name;
    bool had_value;
    std::string original;
    explicit EnvGuard(const char* n) : name(n) {
      const char* p = std::getenv(n);
      had_value = (p != nullptr);
      original = had_value ? p : "";
    }
    ~EnvGuard() {
      if (had_value) { setenv(name, original.c_str(), 1); } else { unsetenv(name); }
    }
  } dir_guard("QILAB_DAQ_DIR"), path_guard("QILAB_DAQ_PATH");

  const fs::path daq_dir =
      fs::temp_directory_path() /
      (std::string("daq_ut_dir_") +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(daq_dir);
  unsetenv("QILAB_DAQ_PATH");
  setenv("QILAB_DAQ_DIR", daq_dir.string().c_str(), 1);

  const std::string relative_name = "env_dir_test.jsonl";
  DAQ_CONFIGURE(true, relative_name);
  ASSERT_TRUE(DAQ_ACTIVE());
  DAQ_RECORD("env_dir").set("ok", true);
  DAQ_CONFIGURE(false, "");

  const fs::path expected = daq_dir / relative_name;
  ASSERT_TRUE(fs::exists(expected));
  const auto lines = read_lines(expected);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].find("\"channel\":\"env_dir\""), std::string::npos);

  fs::remove_all(daq_dir);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// QILAB_DAQ_PATH is a full-path override and wins over QILAB_DAQ_DIR, matching
// the QILAB_PROFILER_PATH / QILAB_PROFILER_DIR precedence in profiler.cpp.
TEST(DataAcquisition, DaqPathEnvVarOverridesDaqDir) {
#if DAQ_ENABLED
  struct EnvGuard {
    const char* name;
    bool had_value;
    std::string original;
    explicit EnvGuard(const char* n) : name(n) {
      const char* p = std::getenv(n);
      had_value = (p != nullptr);
      original = had_value ? p : "";
    }
    ~EnvGuard() {
      if (had_value) { setenv(name, original.c_str(), 1); } else { unsetenv(name); }
    }
  } dir_guard("QILAB_DAQ_DIR"), path_guard("QILAB_DAQ_PATH");

  const std::string stamp =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const fs::path ignored_dir = fs::temp_directory_path() / ("daq_ut_ignored_" + stamp);
  fs::create_directories(ignored_dir);
  const fs::path explicit_path = fs::temp_directory_path() / ("daq_ut_explicit_" + stamp + ".jsonl");

  setenv("QILAB_DAQ_DIR", ignored_dir.string().c_str(), 1);
  setenv("QILAB_DAQ_PATH", explicit_path.string().c_str(), 1);

  DAQ_CONFIGURE(true, "some_relative_name.jsonl");
  ASSERT_TRUE(DAQ_ACTIVE());
  DAQ_RECORD("env_path").set("ok", true);
  DAQ_CONFIGURE(false, "");

  EXPECT_TRUE(fs::exists(explicit_path));
  EXPECT_FALSE(fs::exists(ignored_dir / "some_relative_name.jsonl"));

  fs::remove(explicit_path);
  fs::remove_all(ignored_dir);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}

// An absolute base path still wins over both env vars, so tests and callers that
// pass a concrete file keep working regardless of the environment.
TEST(DataAcquisition, AbsolutePathIgnoresEnvVars) {
#if DAQ_ENABLED
  struct EnvGuard {
    const char* name;
    bool had_value;
    std::string original;
    explicit EnvGuard(const char* n) : name(n) {
      const char* p = std::getenv(n);
      had_value = (p != nullptr);
      original = had_value ? p : "";
    }
    ~EnvGuard() {
      if (had_value) { setenv(name, original.c_str(), 1); } else { unsetenv(name); }
    }
  } dir_guard("QILAB_DAQ_DIR"), path_guard("QILAB_DAQ_PATH");

  const fs::path path = temp_file("absolute_wins");
  setenv("QILAB_DAQ_DIR", "/nonexistent-daq-dir", 1);
  setenv("QILAB_DAQ_PATH", "/nonexistent-daq-path.jsonl", 1);

  DAQ_CONFIGURE(true, path.string());
  ASSERT_TRUE(DAQ_ACTIVE());
  DAQ_RECORD("absolute").set("ok", true);
  DAQ_CONFIGURE(false, "");

  ASSERT_TRUE(fs::exists(path));
  const auto lines = read_lines(path);
  ASSERT_EQ(lines.size(), 1u);

  fs::remove(path);
#else
  GTEST_SKIP() << "Data acquisition disabled";
#endif
}
