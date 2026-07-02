// tests/logger/test_data_acquisition.cpp
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
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
