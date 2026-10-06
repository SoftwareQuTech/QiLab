// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 09.09.2026
** Created by: mrkolev
** Description: Unit tests for the real-time PeriodicPacer
*/

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "utils/periodic_pacer.hpp"

using thread_utils::PeriodicPacer;
using namespace std::chrono;

namespace {

/// Busy portion of a simulated cycle body. Deliberately a sleep, not a spin:
/// these tests run at normal priority, so accuracy of the *pacer* is what matters.
void simulate_cycle_work(milliseconds work)
{
    std::this_thread::sleep_for(work);
}

} // namespace

// A cycle with no work must still take at least the configured period.
TEST(PeriodicPacer, BlocksForAtLeastThePeriod)
{
    constexpr auto period = milliseconds(5);
    PeriodicPacer pacer{period};

    const auto start = steady_clock::now();
    EXPECT_TRUE(pacer.wait_for_next_cycle());
    const auto elapsed = steady_clock::now() - start;

    EXPECT_GE(elapsed, period);
    EXPECT_EQ(pacer.overruns(), 0u);
}

// Ten empty cycles must take at least ten periods: the pacer paces, it does not
// collapse cycles.
TEST(PeriodicPacer, AccumulatesOnePeriodPerCycle)
{
    constexpr auto period = milliseconds(2);
    constexpr int cycles = 10;
    PeriodicPacer pacer{period};

    const auto start = steady_clock::now();
    for (int i = 0; i < cycles; ++i)
    {
        EXPECT_TRUE(pacer.wait_for_next_cycle());
    }
    const auto elapsed = steady_clock::now() - start;

    EXPECT_GE(elapsed, period * cycles);
}

// The core guarantee of an absolute deadline: cycle-body duration does not push the
// cadence out. A relative sleep_for(period) would take cycles * (work + period).
TEST(PeriodicPacer, CadenceDoesNotDriftWithCycleWorkTime)
{
    constexpr auto period = milliseconds(20);
    constexpr auto work = milliseconds(15);
    constexpr int cycles = 6;
    PeriodicPacer pacer{period};

    const auto start = steady_clock::now();
    for (int i = 0; i < cycles; ++i)
    {
        simulate_cycle_work(work);
        pacer.wait_for_next_cycle();
    }
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);

    // Drift-free: ~cycles * period (120 ms). Drifting: ~cycles * (work + period) (210 ms).
    EXPECT_GE(elapsed, period * cycles - milliseconds(5));
    EXPECT_LT(elapsed, milliseconds(165));
}

// A body that outruns the period is reported, not silently absorbed.
TEST(PeriodicPacer, ReportsOverrunWhenBodyExceedsPeriod)
{
    constexpr auto period = milliseconds(1);
    PeriodicPacer pacer{period};

    simulate_cycle_work(milliseconds(10));
    EXPECT_FALSE(pacer.wait_for_next_cycle());
    EXPECT_EQ(pacer.overruns(), 1u);
}

// After an overrun the deadline is re-anchored to now, so the following cycle is
// paced normally instead of firing a burst of zero-length catch-up cycles.
TEST(PeriodicPacer, ReanchorsAfterOverrunInsteadOfChasingTheSchedule)
{
    constexpr auto period = milliseconds(10);
    PeriodicPacer pacer{period};

    simulate_cycle_work(milliseconds(50));
    ASSERT_FALSE(pacer.wait_for_next_cycle());

    const auto start = steady_clock::now();
    EXPECT_TRUE(pacer.wait_for_next_cycle());
    const auto elapsed = steady_clock::now() - start;

    // A pacer chasing the missed deadlines would return immediately here.
    EXPECT_GE(elapsed, period - milliseconds(1));
    EXPECT_EQ(pacer.overruns(), 1u);
}

