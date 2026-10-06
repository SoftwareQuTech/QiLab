// Copyright (c) 2026, Quantum Internet Alliance, Delft University of Technology, Milen Kolev, Samuel Oslovich, Jake Smith, Stephanie Wehner
// SPDX-License-Identifier: BSD-3-Clause-Clear

/**
 * @file profiler_api.hpp
 * @brief Public API for performance profiling with compile-time enable/disable.
 *
 * This header provides a macro-based profiling API that allows instrumentation of code
 * for performance analysis. The profiling system is designed to have zero overhead when
 * disabled (PROF_ENABLED == 0), and minimal overhead when enabled through efficient
 * thread-local storage and lock-free hot paths.
 *
 * ## Usage Example
 * @code
 *   // Register a profiling event (once per call site, compile-time evaluated)
 *   static auto write_event = PROF_REGISTER_EVENT("data_write");
 *
 *   // Start measuring
 *   PROF_START(write_event);
 *   // ... code to measure ...
 *   PROF_STOP(write_event);
 *
 *   // Log a free-form event
 *   PROF_EVENT("Checkpoint reached");
 *
 *   // After all threads complete, save results
 *   PROF_SAVE_DATA("results.csv");
 * @endcode
 *
 * ## Thread Safety
 * - Call PROF_THREAD_INIT() once per thread before using profiling macros
 * - Call PROF_THREAD_FLUSH() on each thread before dumping data
 * - PROF_SAVE_DATA() should be called after all threads have flushed
 */

#pragma once

#include <cstddef>
#include <cstdint>

/// Type alias for profiling event identifiers
typedef std::uint16_t Profiler_id;

#if PROF_ENABLED

#include "profiler.hpp"

/**
 * @brief Register a string literal as a profiling event.
 *
 * This macro registers a compile-time string literal as a unique profiling event.
 * The registration happens only once per call site (static initialization), making
 * it efficient to use in hot code paths.
 *
 * @param name_literal A string literal identifying the event (e.g., "data_transfer").
 * @return A Profiler_id (uint16_t) uniquely identifying the event.
 *
 * @note Should be stored in a static variable to avoid repeated registration overhead.
 *
 * @example
 *   static auto my_event = PROF_REGISTER_EVENT("my_operation");
 */
#define PROF_REGISTER_EVENT(name_literal) \
  ([]() -> std::uint16_t { \
          static const std::uint16_t __prof_id = \
              ::Profiler::instance().register_event_literal(name_literal, sizeof(name_literal) - 1); \
          return __prof_id; }())

/**
 * @brief Initialize the profiler for the current thread.
 *
 * Must be called once per thread before using other profiling macros.
 * This initializes thread-local storage for duration measurements.
 *
 * @note Default reserves space for 2^15 events per event type.
 */
#define PROF_THREAD_INIT() ::Profiler::instance().thread_init()

/**
 * @brief Flush thread-local profiling data to the global container.
 *
 * Must be called on the same thread that initialized profiling, typically
 * before the thread terminates.
 */
#define PROF_THREAD_FLUSH() ::Profiler::instance().thread_merge_data()

/**
 * @brief Start timing an event.
 *
 * Records the current time as the start of a measured interval for the given event.
 * Must be paired with a corresponding PROF_STOP() call.
 *
 * @param id The event ID returned by PROF_REGISTER_EVENT().
 */
#define PROF_START(id) ::Profiler::instance().start(id)

/**
 * @brief Stop timing an event.
 *
 * Records the duration since the corresponding PROF_START() call.
 *
 * @param id The event ID returned by PROF_REGISTER_EVENT().
 */
#define PROF_STOP(id) ::Profiler::instance().stop(id)

/**
 * @brief Log a free-form event message.
 *
 * Records a timestamped event message without duration measurement.
 * Useful for marking checkpoints or recording string-based events.
 *
 * @param message A string_view describing the event.
 */
#define PROF_EVENT(message) ::Profiler::instance().log_event(message)

/**
 * @brief Save profiling data to CSV file(s).
 *
 * Writes all collected profiling data to CSV format. Should be called after
 * all threads have called PROF_THREAD_FLUSH().
 *
 * @param file_name Base name for the output file(s).
 */
#define PROF_SAVE_DATA(file_name) ::Profiler::instance().dump_csv(file_name);

/**
 * @brief RAII helper for automatic profiling scope management.
 *
 * Useful for profiling function or code block durations.
 * Currently commented out but available for use.
 *
 * @example
 *   {
 *       ProfGuard pg;
 *       // ... code to measure ...
 *   } // Automatic stop on scope exit
 */
// #define PROF_GUARD(reserve_per_event) ::ProfGuard __prof_guard{reserve_per_event}

#else // PROF_ENABLED == 0  ========== PROFILING DISABLED ==========

/**
 * No-op macros when profiling is disabled. These provide zero overhead
 * and require no linking against the profiler implementation.
 * This allows code to remain instrumented without performance cost
 * when profiling is not needed.
 */

/// @brief No-op registration when disabled
#define PROF_REGISTER_EVENT(name_literal) (0u)

/// @brief Backward-compatible alias when disabled
#define PROF_EID(name_literal) PROF_REGISTER_EVENT(name_literal)

/// @brief No-op when disabled
#define PROF_THREAD_INIT() \
  do                       \
  {                        \
  } while (0)

/// @brief No-op when disabled
#define PROF_THREAD_FLUSH() \
  do                        \
  {                         \
  } while (0)

/// @brief No-op when disabled
#define PROF_START(name_lit) \
  do                         \
  {                          \
  } while (0)

/// @brief No-op when disabled
#define PROF_STOP(name_lit) \
  do                        \
  {                         \
  } while (0)

/// @brief No-op when disabled
#define PROF_EVENT(msg_sv) \
  do                       \
  {                        \
  } while (0)

/// @brief No-op when disabled
#define PROF_SAVE_DATA(file_name) \
  do                              \
  {                               \
  } while (0)

/// @brief No-op when disabled
#define PROF_GUARD(reserve) \
  do                        \
  {                         \
  } while (0)

/**
 * @brief Stub class providing zero-overhead profiling when disabled.
 *
 * Allows code instrumented with ProfGuard to compile and run without
 * any overhead when profiling is disabled.
 */
struct ProfGuard
{
  explicit ProfGuard(std::size_t = 0) {}
  ~ProfGuard() {}
};

#endif // PROF_ENABLED
