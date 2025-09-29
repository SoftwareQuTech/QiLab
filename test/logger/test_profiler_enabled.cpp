// tests/test_prof_enabled.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <thread>
#include <vector>
#include <atomic>

#include "logger/profiler_api.hpp"   // API users include only this
#if PROF_ENABLED
  #include "logger/profiler.hpp"    // we use Profiler::dump_csv in assertions
#endif

namespace fs = std::filesystem;

static fs::path temp_base(const char* tag) {
  auto base = fs::temp_directory_path()
            / (std::string("prof_ut_") + tag + "_" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  return base;
}

static std::vector<std::string> read_lines(const fs::path& p) {
  std::ifstream ifs(p);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(ifs, line)) out.push_back(line);
  return out;
}

TEST(Registration, SameNameSameId) {
#if PROF_ENABLED
  auto& P = Profiler::instance();
  auto id1 = P.register_event("foo");
  auto id2 = P.register_event("foo");
  EXPECT_EQ(id1, id2);
  EXPECT_GE(P.event_count(), 1u);
#else
  GTEST_SKIP() << "Profiling disabled";
#endif
}

TEST(LazyMacros, StartStopWorksAndRecords)
{
#if PROF_ENABLED
  {
    ProfGuard guard{1024}; // RAII init/flush for THIS thread
    PROF_START("macro_event");
    PROF_STOP("macro_event");
  }
  //PROF_THREAD_FLUSH();

  auto base = temp_base("lazy");
  Profiler::instance().dump_csv(base.string());

  auto durations = read_lines(base.string() + "_durations.csv");
  // Header + >=1 row for "macro_event"
  ASSERT_GE(durations.size(), 2u);
  bool found = false;
  for (auto &ln : durations)
  {
    if (ln.rfind("macro_event,", 0) == 0)
    {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
#else
  GTEST_SKIP() << "Profiling disabled";
#endif
}

TEST(ThreadInitAndGrow, RegisterAfterInitStillWorks) {
#if PROF_ENABLED
  // Pre-register one event, then use a new one AFTER thread_init via macro.
  (void)PROF_EID("pre_registered");

  std::thread t([]{
    ProfGuard guard{1024};
    // This event name is registered lazily after thread_init; TLS must grow.
    for (int i=0;i<10;++i) { PROF_START("post_registered"); PROF_STOP("post_registered"); }
  });
  t.join();

  auto base = temp_base("grow");
  Profiler::instance().dump_csv(base.string());
  auto durations = read_lines(base.string() + "_durations.csv");

  bool seen_pre  = false, seen_post = false;
  for (auto& ln : durations) {
    if (ln.rfind("pre_registered,", 0) == 0)  seen_pre  = true;
    if (ln.rfind("post_registered,", 0) == 0) seen_post = true;
  }
  EXPECT_TRUE(seen_post); // post_registered samples must be present
#else
  GTEST_SKIP() << "Profiling disabled";
#endif
}

TEST(FreeFormEvents, MessageAndTimestampWritten) {
#if PROF_ENABLED
  ProfGuard guard{512};
  PROF_EVENT("worker start");
  PROF_EVENT("worker ok");
  auto base = temp_base("events");
  Profiler::instance().dump_csv(base.string());
  auto events = read_lines(base.string() + "_events.csv");
  ASSERT_GE(events.size(), 3u); // header + at least 2 rows

  // Find a row with "worker start", timestamp should be digits
  bool found = false;
  std::regex re(R"(worker start,\d+)");
  for (auto& ln : events) if (std::regex_match(ln, re)) { found = true; break; }
  EXPECT_TRUE(found);
#else
  GTEST_SKIP() << "Profiling disabled";
#endif
}

TEST(MultiThread, AggregateAllSamples) {
#if PROF_ENABLED
  const int T = 4;
  const int N = 2000;
  std::atomic<int> ready{0};

  auto worker = [&]{
    ProfGuard guard{N};
    ready.fetch_add(1);
    // tiny barrier
    while (ready.load() < T) std::this_thread::yield();
    for (int i=0;i<N;++i) { PROF_START("mt_nop"); PROF_STOP("mt_nop"); }
  };

  std::vector<std::thread> threads;
  threads.reserve(T);
  for (int i=0;i<T;++i) threads.emplace_back(worker);
  for (auto& th : threads) th.join();

  auto base = temp_base("mt");
  Profiler::instance().dump_csv(base.string());
  auto durations = read_lines(base.string() + "_durations.csv");

  // Count rows for mt_nop
  int count = 0;
  for (auto& ln : durations) if (ln.rfind("mt_nop,", 0) == 0) ++count;

  // header row present; we want exactly T*N rows for mt_nop
  EXPECT_EQ(count, T * N);
#else
  GTEST_SKIP() << "Profiling disabled";
#endif
}