// The regression this class exists for: a permanently overrunning loop must still
// release the core. With a zero period every cycle overruns, so only the forced
// relinquish can block — and it must, or the thread never yields its CPU.
TEST(PeriodicPacer, PermanentOverrunStillReleasesTheCore)
{
    PeriodicPacer pacer{nanoseconds(0)};

    constexpr auto blocking_threshold = nanoseconds(PeriodicPacer::kRelinquishBlockNs);
    const auto test_deadline = steady_clock::now() + milliseconds(20);

    bool observed_blocking_wait = false;
    uint64_t cycles = 0;
    while (steady_clock::now() < test_deadline)
    {
        const auto start = steady_clock::now();
        EXPECT_FALSE(pacer.wait_for_next_cycle());
        if (steady_clock::now() - start >= blocking_threshold)
        {
            observed_blocking_wait = true;
        }
        ++cycles;
    }

    EXPECT_TRUE(observed_blocking_wait)
        << "pacer never blocked in " << cycles << " overrunning cycles: the pinned "
        << "SCHED_FIFO thread would starve the per-CPU kernel threads on its core";
    EXPECT_EQ(pacer.overruns(), cycles);
}

// A negative configured period is treated as invalid (every cycle overruns) rather
// than winding the deadline backwards.
TEST(PeriodicPacer, NegativePeriodIsTreatedAsInvalid)
{
    PeriodicPacer pacer{milliseconds(-5)};

    EXPECT_FALSE(pacer.wait_for_next_cycle());
    EXPECT_EQ(pacer.overruns(), 1u);
}

// Counters are reported per cycle, so a flat-out loop is distinguishable from one
// that is unexpectedly missing an achievable deadline.
TEST(PeriodicPacer, CountsEveryCycleNotJustOverruns)
{
    PeriodicPacer pacer{milliseconds(1)};
    EXPECT_EQ(pacer.cycles(), 0u);

    pacer.wait_for_next_cycle();
    pacer.wait_for_next_cycle();

    EXPECT_EQ(pacer.cycles(), 2u);
    EXPECT_EQ(pacer.overruns(), 0u);
    EXPECT_EQ(pacer.max_overshoot(), nanoseconds(0));
}

// The overshoot is the diagnostic that reveals the real cycle time when the
// configured period is deliberately unreachable.
TEST(PeriodicPacer, MaxOvershootTracksTheWorstCycle)
{
    constexpr auto period = milliseconds(1);
    PeriodicPacer pacer{period};

    simulate_cycle_work(milliseconds(5));
    pacer.wait_for_next_cycle();
    const auto after_small = pacer.max_overshoot();

    simulate_cycle_work(milliseconds(25));
    pacer.wait_for_next_cycle();
    const auto after_large = pacer.max_overshoot();

    // First body ran ~5 ms against a 1 ms period, so it overshot by ~4 ms.
    EXPECT_GE(after_small, milliseconds(3));
    EXPECT_GT(after_large, after_small);
    EXPECT_GE(after_large, milliseconds(20));

    // A later on-time cycle must not lower the recorded worst case.
    pacer.reset();
    EXPECT_EQ(pacer.max_overshoot(), nanoseconds(0));
}

// The transmission runner's real regime: a 5 us configured period against a
// 30-45 us SPI-bound cycle body. Every cycle overruns, so the forced relinquish
// is the only thing releasing the core — and it must still fire.
TEST(PeriodicPacer, FlatOutCycleStillRelinquishesOnSchedule)
{
    constexpr auto period = microseconds(5);
    constexpr auto spi_bound_work = microseconds(40);
    constexpr auto relinquish_interval = microseconds(200);
    constexpr auto relinquish_block = microseconds(20);

    PeriodicPacer pacer{period, relinquish_interval, relinquish_block};

    int blocking_waits = 0;
    constexpr int cycles = 40;
    for (int i = 0; i < cycles; ++i)
    {
        // Spin, not sleep: this stands in for a userspace SPI transfer that really
        // does hold the CPU for its whole duration.
        const auto work_end = steady_clock::now() + spi_bound_work;
        while (steady_clock::now() < work_end)
        {
        }

        const auto start = steady_clock::now();
        EXPECT_FALSE(pacer.wait_for_next_cycle());
        if (steady_clock::now() - start >= relinquish_block)
        {
            ++blocking_waits;
        }
    }

    EXPECT_EQ(pacer.overruns(), static_cast<uint64_t>(cycles));

    // 40 cycles of ~40 us span ~1.6 ms, so a 200 us relinquish interval must have
    // fired several times. Without it the core would never have been released.
    EXPECT_GE(blocking_waits, 4);

    // ...but it must not fire every cycle either, or the pacing would cost far more
    // throughput than the interval promises.
    EXPECT_LT(blocking_waits, cycles / 2);
}

