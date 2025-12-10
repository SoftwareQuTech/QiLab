/**
 * @file profiler.hpp
 * @brief High-performance profiling implementation using thread-local storage.
 *
 * The Profiler class implements a singleton pattern for thread-safe, efficient
 * performance measurement. It uses:
 * - Thread-local storage for hot-path (PROF_START/PROF_STOP) lock-free operations
 * - A global registry for event name management
 * - Configurable CSV export for offline analysis
 *
 * Design focuses on minimal overhead for enabled profiling:
 * - Event registration is one-time, lazy initialization via static in macro
 * - Start/stop operations are lock-free (thread-local)
 * - Thread data merging is deferred until explicit flush
 * - CSV output happens after all measurement is complete
 *
 * When PROF_ENABLED is 0, this entire implementation is excluded and replaced
 * with zero-overhead no-op macros.
 */

#pragma once

#if PROF_ENABLED

#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

class ProfilerLocalStorage;
struct EventData;

/**
 * @brief Thread-safe singleton profiler for performance measurement.
 *
 * Provides efficient event registration, start/stop timing, and data export.
 * Designed for low overhead in production code through thread-local storage
 * and deferred synchronization.
 *
 * @note Access via Profiler::instance() singleton.
 * @note Not intended for direct use; use profiler_api.hpp macros instead.
 */
class Profiler
{
public:
    /**
     * @brief Get the singleton Profiler instance.
     * @return Reference to the global Profiler instance.
     */
    static Profiler &instance();

    /**
     * @brief Register an event by name (string_view version).
     *
     * @param name Event name as a string_view.
     * @return Unique event ID for use in PROF_START/PROF_STOP.
     */
    std::uint16_t register_event(std::string_view name);

    /**
     * @brief Register an event from a string literal.
     *
     * Optimized for compile-time string literals. Used internally by
     * PROF_REGISTER_EVENT() macro for single-initialization at call site.
     *
     * @param name Pointer to string literal.
     * @param len Length of the string (typically from macro sizeof trick).
     * @return Unique event ID for use in PROF_START/PROF_STOP.
     */
    std::uint16_t register_event_literal(const char *name, std::size_t len);

    /**
     * @brief Initialize profiling for the current thread.
     *
     * Allocates thread-local storage for measurements. Must be called once
     * per thread before using start() or stop().
     *
     * @param reserve_per_event Initial capacity per event (default 2^15).
     *                          Resize as needed if exceeded.
     */
    void thread_init(std::size_t reserve_per_event = (1u << 15));

    /**
     * @brief Start timing an event (hot path, lock-free).
     *
     * Records the current timestamp in thread-local storage.
     * Must be paired with a corresponding stop() call.
     *
     * @param event_id The event ID from register_event().
     * @note Designed for minimal overhead in hot code paths.
     */
    void start(std::uint16_t event_id) noexcept;

    /**
     * @brief Stop timing an event and record the duration (hot path, lock-free).
     *
     * Calculates duration since corresponding start() call and stores in
     * thread-local storage.
     *
     * @param event_id The event ID from register_event().
     * @note Designed for minimal overhead in hot code paths.
     */
    void stop(std::uint16_t event_id) noexcept;

    /**
     * @brief Merge thread-local profiling data to global container.
     *
     * Must be called on the SAME thread that called thread_init().
     * Typically called on thread termination before thread join.
     * This is a one-way operation; thread-local storage is not cleared.
     */
    void thread_merge_data();

    /**
     * @brief Log a free-form event message with timestamp.
     *
     * Thread-safe free-form event logging (not a hot path).
     * Useful for marking checkpoints or recording string-based events.
     *
     * @param message Event description.
     */
    void log_event(std::string_view message);

    /**
     * @brief Export all collected profiling data to CSV file(s).
     *
     * Generates CSV output with event durations and free-form messages.
     * Should be called after all threads have completed and flushed.
     *
     * @param base_path Base filename or path for CSV output.
     *                 Actual filename(s) may be derived from this.
     */
    void dump_csv(const std::string &base_path);

    /**
     * @brief Get the total number of registered events.
     * @return Count of unique events registered.
     */
    std::size_t event_count() const;

private:
    Profiler();
    Profiler(const Profiler &) = delete;
    Profiler &operator=(const Profiler &) = delete;

    // Registry (cold path)
    mutable std::mutex registration_lock_;
    std::vector<std::string> names_;
    std::unordered_map<std::string, uint16_t> name_to_id_;
    std::size_t max_events_{0};

    // Sink for durations (merge at flush/dump)
    std::mutex merge_lock_;
    //std::vector<std::vector<std::int64_t>> common_container_; // [event_id] -> durations (ns)
    std::vector<EventData> common_container_;

    // Free-form messages (not hot path)
    std::mutex events_lock_;
    std::chrono::steady_clock::time_point start_time_;
    std::vector<std::pair<std::string, std::int64_t>> events_;

    // TLS (hot path) – hidden definition in profiler.cpp
    static thread_local ProfilerLocalStorage local_storage_;
};

struct ProfGuard {
    explicit ProfGuard(std::size_t reserve_per_event = (1u << 15));
    ~ProfGuard();
};

#endif // PROF_ENABLED
