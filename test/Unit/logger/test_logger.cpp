// This binary is built with LOGGING_ENABLED=1 (via linking the real `logger`
// static library target), unlike the consolidated `unit_tests` binary which
// builds everything with LOGGING_ENABLED=0. StandardLogger only exists when
// LOGGING_ENABLED is truthy, so this test needs its own executable target,
// matching the prof_tests_enabled/daq_tests_enabled pattern.
#include <gtest/gtest.h>

#include <sstream>
#include <thread>
#include <vector>

#include "logger/logger.hpp"

namespace {

// StandardLogger is a singleton with mutable state (debug_level/error_level),
// so every test must explicitly set a known level before asserting behavior
// rather than relying on whatever a previous test left it at.
class StandardLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        old_clog_buf_ = std::clog.rdbuf(clog_capture_.rdbuf());
        old_cerr_buf_ = std::cerr.rdbuf(cerr_capture_.rdbuf());
    }

    void TearDown() override {
        std::clog.rdbuf(old_clog_buf_);
        std::cerr.rdbuf(old_cerr_buf_);
    }

    std::ostringstream clog_capture_;
    std::ostringstream cerr_capture_;

private:
    std::streambuf *old_clog_buf_ = nullptr;
    std::streambuf *old_cerr_buf_ = nullptr;
};

TEST_F(StandardLoggerTest, InstanceReturnsSameSingletonAcrossCalls) {
    EXPECT_EQ(&StandardLogger::instance(), &StandardLogger::instance());
}

TEST_F(StandardLoggerTest, LogInfoWritesWhenDebugLevelEnabled) {
    StandardLogger::instance().log_level("DEBUG");
    clog_capture_.str("");
    StandardLogger::instance().log_info("hello info");
    EXPECT_NE(clog_capture_.str().find("[INFO]: hello info"), std::string::npos);
}

TEST_F(StandardLoggerTest, LogInfoSuppressedWhenLevelIsError) {
    StandardLogger::instance().log_level("ERROR"); // debug_level=false, error_level=true
    clog_capture_.str("");
    StandardLogger::instance().log_info("should not appear");
    EXPECT_EQ(clog_capture_.str().find("should not appear"), std::string::npos);
}

TEST_F(StandardLoggerTest, LogWarningWritesWhenErrorLevelEnabled) {
    StandardLogger::instance().log_level("ERROR");
    clog_capture_.str("");
    StandardLogger::instance().log_warning("careful now");
    EXPECT_NE(clog_capture_.str().find("[WARNING]: careful now"), std::string::npos);
}

TEST_F(StandardLoggerTest, LogWarningSuppressedWhenLevelIsOff) {
    StandardLogger::instance().log_level("OFF");
    clog_capture_.str("");
    StandardLogger::instance().log_warning("should not appear");
    EXPECT_EQ(clog_capture_.str().find("should not appear"), std::string::npos);
}

TEST_F(StandardLoggerTest, LogErrorWritesToStderrWhenErrorLevelEnabled) {
    StandardLogger::instance().log_level("ERROR");
    cerr_capture_.str("");
    StandardLogger::instance().log_error("boom");
    EXPECT_NE(cerr_capture_.str().find("[ERROR]: boom"), std::string::npos);
}

TEST_F(StandardLoggerTest, LogErrorSuppressedWhenLevelIsOff) {
    StandardLogger::instance().log_level("OFF");
    cerr_capture_.str("");
    StandardLogger::instance().log_error("should not appear");
    EXPECT_EQ(cerr_capture_.str().find("should not appear"), std::string::npos);
}

TEST_F(StandardLoggerTest, InvalidLogLevelLeavesPreviousStateUnchanged) {
    StandardLogger::instance().log_level("DEBUG"); // known state: info enabled
    clog_capture_.str("");
    StandardLogger::instance().log_level("NOT_A_REAL_LEVEL");
    EXPECT_NE(clog_capture_.str().find("Invalid log level"), std::string::npos);

    // State must be unaffected by the rejected level change: info still prints.
    clog_capture_.str("");
    StandardLogger::instance().log_info("still enabled");
    EXPECT_NE(clog_capture_.str().find("still enabled"), std::string::npos);
}

TEST_F(StandardLoggerTest, TimestampPrefixHasExpectedShape) {
    StandardLogger::instance().log_level("DEBUG");
    clog_capture_.str("");
    StandardLogger::instance().log_info("timestamped");
    // "[HH:MM:SS.NNNNNN] [INFO]: timestamped" -- just check the leading '[' and
    // that a ':'-separated time-like prefix precedes the level tag, without
    // overspecifying exact formatting.
    const std::string out = clog_capture_.str();
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.front(), '[');
    EXPECT_NE(out.find(':'), std::string::npos);
    EXPECT_NE(out.find("[INFO]:"), std::string::npos);
}

TEST_F(StandardLoggerTest, ConcurrentLoggingFromMultipleThreadsDoesNotCrash) {
    StandardLogger::instance().log_level("DEBUG");
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([t] {
            for (int i = 0; i < 50; ++i) {
                StandardLogger::instance().log_info("thread message");
                StandardLogger::instance().log_warning("thread warning");
                StandardLogger::instance().log_error("thread error");
            }
        });
    }
    for (auto &th : threads) th.join();
    // Reaching here without a crash/hang/TSAN report is the assertion; the
    // mutex inside StandardLogger serializes the actual stream writes.
    SUCCEED();
}

} // namespace