// A tighter relinquish interval must release the core more often: the tunables
// actually take effect rather than falling back to the defaults.
TEST(PeriodicPacer, RelinquishIntervalIsHonoured)
{
    constexpr auto work = microseconds(50);
    constexpr auto block = microseconds(20);
    constexpr int cycles = 20;

    const auto count_blocking_waits = [](microseconds interval, microseconds relinquish_block,
                                         microseconds cycle_work, int loop_count)
    {
        PeriodicPacer pacer{nanoseconds(0), interval, relinquish_block};
        int blocking = 0;
        for (int i = 0; i < loop_count; ++i)
        {
            const auto work_end = steady_clock::now() + cycle_work;
            while (steady_clock::now() < work_end)
            {
            }

            const auto start = steady_clock::now();
            pacer.wait_for_next_cycle();
            if (steady_clock::now() - start >= relinquish_block)
            {
                ++blocking;
            }
        }
        return blocking;
    };

    // Interval below the cycle time: every cycle must relinquish.
    const int tight = count_blocking_waits(microseconds(10), block, work, cycles);
    // Interval far above the total run time: at most a couple of relinquishes.
    const int loose = count_blocking_waits(microseconds(5000), block, work, cycles);

    EXPECT_GT(tight, loose);
    EXPECT_GE(tight, cycles - 2);
    EXPECT_LE(loose, 2);
}

// Zero/negative tunables fall back to the documented defaults instead of
// disabling the relinquish guarantee outright.
TEST(PeriodicPacer, InvalidTunablesFallBackToDefaults)
{
    PeriodicPacer pacer{nanoseconds(0), nanoseconds(0), nanoseconds(-1)};

    const auto test_deadline = steady_clock::now() + milliseconds(10);
    bool observed_blocking_wait = false;
    while (steady_clock::now() < test_deadline)
    {
        const auto start = steady_clock::now();
        pacer.wait_for_next_cycle();
        if (steady_clock::now() - start >= nanoseconds(PeriodicPacer::kRelinquishBlockNs))
        {
            observed_blocking_wait = true;
        }
    }

    EXPECT_TRUE(observed_blocking_wait);
}

TEST(PeriodicPacer, PacingSummaryReportsCyclesOverrunsAndOvershoot)
{
    PeriodicPacer pacer{milliseconds(1)};

    pacer.wait_for_next_cycle();          // on time
    simulate_cycle_work(milliseconds(6));
    pacer.wait_for_next_cycle();          // overruns by ~5 ms

    const std::string summary = thread_utils::pacing_summary(pacer);

    EXPECT_NE(summary.find("2 cycles"), std::string::npos) << summary;
    EXPECT_NE(summary.find("1 deadline overruns"), std::string::npos) << summary;
    EXPECT_NE(summary.find("(50%)"), std::string::npos) << summary;
    EXPECT_NE(summary.find("worst overshoot"), std::string::npos) << summary;
}

// Guards the division in pacing_summary() against a never-run pacer.
TEST(PeriodicPacer, PacingSummaryHandlesZeroCycles)
{
    PeriodicPacer pacer{milliseconds(1)};

    const std::string summary = thread_utils::pacing_summary(pacer);

    EXPECT_NE(summary.find("0 cycles"), std::string::npos) << summary;
    EXPECT_NE(summary.find("(0%)"), std::string::npos) << summary;
}

// reset() re-anchors the schedule, so a long gap before the loop starts is not
// charged as an overrun.
TEST(PeriodicPacer, ResetReanchorsTheSchedule)
{
    constexpr auto period = milliseconds(5);
    PeriodicPacer pacer{period};

    simulate_cycle_work(milliseconds(30));
    ASSERT_FALSE(pacer.wait_for_next_cycle());

    pacer.reset();
    EXPECT_EQ(pacer.overruns(), 0u);
    EXPECT_TRUE(pacer.wait_for_next_cycle());
}
