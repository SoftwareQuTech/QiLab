// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/*
** Created on: 09.09.2026
** Created by: mrkolev
** Description: Real-time periodic pacing for pinned SCHED_FIFO loops
*/

#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <ctime>
#include <cerrno>

namespace thread_utils {

/**
 * @brief Fixed-cadence pacer for pinned real-time loops.
 *
 * Paces a loop to a fixed period using an *absolute* deadline sleep
 * (`clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)`), which is the canonical
 * PREEMPT_RT cyclic-task pattern.
 *
 * Why not a spin/busy wait:
 * a `SCHED_FIFO` priority-80 thread pinned to a core and never blocking starves
 * every lower-priority per-CPU kernel thread on that core. On PREEMPT_RT those
 * include `rcuc/N` (prio 1) and `ksoftirqd/N`: `rcuc/N` never runs, the core
 * never reports an RCU quiescent state, the grace period never completes, and
 * any task parked in `synchronize_rcu()` while holding `cgroup_mutex` wedges
 * systemd's `cgroup_rmdir()` path. The result is a boot-time livelock: no units
 * dispatched, nothing reaped, `systemctl` unresponsive.
 *
 * Why not `sched_yield()` / `std::this_thread::yield()`:
 * for `SCHED_FIFO` it only rotates the caller within its own priority level. A
 * prio-1 kernel thread is still never picked. The loop must genuinely *block*.
 *
 * Why not a plain relative `sleep_for(period)`:
 * it sleeps `period` *after* the work, so the real cadence is
 * `work + period + wakeup_latency` and the phase drifts with every cycle.
 * An absolute deadline is measured from the start of the cycle, so per-cycle
 * work time and wakeup jitter do not accumulate.
 *
 * Overrun handling: when the cycle body outruns the period the deadline is
 * already in the past and the pacer would never block at all — reintroducing
 * the starvation it exists to prevent. In that case the deadline is re-anchored
 * to now (no chasing an unreachable schedule, no burst of zero-length cycles),
 * and if the core has not been released within @p relinquish_interval a short
 * blocking sleep is forced. That bounds the worst-case starvation window of the
 * lower-priority kernel threads.
 *
 * Two regimes, both intended:
 *  - *Paced*: the body finishes inside the period, so the deadline sleep blocks
 *    every cycle and the forced relinquish never triggers. Cadence is exact.
 *  - *Flat out*: the body always outruns the period — e.g. the transmission
 *    runner's 5 us configured period against a 30-45 us userspace SPI transfer.
 *    Deliberately configuring a period below the achievable cycle time is a
 *    valid way to say "run as fast as the hardware allows"; the loop then
 *    self-paces on the work itself and the forced relinquish is the *only*
 *    thing keeping the core available. Its cost is bounded and predictable:
 *    roughly `relinquish_block / relinquish_interval` of throughput (~9% at the
 *    defaults), paid once every `relinquish_interval / cycle_time` cycles.
 *    Tune the two via the constructor if that trade is wrong for a given loop.
 *
 * In flat-out mode every cycle is counted as an overrun by construction, so
 * read @ref overruns together with @ref cycles and @ref max_overshoot: the
 * overshoot is what tells you the real cycle time.
 *
 * Hot-path safe: no allocation, no locks, no exceptions, no virtual dispatch.
 * One VDSO `clock_gettime` plus at most one `clock_nanosleep` per cycle.
 */
class PeriodicPacer
{
    public:
    /// Default: force a blocking sleep if the core has not been released for this long.
    static constexpr int64_t kRelinquishIntervalNs = 200'000;   // 200 us

    /// Default duration of the forced blocking sleep taken on the overrun path.
    static constexpr int64_t kRelinquishBlockNs = 20'000;       // 20 us

    /**
     * @brief Constructs a pacer for the given loop period.
     *
     * @param period              Target cycle period. Values <= 0 make every
     *                            cycle an overrun, so only the forced-relinquish
     *                            guarantee applies.
     * @param relinquish_interval Maximum time the thread may hold its core
     *                            without blocking. Only reachable when cycles
     *                            overrun; smaller is friendlier to the per-CPU
     *                            kernel threads, larger costs less throughput.
     * @param relinquish_block    How long the forced sleep asks for. The real
     *                            block is this plus the platform's hrtimer
     *                            wakeup latency, so a small value still yields a
     *                            usable slice to a prio-1 kernel thread.
     */
    explicit PeriodicPacer(std::chrono::nanoseconds period,
                           std::chrono::nanoseconds relinquish_interval = std::chrono::nanoseconds(kRelinquishIntervalNs),
                           std::chrono::nanoseconds relinquish_block = std::chrono::nanoseconds(kRelinquishBlockNs)) noexcept
        : period_ns(period.count() > 0 ? static_cast<int64_t>(period.count()) : 0),
          relinquish_interval_ns(relinquish_interval.count() > 0 ? static_cast<int64_t>(relinquish_interval.count()) : kRelinquishIntervalNs),
          relinquish_block_ns(relinquish_block.count() > 0 ? static_cast<int64_t>(relinquish_block.count()) : kRelinquishBlockNs)
    {
        reset();
    }

    /**
     * @brief Anchors the schedule to the current time.
     *
     * Call immediately before entering the loop, after any startup work
     * (thread pinning, allocation, logging) has completed.
     */
    void reset() noexcept
    {
        next_deadline_ns = monotonic_now_ns();
        last_block_ns = next_deadline_ns;
        total_cycles = 0;
        overrun_cycles = 0;
        max_overshoot_ns = 0;
    }

    /**
     * @brief Blocks until the next cycle deadline.
     *
     * @return true if the deadline was still in the future (loop is on
     *         schedule), false if the cycle body overran the period.
     */
    bool wait_for_next_cycle() noexcept
    {
        ++total_cycles;
        next_deadline_ns += period_ns;

        const int64_t now_ns = monotonic_now_ns();
        if (now_ns < next_deadline_ns)
        {
            sleep_until_ns(next_deadline_ns);
            // We woke at or after the deadline, so that is when the core was last released.
            last_block_ns = next_deadline_ns;
            return true;
        }

        // Overrun: re-anchor instead of chasing a deadline we can no longer meet.
        ++overrun_cycles;
        const int64_t overshoot_ns = now_ns - next_deadline_ns;
        if (overshoot_ns > max_overshoot_ns)
        {
            max_overshoot_ns = overshoot_ns;
        }
        next_deadline_ns = now_ns;

        if (now_ns - last_block_ns >= relinquish_interval_ns)
        {
            sleep_until_ns(now_ns + relinquish_block_ns);
            last_block_ns = monotonic_now_ns();
        }
        return false;
    }

    /// Total cycles paced since construction or the last @ref reset.
    uint64_t cycles() const noexcept { return total_cycles; }

    /// Number of cycles whose body outran the configured period.
    uint64_t overruns() const noexcept { return overrun_cycles; }

    /**
     * @brief Largest amount by which a cycle body overran its deadline.
     *
     * Added to the configured period this is the worst observed cycle time, which
     * is the number to look at when deciding whether a period is achievable.
     */
    std::chrono::nanoseconds max_overshoot() const noexcept
    {
        return std::chrono::nanoseconds(max_overshoot_ns);
    }

    private:
    static int64_t monotonic_now_ns() noexcept
    {
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);   // VDSO on aarch64, no syscall
        return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
    }

    static void sleep_until_ns(int64_t deadline_ns) noexcept
    {
        timespec deadline{};
        deadline.tv_sec = static_cast<time_t>(deadline_ns / 1'000'000'000);
        deadline.tv_nsec = static_cast<long>(deadline_ns % 1'000'000'000);

        // TIMER_ABSTIME makes EINTR restartable with the same target: no drift,
        // no need to recompute a remaining duration.
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR)
        {
        }
    }

    /// Target cycle period in nanoseconds (0 if the configured period was invalid).
    int64_t period_ns;

    /// Maximum time this thread may hold its core without blocking.
    int64_t relinquish_interval_ns;

    /// Requested duration of the forced blocking sleep on the overrun path.
    int64_t relinquish_block_ns;

    /// Absolute deadline of the cycle currently being paced.
    int64_t next_deadline_ns = 0;

    /// Monotonic timestamp of the last time this thread released its core.
    int64_t last_block_ns = 0;

    /// Total cycles paced.
    uint64_t total_cycles = 0;

    /// Count of cycles that missed their deadline.
    uint64_t overrun_cycles = 0;

    /// Worst observed overshoot past a deadline, in nanoseconds.
    int64_t max_overshoot_ns = 0;
};

/**
 * @brief Formats a pacer's counters for a shutdown log line.
 *
 * Not hot-path code: allocates, and is meant to be called once when a runner
 * stops. Reports overruns as a share of total cycles and the worst overshoot,
 * so a loop deliberately running flat out is distinguishable from one that is
 * unexpectedly missing an achievable deadline.
 */
inline std::string pacing_summary(const PeriodicPacer& pacer)
{
    const uint64_t cycles = pacer.cycles();
    const uint64_t overruns = pacer.overruns();
    const auto overshoot_us =
        std::chrono::duration_cast<std::chrono::microseconds>(pacer.max_overshoot()).count();

    return std::to_string(cycles) + " cycles, " + std::to_string(overruns) +
           " deadline overruns (" +
           std::to_string(cycles ? (overruns * 100) / cycles : 0) +
           "%), worst overshoot " + std::to_string(overshoot_us) + " us";
}

} // namespace thread_utils
